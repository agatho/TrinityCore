/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PositionManager.h"
#include "Player.h"
#include "Unit.h"
#include "Map.h"
#include "Log.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "PathGenerator.h"
#include "MoveSpline.h"
#include "PhasingHandler.h"
#include "MotionMaster.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "PhaseShift.h"
#include "SharedDefines.h"
#include "../../Spatial/SpatialGridManager.h"
#include "../../Spatial/SpatialGridQueryHelpers.h"  // PHASE 5B: Thread-safe helpers
#include "ObjectAccessor.h"

namespace Playerbot
{

PositionManager::PositionManager(Player* bot, BotThreatManager* threatManager)
    : _bot(bot), _threatManager(threatManager), _updateInterval(DEFAULT_UPDATE_INTERVAL),
      _lastUpdate(0), _positionTolerance(POSITION_TOLERANCE), _maxCandidates(MAX_CANDIDATES),
      _lastZoneUpdate(0), _lastMovePointTime(0)
{

    if (!_threatManager)
    {
        TC_LOG_ERROR("playerbot", "PositionManager: ThreatManager is null for bot {}", _bot->GetName());
        return;
    }

    TC_LOG_DEBUG("playerbot.position", "PositionManager initialized for bot {}", _bot->GetName());
}

MovementResult PositionManager::UpdatePosition(const MovementContext& context)
{
    auto startTime = std::chrono::steady_clock::now();
    MovementResult result;
    // No lock needed - position data is per-bot instance data

    try
    {
        // FIX #3: SPELL CASTING COORDINATION - Don't move while casting
        if (_bot->IsNonMeleeSpellCast(false))
        {
            result.failureReason = "Bot is casting, movement would interrupt spell";
            TC_LOG_DEBUG("playerbot.position", "⏸️ Bot {} - Movement blocked, currently casting",
                         _bot->GetName());
            return result;
        }

        uint32 currentTime = GameTime::GetGameTimeMS();
        if (currentTime - _lastUpdate < _updateInterval && !context.emergencyMode)
        {
            result.failureReason = "Update interval not reached";
            return result;
        }

        _lastUpdate = currentTime;
        UpdateAoEZones(currentTime);

        Position currentPos = _bot->GetPosition();
        result.currentPosition = currentPos;

        PositionInfo currentPosInfo = EvaluatePosition(currentPos, context);

        if (!context.emergencyMode && currentPosInfo.score >= 80.0f && currentPosInfo.priority >= MovementPriority::OPTIMIZATION)
        {
            result.success = true;
            result.targetPosition = currentPos;
            result.priority = MovementPriority::MAINTENANCE;
            return result;
        }

        if (context.emergencyMode || IsInDangerZone(currentPos))
        {
            return HandleEmergencyMovement(context);
        }

        MovementResult optimalResult = FindOptimalPosition(context);
        if (optimalResult.success)
        {
            float movementDistance = currentPos.GetExactDist(&optimalResult.targetPosition);
            if (movementDistance > _positionTolerance)
            {
                return ExecuteMovement(optimalResult.targetPosition, optimalResult.priority);
            }
        }

        result.success = true;
        result.targetPosition = currentPos;
        result.priority = MovementPriority::MAINTENANCE;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.failureReason = "Exception during position update: " + std::string(e.what());
        TC_LOG_ERROR("playerbot.position", "Exception in UpdatePosition for bot {}: {}", _bot->GetName(), e.what());
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "UpdatePosition");

    return result;
}

MovementResult PositionManager::FindOptimalPosition(const MovementContext& context)
{
    MovementResult result;
    std::vector<Position> candidates = GenerateCandidatePositions(context);
    if (candidates.empty())
    {
        result.failureReason = "No candidate positions generated";
        return result;
    }

    std::vector<PositionInfo> evaluatedPositions = EvaluatePositions(candidates, context);
    if (evaluatedPositions.empty())
    {
        result.failureReason = "No valid positions after evaluation";
        return result;
    }

    std::sort(evaluatedPositions.begin(), evaluatedPositions.end(), std::greater<PositionInfo>());

    PositionInfo bestPosition = evaluatedPositions[0];
    result.success = true;
    result.targetPosition = bestPosition.position;
    result.priority = bestPosition.priority;
    result.estimatedTime = EstimateMovementTime(_bot->GetPosition(), bestPosition.position);
    result.pathDistance = _bot->GetExactDist(&bestPosition.position);
    TC_LOG_DEBUG("playerbot.position", "Found optimal position for bot {} at ({:.2f}, {:.2f}, {:.2f}) with score {:.2f}",
               _bot->GetName(), bestPosition.position.GetPositionX(), bestPosition.position.GetPositionY(),
               bestPosition.position.GetPositionZ(), bestPosition.score);

    return result;
}

MovementResult PositionManager::ExecuteMovement(const Position& targetPos, MovementPriority priority)
{
    MovementResult result;
    result.targetPosition = targetPos;
    result.priority = priority;

    if (!CanReachPosition(targetPos))
    {
        result.failureReason = "Target position unreachable";
        return result;
    }

    Position currentPos = _bot->GetPosition();
    float distance = currentPos.GetExactDist(&targetPos);

    if (distance <= _positionTolerance)
    {
        result.success = true;
        result.pathDistance = 0.0f;
        result.estimatedTime = 0.0f;
        return result;
    }

    std::vector<Position> waypoints = CalculateWaypoints(currentPos, targetPos);
    result.waypoints = waypoints;
    result.pathDistance = 0.0f;

    for (size_t i = 1; i < waypoints.size(); ++i)
    {
        result.pathDistance += waypoints[i-1].GetExactDist(&waypoints[i]);
    }

    result.estimatedTime = EstimateMovementTime(currentPos, targetPos);

    if (priority <= MovementPriority::CRITICAL)
    {
        result.requiresSprint = true;
    }

    float heightDiff = std::abs(targetPos.GetPositionZ() - currentPos.GetPositionZ());
    if (heightDiff > 3.0f)
    {
        result.requiresJump = true;
    }

    // FIX #1: DUPLICATE MOVEMENT PREVENTION - Check if already moving to same position
    // This prevents the infinite movement cancellation bug (60+ MovePoint calls/second)
    uint32 currentTime = GameTime::GetGameTimeMS();
    float distanceToLastTarget = _lastTargetPosition.GetExactDist(&targetPos);

    if (distanceToLastTarget < _positionTolerance && (currentTime - _lastMovePointTime) < 500)
    {
        // Already moving to same destination - don't re-issue command within 500ms
        result.success = true;
        result.failureReason = "Already moving to target position";
        TC_LOG_DEBUG("playerbot.position", "⏭️ Bot {} - Duplicate movement prevented, already moving to ({:.2f}, {:.2f}, {:.2f})",
                     _bot->GetName(), targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ());

        return result;
    }

    // Issue new movement command
    _bot->GetMotionMaster()->MovePoint(0, targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ());

    // FIX #5: SPEED CONTROL SYSTEM - Apply sprint for critical/emergency movement
    if (result.requiresSprint)
    {
        // Increase movement speed for urgent repositioning
        // Note: TrinityCore handles speed through auras/spells, not direct modification
        // This is a marker for future sprint ability integration
        TC_LOG_DEBUG("playerbot.position", "🏃 Bot {} - Sprint required for urgent movement (priority: {})",
                     _bot->GetName(), static_cast<uint8>(priority));

        // Future: Trigger sprint ability here if available
        // For now, just log the requirement - ClassAI should handle sprint abilities
    }

    // Update tracking state
    _lastTargetPosition = targetPos;
    _lastMovePointTime = currentTime;

    result.success = true;
    _metrics.movementCommands++;

    TC_LOG_DEBUG("playerbot.position", "Bot {} moving to position ({:.2f}, {:.2f}, {:.2f}), distance: {:.2f}",
               _bot->GetName(), targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ(), distance);

    return result;
}

PositionInfo PositionManager::EvaluatePosition(const Position& pos, const MovementContext& context)
{
    auto startTime = std::chrono::steady_clock::now();
    PositionInfo info;
    info.position = pos;
    info.evaluationTime = GameTime::GetGameTimeMS();

    if (!ValidatePosition(pos, context.validationFlags))
    {
        info.score = 0.0f;
        info.priority = MovementPriority::IDLE;
        return info;
    }

    float totalScore = 0.0f;

    totalScore += CalculateDistanceScore(pos, context) * context.weights.distanceWeight;
    totalScore += CalculateSafetyScore(pos, context) * context.weights.safetyWeight;
    totalScore += CalculateLineOfSightScore(pos, context) * context.weights.losWeight;
    totalScore += CalculateAngleScore(pos, context) * context.weights.angleWeight;
    totalScore += CalculateGroupScore(pos, context) * context.weights.groupWeight;
    totalScore += CalculateEscapeScore(pos, context) * context.weights.escapeWeight;

    info.score = std::max(0.0f, totalScore);
    info.distanceToTarget = context.target ? pos.GetExactDist(context.target) : 0.0f;
    info.hasLineOfSight = context.target ? _bot->IsWithinLOSInMap(context.target) : true;
    info.isOptimalRange = (info.distanceToTarget >= context.preferredRange * 0.8f &&
                          info.distanceToTarget <= context.preferredRange * 1.2f);
    info.safetyRating = CalculateSafetyScore(pos, context);
    info.movementCost = CalculateMovementCost(_bot->GetPosition(), pos);
    if (info.score >= 90.0f)
        info.priority = MovementPriority::OPTIMIZATION;
    else if (info.score >= 70.0f)
        info.priority = MovementPriority::TACTICAL;
    else if (info.score >= 50.0f)
        info.priority = MovementPriority::MAINTENANCE;
    else
        info.priority = MovementPriority::IDLE;

    if (IsInDangerZone(pos))
    {
        info.priority = MovementPriority::EMERGENCY;
        info.score *= 0.1f;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "EvaluatePosition");

    _metrics.positionEvaluations++;
    return info;
}

std::vector<PositionInfo> PositionManager::EvaluatePositions(const std::vector<Position>& positions, const MovementContext& context)
{
    std::vector<PositionInfo> results;
    results.reserve(positions.size());

    for (const Position& pos : positions)
    {
        PositionInfo info = EvaluatePosition(pos, context);
        if (info.score > 0.0f)
        {
            results.push_back(info);
        }
    }

    return results;
}

std::vector<Position> PositionManager::GenerateCandidatePositions(const MovementContext& context)
{
    std::vector<Position> candidates;

    if (!context.target)
    {
        return candidates;
    }

    Position targetPos = context.target->GetPosition();
    // Variables for potential position calculations
    (void)context.maxRange; // Silence unused warning

    switch (context.desiredType)
    {
        case PositionType::MELEE_COMBAT:
            candidates = GenerateCircularPositions(targetPos, 4.0f, 8);
            break;

        case PositionType::RANGED_DPS:
            {
                std::vector<Position> innerRing = GenerateCircularPositions(targetPos, context.preferredRange * 0.8f, 12);
                std::vector<Position> outerRing = GenerateCircularPositions(targetPos, context.preferredRange * 1.2f, 12);
                candidates.insert(candidates.end(), innerRing.begin(), innerRing.end());
                candidates.insert(candidates.end(), outerRing.begin(), outerRing.end());
            }
            break;

        case PositionType::HEALING:
            {
                if (!context.groupMembers.empty())
                {
                    Position groupCenter = PositionUtils::CalculateGroupCenter(context.groupMembers);
                    candidates = GenerateCircularPositions(groupCenter, 20.0f, 16);
                }
                else
                {
                    candidates = GenerateCircularPositions(targetPos, 25.0f, 12);
                }
            }
            break;

        case PositionType::KITING:
            {
                Position currentPos = _bot->GetPosition();
                float angle = PositionUtils::CalculateAngleBetween(targetPos, currentPos);
                candidates = GenerateArcPositions(targetPos, context.preferredRange, angle - M_PI/3, angle + M_PI/3, 8);
            }
            break;

        case PositionType::FLANKING:
            {
                float targetAngle = context.target->GetOrientation();
                float leftFlankAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI/2);
                float rightFlankAngle = PositionUtils::NormalizeAngle(targetAngle - M_PI/2);

                candidates.push_back(PositionUtils::CalculatePositionAtAngle(targetPos, 6.0f, leftFlankAngle));
                candidates.push_back(PositionUtils::CalculatePositionAtAngle(targetPos, 6.0f, rightFlankAngle));
            }
            break;

        case PositionType::TANKING:
            {
                float targetAngle = context.target->GetOrientation();
                float frontAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
                candidates = GenerateArcPositions(targetPos, 5.0f, frontAngle - M_PI/6, frontAngle + M_PI/6, 6);
            }
            break;

        default:
            candidates = GenerateCircularPositions(targetPos, context.preferredRange, 12);
            break;
    }

    if (candidates.size() > _maxCandidates)
    {
        candidates.resize(_maxCandidates);
    }

    return candidates;
}

Position PositionManager::FindMeleePosition(Unit* target, bool preferBehind)
{
    if (!target)
        return _bot->GetPosition();

    Position targetPos = target->GetPosition();
    float targetAngle = target->GetOrientation();
    if (preferBehind)
    {
        float behindAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
        return PositionUtils::CalculatePositionAtAngle(targetPos, 3.5f, behindAngle);
    }
    else
    {
        std::vector<Position> candidates = GenerateCircularPositions(targetPos, 4.0f, 8);

        MovementContext context;
        context.bot = _bot;
        context.target = target;
        context.desiredType = PositionType::MELEE_COMBAT;
        context.validationFlags = PositionValidation::COMBAT;

        PositionInfo bestPosition;
        for (const Position& pos : candidates)
        {
            PositionInfo info = EvaluatePosition(pos, context);
            if (info.score > bestPosition.score)
            {
                bestPosition = info;
            }
        }

        return bestPosition.position;
    }
}

Position PositionManager::FindRangedPosition(Unit* target, float preferredRange)
{
    if (!target)
        return _bot->GetPosition();

    Position targetPos = target->GetPosition();
    std::vector<Position> candidates = GenerateCircularPositions(targetPos, preferredRange, 16);
    MovementContext context;
    context.bot = _bot;
    context.target = target;
    context.desiredType = PositionType::RANGED_DPS;
    context.preferredRange = preferredRange;
    context.validationFlags = PositionValidation::SAFE;

    PositionInfo bestPosition;
    for (const Position& pos : candidates)
    {
        PositionInfo info = EvaluatePosition(pos, context);
        if (info.score > bestPosition.score)
        {
            bestPosition = info;
        }
    }

    return bestPosition.position;
}

Position PositionManager::FindKitingPosition(Unit* threat, float minDistance)
{
    if (!threat)
        return _bot->GetPosition();

    Position currentPos = _bot->GetPosition();
    Position threatPos = threat->GetPosition();

    float currentDistance = currentPos.GetExactDist(&threatPos);
    if (currentDistance >= minDistance * 1.2f)
    {
        return currentPos;
    }

    float escapeAngle = PositionUtils::CalculateAngleBetween(threatPos, currentPos);

    std::vector<Position> escapePositions;
    for (int i = -2; i <= 2; ++i)
    {
        float angle = PositionUtils::NormalizeAngle(escapeAngle + (i * M_PI/6));
        Position escapePos = PositionUtils::CalculatePositionAtAngle(threatPos, minDistance * 1.5f, angle);
        escapePositions.push_back(escapePos);
    }

    MovementContext context;
    context.bot = _bot;
    context.target = threat;
    context.desiredType = PositionType::KITING;
    context.preferredRange = minDistance * 1.5f;
    context.validationFlags = PositionValidation::BASIC;

    PositionInfo bestPosition;
    for (const Position& pos : escapePositions)
    {
        PositionInfo info = EvaluatePosition(pos, context);
        if (info.score > bestPosition.score)
        {
            bestPosition = info;
        }
    }

    return bestPosition.position;
}

bool PositionManager::IsPositionSafe(const Position& pos, const MovementContext& context)
{
    if (IsInDangerZone(pos))
        return false;

    for (Unit* enemy : context.nearbyEnemies)
    {
        if (!enemy)
            continue;

        float distance = pos.GetExactDist(enemy);
        if (distance < 5.0f && enemy->IsHostileTo(_bot))
        {
            return false;
        }
    }
    return ValidatePosition(pos, PositionValidation::SAFE);
}

bool PositionManager::IsInDangerZone(const Position& pos)
{
    uint32 currentTime = GameTime::GetGameTimeMS();

    for (const AoEZone& zone : _activeZones)
    {
        if (zone.IsPositionInDanger(pos, currentTime))
        {
            return true;
        }
    }

    return false;
}

Position PositionManager::FindSafePosition(const Position& fromPos, float minDistance)
{
    std::vector<Position> safePositions = GenerateCircularPositions(fromPos, minDistance, 16);

    for (const Position& pos : safePositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }
    std::vector<Position> farPositions = GenerateCircularPositions(fromPos, minDistance * 2.0f, 16);
    for (const Position& pos : farPositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }

    return fromPos;
}

void PositionManager::RegisterAoEZone(const AoEZone& zone)
{
    // No lock needed - position data is per-bot instance data
    _activeZones.push_back(zone);

    TC_LOG_DEBUG("playerbot.position", "Registered AoE zone for bot {} at ({:.2f}, {:.2f}) radius {:.2f}",
               _bot->GetName(), zone.center.GetPositionX(), zone.center.GetPositionY(), zone.radius);
}

void PositionManager::UpdateAoEZones(uint32 currentTime)
{
    if (currentTime - _lastZoneUpdate < 1000)  // Update every second
        return;

    ClearExpiredZones(currentTime);
    _lastZoneUpdate = currentTime;
}

void PositionManager::ClearExpiredZones(uint32 currentTime)
{
    _activeZones.erase(
        std::remove_if(_activeZones.begin(), _activeZones.end(),
            [currentTime](const AoEZone& zone) {
                return !zone.isActive || currentTime > zone.startTime + zone.duration;
            }),
        _activeZones.end()
    );
}

bool PositionManager::ValidatePosition(const Position& pos, PositionValidation flags)
{
    if ((static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::WALKABLE)) && !IsWalkablePosition(pos))
        return false;

    if (static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::STABLE_GROUND))
    {
        Map* map = _bot->GetMap();
        if (!map || !PositionUtils::IsPositionOnGround(pos, map))
            return false;
    }

    if (static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::NO_OBSTACLES))
    {
        if (!PositionUtils::CanWalkStraightLine(_bot->GetPosition(), pos, _bot->GetMap()))
            return false;
    }

    if ((static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::AVOID_AOE)) && IsInDangerZone(pos))
        return false;

    return true;
}

bool PositionManager::IsWalkablePosition(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return map->IsInWater(_bot->GetPhaseShift(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()) == false &&
           PositionUtils::IsPositionOnGround(pos, map);
}

float PositionManager::CalculateMovementCost(const Position& from, const Position& to)
{
    float distance = from.GetExactDist(&to);
    float heightDiff = std::abs(to.GetPositionZ() - from.GetPositionZ());

    float cost = distance;
    if (heightDiff > 2.0f)
        cost += heightDiff * 2.0f;  // Penalty for elevation changes

    if (!PositionUtils::CanWalkStraightLine(from, to, _bot->GetMap()))
        cost *= 1.5f;  // Penalty for indirect paths

    return cost;
}

MovementResult PositionManager::HandleEmergencyMovement(const MovementContext& /* context */)
{
    _metrics.emergencyMoves++;

    Position emergencyPos = FindEmergencyEscapePosition();

    MovementResult result;
    result.priority = MovementPriority::EMERGENCY;
    result.requiresSprint = true;

    return ExecuteMovement(emergencyPos, MovementPriority::EMERGENCY);
}

Position PositionManager::FindEmergencyEscapePosition()
{
    Position currentPos = _bot->GetPosition();
    std::vector<Position> escapePositions = GenerateCircularPositions(currentPos, EMERGENCY_DISTANCE, 12);

    for (const Position& pos : escapePositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }

    return currentPos;
}

float PositionManager::CalculateDistanceScore(const Position& pos, const MovementContext& context)
{
    if (!context.target)
        return 50.0f;

    float distance = pos.GetExactDist(context.target);
    float optimalDistance = context.preferredRange;

    if (distance <= optimalDistance * 1.2f && distance >= optimalDistance * 0.8f)
        return 100.0f;

    float deviation = std::abs(distance - optimalDistance) / optimalDistance;
    return std::max(0.0f, 100.0f - (deviation * 100.0f));
}

float PositionManager::CalculateSafetyScore(const Position& pos, const MovementContext& context)
{
    float score = 100.0f;

    if (IsInDangerZone(pos))
        score -= 80.0f;

    for (Unit* enemy : context.nearbyEnemies)
    {
        if (!enemy)
            continue;

        float distance = pos.GetExactDist(enemy);
        if (distance < 5.0f)
            score -= 30.0f;
        else if (distance < 10.0f)
            score -= 15.0f;
    }

    return std::max(0.0f, score);
}

float PositionManager::CalculateLineOfSightScore(const Position& /* pos */, const MovementContext& context)
{
    if (!context.target)
        return 100.0f;

    if (_bot->IsWithinLOSInMap(context.target))
        return 100.0f;
    else
        return 0.0f;
}

float PositionManager::CalculateAngleScore(const Position& pos, const MovementContext& context)
{
    if (!context.target)
        return 50.0f;

    float score = 50.0f;
    Position targetPos = context.target->GetPosition();
    float targetAngle = context.target->GetOrientation();
    float positionAngle = PositionUtils::CalculateAngleBetween(targetPos, pos);

    switch (context.desiredType)
    {
        case PositionType::MELEE_COMBAT:
        case PositionType::FLANKING:
            {
                float behindAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
                float angleDiff = std::abs(PositionUtils::NormalizeAngle(positionAngle - behindAngle));
                if (angleDiff < M_PI/6)  // Within 30 degrees behind
                    score += 30.0f;
            }
            break;

        case PositionType::TANKING:
            {
                float frontAngle = targetAngle;
                float angleDiff = std::abs(PositionUtils::NormalizeAngle(positionAngle - frontAngle));
                if (angleDiff < M_PI/6)  // Within 30 degrees in front
                    score += 30.0f;
            }
            break;

        default:
            score = 50.0f;
            break;
    }
    return score;
}

float PositionManager::CalculateGroupScore(const Position& pos, const MovementContext& context)
{
    if (context.groupMembers.empty())
        return 50.0f;

    Position groupCenter = PositionUtils::CalculateGroupCenter(context.groupMembers);
    float distanceToGroup = pos.GetExactDist(&groupCenter);

    float optimalDistance = GetOptimalGroupDistance(context.botRole);

    if (distanceToGroup <= optimalDistance * 1.2f && distanceToGroup >= optimalDistance * 0.8f)
        return 100.0f;

    float deviation = std::abs(distanceToGroup - optimalDistance) / optimalDistance;
    return std::max(0.0f, 100.0f - (deviation * 50.0f));
}

float PositionManager::GetOptimalGroupDistance(ThreatRole role)
{
    switch (role)
    {
        case ThreatRole::TANK:
            return 8.0f;
        case ThreatRole::HEALER:
            return 25.0f;
        case ThreatRole::DPS:
            return 15.0f;
        case ThreatRole::SUPPORT:
            return 20.0f;
        default:
            return 15.0f;
    }
}

std::vector<Position> PositionManager::GenerateCircularPositions(const Position& center, float radius, uint32 count)
{
    std::vector<Position> positions;
    positions.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        float angle = (2.0f * M_PI * i) / count;
        Position pos = PositionUtils::CalculatePositionAtAngle(center, radius, angle);
        positions.push_back(pos);
    }
    return positions;
}

std::vector<Position> PositionManager::GenerateArcPositions(const Position& center, float radius, float startAngle, float endAngle, uint32 count)
{
    std::vector<Position> positions;
    positions.reserve(count);

    float angleStep = (endAngle - startAngle) / (count - 1);

    for (uint32 i = 0; i < count; ++i)
    {
        float angle = startAngle + (angleStep * i);
        Position pos = PositionUtils::CalculatePositionAtAngle(center, radius, angle);
        positions.push_back(pos);
    }

    return positions;
}

float PositionManager::EstimateMovementTime(const Position& from, const Position& to)
{
    float distance = from.GetExactDist(&to);
    float baseSpeed = _bot->GetSpeed(MOVE_RUN);

    return distance / baseSpeed;
}

bool PositionManager::CanReachPosition(const Position& pos)
{
    return ValidatePosition(pos, PositionValidation::WALKABLE | PositionValidation::NO_OBSTACLES);
}

std::vector<Position> PositionManager::CalculateWaypoints(const Position& from, const Position& to)
{
    std::vector<Position> waypoints;
    waypoints.push_back(from);
    waypoints.push_back(to);

    return waypoints;
}

void PositionManager::TrackPerformance(std::chrono::microseconds duration, const std::string& /* operation */)
{
    if (duration > _metrics.maxEvaluationTime)
        _metrics.maxEvaluationTime = duration;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        uint32 totalEvaluations = _metrics.positionEvaluations.load();
        if (totalEvaluations > 0)
        {
            _metrics.averageEvaluationTime = std::chrono::microseconds(
                static_cast<uint64_t>(_metrics.averageEvaluationTime.count() * 0.9 + duration.count() * 0.1)
            );
        }
        _metrics.lastUpdate = currentTime;
    }
}

float PositionManager::CalculateEscapeScore(const Position& pos, const MovementContext& context)
{
    if (!_bot)
        return 0.0f;

    float score = 0.0f;

    // Base score for distance from current position
    float currentDistance = _bot->GetPosition().GetExactDist(&pos);
    score += std::min(currentDistance * 10.0f, 50.0f); // Max 50 points for distance

    // Lock-free spatial grid query for nearby enemies
    Map* map = _bot->GetMap();
    if (map)
    {
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            // Create grid on demand
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
        }

        if (spatialGrid)
        {
            // PHASE 5B: Thread-safe spatial grid query (replaces QueryNearbyCreatureGuids + ObjectAccessor)
            auto hostileSnapshots = SpatialGridQueryHelpers::FindHostileCreaturesInRange(_bot, 30.0f, true);

            // Check distance from nearby enemies
            float minEnemyDistance = 1000.0f;

            // Use snapshots for position calculations (lock-free!)
            for (auto const* snapshot : hostileSnapshots)
            {
                if (!snapshot)
                    continue;

                // Validate with Unit* for IsHostileTo check
                /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ Unit* enemy = ObjectAccessor::GetUnit(*_bot, snapshot->guid);
                if (!enemy || !_bot->IsHostileTo(enemy))
                    continue;

                float enemyDistance = pos.GetExactDist(snapshot->position);
                minEnemyDistance = std::min(minEnemyDistance, enemyDistance);

                // Higher score for positions farther from enemies
                if (enemyDistance > 0.0f)
                    score += std::min(enemyDistance * 5.0f, 30.0f); // Max 30 points per enemy
            }

            // Bonus for getting to safe range
            if (minEnemyDistance > 15.0f)
                score += 20.0f;
        }
    }

    // Check for line of sight to group members (we want to maintain LOS)
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && member != _bot && member->IsInWorld())
            {
                if (_bot->IsWithinLOSInMap(member))
                    score += 5.0f;
            }
        }
    }

    // Penalty for positions in AoE zones
    // TODO: Implement IsInAoEZone function
    // if (IsInAoEZone(pos))
    //     score -= 40.0f;

    // Penalty for positions too close to walls/obstacles
    MovementContext tempContext;
    if (!IsPositionSafe(pos, tempContext))
        score -= 20.0f;

    return std::max(score, 0.0f);
}

// PositionUtils implementation
Position PositionUtils::CalculatePositionAtAngle(const Position& center, float distance, float angle)
{
    Position result;
    result.m_positionX = center.GetPositionX() + distance * std::cos(angle);
    result.m_positionY = center.GetPositionY() + distance * std::sin(angle);
    result.m_positionZ = center.GetPositionZ();
    result.SetOrientation(angle);

    return result;
}

float PositionUtils::CalculateAngleBetween(const Position& from, const Position& to)
{
    float dx = to.GetPositionX() - from.GetPositionX();
    float dy = to.GetPositionY() - from.GetPositionY();

    return std::atan2(dy, dx);
}

float PositionUtils::NormalizeAngle(float angle)
{
    while (angle > M_PI)
        angle -= 2.0f * M_PI;
    while (angle < -M_PI)
        angle += 2.0f * M_PI;

    return angle;
}

bool PositionUtils::IsInMeleeRange(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    return bot->GetExactDistSq(target) <= (5.0f * 5.0f); // 25.0f
}

bool PositionUtils::IsInOptimalRange(Player* bot, Unit* target, PositionType type)
{
    if (!bot || !target)
        return false;

    float distSq = bot->GetExactDistSq(target);

    switch (type)
    {
        case PositionType::MELEE_COMBAT:
            return distSq <= (5.0f * 5.0f); // 25.0f
        case PositionType::RANGED_DPS:
            return distSq >= (20.0f * 20.0f) && distSq <= (40.0f * 40.0f); // 400.0f - 1600.0f
        case PositionType::HEALING:
            return distSq <= (40.0f * 40.0f); // 1600.0f
        case PositionType::KITING:
            return distSq >= (15.0f * 15.0f); // 225.0f
        default:
            return true;
    }
}

Position PositionUtils::CalculateGroupCenter(const std::vector<Player*>& players)
{
    if (players.empty())
        return Position();

    float totalX = 0.0f, totalY = 0.0f, totalZ = 0.0f;

    for (Player* player : players)
    {
        if (player)
        {
            totalX += player->GetPositionX();
            totalY += player->GetPositionY();
            totalZ += player->GetPositionZ();
        }
    }

    Position center;
    center.m_positionX = totalX / players.size();
    center.m_positionY = totalY / players.size();
    center.m_positionZ = totalZ / players.size();

    return center;
}

bool PositionUtils::IsPositionOnGround(const Position& pos, Map* map)
{
    if (!map)
        return false;

    float groundZ = map->GetHeight(PhasingHandler::GetEmptyPhaseShift(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
    return std::abs(pos.GetPositionZ() - groundZ) <= 2.0f;
}

Position PositionManager::PredictTargetPosition(Unit* target, float timeAhead)
{
    if (!target)
        return Position();

    // Get current position and movement info
    Position currentPos = target->GetPosition();

    // Calculate predicted position based on movement speed
    // Simple prediction: if target has movement speed, predict forward movement

    // Calculate predicted position based on current velocity
    float speed = target->GetSpeed(MOVE_RUN); // Get movement speed
    if (speed <= 0.0f)
        return currentPos;

    // Get current facing direction
    float orientation = target->GetOrientation();

    // Calculate movement distance
    float distance = speed * timeAhead;

    // Project position forward based on current facing direction
    Position predictedPos;
    predictedPos.m_positionX = currentPos.GetPositionX() + distance * std::cos(orientation);
    predictedPos.m_positionY = currentPos.GetPositionY() + distance * std::sin(orientation);
    predictedPos.m_positionZ = currentPos.GetPositionZ();
    predictedPos.SetOrientation(orientation);

    // Validate the predicted position (check if walkable)
    if (_bot && _bot->GetMap())
    {
        float groundZ = _bot->GetMap()->GetHeight(PhasingHandler::GetEmptyPhaseShift(),
                                                 predictedPos.GetPositionX(),
                                                 predictedPos.GetPositionY(),
                                                 predictedPos.GetPositionZ());

        // If ground height is reasonable, use it
        if (std::abs(groundZ - predictedPos.GetPositionZ()) <= 10.0f)
            predictedPos.m_positionZ = groundZ;
    }

    return predictedPos;
}

bool PositionUtils::CanWalkStraightLine(const Position& from, const Position& to, Map* map)
{
    if (!map)
        return false;

    // Use TrinityCore API properly with PhaseShift
    // We need a PhaseShift - for now, create a default one or get from a WorldObject
    PhaseShift phaseShift;
    return map->isInLineOfSight(phaseShift, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                               to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(),
                               LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing);
}

} // namespace Playerbot