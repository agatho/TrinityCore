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
#include "Core/DI/Interfaces/IMountManager.h"
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Mount types based on WoW 11.2 mount system
 */
enum class MountType : uint8
{
    GROUND = 0,
    FLYING = 1,
    AQUATIC = 2,
    DRAGONRIDING = 3,    // WoW 10.0+ Dragonriding mounts
    SPECIAL = 4           // Special mounts (passenger, multi-seat)
};

/**
 * @brief Mount speed categories
 */
enum class MountSpeed : uint8
{
    SLOW = 60,           // 60% ground speed
    NORMAL = 100,        // 100% ground speed
    FAST = 150,          // 150% flying speed (60% in no-fly zones)
    EPIC = 280,          // 280% flying speed (100% in no-fly zones)
    EPIC_PLUS = 310      // 310% flying speed (100% in no-fly zones)
};

/**
 * @brief Mount information
 */
struct MountInfo
{
    uint32 spellId;          // Spell ID to cast for mount
    uint32 displayId;        // Creature display ID
    std::string name;
    MountType type;
    MountSpeed speed;
    uint32 requiredLevel;
    uint32 requiredSkill;    // Riding skill required (75, 150, 225, 300)
    bool isFlyingMount;
    bool isAquaticMount;
    bool isDragonridingMount;
    bool isMultiPassenger;
    uint32 passengerCount;
    std::vector<uint32> zoneRestrictions; // Zones where mount is restricted

    MountInfo() : spellId(0), displayId(0), type(MountType::GROUND),
        speed(MountSpeed::NORMAL), requiredLevel(20), requiredSkill(75),
        isFlyingMount(false), isAquaticMount(false), isDragonridingMount(false),
        isMultiPassenger(false), passengerCount(1) {}
};

/**
 * @brief Mount automation configuration per bot
 */
struct MountAutomationProfile
{
    bool autoMount = true;                   // Auto-mount when traveling
    bool preferFlyingMount = true;           // Prefer flying mount if available
    bool useDragonriding = true;             // Use dragonriding mounts if available
    float minDistanceForMount = 50.0f;       // Min distance to mount (yards)
    bool dismountInCombat = true;            // Auto-dismount when entering combat
    bool remountAfterCombat = true;          // Remount after combat ends
    uint32 mountCastDelay = 1500;            // Delay before mounting (ms)
    bool useGroundMountIndoors = true;       // Use ground mount in no-fly zones
    bool sharePassengerMounts = false;       // Allow group members to ride as passengers
    std::unordered_set<uint32> preferredMounts; // Preferred mount spell IDs

    MountAutomationProfile() = default;
};

/**
 * @brief Mount Manager - Complete mount automation for bots
 *
 * Features:
 * - Auto-mount for long-distance travel
 * - Flying mount support with zone detection
 * - Dragonriding support (WoW 10.0+)
 * - Aquatic mount support
 * - Multi-passenger mounts
 * - Mount collection tracking
 * - Riding skill management
 * - Zone-based mount selection
 * - Performance optimized
 */
class TC_GAME_API MountManager final : public IMountManager
{
public:
    static MountManager* instance();

    // ============================================================================
    // CORE MOUNT MANAGEMENT
    // ============================================================================

    /**
     * Initialize mount system on server startup
     */
    void Initialize() override;

    /**
     * Update mount automation for player (called periodically)
     */
    void Update(::Player* player, uint32 diff) override;

    /**
     * Mount player with best available mount
     */
    bool MountPlayer(::Player* player) override;

    /**
     * Dismount player
     */
    bool DismountPlayer(::Player* player) override;

    /**
     * Check if player is mounted
     */
    bool IsMounted(::Player* player) const override;

    /**
     * Check if player should auto-mount (distance check)
     */
    bool ShouldAutoMount(::Player* player, Position const& destination) const override;

    // ============================================================================
    // MOUNT SELECTION
    // ============================================================================

    /**
     * Get best mount for current zone and player state
     */
    MountInfo const* GetBestMount(::Player* player) const override;

    /**
     * Get flying mount if zone allows flying
     */
    MountInfo const* GetFlyingMount(::Player* player) const override;

    /**
     * Get ground mount
     */
    MountInfo const* GetGroundMount(::Player* player) const override;

    /**
     * Get aquatic mount for underwater travel
     */
    MountInfo const* GetAquaticMount(::Player* player) const override;

    /**
     * Get dragonriding mount
     */
    MountInfo const* GetDragonridingMount(::Player* player) const override;

    /**
     * Check if player can use flying mount in current zone
     */
    bool CanUseFlyingMount(::Player* player) const override;

    /**
     * Check if player is underwater
     */
    bool IsPlayerUnderwater(::Player* player) const override;

    /**
     * Check if zone allows dragonriding
     */
    bool CanUseDragonriding(::Player* player) const override;

    // ============================================================================
    // MOUNT COLLECTION
    // ============================================================================

    /**
     * Get all mounts player knows
     */
    std::vector<MountInfo> GetPlayerMounts(::Player* player) const override;

    /**
     * Check if player knows mount
     */
    bool KnowsMount(::Player* player, uint32 spellId) const override;

    /**
     * Learn mount spell
     */
    bool LearnMount(::Player* player, uint32 spellId) override;

    /**
     * Get mount count for player
     */
    uint32 GetMountCount(::Player* player) const override;

    /**
     * Check if mount is usable by player (level, skill, class restrictions)
     */
    bool CanUseMount(::Player* player, MountInfo const& mount) const override;

    // ============================================================================
    // RIDING SKILL
    // ============================================================================

    /**
     * Get player riding skill level
     */
    uint32 GetRidingSkill(::Player* player) const override;

    /**
     * Check if player has riding skill
     */
    bool HasRidingSkill(::Player* player) const override;

    /**
     * Learn riding skill (apprentice, journeyman, expert, artisan, master)
     */
    bool LearnRidingSkill(::Player* player, uint32 skillLevel) override;

    /**
     * Get max mount speed based on riding skill
     */
    MountSpeed GetMaxMountSpeed(::Player* player) const override;

    // ============================================================================
    // MULTI-PASSENGER MOUNTS
    // ============================================================================

    /**
     * Check if mount is multi-passenger
     */
    bool IsMultiPassengerMount(MountInfo const& mount) const override;

    /**
     * Get available passenger seats
     */
    uint32 GetAvailablePassengerSeats(::Player* player) const override;

    /**
     * Add passenger to mount
     */
    bool AddPassenger(::Player* mountedPlayer, ::Player* passenger) override;

    /**
     * Remove passenger from mount
     */
    bool RemovePassenger(::Player* passenger) override;

    // ============================================================================
    // AUTOMATION PROFILES
    // ============================================================================

    void SetAutomationProfile(uint32 playerGuid, MountAutomationProfile const& profile) override;
    MountAutomationProfile GetAutomationProfile(uint32 playerGuid) const override;

    // ============================================================================
    // METRICS
    // ============================================================================

    struct MountMetrics
    {
        std::atomic<uint32> mountsLearned{0};
        std::atomic<uint32> timesMounted{0};
        std::atomic<uint32> timesDismounted{0};
        std::atomic<uint32> flyingMountUsage{0};
        std::atomic<uint32> dragonridingUsage{0};
        std::atomic<uint64> totalMountedTime{0};  // Milliseconds

        void Reset()
        {
            mountsLearned = 0;
            timesMounted = 0;
            timesDismounted = 0;
            flyingMountUsage = 0;
            dragonridingUsage = 0;
            totalMountedTime = 0;
        }
    };

    MountMetrics const& GetPlayerMetrics(uint32 playerGuid) const;
    MountMetrics const& GetGlobalMetrics() const;

private:
    MountManager();
    ~MountManager() = default;

    // ============================================================================
    // INITIALIZATION HELPERS
    // ============================================================================

    void LoadMountDatabase() override;
    void InitializeVanillaMounts() override;
    void InitializeTBCMounts() override;
    void InitializeWrathMounts() override;
    void InitializeCataclysmMounts() override;
    void InitializePandariaMounts() override;
    void InitializeDraenorMounts() override;
    void InitializeLegionMounts() override;
    void InitializeBfAMounts() override;
    void InitializeShadowlandsMounts() override;
    void InitializeDragonflightMounts() override;
    void InitializeWarWithinMounts();

    // ============================================================================
    // MOUNT CASTING HELPERS
    // ============================================================================

    bool CastMountSpell(::Player* player, uint32 spellId);
    bool CanCastMountSpell(::Player* player, uint32 spellId) const;
    void HandleMountCastResult(::Player* player, uint32 spellId, bool success);

    // ============================================================================
    // ZONE DETECTION HELPERS
    // ============================================================================

    bool IsInNoFlyZone(::Player* player) const;
    bool IsInDragonridingZone(::Player* player) const;
    bool IsInAquaticZone(::Player* player) const;
    uint32 GetCurrentZoneId(::Player* player) const;

    // ============================================================================
    // VALIDATION HELPERS
    // ============================================================================

    bool ValidateMountUsage(::Player* player) const;
    bool IsInCombat(::Player* player) const;
    bool IsIndoors(::Player* player) const;
    bool IsInInstance(::Player* player) const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Mount database (spellId -> MountInfo)
    std::unordered_map<uint32, MountInfo> _mountDatabase;

    // Player automation profiles
    std::unordered_map<uint32, MountAutomationProfile> _playerProfiles;

    // Player mount tracking (playerGuid -> known mount spell IDs)
    std::unordered_map<uint32, std::unordered_set<uint32>> _playerMounts;

    // Mount state tracking (playerGuid -> current mount spell ID)
    std::unordered_map<uint32, uint32> _activeMounts;

    // Mount timestamps (playerGuid -> mount timestamp)
    std::unordered_map<uint32, uint32> _mountTimestamps;

    // Metrics
    std::unordered_map<uint32, MountMetrics> _playerMetrics;
    MountMetrics _globalMetrics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _mutex;

    // Update intervals
    static constexpr uint32 MOUNT_UPDATE_INTERVAL = 5000;  // 5 seconds
    std::unordered_map<uint32, uint32> _lastUpdateTimes;

    // Mount spell IDs (examples - full list loaded from DB/DBC)
    static constexpr uint32 SPELL_MOUNT_RIDING_APPRENTICE = 33388;
    static constexpr uint32 SPELL_MOUNT_RIDING_JOURNEYMAN = 33391;
    static constexpr uint32 SPELL_MOUNT_RIDING_EXPERT = 34090;
    static constexpr uint32 SPELL_MOUNT_RIDING_ARTISAN = 34091;
    static constexpr uint32 SPELL_MOUNT_RIDING_MASTER = 90265;
};

} // namespace Playerbot
