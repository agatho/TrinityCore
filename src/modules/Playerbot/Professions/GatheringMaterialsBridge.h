/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * GATHERING MATERIALS BRIDGE FOR PLAYERBOT
 *
 * This system bridges gathering automation with profession crafting needs:
 * - Analyzes crafting queue to determine material requirements
 * - Prioritizes gathering nodes based on what materials are needed
 * - Triggers gathering sessions for specific materials
 * - Tracks gathered materials vs crafting needs
 * - Coordinates with GatheringManager for node selection
 *
 * Integration Points:
 * - Uses ProfessionManager to determine crafting needs
 * - Uses GatheringManager for resource harvesting
 * - Coordinates with ProfessionAuctionBridge for material sourcing decisions
 *
 * Design Pattern: Bridge Pattern
 * - Decouples gathering logic from crafting logic
 * - All gathering operations delegated to GatheringManager
 * - This class only manages gathering-crafting coordination
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "ProfessionManager.h"
#include "../Professions/GatheringManager.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Material requirement priority
 */
enum class MaterialPriority : uint8
{
    NONE = 0,
    LOW = 1,           // Nice to have
    MEDIUM = 2,        // Needed for leveling
    HIGH = 3,          // Blocking crafting queue
    CRITICAL = 4       // Needed immediately for active craft
};

/**
 * @brief Material requirement entry
 */
struct MaterialRequirement
{
    uint32 itemId;
    uint32 quantityNeeded;
    uint32 quantityHave;
    uint32 quantityGathering;      // Currently being gathered
    MaterialPriority priority;
    ProfessionType forProfession;
    uint32 forRecipeId;
    uint32 addedTime;

    MaterialRequirement()
        : itemId(0), quantityNeeded(0), quantityHave(0), quantityGathering(0)
        , priority(MaterialPriority::NONE), forProfession(ProfessionType::NONE)
        , forRecipeId(0), addedTime(GameTime::GetGameTimeMS()) {}

    uint32 GetRemainingNeeded() const { return quantityNeeded > quantityHave ? quantityNeeded - quantityHave : 0; }
    bool IsFulfilled() const { return quantityHave >= quantityNeeded; }
    float GetFulfillmentPercent() const { return quantityNeeded > 0 ? (float)quantityHave / quantityNeeded : 1.0f; }
};

/**
 * @brief Gathering session for specific material
 */
struct GatheringMaterialSession
{
    uint32 sessionId;
    uint32 playerGuid;
    uint32 targetItemId;
    uint32 targetQuantity;
    uint32 gatheredSoFar;
    GatheringNodeType nodeType;
    uint32 startTime;
    uint32 endTime;              // 0 if still active
    bool isActive;
    Position startPosition;      // Where gathering started
    std::vector<Position> nodesVisited;

    GatheringMaterialSession()
        : sessionId(0), playerGuid(0), targetItemId(0), targetQuantity(0)
        , gatheredSoFar(0), nodeType(GatheringNodeType::NONE)
        , startTime(GameTime::GetGameTimeMS()), endTime(0), isActive(true) {}

    float GetProgress() const { return targetQuantity > 0 ? (float)gatheredSoFar / targetQuantity : 0.0f; }
    uint32 GetDuration() const { return isActive ? (GameTime::GetGameTimeMS() - startTime) : (endTime - startTime); }
};

/**
 * @brief Statistics for gathering-crafting coordination
 */
struct GatheringMaterialsStatistics
{
    std::atomic<uint32> materialsGatheredForCrafting{0};
    std::atomic<uint32> gatheringSessionsStarted{0};
    std::atomic<uint32> gatheringSessionsCompleted{0};
    std::atomic<uint32> recipesUnblockedByGathering{0};
    std::atomic<uint32> timeSpentGathering{0};  // Milliseconds
    std::atomic<float> averageGatheringEfficiency{1.0f};

    void Reset()
    {
        materialsGatheredForCrafting = 0;
        gatheringSessionsStarted = 0;
        gatheringSessionsCompleted = 0;
        recipesUnblockedByGathering = 0;
        timeSpentGathering = 0;
        averageGatheringEfficiency = 1.0f;
    }

    float GetCompletionRate() const
    {
        uint32 started = gatheringSessionsStarted.load();
        uint32 completed = gatheringSessionsCompleted.load();
        return started > 0 ? (float)completed / started : 0.0f;
    }
};

/**
 * @brief Bridge between gathering and profession crafting
 *
 * DESIGN PRINCIPLE: This class does NOT implement gathering operations.
 * All gathering operations are delegated to GatheringManager.
 * This class only coordinates gathering-crafting logic.
 */
class TC_GAME_API GatheringMaterialsBridge final
{
public:
    static GatheringMaterialsBridge* instance();

    // ========================================================================
    // CORE BRIDGE MANAGEMENT
    // ========================================================================

    /**
     * Initialize gathering-crafting bridge on server startup
     */
    void Initialize();

    /**
     * Update gathering-crafting coordination for player (called periodically)
     */
    void Update(::Player* player, uint32 diff);

    /**
     * Enable/disable gathering-crafting automation for player
     */
    void SetEnabled(::Player* player, bool enabled);
    bool IsEnabled(::Player* player) const;

    // ========================================================================
    // MATERIAL REQUIREMENT ANALYSIS
    // ========================================================================

    /**
     * Get materials needed for bot's current crafting queue
     * Queries ProfessionManager for queued recipes and missing materials
     */
    std::vector<MaterialRequirement> GetNeededMaterials(::Player* player);

    /**
     * Check if gathered item is useful for bot's professions
     * Returns true if item is needed for any known recipe
     */
    bool IsItemNeededForCrafting(::Player* player, uint32 itemId);

    /**
     * Get priority level for specific material
     * Based on crafting queue urgency and stockpile levels
     */
    MaterialPriority GetMaterialPriority(::Player* player, uint32 itemId);

    /**
     * Update material requirements based on crafting queue changes
     */
    void UpdateMaterialRequirements(::Player* player);

    // ========================================================================
    // GATHERING AUTOMATION
    // ========================================================================

    /**
     * Prioritize gathering nodes based on material needs
     * Sorts nodes by: priority, distance, skill-up potential
     */
    std::vector<GatheringNode> PrioritizeNodesByNeeds(
        ::Player* player,
        std::vector<GatheringNode> const& nodes);

    /**
     * Trigger gathering session for specific material
     * Returns true if gathering session started successfully
     */
    bool StartGatheringForMaterial(::Player* player, uint32 itemId, uint32 quantity);

    /**
     * Stop current gathering session
     */
    void StopGatheringSession(uint32 sessionId);

    /**
     * Get active gathering session for player
     */
    GatheringMaterialSession const* GetActiveSession(uint32 playerGuid) const;

    /**
     * Handle material gathered event
     * Called when GatheringManager completes a gathering action
     */
    void OnMaterialGathered(::Player* player, uint32 itemId, uint32 quantity);

    // ========================================================================
    // GATHERING NODE SCORING
    // ========================================================================

    /**
     * Calculate score for gathering node based on material needs
     * Higher score = higher priority
     */
    float CalculateNodeScore(::Player* player, GatheringNode const& node);

    /**
     * Check if node provides materials we need
     */
    bool DoeNodeProvideNeededMaterial(::Player* player, GatheringNode const& node);

    /**
     * Get estimated material yield from node
     */
    uint32 GetEstimatedYield(GatheringNode const& node);

    // ========================================================================
    // INTEGRATION WITH GATHERING MANAGER
    // ========================================================================

    /**
     * Configure GatheringManager to prioritize materials over skill-ups
     */
    void ConfigureGatheringForMaterials(::Player* player, bool prioritizeMaterials);

    /**
     * Get GatheringManager instance
     */
    GatheringManager* GetGatheringManager(::Player* player);

    /**
     * Synchronize with gathering manager state
     */
    void SynchronizeWithGatheringManager(::Player* player);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    GatheringMaterialsStatistics const& GetPlayerStatistics(uint32 playerGuid) const;
    GatheringMaterialsStatistics const& GetGlobalStatistics() const;
    void ResetStatistics(uint32 playerGuid);

private:
    GatheringMaterialsBridge();
    ~GatheringMaterialsBridge() = default;

    // ========================================================================
    // INITIALIZATION HELPERS
    // ========================================================================

    void LoadNodeMaterialMappings();
    void InitializeGatheringProfessionMaterials();

    // ========================================================================
    // MATERIAL ANALYSIS HELPERS
    // ========================================================================

    /**
     * Map item ID to node type that provides it
     */
    GatheringNodeType GetNodeTypeForMaterial(uint32 itemId) const;

    /**
     * Get all recipes that use this material
     */
    std::vector<RecipeInfo> GetRecipesThatUseMaterial(::Player* player, uint32 itemId);

    /**
     * Check if player knows any recipes that use this material
     */
    bool PlayerKnowsRecipesUsingMaterial(::Player* player, uint32 itemId);

    /**
     * Calculate opportunity cost of gathering vs buying
     */
    float CalculateGatheringOpportunityCost(::Player* player, uint32 itemId, uint32 quantity);

    // ========================================================================
    // GATHERING SESSION MANAGEMENT
    // ========================================================================

    uint32 CreateGatheringSession(::Player* player, uint32 itemId, uint32 quantity);
    void UpdateGatheringSession(uint32 sessionId);
    void CompleteGatheringSession(uint32 sessionId, bool success);

    // ========================================================================
    // DATA STRUCTURES
    // ========================================================================

    // Enabled state (playerGuid -> enabled)
    std::unordered_map<uint32, bool> _enabledState;

    // Material requirements (playerGuid -> materials)
    std::unordered_map<uint32, std::vector<MaterialRequirement>> _materialRequirements;

    // Active gathering sessions (sessionId -> session)
    std::unordered_map<uint32, GatheringMaterialSession> _activeSessions;

    // Player to session mapping (playerGuid -> sessionId)
    std::unordered_map<uint32, uint32> _playerActiveSessions;

    // Node type to material mapping (itemId -> node type)
    std::unordered_map<uint32, GatheringNodeType> _materialToNodeType;

    // Statistics
    std::unordered_map<uint32, GatheringMaterialsStatistics> _playerStatistics;
    GatheringMaterialsStatistics _globalStatistics;

    std::atomic<uint32> _nextSessionId{1};

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> _mutex;

    // Update intervals
    static constexpr uint32 REQUIREMENT_UPDATE_INTERVAL = 30000;   // 30 seconds
    static constexpr uint32 SESSION_CHECK_INTERVAL = 5000;         // 5 seconds
    static constexpr uint32 MAX_GATHERING_SESSION_DURATION = 1800000; // 30 minutes
};

} // namespace Playerbot
