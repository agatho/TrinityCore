/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include "Group.h"
#include "Item.h"
#include "Loot.h"

namespace Playerbot
{

// Forward declarations
struct LootItem;
enum class LootRollType : uint8;
enum class LootPriority : uint8;
enum class LootDecisionStrategy : uint8;
struct PlayerLootProfile;

class TC_GAME_API ILootDistribution
{
public:
    virtual ~ILootDistribution() = default;

    // Core loot distribution functionality
    virtual void HandleGroupLoot(Group* group, Loot* loot) = 0;
    virtual void InitiateLootRoll(Group* group, const LootItem& item) = 0;
    virtual void ProcessPlayerLootDecision(Player* player, uint32 rollId, LootRollType rollType) = 0;
    virtual void CompleteLootRoll(uint32 rollId) = 0;

    // Loot analysis and decision making
    virtual LootRollType DetermineLootDecision(Player* player, const LootItem& item) = 0;
    virtual LootPriority AnalyzeItemPriority(Player* player, const LootItem& item) = 0;
    virtual bool IsItemUpgrade(Player* player, const LootItem& item) = 0;
    virtual bool IsClassAppropriate(Player* player, const LootItem& item) = 0;

    // Need/Greed/Pass logic implementation
    virtual bool CanPlayerNeedItem(Player* player, const LootItem& item) = 0;
    virtual bool ShouldPlayerGreedItem(Player* player, const LootItem& item) = 0;

    // Roll processing and winner determination
    virtual void ProcessLootRolls(uint32 rollId) = 0;
    virtual uint32 DetermineRollWinner(const LootRoll& roll) = 0;
    virtual void DistributeLootToWinner(uint32 rollId, uint32 winnerGuid) = 0;
    virtual void HandleLootRollTimeout(uint32 rollId) = 0;

    // Group loot settings and policies
    virtual void SetGroupLootMethod(Group* group, LootMethod method) = 0;
    virtual void SetGroupLootThreshold(Group* group, ItemQualities threshold) = 0;

    // Loot fairness and distribution tracking
    virtual LootFairnessTracker GetGroupLootFairness(uint32 groupId) = 0;

    // Performance monitoring
    virtual LootMetrics GetPlayerLootMetrics(uint32 playerGuid) = 0;
    virtual LootMetrics GetGroupLootMetrics(uint32 groupId) = 0;
    virtual LootMetrics GetGlobalLootMetrics() = 0;

    // Player preferences and configuration
    virtual void SetPlayerLootStrategy(uint32 playerGuid, LootDecisionStrategy strategy) = 0;
    virtual LootDecisionStrategy GetPlayerLootStrategy(uint32 playerGuid) = 0;

    // Error handling and edge cases
    virtual void HandleLootConflicts(uint32 rollId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessActiveLootRolls() = 0;
    virtual void CleanupExpiredRolls() = 0;
};

} // namespace Playerbot
