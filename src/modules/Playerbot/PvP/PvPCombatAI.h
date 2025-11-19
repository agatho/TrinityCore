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
#include "Unit.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{
    // Interface
    #include "Core/DI/Interfaces/IPvPCombatAI.h"


/**
 * @brief PvP target priority types
 */
enum class PvPTargetPriority : uint8
{
    HEALER_HIGHEST = 0,    // Focus healers first
    LOW_HEALTH = 1,        // Target lowest health enemies
    HIGH_THREAT = 2,       // Target enemies attacking allies
    CASTER_FOCUS = 3,      // Focus casters/ranged DPS
    BALANCED = 4           // Balanced priority algorithm
};

/**
 * @brief CC (Crowd Control) types
 */
enum class CCType : uint8
{
    STUN = 0,
    ROOT = 1,
    SILENCE = 2,
    FEAR = 3,
    POLYMORPH = 4,
    DISORIENT = 5,
    INCAPACITATE = 6,
    INTERRUPT = 7
};

/**
 * @brief Defensive cooldown types
 */
enum class DefensiveCooldownType : uint8
{
    IMMUNITY = 0,          // Divine Shield, Ice Block, etc.
    DAMAGE_REDUCTION = 1,  // Shield Wall, Barkskin, etc.
    HEAL = 2,              // Lay on Hands, Healthstone, etc.
    ESCAPE = 3,            // Blink, Shadowstep, etc.
    DISPEL = 4             // Dispel Magic, Cleanse, etc.
};

/**
 * @brief Offensive burst types
 */
enum class OffensiveBurstType : uint8
{
    COOLDOWN_STACK = 0,    // Stack all offensive CDs
    EXECUTE_PHASE = 1,     // Execute abilities for low health targets
    AOE_BURST = 2,         // AoE damage burst
    SINGLE_TARGET = 3      // Single target burst
};

/**
 * @brief PvP combat state
 */
enum class PvPCombatState : uint8
{
    IDLE = 0,
    ENGAGING = 1,
    OFFENSIVE = 2,
    DEFENSIVE = 3,
    PEELING = 4,           // Protecting ally
    KITING = 5,
    RETREATING = 6
};

/**
 * @brief CC chain tracking
 */
struct CCChain
{
    ObjectGuid targetGuid;
    std::vector<CCType> ccSequence;
    uint32 lastCCTime;
    uint32 diminishingReturnsLevel;  // 0-3 (0=full, 1=50%, 2=25%, 3=immune)

    CCChain() : lastCCTime(0), diminishingReturnsLevel(0) {}
};

/**
 * @brief Target threat assessment
 */
struct ThreatAssessment
{
    ObjectGuid targetGuid;
    float threatScore;          // 0-100
    uint32 healthPercent;
    uint32 damageOutput;        // DPS estimate
    bool isHealer;
    bool isCaster;
    bool isAttackingAlly;
    uint32 distanceToPlayer;
    std::vector<uint32> activeCooldowns;

    ThreatAssessment() : threatScore(0.0f), healthPercent(100), damageOutput(0),
        isHealer(false), isCaster(false), isAttackingAlly(false), distanceToPlayer(0) {}
};

/**
 * @brief PvP Combat AI configuration
 */
struct PvPCombatProfile
{
    PvPTargetPriority targetPriority = PvPTargetPriority::BALANCED;
    bool autoInterrupt = true;
    bool autoCCChain = true;
    bool autoDefensiveCooldowns = true;
    bool autoOffensiveBurst = true;
    bool autoTrinket = true;           // Auto-trinket CC
    bool autoPeel = true;              // Protect allies
    uint32 burstHealthThreshold = 30;  // Burst when target below %
    uint32 defensiveHealthThreshold = 40; // Use defensive CDs below %
    float ccChainDelay = 0.5f;         // Delay between CC abilities (seconds)

    PvPCombatProfile() = default;
};

/**
 * @brief PvP Combat AI - Advanced PvP combat automation
 *
 * Features:
 * - Intelligent target priority system
 * - CC chain coordination with diminishing returns
 * - Defensive cooldown management
 * - Offensive burst sequences
 * - Interrupt coordination
 * - Trinket usage
 * - Peel mechanics (protecting allies)
 * - Kiting and positioning
 */
class TC_GAME_API PvPCombatAI final : public IPvPCombatAI
{
public:
    static PvPCombatAI* instance();

    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    void Initialize() override;
    void Update(::Player* player, uint32 diff) override;

    // ============================================================================
    // TARGET SELECTION
    // ============================================================================

    /**
     * Select best PvP target based on priority algorithm
     */
    ::Unit* SelectBestTarget(::Player* player) const override;

    /**
     * Assess threat level of target
     */
    ThreatAssessment AssessThreat(::Player* player, ::Unit* target) const override;

    /**
     * Find all enemy players in range
     */
    std::vector<::Unit*> GetEnemyPlayers(::Player* player, float range) const override;

    /**
     * Find healers in enemy team
     */
    std::vector<::Unit*> GetEnemyHealers(::Player* player) const override;

    /**
     * Switch target if current target is suboptimal
     */
    bool ShouldSwitchTarget(::Player* player) const override;

    // ============================================================================
    // CC CHAIN COORDINATION
    // ============================================================================

    /**
     * Execute CC chain on target
     */
    bool ExecuteCCChain(::Player* player, ::Unit* target) override;

    /**
     * Get next CC ability in chain
     */
    uint32 GetNextCCAbility(::Player* player, ::Unit* target) const override;

    /**
     * Check if target has diminishing returns
     */
    uint32 GetDiminishingReturnsLevel(::Unit* target, CCType ccType) const override;

    /**
     * Track CC used on target
     */
    void TrackCCUsed(::Unit* target, CCType ccType) override;

    /**
     * Check if target is CC immune
     */
    bool IsTargetCCImmune(::Unit* target, CCType ccType) const override;

    // ============================================================================
    // DEFENSIVE COOLDOWNS
    // ============================================================================

    /**
     * Use defensive cooldown if needed
     */
    bool UseDefensiveCooldown(::Player* player) override;

    /**
     * Get best defensive cooldown for situation
     */
    uint32 GetBestDefensiveCooldown(::Player* player) const override;

    /**
     * Check if should use immunity
     */
    bool ShouldUseImmunity(::Player* player) const override;

    /**
     * Use trinket to break CC
     */
    bool UseTrinket(::Player* player) override;

    // ============================================================================
    // OFFENSIVE BURSTS
    // ============================================================================

    /**
     * Execute offensive burst sequence
     */
    bool ExecuteOffensiveBurst(::Player* player, ::Unit* target) override;

    /**
     * Check if should burst target
     */
    bool ShouldBurstTarget(::Player* player, ::Unit* target) const override;

    /**
     * Get offensive cooldowns to use
     */
    std::vector<uint32> GetOffensiveCooldowns(::Player* player) const override;

    /**
     * Stack offensive cooldowns
     */
    bool StackOffensiveCooldowns(::Player* player) override;

    // ============================================================================
    // INTERRUPT COORDINATION
    // ============================================================================

    /**
     * Interrupt enemy cast
     */
    bool InterruptCast(::Player* player, ::Unit* target) override;

    /**
     * Check if should interrupt
     */
    bool ShouldInterrupt(::Player* player, ::Unit* target) const override;

    /**
     * Get interrupt spell ID
     */
    uint32 GetInterruptSpell(::Player* player) const override;

    // ============================================================================
    // PEEL MECHANICS
    // ============================================================================

    /**
     * Peel for ally under attack
     */
    bool PeelForAlly(::Player* player, ::Unit* ally) override;

    /**
     * Find ally needing peel
     */
    ::Unit* FindAllyNeedingPeel(::Player* player) const override;

    /**
     * Get peel ability for class
     */
    uint32 GetPeelAbility(::Player* player) const override;

    // ============================================================================
    // COMBAT STATE
    // ============================================================================

    void SetCombatState(::Player* player, PvPCombatState state) override;
    PvPCombatState GetCombatState(::Player* player) const override;

    // ============================================================================
    // PROFILES
    // ============================================================================

    void SetCombatProfile(uint32 playerGuid, PvPCombatProfile const& profile) override;
    PvPCombatProfile GetCombatProfile(uint32 playerGuid) const override;

    // ============================================================================
    // METRICS
    // ============================================================================

    struct PvPMetrics
    {
        std::atomic<uint32> killsSecured{0};
        std::atomic<uint32> deaths{0};
        std::atomic<uint32> ccChainsExecuted{0};
        std::atomic<uint32> interruptsLanded{0};
        std::atomic<uint32> defensivesUsed{0};
        std::atomic<uint32> burstsExecuted{0};
        std::atomic<uint32> peelsPerformed{0};

        void Reset()
        {
            killsSecured = 0;
            deaths = 0;
            ccChainsExecuted = 0;
            interruptsLanded = 0;
            defensivesUsed = 0;
            burstsExecuted = 0;
            peelsPerformed = 0;
        }

        float GetKDRatio() const
        {
            uint32 d = deaths.load();
            return d > 0 ? static_cast<float>(killsSecured.load()) / d : static_cast<float>(killsSecured.load());
        }
    };

    PvPMetrics const& GetPlayerMetrics(uint32 playerGuid) const override;
    PvPMetrics const& GetGlobalMetrics() const override;

private:
    PvPCombatAI();
    ~PvPCombatAI() = default;

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    bool IsHealer(::Unit* unit) const;
    bool IsCaster(::Unit* unit) const;
    uint32 EstimateDPS(::Unit* unit) const;
    float CalculateThreatScore(::Player* player, ::Unit* target) const;
    bool IsInCCRange(::Player* player, ::Unit* target, CCType ccType) const;
    bool HasCCAvailable(::Player* player, CCType ccType) const;
    uint32 GetCCSpellId(::Player* player, CCType ccType) const;
    bool IsCCOnCooldown(::Player* player, CCType ccType) const;
    std::vector<CCType> GetAvailableCCTypes(::Player* player) const;
    bool IsTargetAttackingAlly(::Unit* target, ::Player* player) const;

    // ============================================================================
    // CLASS-SPECIFIC HELPERS
    // ============================================================================

    // Warrior
    std::vector<uint32> GetWarriorDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetWarriorOffensiveCooldowns(::Player* player) const;
    uint32 GetWarriorInterruptSpell(::Player* player) const;

    // Paladin
    std::vector<uint32> GetPaladinDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetPaladinOffensiveCooldowns(::Player* player) const;

    // Hunter
    std::vector<uint32> GetHunterDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetHunterOffensiveCooldowns(::Player* player) const;

    // Rogue
    std::vector<uint32> GetRogueDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetRogueOffensiveCooldowns(::Player* player) const;

    // Priest
    std::vector<uint32> GetPriestDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetPriestOffensiveCooldowns(::Player* player) const;

    // Death Knight
    std::vector<uint32> GetDeathKnightDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetDeathKnightOffensiveCooldowns(::Player* player) const;

    // Shaman
    std::vector<uint32> GetShamanDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetShamanOffensiveCooldowns(::Player* player) const;

    // Mage
    std::vector<uint32> GetMageDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetMageOffensiveCooldowns(::Player* player) const;

    // Warlock
    std::vector<uint32> GetWarlockDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetWarlockOffensiveCooldowns(::Player* player) const;

    // Monk
    std::vector<uint32> GetMonkDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetMonkOffensiveCooldowns(::Player* player) const;

    // Druid
    std::vector<uint32> GetDruidDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetDruidOffensiveCooldowns(::Player* player) const;

    // Demon Hunter
    std::vector<uint32> GetDemonHunterDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetDemonHunterOffensiveCooldowns(::Player* player) const;

    // Evoker
    std::vector<uint32> GetEvokerDefensiveCooldowns(::Player* player) const;
    std::vector<uint32> GetEvokerOffensiveCooldowns(::Player* player) const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Combat profiles
    std::unordered_map<uint32, PvPCombatProfile> _playerProfiles;

    // Combat states
    std::unordered_map<uint32, PvPCombatState> _combatStates;

    // CC chain tracking (targetGuid -> CCChain)
    std::unordered_map<ObjectGuid, CCChain> _ccChains;

    // Cooldown tracking (playerGuid -> spell ID -> last use time)
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _cooldownTracking;

    // Target tracking (playerGuid -> current target GUID)
    std::unordered_map<uint32, ObjectGuid> _currentTargets;

    // Metrics
    std::unordered_map<uint32, PvPMetrics> _playerMetrics;
    PvPMetrics _globalMetrics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TARGET_SELECTOR> _mutex;

    // Update intervals
    static constexpr uint32 COMBAT_UPDATE_INTERVAL = 100;  // 100ms for PvP responsiveness
    std::unordered_map<uint32, uint32> _lastUpdateTimes;

    // Thresholds
    static constexpr float HEALER_THREAT_MULTIPLIER = 2.0f;
    static constexpr float LOW_HEALTH_THREAT_MULTIPLIER = 1.5f;
    static constexpr float ATTACKING_ALLY_MULTIPLIER = 1.3f;
    static constexpr uint32 MAX_CC_CHAIN_LENGTH = 4;
    static constexpr uint32 DR_RESET_TIME = 18000; // 18 seconds
};

} // namespace Playerbot
