/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CombatAIIntegrator.h"
#include "RoleBasedCombatPositioning.h"
#include "InterruptCoordinator.h"
#include "ThreatCoordinator.h"
#include "FormationManager.h"
#include "TargetSelector.h"
#include "PathfindingManager.h"
#include "LineOfSightManager.h"
#include "ObstacleAvoidanceManager.h"
#include "KitingManager.h"
#include "InterruptDatabase.h"
#include "InterruptAwareness.h"
#include "MechanicAwareness.h"
#include "ThreatAbilities.h"
#include "ClassAI/ClassAI.h"
#include "Movement/UnifiedMovementCoordinator.h"  // Phase 2: Unified movement system
#include "Movement/Arbiter/MovementPriority.h"
#include "AI/BotAI.h"
#include "Group.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitAI.h"
#include <algorithm>
#include <execution>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace Playerbot
{

CombatAIIntegrator::CombatAIIntegrator(Player* bot) :
    _bot(bot),
    _classAI(nullptr),
    _currentPhase(CombatPhase::NONE),
    _currentTarget(nullptr),
    _group(nullptr),
    _lastUpdate(0),
    _combatStartTime(0),
    _phaseStartTime(0),
    _lastPositionUpdate(0),
    _lastInterruptCheck(0),
    _lastThreatUpdate(0)
{
    // Initialize all Phase 2 components
    _positioning = std::make_unique<RoleBasedCombatPositioning>(bot);
    _interruptCoordinator = std::make_unique<InterruptCoordinator>(bot);
    _threatCoordinator = std::make_unique<ThreatCoordinator>(bot);
    _formationManager = std::make_unique<FormationManager>();
    _targetSelector = std::make_unique<TargetSelector>(bot);
    _pathfinding = std::make_unique<PathfindingManager>(bot);
    _losManager = std::make_unique<LineOfSightManager>(bot);
    _obstacleAvoidance = std::make_unique<ObstacleAvoidanceManager>(bot);
    _kitingManager = std::make_unique<KitingManager>(bot);

    // Initialize support systems
    _interruptDB = std::make_unique<InterruptDatabase>();
    _interruptAwareness = std::make_unique<InterruptAwareness>(bot);
    _mechanicAwareness = std::make_unique<MechanicAwareness>(bot);
    _threatAbilities = std::make_unique<ThreatAbilities>(bot);
    // Set default configuration
    _config = CombatAIConfig();

    TC_LOG_DEBUG("bot.ai.combat", "CombatAIIntegrator initialized for bot {}", bot->GetName());
}

CombatAIIntegrator::~CombatAIIntegrator() = default;

IntegrationResult CombatAIIntegrator::Update(uint32 diff)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    IntegrationResult result;

    // Performance guard
    if (!IsWithinPerformanceLimits())
    {
        result.success = false;
        result.errorMessage = "Performance limits exceeded";
        return result;
    }

    // Thread safety
    std::lock_guard lock(_mutex);

    // Check minimum update interval
    _lastUpdate += diff;
    if (_lastUpdate < _config.updateIntervalMs)
    {
        result.success = true;
        result.phase = _currentPhase;
        return result;
    }

    StartMetricCapture();

    try
    {
        // Update combat phase state machine
        UpdateCombatPhase(diff);

        // Execute phase-specific behavior
        switch (_currentPhase)
        {
            case CombatPhase::ENGAGING:
                HandleEngagingPhase();
                break;
            case CombatPhase::OPENING:
                HandleOpeningPhase();
                break;
            case CombatPhase::SUSTAINED:
                HandleSustainedPhase();
                break;
            case CombatPhase::EXECUTE:
                HandleExecutePhase();
                break;
            case CombatPhase::DEFENSIVE:
                HandleDefensivePhase();
                break;
            case CombatPhase::KITING:
                HandleKitingPhase();
                break;
            case CombatPhase::REPOSITIONING:
                HandleRepositioningPhase();
                break;
            case CombatPhase::INTERRUPTING:
                HandleInterruptingPhase();
                break;
            case CombatPhase::RECOVERING:
                HandleRecoveringPhase();
                break;
            default:
                break;
        }

        // Update core combat systems
        if (_config.enableTargeting)
            UpdateTargeting(diff);

        if (_config.enablePositioning)
            UpdatePositioning(diff);

        if (_config.enableInterrupts)
            UpdateInterrupts(diff);

        if (_config.enableThreatManagement)
            UpdateThreatManagement(diff);

        if (_config.enableFormations && _group)
            UpdateFormation(diff);

        if (_config.enablePathfinding)
            UpdatePathfinding(diff);

        // Update group coordination if enabled
        if (_config.enableGroupCoordination && _group)
            UpdateGroupCoordination();

        result.success = true;
        result.phase = _currentPhase;
        result.actionsExecuted = _metrics.updateCount;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("bot.ai.combat", "CombatAIIntegrator::Update exception: {}", e.what());
        result.success = false;
        result.errorMessage = e.what();
    }

    _lastUpdate = 0;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    EndMetricCapture(elapsed);
    result.executionTime = elapsed;

    return result;
}

void CombatAIIntegrator::Reset()
{
    std::lock_guard lock(_mutex);

    _inCombat = false;
    _currentPhase = CombatPhase::NONE;
    _currentTarget = nullptr;
    _lastUpdate = 0;
    _combatStartTime = 0;
    _phaseStartTime = 0;

    // Reset all components
    _positioning->Reset();
    _interruptCoordinator->Reset();
    _threatCoordinator->Reset();
    _targetSelector->Reset();
    _pathfinding->Reset();
    _kitingManager->Reset();

    _metrics.Reset();
    TC_LOG_DEBUG("bot.ai.combat", "CombatAIIntegrator reset for bot {}", _bot->GetName());
}

void CombatAIIntegrator::OnCombatStart(Unit* target)
{
    std::lock_guard lock(_mutex);

    _inCombat = true;
    _currentTarget = target;
    _currentPhase = CombatPhase::ENGAGING;
    _combatStartTime = GameTime::GetGameTimeMS();
    _phaseStartTime = _combatStartTime;

    // Initialize combat components with target
    _positioning->OnCombatStart(target);
    _interruptCoordinator->OnCombatStart();
    _threatCoordinator->OnCombatStart();

    // Set initial target
    _targetSelector->SetPrimaryTarget(target);

    // Notify class AI if registered
    if (_classAI)
        _classAI->OnCombatStart(target);

    TC_LOG_DEBUG("bot.ai.combat", "Combat started for bot {} against {}",
        _bot->GetName(), target->GetName());
}

void CombatAIIntegrator::OnCombatEnd()
{
    std::lock_guard lock(_mutex);

    _inCombat = false;
    _currentPhase = CombatPhase::RECOVERING;
    _currentTarget = nullptr;

    // Reset combat components
    _positioning->OnCombatEnd();
    _interruptCoordinator->OnCombatEnd();
    _threatCoordinator->OnCombatEnd();

    // Notify class AI if registered
    if (_classAI)
        _classAI->OnCombatEnd();
    TC_LOG_DEBUG("bot.ai.combat", "Combat ended for bot {}", _bot->GetName());
}

void CombatAIIntegrator::OnTargetChanged(Unit* newTarget)
{
    std::lock_guard lock(_mutex);

    Unit* oldTarget = _currentTarget;
    _currentTarget = newTarget;
    // Update components with new target
    if (newTarget)
    {
        _targetSelector->SetPrimaryTarget(newTarget);
        _positioning->UpdateTarget(newTarget);
        _interruptCoordinator->UpdateTarget(newTarget);
        _threatCoordinator->OnTargetSwitch(oldTarget, newTarget);
    }

    // Notify class AI if registered
    if (_classAI)
        _classAI->OnTargetChanged(newTarget);

    TC_LOG_DEBUG("bot.ai.combat", "Target changed for bot {} from {} to {}",
        _bot->GetName(),
        oldTarget ? oldTarget->GetName() : "none",
        newTarget ? newTarget->GetName() : "none");
}

void CombatAIIntegrator::SetGroup(Group* group)
{
    std::lock_guard lock(_mutex);
    _group = group;

    // Update formation manager with group
    if (_formationManager && group)
    {
        std::vector<ObjectGuid> members;
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
                members.push_back(member->GetGUID());
        }
        _formationManager->UpdateGroupMembers(members);
    }
}

void CombatAIIntegrator::UpdateGroupCoordination()
{
    if (!_group)
        return;

    // Coordinate interrupts across group
    if (_config.enableInterrupts)
    {
        _interruptCoordinator->UpdateGroupCoordination(_group);
    }

    // Coordinate threat for tanks
    if (_config.enableThreatManagement)
    {
        _threatCoordinator->UpdateGroupCoordination(_group);
    }

    // Update formation positions
    if (_config.enableFormations)
    {
        _formationManager->UpdateFormation(FormationType::COMBAT_SPREAD);
    }
}

void CombatAIIntegrator::UpdateCombatPhase(uint32 diff)
{
    if (!_inCombat)
    {
        _currentPhase = CombatPhase::NONE;
        return;
    }

    uint32 currentTime = GameTime::GetGameTimeMS();
    uint32 phaseTime = currentTime - _phaseStartTime;

    // Phase transition logic
    switch (_currentPhase)
    {
        case CombatPhase::ENGAGING:
            if (_bot->IsWithinMeleeRange(_currentTarget) ||
                _bot->IsWithinDistInMap(_currentTarget, 30.0f))
            {
                _currentPhase = CombatPhase::OPENING;
                _phaseStartTime = currentTime;
            }
            break;

        case CombatPhase::OPENING:
            if (phaseTime > 3000) // 3 seconds for opening
            {
                _currentPhase = CombatPhase::SUSTAINED;
                _phaseStartTime = currentTime;
            }
            break;

        case CombatPhase::SUSTAINED:
            // Check for phase transitions
            if (_currentTarget && _currentTarget->GetHealthPct() < 20.0f)
            {
                _currentPhase = CombatPhase::EXECUTE;
                _phaseStartTime = currentTime;
            }
            else if (_bot->GetHealthPct() < 30.0f)
            {
                _currentPhase = CombatPhase::DEFENSIVE;
                _phaseStartTime = currentTime;
            }
            else if (ShouldKite())
            {
                _currentPhase = CombatPhase::KITING;
                _phaseStartTime = currentTime;
            }
            else if (ShouldUpdatePosition())
            {
                _currentPhase = CombatPhase::REPOSITIONING;
                _phaseStartTime = currentTime;
            }
            else if (ShouldInterrupt())
            {
                _currentPhase = CombatPhase::INTERRUPTING;
                _phaseStartTime = currentTime;
            }
            break;

        case CombatPhase::EXECUTE:
        case CombatPhase::DEFENSIVE:
        case CombatPhase::KITING:
        case CombatPhase::REPOSITIONING:
        case CombatPhase::INTERRUPTING:
            // Return to sustained after specific phase actions
            if (phaseTime > 2000) // 2 seconds for special phases
            {
                _currentPhase = CombatPhase::SUSTAINED;
                _phaseStartTime = currentTime;
            }
            break;

        case CombatPhase::RECOVERING:
            if (phaseTime > 5000) // 5 seconds recovery
            {
                _currentPhase = CombatPhase::NONE;
                _phaseStartTime = currentTime;
            }
            break;
    }
}

void CombatAIIntegrator::UpdatePositioning(uint32 diff)
{
    _lastPositionUpdate += diff;
    if (_lastPositionUpdate < 250) // Update every 250ms
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    if (_currentTarget && _positioning)
    {
        // Get optimal position based on role
        Position optimalPos = _positioning->GetOptimalPosition(_currentTarget);

        // Check if we need to move
        if (_bot->GetExactDist2d(&optimalPos) > _config.positionUpdateThreshold)
        {
            // Use pathfinding to get there
            if (_pathfinding)
            {
                auto path = _pathfinding->CalculatePath(
                    _bot->GetPosition(),
                    optimalPos,
                    true // smooth path
                );

                if (!path.empty())
                {
                    // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
                    BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
                    if (botAI && botAI->GetUnifiedMovementCoordinator())
                    {
                        bool accepted = botAI->RequestPointMovement(
                            PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
                            path.back(),
                            "Ranged combat positioning",
                            "CombatAIIntegrator");

                        if (accepted)
                        {
                            _metrics.positionChanges++;
                        }
                    }
                    else
                    {
                        // FALLBACK: Direct MotionMaster if arbiter not available
                        _bot->GetMotionMaster()->MovePoint(0, path.back());
                        _metrics.positionChanges++;
                    }
                }
            }
        }

        // Update positioning system
        _positioning->Update(diff);
    }

    _lastPositionUpdate = 0;

    auto endTime = std::chrono::high_resolution_clock::now();
    _metrics.positioningTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void CombatAIIntegrator::UpdateInterrupts(uint32 diff)
{
    _lastInterruptCheck += diff;
    if (_lastInterruptCheck < _config.interruptReactionTimeMs)
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    if (_currentTarget && _interruptCoordinator)
    {
        // Check if target is casting interruptible spell
        if (_currentTarget->HasUnitState(UNIT_STATE_CASTING))
        {
            if (Spell const* spell = _currentTarget->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            {
                if (SpellInfo const* spellInfo = spell->GetSpellInfo())
                {
                    // Check if spell should be interrupted
                    auto priority = _interruptDB->GetInterruptPriority(spellInfo->Id);
                    if (priority != InterruptPriority::IGNORE)
                    {
                        // Attempt interrupt through coordinator
                        if (_interruptCoordinator->TryInterrupt(_currentTarget, spellInfo->Id))
                        {
                            _metrics.interruptsAttempted++;
                            _metrics.interruptsSuccessful++;
                        }
                    }
                }
            }
        }

        // Update interrupt systems
        _interruptCoordinator->Update(diff);
        _interruptAwareness->Update(diff);
    }

    _lastInterruptCheck = 0;

    auto endTime = std::chrono::high_resolution_clock::now();
    _metrics.interruptTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void CombatAIIntegrator::UpdateThreatManagement(uint32 diff)
{
    _lastThreatUpdate += diff;
    if (_lastThreatUpdate < 500) // Update every 500ms
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    if (_currentTarget && _threatCoordinator)
    {
        // Update threat based on role
        ThreatUpdateRequest request;
        request.target = _currentTarget;

        // Determine desired threat level based on role
        CombatRole role = _positioning->GetRole();
        switch (role)
        {
            case CombatRole::TANK:
                request.desiredThreatLevel = ThreatLevel::HIGHEST;
                break;
            case CombatRole::HEALER:
                request.desiredThreatLevel = ThreatLevel::LOWEST;
                break;
            case CombatRole::MELEE_DPS:
            case CombatRole::RANGED_DPS:
                request.desiredThreatLevel = ThreatLevel::MODERATE;
                break;
        }

        // Apply threat adjustment
        _threatCoordinator->UpdateThreat(request);
        _metrics.threatAdjustments++;

        // Update threat abilities
        _threatAbilities->Update(diff);
    }

    _lastThreatUpdate = 0;

    auto endTime = std::chrono::high_resolution_clock::now();
    _metrics.threatTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void CombatAIIntegrator::UpdateTargeting(uint32 diff)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    if (_targetSelector)
    {
        // Update target priorities
        _targetSelector->UpdateTargetPriorities();

        // Check if we should switch targets
        if (ShouldSwitchTarget())
        {
            Unit* newTarget = _targetSelector->GetBestTarget();
            if (newTarget && newTarget != _currentTarget)
            {
                OnTargetChanged(newTarget);
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    _metrics.targetingTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void CombatAIIntegrator::UpdateFormation(uint32 diff)
{
    if (!_formationManager || !_group)
        return;

    // Update formation based on combat phase
    FormationType formation = FormationType::COMBAT_SPREAD;

    switch (_currentPhase)
    {
        case CombatPhase::ENGAGING:
            formation = FormationType::COMBAT_TIGHT;
            break;
        case CombatPhase::DEFENSIVE:
            formation = FormationType::DEFENSIVE_CIRCLE;
            break;
        case CombatPhase::KITING:
            formation = FormationType::TRAVEL_COLUMN;
            break;
        default:
            formation = FormationType::COMBAT_SPREAD;
            break;
    }

    _formationManager->SetFormationType(formation);
    _formationManager->UpdateFormation(formation);
}

void CombatAIIntegrator::UpdatePathfinding(uint32 diff)
{
    if (!_pathfinding)
        return;

    // Update obstacle avoidance
    if (_obstacleAvoidance)
    {
        _obstacleAvoidance->Update(diff);

        // Check for obstacles in path
        if (_obstacleAvoidance->HasObstaclesAhead())
        {
            Position avoidancePos = _obstacleAvoidance->GetAvoidancePosition();

            // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
            BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
            if (botAI && botAI->GetUnifiedMovementCoordinator())
            {
                botAI->RequestPointMovement(
                    PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
                    avoidancePos,
                    "Obstacle avoidance",
                    "CombatAIIntegrator");
            }
            else
            {
                // FALLBACK: Direct MotionMaster if arbiter not available
                _bot->GetMotionMaster()->MovePoint(0, avoidancePos);
            }
        }
    }

    // Update line of sight
    if (_losManager && _currentTarget)
    {
        if (!_losManager->HasLineOfSight(_currentTarget))
        {
            Position losPos = _losManager->GetLineOfSightPosition(_currentTarget);
            if (_pathfinding)
            {
                auto path = _pathfinding->CalculatePath(
                    _bot->GetPosition(),
                    losPos,
                    true
                );

                if (!path.empty())
                {
                    // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
                    BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
                    if (botAI && botAI->GetUnifiedMovementCoordinator())
                    {
                        botAI->RequestPointMovement(
                            PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
                            path.back(),
                            "Line of sight pursuit",
                            "CombatAIIntegrator");
                    }
                    else
                    {
                        // FALLBACK: Direct MotionMaster if arbiter not available
                        _bot->GetMotionMaster()->MovePoint(0, path.back());
                    }
                }
            }
        }
    }
}

// Phase handlers
void CombatAIIntegrator::HandleEngagingPhase()
{
    if (!_currentTarget)
        return;

    // Move towards target
    if (!_bot->IsWithinMeleeRange(_currentTarget))
    {
        Position engagePos = _positioning->GetEngagementPosition(_currentTarget);

        // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
        BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
        if (botAI && botAI->GetUnifiedMovementCoordinator())
        {
            botAI->RequestPointMovement(
                PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
                engagePos,
                "Melee engagement",
                "CombatAIIntegrator");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available
            _bot->GetMotionMaster()->MovePoint(0, engagePos);
        }
    }

    // Pre-combat buffs and preparations
    if (_classAI)
    {
        _classAI->UpdateBuffs();
    }
}

void CombatAIIntegrator::HandleOpeningPhase()
{
    if (!_currentTarget)
        return;

    // Execute opening rotation
    if (_classAI)
    {
        _classAI->UpdateRotation(_currentTarget);
    }

    // Initial threat establishment for tanks
    if (_positioning->GetRole() == CombatRole::TANK)
    {
        _threatCoordinator->EstablishThreat(_currentTarget);
    }
}

void CombatAIIntegrator::HandleSustainedPhase()
{
    if (!_currentTarget)
        return;

    // Main combat rotation
    if (_classAI)
    {
        _classAI->UpdateRotation(_currentTarget);
    }

    // Maintain optimal positioning
    Position optimalPos = _positioning->GetOptimalPosition(_currentTarget);
    if (_bot->GetExactDist2d(&optimalPos) > 5.0f)
    {
        // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
        BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
        if (botAI && botAI->GetUnifiedMovementCoordinator())
        {
            botAI->RequestPointMovement(
                PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
                optimalPos,
                "Optimal combat positioning",
                "CombatAIIntegrator");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available
            _bot->GetMotionMaster()->MovePoint(0, optimalPos);
        }
    }
}

void CombatAIIntegrator::HandleExecutePhase()
{
    if (!_currentTarget)
        return;

    // Execute phase rotation (high damage abilities)
    if (_classAI)
    {
        _classAI->UpdateRotation(_currentTarget);
    }
    // Aggressive positioning for execute
    _positioning->SetAggressiveMode(true);
}

void CombatAIIntegrator::HandleDefensivePhase()
{
    // Defensive cooldowns
    if (_classAI)
    {
        _classAI->UpdateBuffs(); // Use defensive abilities
    }

    // Defensive positioning
    _positioning->SetDefensiveMode(true);

    // Consider fleeing if health too low
    if (_bot->GetHealthPct() < 10.0f)
    {
        Position fleePos = _kitingManager->GetFleePosition();

        // PHASE 3 MIGRATION: Use Movement Arbiter with EMERGENCY_DEFENSIVE priority (240)
        BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
        if (botAI && botAI->GetUnifiedMovementCoordinator())
        {
            bool accepted = botAI->RequestPointMovement(
                PlayerBotMovementPriority::EMERGENCY_DEFENSIVE,  // Priority 240 - CRITICAL emergency flee
                fleePos,
                "Emergency flee - health below 10%",
                "CombatAIIntegrator");

            if (accepted)
            {
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    "CombatAIIntegrator: Bot {} emergency flee requested (HP: {:.1f}%)",
                    _bot->GetName(), _bot->GetHealthPct());
            }
            else
            {
                // Arbiter rejected - system might be shutting down or bot in invalid state
                TC_LOG_WARN("playerbot.movement.arbiter",
                    "CombatAIIntegrator: Emergency flee rejected for bot {} - using fallback direct call",
                    _bot->GetName());
                _bot->GetMotionMaster()->MovePoint(0, fleePos);
            }
        }
        else
        {
            // FALLBACK: Direct MotionMaster call if arbiter not available
            TC_LOG_DEBUG("playerbot.movement.arbiter",
                "CombatAIIntegrator: Movement Arbiter not available for bot {} - using direct MotionMaster",
                _bot->GetName());
            _bot->GetMotionMaster()->MovePoint(0, fleePos);
        }
    }
}

void CombatAIIntegrator::HandleKitingPhase()
{
    if (!_currentTarget)
        return;

    // Kiting movement
    Position kitePos = _kitingManager->GetKitePosition(_currentTarget);

    // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
    BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
    if (botAI && botAI->GetUnifiedMovementCoordinator())
    {
        botAI->RequestPointMovement(
            PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
            kitePos,
            "Kiting movement",
            "CombatAIIntegrator");
    }
    else
    {
        // FALLBACK: Direct MotionMaster if arbiter not available
        _bot->GetMotionMaster()->MovePoint(0, kitePos);
    }

    // Ranged attacks while kiting
    if (_classAI)
    {
        _classAI->UpdateRotation(_currentTarget);
    }

    _kitingManager->Update(100); // Fixed update for kiting
}

void CombatAIIntegrator::HandleRepositioningPhase()
{
    if (!_currentTarget)
        return;

    // Calculate and move to new position
    Position newPos = _positioning->CalculateRepositioning(_currentTarget);

    if (_pathfinding)
    {
        auto path = _pathfinding->CalculatePath(
            _bot->GetPosition(),
            newPos,
            true
        );

        if (!path.empty())
        {
            // PHASE 6B: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
            BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());
            if (botAI && botAI->GetUnifiedMovementCoordinator())
            {
                botAI->RequestPointMovement(
                    PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,
                    path.back(),
                    "Combat repositioning",
                    "CombatAIIntegrator");
            }
            else
            {
                // FALLBACK: Direct MotionMaster if arbiter not available
                _bot->GetMotionMaster()->MovePoint(0, path.back());
            }
        }
    }
}

void CombatAIIntegrator::HandleInterruptingPhase()
{
    if (!_currentTarget)
        return;

    // Focus on interrupt execution
    if (_currentTarget->HasUnitState(UNIT_STATE_CASTING))
    {
        _interruptCoordinator->ForceInterrupt(_currentTarget);
    }

    // Continue rotation while interrupting
    if (_classAI)
    {
        _classAI->UpdateRotation(_currentTarget);
    }
}

void CombatAIIntegrator::HandleRecoveringPhase()
{
    // Post-combat recovery
    if (_classAI)
    {
        _classAI->UpdateBuffs(); // Rebuff
    }

    // Reset positioning
    _positioning->Reset();

    // Loot if needed
    // TODO: Implement looting system
}

// Utility methods
bool CombatAIIntegrator::ShouldUpdatePosition()
{
    if (!_currentTarget || !_positioning)
        return false;

    Position currentPos = _bot->GetPosition();
    Position optimalPos = _positioning->GetOptimalPosition(_currentTarget);

    return _bot->GetExactDist2d(&optimalPos) > _config.positionUpdateThreshold;
}

bool CombatAIIntegrator::ShouldInterrupt()
{
    if (!_currentTarget || !_interruptCoordinator)
        return false;

    // Check if target is casting
    if (!_currentTarget->HasUnitState(UNIT_STATE_CASTING))
        return false;

    // Check if we have interrupt available
    return _interruptCoordinator->HasInterruptAvailable();
}

bool CombatAIIntegrator::ShouldAdjustThreat()
{
    if (!_currentTarget || !_threatCoordinator)
        return false;

    // Check current threat situation
    float currentThreat = _threatCoordinator->GetThreatPercentage(_currentTarget);
    CombatRole role = _positioning->GetRole();

    // Tanks should maintain high threat
    if (role == CombatRole::TANK && currentThreat < 90.0f)
        return true;

    // DPS should avoid pulling threat
    if ((role == CombatRole::MELEE_DPS || role == CombatRole::RANGED_DPS) && currentThreat > 80.0f)
        return true;

    // Healers should minimize threat
    if (role == CombatRole::HEALER && currentThreat > 50.0f)
        return true;

    return false;
}

bool CombatAIIntegrator::ShouldSwitchTarget()
{
    if (!_targetSelector)
        return false;

    // Check cooldown
    static uint32 lastSwitch = 0;
    uint32 now = GameTime::GetGameTimeMS();
    if (now - lastSwitch < _config.targetSwitchCooldownMs)
        return false;

    Unit* bestTarget = _targetSelector->GetBestTarget();
    if (bestTarget && bestTarget != _currentTarget)
    {
        lastSwitch = now;
        return true;
    }

    return false;
}

bool CombatAIIntegrator::ShouldKite()
{
    if (!_kitingManager)
        return false;

    // Check if we're a ranged class
    CombatRole role = _positioning->GetRole();
    if (role != CombatRole::RANGED_DPS && role != CombatRole::HEALER)
        return false;

    // Check if enemies are too close
    return _kitingManager->ShouldKite();
}

// Performance monitoring
void CombatAIIntegrator::StartMetricCapture()
{
    _metrics.updateCount++;
}

void CombatAIIntegrator::EndMetricCapture(std::chrono::microseconds elapsed)
{
    // Update CPU metrics
    float cpuPercent = (elapsed.count() / 1000.0f) / _config.updateIntervalMs * 100.0f;
    _metrics.avgCpuPercent = (_metrics.avgCpuPercent * (_metrics.updateCount - 1) + cpuPercent) / _metrics.updateCount;

    // Update memory metrics
    ValidateMemoryUsage();
}

bool CombatAIIntegrator::IsWithinPerformanceLimits() const
{
    // Check CPU usage
    if (_metrics.avgCpuPercent > 0.1f) // 0.1% CPU limit
        return false;

    // Check memory usage
    if (_metrics.memoryUsed > _config.maxMemoryBytes)
        return false;

    return true;
}

void CombatAIIntegrator::ValidateMemoryUsage()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    {
        _metrics.memoryUsed = pmc.PrivateUsage;
        _metrics.peakMemory = std::max(_metrics.peakMemory.load(), _metrics.memoryUsed.load());
    }
#endif
}

void CombatAIIntegrator::CompactMemory()
{
    // Compact component memory
    _positioning->CompactMemory();
    _interruptCoordinator->CompactMemory();
    _threatCoordinator->CompactMemory();
    _targetSelector->CompactMemory();
}

// Factory implementations
std::unique_ptr<CombatAIIntegrator> CombatAIFactory::CreateCombatAI(Player* bot)
{
    return std::make_unique<CombatAIIntegrator>(bot);
}

std::unique_ptr<CombatAIIntegrator> CombatAIFactory::CreateCombatAI(Player* bot, CombatAIConfig const& config)
{
    auto integrator = std::make_unique<CombatAIIntegrator>(bot);
    integrator->SetConfig(config);
    return integrator;
}

std::unique_ptr<CombatAIIntegrator> CombatAIFactory::CreateTankCombatAI(Player* bot)
{
    CombatAIConfig config;
    config.enableThreatManagement = true;
    config.threatUpdateThreshold = 5.0f; // More sensitive threat management
    config.positionUpdateThreshold = 3.0f; // More precise positioning

    auto integrator = std::make_unique<CombatAIIntegrator>(bot);
    integrator->SetConfig(config);
    return integrator;
}

std::unique_ptr<CombatAIIntegrator> CombatAIFactory::CreateHealerCombatAI(Player* bot)
{
    CombatAIConfig config;
    config.enablePositioning = true;
    config.enableKiting = true;
    config.positionUpdateThreshold = 10.0f; // Less movement for casting
    config.interruptReactionTimeMs = 150; // Faster interrupt reactions

    auto integrator = std::make_unique<CombatAIIntegrator>(bot);
    integrator->SetConfig(config);
    return integrator;
}

std::unique_ptr<CombatAIIntegrator> CombatAIFactory::CreateMeleeDPSCombatAI(Player* bot)
{
    CombatAIConfig config;
    config.enablePositioning = true;
    config.enableInterrupts = true;
    config.positionUpdateThreshold = 5.0f;
    config.targetSwitchCooldownMs = 500; // Faster target switching

    auto integrator = std::make_unique<CombatAIIntegrator>(bot);
    integrator->SetConfig(config);
    return integrator;
}

std::unique_ptr<CombatAIIntegrator> CombatAIFactory::CreateRangedDPSCombatAI(Player* bot)
{
    CombatAIConfig config;
    config.enablePositioning = true;
    config.enableKiting = true;
    config.enableInterrupts = true;
    config.positionUpdateThreshold = 7.0f;

    auto integrator = std::make_unique<CombatAIIntegrator>(bot);
    integrator->SetConfig(config);
    return integrator;
}

} // namespace Playerbot