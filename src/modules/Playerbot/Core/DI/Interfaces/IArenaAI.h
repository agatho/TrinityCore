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
    virtual void Update(uint32 diff) = 0;
    virtual void OnMatchStart() = 0;
    virtual void OnMatchEnd(bool won) = 0;

    // Strategy selection
    virtual void AnalyzeTeamComposition() = 0;
    virtual ArenaStrategy GetStrategyForComposition(TeamComposition teamComp, TeamComposition enemyComp) const = 0;
    virtual void AdaptStrategy() = 0;

    // Target selection
    virtual ::Unit* SelectFocusTarget() const = 0;
    virtual bool ShouldSwitchTarget(::Unit* currentTarget) const = 0;
    virtual std::vector<::Unit*> GetKillTargetPriority() const = 0;

    // Positioning
    virtual void ExecutePositioning() = 0;
    virtual ArenaPillar const* FindBestPillar() const = 0;
    virtual bool MoveToPillar(ArenaPillar const& pillar) = 0;
    virtual bool IsUsingPillarEffectively() const = 0;
    virtual bool MaintainOptimalDistance() = 0;
    virtual bool RegroupWithTeam() = 0;

    // Pillar kiting
    virtual bool ShouldPillarKite() const = 0;
    virtual bool ExecutePillarKite() = 0;
    virtual bool BreakLoSWithPillar(::Unit* enemy) = 0;

    // Cooldown coordination
    virtual bool CoordinateOffensiveBurst() = 0;
    virtual bool IsTeamReadyForBurst() const = 0;
    virtual void SignalBurst() = 0;

    // CC coordination
    virtual bool CoordinateCCChain(::Unit* target) = 0;
    virtual bool TeammateHasCCAvailable() const = 0;
    virtual void SignalCCTarget(::Unit* target) = 0;

    // Comp-specific strategies
    virtual void Execute2v2Strategy() = 0;
    virtual void Execute2v2DoubleDPS() = 0;
    virtual void Execute2v2DPSHealer() = 0;
    virtual void Execute3v3Strategy() = 0;
    virtual void Execute3v3TripleDPS() = 0;
    virtual void Execute3v3DoubleDPSHealer() = 0;
    virtual void Execute3v3TankDPSHealer() = 0;
    virtual void Execute5v5Strategy() = 0;

    // Composition counters
    virtual ArenaStrategy GetCounterStrategy(TeamComposition enemyComp) const = 0;
    virtual void CounterRMP() = 0;
    virtual void CounterTSG() = 0;
    virtual void CounterTurboCleave() = 0;

    // Match state tracking
    virtual ArenaMatchState GetMatchState() const = 0;
    virtual void UpdateMatchState() = 0;
    virtual bool IsTeamWinning() const = 0;
    virtual uint32 GetMatchDuration() const = 0;

    // Profiles
    virtual void SetArenaProfile(ArenaProfile const& profile) = 0;
    virtual ArenaProfile GetArenaProfile() const = 0;

    // Metrics
    virtual ArenaMetrics const& GetMetrics() const = 0;
    virtual ArenaMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
