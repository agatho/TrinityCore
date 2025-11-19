/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FormationManager.h"
#include "Player.h"
#include "Group.h"
#include "Map.h"
#include "Log.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "../../Movement/UnifiedMovementCoordinator.h  // Phase 2: Unified movement system"
#include "../../Movement/Arbiter/MovementPriorityMapper.h"
#include "../BotAI.h"
#include "UnitAI.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

FormationManager::FormationManager(Player* bot)
    : _bot(bot), _leader(nullptr), _isLeader(false), _inFormation(false),
      _currentFormation(FormationType::NONE), _movementState(MovementState::STATIONARY),
      _currentIntegrity(FormationIntegrity::PERFECT), _formationOrientation(0.0f),
      _isMovingToDestination(false), _updateInterval(DEFAULT_UPDATE_INTERVAL),
      _cohesionRadius(DEFAULT_COHESION_RADIUS), _formationSpacing(DEFAULT_FORMATION_SPACING),
      _reformationThreshold(DEFAULT_REFORMATION_THRESHOLD), _formationPriority(0),
      _adaptiveFormations(true), _emergencyScatter(false), _lastUpdate(0),
      _lastIntegrityCheck(0), _lastReformation(0)
{

    InitializeFormationConfigs();
    TC_LOG_DEBUG("playerbot.formation", "FormationManager initialized for bot {}", _bot->GetName());
}

bool FormationManager::JoinFormation(const std::vector<Player*>& groupMembers, FormationType formation)
{
    // No lock needed - _members is per-bot instance data
    try
    {
        if (_inFormation)
        {
            TC_LOG_DEBUG("playerbot.formation", "Bot {} already in formation", _bot->GetName());
            return false;
        }

        _members.clear();
        _members.reserve(groupMembers.size());

        Player* groupLeader = nullptr;
        if (Group* group = _bot->GetGroup())
        {
            groupLeader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
        }

        if (!groupLeader && !groupMembers.empty())
        {
            groupLeader = groupMembers[0];
        }

        _leader = groupLeader;
        _isLeader = (_leader == _bot);
        for (Player* member : groupMembers)
        {
            if (!member || !member->IsInWorld())
                continue;

            FormationMember formationMember;
            formationMember.guid = member->GetGUID();
            formationMember.player = member;
            formationMember.role = DeterminePlayerRole(member);
            formationMember.currentPosition = member->GetPosition();
            formationMember.name = member->GetName();
            formationMember.lastPositionUpdate = GameTime::GetGameTimeMS();
            formationMember.movementSpeed = member->GetSpeed(MOVE_RUN);
            formationMember.formationSlot = static_cast<uint32>(_members.size());

            _members.push_back(formationMember);
        }

        _currentFormation = formation;
        _inFormation = true;

        if (_isLeader)
        {
            _formationCenter = _bot->GetPosition();
            _formationOrientation = _bot->GetOrientation();
        }
        else
        {
            _formationCenter = FormationUtils::CalculateFormationCenterFromMembers(groupMembers);
            _formationOrientation = _leader ? _leader->GetOrientation() : 0.0f;
        }
        AssignFormationPositions();
        _metrics.formationChanges++;

        TC_LOG_DEBUG("playerbot.formation", "Bot {} joined formation {} with {} members",
                   _bot->GetName(), static_cast<uint32>(formation), _members.size());

        return true;
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.formation", "Exception in JoinFormation for bot {}: {}", _bot->GetName(), e.what());
        return false;
    }
}

bool FormationManager::LeaveFormation()
{
    // No lock needed - _inFormation and _members are per-bot instance data
    if (!_inFormation)
        return false;

    _inFormation = false;
    _currentFormation = FormationType::NONE;
    _movementState = MovementState::STATIONARY;
    _members.clear();
    _leader = nullptr;
    _isLeader = false;
    TC_LOG_DEBUG("playerbot.formation", "Bot {} left formation", _bot->GetName());
    return true;
}

bool FormationManager::ChangeFormation(FormationType newFormation)
{
    // No lock needed - _currentFormation is per-bot instance data
    if (!_inFormation || newFormation == _currentFormation)
        return false;

    if (!_isLeader)
    {
        TC_LOG_DEBUG("playerbot.formation", "Bot {} cannot change formation - not leader", _bot->GetName());
        return false;
    }

    auto startTime = std::chrono::steady_clock::now();

    _currentFormation = newFormation;
    _movementState = MovementState::REFORMING;

    AssignFormationPositions();
    _metrics.formationChanges++;

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "ChangeFormation");

    TC_LOG_DEBUG("playerbot.formation", "Bot {} changed formation to {}",
               _bot->GetName(), static_cast<uint32>(newFormation));

    return true;
}

void FormationManager::UpdateFormation(uint32 diff)
{
    // No lock needed - all formation data is per-bot instance
    if (!_inFormation)
        return;

    uint32 currentTime = GameTime::GetGameTimeMS();
    if (currentTime - _lastUpdate < _updateInterval)
        return;

    _lastUpdate = currentTime;

    try
    {
        UpdateMemberPositions();

        if (currentTime - _lastIntegrityCheck >= INTEGRITY_CHECK_INTERVAL)
        {
            MonitorFormationIntegrity();
            _lastIntegrityCheck = currentTime;
        }
        if (_movementState == MovementState::MOVING)
        {
            MaintainFormationDuringMovement();
        }
        if (RequiresReformation() && currentTime - _lastReformation >= MIN_REFORMATION_INTERVAL)
        {
            TriggerReformationIfNeeded();
            _lastReformation = currentTime;
        }

        if (_adaptiveFormations)
        {
            AdjustForTerrain();
            AdjustForGroupSize();
        }
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.formation", "Exception in UpdateFormation for bot {}: {}", _bot->GetName(), e.what());
    }
}

bool FormationManager::ExecuteFormationCommand(const FormationCommand& command)
{
    // No lock needed - command execution modifies per-bot instance data
    if (!_inFormation)
        return false;

    try
    {
        auto startTime = std::chrono::steady_clock::now();

        if (command.newFormation != FormationType::NONE && command.newFormation != _currentFormation)
        {
            _currentFormation = command.newFormation;
            AssignFormationPositions();
        }

        _movementState = command.movementState;

        if (!command.targetPosition.IsEmpty())
        {
            _targetDestination = command.targetPosition;
            _isMovingToDestination = true;
            _formationOrientation = command.targetOrientation;
        }

        CalculateMovementTargets();
        IssueMovementCommands();

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        TrackPerformance(duration, "ExecuteFormationCommand");
        TC_LOG_DEBUG("playerbot.formation", "Bot {} executed formation command", _bot->GetName());
        return true;
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.formation", "Exception executing formation command for bot {}: {}", _bot->GetName(), e.what());
        return false;
    }
}

bool FormationManager::MoveFormationToPosition(const Position& targetPos, float orientation)
{
    if (!_inFormation || !_isLeader)
        return false;
    FormationCommand command;
    command.targetPosition = targetPos;
    command.targetOrientation = orientation != 0.0f ? orientation : _formationOrientation;
    command.movementState = MovementState::MOVING;
    command.maintainCohesion = true;
    command.reason = "Formation movement";
    return ExecuteFormationCommand(command);
}

Position FormationManager::CalculateFormationPosition(FormationRole role, uint32 memberIndex)
{
    Position leaderPos = _leader ? _leader->GetPosition() : _formationCenter;

    switch (_currentFormation)
    {
        case FormationType::LINE:
            return CalculateLineFormation(leaderPos, _formationOrientation)[memberIndex % CalculateLineFormation(leaderPos, _formationOrientation).size()];

        case FormationType::COLUMN:
            return CalculateColumnFormation(leaderPos, _formationOrientation)[memberIndex % CalculateColumnFormation(leaderPos, _formationOrientation).size()];

        case FormationType::WEDGE:
            return CalculateWedgeFormation(leaderPos, _formationOrientation)[memberIndex % CalculateWedgeFormation(leaderPos, _formationOrientation).size()];

        case FormationType::DIAMOND:
            return CalculateDiamondFormation(leaderPos, _formationOrientation)[memberIndex % CalculateDiamondFormation(leaderPos, _formationOrientation).size()];

        case FormationType::CIRCLE:
            return CalculateCircleFormation(leaderPos)[memberIndex % CalculateCircleFormation(leaderPos).size()];

        case FormationType::BOX:
            return CalculateBoxFormation(leaderPos, _formationOrientation)[memberIndex % CalculateBoxFormation(leaderPos, _formationOrientation).size()];

        case FormationType::DUNGEON:
            return CalculateDungeonFormation(leaderPos, _formationOrientation)[memberIndex % CalculateDungeonFormation(leaderPos, _formationOrientation).size()];

        case FormationType::RAID:
            return CalculateRaidFormation(leaderPos, _formationOrientation)[memberIndex % CalculateRaidFormation(leaderPos, _formationOrientation).size()];

        default:
            return CalculateRoleBasedPosition(role, leaderPos, _formationOrientation);
    }
}

std::vector<Position> FormationManager::CalculateAllFormationPositions()
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    for (size_t i = 0; i < _members.size(); ++i)
    {
        Position pos = CalculateFormationPosition(_members[i].role, static_cast<uint32>(i));
        positions.push_back(pos);
    }

    return positions;
}

Position FormationManager::GetAssignedPosition() const
{
    // No lock needed - reading per-bot instance data (_members)
    for (const FormationMember& member : _members)
    {
        if (member.player == _bot)
        {
            return member.assignedPosition;
        }
    }

    return _bot->GetPosition();
}

bool FormationManager::IsInFormationPosition(float tolerance) const
{
    // No lock needed - GetAssignedPosition() reads per-bot instance data
    Position assignedPos = GetAssignedPosition();
    Position currentPos = _bot->GetPosition();

    return currentPos.GetExactDist(&assignedPos) <= tolerance;
}

FormationIntegrity FormationManager::AssessFormationIntegrity()
{
    // No lock needed - assessing per-bot instance data (_members, _formationSpacing)
    if (_members.empty())
        return FormationIntegrity::BROKEN;

    uint32 inPosition = 0;
    uint32 closeToPosition = 0;
    float totalDeviation = 0.0f;

    for (const FormationMember& member : _members)
    {
        if (member.isInPosition)
        {
            inPosition++;
        }
        else if (member.distanceFromAssigned <= _formationSpacing * 1.5f)
        {
            closeToPosition++;
        }

        totalDeviation += member.distanceFromAssigned;
    }

    float averageDeviation = totalDeviation / _members.size();
    float inPositionRatio = static_cast<float>(inPosition) / _members.size();
    float closePositionRatio = static_cast<float>(inPosition + closeToPosition) / _members.size();

    if (inPositionRatio >= 0.9f && averageDeviation <= _formationSpacing * 0.5f)
        return FormationIntegrity::PERFECT;
    else if (inPositionRatio >= 0.7f && averageDeviation <= _formationSpacing)
        return FormationIntegrity::GOOD;
    else if (closePositionRatio >= 0.6f && averageDeviation <= _formationSpacing * 1.5f)
        return FormationIntegrity::ACCEPTABLE;
    else if (closePositionRatio >= 0.3f)
        return FormationIntegrity::POOR;
    else
        return FormationIntegrity::BROKEN;
}

void FormationManager::CoordinateMovement(const Position& destination)
{
    if (!_inFormation)
        return;

    if (_isLeader)
    {
        MoveFormationToPosition(destination);
    }
    else
    {
        Position myAssignedPos = GetAssignedPosition();
        float spacingSq = _formationSpacing * _formationSpacing;
        if (_bot->GetExactDistSq(myAssignedPos) > spacingSq)
        {
            // PHASE 6B: Use Movement Arbiter with FORMATION priority (160)
            BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
            if (botAI && botAI->GetUnifiedMovementCoordinator())
            {
                botAI->RequestPointMovement(
                    PlayerBotMovementPriority::FORMATION,
                    myAssignedPos,
                    "Formation position maintenance",
                    "FormationManager");
            }
            else
            {
                // FALLBACK: Direct MotionMaster if arbiter not available
                _bot->GetMotionMaster()->MovePoint(0, myAssignedPos.GetPositionX(), myAssignedPos.GetPositionY(), myAssignedPos.GetPositionZ());
            }
        }
    }
}

bool FormationManager::CanMoveWithoutBreakingFormation(const Position& newPos)
{
    // No lock needed - checking per-bot instance data (_formationCenter, _cohesionRadius)
    if (!_inFormation)
        return true;

    Position currentCenter = _formationCenter;
    float currentCohesion = CalculateCohesionLevel();

    float distanceFromCenter = newPos.GetExactDist(&currentCenter);

    return distanceFromCenter <= _cohesionRadius && currentCohesion >= 0.6f;
}

Position FormationManager::AdjustMovementForFormation(const Position& intendedPos)
{
    // No lock needed - adjusting based on per-bot instance data
    if (!_inFormation)
        return intendedPos;

    Position assignedPos = GetAssignedPosition();
    Position currentPos = _bot->GetPosition();

    float distanceToAssigned = currentPos.GetExactDist(&assignedPos);
    float distanceToIntended = currentPos.GetExactDist(&intendedPos);

    if (distanceToAssigned > _formationSpacing * 1.5f)
    {
        float blendFactor = std::min(1.0f, distanceToAssigned / _cohesionRadius);

        Position adjusted;
        adjusted.m_positionX = intendedPos.GetPositionX() * (1.0f - blendFactor) + assignedPos.GetPositionX() * blendFactor;
        adjusted.m_positionY = intendedPos.GetPositionY() * (1.0f - blendFactor) + assignedPos.GetPositionY() * blendFactor;
        adjusted.m_positionZ = intendedPos.GetPositionZ() * (1.0f - blendFactor) + assignedPos.GetPositionZ() * blendFactor;

        return adjusted;
    }

    return intendedPos;
}

void FormationManager::TransitionToCombatFormation(const std::vector<Unit*>& enemies)
{
    if (!_inFormation || !_isLeader)
        return;

    FormationType combatFormation = FormationUtils::GetOptimalFormationForCombat(
        [this]() {
            std::vector<Player*> players;
            for (const auto& member : _members)
                if (member.player)
                    players.push_back(member.player);
            return players;
        }(),
        enemies
    );

    if (combatFormation != _currentFormation)
    {
        ChangeFormation(combatFormation);
        _movementState = MovementState::COMBAT;
    }
}

std::vector<Position> FormationManager::CalculateLineFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    for (size_t i = 0; i < _members.size(); ++i)
    {
        float offset = (static_cast<float>(i) - static_cast<float>(_members.size() - 1) * 0.5f) * _formationSpacing;

        Position pos;
        pos.m_positionX = leaderPos.GetPositionX() + offset * std::cos(orientation + M_PI/2);
        pos.m_positionY = leaderPos.GetPositionY() + offset * std::sin(orientation + M_PI/2);
        pos.m_positionZ = leaderPos.GetPositionZ();

        positions.push_back(pos);
    }

    return positions;
}

std::vector<Position> FormationManager::CalculateColumnFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    for (size_t i = 0; i < _members.size(); ++i)
    {
        float distance = static_cast<float>(i) * _formationSpacing;

        Position pos;
        pos.m_positionX = leaderPos.GetPositionX() - distance * std::cos(orientation);
        pos.m_positionY = leaderPos.GetPositionY() - distance * std::sin(orientation);
        pos.m_positionZ = leaderPos.GetPositionZ();

        positions.push_back(pos);
    }

    return positions;
}

std::vector<Position> FormationManager::CalculateWedgeFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    positions.push_back(leaderPos);

    for (size_t i = 1; i < _members.size(); ++i)
    {
        float row = std::floor((std::sqrt(8.0f * i + 1.0f) - 1.0f) / 2.0f);
        float posInRow = i - (row * (row + 1.0f) / 2.0f);
        bool isLeft = (static_cast<int>(posInRow) % 2) == 1;

        float angle = orientation + (isLeft ? -M_PI/6 : M_PI/6);
        float distance = (row + 1.0f) * _formationSpacing;

        Position pos;
        pos.m_positionX = leaderPos.GetPositionX() + distance * std::cos(angle);
        pos.m_positionY = leaderPos.GetPositionY() + distance * std::sin(angle);
        pos.m_positionZ = leaderPos.GetPositionZ();

        positions.push_back(pos);
    }

    return positions;
}

std::vector<Position> FormationManager::CalculateCircleFormation(const Position& leaderPos)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    positions.push_back(leaderPos);

    if (_members.size() > 1)
    {
        float radius = _formationSpacing;
        float angleIncrement = 2.0f * M_PI / (_members.size() - 1);

        for (size_t i = 1; i < _members.size(); ++i)
        {
            float angle = (i - 1) * angleIncrement;

            Position pos;
            pos.m_positionX = leaderPos.GetPositionX() + radius * std::cos(angle);
            pos.m_positionY = leaderPos.GetPositionY() + radius * std::sin(angle);
            pos.m_positionZ = leaderPos.GetPositionZ();

            positions.push_back(pos);
        }
    }

    return positions;
}

std::vector<Position> FormationManager::CalculateDungeonFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    std::vector<FormationRole> orderedRoles = {
        FormationRole::TANK,
        FormationRole::MELEE_DPS,
        FormationRole::RANGED_DPS,
        FormationRole::HEALER,
        FormationRole::SUPPORT
    };
    for (size_t i = 0; i < _members.size(); ++i)
    {
        FormationRole role = (i < orderedRoles.size()) ? orderedRoles[i] : FormationRole::SUPPORT;
        Position pos = CalculateRoleBasedPosition(role, leaderPos, orientation);
        positions.push_back(pos);
    }
    return positions;
}

Position FormationManager::CalculateRoleBasedPosition(FormationRole role, const Position& leaderPos, float orientation)
{
    Position pos = leaderPos;
    float distance = _formationSpacing;
    float angle = orientation;

    switch (role)
    {
        case FormationRole::TANK:
            distance = _formationSpacing * 0.5f;
            angle = orientation;
            break;

        case FormationRole::MELEE_DPS:
            distance = _formationSpacing;
            angle = orientation + M_PI/4;
            break;

        case FormationRole::RANGED_DPS:
            distance = _formationSpacing * 1.5f;
            angle = orientation + M_PI;
            break;

        case FormationRole::HEALER:
            distance = _formationSpacing * 1.2f;
            angle = orientation + M_PI + M_PI/3;
            break;

        case FormationRole::SUPPORT:
            distance = _formationSpacing;
            angle = orientation + M_PI/2;
            break;

        default:
            break;
    }

    pos.m_positionX += distance * std::cos(angle);
    pos.m_positionY += distance * std::sin(angle);

    return pos;
}
void FormationManager::AssignFormationPositions()
{
    std::vector<Position> positions = CalculateAllFormationPositions();

    for (size_t i = 0; i < _members.size() && i < positions.size(); ++i)
    {
        _members[i].assignedPosition = positions[i];
        _members[i].targetPosition = positions[i];
    }
}

void FormationManager::UpdateMemberPositions()
{
    uint32 currentTime = GameTime::GetGameTimeMS();

    for (FormationMember& member : _members)
    {
        if (!member.player || !member.player->IsInWorld())
            continue;

        member.currentPosition = member.player->GetPosition();
        member.distanceFromAssigned = member.currentPosition.GetExactDist(&member.assignedPosition);
        member.distanceFromLeader = _leader ? member.currentPosition.GetExactDist(_leader) : 0.0f;
        member.isInPosition = member.distanceFromAssigned <= _formationSpacing * 0.8f;
        member.isMoving = member.player->IsMoving();
        member.lastPositionUpdate = currentTime;
    }
}

void FormationManager::MonitorFormationIntegrity()
{
    _currentIntegrity = AssessFormationIntegrity();

    float integrityValue = 0.0f;
    switch (_currentIntegrity)
    {
        case FormationIntegrity::PERFECT: integrityValue = 100.0f; break;
        case FormationIntegrity::GOOD: integrityValue = 80.0f; break;
        case FormationIntegrity::ACCEPTABLE: integrityValue = 60.0f; break;
        case FormationIntegrity::POOR: integrityValue = 40.0f; break;
        case FormationIntegrity::BROKEN: integrityValue = 20.0f; break;
    }

    _metrics.averageIntegrity = _metrics.averageIntegrity * 0.9f + integrityValue * 0.1f;
    if (integrityValue < _metrics.minIntegrity)
        _metrics.minIntegrity = integrityValue;

    if (_currentIntegrity <= FormationIntegrity::POOR)
    {
        _metrics.cohesionBreaks++;
    }
}

bool FormationManager::RequiresReformation()
{
    return _currentIntegrity <= FormationIntegrity::POOR ||
           CalculateCohesionLevel() < 0.5f;
}

void FormationManager::TriggerReformationIfNeeded()
{
    if (!RequiresReformation())
        return;

    AssignFormationPositions();
    _movementState = MovementState::REFORMING;
    _metrics.reformationEvents++;

    TC_LOG_DEBUG("playerbot.formation", "Formation {} triggered reformation due to poor integrity",
               _bot->GetName());
}

float FormationManager::CalculateCohesionLevel()
{
    if (_members.empty())
        return 0.0f;

    float totalDistance = 0.0f;
    uint32 validMembers = 0;

    for (const FormationMember& member : _members)
    {
        if (member.player && member.player->IsInWorld())
        {
            totalDistance += member.distanceFromAssigned;
            validMembers++;
        }
    }

    if (validMembers == 0)
        return 0.0f;

    float averageDistance = totalDistance / validMembers;
    float cohesionLevel = 1.0f - (averageDistance / _cohesionRadius);

    return std::max(0.0f, std::min(1.0f, cohesionLevel));
}

FormationRole FormationManager::DeterminePlayerRole(Player* player)
{
    if (!player)
        return FormationRole::SUPPORT;

    uint8 playerClass = player->getClass();
    uint32 spec = player->GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID);

    switch (playerClass)
    {
        case CLASS_WARRIOR:
            return (spec == TALENT_SPEC_WARRIOR_PROTECTION) ? FormationRole::TANK : FormationRole::MELEE_DPS;

        case CLASS_PALADIN:
            if (spec == TALENT_SPEC_PALADIN_PROTECTION)
                return FormationRole::TANK;
            else if (spec == TALENT_SPEC_PALADIN_HOLY)
                return FormationRole::HEALER;
            else
                return FormationRole::MELEE_DPS;

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return FormationRole::RANGED_DPS;

        case CLASS_ROGUE:
        case CLASS_DEATH_KNIGHT:
        case CLASS_DEMON_HUNTER:
            return FormationRole::MELEE_DPS;

        case CLASS_PRIEST:
            return (spec == TALENT_SPEC_PRIEST_SHADOW) ? FormationRole::RANGED_DPS : FormationRole::HEALER;

        case CLASS_SHAMAN:
            if (spec == TALENT_SPEC_SHAMAN_RESTORATION)
                return FormationRole::HEALER;
            else if (spec == TALENT_SPEC_SHAMAN_ELEMENTAL)
                return FormationRole::RANGED_DPS;
            else
                return FormationRole::MELEE_DPS;

        case CLASS_DRUID:
            if (spec == TALENT_SPEC_DRUID_BEAR)
                return FormationRole::TANK;
            else if (spec == TALENT_SPEC_DRUID_RESTORATION)
                return FormationRole::HEALER;
            else if (spec == TALENT_SPEC_DRUID_BALANCE)
                return FormationRole::RANGED_DPS;
            else
                return FormationRole::MELEE_DPS;

        case CLASS_MONK:
            if (spec == TALENT_SPEC_MONK_BREWMASTER)
                return FormationRole::TANK;
            else if (spec == TALENT_SPEC_MONK_MISTWEAVER)
                return FormationRole::HEALER;
            else
                return FormationRole::MELEE_DPS;

        case CLASS_EVOKER:
            return (spec == TALENT_SPEC_EVOKER_PRESERVATION) ? FormationRole::HEALER : FormationRole::RANGED_DPS;

        default:
            return FormationRole::SUPPORT;
    }
}

void FormationManager::InitializeFormationConfigs()
{
    FormationConfig dungeonConfig;
    dungeonConfig.type = FormationType::DUNGEON;
    dungeonConfig.baseSpacing = 4.0f;
    dungeonConfig.cohesionRadius = 12.0f;
    dungeonConfig.combatFormation = true;
    dungeonConfig.roleOrder = { FormationRole::TANK, FormationRole::MELEE_DPS, FormationRole::RANGED_DPS, FormationRole::HEALER };
    _formationConfigs[FormationType::DUNGEON] = dungeonConfig;

    FormationConfig lineConfig;
    lineConfig.type = FormationType::LINE;
    lineConfig.baseSpacing = 3.0f;
    lineConfig.cohesionRadius = 10.0f;
    lineConfig.maintainOrientation = true;
    _formationConfigs[FormationType::LINE] = lineConfig;

    FormationConfig circleConfig;
    circleConfig.type = FormationType::CIRCLE;
    circleConfig.baseSpacing = 5.0f;
    circleConfig.cohesionRadius = 8.0f;
    circleConfig.combatFormation = true;
    _formationConfigs[FormationType::CIRCLE] = circleConfig;
}

void FormationManager::TrackPerformance(std::chrono::microseconds duration, const std::string& operation)
{
    if (duration > _metrics.maxFormationTime)
        _metrics.maxFormationTime = duration;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        _metrics.averageFormationTime = std::chrono::microseconds(
            static_cast<uint64_t>(_metrics.averageFormationTime.count() * 0.9 + duration.count() * 0.1)
        );
        _metrics.lastUpdate = currentTime;
    }
}

// FormationUtils implementation
FormationType FormationUtils::GetOptimalFormationForGroup(const std::vector<Player*>& members)
{
    if (members.size() <= 2)
        return FormationType::COLUMN;
    else if (members.size() <= 5)
        return FormationType::DUNGEON;
    else if (members.size() <= 10)
        return FormationType::WEDGE;
    else
        return FormationType::RAID;
}

FormationType FormationUtils::GetOptimalFormationForCombat(const std::vector<Player*>& members, const std::vector<Unit*>& enemies)
{
    if (enemies.size() == 1)
        return FormationType::CIRCLE;
    else if (enemies.size() <= 3)
        return FormationType::LINE;
    else
        return FormationType::BOX;
}

FormationRole FormationUtils::DetermineOptimalRole(Player* player)
{
    if (!player)
        return FormationRole::SUPPORT;

    uint8 playerClass = player->getClass();
    switch (playerClass)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return FormationRole::TANK;

        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_EVOKER:
            return FormationRole::HEALER;

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return FormationRole::RANGED_DPS;

        case CLASS_ROGUE:
            return FormationRole::MELEE_DPS;

        default:
            return FormationRole::SUPPORT;
    }
}

bool FormationUtils::IsFormationValid(const std::vector<Position>& positions, FormationType formation)
{
    if (positions.empty())
        return false;

    for (size_t i = 1; i < positions.size(); ++i)
    {
        float distance = positions[0].GetExactDist(&positions[i]);
        if (distance > 30.0f)
            return false;
    }

    return true;
}

Position FormationUtils::CalculateFormationCenterFromMembers(const std::vector<Player*>& members)
{
    if (members.empty())
        return Position();

    float totalX = 0.0f, totalY = 0.0f, totalZ = 0.0f;

    for (Player* member : members)
    {
        if (member)
        {
            totalX += member->GetPositionX();
            totalY += member->GetPositionY();
            totalZ += member->GetPositionZ();
        }
    }

    Position center;
    center.m_positionX = totalX / members.size();
    center.m_positionY = totalY / members.size();
    center.m_positionZ = totalZ / members.size();

    return center;
}

// ============================================================================
// ADDITIONAL FORMATION PATTERNS (Ported from GroupFormationManager)
// ============================================================================

std::vector<Position> FormationManager::CalculateDiamondFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    // Diamond formation: tank front, DPS sides, healer rear center
    // 4 cardinal points + fill interior
    if (_members.empty())
        return positions;

    float spacing = _formationSpacing * 2.0f;

    // Position 0: Leader at center
    positions.push_back(leaderPos);

    if (_members.size() == 1)
        return positions;

    // Position 1: Front (North) - tank position
    Position frontPos;
    frontPos.m_positionX = leaderPos.GetPositionX() + spacing * std::sin(orientation);
    frontPos.m_positionY = leaderPos.GetPositionY() + spacing * std::cos(orientation);
    frontPos.m_positionZ = leaderPos.GetPositionZ();
    positions.push_back(frontPos);

    if (_members.size() == 2)
        return positions;

    // Position 2: Rear (South) - healer position
    Position rearPos;
    rearPos.m_positionX = leaderPos.GetPositionX() - spacing * std::sin(orientation);
    rearPos.m_positionY = leaderPos.GetPositionY() - spacing * std::cos(orientation);
    rearPos.m_positionZ = leaderPos.GetPositionZ();
    positions.push_back(rearPos);

    if (_members.size() == 3)
        return positions;

    // Position 3: Left (West) - DPS position
    float leftAngle = orientation - M_PI/2;
    Position leftPos;
    leftPos.m_positionX = leaderPos.GetPositionX() + spacing * std::sin(leftAngle);
    leftPos.m_positionY = leaderPos.GetPositionY() + spacing * std::cos(leftAngle);
    leftPos.m_positionZ = leaderPos.GetPositionZ();
    positions.push_back(leftPos);

    if (_members.size() == 4)
        return positions;

    // Position 4: Right (East) - DPS position
    float rightAngle = orientation + M_PI/2;
    Position rightPos;
    rightPos.m_positionX = leaderPos.GetPositionX() + spacing * std::sin(rightAngle);
    rightPos.m_positionY = leaderPos.GetPositionY() + spacing * std::cos(rightAngle);
    rightPos.m_positionZ = leaderPos.GetPositionZ();
    positions.push_back(rightPos);

    // Fill interior diamond with remaining members
    float innerRadius = spacing * 0.75f;
    size_t remainingMembers = _members.size() - 5;

    for (size_t i = 0; i < remainingMembers; ++i)
    {
        float angle = orientation + (i / static_cast<float>(remainingMembers)) * 2.0f * M_PI;

        Position pos;
        pos.m_positionX = leaderPos.GetPositionX() + innerRadius * std::sin(angle);
        pos.m_positionY = leaderPos.GetPositionY() + innerRadius * std::cos(angle);
        pos.m_positionZ = leaderPos.GetPositionZ();
        positions.push_back(pos);
    }

    return positions;
}

std::vector<Position> FormationManager::CalculateBoxFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    // Box formation (defensive square): healers center, tanks corners, DPS edges
    if (_members.empty())
        return positions;

    float halfSize = _formationSpacing * 2.0f;

    // Position 0: Leader at center
    positions.push_back(leaderPos);

    if (_members.size() == 1)
        return positions;

    // Calculate corner positions with orientation
    float corners[4][2] = {
        {-halfSize, halfSize},   // NW
        {halfSize, halfSize},    // NE
        {-halfSize, -halfSize},  // SW
        {halfSize, -halfSize}    // SE
    };

    // Add corners (tanks)
    for (size_t i = 1; i < _members.size() && i <= 4; ++i)
    {
        float offsetX = corners[i-1][0];
        float offsetY = corners[i-1][1];

        // Rotate by orientation
        float rotatedX = offsetX * std::cos(orientation) - offsetY * std::sin(orientation);
        float rotatedY = offsetX * std::sin(orientation) + offsetY * std::cos(orientation);

        Position pos;
        pos.m_positionX = leaderPos.GetPositionX() + rotatedX;
        pos.m_positionY = leaderPos.GetPositionY() + rotatedY;
        pos.m_positionZ = leaderPos.GetPositionZ();
        positions.push_back(pos);
    }

    if (_members.size() <= 5)
        return positions;

    // Add edge positions (DPS) - distributed along the 4 edges
    size_t remainingMembers = _members.size() - 5;
    size_t botsPerEdge = (remainingMembers + 3) / 4;  // Distribute evenly

    float edges[4][4] = {
        // Start X, Start Y, End X, End Y
        {-halfSize, halfSize, halfSize, halfSize},     // North edge
        {halfSize, halfSize, halfSize, -halfSize},     // East edge
        {halfSize, -halfSize, -halfSize, -halfSize},   // South edge
        {-halfSize, -halfSize, -halfSize, halfSize}    // West edge
    };

    size_t memberIdx = 0;
    for (size_t edge = 0; edge < 4 && memberIdx < remainingMembers; ++edge)
    {
        for (size_t i = 0; i < botsPerEdge && memberIdx < remainingMembers; ++i)
        {
            float t = (i + 1) / static_cast<float>(botsPerEdge + 1);
            float offsetX = edges[edge][0] + t * (edges[edge][2] - edges[edge][0]);
            float offsetY = edges[edge][1] + t * (edges[edge][3] - edges[edge][1]);

            // Rotate by orientation
            float rotatedX = offsetX * std::cos(orientation) - offsetY * std::sin(orientation);
            float rotatedY = offsetX * std::sin(orientation) + offsetY * std::cos(orientation);

            Position pos;
            pos.m_positionX = leaderPos.GetPositionX() + rotatedX;
            pos.m_positionY = leaderPos.GetPositionY() + rotatedY;
            pos.m_positionZ = leaderPos.GetPositionZ();
            positions.push_back(pos);
            ++memberIdx;
        }
    }

    return positions;
}

std::vector<Position> FormationManager::CalculateRaidFormation(const Position& leaderPos, float orientation)
{
    std::vector<Position> positions;
    positions.reserve(_members.size());

    // Raid formation: Similar to dungeon but with wider spacing and more structured rows
    // Uses 5-person groups arranged in a grid pattern
    if (_members.empty())
        return positions;

    float spacing = _formationSpacing * 1.5f;

    // Leader at front center
    positions.push_back(leaderPos);

    if (_members.size() == 1)
        return positions;

    // Arrange in groups of 5 (standard WoW raid group size)
    size_t remainingMembers = _members.size() - 1;
    size_t numGroups = (remainingMembers + 4) / 5;

    for (size_t group = 0; group < numGroups; ++group)
    {
        size_t membersInGroup = std::min(size_t(5), remainingMembers - (group * 5));

        for (size_t member = 0; member < membersInGroup; ++member)
        {
            // Calculate row and column position
            float row = static_cast<float>(group);
            float col = static_cast<float>(member) - 2.0f;  // Center around 0 (-2, -1, 0, 1, 2)

            float offsetX = col * spacing;
            float offsetY = -row * spacing;  // Negative = behind leader

            // Rotate by orientation
            float rotatedX = offsetX * std::cos(orientation) - offsetY * std::sin(orientation);
            float rotatedY = offsetX * std::sin(orientation) + offsetY * std::cos(orientation);

            Position pos;
            pos.m_positionX = leaderPos.GetPositionX() + rotatedX;
            pos.m_positionY = leaderPos.GetPositionY() + rotatedY;
            pos.m_positionZ = leaderPos.GetPositionZ();
            positions.push_back(pos);
        }
    }

    return positions;
}

} // namespace Playerbot