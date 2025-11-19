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
#include <vector>
#include <cstdint>

// Forward declarations
class Player;
class Unit;

namespace Playerbot
{

// Forward declarations
struct ThreatAssessment;
struct PvPCombatProfile;
enum class PvPCombatState : uint8;
enum class CCType : uint8;

/**
 * @brief Interface for PvP combat AI automation
 *
 * Provides advanced PvP combat automation including intelligent target priority,
 * CC chain coordination with diminishing returns, defensive/offensive cooldown
 * management, interrupt coordination, trinket usage, and peel mechanics.
 */
class TC_GAME_API IPvPCombatAI
{
public:
    struct PvPMetrics
    {
        uint32 killsSecured;
        uint32 deaths;
        uint32 ccChainsExecuted;
        uint32 interruptsLanded;
        uint32 defensivesUsed;
        uint32 burstsExecuted;
        uint32 peelsPerformed;

        float GetKDRatio() const
        {
            return deaths > 0 ? static_cast<float>(killsSecured) / deaths : static_cast<float>(killsSecured);
        }
    };

    virtual ~IPvPCombatAI() = default;

    // Initialization
    virtual void Initialize() = 0;
    virtual void Update(uint32 diff) = 0;

    // Target selection
    virtual ::Unit* SelectBestTarget() const = 0;
    virtual ThreatAssessment AssessThreat(::Unit* target) const = 0;
    virtual std::vector<::Unit*> GetEnemyPlayers(float range) const = 0;
    virtual std::vector<::Unit*> GetEnemyHealers() const = 0;
    virtual bool ShouldSwitchTarget() const = 0;

    // CC chain coordination
    virtual bool ExecuteCCChain(::Unit* target) = 0;
    virtual uint32 GetNextCCAbility(::Unit* target) const = 0;
    virtual uint32 GetDiminishingReturnsLevel(::Unit* target, CCType ccType) const = 0;
    virtual void TrackCCUsed(::Unit* target, CCType ccType) = 0;
    virtual bool IsTargetCCImmune(::Unit* target, CCType ccType) const = 0;

    // Defensive cooldowns
    virtual bool UseDefensiveCooldown() = 0;
    virtual uint32 GetBestDefensiveCooldown() const = 0;
    virtual bool ShouldUseImmunity() const = 0;
    virtual bool UseTrinket() = 0;

    // Offensive bursts
    virtual bool ExecuteOffensiveBurst(::Unit* target) = 0;
    virtual bool ShouldBurstTarget(::Unit* target) const = 0;
    virtual std::vector<uint32> GetOffensiveCooldowns() const = 0;
    virtual bool StackOffensiveCooldowns() = 0;

    // Interrupt coordination
    virtual bool InterruptCast(::Unit* target) = 0;
    virtual bool ShouldInterrupt(::Unit* target) const = 0;
    virtual uint32 GetInterruptSpell() const = 0;

    // Peel mechanics
    virtual bool PeelForAlly(::Unit* ally) = 0;
    virtual ::Unit* FindAllyNeedingPeel() const = 0;
    virtual uint32 GetPeelAbility() const = 0;

    // Combat state
    virtual void SetCombatState(PvPCombatState state) = 0;
    virtual PvPCombatState GetCombatState() const = 0;

    // Profiles
    virtual void SetCombatProfile(PvPCombatProfile const& profile) = 0;
    virtual PvPCombatProfile GetCombatProfile() const = 0;

    // Metrics
    virtual PvPMetrics const& GetMetrics() const = 0;
    virtual PvPMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
