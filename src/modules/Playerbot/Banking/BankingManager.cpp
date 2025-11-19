/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * **Phase 5.1: BankingManager - Per-Bot Instance Implementation**
 *
 * Converted from singleton to per-bot instance pattern.
 * Each bot has its own BankingManager owned by GameSystemsManager.
 */

#include "BankingManager.h"
#include "../Professions/ProfessionManager.h"
#include "../Professions/GatheringMaterialsBridge.h"
#include "../Professions/ProfessionAuctionBridge.h"
#include "../AI/BotAI.h"
#include "../Session/BotSession.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "Item.h"
#include "Log.h"
#include "World.h"
#include "WorldSession.h"
#include "GameTime.h"
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

std::vector<BankingRule> BankingManager::_defaultRules;
BankingStatistics BankingManager::_globalStatistics;
bool BankingManager::_defaultRulesInitialized = false;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BankingManager::BankingManager(Player* bot)
    : BehaviorManager("BankingManager")
    , _bot(bot)
    , _lastBankAccessTime(0)
    , _currentlyBanking(false)
    , _enabled(true)
{
    TC_LOG_DEBUG("playerbot", "BankingManager: Constructed for bot {}", _bot ? _bot->GetName() : "null");
}

BankingManager::~BankingManager()
{
    TC_LOG_DEBUG("playerbot", "BankingManager: Destroyed for bot {}", _bot ? _bot->GetName() : "null");
}

// ============================================================================
// LIFECYCLE (BehaviorManager override)
// ============================================================================

void BankingManager::OnInitialize()
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "BankingManager::OnInitialize - No bot reference!");
        return;
    }

    TC_LOG_DEBUG("playerbot", "BankingManager::OnInitialize - Initializing for bot {}", _bot->GetName());

    // Initialize shared default rules (once)
    if (!_defaultRulesInitialized)
    {
        LoadBankingRules();
        _defaultRulesInitialized = true;
    }

    // Initialize per-bot rules
    InitializeDefaultRules();

    TC_LOG_DEBUG("playerbot", "BankingManager::OnInitialize - Initialized for bot {}", _bot->GetName());
}

void BankingManager::OnUpdate(::Player* player, uint32 diff)
{
    if (!_bot || !player || player != _bot)
        return;

    if (!_enabled)
        return;

    // Throttle updates
    uint32 now = GameTime::GetGameTimeMS();
    if (now - _lastBankAccessTime < _profile.bankCheckInterval)
        return;

    // Check if already banking
    if (_currentlyBanking)
        return;

    // Check if near banker
    if (!IsNearBanker())
    {
        // Check if we should travel to banker
        bool needsBank = false;

        if (_profile.autoDepositGold && ShouldDepositGold())
            needsBank = true;

        if (_profile.autoDepositMaterials)
        {
            // Check if inventory is getting full
            uint32 freeSlots = _bot->GetBagsFreeSlots();
            if (freeSlots < 10)
                needsBank = true;
        }

        if (needsBank && _profile.travelToBankerWhenNeeded)
        {
            TravelToNearestBanker();
        }

        return;
    }

    // Near banker, perform banking operations
    _currentlyBanking = true;

    // Auto-deposit gold
    if (_profile.autoDepositGold && ShouldDepositGold())
    {
        uint32 amount = GetRecommendedGoldDeposit();
        if (amount > 0)
            DepositGold(amount);
    }

    // Auto-deposit materials
    if (_profile.autoDepositMaterials)
        DepositExcessItems();

    // Auto-withdraw for crafting
    if (_profile.autoWithdrawForCrafting)
        WithdrawMaterialsForCrafting();

    _lastBankAccessTime = now;
    _currentlyBanking = false;
}

void BankingManager::OnShutdown()
{
    TC_LOG_DEBUG("playerbot", "BankingManager::OnShutdown - Shutting down for bot {}",
        _bot ? _bot->GetName() : "null");
}

// ============================================================================
// CORE BANKING OPERATIONS
// ============================================================================

void BankingManager::SetEnabled(bool enabled)
{
    _enabled = enabled;
}

bool BankingManager::IsEnabled() const
{
    return _enabled;
}

void BankingManager::SetBankingProfile(BotBankingProfile const& profile)
{
    _profile = profile;
}

BotBankingProfile BankingManager::GetBankingProfile() const
{
    return _profile;
}

void BankingManager::AddBankingRule(BankingRule const& rule)
{
    _profile.customRules.push_back(rule);
}

void BankingManager::RemoveBankingRule(uint32 itemId)
{
    _profile.customRules.erase(
        std::remove_if(_profile.customRules.begin(), _profile.customRules.end(),
            [itemId](BankingRule const& rule) { return rule.itemId == itemId; }),
        _profile.customRules.end());
}

// ============================================================================
// GOLD MANAGEMENT
// ============================================================================

bool BankingManager::DepositGold(uint32 amount)
{
    if (!_bot || amount == 0)
        return false;

    if (_bot->GetMoney() < amount)
        amount = _bot->GetMoney();

    if (amount == 0)
        return false;

    // Use TrinityCore bank API
    // TODO: Integrate with actual bank API when available
    // For now, just move gold from inventory to bank

    TC_LOG_DEBUG("playerbot", "BankingManager: Bot {} deposited {} gold",
        _bot->GetName(), amount / 10000);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::DEPOSIT_GOLD;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.goldAmount = amount;
    transaction.reason = "Auto-deposit gold";
    RecordTransaction(transaction);

    // Update statistics
    _statistics.totalDeposits++;
    _statistics.goldDeposited += amount;
    _globalStatistics.totalDeposits++;
    _globalStatistics.goldDeposited += amount;

    return true;
}

bool BankingManager::WithdrawGold(uint32 amount)
{
    if (!_bot || amount == 0)
        return false;

    // Use TrinityCore bank API
    // TODO: Integrate with actual bank API when available

    TC_LOG_DEBUG("playerbot", "BankingManager: Bot {} withdrew {} gold",
        _bot->GetName(), amount / 10000);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::WITHDRAW_GOLD;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.goldAmount = amount;
    transaction.reason = "Auto-withdraw gold";
    RecordTransaction(transaction);

    // Update statistics
    _statistics.totalWithdrawals++;
    _statistics.goldWithdrawn += amount;
    _globalStatistics.totalWithdrawals++;
    _globalStatistics.goldWithdrawn += amount;

    return true;
}

bool BankingManager::ShouldDepositGold()
{
    if (!_bot)
        return false;

    uint64 currentGold = _bot->GetMoney();
    return currentGold > _profile.maxGoldInInventory;
}

bool BankingManager::ShouldWithdrawGold()
{
    if (!_bot)
        return false;

    uint64 currentGold = _bot->GetMoney();
    return currentGold < _profile.minGoldInInventory;
}

uint32 BankingManager::GetRecommendedGoldDeposit()
{
    if (!_bot)
        return 0;

    uint64 currentGold = _bot->GetMoney();
    if (currentGold <= _profile.maxGoldInInventory)
        return 0;

    // Deposit excess gold, keep maxGoldInInventory
    uint32 excess = static_cast<uint32>(currentGold - _profile.maxGoldInInventory);
    return excess;
}

// ============================================================================
// ITEM MANAGEMENT
// ============================================================================

bool BankingManager::DepositItem(uint32 itemGuid, uint32 quantity)
{
    if (!_bot)
        return false;

    // Use TrinityCore bank API
    // TODO: Integrate with actual bank API

    TC_LOG_DEBUG("playerbot", "BankingManager: Bot {} deposited item {}",
        _bot->GetName(), itemGuid);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::DEPOSIT_ITEM;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.itemId = itemGuid;
    transaction.quantity = quantity;
    transaction.reason = "Auto-deposit item";
    RecordTransaction(transaction);

    // Update statistics
    _statistics.totalDeposits++;
    _statistics.itemsDeposited += quantity;
    _globalStatistics.totalDeposits++;
    _globalStatistics.itemsDeposited += quantity;

    return true;
}

bool BankingManager::WithdrawItem(uint32 itemId, uint32 quantity)
{
    if (!_bot)
        return false;

    // Use TrinityCore bank API
    // TODO: Integrate with actual bank API

    TC_LOG_DEBUG("playerbot", "BankingManager: Bot {} withdrew item {} x{}",
        _bot->GetName(), itemId, quantity);

    // Record transaction
    BankingTransaction transaction;
    transaction.type = BankingTransaction::Type::WITHDRAW_ITEM;
    transaction.timestamp = GameTime::GetGameTimeMS();
    transaction.itemId = itemId;
    transaction.quantity = quantity;
    transaction.reason = "Auto-withdraw item";
    RecordTransaction(transaction);

    // Update statistics
    _statistics.totalWithdrawals++;
    _statistics.itemsWithdrawn += quantity;
    _globalStatistics.totalWithdrawals++;
    _globalStatistics.itemsWithdrawn += quantity;

    return true;
}

bool BankingManager::ShouldDepositItem(uint32 itemId, uint32 currentCount)
{
    BankingRule const* rule = FindBankingRule(itemId);
    if (!rule)
        return false;

    if (!rule->enabled)
        return false;

    if (rule->priority == BankingPriority::NEVER_BANK)
        return false;

    // Check if count exceeds max inventory threshold
    if (currentCount > rule->maxInInventory)
        return true;

    // Check priority level
    if (rule->priority == BankingPriority::CRITICAL)
        return true;

    return false;
}

BankingPriority BankingManager::GetItemBankingPriority(uint32 itemId)
{
    BankingRule const* rule = FindBankingRule(itemId);
    if (rule)
        return rule->priority;

    return CalculateItemPriority(itemId);
}

void BankingManager::DepositExcessItems()
{
    if (!_bot)
        return;

    std::vector<DepositCandidate> candidates = GetDepositCandidates();

    // Sort by priority (highest first)
    std::sort(candidates.begin(), candidates.end(),
        [](DepositCandidate const& a, DepositCandidate const& b) {
            return static_cast<uint8>(a.priority) > static_cast<uint8>(b.priority);
        });

    // Deposit items
    for (auto const& candidate : candidates)
    {
        if (!HasBankSpace())
            break;

        DepositItem(candidate.itemGuid, candidate.quantity);
    }
}

void BankingManager::WithdrawMaterialsForCrafting()
{
    if (!_bot)
        return;

    std::vector<WithdrawRequest> requests = GetWithdrawRequests();

    for (auto const& request : requests)
    {
        WithdrawItem(request.itemId, request.quantity);
    }
}

// ============================================================================
// BANK SPACE ANALYSIS
// ============================================================================

BankSpaceInfo BankingManager::GetBankSpaceInfo()
{
    BankSpaceInfo info;

    if (!_bot)
        return info;

    // Use TrinityCore bank API
    // TODO: Integrate with actual bank API to get real data

    info.totalSlots = 28;  // Default bank size
    info.usedSlots = 0;    // TODO: Count actual used slots
    info.freeSlots = info.totalSlots - info.usedSlots;

    return info;
}

bool BankingManager::HasBankSpace(uint32 slotsNeeded)
{
    BankSpaceInfo info = GetBankSpaceInfo();
    return info.freeSlots >= slotsNeeded;
}

uint32 BankingManager::GetItemCountInBank(uint32 itemId)
{
    if (!_bot)
        return 0;

    // Use TrinityCore bank API
    // TODO: Integrate with actual bank API

    return 0;
}

bool BankingManager::IsItemInBank(uint32 itemId)
{
    return GetItemCountInBank(itemId) > 0;
}

void BankingManager::OptimizeBankSpace()
{
    if (!_bot)
        return;

    // TODO: Implement bank space optimization
    // - Consolidate stacks
    // - Remove junk items
    // - Reorganize by item type
}

// ============================================================================
// BANKER ACCESS
// ============================================================================

bool BankingManager::IsNearBanker()
{
    if (!_bot)
        return false;

    float distance = GetDistanceToNearestBanker();
    return distance <= _profile.maxDistanceToBanker;
}

float BankingManager::GetDistanceToNearestBanker()
{
    if (!_bot)
        return 999999.0f;

    // TODO: Find nearest banker NPC and calculate distance
    // For now return a large distance

    return 999999.0f;
}

bool BankingManager::TravelToNearestBanker()
{
    if (!_bot)
        return false;

    // TODO: Trigger bot movement to nearest banker
    TC_LOG_DEBUG("playerbot", "BankingManager: Bot {} traveling to banker", _bot->GetName());

    _statistics.bankTrips++;
    _globalStatistics.bankTrips++;

    return true;
}

// ============================================================================
// TRANSACTION HISTORY
// ============================================================================

std::vector<BankingTransaction> BankingManager::GetRecentTransactions(uint32 count)
{
    std::vector<BankingTransaction> result;

    uint32 start = _transactionHistory.size() > count ? _transactionHistory.size() - count : 0;
    result.assign(_transactionHistory.begin() + start, _transactionHistory.end());

    return result;
}

void BankingManager::RecordTransaction(BankingTransaction const& transaction)
{
    _transactionHistory.push_back(transaction);

    // Keep only last MAX_TRANSACTION_HISTORY transactions
    if (_transactionHistory.size() > MAX_TRANSACTION_HISTORY)
    {
        _transactionHistory.erase(_transactionHistory.begin(),
            _transactionHistory.begin() + (_transactionHistory.size() - MAX_TRANSACTION_HISTORY));
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

BankingStatistics const& BankingManager::GetStatistics() const
{
    return _statistics;
}

BankingStatistics const& BankingManager::GetGlobalStatistics()
{
    return _globalStatistics;
}

void BankingManager::ResetStatistics()
{
    _statistics.Reset();
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

void BankingManager::InitializeDefaultRules()
{
    // Copy default rules to this bot's profile
    _profile.customRules = _defaultRules;
}

void BankingManager::LoadBankingRules()
{
    // Initialize default banking rules (shared across all bots)
    _defaultRules.clear();

    // Rule: Never bank equipped items
    // (Handled by quest item check, no rule needed)

    // Rule: Bank excess trade goods (keep 40 in inventory)
    BankingRule tradeGoodsRule;
    tradeGoodsRule.itemClass = 7;  // ITEM_CLASS_TRADE_GOODS
    tradeGoodsRule.priority = BankingPriority::HIGH;
    tradeGoodsRule.keepInInventory = 40;
    tradeGoodsRule.maxInInventory = 80;
    _defaultRules.push_back(tradeGoodsRule);

    // Rule: Bank crafted items (keep 20 in inventory for selling)
    BankingRule craftedRule;
    craftedRule.itemClass = 1;  // ITEM_CLASS_CONSUMABLE or crafted
    tradeGoodsRule.priority = BankingPriority::MEDIUM;
    craftedRule.keepInInventory = 20;
    craftedRule.maxInInventory = 60;
    _defaultRules.push_back(craftedRule);

    TC_LOG_DEBUG("playerbot", "BankingManager: Loaded {} default banking rules", _defaultRules.size());
}

// ============================================================================
// BANKING LOGIC HELPERS
// ============================================================================

BankingRule const* BankingManager::FindBankingRule(uint32 itemId)
{
    // Check custom rules first
    for (auto const& rule : _profile.customRules)
    {
        if (ItemMatchesRule(itemId, rule))
            return &rule;
    }

    return nullptr;
}

BankingPriority BankingManager::CalculateItemPriority(uint32 itemId)
{
    if (!_bot)
        return BankingPriority::LOW;

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return BankingPriority::LOW;

    // Quest items - never bank
    if (itemTemplate->StartQuest != 0 || itemTemplate->FoodType != 0)
        return BankingPriority::NEVER_BANK;

    // Profession materials - check if needed
    if (IsNeededForProfessions(itemId))
        return BankingPriority::LOW;  // Keep in inventory

    // Default priority based on quality
    if (itemTemplate->Quality >= 4)  // Epic+
        return BankingPriority::HIGH;
    else if (itemTemplate->Quality >= 3)  // Rare
        return BankingPriority::MEDIUM;
    else
        return BankingPriority::LOW;
}

bool BankingManager::ItemMatchesRule(uint32 itemId, BankingRule const& rule)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return false;

    // Specific item ID match
    if (rule.itemId != 0 && rule.itemId == itemId)
        return true;

    // Item class match
    if (rule.itemClass != 0)
    {
        if (itemTemplate->Class != rule.itemClass)
            return false;

        // Item subclass match (if specified)
        if (rule.itemSubClass != 0 && itemTemplate->SubClass != rule.itemSubClass)
            return false;

        // Quality match (if specified)
        if (rule.itemQuality != 0 && itemTemplate->Quality != rule.itemQuality)
            return false;

        return true;
    }

    return false;
}

std::vector<BankingManager::DepositCandidate> BankingManager::GetDepositCandidates()
{
    std::vector<DepositCandidate> candidates;

    if (!_bot)
        return candidates;

    // Scan inventory for items to deposit
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!item)
            continue;

        uint32 itemId = item->GetEntry();
        uint32 count = item->GetCount();

        if (ShouldDepositItem(itemId, count))
        {
            DepositCandidate candidate;
            candidate.itemGuid = item->GetGUID().GetCounter();
            candidate.itemId = itemId;
            candidate.quantity = count;
            candidate.priority = GetItemBankingPriority(itemId);
            candidates.push_back(candidate);
        }
    }

    // Scan bags
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* pBag = _bot->GetBagByPos(bag);
        if (!pBag)
            continue;

        for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
        {
            Item* item = pBag->GetItemByPos(slot);
            if (!item)
                continue;

            uint32 itemId = item->GetEntry();
            uint32 count = item->GetCount();

            if (ShouldDepositItem(itemId, count))
            {
                DepositCandidate candidate;
                candidate.itemGuid = item->GetGUID().GetCounter();
                candidate.itemId = itemId;
                candidate.quantity = count;
                candidate.priority = GetItemBankingPriority(itemId);
                candidates.push_back(candidate);
            }
        }
    }

    return candidates;
}

std::vector<BankingManager::WithdrawRequest> BankingManager::GetWithdrawRequests()
{
    std::vector<WithdrawRequest> requests;

    if (!_bot)
        return requests;

    // Get material needs from ProfessionManager
    ProfessionManager* profMgr = GetProfessionManager();
    if (!profMgr)
        return requests;

    // TODO: Query ProfessionManager for needed materials
    // For now return empty

    return requests;
}

// ============================================================================
// INTEGRATION HELPERS
// ============================================================================

bool BankingManager::IsNeededForProfessions(uint32 itemId)
{
    ProfessionManager* profMgr = GetProfessionManager();
    if (!profMgr)
        return false;

    // TODO: Check with ProfessionManager if item is needed
    return false;
}

uint32 BankingManager::GetMaterialPriorityFromProfessions(uint32 itemId)
{
    ProfessionManager* profMgr = GetProfessionManager();
    if (!profMgr)
        return 0;

    // TODO: Get material priority from ProfessionManager
    return 0;
}

ProfessionManager* BankingManager::GetProfessionManager()
{
    if (!_bot)
        return nullptr;

    BotSession* session = static_cast<BotSession*>(_bot->GetSession());
    if (!session || !session->GetBotAI())
        return nullptr;

    return session->GetBotAI()->GetGameSystems()->GetProfessionManager();
}

} // namespace Playerbot
