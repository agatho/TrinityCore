/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LootDistribution.h"
#include "Player.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Loot.h"
#include "ObjectMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Util.h"
#include "World.h"
#include "WorldSession.h"
#include "ObjectAccessor.h"
#include "Session/BotSession.h"
#include "Spatial/SpatialGridQueryHelpers.h"
#include <algorithm>
#include <random>
#include "GameTime.h"

namespace Playerbot
{

LootDistribution* LootDistribution::instance()
{
    static LootDistribution instance;
    return &instance;
}

LootDistribution::LootDistribution()
{
    _globalMetrics.Reset();
}

void LootDistribution::HandleGroupLoot(Group* group, Loot* loot)
{
    if (!group || !loot)
        return;

    // Process each loot item
    for (::LootItem& lootItem : loot->items)
    {
        if (lootItem.itemid == 0)
            continue;

        // Convert to our LootItem structure
        Playerbot::LootItem ourLootItem;
        ourLootItem.itemId = lootItem.itemid;
        ourLootItem.itemCount = lootItem.count;
        ourLootItem.lootSlot = 0;
        PopulateLootItemData(ourLootItem);

        // Determine if this item needs rolling
        if (ShouldInitiateRoll(group, ourLootItem))
        {
            InitiateLootRoll(group, ourLootItem);
        }
        else
        {
            // Handle auto-loot for items that don't need rolling
            HandleAutoLoot(group, ourLootItem);
        }
    }
}

void LootDistribution::InitiateLootRoll(Group* group, const LootItem& item)
{
    if (!group)
        return;

    uint32 rollId = _nextRollId++;
    LootRoll roll(rollId, item.itemId, item.lootSlot, group->GetGUID().GetCounter());

    // PHASE 2H: Hybrid validation pattern (snapshot + ObjectAccessor fallback)
    for (auto const& slot : group->GetMemberSlots())
    {
        // Quick snapshot check first (fast, lock-free)
        auto memberSnapshot = SpatialGridQueryHelpers::FindPlayerByGuid(nullptr, slot.guid);
        if (!memberSnapshot)
            continue;

        // Fallback to ObjectAccessor for full validation
        Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
        if (member && CanParticipateInRoll(member, item))
        {
            roll.eligiblePlayers.insert(member->GetGUID().GetCounter());
        }
    }

    if (roll.eligiblePlayers.empty())
        return;

    // Store the roll
    {
        std::lock_guard lock(_lootMutex);
        _activeLootRolls[rollId] = roll;
        _rollTimeouts[rollId] = GameTime::GetGameTimeMS() + LOOT_ROLL_TIMEOUT;
    }

    // Broadcast roll to group members
    BroadcastLootRoll(group, roll);

    // PHASE 2H: Hybrid validation pattern for bot roll decisions
    for (uint32 memberGuid : roll.eligiblePlayers)
    {
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(memberGuid);

        // Quick snapshot check first
        auto memberSnapshot = SpatialGridQueryHelpers::FindPlayerByGuid(nullptr, guid);
        if (!memberSnapshot)
            continue;
        // Fallback to ObjectAccessor for full validation
        Player* member = ObjectAccessor::FindConnectedPlayer(guid);
        if (member && dynamic_cast<BotSession*>(member->GetSession()))
        {
            LootRollType decision = DetermineLootDecision(member, item);
            ProcessPlayerLootDecision(member, rollId, decision);
        }
    }
}
void LootDistribution::ProcessPlayerLootDecision(Player* player, uint32 rollId, LootRollType rollType)
{
    if (!player)
        return;

    std::lock_guard lock(_lootMutex);

    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    LootRoll& roll = rollIt->second;

    // Check if player is eligible
    if (roll.eligiblePlayers.find(player->GetGUID().GetCounter()) == roll.eligiblePlayers.end())
        return;

    // Record the player's decision
    roll.playerRolls[player->GetGUID().GetCounter()] = rollType;
    // Generate roll value if not passing
    if (rollType != LootRollType::PASS)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32> dis(1, 100);
        roll.rollValues[player->GetGUID().GetCounter()] = dis(gen);
    }

    // Check if all players have rolled
    if (roll.playerRolls.size() == roll.eligiblePlayers.size())
    {
        CompleteLootRoll(rollId);
    }
}

void LootDistribution::CompleteLootRoll(uint32 rollId)
{
    std::lock_guard lock(_lootMutex);

    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    LootRoll& roll = rollIt->second;

    // Determine winner
    uint32 winner = DetermineRollWinner(roll);

    if (winner != 0)
    {
        roll.winnerGuid = winner;
        roll.isCompleted = true;

        // Distribute loot to winner
        DistributeLootToWinner(rollId, winner);

        // Update fairness tracking
        LootItem item(roll.itemId, 1, roll.lootSlot);
        PopulateLootItemData(item);
        UpdateLootFairness(roll.groupId, winner, item);
    }

    // Notify group of results
    NotifyRollResult(roll);
    // Clean up
    _activeLootRolls.erase(rollIt);
    _rollTimeouts.erase(rollId);

    // Update metrics
    _globalMetrics.totalRollsCompleted++;
}

LootRollType LootDistribution::DetermineLootDecision(Player* player, const LootItem& item)
{
    if (!player)
        return LootRollType::PASS;

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Get player's loot profile
    PlayerLootProfile profile = GetPlayerLootProfile(playerGuid);

    // Execute the player's strategy
    return ExecuteStrategy(player, item, profile.strategy);
}

LootPriority LootDistribution::AnalyzeItemPriority(Player* player, const LootItem& item)
{
    if (!player)
        return LootPriority::NOT_USEFUL;

    // Check if item is an upgrade
    if (IsItemUpgrade(player, item))
    {
        // Determine upgrade significance
        float upgradeValue = CalculateUpgradeValue(player, item);

        if (upgradeValue > 0.3f)
            return LootPriority::CRITICAL_UPGRADE;
        else if (upgradeValue > 0.15f)
            return LootPriority::SIGNIFICANT_UPGRADE;
        else if (upgradeValue > 0.05f)
            return LootPriority::MINOR_UPGRADE;
        else
            return LootPriority::SIDEGRADE;
    }

    // Check if useful for off-spec
    if (IsItemUsefulForOffSpec(player, item))
        return LootPriority::OFF_SPEC_UPGRADE;

    // Check vendor value
    if (item.vendorValue > 1000) // Arbitrary threshold
        return LootPriority::VENDOR_ITEM;
    return LootPriority::NOT_USEFUL;
}

bool LootDistribution::IsItemUpgrade(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return false;

    // Check if item can be equipped by player
    if (!player->CanUseItem(item.itemTemplate))
        return false;

    // Compare with currently equipped item
    uint8 slot = item.itemTemplate->GetInventoryType();
    Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!equippedItem)
        return true; // No item equipped, so this is an upgrade

    // Compare item levels and stats
    float currentScore = CalculateItemScore(player, equippedItem);
    float newScore = CalculateItemScore(player, item);

    return newScore > currentScore * (1.0f + UPGRADE_THRESHOLD);
}

bool LootDistribution::IsClassAppropriate(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return false;

    // Check class restrictions
    if (item.isClassRestricted)
    {
        uint8 playerClass = player->GetClass();
        return std::find(item.allowedClasses.begin(), item.allowedClasses.end(), playerClass) != item.allowedClasses.end();
    }

    // Check if item type is useful for class
    return IsItemTypeUsefulForClass(player->GetClass(), item.itemTemplate);
}

bool LootDistribution::CanPlayerNeedItem(Player* player, const LootItem& item)
{
    if (!player)
        return false;

    // Must be class appropriate
    if (!IsClassAppropriate(player, item))
        return false;

    // Must be an upgrade for main spec
    if (!IsItemUpgrade(player, item))
        return false;

    // Check if item is for main spec
    return IsItemForMainSpec(player, item);
}

bool LootDistribution::ShouldPlayerGreedItem(Player* player, const LootItem& item)
{
    if (!player)
        return false;

    PlayerLootProfile profile = GetPlayerLootProfile(player->GetGUID().GetCounter());
    // Check greed threshold
    if (item.vendorValue < profile.greedThreshold * 10000) // Convert threshold to copper
        return false;

    // Don't greed if someone else can need it (simplified)
    return true;
}

bool LootDistribution::ShouldPlayerPassItem(Player* player, const LootItem& item)
{
    if (!player)
        return true;

    // Pass if item is blacklisted
    PlayerLootProfile profile = GetPlayerLootProfile(player->GetGUID().GetCounter());
    if (profile.blacklistedItems.find(item.itemId) != profile.blacklistedItems.end())
        return true;

    // Pass if not useful and below greed threshold
    if (!CanPlayerNeedItem(player, item) && !ShouldPlayerGreedItem(player, item))
        return true;

    return false;
}

bool LootDistribution::CanPlayerDisenchantItem(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return false;

    // Check if player has enchanting skill
    if (player->GetSkillValue(SKILL_ENCHANTING) == 0)
        return false;

    // Check if item can be disenchanted
    return Item::GetBaseDisenchantLoot(item.itemTemplate, item.itemQuality, item.itemLevel) != nullptr;
}

void LootDistribution::ProcessLootRolls(uint32 rollId)
{
    std::lock_guard lock(_lootMutex);

    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    LootRoll& roll = rollIt->second;

    // Check for timeout
    if (GameTime::GetGameTimeMS() > roll.rollTimeout)
    {
        HandleLootRollTimeout(rollId);
        return;
    }

    // Process the roll if all players have responded
    if (roll.playerRolls.size() == roll.eligiblePlayers.size())
    {
        CompleteLootRoll(rollId);
    }
}

uint32 LootDistribution::DetermineRollWinner(const LootRoll& roll)
{
    // Process need rolls first
    uint32 needWinner = ProcessNeedRolls(const_cast<LootRoll&>(roll));
    if (needWinner != 0)
        return needWinner;

    // Then greed rolls
    uint32 greedWinner = ProcessGreedRolls(const_cast<LootRoll&>(roll));
    if (greedWinner != 0)
        return greedWinner;

    // Finally disenchant rolls
    uint32 disenchantWinner = ProcessDisenchantRolls(const_cast<LootRoll&>(roll));
    if (disenchantWinner != 0)
        return disenchantWinner;

    return 0; // No winner
}

void LootDistribution::DistributeLootToWinner(uint32 rollId, uint32 winnerGuid)
{
    // PHASE 2H HOTFIX: Removed snapshot pre-check to eliminate TOCTOU race condition
    // The snapshot check created a window where player could be deleted between check and use
    // Now using ObjectAccessor directly with proper null checking (thread-safe)
    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(winnerGuid);
    Player* winner = ObjectAccessor::FindConnectedPlayer(guid);
    if (!winner)
        return;

    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    const LootRoll& roll = rollIt->second;

    // Create item and add to winner's inventory
    ItemPosCountVec dest;
    InventoryResult msg = winner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll.itemId, 1);

    if (msg == EQUIP_ERR_OK)
    {
        Item* item = winner->StoreNewItem(dest, roll.itemId, true);
        if (item)
        {
            winner->SendNewItem(item, 1, false, false, true);

            // Update metrics
            UpdateLootMetrics(winnerGuid, roll, true);
        }
    }
    else
    {
        // Handle inventory full case
        // Could send item by mail or handle differently
        TC_LOG_WARN("playerbot.loot", "Cannot distribute loot to player {}: inventory full", winnerGuid);
    }
}

void LootDistribution::HandleLootRollTimeout(uint32 rollId)
{
    std::lock_guard lock(_lootMutex);

    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    LootRoll& roll = rollIt->second;

    // Auto-pass for players who didn't respond
    for (uint32 playerGuid : roll.eligiblePlayers)
    {
        if (roll.playerRolls.find(playerGuid) == roll.playerRolls.end())
        {
            roll.playerRolls[playerGuid] = LootRollType::PASS;
        }
    }

    // Complete the roll
    CompleteLootRoll(rollId);

    // Update timeout metrics
    _globalMetrics.rollTimeouts++;
}

void LootDistribution::ExecuteNeedBeforeGreedStrategy(Player* player, const LootItem& item, LootRollType& decision)
{
    if (CanPlayerNeedItem(player, item))
    {
        decision = LootRollType::NEED;
    }
    else if (ShouldPlayerGreedItem(player, item))
    {
        decision = LootRollType::GREED;
    }
    else
    {
        decision = LootRollType::PASS;
    }
}

void LootDistribution::ExecuteClassPriorityStrategy(Player* player, const LootItem& item, LootRollType& decision)
{
    // Prioritize items for appropriate classes
    if (IsClassAppropriate(player, item))
    {
        if (IsItemUpgrade(player, item))
            decision = LootRollType::NEED;
        else
            decision = LootRollType::GREED;
    }
    else
    {
        decision = LootRollType::PASS;
    }
}

void LootDistribution::ExecuteUpgradePriorityStrategy(Player* player, const LootItem& item, LootRollType& decision)
{
    LootPriority priority = AnalyzeItemPriority(player, item);
    switch (priority)
    {
        case LootPriority::CRITICAL_UPGRADE:
        case LootPriority::SIGNIFICANT_UPGRADE:
            decision = LootRollType::NEED;
            break;
        case LootPriority::MINOR_UPGRADE:
        case LootPriority::OFF_SPEC_UPGRADE:
            decision = LootRollType::GREED;
            break;
        case LootPriority::VENDOR_ITEM:
            if (ShouldPlayerGreedItem(player, item))
                decision = LootRollType::GREED;
            else
                decision = LootRollType::PASS;
            break;
        default:
            decision = LootRollType::PASS;
            break;
    }
}

void LootDistribution::ExecuteFairDistributionStrategy(Player* player, const LootItem& item, LootRollType& decision)
{
    // Consider fairness in decision making
    Group* group = player->GetGroup();
    if (!group)
    {
        ExecuteNeedBeforeGreedStrategy(player, item, decision);
        return;
    }

    // Check if player has received items recently
    bool shouldConsiderFairness = ShouldConsiderFairnessAdjustment(group, player);

    if (shouldConsiderFairness)
    {
        // Be more conservative with rolls
        if (CanPlayerNeedItem(player, item))
        {
            LootPriority priority = AnalyzeItemPriority(player, item);
            if (priority >= LootPriority::SIGNIFICANT_UPGRADE)
                decision = LootRollType::NEED;
            else
                decision = LootRollType::GREED;
        }
        else
        {
            decision = LootRollType::PASS;
        }
    }
    else
    {
        ExecuteNeedBeforeGreedStrategy(player, item, decision);
    }
}

void LootDistribution::ExecuteMainSpecPriorityStrategy(Player* player, const LootItem& item, LootRollType& decision)
{
    PlayerLootProfile profile = GetPlayerLootProfile(player->GetGUID().GetCounter());
    if (IsItemForMainSpec(player, item))
    {
        if (IsItemUpgrade(player, item))
            decision = LootRollType::NEED;
        else if (ShouldPlayerGreedItem(player, item))
            decision = LootRollType::GREED;
        else
            decision = LootRollType::PASS;
    }
    else if (profile.greedOffSpec && IsItemUsefulForOffSpec(player, item))
    {
        decision = LootRollType::GREED;
    }
    else
    {
        decision = LootRollType::PASS;
    }
}

LootDistribution::LootFairnessTracker LootDistribution::GetGroupLootFairness(uint32 groupId)
{
    std::lock_guard lock(_lootMutex);
    auto it = _groupFairnessTracking.find(groupId);
    if (it != _groupFairnessTracking.end())
        return it->second;

    return LootFairnessTracker();
}

void LootDistribution::UpdateLootFairness(uint32 groupId, uint32 winnerGuid, const LootItem& item)
{
    std::lock_guard lock(_lootMutex);

    auto& tracker = _groupFairnessTracking[groupId];

    tracker.playerLootCount[winnerGuid]++;
    tracker.playerLootValue[winnerGuid] += item.vendorValue;
    tracker.totalItemsDistributed++;
    tracker.totalValueDistributed += item.vendorValue;

    // Recalculate fairness score
    tracker.fairnessScore = CalculateFairnessScore(tracker);
}

float LootDistribution::CalculateFairnessScore(const LootFairnessTracker& tracker)
{
    if (tracker.playerLootCount.empty())
        return 1.0f;

    // Calculate variance in loot distribution
    float averageItems = float(tracker.totalItemsDistributed) / tracker.playerLootCount.size();
    float variance = 0.0f;

    for (const auto& pair : tracker.playerLootCount)
    {
        float diff = pair.second - averageItems;
        variance += diff * diff;
    }

    variance /= tracker.playerLootCount.size();

    // Convert variance to fairness score (lower variance = higher fairness)
    float fairness = 1.0f / (1.0f + variance);
    return std::clamp(fairness, 0.0f, 1.0f);
}

LootDistribution::LootMetrics LootDistribution::GetPlayerLootMetrics(uint32 playerGuid)
{
    std::lock_guard lock(_lootMutex);
    auto it = _playerMetrics.find(playerGuid);
    if (it != _playerMetrics.end())
        return it->second;

    LootMetrics metrics;
    metrics.Reset();
    return metrics;
}

LootDistribution::LootMetrics LootDistribution::GetGroupLootMetrics(uint32 groupId)
{
    // Aggregate metrics from all group members
    LootMetrics groupMetrics;
    groupMetrics.Reset();

    // In a real implementation, we would iterate through group members
    // and aggregate their individual metrics

    return groupMetrics;
}

LootDistribution::LootMetrics LootDistribution::GetGlobalLootMetrics()
{
    return _globalMetrics;
}

void LootDistribution::SetPlayerLootStrategy(uint32 playerGuid, LootDecisionStrategy strategy)
{
    std::lock_guard lock(_lootMutex);
    _playerLootProfiles[playerGuid].strategy = strategy;
}

LootDecisionStrategy LootDistribution::GetPlayerLootStrategy(uint32 playerGuid)
{
    std::lock_guard lock(_lootMutex);
    auto it = _playerLootProfiles.find(playerGuid);
    if (it != _playerLootProfiles.end())
        return it->second.strategy;

    return LootDecisionStrategy::NEED_BEFORE_GREED;
}

void LootDistribution::SetPlayerLootPreferences(uint32 playerGuid, const PlayerLootProfile& profile)
{
    std::lock_guard lock(_lootMutex);
    _playerLootProfiles[playerGuid] = profile;
}

PlayerLootProfile LootDistribution::GetPlayerLootProfile(uint32 playerGuid)
{
    std::lock_guard lock(_lootMutex);
    auto it = _playerLootProfiles.find(playerGuid);
    if (it != _playerLootProfiles.end())
        return it->second;

    // PHASE 2H HOTFIX: Snapshot-only validation (eliminates TOCTOU race condition)
    // Using ObjectAccessor after snapshot check creates race condition where player can be
    // deleted between snapshot validation and ObjectAccessor call, causing crashes
    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(playerGuid);
    auto playerSnapshot = SpatialGridQueryHelpers::FindPlayerByGuid(nullptr, guid);
    if (!playerSnapshot)
        return PlayerLootProfile(playerGuid, CLASS_WARRIOR, 0);  // Player offline - return default

    // Use snapshot class data only (safe, no TOCTOU race)
    // PlayerSnapshot doesn't have spec, so we default to 0 (spec will be learned from actual loot behavior)
    if (playerSnapshot->classId != 0)
        return PlayerLootProfile(playerGuid, static_cast<Classes>(playerSnapshot->classId), 0);

    return PlayerLootProfile(playerGuid, CLASS_WARRIOR, 0);
}

void LootDistribution::PopulateLootItemData(LootItem& item)
{
    item.itemTemplate = sObjectMgr->GetItemTemplate(item.itemId);
    if (!item.itemTemplate)
        return;

    item.itemLevel = item.itemTemplate->GetBaseItemLevel();
    item.itemQuality = item.itemTemplate->GetQuality();
    item.vendorValue = item.itemTemplate->GetSellPrice();
    item.itemName = item.itemTemplate->GetName(LOCALE_enUS);

    // Set binding information
    item.isBoundOnPickup = item.itemTemplate->GetBonding() == BIND_ON_ACQUIRE;
    item.isBoundOnEquip = item.itemTemplate->GetBonding() == BIND_ON_EQUIP;

    // Check class restrictions
    if (item.itemTemplate->GetAllowableClass() != 0)
    {
        item.isClassRestricted = true;
        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
        {
            if (item.itemTemplate->GetAllowableClass() & (1 << (cls - 1)))
            {
                item.allowedClasses.push_back(cls);
            }
        }
    }
}

bool LootDistribution::ShouldInitiateRoll(Group* group, const LootItem& item)
{
    if (!group || !item.itemTemplate)
        return false;

    // Check if item quality meets roll threshold
    if (item.itemQuality < group->GetLootThreshold())
        return false;

    // Check if multiple players can use the item
    uint32 interestedPlayers = 0;
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (member && CanParticipateInRoll(member, item))
        {
            interestedPlayers++;
            if (interestedPlayers >= 2)
                return true;
        }
    }

    return false;
}

bool LootDistribution::CanParticipateInRoll(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return false;

    // Player must be able to use the item
    if (!player->CanUseItem(item.itemTemplate))
        return false;

    // Player must be in range (simplified check)
    return true;
}

void LootDistribution::HandleAutoLoot(Group* group, const LootItem& item)
{
    if (!group)
        return;

    // Find a suitable recipient for auto-loot
    Player* recipient = nullptr;

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (member && CanParticipateInRoll(member, item))
        {
            recipient = member;
            break;
        }
    }

    if (recipient)
    {
        // Give item to recipient
        ItemPosCountVec dest;
        InventoryResult msg = recipient->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemId, item.itemCount);
        if (msg == EQUIP_ERR_OK)
        {
            Item* newItem = recipient->StoreNewItem(dest, item.itemId, true);
            if (newItem)
            {
                recipient->SendNewItem(newItem, item.itemCount, false, false, true);
            }
        }
    }
}

LootRollType LootDistribution::ExecuteStrategy(Player* player, const LootItem& item, LootDecisionStrategy strategy)
{
    LootRollType decision = LootRollType::PASS;

    switch (strategy)
    {
        case LootDecisionStrategy::NEED_BEFORE_GREED:
            ExecuteNeedBeforeGreedStrategy(player, item, decision);
            break;
        case LootDecisionStrategy::CLASS_PRIORITY:
            ExecuteClassPriorityStrategy(player, item, decision);
            break;
        case LootDecisionStrategy::UPGRADE_PRIORITY:
            ExecuteUpgradePriorityStrategy(player, item, decision);
            break;
        case LootDecisionStrategy::FAIR_DISTRIBUTION:
            ExecuteFairDistributionStrategy(player, item, decision);
            break;
        case LootDecisionStrategy::MAINSPEC_PRIORITY:
            ExecuteMainSpecPriorityStrategy(player, item, decision);
            break;
        case LootDecisionStrategy::RANDOM_ROLLS:
            // Random decision
            {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dis(0, 2);
                decision = static_cast<LootRollType>(dis(gen));
            }
            break;
        case LootDecisionStrategy::VENDOR_VALUE:
            if (item.vendorValue > 1000)
                decision = LootRollType::GREED;
            else
                decision = LootRollType::PASS;
            break;
        case LootDecisionStrategy::CONSERVATIVE:
            // Only roll on significant upgrades
            {
                LootPriority priority = AnalyzeItemPriority(player, item);
                if (priority == LootPriority::CRITICAL_UPGRADE)
                    decision = LootRollType::NEED;
                else
                    decision = LootRollType::PASS;
            }
            break;
    }

    // Apply strategy modifiers
    ApplyStrategyModifiers(player, item, decision);

    return decision;
}

void LootDistribution::ApplyStrategyModifiers(Player* player, const LootItem& item, LootRollType& decision)
{
    if (!player)
        return;

    Group* group = player->GetGroup();
    if (group)
    {
        ConsiderGroupComposition(group, player, item, decision);
    }
}

void LootDistribution::ConsiderGroupComposition(Group* group, Player* player, const LootItem& item, LootRollType& decision)
{
    if (!group || !player)
        return;

    // Analyze group composition and adjust decision accordingly
    // For example, if multiple players of the same class are present,
    // be more conservative with rolling
}

float LootDistribution::CalculateItemScore(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return 0.0f;

    float score = 0.0f;

    // Base score from item level
    score += item.itemLevel * 10.0f;

    // Add score for relevant stats
    // This would require detailed stat analysis based on player class/spec

    return score;
}

float LootDistribution::CalculateItemScore(Player* player, Item* item)
{
    if (!player || !item)
        return 0.0f;

    float score = 0.0f;

    // Base score from item level
    score += item->GetItemLevel(player) * 10.0f;

    // Add score for relevant stats
    // This would require detailed stat analysis

    return score;
}

float LootDistribution::CalculateUpgradeValue(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return 0.0f;

    // Get current item in the same slot
    uint8 slot = item.itemTemplate->GetInventoryType();
    Item* currentItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!currentItem)
        return 1.0f; // Maximum upgrade if no item equipped

    float currentScore = CalculateItemScore(player, currentItem);
    float newScore = CalculateItemScore(player, item);

    if (currentScore <= 0.0f)
        return 1.0f;

    return (newScore - currentScore) / currentScore;
}

bool LootDistribution::IsItemForMainSpec(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return false;

    // Simplified check based on item type and player spec
    // In a real implementation, this would be more sophisticated

    uint8 playerClass = player->GetClass();
    uint8 spec = AsUnderlyingType(player->GetPrimarySpecialization());

    // Basic logic for different classes
    switch (playerClass)
    {
        case CLASS_WARRIOR:
            if (spec == 2) // Protection
                return item.itemTemplate->GetInventoryType() == INVTYPE_SHIELD ||
                       item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE;
            else // Arms/Fury
                return item.itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        case CLASS_PALADIN:
            if (spec == 1) // Protection
                return item.itemTemplate->GetInventoryType() == INVTYPE_SHIELD ||
                       item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE;
            else if (spec == 0) // Holy
                return item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE ||
                       (item.itemTemplate->GetClass() == ITEM_CLASS_WEAPON &&
                        item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_WEAPON_MACE);
            else // Retribution
                return item.itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        // Add more classes as needed
        default:
            return true; // Default to allowing all items
    }
}

bool LootDistribution::IsItemUsefulForOffSpec(Player* player, const LootItem& item)
{
    if (!player || !item.itemTemplate)
        return false;

    // Check if item could be useful for an alternative specialization
    // This is a simplified implementation

    return player->CanUseItem(item.itemTemplate);
}

bool LootDistribution::IsItemTypeUsefulForClass(uint8 playerClass, const ItemTemplate* itemTemplate)
{
    if (!itemTemplate)
        return false;

    // Check if item type is generally useful for the class
    switch (playerClass)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE ||
                   itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_MAIL ||
                   itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        case CLASS_ROGUE:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_LEATHER ||
                   itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_CLOTH ||
                   itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        default:
            return true;
    }
}

uint32 LootDistribution::ProcessNeedRolls(LootRoll& roll)
{
    uint32 highestRoll = 0;
    uint32 winner = 0;

    for (const auto& playerRoll : roll.playerRolls)
    {
        if (playerRoll.second == LootRollType::NEED)
        {
            uint32 rollValue = roll.rollValues[playerRoll.first];
            if (rollValue > highestRoll)
            {
                highestRoll = rollValue;
                winner = playerRoll.first;
            }
        }
    }

    if (winner != 0)
    {
        roll.winningRollType = LootRollType::NEED;
        _globalMetrics.needRollsWon++;
    }

    return winner;
}

uint32 LootDistribution::ProcessGreedRolls(LootRoll& roll)
{
    uint32 highestRoll = 0;
    uint32 winner = 0;

    for (const auto& playerRoll : roll.playerRolls)
    {
        if (playerRoll.second == LootRollType::GREED)
        {
            uint32 rollValue = roll.rollValues[playerRoll.first];
            if (rollValue > highestRoll)
            {
                highestRoll = rollValue;
                winner = playerRoll.first;
            }
        }
    }

    if (winner != 0)
    {
        roll.winningRollType = LootRollType::GREED;
        _globalMetrics.greedRollsWon++;
    }

    return winner;
}

uint32 LootDistribution::ProcessDisenchantRolls(LootRoll& roll)
{
    uint32 highestRoll = 0;
    uint32 winner = 0;

    for (const auto& playerRoll : roll.playerRolls)
    {
        if (playerRoll.second == LootRollType::DISENCHANT)
        {
            uint32 rollValue = roll.rollValues[playerRoll.first];
            if (rollValue > highestRoll)
            {
                highestRoll = rollValue;
                winner = playerRoll.first;
            }
        }
    }

    if (winner != 0)
    {
        roll.winningRollType = LootRollType::DISENCHANT;
    }

    return winner;
}

void LootDistribution::BroadcastLootRoll(Group* group, const LootRoll& roll)
{
    if (!group)
        return;

    // Broadcast roll information to all group members
    // In a real implementation, this would send appropriate packets
    TC_LOG_DEBUG("playerbot.loot", "Broadcasting loot roll {} for item {} to group {}",
                roll.rollId, roll.itemId, roll.groupId);
}

void LootDistribution::NotifyRollResult(const LootRoll& roll)
{
    // Notify group members of roll results
    // In a real implementation, this would send result packets
    TC_LOG_DEBUG("playerbot.loot", "Loot roll {} completed. Winner: {}, Item: {}",
                roll.rollId, roll.winnerGuid, roll.itemId);
}

bool LootDistribution::ShouldConsiderFairnessAdjustment(Group* group, Player* player)
{
    if (!group || !player)
        return false;

    LootFairnessTracker tracker = GetGroupLootFairness(group->GetGUID().GetCounter());
    return tracker.fairnessScore < FAIRNESS_ADJUSTMENT_THRESHOLD;
}

void LootDistribution::UpdateLootMetrics(uint32 playerGuid, const LootRoll& roll, bool wasWinner)
{
    std::lock_guard lock(_lootMutex);

    auto& metrics = _playerMetrics[playerGuid];

    if (wasWinner)
    {
        switch (roll.winningRollType)
        {
            case LootRollType::NEED:
                metrics.needRollsWon++;
                break;
            case LootRollType::GREED:
                metrics.greedRollsWon++;
                break;
            default:
                break;
        }
    }

    metrics.lastUpdate = std::chrono::steady_clock::now();
}

void LootDistribution::Update(uint32 diff)
{
    static uint32 lastUpdate = 0;
    uint32 currentTime = GameTime::GetGameTimeMS();

    if (currentTime - lastUpdate < LOOT_UPDATE_INTERVAL)
        return;

    lastUpdate = currentTime;

    // Process active rolls
    ProcessActiveLootRolls();

    // Clean up expired rolls
    CleanupExpiredRolls();

    // Validate loot states
    ValidateLootStates();
}

void LootDistribution::ProcessActiveLootRolls()
{
    std::vector<uint32> rollsToProcess;

    {
        std::lock_guard lock(_lootMutex);
        for (const auto& rollPair : _activeLootRolls)
        {
            rollsToProcess.push_back(rollPair.first);
        }
    }

    for (uint32 rollId : rollsToProcess)
    {
        ProcessLootRolls(rollId);
    }
}

void LootDistribution::CleanupExpiredRolls()
{
    std::lock_guard lock(_lootMutex);

    uint32 currentTime = GameTime::GetGameTimeMS();
    std::vector<uint32> expiredRolls;

    for (const auto& timeoutPair : _rollTimeouts)
    {
        if (currentTime > timeoutPair.second)
        {
            expiredRolls.push_back(timeoutPair.first);
        }
    }

    for (uint32 rollId : expiredRolls)
    {
        HandleLootRollTimeout(rollId);
    }
}

void LootDistribution::ValidateLootStates()
{
    // Validate that loot system state is consistent
    // Clean up any orphaned data
    // Detect and handle any inconsistencies
}

} // namespace Playerbot