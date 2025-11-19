/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DungeonBehavior.h"
#include "InstanceCoordination.h"
#include "EncounterStrategy.h"
#include "../Group/GroupFormation.h"
#include "../Group/RoleAssignment.h"
#include "Player.h"
#include "Group.h"
#include "Map.h"
#include "InstanceScript.h"
#include "Creature.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "DB2Stores.h"
#include "MotionMaster.h"
#include <algorithm>
#include <cmath>
#include "../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "../Movement/Arbiter/MovementArbiter.h"
#include "../Movement/Arbiter/MovementPriorityMapper.h"
#include "../AI/BotAI.h"
#include "UnitAI.h"

namespace Playerbot
{

// Singleton instance
DungeonBehavior* DungeonBehavior::instance()
{
    static DungeonBehavior instance;
    return &instance;
}

DungeonBehavior::DungeonBehavior()
{
    TC_LOG_INFO("server.loading", "Initializing DungeonBehavior system...");
    InitializeDungeonDatabase();
    TC_LOG_INFO("server.loading", "DungeonBehavior system initialized with {} dungeons", _dungeonDatabase.size());
}

// ============================================================================
// CORE DUNGEON MANAGEMENT
// ============================================================================

bool DungeonBehavior::EnterDungeon(Group* group, uint32 dungeonId)
{

    std::lock_guard lock(_dungeonMutex);

    // Check if dungeon data exists
    auto dungeonItr = _dungeonDatabase.find(dungeonId);
    if (dungeonItr == _dungeonDatabase.end())
    {
        TC_LOG_ERROR("module.playerbot", "DungeonBehavior::EnterDungeon - Unknown dungeon ID {}", dungeonId);
        return false;
    }

    DungeonData const& dungeonData = dungeonItr->second;

    // Validate group size
    if (group->GetMembersCount() < dungeonData.recommendedGroupSize)
    {
        TC_LOG_WARN("module.playerbot", "DungeonBehavior::EnterDungeon - Group {} entering {} with only {} members (recommended: {})",
            group->GetGUID().GetCounter(), dungeonData.dungeonName, group->GetMembersCount(), dungeonData.recommendedGroupSize);
    }

    // Create group dungeon state
    GroupDungeonState state(group->GetGUID().GetCounter(), dungeonId);
    state.totalEncounters = static_cast<uint32>(dungeonData.encounters.size());
    state.currentPhase = DungeonPhase::ENTERING;
    state.activeStrategy = dungeonData.encounters.empty() ? EncounterStrategy::BALANCED :
                           dungeonData.encounters[0].recommendedStrategy;

    _groupDungeonStates[group->GetGUID().GetCounter()] = state;

    // Initialize metrics for this group
    if (_groupMetrics.find(group->GetGUID().GetCounter()) == _groupMetrics.end())
    {
        _groupMetrics[group->GetGUID().GetCounter()] = DungeonMetrics();
    }

    _groupMetrics[group->GetGUID().GetCounter()].dungeonsAttempted++;
    _globalMetrics.dungeonsAttempted++;

    // Initialize instance coordination
    if (group->GetInstanceScript())
    {
        InstanceCoordination::instance()->InitializeInstanceCoordination(group, group->GetInstanceScript()->instance);
    }

    TC_LOG_INFO("module.playerbot", "Group {} entered dungeon: {} (ID: {})",
        group->GetGUID().GetCounter(), dungeonData.dungeonName, dungeonId);

    // Set initial strategy for group
    SetEncounterStrategy(group->GetGUID().GetCounter(), state.activeStrategy);

    // Begin dungeon progression
    UpdateDungeonProgress(group);

    return true;
}

void DungeonBehavior::UpdateDungeonProgress(Group* group)
{
    if (!group)
        return;

    std::lock_guard lock(_dungeonMutex);

    auto stateItr = _groupDungeonStates.find(group->GetGUID().GetCounter());
    if (stateItr == _groupDungeonStates.end())
        return;

    GroupDungeonState& state = stateItr->second;
    state.lastProgressTime = GameTime::GetGameTimeMS();

    // Check for stuck detection
    Position currentPos = CalculateGroupCenterPoint(group);
    if (state.lastGroupPosition.GetExactDist(&currentPos) < 1.0f)
    {
        state.stuckTime += (GameTime::GetGameTimeMS() - state.lastProgressTime);
        if (state.stuckTime > STUCK_DETECTION_TIME)
        {
            state.isStuck = true;
            TC_LOG_WARN("module.playerbot", "Group {} appears stuck in dungeon (no movement for {} seconds)",
                group->GetGUID().GetCounter(), STUCK_DETECTION_TIME / 1000);
        }
    }
    else
    {
        state.stuckTime = 0;
        state.isStuck = false;
    }

    state.lastGroupPosition = currentPos;

    // Update phase based on current state
    switch (state.currentPhase)
    {
        case DungeonPhase::ENTERING:
            // Transition to clearing trash after entry
            if (GameTime::GetGameTimeMS() - state.startTime > 30000) // 30 seconds after entering
            {
                state.currentPhase = DungeonPhase::CLEARING_TRASH;
                TC_LOG_DEBUG("module.playerbot", "Group {} transitioned to CLEARING_TRASH phase", group->GetGUID().GetCounter());
            }
            break;

        case DungeonPhase::CLEARING_TRASH:
            // Check if next boss encounter should begin
            if (state.encountersCompleted < state.totalEncounters)
            {
                // Transition to boss encounter if group is ready
                if (!group->IsInCombat())
                {
                    uint32 nextEncounterId = state.encountersCompleted;
                    auto dungeonData = GetDungeonData(state.dungeonId);
                    if (nextEncounterId < dungeonData.encounters.size())
                    {
                        DungeonEncounter const& nextEncounter = dungeonData.encounters[nextEncounterId];

                        // Check if group is near encounter location
                        if (currentPos.GetExactDist(&nextEncounter.encounterLocation) < 50.0f)
                        {
                            StartEncounter(group, nextEncounter.encounterId);
                        }
                    }
                }
            }
            else
            {
                state.currentPhase = DungeonPhase::COMPLETED;
                HandleDungeonCompletion(group);
            }
            break;

        case DungeonPhase::BOSS_ENCOUNTER:
            // Monitor encounter progress (handled by encounter update functions)
            break;

        case DungeonPhase::LOOTING:
            // Transition back to clearing trash after looting
            if (!group->IsInCombat() && GameTime::GetGameTimeMS() - state.lastProgressTime > 15000)
            {
                state.currentPhase = DungeonPhase::CLEARING_TRASH;
            }
            break;

        case DungeonPhase::RESTING:
            // Transition back to clearing after rest break
            if (!group->IsInCombat() && GameTime::GetGameTimeMS() - state.lastProgressTime > 30000)
            {
                state.currentPhase = DungeonPhase::CLEARING_TRASH;
            }
            break;

        case DungeonPhase::COMPLETED:
            // Dungeon completed, cleanup
            break;

        case DungeonPhase::WIPED:
            // Handle wipe recovery
            if (GameTime::GetGameTimeMS() - state.lastProgressTime > WIPE_RECOVERY_TIME)
            {
                RecoverFromWipe(group);
            }
            break;
    }

    // Update instance coordination
    InstanceCoordination::instance()->UpdateInstanceCoordination(group, 1000);
}

void DungeonBehavior::HandleDungeonCompletion(Group* group)
{
    if (!group)
        return;

    std::lock_guard lock(_dungeonMutex);

    auto stateItr = _groupDungeonStates.find(group->GetGUID().GetCounter());
    if (stateItr == _groupDungeonStates.end())
        return;

    GroupDungeonState& state = stateItr->second;
    state.currentPhase = DungeonPhase::COMPLETED;

    uint32 completionTime = GameTime::GetGameTimeMS() - state.startTime;

    // Update metrics
    _groupMetrics[group->GetGUID().GetCounter()].dungeonsCompleted++;
    _globalMetrics.dungeonsCompleted++;

    // Update average completion time
    auto& metrics = _groupMetrics[group->GetGUID().GetCounter()];
    float currentAvg = metrics.averageCompletionTime.load();
    float newAvg = (currentAvg + completionTime) / 2.0f;
    metrics.averageCompletionTime = newAvg;

    auto dungeonData = GetDungeonData(state.dungeonId);

    TC_LOG_INFO("module.playerbot", "Group {} completed dungeon: {} in {} minutes ({} wipes)",
        group->GetGUID().GetCounter(), dungeonData.dungeonName,
        completionTime / 60000, state.wipeCount);

    // Notify instance coordination
    InstanceCoordination::instance()->HandleInstanceCompletion(group);

    LogDungeonEvent(group->GetGUID().GetCounter(), "DUNGEON_COMPLETED",
        Trinity::StringFormat("Dungeon: {}, Time: {}ms, Wipes: {}",
            dungeonData.dungeonName, completionTime, state.wipeCount));
}

void DungeonBehavior::HandleDungeonWipe(Group* group)
{
    if (!group)
        return;

    std::lock_guard lock(_dungeonMutex);

    auto stateItr = _groupDungeonStates.find(group->GetGUID().GetCounter());
    if (stateItr == _groupDungeonStates.end())
        return;

    GroupDungeonState& state = stateItr->second;
    state.currentPhase = DungeonPhase::WIPED;
    state.wipeCount++;

    // Update metrics
    _groupMetrics[group->GetGUID().GetCounter()].encounterWipes++;
    _globalMetrics.encounterWipes++;

    TC_LOG_INFO("module.playerbot", "Group {} wiped in dungeon (wipe count: {})",
        group->GetGUID().GetCounter(), state.wipeCount);

    // Notify instance coordination
    InstanceCoordination::instance()->HandleInstanceFailure(group);

    // If too many wipes on same encounter, adapt strategy
    if (state.wipeCount >= MAX_ENCOUNTER_RETRIES)
    {
        AdaptStrategyBasedOnPerformance(group);
        state.wipeCount = 0; // Reset counter after adaptation
    }

    LogDungeonEvent(group->GetGUID().GetCounter(), "DUNGEON_WIPE",
        Trinity::StringFormat("Total wipes: {}, Current encounter: {}",
            state.wipeCount, state.currentEncounterId));
}

// ============================================================================
// ENCOUNTER MANAGEMENT
// ============================================================================

void DungeonBehavior::StartEncounter(Group* group, uint32 encounterId)
{
    if (!group)
        return;

    std::lock_guard lock(_dungeonMutex);

    auto stateItr = _groupDungeonStates.find(group->GetGUID().GetCounter());
    if (stateItr == _groupDungeonStates.end())
        return;

    GroupDungeonState& state = stateItr->second;
    state.currentPhase = DungeonPhase::BOSS_ENCOUNTER;
    state.currentEncounterId = encounterId;

    _encounterProgress[group->GetGUID().GetCounter()] = encounterId;
    _encounterStartTime[group->GetGUID().GetCounter()] = GameTime::GetGameTimeMS();

    DungeonEncounter encounter = GetEncounterData(encounterId);

    TC_LOG_INFO("module.playerbot", "Group {} starting encounter: {} (ID: {})",
        group->GetGUID().GetCounter(), encounter.encounterName, encounterId);

    // Prepare group for encounter
    InstanceCoordination::instance()->PrepareForEncounter(group, encounterId);

    // Execute encounter strategy
    EncounterStrategy::instance()->ExecuteEncounterStrategy(group, encounterId);

    // Set up boss-specific behavior
    ExecuteBossStrategy(group, encounter);

    LogDungeonEvent(group->GetGUID().GetCounter(), "ENCOUNTER_START", encounter.encounterName);
}

void DungeonBehavior::UpdateEncounter(Group* group, uint32 encounterId)
{
    if (!group)
        return;

    DungeonEncounter encounter = GetEncounterData(encounterId);

    // Update encounter strategy
    EncounterStrategy::instance()->UpdateEncounterExecution(group, encounterId, 1000);

    // Monitor encounter progress
    InstanceCoordination::instance()->MonitorEncounterProgress(group, encounterId);

    // Handle enrage timer if present
    if (encounter.hasEnrageTimer)
    {
        HandleEnrageTimer(group, encounter);
    }

    // Coordinate group positioning
    UpdateGroupPositioning(group, encounter);

    // Manage threat and healing
    ManageGroupThreat(group, encounter);
    CoordinateGroupHealing(group, encounter);
    CoordinateGroupDamage(group, encounter);
}

void DungeonBehavior::CompleteEncounter(Group* group, uint32 encounterId)
{
    if (!group)
        return;

    std::lock_guard lock(_dungeonMutex);

    auto stateItr = _groupDungeonStates.find(group->GetGUID().GetCounter());
    if (stateItr == _groupDungeonStates.end())
        return;

    GroupDungeonState& state = stateItr->second;
    state.encountersCompleted++;
    state.completedEncounters.push_back(encounterId);
    state.currentPhase = DungeonPhase::LOOTING;

    DungeonEncounter encounter = GetEncounterData(encounterId);
    uint32 encounterDuration = GameTime::GetGameTimeMS() - _encounterStartTime[group->GetGUID().GetCounter()];

    // Update metrics
    _groupMetrics[group->GetGUID().GetCounter()].encountersCompleted++;
    _globalMetrics.encountersCompleted++;

    TC_LOG_INFO("module.playerbot", "Group {} completed encounter: {} in {} seconds",
        group->GetGUID().GetCounter(), encounter.encounterName, encounterDuration / 1000);

    // Handle loot distribution
    HandleEncounterLoot(group, encounterId);

    // Analyze performance and learn
    AnalyzeGroupPerformance(group, encounter);

    LogDungeonEvent(group->GetGUID().GetCounter(), "ENCOUNTER_COMPLETE",
        Trinity::StringFormat("{} ({}s)", encounter.encounterName, encounterDuration / 1000));
}

void DungeonBehavior::HandleEncounterWipe(Group* group, uint32 encounterId)
{
    if (!group)
        return;

    DungeonEncounter encounter = GetEncounterData(encounterId);

    TC_LOG_INFO("module.playerbot", "Group {} wiped on encounter: {}",
        group->GetGUID().GetCounter(), encounter.encounterName);

    // Record failed encounter
    std::lock_guard lock(_dungeonMutex);
    auto stateItr = _groupDungeonStates.find(group->GetGUID().GetCounter());
    if (stateItr != _groupDungeonStates.end())
    {
        stateItr->second.failedEncounters.push_back(encounterId);
    }

    // Trigger dungeon wipe handling
    HandleDungeonWipe(group);

    // Recover encounter mechanics
    InstanceCoordination::instance()->HandleEncounterRecovery(group, encounterId);

    LogDungeonEvent(group->GetGUID().GetCounter(), "ENCOUNTER_WIPE", encounter.encounterName);
}

// ============================================================================
// ROLE-SPECIFIC COORDINATION
// ============================================================================

void DungeonBehavior::CoordinateTankBehavior(Player* tank, const DungeonEncounter& encounter)
{
    if (!tank)
        return;

    // Get tank strategy from encounter system
    auto tankStrategy = EncounterStrategy::instance()->GetTankStrategy(encounter.encounterId, tank);
    // Execute positioning strategy
    if (tankStrategy.positioningStrategy)
    {
        tankStrategy.positioningStrategy(tank, tank->GetGroup(), encounter);
    }

    // Position tank at optimal location
    Position optimalPos = GetOptimalPosition(tank, DungeonRole::TANK, encounter);
    if (tank->GetExactDist(&optimalPos) > POSITIONING_TOLERANCE)
    {
        // PHASE 6D: Use Movement Arbiter with DUNGEON_POSITIONING priority (110)
        BotAI* botAI = dynamic_cast<BotAI*>(tank->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            botAI->RequestPointMovement(
                PlayerBotMovementPriority::DUNGEON_POSITIONING,
                optimalPos,
                "Dungeon tank positioning",
                "DungeonBehavior");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available
            tank->GetMotionMaster()->MovePoint(0, optimalPos.GetPositionX(),
                optimalPos.GetPositionY(), optimalPos.GetPositionZ());
        }
    }

    TC_LOG_TRACE("module.playerbot", "Coordinating tank {} behavior for encounter {}",
        tank->GetName(), encounter.encounterName);
}

void DungeonBehavior::CoordinateHealerBehavior(Player* healer, const DungeonEncounter& encounter)
{
    if (!healer)
        return;

    // Get healer strategy
    auto healerStrategy = EncounterStrategy::instance()->GetHealerStrategy(encounter.encounterId, healer);

    // Execute healing priority strategy
    if (healerStrategy.healingPriorityStrategy)
    {
        healerStrategy.healingPriorityStrategy(healer, healer->GetGroup(), encounter);
    }

    // Position healer safely
    Position safePos = GetOptimalPosition(healer, DungeonRole::HEALER, encounter);
    if (healer->GetExactDist(&safePos) > POSITIONING_TOLERANCE)
    {
        // PHASE 6D: Use Movement Arbiter with DUNGEON_POSITIONING priority (110)
        BotAI* botAI = dynamic_cast<BotAI*>(healer->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            botAI->RequestPointMovement(
                PlayerBotMovementPriority::DUNGEON_POSITIONING,
                safePos,
                "Dungeon healer positioning",
                "DungeonBehavior");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available
            healer->GetMotionMaster()->MovePoint(0, safePos.GetPositionX(),
                safePos.GetPositionY(), safePos.GetPositionZ());
        }
    }

    TC_LOG_TRACE("module.playerbot", "Coordinating healer {} behavior for encounter {}",
        healer->GetName(), encounter.encounterName);
}

void DungeonBehavior::CoordinateDpsBehavior(Player* dps, const DungeonEncounter& encounter)
{
    if (!dps)
        return;

    // Get DPS strategy
    auto dpsStrategy = EncounterStrategy::instance()->GetDpsStrategy(encounter.encounterId, dps);

    // Execute damage optimization strategy
    if (dpsStrategy.damageOptimizationStrategy)
    {
        dpsStrategy.damageOptimizationStrategy(dps, dps->GetGroup(), encounter);
    }

    // Position DPS optimally (melee vs ranged)
    bool isMelee = dps->GetClass() == CLASS_WARRIOR || dps->GetClass() == CLASS_ROGUE ||
                   dps->GetClass() == CLASS_DEATH_KNIGHT || dps->GetClass() == CLASS_PALADIN;

    DungeonRole role = isMelee ? DungeonRole::MELEE_DPS : DungeonRole::RANGED_DPS;
    Position optimalPos = GetOptimalPosition(dps, role, encounter);

    if (dps->GetExactDist(&optimalPos) > POSITIONING_TOLERANCE)
    {
        // PHASE 6D: Use Movement Arbiter with DUNGEON_POSITIONING priority (110)
        BotAI* botAI = dynamic_cast<BotAI*>(dps->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            botAI->RequestPointMovement(
                PlayerBotMovementPriority::DUNGEON_POSITIONING,
                optimalPos,
                "Dungeon DPS positioning",
                "DungeonBehavior");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available
            dps->GetMotionMaster()->MovePoint(0, optimalPos.GetPositionX(),
                optimalPos.GetPositionY(), optimalPos.GetPositionZ());
        }
    }

    TC_LOG_TRACE("module.playerbot", "Coordinating DPS {} behavior for encounter {}",
        dps->GetName(), encounter.encounterName);
}
void DungeonBehavior::CoordinateCrowdControlBehavior(Player* cc, const DungeonEncounter& encounter)
{
    if (!cc || !cc->GetGroup())
        return;
    // Identify targets requiring crowd control
    std::vector<Unit*> ccTargets;
    // Scan for adds or specific mechanic targets
    for (auto const& member : cc->GetGroup()->GetMemberSlots())
    {
        Player* groupMember = ObjectAccessor::FindPlayer(member.guid);
        if (!groupMember || !groupMember->IsInWorld())
            continue;

        // Look for nearby enemies that need CC
        std::list<Creature*> nearbyCreatures;
        Trinity::AnyUnitInObjectRangeCheck checker(groupMember, 40.0f, true, true);
        Trinity::CreatureListSearcher searcher(groupMember, nearbyCreatures, checker);
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = groupMember->GetMap();
    if (!map)
        return; // Adjust return value as needed

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return; // Adjust return value as needed
    }

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        groupMember->GetPosition(), 40.0f);

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        auto* entity = ObjectAccessor::GetCreature(*groupMember, guid);
        if (!entity)
            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix

        for (Creature* creature : nearbyCreatures)
        {
            if (!creature || !creature->IsAlive() || creature->IsFriendlyTo(cc))
                continue;

            // Check if this creature should be CC'd
            if (encounter.mechanics.empty())
                continue;

            ccTargets.push_back(creature);
        }
    }
    if (!ccTargets.empty())
    {
        CoordinateCrowdControl(cc->GetGroup(), ccTargets);
    }

    TC_LOG_TRACE("module.playerbot", "Coordinating CC behavior for {} ({} targets)",
        cc->GetName(), ccTargets.size());
}

// ============================================================================
// MOVEMENT AND POSITIONING
// ============================================================================

void DungeonBehavior::UpdateGroupPositioning(Group* group, const DungeonEncounter& encounter)
{
    if (!group)
        return;

    // Special positioning required?
    if (encounter.requiresSpecialPositioning)
    {
        HandleSpecialPositioning(group, encounter.encounterId);
        return;
    }

    // Standard formation positioning
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // Determine role
        DungeonRole role = DungeonRole::MELEE_DPS; // Default

        switch (player->GetClass())
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
            case CLASS_DRUID:
            case CLASS_MONK:
                role = player->GetPrimaryTalentTree() == 0 ? DungeonRole::TANK : DungeonRole::MELEE_DPS;
                break;
            case CLASS_PRIEST:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
                role = DungeonRole::HEALER;
                break;
            case CLASS_HUNTER:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                role = DungeonRole::RANGED_DPS;
                break;
            case CLASS_ROGUE:
                role = DungeonRole::MELEE_DPS;
                break;
        }

        // Get optimal position for role
        Position optimalPos = GetOptimalPosition(player, role, encounter);

        // Move if too far from optimal position
        if (player->GetExactDist(&optimalPos) > POSITIONING_TOLERANCE * 2.0f)
        {
            // PHASE 6D: Use Movement Arbiter with DUNGEON_POSITIONING priority (110)
            BotAI* botAI = dynamic_cast<BotAI*>(player->GetAI());
            if (botAI && botAI->GetMovementArbiter())
            {
                botAI->RequestPointMovement(
                    PlayerBotMovementPriority::DUNGEON_POSITIONING,
                    optimalPos,
                    "Dungeon spread positioning",
                    "DungeonBehavior");
            }
            else
            {
                // FALLBACK: Direct MotionMaster if arbiter not available
                player->GetMotionMaster()->MovePoint(0, optimalPos.GetPositionX(),
                    optimalPos.GetPositionY(), optimalPos.GetPositionZ());
            }
        }
    }
}

void DungeonBehavior::HandleSpecialPositioning(Group* group, uint32 encounterId)
{
    if (!group)
        return;

    DungeonEncounter encounter = GetEncounterData(encounterId);

    // Execute encounter-specific positioning logic
    switch (encounterId)
    {
        case 1: // Example: Deadmines VanCleef
            HandleDeadminesStrategy(group, encounterId);
            break;
        case 2: // Wailing Caverns encounters
            HandleWailingCavernsStrategy(group, encounterId);
            break;
        case 3: // Shadowfang Keep
            HandleShadowfangKeepStrategy(group, encounterId);
            break;
        default:
            // Use generic positioning
            UpdateGroupPositioning(group, encounter);
            break;
    }
}

Position DungeonBehavior::GetOptimalPosition(Player* player, DungeonRole role, const DungeonEncounter& encounter)
{
    Position optimalPos = encounter.encounterLocation;

    switch (role)
    {
        case DungeonRole::TANK:
            optimalPos = CalculateTankPosition(encounter, {});
            break;
        case DungeonRole::HEALER:
            optimalPos = CalculateHealerPosition(encounter, {});
            break;
        case DungeonRole::MELEE_DPS:
            optimalPos = CalculateDpsPosition(encounter, nullptr);
            optimalPos.RelocateOffset({0.0f, -3.0f, 0.0f}); // Behind boss
            break;
        case DungeonRole::RANGED_DPS:
            optimalPos = CalculateDpsPosition(encounter, nullptr);
            optimalPos.RelocateOffset({0.0f, -15.0f, 0.0f}); // 15 yards back
            break;
        case DungeonRole::CROWD_CONTROL:
            optimalPos = CalculateHealerPosition(encounter, {}); // Near healers for safety
            break;
        case DungeonRole::SUPPORT:
            optimalPos = encounter.encounterLocation;
            optimalPos.RelocateOffset({0.0f, -10.0f, 0.0f});
            break;
    }

    return optimalPos;
}

void DungeonBehavior::AvoidDangerousAreas(Player* player, const std::vector<Position>& dangerousAreas)
{
    if (!player || dangerousAreas.empty())
        return;

    Position currentPos = player->GetPosition();
    bool inDanger = false;
    Position nearestSafeSpot = currentPos;

    for (Position const& dangerZone : dangerousAreas)
    {
        float distance = currentPos.GetExactDist(&dangerZone);
        if (distance < 10.0f) // Within danger radius
        {
            inDanger = true;

            // Calculate safe position away from danger
            float angle = dangerZone.GetAngle(&currentPos) + M_PI; // Opposite direction
            nearestSafeSpot.RelocateOffset({std::cos(angle) * 15.0f, std::sin(angle) * 15.0f, 0.0f});
            break;
        }
    }

    if (inDanger)
    {
        // PHASE 6D: Use Movement Arbiter with DUNGEON_MECHANIC priority (205)
        BotAI* botAI = dynamic_cast<BotAI*>(player->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            botAI->RequestPointMovement(
                PlayerBotMovementPriority::DUNGEON_MECHANIC,
                nearestSafeSpot,
                "Dungeon danger zone avoidance",
                "DungeonBehavior");
        }
        else
        {
            // FALLBACK: Direct MotionMaster if arbiter not available
            player->GetMotionMaster()->MovePoint(0, nearestSafeSpot.GetPositionX(),
                nearestSafeSpot.GetPositionY(), nearestSafeSpot.GetPositionZ());
        }

        TC_LOG_DEBUG("module.playerbot", "Player {} moving to avoid dangerous area", player->GetName());
    }
}

// ============================================================================
// TRASH MOB HANDLING
// ============================================================================

void DungeonBehavior::HandleTrashMobs(Group* group, const std::vector<uint32>& trashMobIds)
{
    if (!group || trashMobIds.empty())
        return;

    // Scan for trash mobs in range
    std::vector<Unit*> trashMobs;

    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld())
            continue;

        std::list<Creature*> nearbyCreatures;
        Trinity::AnyUnitInObjectRangeCheck checker(player, 50.0f, true, true);
        Trinity::CreatureListSearcher searcher(player, nearbyCreatures, checker);
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = player->GetMap();
    if (!map)
        return; // Adjust return value as needed

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return; // Adjust return value as needed
    }

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        player->GetPosition(), 50.0f);

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        auto* entity = ObjectAccessor::GetCreature(*player, guid);
        if (!entity)
            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix

        for (Creature* creature : nearbyCreatures)
        {
            if (!creature || !creature->IsAlive())
                continue;

            // Check if this is a trash mob we should handle
            if (std::find(trashMobIds.begin(), trashMobIds.end(), creature->GetEntry()) != trashMobIds.end())
            {
                trashMobs.push_back(creature);
            }
        }
    }

    if (!trashMobs.empty())
    {
        PullTrashGroup(group, trashMobs);
    }
}

void DungeonBehavior::PullTrashGroup(Group* group, const std::vector<Unit*>& trashMobs)
{
    if (!group || trashMobs.empty())
        return;

    TC_LOG_DEBUG("module.playerbot", "Group {} pulling trash group ({} mobs)",
        group->GetGUID().GetCounter(), trashMobs.size());

    // Assign targets to group members
    AssignTrashTargets(group, trashMobs);

    // Execute trash strategy
    ExecuteTrashStrategy(group, trashMobs);

    // Coordinate crowd control if needed
    if (trashMobs.size() > 3)
    {
        std::vector<Unit*> ccTargets(trashMobs.begin() + 2, trashMobs.end());
        CoordinateCrowdControl(group, ccTargets);
    }
}

void DungeonBehavior::AssignTrashTargets(Group* group, const std::vector<Unit*>& trashMobs)
{
    if (!group || trashMobs.empty())
        return;

    // Assign tank to main target
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // Tank gets first target
        if (player->GetClass() == CLASS_WARRIOR || player->GetClass() == CLASS_PALADIN ||
            player->GetClass() == CLASS_DEATH_KNIGHT)
        {
            if (player->GetPrimaryTalentTree() == 0) // Protection/Blood/Prot
            {
                AssignTankTargets(player, trashMobs);
                continue;
            }
        }

        // DPS get assigned targets
        AssignDpsTargets(player, trashMobs);
    }
}

void DungeonBehavior::ExecuteTrashStrategy(Group* group, const std::vector<Unit*>& trashMobs)
{
    if (!group || trashMobs.empty())
        return;

    // Get group strategy
    uint32 groupId = group->GetGUID().GetCounter();
    EncounterStrategy strategy = GetEncounterStrategy(groupId);
    switch (strategy)
    {
        case EncounterStrategy::CONSERVATIVE:
            // Pull one at a time, CC rest
            if (trashMobs.size() > 1)
            {
                std::vector<Unit*> ccTargets(trashMobs.begin() + 1, trashMobs.end());
                CoordinateCrowdControl(group, ccTargets);
            }
            break;

        case EncounterStrategy::AGGRESSIVE:
            // AoE burn everything
            CoordinateGroupDamage(group, DungeonEncounter(0, "Trash", 0));
            break;

        case EncounterStrategy::BALANCED:
            // Kill priority targets, CC extras
            if (trashMobs.size() > 2)
            {
                std::vector<Unit*> ccTargets(trashMobs.begin() + 2, trashMobs.end());
                CoordinateCrowdControl(group, ccTargets);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// BOSS ENCOUNTER STRATEGIES
// ============================================================================

void DungeonBehavior::ExecuteBossStrategy(Group* group, const DungeonEncounter& encounter)
{
    if (!group)
        return;

    TC_LOG_DEBUG("module.playerbot", "Executing boss strategy for: {}", encounter.encounterName);

    // Set encounter-specific strategy
    EncounterStrategy::instance()->ExecuteEncounterStrategy(group, encounter.encounterId);

    // Coordinate initial positioning
    UpdateGroupPositioning(group, encounter);

    // Begin combat preparation
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // Coordinate role-specific behavior
        switch (player->GetClass())
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                if (player->GetPrimaryTalentTree() == 0)
                    CoordinateTankBehavior(player, encounter);
                else
                    CoordinateDpsBehavior(player, encounter);
                break;

            case CLASS_PRIEST:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
                if (player->GetPrimaryTalentTree() == 2)
                    CoordinateHealerBehavior(player, encounter);
                else
                    CoordinateDpsBehavior(player, encounter);
                break;

            default:
                CoordinateDpsBehavior(player, encounter);
                break;
        }
    }

    // Handle encounter-specific mechanics
    for (std::string const& mechanic : encounter.mechanics)
    {
        HandleBossMechanics(group, encounter.encounterId, mechanic);
    }
}

void DungeonBehavior::HandleBossMechanics(Group* group, uint32 encounterId, const std::string& mechanic)
{
    if (!group)
        return;

    TC_LOG_TRACE("module.playerbot", "Handling boss mechanic: {} for encounter {}",
        mechanic, encounterId);

    // Delegate to encounter strategy system
    EncounterStrategy::instance()->HandleEncounterMechanic(group, encounterId, mechanic);

    // Handle common mechanics
    if (mechanic == "tank_swap")
    {
        // Find tanks in group
        Player* currentTank = nullptr;
        Player* otherTank = nullptr;

        for (auto const& member : group->GetMemberSlots())
        {
            Player* player = ObjectAccessor::FindPlayer(member.guid);
            if (!player || !player->IsInWorld() || !player->IsAlive())
                continue;

            if ((player->GetClass() == CLASS_WARRIOR || player->GetClass() == CLASS_PALADIN ||
                 player->GetClass() == CLASS_DEATH_KNIGHT) && player->GetPrimaryTalentTree() == 0)
            {
                if (!currentTank)
                    currentTank = player;
                else if (!otherTank)
                    otherTank = player;
            }
        }

        if (currentTank && otherTank)
        {
            HandleTankSwap(group, currentTank, otherTank);
        }
    }
    else if (mechanic == "aoe_damage")
    {
        // Spread out group
        UpdateGroupPositioning(group, GetEncounterData(encounterId));
    }
    else if (mechanic == "adds")
    {
        // Handle add spawns
        // DPS should switch to adds
        CoordinateGroupDamage(group, GetEncounterData(encounterId));
    }
}

void DungeonBehavior::AdaptToEncounterPhase(Group* group, uint32 encounterId, uint32 phase)
{
    if (!group)
        return;

    TC_LOG_DEBUG("module.playerbot", "Group {} adapting to encounter phase {}",
        group->GetGUID().GetCounter(), phase);

    // Notify encounter strategy system
    EncounterStrategy::instance()->HandleEncounterPhaseTransition(group, encounterId, phase);

    // Update group positioning for new phase
    DungeonEncounter encounter = GetEncounterData(encounterId);
    UpdateGroupPositioning(group, encounter);
}

void DungeonBehavior::HandleEnrageTimer(Group* group, const DungeonEncounter& encounter)
{
    if (!group || !encounter.hasEnrageTimer)
        return;

    uint32 elapsedTime = GameTime::GetGameTimeMS() - _encounterStartTime[group->GetGUID().GetCounter()];
    uint32 remainingTime = (encounter.enrageTimeSeconds * 1000) - elapsedTime;
    if (remainingTime < (encounter.enrageTimeSeconds * 1000 * ENRAGE_WARNING_THRESHOLD))
    {
        TC_LOG_WARN("module.playerbot", "Group {} approaching enrage timer ({} seconds remaining)",
            group->GetGUID().GetCounter(), remainingTime / 1000);

        // Push for maximum DPS
        OptimizeDamageOutput(group, encounter);
    }
}

// ============================================================================
// THREAT AND AGGRO MANAGEMENT
// ============================================================================

void DungeonBehavior::ManageGroupThreat(Group* group, const DungeonEncounter& encounter)
{
    if (!group)
        return;

    ThreatManagement threatStyle = _groupThreatManagement[group->GetGUID().GetCounter()];

    switch (threatStyle)
    {
        case ThreatManagement::STRICT_AGGRO:
            // Ensure tank maintains threat
            ManageThreatMeters(group);
            break;

        case ThreatManagement::LOOSE_AGGRO:
            // Allow some threat variation
            break;

        case ThreatManagement::BURN_STRATEGY:
            // Ignore threat, maximize DPS
            OptimizeDamageOutput(group, encounter);
            break;

        case ThreatManagement::TANK_SWAP:
            // Coordinate tank swapping
            break;

        case ThreatManagement::OFF_TANK:
            // Off-tank handles adds
            break;
    }
}
void DungeonBehavior::HandleTankSwap(Group* group, Player* currentTank, Player* newTank)
{
    if (!group || !currentTank || !newTank)
        return;
    TC_LOG_DEBUG("module.playerbot", "Executing tank swap: {} -> {}",
        currentTank->GetName(), newTank->GetName());

    // Coordinate the swap through encounter strategy
    EncounterStrategy::instance()->HandleTankSwapMechanic(group, currentTank, newTank);
    LogDungeonEvent(group->GetGUID().GetCounter(), "TANK_SWAP",
        Trinity::StringFormat("{} -> {}", currentTank->GetName(), newTank->GetName()));
}

void DungeonBehavior::ManageThreatMeters(Group* group)
{
    if (!group)
        return;
    // Monitor threat levels and warn DPS if needed
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // Check if player is about to pull aggro
        if (Unit* victim = player->GetVictim())
        {
            float playerThreat = victim->GetThreatManager().GetThreat(player);
            float tankThreat = 0.0f;

            // Find tank's threat
            for (auto const& tankMember : group->GetMemberSlots())
            {
                Player* tank = ObjectAccessor::FindPlayer(tankMember.guid);
                if (!tank || !tank->IsInWorld() || !tank->IsAlive())
                    continue;

                if ((tank->GetClass() == CLASS_WARRIOR || tank->GetClass() == CLASS_PALADIN ||
                     tank->GetClass() == CLASS_DEATH_KNIGHT) && tank->GetPrimaryTalentTree() == 0)
                {
                    tankThreat = victim->GetThreatManager().GetThreat(tank);
                    break;
                }
            }

            if (tankThreat > 0 && playerThreat > tankThreat * THREAT_WARNING_THRESHOLD)
            {
                HandleThreatEmergency(group, player);
            }
        }
    }
}

void DungeonBehavior::HandleThreatEmergency(Group* group, Player* player)
{
    if (!group || !player)
        return;

    TC_LOG_DEBUG("module.playerbot", "Threat emergency: {} approaching tank threat",
        player->GetName());

    // Player should reduce threat (stop DPS, use threat reduction abilities, etc.)
    // This would be handled by the player's AI

    LogDungeonEvent(group->GetGUID().GetCounter(), "THREAT_WARNING", player->GetName());
}

// ============================================================================
// HEALING AND DAMAGE COORDINATION
// ============================================================================

void DungeonBehavior::CoordinateGroupHealing(Group* group, const DungeonEncounter& encounter)
{
    if (!group)
        return;

    // Find healers in group
    std::vector<Player*> healers;
    std::vector<Player*> groupMembers;
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld())
            continue;

        groupMembers.push_back(player);

        // Identify healers
        if (player->GetClass() == CLASS_PRIEST || player->GetClass() == CLASS_SHAMAN ||
            player->GetClass() == CLASS_PALADIN || player->GetClass() == CLASS_DRUID)
        {
            if (player->GetPrimaryTalentTree() == 2) // Holy/Resto/Holy
            {
                healers.push_back(player);
            }
        }
    }

    // Coordinate healing priorities for each healer
    for (Player* healer : healers)
    {
        PrioritizeHealingTargets(healer, groupMembers);
    }

    // Check for healing emergency
    bool emergencyDetected = false;
    for (Player* member : groupMembers)
    {
        if (!member->IsAlive())
            continue;

        float healthPct = static_cast<float>(member->GetHealth()) / member->GetMaxHealth();
        if (healthPct < MIN_GROUP_HEALTH_THRESHOLD)
        {
            emergencyDetected = true;
            break;
        }
    }

    if (emergencyDetected)
    {
        HandleHealingEmergency(group);
    }
}

void DungeonBehavior::CoordinateGroupDamage(Group* group, const DungeonEncounter& encounter)
{
    if (!group)
        return;

    // Find DPS players
    std::vector<Player*> dpsPlayers;

    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // Exclude tanks and healers
        bool isTankOrHealer = false;
        if ((player->GetClass() == CLASS_WARRIOR || player->GetClass() == CLASS_PALADIN ||
             player->GetClass() == CLASS_DEATH_KNIGHT) && player->GetPrimaryTalentTree() == 0)
        {
            isTankOrHealer = true;
        }
        else if ((player->GetClass() == CLASS_PRIEST || player->GetClass() == CLASS_SHAMAN ||
                  player->GetClass() == CLASS_DRUID) && player->GetPrimaryTalentTree() == 2)
        {
            isTankOrHealer = true;
        }

        if (!isTankOrHealer)
        {
            dpsPlayers.push_back(player);
        }
    }

    // Assign DPS targets
    for (Player* dps : dpsPlayers)
    {
        // Scan for appropriate targets
        std::vector<Unit*> enemies;

        std::list<Creature*> nearbyCreatures;
        Trinity::AnyUnitInObjectRangeCheck checker(dps, 40.0f, true, true);
        Trinity::CreatureListSearcher searcher(dps, nearbyCreatures, checker);
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = dps->GetMap();
    if (!map)
        return; // Adjust return value as needed

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return; // Adjust return value as needed
    }

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        dps->GetPosition(), 40.0f);

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {
        auto* entity = ObjectAccessor::GetCreature(*dps, guid);
        if (!entity)
            continue;
        // Original filtering logic goes here
    }
    // End of spatial grid fix

        for (Creature* creature : nearbyCreatures)
        {
            if (!creature || !creature->IsAlive() || creature->IsFriendlyTo(dps))
                continue;

            enemies.push_back(creature);
        }

        if (!enemies.empty())
        {
            AssignDpsTargets(dps, enemies);
        }
    }

    // Coordinate cooldown usage
    CoordinateCooldownUsage(group, encounter);
}

void DungeonBehavior::HandleHealingEmergency(Group* group)
{
    if (!group)
        return;

    TC_LOG_WARN("module.playerbot", "Group {} healing emergency detected", group->GetGUID().GetCounter());

    // Trigger emergency healing cooldowns through encounter strategy
    EncounterStrategy::instance()->HandleEmergencyCooldowns(group);

    LogDungeonEvent(group->GetGUID().GetCounter(), "HEALING_EMERGENCY", "Low group health");
}

void DungeonBehavior::OptimizeDamageOutput(Group* group, const DungeonEncounter& encounter)
{
    if (!group)
        return;

    // Coordinate burst damage windows
    CoordinateCooldownUsage(group, encounter);

    // Ensure optimal target selection
    CoordinateGroupDamage(group, encounter);

    TC_LOG_DEBUG("module.playerbot", "Optimizing damage output for group {}", group->GetGUID().GetCounter());
}

// Implementation continues in next message due to length...
