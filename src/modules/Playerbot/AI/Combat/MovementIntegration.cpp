/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "MovementIntegration.h"
#include "Player.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "Log.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

MovementIntegration::MovementIntegration(Player* bot)
    : _bot(bot)
    , _lastUpdate(0)
    , _currentSituation(static_cast<CombatSituation>(0))
{
}

void MovementIntegration::Update(uint32 diff, CombatSituation situation)
{
    if (!_bot)
        return;

    _lastUpdate += diff;
    _currentSituation = situation;

    if (_lastUpdate < UPDATE_INTERVAL)
        return;

    _lastUpdate = 0;

    // Update danger zones (remove expired)
    UpdateDangerZones();

    // Evaluate positioning needs (priority order)
    MovementCommand command;

    // 1. Check danger (EMERGENCY)
    command = CheckDanger();
    if (command.urgency == MovementUrgency::EMERGENCY)
    {
        _currentCommand = command;
        return;
    }

    // 2. Check line of sight (HIGH)
    command = CheckLineOfSight();
    if (command.urgency == MovementUrgency::HIGH)
    {
        _currentCommand = command;
        return;
    }

    // 3. Check range (HIGH)
    command = CheckRange();
    if (command.urgency == MovementUrgency::HIGH)
    {
        _currentCommand = command;
        return;
    }

    // 4. Evaluate general positioning (MEDIUM/LOW)
    command = EvaluatePositioning();
    _currentCommand = command;
}

void MovementIntegration::Reset()
{
    _dangerZones.clear();
    _currentCommand = MovementCommand();
    _lastUpdate = 0;
}

bool MovementIntegration::NeedsMovement()
{
    return _currentCommand.IsValid();
}

bool MovementIntegration::NeedsUrgentMovement()
{
    return _currentCommand.IsValid() && _currentCommand.urgency == MovementUrgency::HIGH;
}

bool MovementIntegration::NeedsEmergencyMovement()
{
    return _currentCommand.IsValid() && _currentCommand.urgency == MovementUrgency::EMERGENCY;
}

bool MovementIntegration::NeedsRepositioning()
{
    return _currentCommand.IsValid() &&
           (_currentCommand.urgency == MovementUrgency::MEDIUM ||
            _currentCommand.urgency == MovementUrgency::LOW);
}

bool MovementIntegration::ShouldMoveToPosition(const Position& pos)
{
    if (!_bot)
        return false;

    // Check if position is better than current
    float currentDanger = GetDangerLevel(*_bot);
    float newDanger = GetDangerLevel(pos);

    // New position must be safer
    return newDanger < currentDanger;
}

bool MovementIntegration::IsPositionSafe(const Position& pos)
{
    return GetDangerLevel(pos) == 0.0f;
}

Position MovementIntegration::GetOptimalPosition()
{
    if (!_bot)
        return Position();

    // If in danger, find nearest safe position
    if (GetDangerLevel(*_bot) > 0.0f)
        return FindNearestSafePosition(*_bot, 5.0f);

    // Otherwise, use role-based positioning
    return CalculateRolePosition();
}

Position MovementIntegration::GetTargetPosition()
{
    if (_currentCommand.IsValid())
        return _currentCommand.destination;

    return Position();
}

float MovementIntegration::GetOptimalRange(Unit* target)
{
    if (!_bot || !target)
        return MELEE_RANGE;

    CombatRole role = GetCombatRole();

    switch (role)
    {
        case CombatRole::TANK:
        case CombatRole::MELEE_DPS:
            return MELEE_RANGE;

        case CombatRole::HEALER:
            return 30.0f;  // Healing range

        case CombatRole::RANGED_DPS:
            return RANGED_OPTIMAL;

        default:
            return MELEE_RANGE;
    }
}

void MovementIntegration::MoveToPosition(const Position& pos, bool urgent)
{
    if (!_bot)
        return;

    // Check if path is safe
    if (!IsPathSafe(*_bot, pos))
    {
        TC_LOG_DEBUG("playerbot", "MovementIntegration: Unsafe path from ({}, {}) to ({}, {})",
            _bot->GetPositionX(), _bot->GetPositionY(),
            pos.GetPositionX(), pos.GetPositionY());
        return;
    }

    // Execute movement
    TC_LOG_DEBUG("playerbot", "MovementIntegration: {} moving to ({}, {}) - Urgent: {}",
        _bot->GetName(), pos.GetPositionX(), pos.GetPositionY(), urgent ? "YES" : "NO");

    // TODO: Integrate with bot movement system
    // This is a placeholder that should be replaced with actual movement command
    // _bot->GetMotionMaster()->MovePoint(0, pos, urgent);
}

void MovementIntegration::RegisterDangerZone(const Position& center, float radius, uint32 duration, float dangerLevel)
{
    DangerZone zone;
    zone.center = center;
    zone.radius = radius;
    zone.expiryTime = GameTime::GetGameTimeMS() + duration;
    zone.dangerLevel = dangerLevel;

    _dangerZones.push_back(zone);

    TC_LOG_DEBUG("playerbot", "MovementIntegration: Registered danger zone at ({}, {}) radius {} danger {}",
        center.GetPositionX(), center.GetPositionY(), radius, dangerLevel);
}

void MovementIntegration::ClearDangerZones()
{
    _dangerZones.clear();
}

std::vector<DangerZone> MovementIntegration::GetDangerZones() const
{
    std::vector<DangerZone> active;

    for (const auto& zone : _dangerZones)
    {
        if (!zone.IsExpired())
            active.push_back(zone);
    }

    return active;
}

float MovementIntegration::GetDangerLevel(const Position& pos) const
{
    float maxDanger = 0.0f;

    for (const auto& zone : _dangerZones)
    {
        if (zone.IsExpired())
            continue;

        if (zone.IsInDanger(pos))
            maxDanger = std::max(maxDanger, zone.dangerLevel);
    }

    return maxDanger;
}

Position MovementIntegration::FindNearestSafePosition(const Position& from, float minDistance)
{
    // Try 8 directions
    const float angles[] = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f };
    const float distances[] = { 5.0f, 10.0f, 15.0f, 20.0f };

    for (float distance : distances)
    {
        for (float angle : angles)
        {
            float radians = angle * M_PI / 180.0f;
            float x = from.GetPositionX() + distance * std::cos(radians);
            float y = from.GetPositionY() + distance * std::sin(radians);

            Position test(x, y, from.GetPositionZ());

            if (IsPositionSafe(test))
                return test;
        }
    }

    // No safe position found, return original
    return from;
}

bool MovementIntegration::ShouldKite(Unit* target)
{
    if (!_bot || !target)
        return false;

    // Only ranged classes should kite
    CombatRole role = GetCombatRole();
    if (role != CombatRole::RANGED_DPS && role != CombatRole::HEALER)
        return false;

    // Check if target is melee
    if (target->GetPowerType() == POWER_MANA)
        return false;  // Likely a caster

    // Check distance
    float distance = _bot->GetDistance(target);

    // Kite if target is too close
    return distance < KITING_DISTANCE;
}

Position MovementIntegration::GetKitingPosition(Unit* target)
{
    if (!_bot || !target)
        return Position();

    // Move away from target
    float angle = target->GetAngle(_bot);
    float distance = KITING_DISTANCE + 5.0f;  // Kite to safe distance

    float x = target->GetPositionX() + distance * std::cos(angle);
    float y = target->GetPositionY() + distance * std::sin(angle);

    return Position(x, y, _bot->GetPositionZ());
}

// Private helper functions

void MovementIntegration::UpdateDangerZones()
{
    _dangerZones.erase(
        std::remove_if(_dangerZones.begin(), _dangerZones.end(),
            [](const DangerZone& zone) { return zone.IsExpired(); }),
        _dangerZones.end()
    );
}

MovementCommand MovementIntegration::EvaluatePositioning()
{
    MovementCommand command;

    if (!_bot)
        return command;

    // Check if current position is optimal
    Position optimal = CalculateRolePosition();
    float distance = _bot->GetDistance(optimal);

    // If too far from optimal position
    if (distance > 5.0f)
    {
        command.destination = optimal;
        command.urgency = MovementUrgency::MEDIUM;
        command.reason = MovementReason::OPTIMIZE_POSITION;
        command.acceptableRadius = 2.0f;
        command.expiryTime = GameTime::GetGameTimeMS() + 5000;  // 5 second validity
    }

    return command;
}

MovementCommand MovementIntegration::CheckRange()
{
    MovementCommand command;

    if (!_bot)
        return command;

    Unit* target = _bot->GetVictim();
    if (!target)
        return command;

    float optimalRange = GetOptimalRange(target);
    float currentDistance = _bot->GetDistance(target);

    // Out of range?
    if (currentDistance > optimalRange + 5.0f)
    {
        // Move closer
        float angle = _bot->GetAngle(target);
        float moveDistance = currentDistance - optimalRange;

        float x = _bot->GetPositionX() + moveDistance * std::cos(angle);
        float y = _bot->GetPositionY() + moveDistance * std::sin(angle);

        command.destination = Position(x, y, _bot->GetPositionZ());
        command.urgency = MovementUrgency::HIGH;
        command.reason = MovementReason::MAINTAIN_RANGE;
        command.acceptableRadius = 2.0f;
        command.expiryTime = GameTime::GetGameTimeMS() + 3000;
    }
    // Too close for ranged?
    else if (GetCombatRole() == CombatRole::RANGED_DPS && currentDistance < optimalRange - 10.0f)
    {
        // Move back
        command.destination = GetKitingPosition(target);
        command.urgency = MovementUrgency::HIGH;
        command.reason = MovementReason::KITING;
        command.acceptableRadius = 2.0f;
        command.expiryTime = GameTime::GetGameTimeMS() + 3000;
    }

    return command;
}

MovementCommand MovementIntegration::CheckDanger()
{
    MovementCommand command;

    if (!_bot)
        return command;

    // Check if in danger
    float danger = GetDangerLevel(*_bot);

    if (danger > 0.0f)
    {
        // Find nearest safe position
        Position safe = FindNearestSafePosition(*_bot, 5.0f);

        command.destination = safe;
        command.urgency = MovementUrgency::EMERGENCY;
        command.reason = MovementReason::AVOID_DANGER;
        command.acceptableRadius = 1.0f;
        command.expiryTime = GameTime::GetGameTimeMS() + 2000;  // 2 second validity

        TC_LOG_DEBUG("playerbot", "MovementIntegration: {} in danger (level {}), moving to safety",
            _bot->GetName(), danger);
    }

    return command;
}

MovementCommand MovementIntegration::CheckLineOfSight()
{
    MovementCommand command;

    if (!_bot)
        return command;

    Unit* target = _bot->GetVictim();
    if (!target)
        return command;

    // Check LoS
    if (!_bot->IsWithinLOSInMap(target))
    {
        // Need to move to get LoS
        // Simple: move towards target
        float angle = _bot->GetAngle(target);
        float distance = 10.0f;

        float x = _bot->GetPositionX() + distance * std::cos(angle);
        float y = _bot->GetPositionY() + distance * std::sin(angle);

        command.destination = Position(x, y, _bot->GetPositionZ());
        command.urgency = MovementUrgency::HIGH;
        command.reason = MovementReason::LINE_OF_SIGHT;
        command.acceptableRadius = 2.0f;
        command.expiryTime = GameTime::GetGameTimeMS() + 3000;
    }

    return command;
}

Position MovementIntegration::CalculateRolePosition()
{
    if (!_bot)
        return Position();

    Unit* target = _bot->GetVictim();
    if (!target)
        return *_bot;  // No target, stay in place

    CombatRole role = GetCombatRole();
    float optimalRange = GetOptimalRange(target);

    // Calculate position at optimal range
    float angle = target->GetAngle(_bot);
    float x = target->GetPositionX() + optimalRange * std::cos(angle);
    float y = target->GetPositionY() + optimalRange * std::sin(angle);

    return Position(x, y, _bot->GetPositionZ());
}

MovementIntegration::CombatRole MovementIntegration::GetCombatRole() const
{
    if (!_bot)
        return CombatRole::MELEE_DPS;

    // Simple heuristic based on class
    // TODO: Proper role detection from spec/talents
    switch (_bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return CombatRole::TANK;

        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            return CombatRole::HEALER;

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return CombatRole::RANGED_DPS;

        case CLASS_ROGUE:
        case CLASS_DRUID:
        case CLASS_MONK:
            return CombatRole::MELEE_DPS;

        default:
            return CombatRole::MELEE_DPS;
    }
}

bool MovementIntegration::IsPathSafe(const Position& from, const Position& to) const
{
    // Simple check: sample points along path
    const int samples = 5;
    for (int i = 0; i <= samples; ++i)
    {
        float t = static_cast<float>(i) / samples;
        float x = from.GetPositionX() + t * (to.GetPositionX() - from.GetPositionX());
        float y = from.GetPositionY() + t * (to.GetPositionY() - from.GetPositionY());
        float z = from.GetPositionZ() + t * (to.GetPositionZ() - from.GetPositionZ());

        Position sample(x, y, z);

        if (GetDangerLevel(sample) > 0.0f)
            return false;
    }

    return true;
}

float MovementIntegration::GetMovementSpeed() const
{
    if (!_bot)
        return 7.0f;  // Default run speed

    return _bot->GetSpeed(MOVE_RUN);
}

} // namespace Playerbot
