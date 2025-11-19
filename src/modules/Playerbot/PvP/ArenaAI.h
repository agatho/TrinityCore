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
#include "Player.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{
    // Interface
    #include "Core/DI/Interfaces/IArenaAI.h"


/**
 * @brief Arena bracket types
 */
enum class ArenaBracket : uint8
{
    BRACKET_2v2 = 0,
    BRACKET_3v3 = 1,
    BRACKET_5v5 = 2
};

/**
 * @brief Arena strategy types
 */
enum class ArenaStrategy : uint8
{
    KILL_HEALER_FIRST = 0,     // Focus healer
    KILL_DPS_FIRST = 1,        // Focus DPS
    KILL_LOWEST_HEALTH = 2,    // Focus lowest health target
    SPREAD_PRESSURE = 3,       // Spread damage across targets
    TRAIN_ONE_TARGET = 4,      // Focus single target until death
    ADAPTIVE = 5               // Adapt based on situation
};

/**
 * @brief Arena positioning strategy
 */
enum class PositioningStrategy : uint8
{
    AGGRESSIVE = 0,            // Push forward
    DEFENSIVE = 1,             // Stay back, kite
    PILLAR_KITE = 2,           // Use pillars for LoS
    SPREAD_OUT = 3,            // Spread to avoid AoE
    GROUP_UP = 4               // Stay together
};

/**
 * @brief Arena team composition types
 */
enum class TeamComposition : uint8
{
    DOUBLE_DPS = 0,            // 2v2: Two DPS
    DPS_HEALER = 1,            // 2v2: DPS + Healer
    TRIPLE_DPS = 2,            // 3v3: Three DPS (Training comp)
    DOUBLE_DPS_HEALER = 3,     // 3v3: Two DPS + Healer (Standard)
    TANK_DPS_HEALER = 4        // 3v3: Tank + DPS + Healer
};

/**
 * @brief Arena pillar/LoS object
 */
struct ArenaPillar
{
    Position position;
    float radius;
    bool isAvailable;

    ArenaPillar() : radius(5.0f), isAvailable(true) {}
    ArenaPillar(Position pos, float r) : position(pos), radius(r), isAvailable(true) {}
};

/**
 * @brief Arena match state
 */
struct ArenaMatchState
{
    uint32 matchStartTime;
    uint32 damageDealt;
    uint32 damageTaken;
    uint32 healingDone;
    uint32 ccLanded;
    uint32 defensivesUsed;
    uint32 offensivesUsed;
    bool isWinning;
    uint32 teammateAliveCount;
    uint32 enemyAliveCount;

    ArenaMatchState() : matchStartTime(0), damageDealt(0), damageTaken(0),
        healingDone(0), ccLanded(0), defensivesUsed(0), offensivesUsed(0),
        isWinning(false), teammateAliveCount(0), enemyAliveCount(0) {}
};

/**
 * @brief Arena profile configuration
 */
struct ArenaProfile
{
    ArenaStrategy strategy = ArenaStrategy::ADAPTIVE;
    PositioningStrategy positioning = PositioningStrategy::PILLAR_KITE;
    bool usePillars = true;
    bool autoSwitch = true;           // Auto-switch targets
    bool prioritizeHealers = true;
    bool coordCC = true;              // Coordinate CC with teammates
    bool saveDefensivesForBurst = true;
    uint32 pillarKiteHealthThreshold = 40; // Start pillar kiting below %
    float maxDistanceFromTeam = 25.0f;

    ArenaProfile() = default;
};

/**
 * @brief Arena AI - Complete arena automation
 *
 * **Phase 7.1: Per-Bot Instance Pattern (27th Manager)**
 *
 * Features:
 * - 2v2/3v3/5v5 bracket strategies
 * - Team composition analysis
 * - Pillar kiting and LoS mechanics
 * - Focus target coordination
 * - Positioning algorithms
 * - Composition-specific counters
 * - Adaptive strategy based on match state
 * - Performance optimized (per-bot isolation, zero mutex)
 *
 * **Ownership:**
 * - Owned by GameSystemsManager (27th manager)
 * - Each bot has independent arena state and strategy
 * - Shared arena map data across all bots (static)
 */
class TC_GAME_API ArenaAI final : public IArenaAI
{
public:
    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    void Initialize() override;
    void Update(uint32 diff) override;

    /**
     * Called when arena match starts
     */
    void OnMatchStart() override;

    /**
     * Called when arena match ends
     */
    void OnMatchEnd(bool won) override;

    // ============================================================================
    // STRATEGY SELECTION
    // ============================================================================

    /**
     * Analyze team composition and select strategy
     */
    void AnalyzeTeamComposition() override;

    /**
     * Get recommended strategy for composition
     */
    ArenaStrategy GetStrategyForComposition(TeamComposition teamComp,
        TeamComposition enemyComp) const override;

    /**
     * Adapt strategy based on match state
     */
    void AdaptStrategy() override;

    // ============================================================================
    // TARGET SELECTION
    // ============================================================================

    /**
     * Select focus target for arena
     */
    ::Unit* SelectFocusTarget() const override;

    /**
     * Check if should switch target
     */
    bool ShouldSwitchTarget(::Unit* currentTarget) const override;

    /**
     * Get kill target priority
     */
    std::vector<::Unit*> GetKillTargetPriority() const override;

    // ============================================================================
    // POSITIONING
    // ============================================================================

    /**
     * Execute positioning strategy
     */
    void ExecutePositioning() override;

    /**
     * Find best pillar for kiting
     */
    ArenaPillar const* FindBestPillar() const override;

    /**
     * Move to pillar for LoS
     */
    bool MoveToPillar(ArenaPillar const& pillar) override;

    /**
     * Check if using pillar effectively
     */
    bool IsUsingPillarEffectively() const override;

    /**
     * Maintain optimal distance from enemies
     */
    bool MaintainOptimalDistance() override;

    /**
     * Regroup with teammates
     */
    bool RegroupWithTeam() override;

    // ============================================================================
    // PILLAR KITING
    // ============================================================================

    /**
     * Check if should pillar kite
     */
    bool ShouldPillarKite() const override;

    /**
     * Execute pillar kite
     */
    bool ExecutePillarKite() override;

    /**
     * Break line of sight with pillar
     */
    bool BreakLoSWithPillar(::Unit* enemy) override;

    // ============================================================================
    // COOLDOWN COORDINATION
    // ============================================================================

    /**
     * Coordinate offensive burst with team
     */
    bool CoordinateOffensiveBurst() override;

    /**
     * Check if team is ready for burst
     */
    bool IsTeamReadyForBurst() const override;

    /**
     * Signal team for burst
     */
    void SignalBurst() override;

    // ============================================================================
    // CC COORDINATION
    // ============================================================================

    /**
     * Coordinate CC chain with team
     */
    bool CoordinateCCChain(::Unit* target) override;

    /**
     * Check if teammate has CC available
     */
    bool TeammateHasCCAvailable() const override;

    /**
     * Signal CC target to team
     */
    void SignalCCTarget(::Unit* target) override;

    // ============================================================================
    // COMP-SPECIFIC STRATEGIES
    // ============================================================================

    // 2v2 Strategies
    void Execute2v2Strategy() override;
    void Execute2v2DoubleDPS() override;
    void Execute2v2DPSHealer() override;

    // 3v3 Strategies
    void Execute3v3Strategy() override;
    void Execute3v3TripleDPS() override;
    void Execute3v3DoubleDPSHealer() override;
    void Execute3v3TankDPSHealer() override;

    // 5v5 Strategies
    void Execute5v5Strategy() override;

    // ============================================================================
    // COMPOSITION COUNTERS
    // ============================================================================

    /**
     * Get counter strategy for enemy composition
     */
    ArenaStrategy GetCounterStrategy(TeamComposition enemyComp) const override;

    /**
     * Counter RMP (Rogue/Mage/Priest)
     */
    void CounterRMP() override;

    /**
     * Counter TSG (Warrior/DK/Healer)
     */
    void CounterTSG() override;

    /**
     * Counter Turbo Cleave (Warrior/Shaman/Healer)
     */
    void CounterTurboCleave() override;

    // ============================================================================
    // MATCH STATE TRACKING
    // ============================================================================

    ArenaMatchState GetMatchState() const override;
    void UpdateMatchState() override;

    /**
     * Check if team is winning
     */
    bool IsTeamWinning() const override;

    /**
     * Get match duration (seconds)
     */
    uint32 GetMatchDuration() const override;

    // ============================================================================
    // PROFILES
    // ============================================================================

    void SetArenaProfile(ArenaProfile const& profile) override;
    ArenaProfile GetArenaProfile() const override;

    // ============================================================================
    // METRICS
    // ============================================================================

    struct ArenaMetrics
    {
        std::atomic<uint32> matchesWon{0};
        std::atomic<uint32> matchesLost{0};
        std::atomic<uint32> kills{0};
        std::atomic<uint32> deaths{0};
        std::atomic<uint32> pillarKites{0};
        std::atomic<uint32> successfulBursts{0};
        std::atomic<uint32> coordCCs{0};
        std::atomic<uint32> rating{1500}; // Starting rating

        void Reset()
        {
            matchesWon = 0;
            matchesLost = 0;
            kills = 0;
            deaths = 0;
            pillarKites = 0;
            successfulBursts = 0;
            coordCCs = 0;
            rating = 1500;
        }

        float GetWinRate() const
        {
            uint32 total = matchesWon.load() + matchesLost.load();
            return total > 0 ? static_cast<float>(matchesWon.load()) / total : 0.0f;
        }

        float GetKDRatio() const
        {
            uint32 d = deaths.load();
            return d > 0 ? static_cast<float>(kills.load()) / d : static_cast<float>(kills.load());
        }
    };

    ArenaMetrics const& GetMetrics() const override;
    ArenaMetrics const& GetGlobalMetrics() const override;

public:
    /**
     * @brief Construct arena AI for specific bot
     * @param bot The bot player this manager serves
     */
    explicit ArenaAI(Player* bot);
    ~ArenaAI();

    // Non-copyable
    ArenaAI(ArenaAI const&) = delete;
    ArenaAI& operator=(ArenaAI const&) = delete;

private:

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    ArenaBracket GetArenaBracket() const;
    TeamComposition GetTeamComposition() const;
    TeamComposition GetEnemyTeamComposition() const;
    std::vector<::Player*> GetTeammates() const;
    std::vector<::Unit*> GetEnemyTeam() const;
    bool IsInLineOfSight(::Unit* target) const;
    float GetOptimalRangeForClass() const;
    bool IsTeammateInDanger(::Player* teammate) const;

    // ============================================================================
    // PILLAR DATABASE
    // ============================================================================

    void InitializePillarDatabase();
    void LoadArenaPillars(uint32 arenaMapId);

    // Blade's Edge Arena
    void LoadBladesEdgePillars();

    // Nagrand Arena
    void LoadNagrandPillars();

    // Ruins of Lordaeron
    void LoadLordaeronPillars();

    // Dalaran Arena
    void LoadDalaranPillars();

    // Ring of Valor
    void LoadRingOfValorPillars();

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Arena profiles
    std::unordered_map<uint32, ArenaProfile> _playerProfiles;

    // Match states
    std::unordered_map<uint32, ArenaMatchState> _matchStates;

    // Team compositions (playerGuid -> composition)
    std::unordered_map<uint32, TeamComposition> _teamCompositions;
    std::unordered_map<uint32, TeamComposition> _enemyCompositions;

    // Focus targets (playerGuid -> target GUID)
    std::unordered_map<uint32, ObjectGuid> _focusTargets;

    // Pillar database (mapId -> pillars)
    std::unordered_map<uint32, std::vector<ArenaPillar>> _arenaP illars;

    // Burst coordination (playerGuid -> burst ready)
    std::unordered_map<uint32, bool> _burstReady;

    // Metrics
    std::unordered_map<uint32, ArenaMetrics> _playerMetrics;
    ArenaMetrics _globalMetrics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _mutex;

    // Update intervals
    static constexpr uint32 ARENA_UPDATE_INTERVAL = 100;  // 100ms
    std::unordered_map<uint32, uint32> _lastUpdateTimes;

    // Constants
    static constexpr float PILLAR_RANGE = 30.0f;
    static constexpr float REGROUP_RANGE = 15.0f;
    static constexpr float BURST_COORDINATION_RANGE = 40.0f;
};

} // namespace Playerbot
