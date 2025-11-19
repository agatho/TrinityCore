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
#include "Threading/LockHierarchy.h"
#include "Core/DI/Interfaces/IUnifiedLootManager.h"
#include "LootDistribution.h"
#include "Player.h"
#include "Group.h"
#include "Item.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>
#include "GameTime.h"

namespace Playerbot
{

/**
 * @brief Unified loot management system
 *
 * Consolidates three separate managers into one cohesive system:
 * - LootAnalysis: Item evaluation and upgrade detection
 * - LootCoordination: Session management and orchestration
 * - LootDistribution: Roll handling and distribution execution
 *
 * **Architecture:**
 * ```
 * UnifiedLootManager
 *   ├─> AnalysisModule     (item scoring, upgrade detection)
 *   ├─> CoordinationModule (session management, orchestration)
 *   └─> DistributionModule (roll handling, distribution)
 * ```
 *
 * **Thread Safety:**
 * - Uses OrderedMutex<LOOT_MANAGER> for all operations
 * - Modules share data through thread-safe interfaces
 * - Lock ordering prevents deadlocks
 *
 * **Migration Path:**
 * - Old managers (LootAnalysis, LootCoordination, LootDistribution) still work
 * - New code should use UnifiedLootManager
 * - Gradually migrate callsites over time
 * - Eventually deprecate old managers
 */
class TC_GAME_API UnifiedLootManager final : public IUnifiedLootManager
{
public:
    static UnifiedLootManager* instance();

    // Singleton management
    UnifiedLootManager();
    ~UnifiedLootManager() override;

    // Non-copyable, non-movable
    UnifiedLootManager(UnifiedLootManager const&) = delete;
    UnifiedLootManager& operator=(UnifiedLootManager const&) = delete;
    UnifiedLootManager(UnifiedLootManager&&) = delete;
    UnifiedLootManager& operator=(UnifiedLootManager&&) = delete;

    // ========================================================================
    // ANALYSIS MODULE INTERFACE
    // ========================================================================

    float CalculateItemValue(Player* player, LootItem const& item) override;
    float CalculateUpgradeValue(Player* player, LootItem const& item) override;
    bool IsSignificantUpgrade(Player* player, LootItem const& item) override;
    float CalculateStatWeight(Player* player, uint32 statType) override;
    float CompareItems(Player* player, LootItem const& newItem, Item const* currentItem) override;
    float CalculateItemScore(Player* player, LootItem const& item) override;
    std::vector<std::pair<uint32, float>> GetStatPriorities(Player* player) override;

    // ========================================================================
    // COORDINATION MODULE INTERFACE
    // ========================================================================

    void InitiateLootSession(Group* group, Loot* loot) override;
    void ProcessLootSession(Group* group, uint32 lootSessionId) override;
    void CompleteLootSession(uint32 lootSessionId) override;
    void HandleLootSessionTimeout(uint32 lootSessionId) override;
    void OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items) override;
    void PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items) override;
    void OptimizeLootSequence(Group* group, std::vector<LootItem>& items) override;
    void FacilitateGroupLootDiscussion(Group* group, LootItem const& item) override;
    void HandleLootConflictResolution(Group* group, LootItem const& item) override;
    void BroadcastLootRecommendations(Group* group, LootItem const& item) override;
    void OptimizeLootEfficiency(Group* group) override;
    void MinimizeLootTime(Group* group, uint32 sessionId) override;
    void MaximizeLootFairness(Group* group, uint32 sessionId) override;

    // ========================================================================
    // DISTRIBUTION MODULE INTERFACE
    // ========================================================================

    void DistributeLoot(Group* group, LootItem const& item) override;
    void HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType) override;
    LootRollType DetermineLootDecision(Player* player, LootItem const& item,
                                        LootDecisionStrategy strategy) override;
    LootPriority CalculateLootPriority(Player* player, LootItem const& item) override;
    bool ShouldRollNeed(Player* player, LootItem const& item) override;
    bool ShouldRollGreed(Player* player, LootItem const& item) override;
    bool IsItemForClass(Player* player, LootItem const& item) override;
    bool IsItemForMainSpec(Player* player, LootItem const& item) override;
    bool IsItemForOffSpec(Player* player, LootItem const& item) override;
    void ExecuteLootDistribution(Group* group, uint32 rollId) override;
    void ResolveRollTies(Group* group, uint32 rollId) override;
    void HandleLootNinja(Group* group, uint32 suspectedPlayer) override;

    // ========================================================================
    // UNIFIED OPERATIONS
    // ========================================================================

    void ProcessCompleteLootFlow(Group* group, Loot* loot) override;
    std::string GetLootRecommendation(Player* player, LootItem const& item) override;
    std::string GetLootStatistics() const override;

private:
    // ========================================================================
    // INTERNAL MODULES
    // ========================================================================

    /**
     * @brief Analysis module - item evaluation and scoring
     */
    class AnalysisModule
    {
    public:
        float CalculateItemValue(Player* player, LootItem const& item);
        float CalculateUpgradeValue(Player* player, LootItem const& item);
        bool IsSignificantUpgrade(Player* player, LootItem const& item);
        float CalculateStatWeight(Player* player, uint32 statType);
        float CompareItems(Player* player, LootItem const& newItem, Item const* currentItem);
        float CalculateItemScore(Player* player, LootItem const& item);
        std::vector<std::pair<uint32, float>> GetStatPriorities(Player* player);

    private:
        float CalculateArmorValue(Player* player, LootItem const& item);
        float CalculateWeaponValue(Player* player, LootItem const& item);
        float CalculateAccessoryValue(Player* player, LootItem const& item);
        bool IsItemForMainSpec(Player* player, LootItem const& item);
        bool IsItemForOffSpec(Player* player, LootItem const& item);

        // Statistics
        std::atomic<uint64> _itemsAnalyzed{0};
        std::atomic<uint64> _upgradesDetected{0};
    };

    /**
     * @brief Coordination module - session management and orchestration
     */
    class CoordinationModule
    {
    public:
        void InitiateLootSession(Group* group, Loot* loot);
        void ProcessLootSession(Group* group, uint32 lootSessionId);
        void CompleteLootSession(uint32 lootSessionId);
        void HandleLootSessionTimeout(uint32 lootSessionId);
        void OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items);
        void PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items);
        void OptimizeLootSequence(Group* group, std::vector<LootItem>& items);
        void FacilitateGroupLootDiscussion(Group* group, LootItem const& item);
        void HandleLootConflictResolution(Group* group, LootItem const& item);
        void BroadcastLootRecommendations(Group* group, LootItem const& item);
        void OptimizeLootEfficiency(Group* group);
        void MinimizeLootTime(Group* group, uint32 sessionId);
        void MaximizeLootFairness(Group* group, uint32 sessionId);

    private:
        struct LootSession
        {
            uint32 sessionId;
            uint32 groupId;
            std::vector<LootItem> availableItems;
            std::vector<uint32> activeRolls;
            uint32 sessionStartTime;
            uint32 sessionTimeout;
            bool isActive;

            LootSession(uint32 id, uint32 gId)
                : sessionId(id), groupId(gId)
                , sessionStartTime(GameTime::GetGameTimeMS())
                , sessionTimeout(GameTime::GetGameTimeMS() + 300000) // 5 minutes
                , isActive(true)
            {}
        };

        std::unordered_map<uint32, LootSession> _activeSessions;
        uint32 _nextSessionId{1};
        Playerbot::OrderedMutex<Playerbot::LockOrder::LOOT_MANAGER> _sessionMutex;

        // Statistics
        std::atomic<uint64> _sessionsCreated{0};
        std::atomic<uint64> _sessionsCompleted{0};
    };

    /**
     * @brief Distribution module - roll handling and execution
     */
    class DistributionModule
    {
    public:
        void DistributeLoot(Group* group, LootItem const& item);
        void HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType);
        LootRollType DetermineLootDecision(Player* player, LootItem const& item,
                                            LootDecisionStrategy strategy);
        LootPriority CalculateLootPriority(Player* player, LootItem const& item);
        bool ShouldRollNeed(Player* player, LootItem const& item);
        bool ShouldRollGreed(Player* player, LootItem const& item);
        bool IsItemForClass(Player* player, LootItem const& item);
        bool IsItemForMainSpec(Player* player, LootItem const& item);
        bool IsItemForOffSpec(Player* player, LootItem const& item);
        void ExecuteLootDistribution(Group* group, uint32 rollId);
        void ResolveRollTies(Group* group, uint32 rollId);
        void HandleLootNinja(Group* group, uint32 suspectedPlayer);

    private:
        struct LootRoll
        {
            uint32 rollId;
            uint32 itemId;
            uint32 lootSlot;
            uint32 groupId;
            std::unordered_map<uint32, LootRollType> playerRolls;
            std::unordered_map<uint32, uint32> rollValues;
            bool isComplete;

            LootRoll(uint32 id) : rollId(id), itemId(0), lootSlot(0), groupId(0), isComplete(false) {}
        };

        std::unordered_map<uint32, LootRoll> _activeRolls;
        uint32 _nextRollId{1};
        Playerbot::OrderedMutex<Playerbot::LockOrder::LOOT_MANAGER> _rollMutex;

        // Statistics
        std::atomic<uint64> _rollsProcessed{0};
        std::atomic<uint64> _itemsDistributed{0};
    };

    // Module instances
    std::unique_ptr<AnalysisModule> _analysis;
    std::unique_ptr<CoordinationModule> _coordination;
    std::unique_ptr<DistributionModule> _distribution;

    // Global mutex for unified operations
    mutable Playerbot::OrderedMutex<Playerbot::LockOrder::LOOT_MANAGER> _mutex;

    // Statistics
    std::atomic<uint64> _totalOperations{0};
    std::atomic<uint64> _totalProcessingTimeMs{0};
};

} // namespace Playerbot
