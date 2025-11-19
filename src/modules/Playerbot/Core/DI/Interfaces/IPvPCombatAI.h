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
    virtual void Update(::Player* player, uint32 diff) = 0;

    // Target selection
    virtual ::Unit* SelectBestTarget(::Player* player) const = 0;
    virtual ThreatAssessment AssessThreat(::Player* player, ::Unit* target) const = 0;
    virtual std::vector<::Unit*> GetEnemyPlayers(::Player* player, float range) const = 0;
    virtual std::vector<::Unit*> GetEnemyHealers(::Player* player) const = 0;
    virtual bool ShouldSwitchTarget(::Player* player) const = 0;

    // CC chain coordination
    virtual bool ExecuteCCChain(::Player* player, ::Unit* target) = 0;
    virtual uint32 GetNextCCAbility(::Player* player, ::Unit* target) const = 0;
    virtual uint32 GetDiminishingReturnsLevel(::Unit* target, CCType ccType) const = 0;
    virtual void TrackCCUsed(::Unit* target, CCType ccType) = 0;
    virtual bool IsTargetCCImmune(::Unit* target, CCType ccType) const = 0;

    // Defensive cooldowns
    virtual bool UseDefensiveCooldown(::Player* player) = 0;
    virtual uint32 GetBestDefensiveCooldown(::Player* player) const = 0;
    virtual bool ShouldUseImmunity(::Player* player) const = 0;
    virtual bool UseTrinket(::Player* player) = 0;

    // Offensive bursts
    virtual bool ExecuteOffensiveBurst(::Player* player, ::Unit* target) = 0;
    virtual bool ShouldBurstTarget(::Player* player, ::Unit* target) const = 0;
    virtual std::vector<uint32> GetOffensiveCooldowns(::Player* player) const = 0;
    virtual bool StackOffensiveCooldowns(::Player* player) = 0;

    // Interrupt coordination
    virtual bool InterruptCast(::Player* player, ::Unit* target) = 0;
    virtual bool ShouldInterrupt(::Player* player, ::Unit* target) const = 0;
    virtual uint32 GetInterruptSpell(::Player* player) const = 0;

    // Peel mechanics
    virtual bool PeelForAlly(::Player* player, ::Unit* ally) = 0;
    virtual ::Unit* FindAllyNeedingPeel(::Player* player) const = 0;
    virtual uint32 GetPeelAbility(::Player* player) const = 0;

    // Combat state
    virtual void SetCombatState(::Player* player, PvPCombatState state) = 0;
    virtual PvPCombatState GetCombatState(::Player* player) const = 0;

    // Profiles
    virtual void SetCombatProfile(uint32 playerGuid, PvPCombatProfile const& profile) = 0;
    virtual PvPCombatProfile GetCombatProfile(uint32 playerGuid) const = 0;

    // Metrics
    virtual PvPMetrics const& GetPlayerMetrics(uint32 playerGuid) const = 0;
    virtual PvPMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
