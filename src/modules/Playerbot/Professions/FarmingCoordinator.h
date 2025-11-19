/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * FARMING COORDINATOR SYSTEM FOR PLAYERBOT
 *
 * This system coordinates profession leveling with character progression:
 * - Monitors profession skill gap vs character level
 * - Triggers farming sessions when skills fall behind
 * - Selects optimal zones/nodes for current skill level
 * - Balances gathering vs crafting for efficient leveling
 * - Manages material stockpiles for auction house
 *
 * Skill Sync Algorithm:
 * - Target: Profession skill ≈ Character Level × 5
 * - Example: Level 20 character should have ~100 skill
 * - Trigger: If skill < (char_level × 5 - 25), start farming
 * - Duration: Farm until skill ≥ (char_level × 5)
 *
 * Integration:
 * - Works with ProfessionManager for skill tracking
 * - Uses GatheringAutomation for node harvesting
 * - Coordinates with auction house for material management
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "ProfessionManager.h"
#include "../Core/DI/Interfaces/IFarmingCoordinator.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Farming session types
 */
enum class FarmingSessionType : uint8
{
    NONE = 0,
    SKILL_CATCHUP,          // Farm to catch up profession skill to character level
    MATERIAL_STOCKPILE,     // Farm materials for auction house
    CRAFTING_MATERIALS,     // Farm materials for crafting leveling
    QUEST_SUPPORT           // Farm materials for quest completion
};

/**
 * @brief Zone information for farming
 */
struct FarmingZoneInfo
{
    uint32 zoneId;
    uint32 areaId;
    std::string zoneName;
    uint16 minSkillLevel;           // Minimum profession skill
    uint16 maxSkillLevel;           // Maximum useful skill level
    ProfessionType profession;
    std::vector<uint32> nodeEntries; // GameObject entries for nodes in zone
    Position centerPosition;
    float zoneRadius;
    uint8 recommendedCharLevel;      // Character level for this zone
    bool isContested;                // PvP zone

    FarmingZoneInfo()
        : zoneId(0), areaId(0), minSkillLevel(0), maxSkillLevel(0),
          profession(ProfessionType::NONE), zoneRadius(0.0f),
          recommendedCharLevel(0), isContested(false) {}
};

/**
 * @brief Active farming session
 */
struct FarmingSession
{
    uint32 sessionId;
    uint32 playerGuid;
    FarmingSessionType sessionType;
    ProfessionType profession;
    FarmingZoneInfo zone;
    uint32 startTime;
    uint32 duration;                    // Expected duration (ms)
    uint16 startingSkill;
    uint16 targetSkill;
    uint32 nodesGathered;
    uint32 materialsCollected;
    bool isActive;
    Position originalPosition;          // Return position after session

    FarmingSession()
        : sessionId(0), playerGuid(0), sessionType(FarmingSessionType::NONE),
          profession(ProfessionType::NONE), startTime(0), duration(0),
          startingSkill(0), targetSkill(0), nodesGathered(0),
          materialsCollected(0), isActive(false) {}
};

/**
 * @brief Farming coordination profile per bot
 */
struct FarmingCoordinatorProfile
{
    bool autoFarm = true;                   // Enable automatic farming
    uint16 skillGapThreshold = 25;          // Skill gap before triggering farm (default: 25)
    float skillLevelMultiplier = 5.0f;      // Target = char_level × multiplier (default: 5.0)
    uint32 maxFarmingDuration = 1800000;    // Max farming session (30 minutes)
    uint32 minFarmingDuration = 300000;     // Min farming session (5 minutes)
    bool returnToOriginalPosition = true;   // Return after farming
    bool prioritizePrimaryProfessions = true; // Farm primary before secondary
    uint32 materialStockpileTarget = 100;   // Target stack size for AH materials

    // Session scheduling
    uint32 farmingCooldown = 600000;        // 10 minutes between sessions
    bool farmDuringDowntime = true;         // Farm when not questing/grouping
    bool farmWhileTraveling = true;         // Opportunistic gathering

    FarmingCoordinatorProfile() = default;
};

/**
 * @brief Farming statistics per bot
 */
struct FarmingStatistics
{
    std::atomic<uint32> sessionsCompleted{0};
    std::atomic<uint32> totalTimeSpent{0};          // Milliseconds
    std::atomic<uint32> totalNodesGathered{0};
    std::atomic<uint32> totalSkillPointsGained{0};
    std::atomic<uint32> totalMaterialsCollected{0};
    std::atomic<uint32> zonesVisited{0};

    void Reset()
    {
        sessionsCompleted = 0;
        totalTimeSpent = 0;
        totalNodesGathered = 0;
        totalSkillPointsGained = 0;
        totalMaterialsCollected = 0;
        zonesVisited = 0;
    }
};

/**
 * @brief Complete farming coordination system for profession leveling
 */
class TC_GAME_API FarmingCoordinator final : public IFarmingCoordinator
{
public:
    static FarmingCoordinator* instance();

    // ============================================================================
    // CORE FARMING COORDINATION
    // ============================================================================

    /**
     * Initialize farming coordinator on server startup
     */
    void Initialize() override;

    /**
     * Update farming coordination for player (called periodically)
     */
    void Update(::Player* player, uint32 diff) override;

    /**
     * Enable/disable farming coordination for player
     */
    void SetEnabled(::Player* player, bool enabled) override;
    bool IsEnabled(::Player* player) const override;

    /**
     * Get coordination profile for player
     */
    void SetCoordinatorProfile(uint32 playerGuid, FarmingCoordinatorProfile const& profile) override;
    FarmingCoordinatorProfile GetCoordinatorProfile(uint32 playerGuid) const override;

    // ============================================================================
    // SKILL ANALYSIS
    // ============================================================================

    /**
     * Check if profession skill needs catch-up farming
     * Returns true if skill gap exceeds threshold
     */
    bool NeedsFarming(::Player* player, ProfessionType profession) const override;

    /**
     * Calculate skill gap for profession
     * Returns: (target skill) - (current skill)
     * Positive = behind, Negative = ahead
     */
    int32 GetSkillGap(::Player* player, ProfessionType profession) const override;

    /**
     * Get target skill level for character level
     * Formula: character_level × skillLevelMultiplier
     */
    uint16 GetTargetSkillLevel(::Player* player, ProfessionType profession) const override;

    /**
     * Get professions that need farming (sorted by priority)
     */
    std::vector<ProfessionType> GetProfessionsNeedingFarm(::Player* player) const override;

    /**
     * Calculate recommended farming duration based on skill gap
     */
    uint32 CalculateFarmingDuration(::Player* player, ProfessionType profession) const override;

    // ============================================================================
    // FARMING SESSION MANAGEMENT
    // ============================================================================

    /**
     * Start farming session for profession
     */
    bool StartFarmingSession(::Player* player, ProfessionType profession, FarmingSessionType sessionType = FarmingSessionType::SKILL_CATCHUP) override;

    /**
     * Stop active farming session
     */
    void StopFarmingSession(::Player* player) override;

    /**
     * Get active farming session for player
     */
    FarmingSession const* GetActiveFarmingSession(uint32 playerGuid) const override;

    /**
     * Check if player has active farming session
     */
    bool HasActiveFarmingSession(::Player* player) const override;

    /**
     * Update farming session progress
     */
    void UpdateFarmingSession(::Player* player, uint32 diff) override;

    /**
     * Check if farming session should end
     */
    bool ShouldEndFarmingSession(::Player* player, FarmingSession const& session) const override;

    // ============================================================================
    // ZONE SELECTION
    // ============================================================================

    /**
     * Get optimal farming zone for profession and skill level
     */
    FarmingZoneInfo const* GetOptimalFarmingZone(::Player* player, ProfessionType profession) const override;

    /**
     * Get all suitable zones for skill level
     */
    std::vector<FarmingZoneInfo> GetSuitableZones(::Player* player, ProfessionType profession) const override;

    /**
     * Calculate zone score based on:
     * - Distance from current position
     * - Node density
     * - Skill-up potential
     * - Safety (PvP risk)
     */
    float CalculateZoneScore(::Player* player, FarmingZoneInfo const& zone) const override;

    // ============================================================================
    // MATERIAL MANAGEMENT
    // ============================================================================

    /**
     * Check if material stockpile target reached
     */
    bool HasReachedStockpileTarget(::Player* player, uint32 itemId) const override;

    /**
     * Get current material count in inventory
     */
    uint32 GetMaterialCount(::Player* player, uint32 itemId) const override;

    /**
     * Get materials needed for auction house target
     */
    std::vector<std::pair<uint32, uint32>> GetNeededMaterials(::Player* player, ProfessionType profession) const override;

    // ============================================================================
    // STATISTICS
    // ============================================================================

    FarmingStatistics const& GetPlayerStatistics(uint32 playerGuid) const override;
    FarmingStatistics const& GetGlobalStatistics() const override;

    /**
     * Reset statistics for player
     */
    void ResetStatistics(uint32 playerGuid) override;

private:
    FarmingCoordinator();
    ~FarmingCoordinator() = default;

    // ============================================================================
    // INITIALIZATION HELPERS
    // ============================================================================

    void LoadFarmingZones();
    void InitializeZoneDatabase();
    void InitializeMiningZones();
    void InitializeHerbalismZones();
    void InitializeSkinningZones();

    // ============================================================================
    // FARMING HELPERS
    // ============================================================================

    /**
     * Generate unique session ID
     */
    uint32 GenerateSessionId();

    /**
     * Teleport/navigate bot to farming zone
     */
    bool TravelToFarmingZone(::Player* player, FarmingZoneInfo const& zone);

    /**
     * Return bot to original position after farming
     */
    void ReturnToOriginalPosition(::Player* player, FarmingSession const& session);

    /**
     * Check if farming conditions are met (not in combat, not in group, etc.)
     */
    bool CanStartFarming(::Player* player) const;

    /**
     * Validate farming session is still valid
     */
    bool ValidateFarmingSession(::Player* player, FarmingSession const& session) const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Active farming sessions (playerGuid -> session)
    std::unordered_map<uint32, FarmingSession> _activeSessions;

    // Farming zone database (profession -> zones sorted by skill level)
    std::unordered_map<ProfessionType, std::vector<FarmingZoneInfo>> _farmingZones;

    // Coordination profiles (playerGuid -> profile)
    std::unordered_map<uint32, FarmingCoordinatorProfile> _profiles;

    // Last farming time (playerGuid -> timestamp) for cooldown tracking
    std::unordered_map<uint32, uint32> _lastFarmingTimes;

    // Statistics
    std::unordered_map<uint32, FarmingStatistics> _playerStatistics;
    FarmingStatistics _globalStatistics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::PROFESSION_MANAGER> _mutex;

    // Session ID generator
    std::atomic<uint32> _nextSessionId{1};

    // Update intervals
    static constexpr uint32 FARMING_CHECK_INTERVAL = 10000;     // 10 seconds
    static constexpr uint32 SESSION_UPDATE_INTERVAL = 5000;     // 5 seconds
};

} // namespace Playerbot
