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
struct ArenaPillar;
struct ArenaMatchState;
struct ArenaProfile;
enum class ArenaStrategy : uint8;
enum class TeamComposition : uint8;

/**
 * @brief Interface for arena AI automation
 *
 * Provides complete arena automation including bracket strategies,
 * team composition analysis, pillar kiting, focus target coordination,
 * positioning algorithms, and composition-specific counters.
 */
class TC_GAME_API IArenaAI
{
public:
    struct ArenaMetrics
    {
        uint32 matchesWon;
        uint32 matchesLost;
        uint32 kills;
        uint32 deaths;
        uint32 pillarKites;
        uint32 successfulBursts;
        uint32 coordCCs;
        uint32 rating;

        float GetWinRate() const
        {
            uint32 total = matchesWon + matchesLost;
            return total > 0 ? static_cast<float>(matchesWon) / total : 0.0f;
        }

        float GetKDRatio() const
        {
            return deaths > 0 ? static_cast<float>(kills) / deaths : static_cast<float>(kills);
        }
    };

    virtual ~IArenaAI() = default;

    // Initialization
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;
    virtual void OnMatchStart(::Player* player) = 0;
    virtual void OnMatchEnd(::Player* player, bool won) = 0;

    // Strategy selection
    virtual void AnalyzeTeamComposition(::Player* player) = 0;
    virtual ArenaStrategy GetStrategyForComposition(TeamComposition teamComp, TeamComposition enemyComp) const = 0;
    virtual void AdaptStrategy(::Player* player) = 0;

    // Target selection
    virtual ::Unit* SelectFocusTarget(::Player* player) const = 0;
    virtual bool ShouldSwitchTarget(::Player* player, ::Unit* currentTarget) const = 0;
    virtual std::vector<::Unit*> GetKillTargetPriority(::Player* player) const = 0;

    // Positioning
    virtual void ExecutePositioning(::Player* player) = 0;
    virtual ArenaPillar const* FindBestPillar(::Player* player) const = 0;
    virtual bool MoveToPillar(::Player* player, ArenaPillar const& pillar) = 0;
    virtual bool IsUsingPillarEffectively(::Player* player) const = 0;
    virtual bool MaintainOptimalDistance(::Player* player) = 0;
    virtual bool RegroupWithTeam(::Player* player) = 0;

    // Pillar kiting
    virtual bool ShouldPillarKite(::Player* player) const = 0;
    virtual bool ExecutePillarKite(::Player* player) = 0;
    virtual bool BreakLoSWithPillar(::Player* player, ::Unit* enemy) = 0;

    // Cooldown coordination
    virtual bool CoordinateOffensiveBurst(::Player* player) = 0;
    virtual bool IsTeamReadyForBurst(::Player* player) const = 0;
    virtual void SignalBurst(::Player* player) = 0;

    // CC coordination
    virtual bool CoordinateCCChain(::Player* player, ::Unit* target) = 0;
    virtual bool TeammateHasCCAvailable(::Player* player) const = 0;
    virtual void SignalCCTarget(::Player* player, ::Unit* target) = 0;

    // Comp-specific strategies
    virtual void Execute2v2Strategy(::Player* player) = 0;
    virtual void Execute2v2DoubleDPS(::Player* player) = 0;
    virtual void Execute2v2DPSHealer(::Player* player) = 0;
    virtual void Execute3v3Strategy(::Player* player) = 0;
    virtual void Execute3v3TripleDPS(::Player* player) = 0;
    virtual void Execute3v3DoubleDPSHealer(::Player* player) = 0;
    virtual void Execute3v3TankDPSHealer(::Player* player) = 0;
    virtual void Execute5v5Strategy(::Player* player) = 0;

    // Composition counters
    virtual ArenaStrategy GetCounterStrategy(TeamComposition enemyComp) const = 0;
    virtual void CounterRMP(::Player* player) = 0;
    virtual void CounterTSG(::Player* player) = 0;
    virtual void CounterTurboCleave(::Player* player) = 0;

    // Match state tracking
    virtual ArenaMatchState GetMatchState(::Player* player) const = 0;
    virtual void UpdateMatchState(::Player* player) = 0;
    virtual bool IsTeamWinning(::Player* player) const = 0;
    virtual uint32 GetMatchDuration(::Player* player) const = 0;

    // Profiles
    virtual void SetArenaProfile(uint32 playerGuid, ArenaProfile const& profile) = 0;
    virtual ArenaProfile GetArenaProfile(uint32 playerGuid) const = 0;

    // Metrics
    virtual ArenaMetrics const& GetPlayerMetrics(uint32 playerGuid) const = 0;
    virtual ArenaMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
