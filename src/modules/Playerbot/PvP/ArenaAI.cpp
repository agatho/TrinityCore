/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArenaAI.h"
#include "Player.h"
#include "Unit.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "World.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

ArenaAI::ArenaAI(Player* bot)
    : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot.arena", "ArenaAI: Attempted to create with null bot!");
        return;
    }

    // Initialize shared arena map data once
    if (!_initialized)
    {
        TC_LOG_INFO("playerbot.arena", "ArenaAI: Loading arena map data...");
        // Load arena pillar positions for each map
        // This would be loaded from database or hardcoded positions
        _initialized = true;
        TC_LOG_INFO("playerbot.arena", "ArenaAI: Arena map data initialized");
    }

    TC_LOG_DEBUG("playerbot.arena", "ArenaAI: Created for bot {} ({})",
                 _bot->GetName(), _bot->GetGUID().ToString());
}

ArenaAI::~ArenaAI()
{
    TC_LOG_DEBUG("playerbot.arena", "ArenaAI: Destroyed for bot {} ({})",
                 _bot ? _bot->GetName() : "Unknown",
                 _bot ? _bot->GetGUID().ToString() : "Unknown");
}


// Static member initialization
std::unordered_map<uint32, std::vector<ArenaPillar>> ArenaAI::_arenaMapPillars;
ArenaAI::ArenaMetrics ArenaAI::_globalMetrics;
bool ArenaAI::_initialized = false;




// ============================================================================
// INITIALIZATION
// ============================================================================

void ArenaAI::Initialize()
{
    std::lock_guard lock(_mutex);

    TC_LOG_INFO("playerbot", "ArenaAI: Initializing arena systems...");

    InitializePillarDatabase();

    TC_LOG_INFO("playerbot", "ArenaAI: Initialization complete");
}

void ArenaAI::InitializePillarDatabase()
{
    // Load pillar locations for each arena map
    LoadBladesEdgePillars();
    LoadNagrandPillars();
    LoadLordaeronPillars();
    LoadDalaranPillars();
    LoadRingOfValorPillars();

    TC_LOG_INFO("playerbot", "ArenaAI: Loaded {} arena pillar configurations",
        _arenaPillars.size());
}

void ArenaAI::LoadBladesEdgePillars()
{
    uint32 mapId = 562; // Blade's Edge Arena
    std::vector<ArenaPillar> pillars;

    // Bridge pillar (center)
    pillars.push_back(ArenaPillar(Position(6238.0f, 262.0f, 0.8f, 0.0f), 8.0f));

    // Side pillars
    pillars.push_back(ArenaPillar(Position(6229.0f, 272.0f, 0.8f, 0.0f), 5.0f));
    pillars.push_back(ArenaPillar(Position(6247.0f, 252.0f, 0.8f, 0.0f), 5.0f));

    _arenaPillars[mapId] = pillars;
}

void ArenaAI::LoadNagrandPillars()
{
    uint32 mapId = 559; // Nagrand Arena
    std::vector<ArenaPillar> pillars;

    // Center pillars
    pillars.push_back(ArenaPillar(Position(4055.0f, 2919.0f, 13.6f, 0.0f), 6.0f));
    pillars.push_back(ArenaPillar(Position(4037.0f, 2935.0f, 13.6f, 0.0f), 6.0f));

    // Corner pillars
    pillars.push_back(ArenaPillar(Position(4070.0f, 2934.0f, 13.6f, 0.0f), 5.0f));
    pillars.push_back(ArenaPillar(Position(4022.0f, 2920.0f, 13.6f, 0.0f), 5.0f));

    _arenaPillars[mapId] = pillars;
}

void ArenaAI::LoadLordaeronPillars()
{
    uint32 mapId = 572; // Ruins of Lordaeron
    std::vector<ArenaPillar> pillars;

    // Tombstone pillars
    pillars.push_back(ArenaPillar(Position(1285.0f, 1667.0f, 39.6f, 0.0f), 4.0f));
    pillars.push_back(ArenaPillar(Position(1295.0f, 1677.0f, 39.6f, 0.0f), 4.0f));
    pillars.push_back(ArenaPillar(Position(1305.0f, 1667.0f, 39.6f, 0.0f), 4.0f));
    pillars.push_back(ArenaPillar(Position(1295.0f, 1657.0f, 39.6f, 0.0f), 4.0f));

    _arenaPillars[mapId] = pillars;
}

void ArenaAI::LoadDalaranPillars()
{
    uint32 mapId = 617; // Dalaran Arena
    std::vector<ArenaPillar> pillars;

    // Water pipes (center)
    pillars.push_back(ArenaPillar(Position(1299.0f, 784.0f, 9.3f, 0.0f), 6.0f));
    pillars.push_back(ArenaPillar(Position(1214.0f, 765.0f, 9.3f, 0.0f), 6.0f));

    _arenaPillars[mapId] = pillars;
}

void ArenaAI::LoadRingOfValorPillars()
{
    uint32 mapId = 618; // Ring of Valor
    std::vector<ArenaPillar> pillars;

    // Center pillars (when lowered)
    pillars.push_back(ArenaPillar(Position(763.0f, -284.0f, 28.3f, 0.0f), 7.0f));
    pillars.push_back(ArenaPillar(Position(763.0f, -294.0f, 28.3f, 0.0f), 7.0f));

    _arenaPillars[mapId] = pillars;
}

void ArenaAI::Update(uint32 diff)
{
    if (!player || !_bot->IsInWorld())
        return;

    // Check if player is in arena
    if (!_bot->InArena())
        return;
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Throttle updates (100ms for arena responsiveness)
    if (_lastUpdateTimes.count(_bot->GetGUID().GetCounter()))
    {
        uint32 timeSinceLastUpdate = currentTime - _lastUpdateTime;
        if (timeSinceLastUpdate < ARENA_UPDATE_INTERVAL)
            return;
    }

    _lastUpdateTime = currentTime;

    std::lock_guard lock(_mutex);

    // Update match state
    UpdateMatchState(player);

    // Analyze composition if not done
    if (!_teamCompositions.count(_bot->GetGUID().GetCounter()))
        AnalyzeTeamComposition(player);

    // Execute positioning
    ExecutePositioning(player);

    // Execute bracket-specific strategy
    ArenaBracket bracket = GetArenaBracket();
    switch (bracket)
    {
        case ArenaBracket::BRACKET_2v2:
            Execute2v2Strategy(player);
            break;

        case ArenaBracket::BRACKET_3v3:
            Execute3v3Strategy(player);
            break;

        case ArenaBracket::BRACKET_5v5:
            Execute5v5Strategy(player);
            break;

        default:
            break;
    }

    // Adapt strategy based on match state
    AdaptStrategy(player);
}

void ArenaAI::OnMatchStart()
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);
    // Initialize match state
    ArenaMatchState state;
    state.matchStartTime = GameTime::GetGameTimeMS();
    _matchState = state;
    // Analyze compositions
    AnalyzeTeamComposition(player);

    TC_LOG_INFO("playerbot", "ArenaAI: Match started for player {}", _bot->GetGUID().GetCounter());
}

void ArenaAI::OnMatchEnd(bool won)
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);
    // Update metrics
    if (won)
    {
        _metrics.matchesWon++;
        _globalMetrics.matchesWon++;
        _metrics.rating += 15; // Simplified rating gain
    }
    else
    {
        _metrics.matchesLost++;
        _globalMetrics.matchesLost++;
        uint32 currentRating = _metrics.rating.load();
        if (currentRating > 15)
            _metrics.rating -= 15;
    }

    TC_LOG_INFO("playerbot", "ArenaAI: Match ended for player {} - {} (Rating: {})",
        _bot->GetGUID().GetCounter(), won ? "WON" : "LOST", _metrics.rating.load());

    // Clear match data}

// ============================================================================
// STRATEGY SELECTION
// ============================================================================

void ArenaAI::AnalyzeTeamComposition()
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);
    TeamComposition teamComp = GetTeamComposition();
    TeamComposition enemyComp = GetEnemyTeamComposition();

    _teamComposition = teamComp;
    _enemyComposition = enemyComp;

    // Select strategy based on compositions
    ArenaStrategy strategy = GetStrategyForComposition(teamComp, enemyComp);
    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    profile.strategy = strategy;
    SetArenaProfile(_bot->GetGUID().GetCounter(), profile);

    TC_LOG_INFO("playerbot", "ArenaAI: Player {} team comp: {}, enemy comp: {}, strategy: {}",
        _bot->GetGUID().GetCounter(), static_cast<uint32>(teamComp), static_cast<uint32>(enemyComp),
        static_cast<uint32>(strategy));
}

ArenaStrategy ArenaAI::GetStrategyForComposition(TeamComposition teamComp,
    TeamComposition enemyComp) const
{
    // If enemy has healer, prioritize killing healer
    if (enemyComp == TeamComposition::DPS_HEALER ||
        enemyComp == TeamComposition::DOUBLE_DPS_HEALER ||
        enemyComp == TeamComposition::TANK_DPS_HEALER)
    {
        return ArenaStrategy::KILL_HEALER_FIRST;
    }

    // If both teams are triple DPS, kill lowest health
    if (teamComp == TeamComposition::TRIPLE_DPS &&
        enemyComp == TeamComposition::TRIPLE_DPS)
    {
        return ArenaStrategy::KILL_LOWEST_HEALTH;
    }

    // Default: adaptive strategy
    return ArenaStrategy::ADAPTIVE;
}

void ArenaAI::AdaptStrategy()
{
    if (!_bot)
        return;
    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());

    if (profile.strategy != ArenaStrategy::ADAPTIVE)
        return;

    ArenaMatchState state = GetMatchState(player);
    // If team is losing, switch to aggressive
    if (!state.isWinning)
    {
        profile.positioning = PositioningStrategy::AGGRESSIVE;
    }
    // If team is winning, play defensively
    else
    {
        profile.positioning = PositioningStrategy::DEFENSIVE;
    }

    SetArenaProfile(_bot->GetGUID().GetCounter(), profile);
}

// ============================================================================
// TARGET SELECTION
// ============================================================================

::Unit* ArenaAI::SelectFocusTarget() const
{
    if (!_bot)
        return nullptr;

    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    std::vector<::Unit*> enemies = GetEnemyTeam();

    if (enemies.empty())
        return nullptr;

    switch (profile.strategy)
    {
        case ArenaStrategy::KILL_HEALER_FIRST:
        {
            // Find healer
            for (::Unit* enemy : enemies)
            {
                if (enemy->IsPlayer())
                {
                    ::Player* enemyPlayer = enemy->ToPlayer();
                    // Check if healer (simplified)
                    uint8 enemyClass = enemyPlayer->getClass();
                    if (enemyClass == CLASS_PRIEST || enemyClass == CLASS_PALADIN ||
                        enemyClass == CLASS_SHAMAN || enemyClass == CLASS_DRUID ||
                        enemyClass == CLASS_MONK || enemyClass == CLASS_EVOKER)
                        return enemy;
                }
            }
            break;
        }

        case ArenaStrategy::KILL_LOWEST_HEALTH:
        {
            // Find lowest health target
            ::Unit* lowestHealthTarget = nullptr;
            uint32 lowestHealth = 100;

            for (::Unit* enemy : enemies)
            {
                uint32 healthPct = enemy->GetHealthPct();
                if (healthPct < lowestHealth)
                {
                    lowestHealth = healthPct;
                    lowestHealthTarget = enemy;
                }
            }

            return lowestHealthTarget;
        }

        case ArenaStrategy::TRAIN_ONE_TARGET:
        {
            // Keep attacking same target
            if (_focusTargets.count(_bot->GetGUID().GetCounter()))
            {
                ObjectGuid targetGuid = _focusTarget;
                return ObjectAccessor::GetUnit(*player, targetGuid);
            }
            break;
        }

        default:
            break;
    }

    // Default: return first enemy
    return enemies.empty() ? nullptr : enemies[0];
}

bool ArenaAI::ShouldSwitchTarget(::Unit* currentTarget) const
{
    if (!player || !currentTarget)
        return true;

    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    if (!profile.autoSwitch)
        return false;

    // Switch if current target is dead
    if (currentTarget->IsDead())
        return true;

    // Switch if better target is available (e.g., healer in LoS)
    ::Unit* newTarget = SelectFocusTarget(player);
    if (newTarget && newTarget != currentTarget)
    {
        // Switch if new target is healer and in LoS
        if (newTarget->IsPlayer())
        {
            ::Player* newPlayer = newTarget->ToPlayer();
            uint8 newClass = newPlayer->getClass();
                 newClass == CLASS_MONK || newClass == CLASS_EVOKER) &&
                IsInLineOfSight(newTarget))
            {
                return true;
            }
        }
    }

    return false;
}

std::vector<::Unit*> ArenaAI::GetKillTargetPriority() const
{
    std::vector<::Unit*> priorities;

    if (!_bot)
        return priorities;

    std::vector<::Unit*> enemies = GetEnemyTeam();
    // Sort by priority: Healers > Low health > DPS
    std::sort(enemies.begin(), enemies.end(),
        [](::Unit* a, ::Unit* b) {
            bool aIsHealer = false, bIsHealer = false;
            if (a->IsPlayer())
            {
                uint8 aClass = a->ToPlayer()->getClass();
                aIsHealer = (aClass == CLASS_PRIEST || aClass == CLASS_PALADIN ||
                            aClass == CLASS_SHAMAN || aClass == CLASS_DRUID ||
                            aClass == CLASS_MONK || aClass == CLASS_EVOKER);
            }

            if (b->IsPlayer())
            {
                uint8 bClass = b->ToPlayer()->getClass();
                bIsHealer = (bClass == CLASS_PRIEST || bClass == CLASS_PALADIN ||
                            bClass == CLASS_SHAMAN || bClass == CLASS_DRUID ||
                            bClass == CLASS_MONK || bClass == CLASS_EVOKER);
            }

            if (aIsHealer && !bIsHealer) return true;
            if (!aIsHealer && bIsHealer) return false;

            // If both same priority, sort by health
            return a->GetHealthPct() < b->GetHealthPct();
        });

    return enemies;
}

// ============================================================================
// POSITIONING
// ============================================================================

void ArenaAI::ExecutePositioning()
{
    if (!_bot)
        return;

    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    switch (profile.positioning)
    {
        case PositioningStrategy::AGGRESSIVE:
            // Move forward, pressure enemies
            break;

        case PositioningStrategy::DEFENSIVE:
            // Stay back, maintain distance
            MaintainOptimalDistance(player);
            break;

        case PositioningStrategy::PILLAR_KITE:
            if (ShouldPillarKite(player))
                ExecutePillarKite(player);
            break;

        case PositioningStrategy::SPREAD_OUT:
            // Spread from teammates to avoid AoE
            break;

        case PositioningStrategy::GROUP_UP:
            RegroupWithTeam(player);
            break;

        default:
            break;
    }
}

ArenaPillar const* ArenaAI::FindBestPillar() const
{
    if (!_bot)
        return nullptr;

    uint32 mapId = player->GetMapId();
    if (!_arenaPillars.count(mapId))
        return nullptr;

    std::vector<ArenaPillar> const& pillars = _arenaPillars.at(mapId);
    if (pillars.empty())
        return nullptr;

    // Find closest pillar
    ArenaPillar const* bestPillar = nullptr;
    float closestDistanceSq = 9999.0f * 9999.0f; // Squared distance

    for (ArenaPillar const& pillar : pillars)
    {
        if (!pillar.isAvailable)
            continue;

        float distanceSq = player->GetExactDistSq(pillar.position);
        if (distanceSq < closestDistanceSq)
        {
            closestDistanceSq = distanceSq;
            bestPillar = &pillar;
        }
    }

    return bestPillar;
}

bool ArenaAI::MoveToPillar(ArenaPillar const& pillar)
{
    if (!_bot)
        return false;

    float distance = std::sqrt(_bot->GetExactDistSq(pillar.position)); // Calculate once from squared distance
    if (distance < 5.0f)
        return true;

    // Full implementation: Use PathGenerator to move to pillar
    TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} moving to pillar",
        _bot->GetGUID().GetCounter());

    return false;
}

bool ArenaAI::IsUsingPillarEffectively() const
{
    if (!_bot)
        return false;

    // Check if player is behind pillar and enemy is out of LoS
    std::vector<::Unit*> enemies = GetEnemyTeam();
    for (::Unit* enemy : enemies)
    {
        if (!IsInLineOfSight(enemy))
            return true; // Successfully broke LoS
    }

    return false;
}

bool ArenaAI::MaintainOptimalDistance()
{
    if (!_bot)
        return false;

    float optimalRange = GetOptimalRangeForClass();
    std::vector<::Unit*> enemies = GetEnemyTeam();

    for (::Unit* enemy : enemies)
    {
        float distance = std::sqrt(_bot->GetExactDistSq(enemy)); // Calculate once from squared distance
        if (distance < optimalRange)
        {
            // Too close - kite away
            TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} kiting away from enemy",
                _bot->GetGUID().GetCounter());
            // Full implementation: Move away from enemy
            return true;
        }
    }

    return false;
}

bool ArenaAI::RegroupWithTeam()
{
    if (!_bot)
        return false;

    std::vector<::Player*> teammates = GetTeammates();
    if (teammates.empty())
        return false;

    // Find teammate position
    Position teammatePos = teammates[0]->GetPosition();
    float distance = std::sqrt(_bot->GetExactDistSq(teammatePos)); // Calculate once from squared distance
    if (distance > REGROUP_RANGE)
    {
        // Move to teammate
        TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} regrouping with team",
            _bot->GetGUID().GetCounter());
        // Full implementation: Move to teammate
        return false;
    }

    return true;
}

// ============================================================================
// PILLAR KITING
// ============================================================================

bool ArenaAI::ShouldPillarKite() const
{
    if (!_bot)
        return false;

    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    if (!profile.usePillars)
        return false;

    // Pillar kite if health below threshold
    if (_bot->GetHealthPct() < profile.pillarKiteHealthThreshold)
        return true;

    // Pillar kite if under heavy pressure
    std::vector<::Unit*> enemies = GetEnemyTeam();
    uint32 enemiesAttacking = 0;

    for (::Unit* enemy : enemies)
    {
        if (enemy->GetVictim() == player)
            enemiesAttacking++;
    }

    return enemiesAttacking >= 2;
}

bool ArenaAI::ExecutePillarKite()
{
    if (!_bot)
        return false;

    ArenaPillar const* pillar = FindBestPillar(player);
    if (!pillar)
        return false;

    // Move to pillar
    if (!MoveToPillar(player, *pillar))
        return false;

    // Break LoS with enemies
    std::vector<::Unit*> enemies = GetEnemyTeam();
    for (::Unit* enemy : enemies)
    {
        if (IsInLineOfSight(enemy))
            BreakLoSWithPillar(player, enemy);
    }

    // Update metrics
    _metrics.pillarKites++;
    _globalMetrics.pillarKites++;

    return true;
}

bool ArenaAI::BreakLoSWithPillar(::Unit* enemy)
{
    if (!player || !enemy)
        return false;

    // Full implementation: Position player behind pillar relative to enemy
    TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} breaking LoS with pillar",
        _bot->GetGUID().GetCounter());

    return true;
}

// ============================================================================
// COOLDOWN COORDINATION
// ============================================================================

bool ArenaAI::CoordinateOffensiveBurst()
{
    if (!_bot)
        return false;

    if (!IsTeamReadyForBurst(player))
        return false;

    // Signal burst
    SignalBurst(player);

    // Use offensive cooldowns
    TC_LOG_INFO("playerbot", "ArenaAI: Player {} coordinating offensive burst",
        _bot->GetGUID().GetCounter());

    // Full implementation: Cast offensive cooldowns

    // Update metrics
    _metrics.successfulBursts++;
    _globalMetrics.successfulBursts++;

    return true;
}
bool ArenaAI::IsTeamReadyForBurst() const
{
    if (!_bot)
        return false;

    std::vector<::Player*> teammates = GetTeammates();

    // Check if teammates are in range and have cooldowns
    for (::Player* teammate : teammates)
    {
        float distance = std::sqrt(_bot->GetExactDistSq(teammate)); // Calculate once from squared distance
        if (distance > BURST_COORDINATION_RANGE)
            return false;

        // Full implementation: Check if teammate has offensive CDs available
    }

    return true;
}

void ArenaAI::SignalBurst()
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);
    _burstReady = true;

    TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} signaling burst", _bot->GetGUID().GetCounter());
}

// ============================================================================
// CC COORDINATION
// ============================================================================

bool ArenaAI::CoordinateCCChain(::Unit* target)
{
    if (!player || !target)
        return false;

    // Signal CC target to team
    SignalCCTarget(player, target);

    // Execute CC
    TC_LOG_INFO("playerbot", "ArenaAI: Player {} coordinating CC chain",
        _bot->GetGUID().GetCounter());

    // Update metrics
    _metrics.coordCCs++;
    _globalMetrics.coordCCs++;

    return true;
}

bool ArenaAI::TeammateHasCCAvailable() const
{
    if (!_bot)
        return false;

    std::vector<::Player*> teammates = GetTeammates();
    for (::Player* teammate : teammates)
    {
        // Full implementation: Check if teammate has CC available
        // Query cooldowns and spell availability
    }

    return false;
}

void ArenaAI::SignalCCTarget(::Unit* target)
{
    if (!player || !target)
        return;

    TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} signaling CC target {}",
        _bot->GetGUID().GetCounter(), target->GetGUID().GetCounter());

    // Full implementation: Communicate CC target to team
}

// ============================================================================
// 2v2 STRATEGIES
// ============================================================================

void ArenaAI::Execute2v2Strategy()
{
    if (!_bot)
        return;
    TeamComposition comp = _teamComposition;

    switch (comp)
    {
        case TeamComposition::DOUBLE_DPS:
            Execute2v2DoubleDPS(player);
            break;

        case TeamComposition::DPS_HEALER:
            Execute2v2DPSHealer(player);
            break;

        default:
            break;
    }
}

void ArenaAI::Execute2v2DoubleDPS()
{
    if (!_bot)
        return;

    // Double DPS: Aggressive strategy, burst same target
    ::Unit* target = SelectFocusTarget(player);
    if (target)
    {
        player->SetSelection(target->GetGUID());
        CoordinateOffensiveBurst(player);
    }
}

void ArenaAI::Execute2v2DPSHealer()
{
    if (!_bot)
        return;

    // DPS/Healer: Protect healer, pressure enemy healer
    std::vector<::Player*> teammates = GetTeammates();
    for (::Player* teammate : teammates)
    {
        if (IsTeammateInDanger(teammate))
        {
            // Peel for teammate
            TC_LOG_DEBUG("playerbot", "ArenaAI: Player {} peeling for teammate",
                _bot->GetGUID().GetCounter());
            break;
        }
    }

    // Attack enemy healer
    ::Unit* enemyHealer = SelectFocusTarget(player);
    if (enemyHealer)
        player->SetSelection(enemyHealer->GetGUID());
}

// ============================================================================
// 3v3 STRATEGIES
// ============================================================================

void ArenaAI::Execute3v3Strategy()
{
    if (!_bot)
        return;
    TeamComposition comp = _teamComposition;
    switch (comp)
    {
        case TeamComposition::TRIPLE_DPS:
            Execute3v3TripleDPS(player);
            break;

        case TeamComposition::DOUBLE_DPS_HEALER:
            Execute3v3DoubleDPSHealer(player);
            break;

        case TeamComposition::TANK_DPS_HEALER:
            Execute3v3TankDPSHealer(player);
            break;

        default:
            break;
    }
}

void ArenaAI::Execute3v3TripleDPS()
{
    if (!_bot)
        return;

    // Triple DPS: Train one target until death
    ::Unit* target = SelectFocusTarget(player);
    if (target)
    {
        player->SetSelection(target->GetGUID());

        // Save target as focus
        std::lock_guard lock(_mutex);
        _focusTargets[_bot->GetGUID().GetCounter()] = target->GetGUID();
    }
}

void ArenaAI::Execute3v3DoubleDPSHealer()
{
    if (!_bot)
        return;

    // Standard comp: Kill enemy healer, protect friendly healer
    TeamComposition enemyComp = _enemyCompositions[_bot->GetGUID().GetCounter()];
    if (enemyComp == TeamComposition::DOUBLE_DPS_HEALER)
    {
        // Focus enemy healer
        ::Unit* enemyHealer = SelectFocusTarget(player);
        if (enemyHealer)
            player->SetSelection(enemyHealer->GetGUID());
    }
}

void ArenaAI::Execute3v3TankDPSHealer()
{
    if (!_bot)
        return;

    // Tanky comp: Survive and outlast enemy team
    if (_bot->GetHealthPct() < 50)
    {
        // Play defensively
        ExecutePillarKite(player);
    }
}

// ============================================================================
// 5v5 STRATEGIES
// ============================================================================

void ArenaAI::Execute5v5Strategy()
{
    if (!_bot)
        return;

    // 5v5: Focus fire on priority targets, stay grouped
    ::Unit* target = SelectFocusTarget(player);
    if (target)
        player->SetSelection(target->GetGUID());

    // Stay grouped
    RegroupWithTeam(player);
}

// ============================================================================
// COMPOSITION COUNTERS
// ============================================================================

ArenaStrategy ArenaAI::GetCounterStrategy(TeamComposition enemyComp) const
{
    switch (enemyComp)
    {
        case TeamComposition::DOUBLE_DPS_HEALER:
            return ArenaStrategy::KILL_HEALER_FIRST;

        case TeamComposition::TRIPLE_DPS:
            return ArenaStrategy::SPREAD_PRESSURE;

        default:
            return ArenaStrategy::ADAPTIVE;
    }
}

void ArenaAI::CounterRMP()
{
    // Counter RMP: Spread out, avoid CC chains, kill healer
    if (!_bot)
        return;

    TC_LOG_DEBUG("playerbot", "ArenaAI: Countering RMP composition");

    // Use defensive positioning
    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    profile.positioning = PositioningStrategy::SPREAD_OUT;
    SetArenaProfile(_bot->GetGUID().GetCounter(), profile);
}

void ArenaAI::CounterTSG()
{
    // Counter TSG: Kite melee, pillar, kill healer
    if (!_bot)
        return;

    TC_LOG_DEBUG("playerbot", "ArenaAI: Countering TSG composition");

    ArenaProfile profile = GetArenaProfile(_bot->GetGUID().GetCounter());
    profile.positioning = PositioningStrategy::PILLAR_KITE;
    SetArenaProfile(_bot->GetGUID().GetCounter(), profile);
}

void ArenaAI::CounterTurboCleave()
{
    // Counter Turbo: Interrupt healer, kite melee
    if (!_bot)
        return;

    TC_LOG_DEBUG("playerbot", "ArenaAI: Countering Turbo Cleave composition");
}

// ============================================================================
// MATCH STATE TRACKING
// ============================================================================

ArenaMatchState ArenaAI::GetMatchState() const
{
    if (!_bot)
        return ArenaMatchState();

    std::lock_guard lock(_mutex);
    if (_matchStates.count(_bot->GetGUID().GetCounter()))
        return _matchState;

    return ArenaMatchState();
}

void ArenaAI::UpdateMatchState()
{
    if (!_bot)
        return;

    std::lock_guard lock(_mutex);
    if (!_matchStates.count(_bot->GetGUID().GetCounter()))
        return;

    ArenaMatchState& state = _matchState;

    // Update state
    state.isWinning = IsTeamWinning(player);

    // Count alive teammates and enemies
    std::vector<::Player*> teammates = GetTeammates();
    state.teammateAliveCount = 0;
    for (::Player* teammate : teammates)
    {
        if (!teammate->IsDead())
            state.teammateAliveCount++;
    }

    std::vector<::Unit*> enemies = GetEnemyTeam();
    state.enemyAliveCount = 0;
    for (::Unit* enemy : enemies)
    {
        if (!enemy->IsDead())
            state.enemyAliveCount++;
    }
}

bool ArenaAI::IsTeamWinning() const
{
    if (!_bot)
        return false;

    ArenaMatchState state = GetMatchState(player);
    return state.teammateAliveCount > state.enemyAliveCount;
}

uint32 ArenaAI::GetMatchDuration() const
{
    if (!_bot)
        return 0;

    ArenaMatchState state = GetMatchState(player);
    uint32 currentTime = GameTime::GetGameTimeMS();

    return (currentTime - state.matchStartTime) / 1000; // Convert to seconds
}

// ============================================================================
// PROFILES
// ============================================================================

void ArenaAI::SetArenaProfile(ArenaProfile const& profile)
{
    std::lock_guard lock(_mutex);
    _profile = profile;
}

ArenaProfile ArenaAI::GetArenaProfile() const
{
    std::lock_guard lock(_mutex);

    if (_playerProfiles.count(_bot->GetGUID().GetCounter()))
        return _profile;

    return ArenaProfile(); // Default
}

// ============================================================================
// METRICS
// ============================================================================

ArenaAI::ArenaMetrics const& ArenaAI::GetMetrics() const
{
    std::lock_guard lock(_mutex);

    if (!_playerMetrics.count(_bot->GetGUID().GetCounter()))
    {
        static ArenaMetrics emptyMetrics;
        return emptyMetrics;
    }

    return _metrics;
}

ArenaAI::ArenaMetrics const& ArenaAI::GetGlobalMetrics() const
{
    return _globalMetrics;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

ArenaBracket ArenaAI::GetArenaBracket() const
{
    if (!_bot)
        return ArenaBracket::BRACKET_2v2;

    // Full implementation: Query arena type
    // For now, default to 3v3
    return ArenaBracket::BRACKET_3v3;
}

TeamComposition ArenaAI::GetTeamComposition() const
{
    if (!_bot)
        return TeamComposition::DOUBLE_DPS_HEALER;

    // Full implementation: Analyze team classes/specs
    return TeamComposition::DOUBLE_DPS_HEALER;
}

TeamComposition ArenaAI::GetEnemyTeamComposition() const
{
    if (!_bot)
        return TeamComposition::DOUBLE_DPS_HEALER;

    // Full implementation: Analyze enemy team classes/specs
    return TeamComposition::DOUBLE_DPS_HEALER;
}

std::vector<::Player*> ArenaAI::GetTeammates() const
{
    std::vector<::Player*> teammates;

    if (!_bot)
        return teammates;

    Group* group = _bot->GetGroup();
    if (!group)
        return teammates;

    for (GroupReference* ref : *group)
    {
        ::Player* member = ref->GetSource();
        if (member && member != player && member->IsInWorld() && !member->IsDead())
            teammates.push_back(member);
    }

    return teammates;
}

std::vector<::Unit*> ArenaAI::GetEnemyTeam() const
{
    std::vector<::Unit*> enemies;

    if (!_bot)
        return enemies;

    // Full implementation: Find all hostile players in arena
    return enemies;
}

bool ArenaAI::IsInLineOfSight(::Unit* target) const
{
    if (!player || !target)
        return false;

    return player->IsWithinLOSInMap(target);
}

float ArenaAI::GetOptimalRangeForClass() const
{
    if (!_bot)
        return 10.0f;

    uint8 playerClass = player->getClass();

    switch (playerClass)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_ROGUE:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return 5.0f; // Melee range

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_EVOKER:
            return 30.0f; // Ranged

        default:
            return 10.0f;
    }
}

bool ArenaAI::IsTeammateInDanger(::Player* teammate) const
{
    if (!teammate)
        return false;

    // Check if teammate is low health or under heavy attack
    if (teammate->GetHealthPct() < 30)
        return true;

    return false;
}

} // namespace Playerbot
