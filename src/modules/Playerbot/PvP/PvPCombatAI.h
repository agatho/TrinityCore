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
    explicit PvPCombatAI(Player* bot);
    ~PvPCombatAI();
    PvPCombatAI(PvPCombatAI const&) = delete;
    PvPCombatAI& operator=(PvPCombatAI const&) = delete;

    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    void Initialize() override;
    void Update(uint32 diff) override;

    // ============================================================================
    // TARGET SELECTION
    // ============================================================================

    /**
     * Select best PvP target based on priority algorithm
     */
    ::Unit* SelectBestTarget() const override;

    /**
     * Assess threat level of target
     */
    ThreatAssessment AssessThreat(::Unit* target) const override;

    /**
     * Find all enemy players in range
     */
    std::vector<::Unit*> GetEnemyPlayers(float range) const override;

    /**
     * Find healers in enemy team
     */
    std::vector<::Unit*> GetEnemyHealers() const override;

    /**
     * Switch target if current target is suboptimal
     */
    bool ShouldSwitchTarget() const override;

    // ============================================================================
    // CC CHAIN COORDINATION
    // ============================================================================

    /**
     * Execute CC chain on target
     */
    bool ExecuteCCChain(::Unit* target) override;

    /**
     * Get next CC ability in chain
     */
    uint32 GetNextCCAbility(::Unit* target) const override;

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
    bool UseDefensiveCooldown() override;

    /**
     * Get best defensive cooldown for situation
     */
    uint32 GetBestDefensiveCooldown() const override;

    /**
     * Check if should use immunity
     */
    bool ShouldUseImmunity() const override;

    /**
     * Use trinket to break CC
     */
    bool UseTrinket() override;

    // ============================================================================
    // OFFENSIVE BURSTS
    // ============================================================================

    /**
     * Execute offensive burst sequence
     */
    bool ExecuteOffensiveBurst(::Unit* target) override;

    /**
     * Check if should burst target
     */
    bool ShouldBurstTarget(::Unit* target) const override;

    /**
     * Get offensive cooldowns to use
     */
    std::vector<uint32> GetOffensiveCooldowns() const override;

    /**
     * Stack offensive cooldowns
     */
    bool StackOffensiveCooldowns() override;

    // ============================================================================
    // INTERRUPT COORDINATION
    // ============================================================================

    /**
     * Interrupt enemy cast
     */
    bool InterruptCast(::Unit* target) override;

    /**
     * Check if should interrupt
     */
    bool ShouldInterrupt(::Unit* target) const override;

    /**
     * Get interrupt spell ID
     */
    uint32 GetInterruptSpell() const override;

    // ============================================================================
    // PEEL MECHANICS
    // ============================================================================

    /**
     * Peel for ally under attack
     */
    bool PeelForAlly(::Unit* ally) override;

    /**
     * Find ally needing peel
     */
    ::Unit* FindAllyNeedingPeel() const override;

    /**
     * Get peel ability for class
     */
    uint32 GetPeelAbility() const override;

    // ============================================================================
    // COMBAT STATE
    // ============================================================================

    void SetCombatState(PvPCombatState state) override;
    PvPCombatState GetCombatState() const override;

    // ============================================================================
    // PROFILES
    // ============================================================================

    void SetCombatProfile(PvPCombatProfile const& profile) override;
    PvPCombatProfile GetCombatProfile() const override;

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

    PvPMetrics const& GetMetrics() const override;
    PvPMetrics const& GetGlobalMetrics() const override;

private:
    Player* _bot;

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    bool IsHealer(::Unit* unit) const;
    bool IsCaster(::Unit* unit) const;
    uint32 EstimateDPS(::Unit* unit) const;
    float CalculateThreatScore(::Unit* target) const;
    bool IsInCCRange(::Unit* target, CCType ccType) const;
    bool HasCCAvailable(CCType ccType) const;
    uint32 GetCCSpellId(CCType ccType) const;
    bool IsCCOnCooldown(CCType ccType) const;
    std::vector<CCType> GetAvailableCCTypes() const;
    bool IsTargetAttackingAlly(::Unit* target) const;

    // ============================================================================
    // CLASS-SPECIFIC HELPERS
    // ============================================================================

    // Warrior
    std::vector<uint32> GetWarriorDefensiveCooldowns() const;
    std::vector<uint32> GetWarriorOffensiveCooldowns() const;
    uint32 GetWarriorInterruptSpell() const;

    // Paladin
    std::vector<uint32> GetPaladinDefensiveCooldowns() const;
    std::vector<uint32> GetPaladinOffensiveCooldowns() const;

    // Hunter
    std::vector<uint32> GetHunterDefensiveCooldowns() const;
    std::vector<uint32> GetHunterOffensiveCooldowns() const;

    // Rogue
    std::vector<uint32> GetRogueDefensiveCooldowns() const;
    std::vector<uint32> GetRogueOffensiveCooldowns() const;

    // Priest
    std::vector<uint32> GetPriestDefensiveCooldowns() const;
    std::vector<uint32> GetPriestOffensiveCooldowns() const;

    // Death Knight
    std::vector<uint32> GetDeathKnightDefensiveCooldowns() const;
    std::vector<uint32> GetDeathKnightOffensiveCooldowns() const;

    // Shaman
    std::vector<uint32> GetShamanDefensiveCooldowns() const;
    std::vector<uint32> GetShamanOffensiveCooldowns() const;

    // Mage
    std::vector<uint32> GetMageDefensiveCooldowns() const;
    std::vector<uint32> GetMageOffensiveCooldowns() const;

    // Warlock
    std::vector<uint32> GetWarlockDefensiveCooldowns() const;
    std::vector<uint32> GetWarlockOffensiveCooldowns() const;

    // Monk
    std::vector<uint32> GetMonkDefensiveCooldowns() const;
    std::vector<uint32> GetMonkOffensiveCooldowns() const;

    // Druid
    std::vector<uint32> GetDruidDefensiveCooldowns() const;
    std::vector<uint32> GetDruidOffensiveCooldowns() const;

    // Demon Hunter
    std::vector<uint32> GetDemonHunterDefensiveCooldowns() const;
    std::vector<uint32> GetDemonHunterOffensiveCooldowns() const;

    // Evoker
    std::vector<uint32> GetEvokerDefensiveCooldowns() const;
    std::vector<uint32> GetEvokerOffensiveCooldowns() const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Per-bot instance data
    PvPCombatProfile _profile;
    PvPCombatState _combatState{PvPCombatState::IDLE};

    // CC chain tracking (targetGuid -> CCChain) - tracks this bot's CC chains on various targets
    std::unordered_map<ObjectGuid, CCChain> _ccChains;

    // Cooldown tracking (spell ID -> last use time)
    std::unordered_map<uint32, uint32> _cooldownTracking;

    // Current target
    ObjectGuid _currentTarget;

    // Per-bot metrics
    PvPMetrics _metrics;

    // Last update time
    uint32 _lastUpdateTime{0};

    // Shared static data
    static PvPMetrics _globalMetrics;

    // Update intervals
    static constexpr uint32 COMBAT_UPDATE_INTERVAL = 100;  // 100ms for PvP responsiveness

    // Thresholds
    static constexpr float HEALER_THREAT_MULTIPLIER = 2.0f;
    static constexpr float LOW_HEALTH_THREAT_MULTIPLIER = 1.5f;
    static constexpr float ATTACKING_ALLY_MULTIPLIER = 1.3f;
    static constexpr uint32 MAX_CC_CHAIN_LENGTH = 4;
    static constexpr uint32 DR_RESET_TIME = 18000; // 18 seconds
};

} // namespace Playerbot
