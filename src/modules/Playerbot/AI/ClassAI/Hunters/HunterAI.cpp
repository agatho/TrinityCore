/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HunterAI.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include "BeastMasteryHunterRefactored.h"
#include "MarksmanshipHunterRefactored.h"
#include "SurvivalHunterRefactored.h"
#include "../BaselineRotationManager.h"
#include "Player.h"
#include "Pet.h"
#include "Group.h"
#include "CreatureAI.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "WorldSession.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Map.h"
#include "Log.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include <chrono>
#include "../../../Spatial/SpatialGridManager.h"
#include "../../../Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries
#include "../../../Movement/Arbiter/MovementArbiter.h"
#include "../../../Movement/Arbiter/MovementPriorityMapper.h"
#include "../../BotAI.h"
#include "UnitAI.h"

namespace Playerbot
{

HunterAI::HunterAI(Player* bot) :
    ClassAI(bot),
    _lastCounterShot(0),
    _lastFeignDeath(0),
    _lastDeterrence(0),
    _lastDisengage(0),
    _lastTrapPlacement(0),
    _lastPetCommand(0),
    _lastAspectSwitch(0),
    _lastPetRevive(0),
    _lastPetHeal(0),
    _petNeedsHeal(false),
    _petIsAggressive(false),
    _petTargetSwitch(0),
    _activeTrapType(0),
    _updateCounter(0),
    _totalUpdateTime(0),
    _peakUpdateTime(0)
{
    // Initialize combat behavior integration
    _combatBehaviors = std::make_unique<CombatBehaviorIntegration>(bot);    // Reset combat metrics
    _combatMetrics.Reset();

    TC_LOG_DEBUG("playerbot", "HunterAI initialized for {} with CombatBehaviorIntegration", bot->GetName());
}

void HunterAI::InitializeCombatSystems()
{
    // Initialize combat behavior integration systems
    if (_combatBehaviors)
    {
        _combatBehaviors->Update(0); // Initial update to set up managers
    }
}

void HunterAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Performance tracking
    auto startTime = std::chrono::high_resolution_clock::now();

    // Update combat behavior integration
    if (_combatBehaviors)
    {
        _combatBehaviors->Update(100); // Update with 100ms diff for standard tick
    }

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        // Use baseline rotation manager for unspecialized bots
        static BaselineRotationManager baselineManager;

        // Try auto-specialization if level 10+
        baselineManager.HandleAutoSpecialization(_bot);        // Execute baseline rotation

        if (baselineManager.ExecuteBaselineRotation(_bot, target))
        return;

        // Fallback to basic ranged attack        if (_bot->HasSpell(ARCANE_SHOT) && CanUseAbility(ARCANE_SHOT))

        {
        _bot->CastSpell(target, ARCANE_SHOT, false);
        }
        return;
    }

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Intelligent decision making for Hunters
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Priority 1: Interrupts (Counter Shot, Silencing Shot)
    if (HandleInterrupts(target))
        return;

    // Priority 2: Defensives (Feign Death, Deterrence, Aspect of the Turtle)
    if (HandleDefensives(target))
        return;

    // Priority 3: Positioning (Maintain optimal range, avoid dead zone)
    if (HandlePositioning(target))
        return;

    // Priority 4: Pet Management (Revive, Heal, Commands)
    if (HandlePetManagement(target))
        return;

    // Priority 5: Target Switching (Priority targets)
    if (HandleTargetSwitching(target))
        return;

    // Priority 6: Crowd Control (Freezing Trap, Scatter Shot)
    if (HandleCrowdControl(target))
        return;

    // Priority 7: AoE Decisions (Multi-Shot, Volley, Explosive Shot)
    if (HandleAoEDecisions(target))
        return;

    // Priority 8: Offensive Cooldowns (Bestial Wrath, Rapid Fire, Trueshot)
    if (HandleOffensiveCooldowns(target))
        return;

    // Priority 9: Normal Rotation - Delegate to specialization
    ExecuteNormalRotation(target);

    // Update combat metrics
    auto endTime = std::chrono::high_resolution_clock::now();
    uint32 updateTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    _totalUpdateTime += updateTime;
    _updateCounter++;
    if (updateTime > _peakUpdateTime)
        _peakUpdateTime = updateTime;
}

bool HunterAI::HandleInterrupts(::Unit* target)
{    if (!_combatBehaviors || !target)
        return false;

    if (_combatBehaviors->ShouldInterrupt(target))
    {
        Unit* interruptTarget = _combatBehaviors->GetInterruptTarget();
        if (!interruptTarget)

            interruptTarget = target;

        // Counter Shot (primary interrupt)
        if (CanInterruptTarget(interruptTarget))
        {

            uint32 now = GameTime::GetGameTimeMS();
            // Try Counter Shot first
            if (_bot->HasSpell(COUNTER_SHOT) && CanUseAbility(COUNTER_SHOT) &&

                now - _lastCounterShot > 24000) // 24 second cooldown

            {

                if (CastSpell(interruptTarget, COUNTER_SHOT))

                {

                    _lastCounterShot = now;

                    _combatMetrics.interrupts++;

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} interrupted {} with Counter Shot",

                                 GetBot()->GetName(), interruptTarget->GetName());

                    return true;

                }

            }

            // Try Silencing Shot if available (MM spec)            if (_bot->HasSpell(SILENCING_SHOT) && CanUseAbility(SILENCING_SHOT))

            {

                if (CastSpell(interruptTarget, SILENCING_SHOT))
                {

                    _combatMetrics.interrupts++;

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} interrupted {} with Silencing Shot",

                                 GetBot()->GetName(), interruptTarget->GetName());

                    return true;

                }

            }
        }
    }
    return false;
}

bool HunterAI::HandleDefensives(::Unit* target)
{
    if (!_combatBehaviors)
        return false;    if (_combatBehaviors->NeedsDefensive())
    {
        float healthPct = GetBot()->GetHealthPct();
        uint32 now = GameTime::GetGameTimeMS();

        // Feign Death - emergency escape
        if (healthPct < FEIGN_DEATH_THRESHOLD && ShouldFeignDeath())

        {
        if (_bot->HasSpell(FEIGN_DEATH) && CanUseAbility(FEIGN_DEATH) &&

                now - _lastFeignDeath > 30000) // 30 second cooldown
                {

                if (CastSpell(FEIGN_DEATH))

                {

                    _lastFeignDeath = now;

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} used Feign Death at {}% health",

                                 GetBot()->GetName(), static_cast<uint32>(healthPct));

                    return true;

                }

            }
        }

        // Deterrence - damage reduction        if (healthPct < DEFENSIVE_HEALTH_THRESHOLD)

        {
        if (_bot->HasSpell(DETERRENCE) && CanUseAbility(DETERRENCE) &&

                now - _lastDeterrence > 120000) // 2 minute cooldown

            {

                if (CastSpell(DETERRENCE))

                {

                    _lastDeterrence = now;
                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Deterrence",

                                 GetBot()->GetName());

                    return true;

                }

            }
        }

        // Aspect of the Turtle - modern defensive
        if (healthPct < DEFENSIVE_HEALTH_THRESHOLD)

        {
        if (_bot->HasSpell(ASPECT_OF_THE_TURTLE) && CanUseAbility(ASPECT_OF_THE_TURTLE))

            {

                if (CastSpell(ASPECT_OF_THE_TURTLE))

                {

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Aspect of the Turtle",

                                 GetBot()->GetName());

                    return true;

                }

            }
        }

        // Exhilaration - self heal        if (healthPct < 50.0f && _bot->HasSpell(EXHILARATION) && CanUseAbility(EXHILARATION))

        {
        if (CastSpell(EXHILARATION))

            {

                TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} used Exhilaration for healing",

                             GetBot()->GetName());

                return true;

            }
        }

        // Disengage for creating distance
        if (target && GetDistanceToTarget(target) < DEAD_ZONE_MAX)

        {
        if (_bot->HasSpell(HUNTER_DISENGAGE) && CanUseAbility(HUNTER_DISENGAGE) &&

                now - _lastDisengage > 20000) // 20 second cooldown

            {

                if (CastSpell(HUNTER_DISENGAGE))

                {

                    _lastDisengage = now;

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} used Disengage to escape dead zone",

                                 GetBot()->GetName());

                    return true;

                }

            }
        }
    }
    return false;
}

bool HunterAI::HandlePositioning(::Unit* target)
{
    if (!_combatBehaviors || !target)
        return false;    if (_combatBehaviors->NeedsRepositioning())
    {
        float distance = GetDistanceToTarget(target);

        // Check if in dead zone
        if (IsInDeadZone(target))
        {

            _combatMetrics.timeInDeadZone += 0.1f;

            // Try to get out of dead zone            if (distance < DEAD_ZONE_MAX)

            {
                // Too close - use disengage or move back                if (_bot->HasSpell(HUNTER_DISENGAGE) && CanUseAbility(HUNTER_DISENGAGE))

                {

                    uint32 now = GameTime::GetGameTimeMS();

                    if (now - _lastDisengage > 20000)

                    {

                        if (CastSpell(HUNTER_DISENGAGE))

                        {

                            _lastDisengage = now;

                            TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} disengaged from dead zone",

                                         GetBot()->GetName());

                            return true;

                        }

                    }

                }

                // Use melee abilities while in dead zone                if (_bot->HasSpell(WING_CLIP) && CanUseAbility(WING_CLIP))

                {

                    CastSpell(target, WING_CLIP);

                    return true;

                }

                // Movement will be handled by BotAI movement strategies

                Position optimalPos = _combatBehaviors->GetOptimalPosition();
                // Movement is handled externally

            }
        }
        else
        {

            _combatMetrics.timeAtRange += 0.1f;
        }

        // Maintain optimal range for kiting
        if (NeedsToKite(target))
        {
            // Apply slowing effects            if (_bot->HasSpell(CONCUSSIVE_SHOT) && CanUseAbility(CONCUSSIVE_SHOT))

            {

                if (CastSpell(target, CONCUSSIVE_SHOT))

                {

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} applied Concussive Shot for kiting",

                                 GetBot()->GetName());

                    return true;

                }

            }

            // Switch to Aspect of the Cheetah if needed            if (!_bot->IsInCombat() && distance > 40.0f)

            {

                SwitchToMovementAspect();

            }
        }
    }
    return false;
}bool HunterAI::HandlePetManagement(::Unit* target)
{
    UpdatePetStatus();

    // Priority: Revive dead pet
    if (NeedsPetRevive())
    {
        uint32 now = GameTime::GetGameTimeMS();
        if (now - _lastPetRevive > 10000) // Don't spam revive
        {

            RevivePet();

            _lastPetRevive = now;

            TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} reviving pet",

                         GetBot()->GetName());

            return true;
        }
    }

    // Call pet if no pet
    if (!HasActivePet())
    {
        CallPet();
        TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} calling pet",

                     GetBot()->GetName());
        return true;    }

    if (!priorityTarget)
    {
        return nullptr;
    }
    Pet* pet = GetPet();
    if (!pet)
        return false;

    // Heal pet if needed
    if (!priorityTarget)
    {
        return nullptr;
    }
    if (NeedsPetHeal())
    {
        uint32 now = GameTime::GetGameTimeMS();
        if (now - _lastPetHeal > 3000) // Don't spam heal
        {

            HealPet();

            _lastPetHeal = now;

            TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} healing pet ({}% health)",

                         GetBot()->GetName(), static_cast<uint32>(GetPetHealthPercent()));

            return true;
        }
    }

    if (!priorityTarget)
    {
        return nullptr;
    }
    // Command pet to attack if not already
    if (target && !IsPetInCombat())
    {
        if (!priorityTarget)
        {

            return nullptr;
        }
        CommandPetAttack(target);
        _combatMetrics.petCommands++;
        return false; // Don't stop rotation for this
    }

    // Use Kill Command if available (BM spec)
    if (target && GetCurrentSpecialization() == HunterSpec::BEAST_MASTERY)
    {        if (_bot->HasSpell(KILL_COMMAND) && CanUseAbility(KILL_COMMAND))
        {

            if (CastSpell(target, KILL_COMMAND))

            {

                TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} used Kill Command",

                             GetBot()->GetName());

                return false; // Continue rotation

            }
        }
    }

    if (!ccTarget)
    {
        return nullptr;
    }
    // Master's Call for freedom effects
    if (_bot->HasUnitState(UNIT_STATE_ROOT) || _bot->HasUnitState(UNIT_STATE_STUNNED))
    if (!ccTarget)
    {
        return nullptr;
    }
    {
        if (!bot)
        {

            if (!ccTarget)

            {

                return nullptr;

            }

            return nullptr;
        }
        if (_bot->HasSpell(MASTER_S_CALL) && CanUseAbility(MASTER_S_CALL))
        {

            if (CastSpell(MASTER_S_CALL))
            {

                TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} used Master's Call for freedom",

                             GetBot()->GetName());

                return true;

            }
        if (!ccTarget)
        {

            return nullptr;
        }
        }
    }

    return false;
}bool HunterAI::HandleTargetSwitching(::Unit* target)
{
    if (!_combatBehaviors)
        return false;

    if (!ccTarget)
    {
        return nullptr;
    }
    if (_combatBehaviors->ShouldSwitchTarget())
    {
        Unit* priorityTarget = _combatBehaviors->GetPriorityTarget();
        if (priorityTarget && priorityTarget != target)

                                     if (!priorityTarget)

                                     {
                                     TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: priorityTarget in method HasAura");

                                         return nullptr;

                                     }

                                     if (!priorityTarget)

                                     {

                                         return;

                                     }
        {
            // Apply Hunter's Mark to new target            if (_bot->HasSpell(HUNTER_S_MARK) && CanUseAbility(HUNTER_S_MARK))

            {

                if (!priorityTarget->HasAura(HUNTER_S_MARK))

                {

                    if (CastSpell(priorityTarget, HUNTER_S_MARK))

                    {

                        TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} marked priority target {}",

                                     GetBot()->GetName(), priorityTarget->GetName());

                    }

                }

            }
            // Command pet to switch targets

            if (HasActivePet())

            {

                CommandPetAttack(priorityTarget);

                _petTargetSwitch = GameTime::GetGameTimeMS();

            }

            // Update current target

            _currentTarget = priorityTarget->GetGUID();

                         if (!priorityTarget)

                         {

                             return;

                         }


            TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} switching to priority target {}",

                         GetBot()->GetName(), priorityTarget->GetName());

            return false; // Continue with new target
        }    }
    return false;
}

bool HunterAI::HandleCrowdControl(::Unit* target)
{
    if (!_combatBehaviors)
        return false;

    if (_combatBehaviors->ShouldUseCrowdControl())
    {
        ::Unit* ccTarget = GetBestCrowdControlTarget();

        if (ccTarget && ccTarget != target)
        if (!ccTarget)

                                     {

                                         return nullptr;

                                     }

            if (!ccTarget)

            {

                return nullptr;

            }

                                 if (!ccTarget)

                                 {

                                     return;

                                 }
        {

            uint32 now = GameTime::GetGameTimeMS();

            // Freezing Trap for long CC

            if (ShouldPlaceFreezingTrap(ccTarget))

            {

                if (now - _lastTrapPlacement > 30000) // 30 second trap CD

                {

                    PlaceTrap(FREEZING_TRAP, ccTarget->GetPosition());

                    _lastTrapPlacement = now;

                    _frozenTargets.insert(ccTarget->GetGUID());

                    _combatMetrics.trapsTriggered++;

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} placed Freezing Trap for {}",
                    GetBot()->GetName(), ccTarget->GetName());

                    return true;

                }

            }

            // Scatter Shot for instant CC            if (_bot->HasSpell(SCATTER_SHOT) && CanUseAbility(SCATTER_SHOT))

            {

                if (CastSpell(ccTarget, SCATTER_SHOT))
                {

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} used Scatter Shot on {}",

                                 if (!ccTarget)

                                 {

                                     return;

                                 }

                                 GetBot()->GetName(), ccTarget->GetName());

                    return true;

                }

            }

            // Concussive Shot for slowing            if (_bot->HasSpell(CONCUSSIVE_SHOT) && CanUseAbility(CONCUSSIVE_SHOT))

            {

                if (CastSpell(ccTarget, CONCUSSIVE_SHOT))

                {

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} slowed {} with Concussive Shot",

                                 if (!ccTarget)
                                 {

                                     return;

                                 }

                                 GetBot()->GetName(), ccTarget->GetName());

                    return false; // Continue rotation

                }

            }
        }
    }
    return false;
}

bool HunterAI::HandleAoEDecisions(::Unit* target){
    if (!_combatBehaviors || !target)
        return false;

    if (_combatBehaviors->ShouldAOE())
    {
        uint32 nearbyEnemies = GetNearbyEnemyCount(10.0f);

        // Multi-Shot for 3+ targets
        if (nearbyEnemies >= 3)

        {
        if (_bot->HasSpell(MULTI_SHOT) && CanUseAbility(MULTI_SHOT))

            {

                if (CastSpell(target, MULTI_SHOT))

                {

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} using Multi-Shot on {} enemies",

                                 GetBot()->GetName(), nearbyEnemies);

                    return true;

                }

            }
        }        // Volley for ground-targeted AoE
        if (nearbyEnemies >= 4 && _bot->HasSpell(VOLLEY) && CanUseAbility(VOLLEY))        {
            // Volley needs special handling for ground targeting

            Position aoeCenter = _combatBehaviors->GetOptimalPosition();

            TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} would cast Volley for {} enemies",

                         GetBot()->GetName(), nearbyEnemies);
            // Ground-targeted spell handling would go here
        }

        // Explosive Shot for Survival spec        if (GetCurrentSpecialization() == HunterSpec::SURVIVAL && nearbyEnemies >= 2)
        {

            if (_bot->HasSpell(EXPLOSIVE_SHOT) && CanUseAbility(EXPLOSIVE_SHOT))

            {

                if (CastSpell(target, EXPLOSIVE_SHOT))
                {

                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} using Explosive Shot for AoE",

                                 GetBot()->GetName());

                    return true;

                }

            }
        }

        // Barrage for modern AoE
        if (nearbyEnemies >= 3 && _bot->HasSpell(BARRAGE) && CanUseAbility(BARRAGE))        {

            if (CastSpell(target, BARRAGE))

            {

                TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} using Barrage for AoE",

                             GetBot()->GetName());

                return true;

            }
        }

        // Place explosive trap for AoE damage
        if (ShouldPlaceExplosiveTrap())
        {

            uint32 now = GameTime::GetGameTimeMS();

            if (now - _lastTrapPlacement > 30000)

            {
            PlaceTrap(13813, target->GetPosition()); // Explosive Trap spell ID

                _lastTrapPlacement = now;

                _combatMetrics.trapsTriggered++;

                TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} placed Explosive Trap for AoE",

                             GetBot()->GetName());

                return true;

            }
        }
    }
    return false;
}

bool HunterAI::HandleOffensiveCooldowns(::Unit* target){
    if (!_combatBehaviors || !target)
        return false;

    if (_combatBehaviors->ShouldUseCooldowns())
    {
        bool usedCooldown = false;

        switch (GetCurrentSpecialization())
        {

            case HunterSpec::BEAST_MASTERY:
                // Bestial Wrath - pet damage boost                if (_bot->HasSpell(BESTIAL_WRATH) && CanUseAbility(BESTIAL_WRATH))

                {

                    if (HasActivePet() && IsPetInCombat())

                    {

                        if (CastSpell(BESTIAL_WRATH))

                        {
                        TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Bestial Wrath",

                                         GetBot()->GetName());

                            usedCooldown = true;

                        }

                    }

                }

                // Aspect of the Wild - BM cooldown                if (_bot->HasSpell(ASPECT_OF_THE_WILD) && CanUseAbility(ASPECT_OF_THE_WILD))

                {

                    if (CastSpell(ASPECT_OF_THE_WILD))

                    {

                        TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Aspect of the Wild",

                                     GetBot()->GetName());

                        usedCooldown = true;

                    }

                }

                break;


            case HunterSpec::MARKSMANSHIP:
                // Trueshot - MM burst                if (_bot->HasSpell(TRUESHOT) && CanUseAbility(TRUESHOT))

                {

                    if (CastSpell(TRUESHOT))

                    {
                    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Trueshot",

                                     GetBot()->GetName());

                        usedCooldown = true;

                    }

                }

                // Rapid Fire for attack speed                if (_bot->HasSpell(RAPID_FIRE) && CanUseAbility(RAPID_FIRE))

                {

                    if (CastSpell(RAPID_FIRE))

                    {

                        TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Rapid Fire",

                                     GetBot()->GetName());

                        usedCooldown = true;

                    }

                }

                break;


            case HunterSpec::SURVIVAL:
            // Coordinated Assault or similar survival cooldowns
                // Survival uses different cooldowns depending on version                if (_bot->HasSpell(RAPID_FIRE) && CanUseAbility(RAPID_FIRE))

                {

                    if (CastSpell(RAPID_FIRE))

                    {

                        TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} activated Rapid Fire",

                                     GetBot()->GetName());

                        usedCooldown = true;

                    }

                }

                break;
        }

        return usedCooldown;
    }    return false;
}

void HunterAI::ExecuteNormalRotation(::Unit* target){
    if (!target)
        return;

    // Ensure we have proper aspect
    ManageAspects();

    // Apply Hunter's Mark if not present    if (!target->HasAura(HUNTER_S_MARK) && _bot->HasSpell(HUNTER_S_MARK) && CanUseAbility(HUNTER_S_MARK))
    {
        CastSpell(target, HUNTER_S_MARK);
    }

    // Apply Serpent Sting if not present    if (!target->HasAura(SERPENT_STING) && _bot->HasSpell(SERPENT_STING) && CanUseAbility(SERPENT_STING))
    {
        CastSpell(target, SERPENT_STING);
    }

    // Kill Shot if target is low health    if (target->GetHealthPct() < 20.0f && _bot->HasSpell(KILL_SHOT) && CanUseAbility(KILL_SHOT))
    {
        if (CastSpell(target, KILL_SHOT))
        {

            RecordShotResult(true, false);

            return;
        }
    }

    // Fallback basic rotation (specialization rotations handled by refactored system)    if (_bot->HasSpell(STEADY_SHOT) && CanUseAbility(STEADY_SHOT))
    {
        CastSpell(target, STEADY_SHOT);
        RecordShotResult(true, false);
    }    else if (_bot->HasSpell(ARCANE_SHOT) && CanUseAbility(ARCANE_SHOT))
    {
        CastSpell(target, ARCANE_SHOT);
        RecordShotResult(true, false);
    }
}

void HunterAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Check if bot should use baseline buffs
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(_bot);
        return;
    }

    // Use full hunter buff system for specialized bots
    ManageAspects();
    UpdateTracking();

    // Specialization buffs handled by refactored system
}

void HunterAI::UpdateCooldowns(uint32 diff)
{
    // Update combat behavior integration
    if (_combatBehaviors)
        _combatBehaviors->Update(diff);

    // Base class handles cooldown tracking
    ClassAI::UpdateCooldowns(diff);
}

bool HunterAI::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Additional hunter-specific checks
    if (IsInDeadZone(nullptr) && spellId != WING_CLIP && spellId != HUNTER_DISENGAGE)
        return false;

    return ClassAI::CanUseAbility(spellId);
}

void HunterAI::OnCombatStart(::Unit* target)
{
    _inCombat = true;
    _currentTarget = target ? target->GetGUID() : ObjectGuid::Empty;    _combatTime = 0;

    // Reset combat metrics
    _combatMetrics.Reset();

    // Initialize combat behaviors
    InitializeCombatSystems();

    // Switch to combat aspect
    SwitchToCombatAspect();

    // Command pet to attack
    if (HasActivePet() && target)
    {
        CommandPetAttack(target);
    }

    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} entering combat with {}",

                 GetBot()->GetName(), target ? target->GetName() : "null");

    ClassAI::OnCombatStart(target);
}

void HunterAI::OnCombatEnd(){
    _inCombat = false;
    _currentTarget = ObjectGuid::Empty;
    _combatTime = 0;

    // Log combat metrics    LogCombatMetrics();

    // Command pet to follow
    if (HasActivePet())
    {
        CommandPetFollow();    }

    // Clear trap tracking    _frozenTargets.clear();
    _activeTrapType = 0;

    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} left combat", GetBot()->GetName());    ClassAI::OnCombatEnd();
}bool HunterAI::HasEnoughResource(uint32 spellId)
{
    if (!_bot)
        return false;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());    if (!spellInfo)
        return false;

    // Check focus cost
    for (SpellPowerEntry const* power : spellInfo->PowerCosts)
    {
        if (power && power->PowerType == POWER_FOCUS)
        {

            uint32 focusCost = power->ManaCost;

            return HasFocus(focusCost);
        }
    }

    // Default for abilities without focus cost
    return true;
}

void HunterAI::ConsumeResource(uint32 spellId)
{
    if (!_bot)
        return;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());    if (!spellInfo)
        return;

    // Track focus consumption
    for (SpellPowerEntry const* power : spellInfo->PowerCosts)
    {
        if (power && power->PowerType == POWER_FOCUS)
        {

            _combatMetrics.focusSpent += power->ManaCost;

            break;
        }
    }

    // Track shot statistics
    _combatMetrics.shotsLanded++;
}

Position HunterAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)        return _bot ? _bot->GetPosition() : Position();

    // Use combat behavior integration for positioning
    if (_combatBehaviors)
        return _combatBehaviors->GetOptimalPosition();

    // Fallback to preferred range
    float angle = _bot->GetAbsoluteAngle(target);    float distance = OPTIMAL_RANGE_PREFERRED;

    Position pos;
    pos.m_positionX = target->GetPositionX() - distance * cos(angle);    pos.m_positionY = target->GetPositionY() - distance * sin(angle);    pos.m_positionZ = target->GetPositionZ();
        pos.SetOrientation(target->GetOrientation());

    return pos;
}

float HunterAI::GetOptimalRange(::Unit* target)
{
    if (!target)
        return OPTIMAL_RANGE_PREFERRED;

    // Use combat behavior integration
    if (_combatBehaviors)
        return _combatBehaviors->GetOptimalRange(target);

    // Kiting range if target is dangerous
    if (NeedsToKite(target))
        return KITING_RANGE;

    return OPTIMAL_RANGE_PREFERRED;
}

HunterSpec HunterAI::GetCurrentSpecialization() const
{
    if (!_bot)
        return HunterSpec::BEAST_MASTERY;

    // Use TrinityCore's GetPrimarySpecialization to determine specialization
    // 253 = Beast Mastery, 254 = Marksmanship, 255 = Survival
    uint32 spec = static_cast<uint32>(_bot->GetPrimarySpecialization());

    switch (spec)
    {
        case 253: // Beast Mastery

            return HunterSpec::BEAST_MASTERY;
        case 254: // Marksmanship

            return HunterSpec::MARKSMANSHIP;
        case 255: // Survival

            return HunterSpec::SURVIVAL;
        default:

            return HunterSpec::BEAST_MASTERY;
    }
}

Pet* HunterAI::GetPet() const
{
    if (!_bot)
        return nullptr;
    return _bot->GetPet();
}

bool HunterAI::HasActivePet() const
{
    Pet* pet = GetPet();
    return pet && pet->IsAlive();
}

// Pet management implementation
void HunterAI::UpdatePetStatus()
{
    Pet* pet = GetPet();
    if (pet)
    {
        _petGuid = pet->GetGUID();
        _petNeedsHeal = pet->GetHealthPct() < PET_HEAL_THRESHOLD;
    }
    else
    {
        _petGuid.Clear();
        _petNeedsHeal = false;
    }
}

bool HunterAI::NeedsPetRevive() const
{
    Pet* pet = GetPet();
    return pet && !pet->IsAlive();
}

bool HunterAI::NeedsPetHeal() const
{
    return _petNeedsHeal;
}

bool HunterAI::ShouldDismissPet() const
{
    // Dismiss pet in specific situations (e.g., stealth required)
    return false;
}

void HunterAI::CommandPetAttack(::Unit* target)
{
    if (!target || !HasActivePet())
        return;

    Pet* pet = GetPet();    if (pet)
    {
        pet->AI()->AttackStart(target);
        _lastPetCommand = GameTime::GetGameTimeMS();
    }
}

void HunterAI::CommandPetFollow()
{
    Pet* pet = GetPet();
    if (pet)
    {
        pet->GetMotionMaster()->MoveFollow(_bot, 2.0f, M_PI);
        _lastPetCommand = GameTime::GetGameTimeMS();
    }
}

void HunterAI::CommandPetStay()
{
    Pet* pet = GetPet();
    if (pet)
    {
        pet->StopMoving();
        pet->GetMotionMaster()->Clear();
        _lastPetCommand = GameTime::GetGameTimeMS();
    }
}

bool HunterAI::IsPetInCombat() const
{
    Pet* pet = GetPet();
    return pet && pet->IsInCombat();
}

float HunterAI::GetPetHealthPercent() const
{
    Pet* pet = GetPet();    return pet ? pet->GetHealthPct() : 0.0f;
}void HunterAI::HealPet()
{    if (!_bot->HasSpell(MEND_PET) || !CanUseAbility(MEND_PET))        return;    Pet* pet = GetPet();
    if (pet && pet->IsAlive())
    {        _bot->CastSpell(pet, MEND_PET, false);
    }
}

void HunterAI::RevivePet()
{
    
    if (!_bot->HasSpell(REVIVE_PET) || !CanUseAbility(REVIVE_PET))
        return;    _bot->CastSpell(_bot, REVIVE_PET, false);
}

void HunterAI::CallPet()
{    if (!_bot->HasSpell(CALL_PET) || !CanUseAbility(CALL_PET))
        return;    _bot->CastSpell(_bot, CALL_PET, false);
}

// Trap management implementation
bool HunterAI::CanPlaceTrap() const
{
    uint32 now = GameTime::GetGameTimeMS();
    return (now - _lastTrapPlacement) > 30000; // 30 second cooldown
}

bool HunterAI::ShouldPlaceFreezingTrap(::Unit* target) const
{    if (!target || !CanPlaceTrap())
        return false;

    // Place freezing trap for dangerous adds or CC targets
    return IsTargetDangerous(target) && GetDistanceToTarget(target) < TRAP_PLACEMENT_RANGE;
}

bool HunterAI::ShouldPlaceExplosiveTrap() const
{
    if (!CanPlaceTrap())
        return false;

    // Place explosive trap when multiple enemies nearby
    return GetNearbyEnemyCount(10.0f) >= 3;
}bool HunterAI::ShouldPlaceSnakeTrap() const
{
    if (!CanPlaceTrap())
        return false;

    // Place snake trap for slowing multiple enemies
    return GetNearbyEnemyCount(10.0f) >= 2;
}

void HunterAI::PlaceTrap(uint32 trapSpell, const Position& pos)
{    if (!_bot->HasSpell(trapSpell) || !CanUseAbility(trapSpell))
        return;

    // Note: Ground-targeted spells need special handling    _bot->CastSpell(_bot, trapSpell, false);
    _lastTrapPosition = pos;
    _activeTrapType = trapSpell;
    RecordTrapPlacement(trapSpell);
}

uint32 HunterAI::GetBestTrapForSituation() const
{
    if (ShouldPlaceFreezingTrap(nullptr))
        return FREEZING_TRAP;
    if (ShouldPlaceExplosiveTrap())
        return 13813; // Explosive Trap
    if (ShouldPlaceSnakeTrap())
        return SNAKE_TRAP;
    return FREEZING_TRAP; // Default
}// Range management implementation
bool HunterAI::IsInOptimalRange(::Unit* target) const
{
    if (!target)
        return false;

    float distance = GetDistanceToTarget(target);
    return distance >= OPTIMAL_RANGE_MIN && distance <= OPTIMAL_RANGE_MAX;
}

bool HunterAI::IsInDeadZone(::Unit* target) const
{
    if (!target)
    {        // Check if any enemy is in dead zone
        float minDistance = 100.0f;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, DEAD_ZONE_MAX);
        Trinity::UnitLastSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, target, u_check);
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = _bot->GetMap();    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)

            return false;
    }    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        _bot->GetPosition(), DEAD_ZONE_MAX);

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        // PHASE 5F: Thread-safe spatial grid validation
        auto snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, guid);
        Creature* entity = nullptr;
        if (snapshot_entity)
        {

        }
        if (!entity)

            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix

        if (target)
        {

            minDistance = std::sqrt(_bot->GetExactDistSq(target)); // Calculate once from squared distance
            }
        return minDistance > DEAD_ZONE_MIN && minDistance <= DEAD_ZONE_MAX;
    }

    float distance = GetDistanceToTarget(target);
    return distance > DEAD_ZONE_MIN && distance <= DEAD_ZONE_MAX;
}

bool HunterAI::NeedsToKite(::Unit* target) const
{
    if (!target)
        return false;

    // Kite if target is melee and dangerous
    float kiteSq = 10.0f * 10.0f; // 100.0f
    return target->GetExactDistSq(_bot) < kiteSq && IsTargetDangerous(target);
}

void HunterAI::MaintainRange(::Unit* target){
    if (!target)
        return;

    float distance = GetDistanceToTarget(target);

    if (distance < OPTIMAL_RANGE_MIN)
    {
        // Too close, move back
        Position pos = GetOptimalPosition(target);

        // PHASE 6C: Use Movement Arbiter with ROLE_POSITIONING priority (170)
        BotAI* botAI = dynamic_cast<BotAI*>(_bot->GetAI());        if (botAI && botAI->GetMovementArbiter())
        {

            botAI->RequestPointMovement(

                PlayerBotMovementPriority::ROLE_POSITIONING,

                pos,

                "Hunter optimal range positioning (too close)",

                "HunterAI");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available

            _bot->GetMotionMaster()->MovePoint(0, pos);
        }
    }
    else if (distance > OPTIMAL_RANGE_MAX)
    {
        // Too far, move closer
        // CRITICAL FIX: Only issue MoveChase if not already chasing
        // Re-issuing MoveChase causes speed multiplication bug
        MotionMaster* mm = _bot->GetMotionMaster();
        if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) != CHASE_MOTION_TYPE)
        {

            mm->MoveChase(target, OPTIMAL_RANGE_PREFERRED);
        }
    }
}

float HunterAI::GetDistanceToTarget(::Unit* target) const
{
    if (!target || !_bot)
        return 0.0f;
    return std::sqrt(_bot->GetExactDistSq(target)); // Calculate once from squared distance
}

// Hunter-specific mechanics implementation
void HunterAI::ManageAspects()
{
    if (!_bot)
        return;

    uint32 currentAspect = GetCurrentAspect();
    uint32 optimalAspect = GetOptimalAspect();

    if (currentAspect != optimalAspect)
    {
        uint32 now = GameTime::GetGameTimeMS();
        if (now - _lastAspectSwitch > 1000) // 1 second GCD

        {
        if (_bot->HasSpell(optimalAspect) && CanUseAbility(optimalAspect))

            {

                CastSpell(optimalAspect);

                _lastAspectSwitch = now;

            }
        }
    }
}

void HunterAI::UpdateTracking()
{
    if (!_bot)
        return;

    // Check if we already have tracking active - don't spam tracking changes
    uint32 currentTracking = 0;
    if (_bot->HasAura(TRACK_BEASTS)) currentTracking = TRACK_BEASTS;
    else if (_bot->HasAura(TRACK_DEMONS)) currentTracking = TRACK_DEMONS;
    else if (_bot->HasAura(TRACK_DRAGONKIN)) currentTracking = TRACK_DRAGONKIN;
    else if (_bot->HasAura(TRACK_ELEMENTALS)) currentTracking = TRACK_ELEMENTALS;
    else if (_bot->HasAura(TRACK_GIANTS)) currentTracking = TRACK_GIANTS;
    else if (_bot->HasAura(TRACK_HUMANOIDS)) currentTracking = TRACK_HUMANOIDS;
    else if (_bot->HasAura(TRACK_UNDEAD)) currentTracking = TRACK_UNDEAD;
    else if (_bot->HasAura(TRACK_HIDDEN)) currentTracking = TRACK_HIDDEN;

    // Determine the best tracking based on current target and nearby enemies
    uint32 optimalTracking = 0;

    // Priority 1: If we have a current target, use tracking appropriate for its creature type
    if (Unit* target = _bot->GetSelectedUnit())
    {
        if (target->GetTypeId() == TYPEID_UNIT)
        {

            Creature* creature = target->ToCreature();

            if (creature)

            {

                CreatureType creatureType = creature->GetCreatureTemplate()->type;


                switch (creatureType)

                {

                    case CREATURE_TYPE_BEAST:

                        if (_bot->HasSpell(TRACK_BEASTS))

                            optimalTracking = TRACK_BEASTS;

                        break;

                    case CREATURE_TYPE_DEMON:

                        if (_bot->HasSpell(TRACK_DEMONS))

                            optimalTracking = TRACK_DEMONS;

                        break;

                    case CREATURE_TYPE_DRAGONKIN:

                        if (_bot->HasSpell(TRACK_DRAGONKIN))

                            optimalTracking = TRACK_DRAGONKIN;

                        break;

                    case CREATURE_TYPE_ELEMENTAL:

                        if (_bot->HasSpell(TRACK_ELEMENTALS))

                            optimalTracking = TRACK_ELEMENTALS;

                        break;

                    case CREATURE_TYPE_GIANT:

                        if (_bot->HasSpell(TRACK_GIANTS))

                            optimalTracking = TRACK_GIANTS;

                        break;

                    case CREATURE_TYPE_HUMANOID:

                        if (_bot->HasSpell(TRACK_HUMANOIDS))

                            optimalTracking = TRACK_HUMANOIDS;

                        break;

                    case CREATURE_TYPE_UNDEAD:

                        if (_bot->HasSpell(TRACK_UNDEAD))

                            optimalTracking = TRACK_UNDEAD;

                        break;

                    default:
                        // For other types, default to humanoid tracking if in PvP zone

                        if (_bot->IsInPvPArea() && _bot->HasSpell(TRACK_HUMANOIDS))

                            optimalTracking = TRACK_HUMANOIDS;

                        break;

                }

            }
        }
        else if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // Tracking players with Track Humanoids

            if (_bot->HasSpell(TRACK_HUMANOIDS))

                optimalTracking = TRACK_HUMANOIDS;
        }
    }

    // Priority 2: If no specific target, analyze nearby creatures
    if (optimalTracking == 0)
    {
        std::unordered_map<CreatureType, uint32> creatureTypeCounts;

        // Count nearby creature types within 40 yards
        std::list<Creature*> nearbyCreatures;
        Trinity::AllCreaturesInRange check(_bot, 40.0f);
        Trinity::CreatureListSearcher<Trinity::AllCreaturesInRange> searcher(_bot, nearbyCreatures, check);
        Cell::VisitGridObjects(_bot, searcher, 40.0f);

        for (Creature* creature : nearbyCreatures)
        {

            if (!creature || creature->IsFriendlyTo(_bot))

                continue;


            CreatureType type = creature->GetCreatureTemplate()->type;

            creatureTypeCounts[type]++;
        }

        // Select tracking for the most common nearby creature type
        uint32 maxCount = 0;
        CreatureType mostCommonType = CREATURE_TYPE_NON_COMBAT_PET;

        for (const auto& [type, count] : creatureTypeCounts)
        {

            if (count > maxCount)

            {

                maxCount = count;

                mostCommonType = type;

            }
        }

        // Apply tracking for most common type
        if (maxCount > 0)
        {

            switch (mostCommonType)

            {

                case CREATURE_TYPE_BEAST:

                    if (_bot->HasSpell(TRACK_BEASTS))

                        optimalTracking = TRACK_BEASTS;

                    break;

                case CREATURE_TYPE_DEMON:

                    if (_bot->HasSpell(TRACK_DEMONS))

                        optimalTracking = TRACK_DEMONS;

                    break;

                case CREATURE_TYPE_DRAGONKIN:

                    if (_bot->HasSpell(TRACK_DRAGONKIN))

                        optimalTracking = TRACK_DRAGONKIN;

                    break;

                case CREATURE_TYPE_ELEMENTAL:

                    if (_bot->HasSpell(TRACK_ELEMENTALS))

                        optimalTracking = TRACK_ELEMENTALS;

                    break;

                case CREATURE_TYPE_GIANT:

                    if (_bot->HasSpell(TRACK_GIANTS))

                        optimalTracking = TRACK_GIANTS;

                    break;

                case CREATURE_TYPE_HUMANOID:

                    if (_bot->HasSpell(TRACK_HUMANOIDS))

                        optimalTracking = TRACK_HUMANOIDS;

                    break;

                case CREATURE_TYPE_UNDEAD:

                    if (_bot->HasSpell(TRACK_UNDEAD))

                        optimalTracking = TRACK_UNDEAD;

                    break;

                default:

                    break;

            }
        }
    }

    // Priority 3: Default to appropriate tracking based on zone type
    if (optimalTracking == 0)
    {
        // In dungeons/raids, prioritize Track Hidden for stealth detection
        if (_bot->GetMap()->IsDungeon() && _bot->HasSpell(TRACK_HIDDEN))
        {

            optimalTracking = TRACK_HIDDEN;
        }
        // In PvP zones, track humanoids
        else if (_bot->IsInPvPArea() && _bot->HasSpell(TRACK_HUMANOIDS))
        {

            optimalTracking = TRACK_HUMANOIDS;
        }
        // Default to beast tracking in open world
        else if (_bot->HasSpell(TRACK_BEASTS))
        {

            optimalTracking = TRACK_BEASTS;
        }
    }

    // Apply the optimal tracking if it's different from current
    if (optimalTracking != 0 && optimalTracking != currentTracking)
    {
        if (CastSpell(_bot, optimalTracking))
        {

            TC_LOG_DEBUG("module.playerbot.hunter", "Hunter {} switched tracking to spell {}",

                         _bot->GetName(), optimalTracking);
        }
    }
}

bool HunterAI::HasAnyAspect()
{
    return HasAura(ASPECT_OF_THE_HAWK) ||
           HasAura(ASPECT_OF_THE_CHEETAH) ||
           HasAura(ASPECT_OF_THE_PACK) ||
           HasAura(ASPECT_OF_THE_VIPER) ||
           HasAura(ASPECT_OF_THE_DRAGONHAWK) ||
           HasAura(ASPECT_OF_THE_TURTLE);
}

uint32 HunterAI::GetCurrentAspect()
{
    if (HasAura(ASPECT_OF_THE_DRAGONHAWK)) return ASPECT_OF_THE_DRAGONHAWK;
    if (HasAura(ASPECT_OF_THE_HAWK)) return ASPECT_OF_THE_HAWK;
    if (HasAura(ASPECT_OF_THE_CHEETAH)) return ASPECT_OF_THE_CHEETAH;
    if (HasAura(ASPECT_OF_THE_PACK)) return ASPECT_OF_THE_PACK;
    if (HasAura(ASPECT_OF_THE_VIPER)) return ASPECT_OF_THE_VIPER;
    if (HasAura(ASPECT_OF_THE_TURTLE)) return ASPECT_OF_THE_TURTLE;
    return 0;
}

void HunterAI::SwitchToCombatAspect()
{
    uint32 combatAspect = _bot->HasSpell(ASPECT_OF_THE_DRAGONHAWK) ? ASPECT_OF_THE_DRAGONHAWK : ASPECT_OF_THE_HAWK;    if (!HasAura(combatAspect) && _bot->HasSpell(combatAspect))
    {
        CastSpell(combatAspect);
        _lastAspectSwitch = GameTime::GetGameTimeMS();
    }
}

void HunterAI::SwitchToMovementAspect()
{
    uint32 moveAspect = _bot->HasSpell(ASPECT_OF_THE_CHEETAH) ? ASPECT_OF_THE_CHEETAH : ASPECT_OF_THE_PACK;    if (!HasAura(moveAspect) && _bot->HasSpell(moveAspect))
    {
        CastSpell(moveAspect);
        _lastAspectSwitch = GameTime::GetGameTimeMS();
    }
}

bool HunterAI::ValidateAspectForAbility(uint32 spellId) const
{
    // Some abilities require specific aspects
    return true;
}

uint32 HunterAI::GetOptimalAspect() const
{
    if (_bot->IsInCombat())
    {
        // Low on focus? Use Viper        if (GetFocusPercent() < 30.0f && _bot->HasSpell(ASPECT_OF_THE_VIPER))

            return ASPECT_OF_THE_VIPER;

        // Normal combat aspect        return _bot->HasSpell(ASPECT_OF_THE_DRAGONHAWK) ? ASPECT_OF_THE_DRAGONHAWK : ASPECT_OF_THE_HAWK;
    }
    else
    {
        // Out of combat movement        return _bot->HasSpell(ASPECT_OF_THE_CHEETAH) ? ASPECT_OF_THE_CHEETAH : ASPECT_OF_THE_PACK;
    }
}

// Utility method implementations
bool HunterAI::ShouldFeignDeath() const
{
    // Feign death to drop aggro or escape
    return _bot->GetHealthPct() < FEIGN_DEATH_THRESHOLD || _bot->GetThreatManager().GetThreatListSize() > 3;
}

bool HunterAI::CanInterruptTarget(::Unit* target) const
{
    if (!target)
        return false;

    // Check if target is casting an interruptible spell
    return target->IsNonMeleeSpellCast(false, false, true);
}

::Unit* HunterAI::GetBestCrowdControlTarget() const
{
    ::Unit* bestTarget = nullptr;
    float lowestHealth = 100.0f;

    // Find best CC target (lowest health add that's not the main target)
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = _bot->GetMap();    if (!map)
        return nullptr;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)

            return nullptr;
    }

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        _bot->GetPosition(), 30.0f);

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        // PHASE 5F: Thread-safe spatial grid validation
        auto snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, guid);
        Creature* entity = nullptr;
        if (snapshot_entity)
        {

        }
        if (!entity)

            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix

    // Find the target with lowest health that's not the current main target
    Unit* currentTarget = _bot->GetVictim();
    for (Unit* unit : targets)
    {
        if (unit && unit != currentTarget && unit->IsAlive())
        {

            float healthPct = unit->GetHealthPct();

            if (healthPct < lowestHealth)

            {

                lowestHealth = healthPct;

                bestTarget = unit;

            }
        }
    }

    return bestTarget;
}

uint32 HunterAI::GetNearbyEnemyCount(float range) const
{
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = _bot->GetMap();    if (!map)
        return 0;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)

            return 0;
    }

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        _bot->GetPosition(), range);

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        // PHASE 5F: Thread-safe spatial grid validation
        auto snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, guid);
        Creature* entity = nullptr;
        if (snapshot_entity)
        {

        }
        if (!entity)

            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix
    return targets.size();
}

bool HunterAI::HasFocus(uint32 amount) const
{
    return _bot && _bot->GetPower(POWER_FOCUS) >= amount;}

uint32 HunterAI::GetFocus() const
{
    return _bot ? _bot->GetPower(POWER_FOCUS) : 0;
}

uint32 HunterAI::GetMaxFocus() const
{
    return _bot ? _bot->GetMaxPower(POWER_FOCUS) : 0;
}

float HunterAI::GetFocusPercent() const
{
    uint32 maxFocus = GetMaxFocus();
    return maxFocus > 0 ? (static_cast<float>(GetFocus()) / maxFocus) * 100.0f : 0.0f;
}

void HunterAI::LogCombatMetrics()
{
    if (_updateCounter == 0)
        return;

    TC_LOG_DEBUG("module.playerbot.ai",

                 "Hunter {} combat ended - Shots: {}/{}, Crits: {}, Interrupts: {}, Traps: {}, Pet Commands: {}, "

                 "Focus Spent: {}, Damage: {}, Time at Range: {:.1f}s, Time in Dead Zone: {:.1f}s, "

                 "Avg Update: {} us, Peak: {} us",

                 GetBot()->GetName(),

                 uint32(_combatMetrics.shotsLanded.load()),

                 uint32(_combatMetrics.shotsMissed.load()),

                 uint32(_combatMetrics.criticalStrikes.load()),

                 uint32(_combatMetrics.interrupts.load()),

                 uint32(_combatMetrics.trapsTriggered.load()),

                 uint32(_combatMetrics.petCommands.load()),

                 uint32(_combatMetrics.focusSpent.load()),

                 uint32(_combatMetrics.damageDealt.load()),

                 _combatMetrics.timeAtRange.load(),

                 _combatMetrics.timeInDeadZone.load(),

                 _totalUpdateTime / _updateCounter,

                 _peakUpdateTime);
}

Player* HunterAI::GetMainTank()
{    if (!_bot->GetGroup())
        return nullptr;

    Group* group = _bot->GetGroup();    Player* tank = nullptr;

    // Find tank by looking for warriors/paladins/death knights with tank specs
    // In TrinityCore, we check class + role based on talents/gear
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (member)
        {

            uint8 playerClass = member->GetClass();
            // Typical tank classes: Warrior, Paladin, Death Knight, Druid (in bear form)

            if (playerClass == CLASS_WARRIOR || playerClass == CLASS_PALADIN ||

                playerClass == CLASS_DEATH_KNIGHT || playerClass == CLASS_DRUID)

            {

                tank = member;

                break; // Use first potential tank found

            }
        }
    }

    return tank;
}

bool HunterAI::IsTargetDangerous(::Unit* target) const
{
    if (!target)
        return false;

    // Check if target is elite or boss
    if (target->GetCreatureType() == CREATURE_TYPE_BEAST && target->GetLevel() > _bot->GetLevel() + 2)        return true;

    // Check if target has high damage output
    return target->GetTotalAttackPowerValue(BASE_ATTACK) > _bot->GetMaxHealth() * 0.3f;
}

bool HunterAI::ShouldSaveDefensives() const
{
    // Save defensives for boss fights or multiple enemies
    return GetNearbyEnemyCount(20.0f) > 1 || (_currentTarget.IsCreature() &&
           ObjectAccessor::GetCreature(*_bot, _currentTarget) &&
           ObjectAccessor::GetCreature(*_bot, _currentTarget)->isWorldBoss());
}

void HunterAI::RecordShotResult(bool hit, bool crit)
{
    if (hit)
    {
        _combatMetrics.shotsLanded++;
        if (crit)

            _combatMetrics.criticalStrikes++;
    }
    else
    {
        _combatMetrics.shotsMissed++;
    }
}

void HunterAI::RecordTrapPlacement(uint32 trapSpell)
{
    _combatMetrics.trapsTriggered++;
    TC_LOG_DEBUG("module.playerbot.ai", "Hunter {} placed trap type {}",

                 GetBot()->GetName(), trapSpell);
}

} // namespace Playerbot