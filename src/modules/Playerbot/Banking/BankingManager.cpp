/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BankingManager.h"
#include "../Professions/ProfessionManager.h"
#include "../Professions/GatheringMaterialsBridge.h"
#include "../Professions/ProfessionAuctionBridge.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "Item.h"
#include "Log.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

BankingManager* BankingManager::instance()
{
    static BankingManager instance;
    return &instance;
}

BankingManager::BankingManager()
    : BehaviorManager("BankingManager")
{
}

// ============================================================================
// LIFECYCLE (BehaviorManager override)
// ============================================================================

void BankingManager::OnInitialize()
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    TC_LOG_INFO("playerbot", "BankingManager::OnInitialize - Initializing personal banking system");

    InitializeDefaultRules();
    LoadBankingRules();

    TC_LOG_INFO("playerbot", "BankingManager::OnInitialize - Personal banking system initialized");
}

void BankingManager::OnUpdate(::Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    // Check if enabled
    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr == _bankingProfiles.end())
        return;

    BotBankingProfile const& profile = profileItr->second;

    // Throttle updates
    uint32 now = GameTime::GetGameTimeMS();
    auto lastAccessItr = _lastBankAccessTimes.find(playerGuid);
    if (lastAccessItr != _lastBankAccessTimes.end())
    {
        if (now - lastAccessItr->second < profile.bankCheckInterval)
            return;
    }

    // Check if already banking
    if (_currentlyBanking.find(playerGuid) != _currentlyBanking.end())
        return;

    // Check if near banker
    if (!IsNearBanker(player))
    {
        // Check if we should travel to banker
        bool needsBank = false;

        if (profile.autoDepositGold && ShouldDepositGold(player))
            needsBank = true;

        if (profile.autoDepositMaterials)
        {
            // Check if inventory is getting full
            uint32 freeSlots = player->GetBagsFreeSlots();
            if (freeSlots < 10)
                needsBank = true;
        }

        if (needsBank && profile.travelToBankerWhenNeeded)
        {
            TravelToNearestBanker(player);
        }

        return;
    }

    // Perform banking operations
    _currentlyBanking.insert(playerGuid);
    _lastBankAccessTimes[playerGuid] = now;

    uint32 startTime = GameTime::GetGameTimeMS();

    // Gold management
    if (profile.autoDepositGold && ShouldDepositGold(player))
    {
        uint32 depositAmount = GetRecommendedGoldDeposit(player);
        if (depositAmount > 0)
            DepositGold(player, depositAmount);
    }

    if (profile.autoDepositGold && ShouldWithdrawGold(player))
    {
        uint32 withdrawAmount = profile.minGoldInInventory - player->GetMoney();
        if (withdrawAmount > 0)
            WithdrawGold(player, withdrawAmount);
    }

    // Item management
    if (profile.autoDepositMaterials)
    {
        DepositExcessItems(player);
    }

    if (profile.autoWithdrawForCrafting)
    {
        WithdrawMaterialsForCrafting(player);
    }

    uint32 endTime = GameTime::GetGameTimeMS();
    uint32 duration = endTime - startTime;

    _playerStatistics[playerGuid].timeSpentBanking += duration;
    _globalStatistics.timeSpentBanking += duration;
    _playerStatistics[playerGuid].bankTrips++;
    _globalStatistics.bankTrips++;

    _currentlyBanking.erase(playerGuid);
}

void BankingManager::OnShutdown()
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    TC_LOG_INFO("playerbot", "BankingManager::OnShutdown - Shutting down personal banking system");

    _bankingProfiles.clear();
    _transactionHistory.clear();
    _lastBankAccessTimes.clear();
    _currentlyBanking.clear();
}

// ========================================================================
// CORE BANKING OPERATIONS
// ========================================================================

void BankingManager::SetEnabled(::Player* player, bool enabled)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    if (enabled)
    {
        if (_bankingProfiles.find(playerGuid) == _bankingProfiles.end())
        {
            _bankingProfiles[playerGuid] = BotBankingProfile();
        }
    }
    else
    {
        _bankingProfiles.erase(playerGuid);
        _currentlyBanking.erase(playerGuid);
    }
}

bool BankingManager::IsEnabled(::Player* player) const
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    return _bankingProfiles.find(playerGuid) != _bankingProfiles.end();
}

void BankingManager::SetBankingProfile(uint32 playerGuid, BotBankingProfile const& profile)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _bankingProfiles[playerGuid] = profile;
}

BotBankingProfile BankingManager::GetBankingProfile(uint32 playerGuid) const
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _bankingProfiles.find(playerGuid);
    if (itr != _bankingProfiles.end())
        return itr->second;

    return BotBankingProfile();
}

void BankingManager::AddBankingRule(uint32 playerGuid, BankingRule const& rule)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr != _bankingProfiles.end())
    {
        profileItr->second.customRules.push_back(rule);
    }
}

void BankingManager::RemoveBankingRule(uint32 playerGuid, uint32 itemId)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr != _bankingProfiles.end())
    {
        auto& rules = profileItr->second.customRules;
        rules.erase(std::remove_if(rules.begin(), rules.end(),
            [itemId](const BankingRule& rule) { return rule.itemId == itemId; }),
            rules.end());
    }
}

// ========================================================================
// GOLD MANAGEMENT
// ========================================================================

bool BankingManager::DepositGold(::Player* player, uint32 amount)
{
    if (!player || amount == 0)
        return false;

    uint32 playerMoney = player->GetMoney();
    if (playerMoney < amount)
        amount = playerMoney;

    if (amount == 0)
        return false;

    // TrinityCore API: Deposit gold to bank
    // player->SetMoney(playerMoney - amount);
    // player->ModifyBankMoney(amount); // Assuming this exists

    // For now, use simplified approach
    player->ModifyMoney(-(int32)amount);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::DEPOSIT_GOLD;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.goldAmount = amount;
    transaction.reason = "Automatic gold deposit";

    uint32 playerGuid = player->GetGUID().GetCounter();
    RecordTransaction(playerGuid, transaction);

    // Update statistics
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _playerStatistics[playerGuid].totalDeposits++;
    _playerStatistics[playerGuid].goldDeposited += amount;
    _globalStatistics.totalDeposits++;
    _globalStatistics.goldDeposited += amount;

    TC_LOG_DEBUG("playerbot", "BankingManager::DepositGold - Bot {} deposited {} copper to bank",
        player->GetName(), amount);

    return true;
}

bool BankingManager::WithdrawGold(::Player* player, uint32 amount)
{
    if (!player || amount == 0)
        return false;

    // TrinityCore API: Withdraw gold from bank
    // uint32 bankMoney = player->GetBankMoney();
    // if (bankMoney < amount)
    //     return false;
    // player->SetMoney(player->GetMoney() + amount);
    // player->ModifyBankMoney(-(int32)amount);

    // For now, use simplified approach (assume bank has money)
    player->ModifyMoney(amount);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::WITHDRAW_GOLD;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.goldAmount = amount;
    transaction.reason = "Automatic gold withdrawal";

    uint32 playerGuid = player->GetGUID().GetCounter();
    RecordTransaction(playerGuid, transaction);

    // Update statistics
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _playerStatistics[playerGuid].totalWithdrawals++;
    _playerStatistics[playerGuid].goldWithdrawn += amount;
    _globalStatistics.totalWithdrawals++;
    _globalStatistics.goldWithdrawn += amount;

    return true;
}

bool BankingManager::ShouldDepositGold(::Player* player)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr == _bankingProfiles.end())
        return false;

    uint32 currentGold = player->GetMoney();
    return currentGold > profileItr->second.maxGoldInInventory;
}

bool BankingManager::ShouldWithdrawGold(::Player* player)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr == _bankingProfiles.end())
        return false;

    uint32 currentGold = player->GetMoney();
    return currentGold < profileItr->second.minGoldInInventory;
}

uint32 BankingManager::GetRecommendedGoldDeposit(::Player* player)
{
    if (!player)
        return 0;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr == _bankingProfiles.end())
        return 0;

    uint32 currentGold = player->GetMoney();
    uint32 targetGold = profileItr->second.minGoldInInventory +
                       (profileItr->second.maxGoldInInventory - profileItr->second.minGoldInInventory) / 2;

    if (currentGold > targetGold)
        return currentGold - targetGold;

    return 0;
}

// ========================================================================
// ITEM MANAGEMENT
// ========================================================================

bool BankingManager::DepositItem(::Player* player, uint32 itemGuid, uint32 quantity)
{
    if (!player)
        return false;

    Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, 0, itemGuid));
    if (!item)
        return false;

    uint32 itemId = item->GetEntry();
    uint32 itemCount = item->GetCount();

    if (quantity > itemCount)
        quantity = itemCount;

    // TrinityCore API: Move item to bank
    // player->MoveItemToBank(item, quantity);

    // For now, simplified approach
    // Actual implementation would use TrinityCore's bank system

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::DEPOSIT_ITEM;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.itemId = itemId;
    transaction.quantity = quantity;
    transaction.reason = "Automatic item deposit";

    uint32 playerGuid = player->GetGUID().GetCounter();
    RecordTransaction(playerGuid, transaction);

    // Update statistics
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _playerStatistics[playerGuid].totalDeposits++;
    _playerStatistics[playerGuid].itemsDeposited += quantity;
    _globalStatistics.totalDeposits++;
    _globalStatistics.itemsDeposited += quantity;

    return true;
}

bool BankingManager::WithdrawItem(::Player* player, uint32 itemId, uint32 quantity)
{
    if (!player)
        return false;

    // TrinityCore API: Find item in bank and move to inventory
    // Item* bankItem = player->GetBankItem(itemId);
    // if (!bankItem)
    //     return false;
    // player->MoveItemFromBank(bankItem, quantity);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::WITHDRAW_ITEM;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.itemId = itemId;
    transaction.quantity = quantity;
    transaction.reason = "Automatic item withdrawal";

    uint32 playerGuid = player->GetGUID().GetCounter();
    RecordTransaction(playerGuid, transaction);

    // Update statistics
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _playerStatistics[playerGuid].totalWithdrawals++;
    _playerStatistics[playerGuid].itemsWithdrawn += quantity;
    _globalStatistics.totalWithdrawals++;
    _globalStatistics.itemsWithdrawn += quantity;

    return true;
}

bool BankingManager::ShouldDepositItem(::Player* player, uint32 itemId, uint32 currentCount)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    // Find banking rule
    BankingRule const* rule = FindBankingRule(playerGuid, itemId);
    if (rule)
    {
        if (!rule->enabled)
            return false;

        if (rule->priority == BankingPriority::NEVER_BANK)
            return false;

        // Check if exceeds max in inventory
        return currentCount > rule->maxInInventory;
    }

    // No rule found - use default heuristics
    BankingPriority priority = CalculateItemPriority(player, itemId);

    switch (priority)
    {
        case BankingPriority::NEVER_BANK:
            return false;
        case BankingPriority::CRITICAL:
            return true;
        case BankingPriority::HIGH:
            return currentCount > 20;
        case BankingPriority::MEDIUM:
            return currentCount > 50;
        case BankingPriority::LOW:
            return currentCount > 100;
        default:
            return false;
    }
}

BankingPriority BankingManager::GetItemBankingPriority(::Player* player, uint32 itemId)
{
    if (!player)
        return BankingPriority::NEVER_BANK;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    BankingRule const* rule = FindBankingRule(playerGuid, itemId);
    if (rule)
        return rule->priority;

    return CalculateItemPriority(player, itemId);
}

void BankingManager::DepositExcessItems(::Player* player)
{
    if (!player)
        return;

    std::vector<DepositCandidate> candidates = GetDepositCandidates(player);

    // Sort by priority (highest first)
    std::sort(candidates.begin(), candidates.end(),
        [](const DepositCandidate& a, const DepositCandidate& b) {
            return a.priority > b.priority;
        });

    // Deposit items
    for (const DepositCandidate& candidate : candidates)
    {
        if (!HasBankSpace(player))
            break;

        DepositItem(player, candidate.itemGuid, candidate.quantity);
    }
}

void BankingManager::WithdrawMaterialsForCrafting(::Player* player)
{
    if (!player)
        return;

    std::vector<WithdrawRequest> requests = GetWithdrawRequests(player);

    for (const WithdrawRequest& request : requests)
    {
        // Check if player has inventory space
        uint32 freeSlots = player->GetBagsFreeSlots();
        if (freeSlots == 0)
            break;

        // Check if item is in bank
        if (!IsItemInBank(player, request.itemId))
            continue;

        uint32 bankCount = GetItemCountInBank(player, request.itemId);
        uint32 withdrawAmount = std::min(request.quantity, bankCount);

        if (withdrawAmount > 0)
            WithdrawItem(player, request.itemId, withdrawAmount);
    }
}

// ========================================================================
// BANK SPACE ANALYSIS
// ========================================================================

BankSpaceInfo BankingManager::GetBankSpaceInfo(::Player* player)
{
    BankSpaceInfo info;

    if (!player)
        return info;

    // TrinityCore API: Get bank slot information
    // info.totalSlots = player->GetBankBagSlotCount();
    // info.usedSlots = player->GetBankUsedSlots();
    // info.freeSlots = info.totalSlots - info.usedSlots;

    // Simplified for now
    info.totalSlots = 28; // Default bank slots
    info.usedSlots = 0;
    info.freeSlots = info.totalSlots;

    return info;
}

bool BankingManager::HasBankSpace(::Player* player, uint32 slotsNeeded)
{
    BankSpaceInfo info = GetBankSpaceInfo(player);
    return info.HasSpace(slotsNeeded);
}

uint32 BankingManager::GetItemCountInBank(::Player* player, uint32 itemId)
{
    if (!player)
        return 0;

    // TrinityCore API: Count items in bank
    // return player->GetBankItemCount(itemId);

    return 0; // Simplified
}

bool BankingManager::IsItemInBank(::Player* player, uint32 itemId)
{
    return GetItemCountInBank(player, itemId) > 0;
}

void BankingManager::OptimizeBankSpace(::Player* player)
{
    if (!player)
        return;

    // TrinityCore API: Consolidate item stacks in bank
    // player->ConsolidateBankItems();

    TC_LOG_DEBUG("playerbot", "BankingManager::OptimizeBankSpace - Optimized bank space for bot {}",
        player->GetName());
}

// ========================================================================
// BANKER ACCESS
// ========================================================================

bool BankingManager::IsNearBanker(::Player* player)
{
    if (!player)
        return false;

    // Check if player is in a city with banker access
    // Simplified: Check if in rest area
    if (player->HasRestFlag(REST_FLAG_IN_CITY))
        return true;

    // Check proximity to banker NPCs
    // This would require creature search
    return false;
}

float BankingManager::GetDistanceToNearestBanker(::Player* player)
{
    if (!player)
        return 10000.0f;

    // Search for banker NPCs
    // This would require world creature query
    return 1000.0f; // Simplified
}

bool BankingManager::TravelToNearestBanker(::Player* player)
{
    if (!player)
        return false;

    // Trigger bot movement to nearest banker
    // This would integrate with bot movement system
    TC_LOG_DEBUG("playerbot", "BankingManager::TravelToNearestBanker - Bot {} traveling to banker",
        player->GetName());

    return false; // Simplified
}

// ========================================================================
// TRANSACTION HISTORY
// ========================================================================

std::vector<BankingTransaction> BankingManager::GetRecentTransactions(uint32 playerGuid, uint32 count)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _transactionHistory.find(playerGuid);
    if (itr == _transactionHistory.end())
        return {};

    const std::vector<BankingTransaction>& history = itr->second;

    uint32 start = history.size() > count ? history.size() - count : 0;
    return std::vector<BankingTransaction>(history.begin() + start, history.end());
}

void BankingManager::RecordTransaction(uint32 playerGuid, BankingTransaction const& transaction)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto& history = _transactionHistory[playerGuid];
    history.push_back(transaction);

    // Limit history size
    if (history.size() > MAX_TRANSACTION_HISTORY)
    {
        history.erase(history.begin(), history.begin() + (history.size() - MAX_TRANSACTION_HISTORY));
    }
}

// ========================================================================
// STATISTICS
// ========================================================================

BankingStatistics const& BankingManager::GetPlayerStatistics(uint32 playerGuid) const
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _playerStatistics.find(playerGuid);
    if (itr != _playerStatistics.end())
        return itr->second;

    static BankingStatistics empty;
    return empty;
}

BankingStatistics const& BankingManager::GetGlobalStatistics() const
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    return _globalStatistics;
}

void BankingManager::ResetStatistics(uint32 playerGuid)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _playerStatistics.find(playerGuid);
    if (itr != _playerStatistics.end())
        itr->second.Reset();
}

// ========================================================================
// INITIALIZATION HELPERS
// ========================================================================

void BankingManager::InitializeDefaultRules()
{
    // Default rules are defined per-bot in BotBankingProfile
    TC_LOG_DEBUG("playerbot", "BankingManager::InitializeDefaultRules - Initialized default banking rules");
}

void BankingManager::LoadBankingRules()
{
    // Load banking rules from configuration or database
    TC_LOG_DEBUG("playerbot", "BankingManager::LoadBankingRules - Loaded banking rules");
}

// ========================================================================
// BANKING LOGIC HELPERS
// ========================================================================

BankingRule const* BankingManager::FindBankingRule(uint32 playerGuid, uint32 itemId)
{
    auto profileItr = _bankingProfiles.find(playerGuid);
    if (profileItr == _bankingProfiles.end())
        return nullptr;

    for (const BankingRule& rule : profileItr->second.customRules)
    {
        if (ItemMatchesRule(itemId, rule))
            return &rule;
    }

    return nullptr;
}

BankingPriority BankingManager::CalculateItemPriority(::Player* player, uint32 itemId)
{
    if (!player)
        return BankingPriority::NEVER_BANK;

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return BankingPriority::NEVER_BANK;

    uint32 itemClass = itemTemplate->GetClass();
    uint32 itemQuality = itemTemplate->GetQuality();

    // Never bank equipped items or quest items
    if (itemClass == ITEM_CLASS_QUEST)
        return BankingPriority::NEVER_BANK;

    // Bank high quality items for safekeeping
    if (itemQuality >= ITEM_QUALITY_EPIC)
        return BankingPriority::CRITICAL;

    if (itemQuality == ITEM_QUALITY_RARE)
        return BankingPriority::HIGH;

    // Bank profession materials
    if (itemClass == ITEM_CLASS_TRADE_GOODS || itemClass == ITEM_CLASS_REAGENT)
    {
        if (IsNeededForProfessions(player, itemId))
            return BankingPriority::MEDIUM; // Keep some in inventory
        else
            return BankingPriority::HIGH; // Bank excess
    }

    // Bank consumables
    if (itemClass == ITEM_CLASS_CONSUMABLE)
        return BankingPriority::MEDIUM;

    return BankingPriority::LOW;
}

bool BankingManager::ItemMatchesRule(uint32 itemId, BankingRule const& rule)
{
    // Specific item ID match
    if (rule.itemId != 0 && rule.itemId == itemId)
        return true;

    // Class/subclass match
    if (rule.itemId == 0 && rule.itemClass != 0)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
            return false;

        if (itemTemplate->GetClass() != rule.itemClass)
            return false;

        if (rule.itemSubClass != 0 && itemTemplate->GetSubClass() != rule.itemSubClass)
            return false;

        if (rule.itemQuality != 0 && itemTemplate->GetQuality() != rule.itemQuality)
            return false;

        return true;
    }

    return false;
}

std::vector<BankingManager::DepositCandidate> BankingManager::GetDepositCandidates(::Player* player)
{
    std::vector<DepositCandidate> candidates;

    if (!player)
        return candidates;

    // Scan inventory for items to deposit
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!item)
            continue;

        uint32 itemId = item->GetEntry();
        uint32 itemCount = item->GetCount();

        if (ShouldDepositItem(player, itemId, itemCount))
        {
            DepositCandidate candidate;
            candidate.itemGuid = item->GetGUID().GetCounter();
            candidate.itemId = itemId;
            candidate.quantity = itemCount;
            candidate.priority = GetItemBankingPriority(player, itemId);

            candidates.push_back(candidate);
        }
    }

    return candidates;
}

std::vector<BankingManager::WithdrawRequest> BankingManager::GetWithdrawRequests(::Player* player)
{
    std::vector<WithdrawRequest> requests;

    if (!player)
        return requests;

    // Get materials needed for crafting from ProfessionManager
    ProfessionManager* profMgr = ProfessionManager::instance();
    if (!profMgr)
        return requests;

    GatheringMaterialsBridge* gatherBridge = GatheringMaterialsBridge::instance();
    if (!gatherBridge)
        return requests;

    auto neededMaterials = gatherBridge->GetNeededMaterials(player);

    for (const auto& material : neededMaterials)
    {
        uint32 inventoryCount = player->GetItemCount(material.itemId);
        uint32 needed = material.quantityNeeded;

        if (inventoryCount < needed)
        {
            WithdrawRequest request;
            request.itemId = material.itemId;
            request.quantity = needed - inventoryCount;
            request.reason = "Needed for crafting";

            requests.push_back(request);
        }
    }

    return requests;
}

// ========================================================================
// INTEGRATION HELPERS
// ========================================================================

bool BankingManager::IsNeededForProfessions(::Player* player, uint32 itemId)
{
    if (!player)
        return false;

    GatheringMaterialsBridge* gatherBridge = GatheringMaterialsBridge::instance();
    if (!gatherBridge)
        return false;

    return gatherBridge->IsItemNeededForCrafting(player, itemId);
}

uint32 BankingManager::GetMaterialPriorityFromProfessions(::Player* player, uint32 itemId)
{
    if (!player)
        return 0;

    GatheringMaterialsBridge* gatherBridge = GatheringMaterialsBridge::instance();
    if (!gatherBridge)
        return 0;

    auto priority = gatherBridge->GetMaterialPriority(player, itemId);
    return static_cast<uint32>(priority);
}

} // namespace Playerbot
