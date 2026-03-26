/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "QuestStrategy.h"
#include "Core/PlayerBotHelpers.h"  // GetBotAI, GetGameSystems
#include "../BotAI.h"
#include "Player.h"
#include "../../Session/BotSession.h"  // For IsInstanceBot check
#include "Group.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "SpellHistory.h"
#include "PathGenerator.h"
#include "Item.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SharedDefines.h"
#include "../../Game/QuestAcceptanceManager.h"
#include "../../Quest/QuestHubDatabase.h"
#include "../../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "../../Spatial/SpatialGridQueryHelpers.h"  // Thread-safe spatial queries
#include "../../Core/Threading/SafeGridOperations.h"  // SEH-protected grid operations
#include "../../Equipment/EquipmentManager.h"  // For reward evaluation
#include "Movement/UnifiedMovementCoordinator.h"
#include "../../Movement/Arbiter/MovementPriorityMapper.h"
#include "LootItemType.h"  // For LootItemType enum used in RewardQuest
#include "UnitAI.h"
#include <limits>
#include "GameTime.h"
#include "../../Interaction/FlightMasterManager.h"     // Cross-map travel via flight paths
#include "../../Travel/TravelRouteManager.h"     // Multi-station travel planning
#include "SpellHistory.h"                        // For hearthstone cooldown check
#include "Spell.h"                               // For casting hearthstone spell

namespace Playerbot
{

QuestStrategy::QuestStrategy()
    : Strategy("quest")
    , _currentPhase(QuestPhase::IDLE)
    , _phaseTimer(0)
    , _lastObjectiveUpdate(0)
    , _currentQuestId(0)
    , _currentObjectiveIndex(0)
    , _lastQuestGiverSearchTime(0)
    , _questGiverSearchFailures(0)
    , _lastWanderTime(0)
    , _currentWanderPointIndex(0)
    , _objectivesCompleted(0)
    , _questsCompleted(0)
    , _averageObjectiveTime(0)
    , _acceptanceManager(nullptr)
{
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: Initialized");
}

void QuestStrategy::InitializeActions()
{
    // No actions needed - quest strategy drives movement directly
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No actions (direct movement control)");
}

void QuestStrategy::InitializeTriggers()
{
    // No triggers needed - relevance system handles activation
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No triggers (using relevance system)");
}

void QuestStrategy::InitializeValues()
{
    // No values needed for this simple strategy
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No values");
}

bool QuestStrategy::IsActive(BotAI* ai) const
{
    bool activeFlag = _active.load();

    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // CRITICAL: Instance bots should NEVER quest - they exist only for BG/dungeon
    // This prevents SmartAI thread-safety crashes when accepting quests from worker threads
    if (BotSession* session = dynamic_cast<BotSession*>(bot->GetSession()))
    {
        if (session->IsInstanceBot())
            return false;
    }

    // NOT active during combat (combat takes priority)
    if (bot->IsInCombat())
        return false;

    // Active for ALL levels - questing is always valuable
    // - Below max level: Quest for XP (high priority)
    // - At max level: Quest for gold, reputation, achievements (lower priority)
    return activeFlag;
}

float QuestStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // Instance bots should NEVER quest - return 0 relevance
    if (BotSession* session = dynamic_cast<BotSession*>(bot->GetSession()))
    {
        if (session->IsInstanceBot())
            return 0.0f;
    }

    // Combat has higher priority - return 0 if in combat
    if (bot->IsInCombat())
        return 0.0f;

    bool isMaxLevel = (bot->GetLevel() >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    bool hasObjectives = HasActiveObjectives(ai);

    // HIGH PRIORITY: Has active quest objectives to complete
    if (hasObjectives)
    {
        // Pre-max level: Very high priority (quest for XP)
        // Max level: Medium-high priority (quest for gold/rep)
        return isMaxLevel ? 60.0f : 70.0f;
    }

    // MEDIUM/LOW PRIORITY: No active quests - search for quest givers
    if (isMaxLevel)
    {
        // Max level: Lower priority (quests are optional for gold/rep)
        return 30.0f; // Lower than loot=60, solo=10-50
    }
    else
    {
        // Pre-max level: Medium-high priority (actively seek quests to level up)
        return 50.0f; // Higher than solo, actively search for quests
    }
}

void QuestStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // CRITICAL: Safety check for worker thread access during bot destruction
    // IsInWorld() returns false during Player destruction, preventing ACCESS_VIOLATION
    // in WorldObject::GetMap() and related grid operations (GetCreatureListWithEntryInGrid, etc.)
    if (!bot->IsInWorld())
        return;

    // Don't interrupt combat
    if (bot->IsInCombat())
        return;

    // Skip processing after teleport — worker thread has stale map/grid data
    // until the main thread completes the map change
    uint32 now = GameTime::GetGameTimeMS();
    if (_teleportCooldownUntil > 0 && now < _teleportCooldownUntil)
        return;
    _teleportCooldownUntil = 0;
    _failedQuestGiverGuids.clear(); // Reset after teleport — new zone, new NPCs

    // ========================================================================
    // PERSISTENT TRAVEL MANAGER UPDATE
    // ========================================================================
    // If we have an active multi-leg travel route (ships, zeppelins, portals),
    // we must update it every tick to progress through the journey states:
    // WALKING_TO_TRANSPORT -> WAITING_FOR_TRANSPORT -> ON_TRANSPORT -> ARRIVING

    // DIAGNOSTIC: Log travel manager state BEFORE checking IsTraveling
    if (_travelManager)
    {
        bool isTraveling = _travelManager->IsTraveling();
        TravelState currentState = _travelManager->GetCurrentState();
        TravelRoute const* route = _travelManager->GetCurrentRoute();
        uint32 legCount = route ? route->totalLegs : 0;
        uint32 currentLegIdx = route ? route->currentLegIndex : 0;

        TC_LOG_DEBUG("module.playerbot.quest",
            "🔍 UpdateBehavior PRE-CHECK: Bot {} - manager exists, IsTraveling={}, state={}, legs={}, currentLeg={}, quest={}",
            bot->GetName(), isTraveling, static_cast<int>(currentState), legCount, currentLegIdx, _lastTravelQuestId);
    }

    if (_travelManager && _travelManager->IsTraveling())
    {
        // If we arrived on the destination map (e.g. via hearthstone), the travel
        // route is obsolete — clear it so we can proceed with quest processing
        TravelRoute const* route = _travelManager->GetCurrentRoute();

        TC_LOG_INFO("module.playerbot.quest",
            "UpdateBehavior: Bot {} travel check — route={}, botMap={}, originMap={}, destMap={}",
            bot->GetName(), route ? "valid" : "NULL", bot->GetMapId(),
            route ? route->originMapId : 0, route ? route->destinationMapId : 0);

        // Clear stale route if:
        // 1. Bot's map changed from route origin (hearthstone/teleport)
        // 2. Bot is already on the destination map (no cross-map travel needed)
        // 3. Route has been active for > 2 minutes without completing first leg
        //    (handles worker thread GetMapId() stale data after teleport)
        bool routeStale = false;
        if (route)
        {
            routeStale = bot->GetMapId() != route->originMapId ||
                         bot->GetMapId() == route->destinationMapId;

            // Timeout only when stuck walking to the FIRST transport dock
            // (not waiting for ship, not on transport, not on later legs)
            if (!routeStale && route->routeStartTime > 0 && route->currentLegIndex == 0)
            {
                TravelLeg const* firstLeg = !route->legs.empty() ? &route->legs[0] : nullptr;
                bool stuckWalkingToFirstDock = firstLeg &&
                    firstLeg->currentState == TravelState::WALKING_TO_TRANSPORT;

                if (stuckWalkingToFirstDock)
                {
                    uint32 elapsed = GameTime::GetGameTimeMS() - route->routeStartTime;
                    if (elapsed > 120000)
                        routeStale = true;
                }
            }
        }
        if (routeStale)
        {
            TC_LOG_INFO("module.playerbot.quest",
                "UpdateBehavior: Bot {} route stale (botMap={}, origin={}, dest={}) — clearing for quest {}",
                bot->GetName(), bot->GetMapId(), route->originMapId, route->destinationMapId, _lastTravelQuestId);
            _travelManager.reset();
            _lastTravelQuestId = 0;
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest",
                "🚢 UpdateBehavior: Bot {} updating active travel route (quest {})",
                bot->GetName(), _lastTravelQuestId);

            bool stillTraveling = _travelManager->Update(diff);

            if (stillTraveling)
                return;
        }

        // Route finished - check if completed or failed
        TravelState finalState = _travelManager->GetCurrentState();
        if (finalState == TravelState::COMPLETED)
        {
            TC_LOG_INFO("module.playerbot.quest",
                "✅ UpdateBehavior: Bot {} completed multi-leg travel route for quest {}",
                bot->GetName(), _lastTravelQuestId);

            // Clear failure tracking on success
            _travelFailures.erase(_lastTravelQuestId);
        }
        else
        {
            // Track travel failure to prevent infinite retry loops
            uint32 now = GameTime::GetGameTimeMS();
            auto& failInfo = _travelFailures[_lastTravelQuestId];
            failInfo.lastFailureTime = now;
            failInfo.failureCount++;

            TC_LOG_WARN("module.playerbot.quest",
                "❌ UpdateBehavior: Bot {} travel route FAILED (attempt {}/{}) for quest {} - cooldown {} minutes",
                bot->GetName(), failInfo.failureCount, MAX_TRAVEL_FAILURES,
                _lastTravelQuestId, TRAVEL_FAILURE_COOLDOWN_MS / 60000);
        }

        // Clean up travel manager
        _travelManager.reset();
        _lastTravelQuestId = 0;
        // Continue to normal quest processing
    }

    // Update objective tracker periodically
    uint32 currentTime = GameTime::GetGameTimeMS();
    if (currentTime - _lastObjectiveUpdate > 2000) // Every 2 seconds
    {
        // CRITICAL FIX: Check BOTH GameSystems AND ObjectiveTracker for null
        // ObjectiveTracker is NOT created in instance-only mode (JIT bots for BG/LFG)
        auto* gameSystems = ai->GetGameSystems();
        if (gameSystems && gameSystems->GetObjectiveTracker())
            gameSystems->GetObjectiveTracker()->UpdateBotTracking(bot, diff);
        _lastObjectiveUpdate = currentTime;
    }

    // Update dynamic spawn handler (processes area triggers, etc.)
    if (_dynamicSpawnHandler)
    {
        _dynamicSpawnHandler->Update(diff);
    }

    // ========================================================================
    // PENDING QUEST GIVER INTERACTION — complete walk-to + accept across ticks
    // ========================================================================
    if (!_pendingQuestGiverGuid.IsEmpty())
    {
        // Try to resolve creature — ObjectAccessor first, then grid scan with
        // wide radius. The stored position may not match the NPC's actual position
        // due to Z-height differences or navmesh pathing stopping early.
        Creature* pendingNPC = ObjectAccessor::GetCreature(*bot, _pendingQuestGiverGuid);

        if (!pendingNPC || !pendingNPC->IsAlive() || !pendingNPC->IsInWorld())
        {
            uint32 creatureEntry = _pendingQuestGiverGuid.GetEntry();
            if (creatureEntry)
            {
                // Wide scan — bot may have stopped pathing far from stored position
                // but close to the actual NPC
                std::list<Creature*> nearby;
                if (SafeGridOperations::GetCreatureListSafe(bot, nearby, creatureEntry, 50.0f))
                {
                    for (Creature* c : nearby)
                    {
                        if (c && c->IsAlive() && c->IsQuestGiver())
                        {
                            pendingNPC = c;
                            break;
                        }
                    }
                }
            }
        }

        // Use BotAI's cached position — interpolated from movespline, accurate on
        // worker threads unlike bot->GetPositionX/Y/Z() which is main-thread-only.
        Position const& botPos = ai->GetCurrentPosition();
        float dist = pendingNPC ? botPos.GetExactDist2d(pendingNPC->GetPosition())
                                : botPos.GetExactDist2d(_pendingQuestGiverPos);

        if (dist <= INTERACTION_DISTANCE)
        {
            if (pendingNPC)
            {
                TC_LOG_INFO("module.playerbot.quest",
                    "QuestStrategy: Bot {} interacting with quest giver {} (posDist={:.1f}, npcDist={:.1f})",
                    bot->GetName(), pendingNPC->GetName(), dist, bot->GetExactDist2d(pendingNPC));

                if (!_acceptanceManager)
                    _acceptanceManager = std::make_unique<QuestAcceptanceManager>(bot);

                _acceptanceManager->ProcessQuestGiver(pendingNPC);

                TC_LOG_INFO("module.playerbot.quest",
                    "QuestStrategy: Bot {} quest acceptance done (accepted: {}, dropped: {})",
                    bot->GetName(),
                    _acceptanceManager->GetQuestsAccepted(),
                    _acceptanceManager->GetQuestsDropped());
            }
            else
            {
                TC_LOG_WARN("module.playerbot.quest",
                    "QuestStrategy: Bot {} at quest giver position but creature not found — clearing",
                    bot->GetName());
            }
            _pendingQuestGiverGuid.Clear();
        }
        else
        {
            // Detect stuck — if distance hasn't decreased in 30 seconds, give up
            static std::unordered_map<ObjectGuid, std::pair<float, uint32>> walkProgress;
            ObjectGuid botGuid = bot->GetGUID();
            uint32 now = GameTime::GetGameTimeMS();

            auto& [lastDist, lastProgressTime] = walkProgress[botGuid];
            if (lastDist == 0.0f || dist < lastDist - 1.0f) // At least 1yd progress
            {
                lastDist = dist;
                lastProgressTime = now;
            }
            else if (now - lastProgressTime > 30000)
            {
                TC_LOG_WARN("module.playerbot.quest",
                    "QuestStrategy: Bot {} gave up walking to quest giver (stuck at {:.1f}yd for 30s)",
                    bot->GetName(), dist);
                _pendingQuestGiverGuid.Clear();
                walkProgress.erase(botGuid);
                return;
            }

            // Throttle log to once per 5 seconds
            static std::unordered_map<ObjectGuid, uint32> lastWalkLog;
            if (now - lastWalkLog[botGuid] > 5000)
            {
                TC_LOG_INFO("module.playerbot.quest",
                    "QuestStrategy: Bot {} walking to quest giver (dist={:.1f})",
                    bot->GetName(), dist);
                lastWalkLog[botGuid] = now;
            }

            BotMovementUtil::MoveToPosition(bot, _pendingQuestGiverPos);
            return;
        }
    }

    // Check if bot has active quests
    bool hasActiveQuests = false;
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId != 0)
        {
            hasActiveQuests = true;
            TC_LOG_DEBUG("module.playerbot.quest", "📋 UpdateBehavior: Bot {} has active quest {} in slot {}",
                         bot->GetName(), questId, slot);
            break;
        }
    }

    TC_LOG_DEBUG("module.playerbot.quest", "📊 UpdateBehavior: Bot {} hasActiveQuests={}",
                 bot->GetName(), hasActiveQuests);

    if (hasActiveQuests)
    {
        // CRITICAL FIX: Periodically search for new quest givers even when quests are active
        // This prevents bots from being permanently stuck on broken/unavailable quests
        // Check every 60 seconds to see if there are new quest givers nearby
        uint32 currentTime = GameTime::GetGameTimeMS();
        constexpr uint32 QUEST_GIVER_SEARCH_INTERVAL = 60000; // 60 seconds

        if (currentTime - _lastQuestGiverSearchTime > QUEST_GIVER_SEARCH_INTERVAL)
        {
            TC_LOG_DEBUG("module.playerbot.quest",
                "🔄 UpdateBehavior: Bot {} - Periodic quest giver search ({}s since last search)",
                bot->GetName(), (currentTime - _lastQuestGiverSearchTime) / 1000);
            SearchForQuestGivers(ai);
        }

        TC_LOG_DEBUG("module.playerbot.quest", "🎯 UpdateBehavior: Bot {} processing quest objectives", bot->GetName());
        // Process quest objectives
        ProcessQuestObjectives(ai);
    }
    else
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🔍 UpdateBehavior: Bot {} has NO active quests - calling SearchForQuestGivers", bot->GetName());
        // No active quests - search for quest givers to accept new quests
        SearchForQuestGivers(ai);
    }
    TC_LOG_DEBUG("module.playerbot.quest", "✅ UpdateBehavior: Bot {} quest behavior update complete", bot->GetName());
}

void QuestStrategy::ProcessQuestObjectives(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;
    Player* bot = ai->GetBot();

    // CRITICAL: Safety check for worker thread access during bot destruction
    if (!bot->IsInWorld())
        return;

    TC_LOG_DEBUG("module.playerbot.quest", "ProcessQuestObjectives: Bot {} starting objective processing", bot->GetName());

    // CRITICAL FIX: FIRST check for any COMPLETE quests that need to be turned in
    // This MUST happen BEFORE processing objectives, otherwise bots will work on
    // INCOMPLETE quests while ignoring COMPLETE quests ready for turn-in!
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        QuestStatus status = bot->GetQuestStatus(questId);
        if (status == QUEST_STATUS_COMPLETE)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "✅ ProcessQuestObjectives: Bot {} found COMPLETE quest {} in slot {} - turning it in FIRST!",
                         bot->GetName(), questId, slot);
            TurnInQuest(ai, questId);
            return;  // Return after initiating turn-in, will process objectives next update
        }
    }

    // Get highest priority objective from ObjectiveTracker
    // CRITICAL FIX: Use ai->GetGameSystems() directly instead of GetGameSystems(bot)
    // The helper function GetGameSystems(bot) goes through player->GetAI() which may return
    // nullptr if the AI is being accessed from a different context (e.g., worker thread)
    // CRITICAL FIX #2: Check BOTH GameSystems AND ObjectiveTracker for null
    // ObjectiveTracker is NOT created in instance-only mode (JIT bots for BG/LFG)
    ObjectivePriority priority(0, 0, 0.0f);
    {
        auto* gameSystems = ai->GetGameSystems();
        auto* tracker = gameSystems ? gameSystems->GetObjectiveTracker() : nullptr;
        if (tracker)
            priority = tracker->GetHighestPriorityObjective(bot);
    }

    TC_LOG_DEBUG("module.playerbot.quest", "🎯 ProcessQuestObjectives: Bot {} - priority.questId={}, priority.objectiveIndex={}",
                 bot->GetName(), priority.questId, priority.objectiveIndex);

    if (priority.questId == 0)
    {
        // ObjectiveTracker doesn't know about bot's quests - initialize it
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} ObjectiveTracker returned questId=0, initializing from quest log",
                     bot->GetName());

        // Scan quest log and register all active quests with ObjectiveTracker
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (questId == 0)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            // CRITICAL FIX: Only register objectives for INCOMPLETE quests
            // COMPLETE and REWARDED quests should not have objectives tracked
            QuestStatus status = bot->GetQuestStatus(questId);
            if (status != QUEST_STATUS_INCOMPLETE)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⏭️ ProcessQuestObjectives: Bot {} skipping quest {} (status={}) - not INCOMPLETE",
                            bot->GetName(), questId, static_cast<int>(status));
                continue;
            }

            TC_LOG_DEBUG("module.playerbot.quest", "🔄 ProcessQuestObjectives: Bot {} registering quest {} with ObjectiveTracker (Quest has {} objectives)",
                         bot->GetName(), questId, quest->Objectives.size());

            // Skip quests with no objectives (autocomplete/scripted quests)
            if (quest->Objectives.empty())
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⚠️ Quest {} has NO objectives - skipping registration (likely autocomplete/scripted quest)",
                            questId);
                continue;
            }

            // Register each objective of this quest
            for (uint32 i = 0; i < quest->Objectives.size(); ++i)
            {
                QuestObjective const& objective = quest->Objectives[i];

                // Map TrinityCore objective type to our QuestObjectiveType enum
                QuestObjectiveType objType = static_cast<QuestObjectiveType>(objective.Type);

                // Create objective data using constructor (questId, index, type, targetId, requiredCount)
                QuestObjectiveData objData(questId, i, objType, objective.ObjectID, objective.Amount);

                IGameSystemsManager* gameSystems = ai->GetGameSystems();
                if (gameSystems && gameSystems->GetObjectiveTracker())
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "📝 ProcessQuestObjectives: Bot {} registering objective {} for quest {} (type={}, targetId={}, amount={})",
                                 bot->GetName(), i, questId, static_cast<int>(objType), objective.ObjectID, objective.Amount);
                    gameSystems->GetObjectiveTracker()->StartTrackingObjective(bot, objData);
                }
                else
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "❌ ProcessQuestObjectives: Bot {} - GetGameSystems() or GetObjectiveTracker() returned nullptr! Cannot register objective.",
                                 bot->GetName());
                }
            }
        }
        // Try again after initialization
        // CRITICAL FIX: Check BOTH GameSystems AND ObjectiveTracker for null
        {
            auto* gameSystems = ai->GetGameSystems();
            auto* tracker = gameSystems ? gameSystems->GetObjectiveTracker() : nullptr;
            if (tracker)
                priority = tracker->GetHighestPriorityObjective(bot);
        }
        TC_LOG_DEBUG("module.playerbot.quest", "🔄 ProcessQuestObjectives: Bot {} after initialization - priority.questId={}, priority.objectiveIndex={}",
                     bot->GetName(), priority.questId, priority.objectiveIndex);

        if (priority.questId == 0)
        {
            // Still no objectives - bot has only autocomplete/scripted quests with no trackable objectives
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} has NO trackable objectives (checking for talk-to/turn-in quests)",
                         bot->GetName());

            // CRITICAL FIX: Handle quests with no objectives
            // These include:
            // 1. Completed quests ready to turn in
            // 2. "Talk-to" quests (no objectives, just need to talk to quest ender)
            // 3. IsTurnIn() quests (QUEST_TYPE_TURNIN)
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 questId = bot->GetQuestSlotQuestId(slot);
                if (questId == 0)
                    continue;

                Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
                if (!quest)
                    continue;

                QuestStatus status = bot->GetQuestStatus(questId);

                // Check if quest is complete - turn it in
                if (status == QUEST_STATUS_COMPLETE)
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "✅ ProcessQuestObjectives: Bot {} has COMPLETE quest {} - turning it in!",
                                 bot->GetName(), questId);
                    TurnInQuest(ai, questId);
                    return;
                }

                // CRITICAL FIX: Check if quest status is INCOMPLETE but ALL objectives are actually complete
                // This handles cases where TrinityCore didn't update quest status after objectives were done
                // (e.g., quest 26391 with 8/8 progress but status still INCOMPLETE)
                if (status == QUEST_STATUS_INCOMPLETE && !quest->Objectives.empty())
                {
                    bool allObjectivesComplete = true;
                    for (QuestObjective const& objective : quest->Objectives)
                    {
                        uint32 currentProgress = bot->GetQuestObjectiveData(questId, objective.StorageIndex);
                        uint32 requiredAmount = static_cast<uint32>(objective.Amount);
                        if (currentProgress < requiredAmount)
                        {
                            allObjectivesComplete = false;
                            break;
                        }
                    }

                    if (allObjectivesComplete)
                    {
                        TC_LOG_DEBUG("module.playerbot.quest", "✅ ProcessQuestObjectives: Bot {} quest {} has ALL objectives COMPLETE but status is still INCOMPLETE - treating as COMPLETE for turn-in!",
                                     bot->GetName(), questId);
                        TurnInQuest(ai, questId);
                        return;
                    }
                }

                // CRITICAL FIX: Check for DELIVERY quests (StartItem == objective item)
                // These quests give the item on acceptance, and completion just requires turning it in
                // Even if quest progress shows 0/1, if bot has the StartItem, quest is ready to turn in
                if (status == QUEST_STATUS_INCOMPLETE)
                {
                    uint32 startItemId = quest->GetSrcItemId();
                    if (startItemId != 0)
                    {
                        // Check if any ITEM objective matches the StartItem
                        for (QuestObjective const& objective : quest->Objectives)
                        {
                            if (objective.Type == QUEST_OBJECTIVE_ITEM &&
                                static_cast<uint32>(objective.ObjectID) == startItemId)
                            {
                                // This is a delivery quest - check if bot has the item
                                uint32 itemCount = bot->GetItemCount(startItemId);
                                if (itemCount >= static_cast<uint32>(objective.Amount))
                                {
                                    TC_LOG_DEBUG("module.playerbot.quest", "📬 ProcessQuestObjectives: Bot {} has DELIVERY quest {} (StartItem={}, count={}), turning in!",
                                                 bot->GetName(), questId, startItemId, itemCount);
                                    TurnInQuest(ai, questId);
                                    return;
                                }
                                break;  // Found the StartItem objective, stop searching
                            }
                        }
                    }
                }

                // Check for "talk-to" quests: no objectives but INCOMPLETE status
                // These are quests where you just need to talk to the quest ender to complete them
                if (status == QUEST_STATUS_INCOMPLETE && quest->Objectives.empty())
                {
                    // This is a "talk-to" quest - navigate to quest ender and turn in
                    TC_LOG_DEBUG("module.playerbot.quest", "🗣️ ProcessQuestObjectives: Bot {} has TALK-TO quest {} (no objectives, incomplete) - navigating to quest ender",
                                 bot->GetName(), questId);
                    TurnInQuest(ai, questId);
                    return;
                }

                // Check for IsTurnIn() quests (QUEST_TYPE_TURNIN)
                if (quest->IsTurnIn())
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "📜 ProcessQuestObjectives: Bot {} has TURN-IN type quest {} - navigating to quest ender",
                                 bot->GetName(), questId);
                    TurnInQuest(ai, questId);
                    return;
                }
            }

            // No actionable quests - fall back to searching for new quests
            TC_LOG_DEBUG("module.playerbot.quest", "📍 ProcessQuestObjectives: Bot {} has no actionable quests, searching for new quests",
                         bot->GetName());
            SearchForQuestGivers(ai);
            return;
        }
    }

    // Get objective state
    // CRITICAL FIX: Check BOTH GameSystems AND ObjectiveTracker for null
    ObjectiveState objective;
    {
        auto* gameSystems = ai->GetGameSystems();
        auto* tracker = gameSystems ? gameSystems->GetObjectiveTracker() : nullptr;
        if (tracker)
            objective = tracker->GetObjectiveState(bot, priority.questId, priority.objectiveIndex);
    }

    // Cache current objective info
    _currentQuestId = objective.questId;
    _currentObjectiveIndex = objective.objectiveIndex;

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest)
        return;

    // CRITICAL FIX: Validate quest status BEFORE processing
    // ObjectiveTracker may return stale data for quests the bot no longer has
    QuestStatus questStatus = bot->GetQuestStatus(objective.questId);

    // Check if quest is complete - turn it in
    if (questStatus == QUEST_STATUS_COMPLETE)
    {
        TurnInQuest(ai, objective.questId);
        return;
    }

    // CRITICAL: If quest is not INCOMPLETE, skip it - ObjectiveTracker has stale data
    // This happens when ObjectiveTracker returns a quest that was abandoned/completed/rewarded
    if (questStatus != QUEST_STATUS_INCOMPLETE)
    {
        TC_LOG_WARN("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} - ObjectiveTracker returned quest {} but status is {} (not INCOMPLETE) - checking for complete quests to turn in",
                     bot->GetName(), objective.questId, static_cast<int>(questStatus));

        // Fall back to checking for any COMPLETE quests to turn in
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (questId == 0)
                continue;

            if (bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
            {
                TC_LOG_INFO("module.playerbot.quest", "✅ ProcessQuestObjectives: Bot {} found COMPLETE quest {} to turn in",
                            bot->GetName(), questId);
                TurnInQuest(ai, questId);
                return;
            }
        }

        // No complete quests - search for new quests
        TC_LOG_INFO("module.playerbot.quest", "📍 ProcessQuestObjectives: Bot {} has no incomplete/complete quests - searching for quest givers",
                    bot->GetName());
        SearchForQuestGivers(ai);
        return;
    }

    // CRITICAL FIX: Check if this objective is blacklisted (too many failures)
    // This prevents infinite loops on quests with unreachable/dynamically-spawned objectives
    if (IsObjectiveBlacklisted(objective.questId, objective.objectiveIndex))
    {
        TC_LOG_WARN("module.playerbot.quest", "🚫 ProcessQuestObjectives: Bot {} - Quest {} objective {} is BLACKLISTED (unreachable) - skipping to next quest/objective",
                    bot->GetName(), objective.questId, objective.objectiveIndex);

        // Check if ALL objectives for this quest are blacklisted - if so, abandon the quest
        bool allBlacklisted = true;
        for (uint8 i = 0; i < quest->Objectives.size(); ++i)
        {
            if (!IsObjectiveBlacklisted(objective.questId, i))
            {
                allBlacklisted = false;
                break;
            }
        }

        if (allBlacklisted && quest->Objectives.size() > 0)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "ProcessQuestObjectives: Bot {} abandoning quest {} ({})",
                         bot->GetName(), objective.questId, quest->GetLogTitle());
            bot->RemoveActiveQuest(objective.questId);
            bot->AbandonQuest(objective.questId);

            // Blacklist this quest to prevent re-accepting
            if (_acceptanceManager)
                _acceptanceManager->BlacklistQuest(objective.questId);
        }

        // Search for other quests or quest givers
        SearchForQuestGivers(ai);
        return;
    }

    // Determine objective type and execute appropriate behavior
    QuestObjective const* questObjective = quest->Objectives.size() > objective.objectiveIndex
        ? &quest->Objectives[objective.objectiveIndex]
        : nullptr;

    if (!questObjective)
        return;

    // Route to appropriate handler based on objective type
    TC_LOG_DEBUG("module.playerbot.quest", "🔀 ProcessQuestObjectives: Bot {} - Routing objective type {} for quest {}",
                 bot->GetName(), questObjective->Type, objective.questId);

    switch (questObjective->Type)
    {
        case QUEST_OBJECTIVE_MONSTER:
        case QUEST_OBJECTIVE_PLAYERKILLS:
            // CRITICAL: Type 0 (QUEST_OBJECTIVE_MONSTER) can mean EITHER:
            // 1. Kill target (normal case)
            // 2. Use quest item on target (e.g., Quest 26391 - use item 58362 on fire GameObject)
            //
            // Detect "use item on target" quests by checking if quest has StartItem
            // (item given when quest is accepted and required for objective)
            if (quest->GetSrcItemId() != 0)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "🎯 ProcessQuestObjectives: Bot {} - Quest {} is USE ITEM quest (StartItem={}), calling UseQuestItemOnTarget",
                             bot->GetName(), objective.questId, quest->GetSrcItemId());
                UseQuestItemOnTarget(ai, objective);
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⚔️ ProcessQuestObjectives: Bot {} - Quest {} is KILL TARGET quest, calling EngageQuestTargets",
                             bot->GetName(), objective.questId);
                EngageQuestTargets(ai, objective);
            }
            break;

        case QUEST_OBJECTIVE_ITEM:
        {
            // CRITICAL FIX: ITEM objectives can require either:
            // 1. Looting from killed creatures (check creature_loot_template)
            // 2. Interacting with GameObjects (check gameobject_loot_template)
            // 3. DELIVERY QUEST: Item is the quest's StartItem (given on acceptance)
            // We need to check which one and route appropriately!

            // CRITICAL: Check if this is a DELIVERY QUEST
            // Delivery quests have StartItem == objective item (e.g., Quest 54 "Report to Goldshire")
            // The bot receives the item on quest acceptance and just needs to turn it in
            uint32 startItemId = quest->GetSrcItemId();
            if (startItemId != 0 && static_cast<uint32>(questObjective->ObjectID) == startItemId)
            {
                // Check if bot has the delivery item in inventory
                uint32 itemCount = bot->GetItemCount(startItemId);
                if (itemCount >= static_cast<uint32>(questObjective->Amount))
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "📬 ProcessQuestObjectives: Bot {} - Quest {} is DELIVERY quest (StartItem={}, has {}), routing to TurnInQuest!",
                                 bot->GetName(), objective.questId, startItemId, itemCount);
                    TurnInQuest(ai, objective.questId);
                    return;
                }
                else
                {
                    // Bot doesn't have the item - this shouldn't happen for delivery quests
                    // The item should have been given on quest acceptance
                    TC_LOG_DEBUG("module.playerbot.quest", "❌ ProcessQuestObjectives: Bot {} - DELIVERY quest {} but MISSING StartItem {} (has {} need {})! Quest may be broken.",
                                 bot->GetName(), objective.questId, startItemId, itemCount, questObjective->Amount);
                    // Fall through to try normal item collection as a fallback
                }
            }

            // Check if item comes from creature loot
            bool isLootFromCreature = IsItemFromCreatureLoot(questObjective->ObjectID);

            if (isLootFromCreature)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⚔️ ProcessQuestObjectives: Bot {} - Item {} comes from CREATURE LOOT, calling EngageQuestTargets for quest {}",
                             bot->GetName(), questObjective->ObjectID, objective.questId);

                // Route to EngageQuestTargets to kill the creature that drops this item
                EngageQuestTargets(ai, objective);
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.quest", "📦 ProcessQuestObjectives: Bot {} - Item {} comes from GAMEOBJECT or ground loot, calling CollectQuestItems for quest {}",
                             bot->GetName(), questObjective->ObjectID, objective.questId);
                // Route to CollectQuestItems for GameObject interaction
                CollectQuestItems(ai, objective);
            }
            break;
        }

        case QUEST_OBJECTIVE_GAMEOBJECT:
            TC_LOG_DEBUG("module.playerbot.quest", "📦 ProcessQuestObjectives: Bot {} - Calling CollectQuestItems for quest {}",
                         bot->GetName(), objective.questId);
            CollectQuestItems(ai, objective);
            break;

        case QUEST_OBJECTIVE_AREATRIGGER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ ProcessQuestObjectives: Bot {} - Calling ExploreQuestArea for quest {}",
                         bot->GetName(), objective.questId);
            ExploreQuestArea(ai, objective);
            break;

        // ========== TALKTO OBJECTIVES ==========
        // Bot needs to interact with an NPC (gossip/dialog)
        case QUEST_OBJECTIVE_TALKTO:
            TC_LOG_DEBUG("module.playerbot.quest", "🗣️ ProcessQuestObjectives: Bot {} - TALKTO objective for quest {}, calling TalkToNpc",
                         bot->GetName(), objective.questId);
            TalkToNpc(ai, objective);
            break;

        // ========== KILL WITH LABEL ==========
        // Same as MONSTER but with special kill label requirement
        case QUEST_OBJECTIVE_KILL_WITH_LABEL:
            TC_LOG_DEBUG("module.playerbot.quest", "🏷️ ProcessQuestObjectives: Bot {} - KILL_WITH_LABEL objective for quest {}, calling EngageQuestTargets",
                         bot->GetName(), objective.questId);
            EngageQuestTargets(ai, objective);
            break;

        // ========== CURRENCY OBJECTIVES ==========
        // These track currency spending/obtaining - bot handles currency passively
        case QUEST_OBJECTIVE_CURRENCY:
            TC_LOG_DEBUG("module.playerbot.quest", "💰 ProcessQuestObjectives: Bot {} - CURRENCY objective for quest {} (ObjectID={}, Amount={}) - handled passively via currency spending",
                         bot->GetName(), objective.questId, questObjective->ObjectID, questObjective->Amount);
            // Currency objectives are completed when bot spends currency
            // Bot may need to visit vendors - navigate to quest area
            NavigateToObjective(ai, objective);
            break;

        case QUEST_OBJECTIVE_HAVE_CURRENCY:
            TC_LOG_DEBUG("module.playerbot.quest", "💰 ProcessQuestObjectives: Bot {} - HAVE_CURRENCY objective for quest {} (ObjectID={}, Amount={}) - checking if bot has required currency",
                         bot->GetName(), objective.questId, questObjective->ObjectID, questObjective->Amount);
            // Bot needs to have currency when turning in - check and navigate to turn-in if ready
            HandleCurrencyObjective(ai, objective);
            break;

        case QUEST_OBJECTIVE_OBTAIN_CURRENCY:
            TC_LOG_DEBUG("module.playerbot.quest", "💰 ProcessQuestObjectives: Bot {} - OBTAIN_CURRENCY objective for quest {} (ObjectID={}, Amount={}) - currency gained passively",
                         bot->GetName(), objective.questId, questObjective->ObjectID, questObjective->Amount);
            // Currency is obtained through gameplay - navigate to quest area
            NavigateToObjective(ai, objective);
            break;

        // ========== REPUTATION OBJECTIVES ==========
        // These track reputation gains/levels - completed through gameplay
        case QUEST_OBJECTIVE_MIN_REPUTATION:
            TC_LOG_DEBUG("module.playerbot.quest", "⭐ ProcessQuestObjectives: Bot {} - MIN_REPUTATION objective for quest {} (FactionID={}, Required={}) - reputation gained passively",
                         bot->GetName(), objective.questId, questObjective->ObjectID, questObjective->Amount);
            // Reputation is gained through quests/kills - navigate to quest area
            NavigateToObjective(ai, objective);
            break;

        case QUEST_OBJECTIVE_MAX_REPUTATION:
            TC_LOG_DEBUG("module.playerbot.quest", "⭐ ProcessQuestObjectives: Bot {} - MAX_REPUTATION objective for quest {} (FactionID={}, MaxAllowed={}) - waiting for conditions",
                         bot->GetName(), objective.questId, questObjective->ObjectID, questObjective->Amount);
            // This is a "don't exceed" reputation check - usually just waiting
            break;

        case QUEST_OBJECTIVE_INCREASE_REPUTATION:
            TC_LOG_DEBUG("module.playerbot.quest", "⭐ ProcessQuestObjectives: Bot {} - INCREASE_REPUTATION objective for quest {} (FactionID={}, Amount={}) - reputation gained passively",
                         bot->GetName(), objective.questId, questObjective->ObjectID, questObjective->Amount);
            // Reputation is gained through quests/kills - navigate to quest area
            NavigateToObjective(ai, objective);
            break;

        // ========== SPELL/MONEY OBJECTIVES ==========
        case QUEST_OBJECTIVE_LEARNSPELL:
            TC_LOG_DEBUG("module.playerbot.quest", "📖 ProcessQuestObjectives: Bot {} - LEARNSPELL objective for quest {} (SpellID={}) - spell learned via trainer/reward",
                         bot->GetName(), objective.questId, questObjective->ObjectID);
            // Bot may need to visit a trainer - navigate to quest area or seek trainer
            NavigateToObjective(ai, objective);
            break;

        case QUEST_OBJECTIVE_MONEY:
            TC_LOG_DEBUG("module.playerbot.quest", "💵 ProcessQuestObjectives: Bot {} - MONEY objective for quest {} (Amount={} copper) - checking if bot has required gold",
                         bot->GetName(), objective.questId, questObjective->Amount);
            // Check if bot has required money - usually just a check at turn-in
            HandleMoneyObjective(ai, objective);
            break;

        // ========== PROGRESS BAR / CRITERIA OBJECTIVES ==========
        // These are completed through various gameplay actions
        case QUEST_OBJECTIVE_CRITERIA_TREE:
            TC_LOG_DEBUG("module.playerbot.quest", "🎯 ProcessQuestObjectives: Bot {} - CRITERIA_TREE objective for quest {} (CriteriaID={}) - progress tracked automatically",
                         bot->GetName(), objective.questId, questObjective->ObjectID);
            // Criteria tree objectives track achievement-like progress
            NavigateToObjective(ai, objective);
            break;

        case QUEST_OBJECTIVE_PROGRESS_BAR:
            TC_LOG_DEBUG("module.playerbot.quest", "📊 ProcessQuestObjectives: Bot {} - PROGRESS_BAR objective for quest {} - progress tracked automatically",
                         bot->GetName(), objective.questId);
            // Progress bar objectives are completed through various actions in quest area
            NavigateToObjective(ai, objective);
            break;

        // ========== PET BATTLE OBJECTIVES (Not Applicable for Bots) ==========
        // These require the Pet Battle system which bots cannot participate in
        case QUEST_OBJECTIVE_WINPETBATTLEAGAINSTNPC:
            TC_LOG_WARN("module.playerbot.quest", "🐾 ProcessQuestObjectives: Bot {} - WINPETBATTLEAGAINSTNPC objective for quest {} - Pet battles NOT SUPPORTED by bots!",
                        bot->GetName(), objective.questId);
            // Pet battle system - bots cannot participate, quest may be stuck
            break;

        case QUEST_OBJECTIVE_DEFEATBATTLEPET:
            TC_LOG_WARN("module.playerbot.quest", "🐾 ProcessQuestObjectives: Bot {} - DEFEATBATTLEPET objective for quest {} - Pet battles NOT SUPPORTED by bots!",
                        bot->GetName(), objective.questId);
            // Pet battle system - bots cannot participate, quest may be stuck
            break;

        case QUEST_OBJECTIVE_WINPVPPETBATTLES:
            TC_LOG_WARN("module.playerbot.quest", "🐾 ProcessQuestObjectives: Bot {} - WINPVPPETBATTLES objective for quest {} - Pet battles NOT SUPPORTED by bots!",
                        bot->GetName(), objective.questId);
            // PvP Pet battle system - bots cannot participate, quest may be stuck
            break;

        default:
            TC_LOG_DEBUG("module.playerbot.quest", "❓ ProcessQuestObjectives: Bot {} - Unknown objective type {}, calling NavigateToObjective for quest {}",
                         bot->GetName(), questObjective->Type, objective.questId);
            // Unknown objective type - try to navigate to objective location
            NavigateToObjective(ai, objective);
            break;
    }
}

void QuestStrategy::NavigateToObjective(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ NavigateToObjective: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ NavigateToObjective: Bot not in world, aborting");
        return;
    }

    // CRITICAL FIX: Check for combat FIRST - combat always takes priority over navigation!
    if (bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚔️ NavigateToObjective: Bot {} IN COMBAT - aborting navigation, combat takes priority!",
                     bot->GetName());
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "🗺️ NavigateToObjective: Bot {} navigating to quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Get objective position from tracker
    Position objectivePos = GetObjectivePosition(ai, objective);

    TC_LOG_DEBUG("module.playerbot.quest", "📍 NavigateToObjective: Bot {} - Objective position: ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(),
                 objectivePos.GetPositionX(), objectivePos.GetPositionY(), objectivePos.GetPositionZ());

    if (objectivePos.GetExactDist2d(0.0f, 0.0f) < 0.1f)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ NavigateToObjective: Bot {} - NO VALID position for objective {} of quest {} (position is at origin)",
                     bot->GetName(), objective.objectiveIndex, objective.questId);
        return;
    }

    float distance = bot->GetExactDist2d(objectivePos.GetPositionX(), objectivePos.GetPositionY());
    TC_LOG_DEBUG("module.playerbot.quest", "🚶 NavigateToObjective: Bot {} moving to objective (distance: {:.1f})",
                 bot->GetName(), distance);

    // CRITICAL FIX: Add randomness to prevent all bots standing at exact same spot
    // When moving to quest POI (waiting for respawns), spread bots out in a radius
    // This prevents bot clumping and looks more natural
    Position randomizedPos = objectivePos;

    // Generate random offset within 15-yard radius
    // Use bot GUID as seed for deterministic but unique positioning per bot
    uint32 botSeed = bot->GetGUID().GetCounter();
    float randomAngle = (botSeed % 360) * (static_cast<float>(M_PI) / 180.0f); // Convert bot GUID to angle
    float randomDistance = 5.0f + ((botSeed % 1000) / 1000.0f) * 10.0f; // 5-15 yards

    randomizedPos.Relocate(
        objectivePos.GetPositionX() + cos(randomAngle) * randomDistance,
        objectivePos.GetPositionY() + sin(randomAngle) * randomDistance,
        objectivePos.GetPositionZ()
    );

    TC_LOG_DEBUG("module.playerbot.quest", "🎲 NavigateToObjective: Bot {} - Randomized position offset: angle={:.1f}°, distance={:.1f}yd → ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(), randomAngle * (180.0f / static_cast<float>(M_PI)), randomDistance,
                 randomizedPos.GetPositionX(), randomizedPos.GetPositionY(), randomizedPos.GetPositionZ());

    // Move to randomized objective location
    bool moveResult = MoveToObjectiveLocation(ai, randomizedPos);
    TC_LOG_DEBUG("module.playerbot.quest", "🚶 NavigateToObjective: Bot {} MoveToObjectiveLocation result: {}",
                 bot->GetName(), moveResult ? "SUCCESS" : "FAILED");
}

void QuestStrategy::EngageQuestTargets(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ EngageQuestTargets: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    if (!bot->IsInWorld())
        return;

    TC_LOG_DEBUG("module.playerbot.quest", "🎯 EngageQuestTargets: Bot {} searching for quest targets for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Find quest target near bot
    ::Unit* target = FindQuestTarget(ai, objective);

    if (!target)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - NO hostile target found",
                     bot->GetName());

        // CRITICAL FIX: Check if this is a FRIENDLY NPC interaction quest (like Quest 28809)
        // FindQuestTarget returns nullptr for friendly NPCs to prevent attacking them
        // We need to check if there's a friendly NPC nearby that matches the objective
        Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
        if (quest && objective.objectiveIndex < quest->Objectives.size())
        {
            QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];
            // Scan for friendly NPCs with this entry in range (300 yards to match hostile creature scan)
            // THREAD-SAFE: Use SafeGridOperations with SEH protection to catch access violations
            std::list<Creature*> nearbyCreatures;
            if (!SafeGridOperations::GetCreatureListSafe(bot, nearbyCreatures, questObjective.ObjectID, 300.0f))
            {
                TC_LOG_TRACE("module.playerbot.quest", "EngageQuestTargets: Grid search failed for bot {}", bot->GetName());
                return;
            }
            for (Creature* creature : nearbyCreatures)
            {
                // CRITICAL SAFETY: Check IsInWorld() before any operations that access Map
                // Prevents ASSERTION FAILED: m_currMap in WorldObject::GetMap (Object.h:785)
                // when creature is despawned or not yet fully added to world
                if (!creature || !creature->IsAlive() || !creature->IsInWorld())
                    continue;

                // Check if this is a FRIENDLY NPC (not hostile to bot)
                if (!bot->IsHostileTo(creature))
                {
                    // CRITICAL: Only interact with NPCs that have spell click data
                    // Neutral mobs WITHOUT spell click should be attacked, not interacted with!
                    if (!RequiresSpellClickInteraction(questObjective.ObjectID))
                    {
                        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ EngageQuestTargets: Mob {} (Entry: {}) is neutral but has NO spell click - should be ATTACKED, not interacted with!",
                                     creature->GetName(), questObjective.ObjectID);
                        continue;  // Skip this creature - it should be attacked via FindQuestTarget(), not interacted with
                    }

                    float distance = std::sqrt(bot->GetExactDistSq(creature)); // Calculate once from squared distance
                    TC_LOG_DEBUG("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} found FRIENDLY quest NPC {} (Entry: {}) with spell click at distance {:.1f}",
                                 bot->GetName(), creature->GetName(), questObjective.ObjectID, distance);

                    // CRITICAL: Check if objective is already complete BEFORE interacting
                    // GetQuestObjectiveData returns the current progress count for this objective
                    uint32 currentProgress = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
                    uint32 requiredAmount = static_cast<uint32>(questObjective.Amount);

                    if (currentProgress >= requiredAmount)
                    {
                        TC_LOG_DEBUG("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} OBJECTIVE COMPLETE ({} / {}) - skipping interaction with {}",
                                     bot->GetName(), currentProgress, requiredAmount, creature->GetName());
                        return;  // Objective complete - stop interacting
                    }

                    TC_LOG_DEBUG("module.playerbot.quest", "📊 EngageQuestTargets: Bot {} objective progress: {} / {} - interaction needed",
                                 bot->GetName(), currentProgress, requiredAmount);

                    // Check if in interaction range
                    if (distance <= INTERACTION_DISTANCE)
                    {
                        // INTERACT with the friendly NPC using spell click (right-click interaction)
                        TC_LOG_DEBUG("module.playerbot.quest", "🤝 EngageQuestTargets: Bot {} INTERACTING with friendly NPC {} for quest {} (using HandleSpellClick)",
                                     bot->GetName(), creature->GetName(), objective.questId);

                        // Right-click on the NPC triggers HandleSpellClick
                        // This is used for quest NPCs like "Injured Stormwind Infantry" that have npc_spellclick_spells
                        creature->HandleSpellClick(bot);
                        TC_LOG_DEBUG("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} sent spell click interaction to {} - quest objective should progress",
                                     bot->GetName(), creature->GetName());
                        return;
                    }
                    else
                    {
                        // Move closer to the friendly NPC
                        TC_LOG_DEBUG("module.playerbot.quest", "🚶 EngageQuestTargets: Bot {} moving to friendly NPC {} (distance: {:.1f} > INTERACTION_DISTANCE)",
                                     bot->GetName(), creature->GetName(), distance);

                        Position npcPos;
                        npcPos.Relocate(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ());
                        BotMovementUtil::MoveToPosition(bot, npcPos);
                        return;
                    }
                }
            }
        }

        // No friendly NPC found either - check if dynamic spawn is needed
        // DYNAMIC SPAWN HANDLER: Try to trigger spawn for dynamically-spawned NPCs
        if (TryTriggerDynamicSpawn(ai, objective))
        {
            TC_LOG_DEBUG("module.playerbot.quest",
                "🔮 EngageQuestTargets: Bot {} - Attempting dynamic spawn trigger for quest {} objective {}",
                bot->GetName(), objective.questId, objective.objectiveIndex);
            return;  // Dynamic spawn triggered - wait for next update to check for target
        }

        // Track failure and wait for respawns
        IncrementObjectiveFailures(objective.questId, objective.objectiveIndex);
        uint32 failures = GetObjectiveFailures(objective.questId, objective.objectiveIndex);

        if (failures >= MAX_QUEST_OBJECTIVE_FAILURES)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🚫 EngageQuestTargets: Bot {} - Quest {} objective {} BLACKLISTED after {} failures (target unreachable/doesn't exist)",
                         bot->GetName(), objective.questId, objective.objectiveIndex, failures);
            return; // Stop trying this objective, let priority system pick next one
        }

        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - NO target found (failure {}/{}, waiting for respawns)",
                     bot->GetName(), failures, MAX_QUEST_OBJECTIVE_FAILURES);

        // CRITICAL FIX: Check if quest has an area to wander in
        // If quest has multiple POI points defining an area, wander through it to search for spawns
        // Otherwise, just move to the single POI point with randomness
        if (ShouldWanderInQuestArea(ai, objective))
        {
            // Initialize wandering if not already done
            if (_questAreaWanderPoints.empty())
            {
                TC_LOG_DEBUG("module.playerbot.quest", "🗺️ EngageQuestTargets: Bot {} - Initializing quest area wandering",
                             bot->GetName());
                InitializeQuestAreaWandering(ai, objective);
            }

            // CRITICAL FIX: Check if wander points were actually loaded
            // If InitializeQuestAreaWandering failed to find matching POI blob, fall back to NavigateToObjective
            if (_questAreaWanderPoints.empty())
            {
                TC_LOG_WARN("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - Wander points failed to load, falling back to NavigateToObjective",
                             bot->GetName());
                NavigateToObjective(ai, objective);
            }
            else
            {
                // Wander through quest area to find respawns
                TC_LOG_DEBUG("module.playerbot.quest", "🚶 EngageQuestTargets: Bot {} - Wandering in quest area to search for spawns",
                             bot->GetName());
                WanderInQuestArea(ai);
            }
        }
        else
        {
            // No quest area - just navigate to objective POI with randomness
            TC_LOG_DEBUG("module.playerbot.quest", "📍 EngageQuestTargets: Bot {} - No quest area, navigating to objective POI",
                         bot->GetName());
            NavigateToObjective(ai, objective);
        }

        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} found target {} (Entry: {}) at distance {:.1f}",
                 bot->GetName(), target->GetName(), target->GetEntry(), std::sqrt(bot->GetExactDistSq(target)));

    // Target found - reset objective failure counter
    ResetObjectiveFailures(objective.questId, objective.objectiveIndex);

    // Check if we should engage this target
    if (!ShouldEngageTarget(ai, target, objective))
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - Should NOT engage target {} (already at max kills or wrong target)",
                     bot->GetName(), target->GetName());
        return;
    }
    TC_LOG_DEBUG("module.playerbot.quest", "⚔️ EngageQuestTargets: Bot {} setting combat target to {} (Entry: {})",
                 bot->GetName(), target->GetName(), target->GetEntry());

    // Set as combat target
    bot->SetTarget(target->GetGUID());

    // CRITICAL: Actually initiate combat with the target!
    // Solution from mod-playerbots: Set bot to combat state, THEN call Attack().
    // When bot is in combat state, ClassAI/combat rotation will automatically
    // start casting spells, which will damage the neutral mob and make it hostile.
    if (!bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚡ EngageQuestTargets: Bot {} not in combat - initiating attack on {} to start combat",
                     bot->GetName(), target->GetName());

        // CRITICAL: Set bot to COMBAT state BEFORE calling Attack()
        // This is the key from mod-playerbots AttackAction.cpp:160
        // When in combat state, ClassAI OnCombatUpdate() will execute rotation
        // which will cast spells and damage the neutral mob

        // 1. Set Unit combat state (makes Unit::IsInCombat() return true)
        bot->SetInCombatWith(target);

        // 2. Set BotAI state to COMBAT (triggers BotAI to call ClassAI::OnCombatUpdate())
        if (ai && ai->GetBot() == bot)
        {
            ai->SetAIState(BotAIState::COMBAT);
        }

        // 3. Now call Attack() to start the swing timer
        bot->Attack(target, true);

        // ========================================================================
        // COMBAT POSITIONING DELEGATED TO SoloCombatStrategy
        // ========================================================================
        // CRITICAL FIX: Do NOT call MovePoint/MoveChase here for ANY class!
        // SoloCombatStrategy handles ALL combat positioning with proper throttling.
        //
        // Calling movement commands from multiple systems (QuestStrategy AND
        // SoloCombatStrategy) causes stuttering at combat engagement because:
        // 1. QuestStrategy calls MovePoint → motion type = POINT
        // 2. SoloCombatStrategy calls MoveChase → motion type = CHASE
        // 3. Next frame, motion type changes detected → another movement command
        // 4. This creates a rapid oscillation causing visible stutter
        //
        // SoloCombatStrategy already handles ranged vs melee positioning correctly
        // using ClassAI::GetOptimalRange() which knows the bot's spec.
        // ========================================================================
        TC_LOG_DEBUG("module.playerbot.quest",
            "EngageQuestTargets: Bot {} (class {}) - SoloCombatStrategy will handle ALL combat positioning",
            bot->GetName(), bot->GetClass());

        TC_LOG_DEBUG("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} set to combat state and initiated attack on {} - ClassAI will handle rotation",
                     bot->GetName(), target->GetName());
    }
    else
    {
        TC_LOG_DEBUG("module.playerbot.quest", "ℹ️ EngageQuestTargets: Bot {} already in combat, letting combat AI handle target {}",
                     bot->GetName(), target->GetName());
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} successfully engaged quest mob {} for quest {}",
                 bot->GetName(), target->GetName(), objective.questId);
}

void QuestStrategy::CollectQuestItems(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CollectQuestItems: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CollectQuestItems: Bot not in world, aborting");
        return;
    }

    // CRITICAL FIX: Check for combat FIRST - combat always takes priority!
    if (bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚔️ CollectQuestItems: Bot {} IN COMBAT - aborting item collection, combat takes priority!",
                     bot->GetName());
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "📦 CollectQuestItems: Bot {} starting item collection for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Check if bot already has required items
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CollectQuestItems: Bot {} - Invalid quest {} or objective index {}",
                     bot->GetName(), objective.questId, objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} - Quest {} requires item {} (Amount: {})",
                 bot->GetName(), objective.questId, questObjective.ObjectID, questObjective.Amount);

    // Check quest objective progress
    // CRITICAL FIX: In modern WoW (TrinityCore 11.x), quest items are NOT stored in bags!
    // Quest item progress is tracked via GetQuestObjectiveData(), not GetItemCount().
    // Quest items go into a special "quest item bag" that doesn't use inventory slots.
    uint32 itemCount = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
    TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} - quest objective progress {} / {} for item {}",
                 bot->GetName(), itemCount, questObjective.Amount, questObjective.ObjectID);

    if (itemCount >= static_cast<uint32>(questObjective.Amount))
    {
        // Objective complete
        TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} completed item objective {} for quest {} ({} / {})",
                     bot->GetName(), objective.objectiveIndex, objective.questId, itemCount, questObjective.Amount);
        return;
    }

    // Find quest object to interact with
    TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} searching for quest object...",
                 bot->GetName());
    GameObject* questObject = FindQuestObject(ai, objective);

    if (!questObject)
    {
        // No object found - navigate to objective area
        TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} - NO quest object found, navigating to objective area",
                     bot->GetName());
        NavigateToObjective(ai, objective);
        return;
    }

    float distance = std::sqrt(bot->GetExactDistSq(questObject)); // Calculate once from squared distance
    TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} found quest object {} at distance {:.1f}",
                 bot->GetName(), questObject->GetEntry(), distance);

    // Move to object
    if (distance > INTERACTION_DISTANCE)
    {
        Position objPos;
        objPos.Relocate(questObject->GetPositionX(), questObject->GetPositionY(), questObject->GetPositionZ());
        TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} moving to object {} (distance: {:.1f})",
                     bot->GetName(), questObject->GetEntry(), distance);
        BotMovementUtil::MoveToPosition(bot, objPos);
        return;
    }

    // Interact with object
    TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} interacting with object {} (distance: {:.1f})",
                 bot->GetName(), questObject->GetEntry(), distance);
    bot->PrepareGossipMenu(questObject, questObject->GetGOInfo()->type == GAMEOBJECT_TYPE_QUESTGIVER ? 0 : questObject->GetGOInfo()->entry);
    bot->SendPreparedGossip(questObject);

    TC_LOG_DEBUG("module.playerbot.quest", "CollectQuestItems: Bot {} interaction sent for object {}",
                 bot->GetName(), questObject->GetEntry());
}

void QuestStrategy::ExploreQuestArea(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Get the area trigger position from the objective
    Position objectivePos = GetObjectivePosition(ai, objective);

    if (objectivePos.GetExactDist2d(0.0f, 0.0f) < 0.1f)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ ExploreQuestArea: Bot {} - NO VALID position for exploration quest {} objective {}",
                     bot->GetName(), objective.questId, objective.objectiveIndex);
        return;
    }

    // Calculate 3D distance to the area trigger center
    float distance3D = bot->GetExactDist(objectivePos);
    float distance2D = bot->GetExactDist2d(objectivePos.GetPositionX(), objectivePos.GetPositionY());
    float zDiff = std::abs(bot->GetPositionZ() - objectivePos.GetPositionZ());

    TC_LOG_DEBUG("module.playerbot.quest", "🗺️ ExploreQuestArea: Bot {} - Quest {} - Distance to area trigger: 2D={:.1f}, 3D={:.1f}, zDiff={:.1f} at ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(), objective.questId, distance2D, distance3D, zDiff,
                 objectivePos.GetPositionX(), objectivePos.GetPositionY(), objectivePos.GetPositionZ());

    // MINE/CAVE FIX: For exploration quests, we need to reach the EXACT Z level
    // The bot must be within a small radius (typically 10-20 yards) AND at the correct Z
    // to trigger the area trigger event
    if (distance3D < 5.0f)
    {
        // Bot is very close in 3D - the area trigger should fire automatically
        // when the bot enters the trigger zone
        TC_LOG_DEBUG("module.playerbot.quest", "✅ ExploreQuestArea: Bot {} is AT exploration area (3D dist {:.1f} < 5yd) - waiting for trigger",
                     bot->GetName(), distance3D);
        return;
    }

    // MINE/CAVE FIX: If we're close horizontally but far vertically, we need to go DOWN
    if (distance2D < 15.0f && zDiff > 10.0f)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🏔️ ExploreQuestArea: Bot {} is ABOVE/BELOW exploration area (2D={:.1f} < 15, zDiff={:.1f} > 10) - navigating to correct Z level",
                     bot->GetName(), distance2D, zDiff);
    }

    // Move directly to the area trigger center position (uses 3D pathfinding)
    BotMovementUtil::MoveToPosition(bot, objectivePos);
}

void QuestStrategy::UseQuestItemOnTarget(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ UseQuestItemOnTarget: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    // IsInWorld() returns false during Player destruction, preventing ACCESS_VIOLATION
    // in WorldObject::GetMap() calls
    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ UseQuestItemOnTarget: Bot not in world, aborting");
        return;
    }

    // CRITICAL FIX: Check for combat FIRST - combat always takes priority over questing!
    // This prevents bots from dying because they're trying to use quest items while being attacked.
    if (bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚔️ UseQuestItemOnTarget: Bot {} IN COMBAT - aborting quest item usage, combat takes priority!",
                     bot->GetName());
        return;
    }

    // CRITICAL FIX: Check if bot is currently casting/channeling
    // Prevents recasting on same target every tick while channeled spell is active
    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⏳ UseQuestItemOnTarget: Bot {} is CHANNELING - waiting for spell to complete",
                     bot->GetName());
        return;
    }

    if (bot->HasUnitState(UNIT_STATE_CASTING))
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⏳ UseQuestItemOnTarget: Bot {} is CASTING - waiting for spell to complete",
                     bot->GetName());
        return;
    }

    // CRITICAL FIX: Cooldown between quest item casts to prevent spam
    // This gives time for creatures to despawn and new ones to spawn
    uint32 currentTime = GameTime::GetGameTimeMS();
    if (_lastQuestItemCastTime > 0 && (currentTime - _lastQuestItemCastTime) < QUEST_ITEM_CAST_COOLDOWN_MS)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⏳ UseQuestItemOnTarget: Bot {} on cooldown - {} ms remaining",
                     bot->GetName(), QUEST_ITEM_CAST_COOLDOWN_MS - (currentTime - _lastQuestItemCastTime));
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Bot {} using quest item for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Get quest and verify it has a source item
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || quest->GetSrcItemId() == 0)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ UseQuestItemOnTarget: Quest {} has NO source item!",
                     objective.questId);
        return;
    }

    uint32 questItemId = quest->GetSrcItemId();
    TC_LOG_DEBUG("module.playerbot.quest", "📦 UseQuestItemOnTarget: Quest {} requires item {} to complete objective",
                 objective.questId, questItemId);

    // Check if bot has the quest item
    Item* questItem = bot->GetItemByEntry(questItemId);
    if (!questItem)
    {
        TC_LOG_WARN("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: Bot {} missing quest item {} - waiting for item acquisition",
                     bot->GetName(), questItemId);

        // DESIGN NOTE: Quest Objective Order Detection
        // =============================================================
        // Some quests have multi-step objectives where you must:
        //   1) Collect quest item from a nearby GameObject/Creature
        //   2) Use that item on a target at the quest POI
        //
        // Current behavior: If item is missing, return and let the quest system
        // process objectives in order — catching the "collect item" objective.
        //
        // Potential enhancement: Iterate quest->Objectives to find an earlier
        // QUEST_OBJECTIVE_ITEM that provides this item, then navigate there.
        // For now, simple return prevents skipping the item acquisition step.
        // =============================================================
        return;
    }

    // Get objective details
    if (objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ UseQuestItemOnTarget: Invalid objective index {}",
                     objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // Check if objective is already complete
    uint32 currentProgress = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
    uint32 requiredAmount = static_cast<uint32>(questObjective.Amount);

    if (currentProgress >= requiredAmount)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Objective COMPLETE ({} / {}), nothing to do",
                     currentProgress, requiredAmount);
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "📊 UseQuestItemOnTarget: Progress: {} / {} - need to use item {} more times",
                 currentProgress, requiredAmount, requiredAmount - currentProgress);

    // CRITICAL: ObjectID can be either a GameObject entry OR a Creature entry!
    // For example, Quest 26391 uses item on Creature entry 42940 (Northshire Vineyards Fire Trigger)
    // We need to try BOTH types.
    uint32 targetObjectId = questObjective.ObjectID;

    TC_LOG_DEBUG("module.playerbot.quest", "🔍 UseQuestItemOnTarget: Looking for target with ObjectID {} (could be GameObject OR Creature)",
                 targetObjectId);

    // ============================================================
    // PHASE 1: Try to find as CREATURE first (more common for use-item quests)
    // ============================================================
    ::Unit* targetCreature = nullptr;
    float nearestCreatureDistance = std::numeric_limits<float>::max();

    // Use spatial grid to find nearby creatures
    Map* map = bot->GetMap();
    if (!map)
        return;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (spatialGrid)
    {
        std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
            spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 100.0f);

        TC_LOG_DEBUG("module.playerbot.quest", "🔍 UseQuestItemOnTarget: Scanning {} nearby creatures for entry {}",
                     nearbyCreatures.size(), targetObjectId);

        // Get the set of already-used targets for this quest
        auto& usedTargets = _usedQuestItemTargets[objective.questId];

        ObjectGuid nearestCreatureGuid;
        for (auto const& snapshot : nearbyCreatures)
        {
            if (snapshot.entry != targetObjectId)
                continue;

            // CRITICAL FIX: Skip targets we've already used
            // This prevents recasting on the same creature after it despawns
            if (usedTargets.find(snapshot.guid) != usedTargets.end())
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⏭️ UseQuestItemOnTarget: Skipping ALREADY USED target GUID {}",
                             snapshot.guid.ToString());
                continue;
            }

            float distance = bot->GetExactDist(snapshot.position);
            TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Found Creature entry={} at distance {:.1f}yd",
                         snapshot.entry, distance);

            if (distance < nearestCreatureDistance)
            {
                nearestCreatureDistance = distance;
                nearestCreatureGuid = snapshot.guid;
            }
        }

        if (!nearestCreatureGuid.IsEmpty())
        {
            targetCreature = ObjectAccessor::GetCreature(*bot, nearestCreatureGuid);
            if (targetCreature)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Using CREATURE {} (entry {}) at {:.1f}yd",
                             targetCreature->GetName(), targetCreature->GetEntry(), nearestCreatureDistance);
            }
        }
    }

    // ============================================================
    // PHASE 2: If no creature found, try GameObject
    // ============================================================
    GameObject* targetObject = nullptr;
    float nearestObjectDistance = std::numeric_limits<float>::max();

    if (!targetCreature && spatialGrid)
    {
        std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
            spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), 100.0f);

        TC_LOG_DEBUG("module.playerbot.quest", "🔍 UseQuestItemOnTarget: Scanning {} nearby GameObjects for entry {}",
                     nearbyObjects.size(), targetObjectId);

        // Get the set of already-used targets for this quest
        auto& usedTargets = _usedQuestItemTargets[objective.questId];

        ObjectGuid nearestObjectGuid;
        for (auto const& snapshot : nearbyObjects)
        {
            if (snapshot.entry != targetObjectId)
                continue;

            // CRITICAL FIX: Skip targets we've already used
            if (usedTargets.find(snapshot.guid) != usedTargets.end())
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⏭️ UseQuestItemOnTarget: Skipping ALREADY USED GameObject GUID {}",
                             snapshot.guid.ToString());
                continue;
            }

            float distance = bot->GetExactDist(snapshot.position);
            TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Found GameObject entry={} at distance {:.1f}yd",
                         snapshot.entry, distance);

            if (distance < nearestObjectDistance)
            {
                nearestObjectDistance = distance;
                nearestObjectGuid = snapshot.guid;
            }
        }

        if (!nearestObjectGuid.IsEmpty())
        {
            targetObject = ObjectAccessor::GetGameObject(*bot, nearestObjectGuid);
            if (targetObject)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Using GAMEOBJECT {} (entry {}) at {:.1f}yd",
                             targetObject->GetName(), targetObject->GetEntry(), nearestObjectDistance);
            }
        }
    }

    // ============================================================
    // PHASE 3: Check if we found anything
    // ============================================================
    if (!targetCreature && !targetObject)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ UseQuestItemOnTarget: No target found for ObjectID {} (checked both Creatures and GameObjects)",
                     targetObjectId);
        NavigateToObjective(ai, objective);
        return;
    }

    // Determine which target to use (creature takes priority if both exist)
    WorldObject* targetWorldObject = targetCreature ? static_cast<WorldObject*>(targetCreature) : static_cast<WorldObject*>(targetObject);
    float currentDistance = targetCreature ? nearestCreatureDistance : nearestObjectDistance;
    bool isCreatureTarget = (targetCreature != nullptr);

    TC_LOG_DEBUG("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Target is {} (entry {}), distance {:.1f}yd",
                 isCreatureTarget ? "CREATURE" : "GAMEOBJECT",
                 isCreatureTarget ? targetCreature->GetEntry() : targetObject->GetEntry(),
                 currentDistance);

    // ============================================================
    // PHASE 4: Position and use item
    // ============================================================
    // Most quest item spells have short range (5-10 yards)
    // We need to get close before attempting to cast
    float maxUseDistance = 8.0f; // Reduced from 30 - most quest items need close range

    // Check if bot is in range
    if (currentDistance > maxUseDistance)
    {
        // TOO FAR - move closer to the target
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: Bot {} TOO FAR ({:.1f}yd > {:.1f}yd) - MOVING CLOSER",
                     bot->GetName(), currentDistance, maxUseDistance);

        // Move to a position near the target (5 yards away)
        float targetDistance = 5.0f;
        float angle = targetWorldObject->GetAbsoluteAngle(bot);
        Position closePos;
        closePos.Relocate(
            targetWorldObject->GetPositionX() + targetDistance * std::cos(angle),
            targetWorldObject->GetPositionY() + targetDistance * std::sin(angle),
            targetWorldObject->GetPositionZ()
        );

        TC_LOG_DEBUG("module.playerbot.quest", "🚶 UseQuestItemOnTarget: Bot {} moving to ({:.1f}, {:.1f}, {:.1f}) - {:.1f}yd from target",
                     bot->GetName(), closePos.GetPositionX(), closePos.GetPositionY(), closePos.GetPositionZ(), targetDistance);

        BotMovementUtil::MoveToPosition(bot, closePos);
        return;
    }

    // Bot is in range - stop, face target, and use item
    TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Bot {} IN RANGE ({:.1f}yd <= {:.1f}yd), preparing to use item {}",
                 bot->GetName(), currentDistance, maxUseDistance, questItemId);

    // CRITICAL: Stop all movement before using the item (required for spell casting)
    bot->StopMoving();
    bot->GetMotionMaster()->Clear();
    bot->GetMotionMaster()->MoveIdle();

    // Face the target
    bot->SetFacingToObject(targetWorldObject);

    TC_LOG_DEBUG("module.playerbot.quest", "👁️ UseQuestItemOnTarget: Bot {} now facing target {} (entry {})",
                 bot->GetName(),
                 isCreatureTarget ? "Creature" : "GameObject",
                 isCreatureTarget ? targetCreature->GetEntry() : targetObject->GetEntry());

    // Get the spell ID from the quest item
    // Quest items trigger spells through their ItemEffect entries
    uint32 spellId = 0;
    for (ItemEffectEntry const* itemEffect : questItem->GetEffects())
    {
        if (itemEffect->TriggerType == ITEM_SPELLTRIGGER_ON_USE)
        {
            spellId = itemEffect->SpellID;
            break;
        }
    }

    if (spellId == 0)
    {
        // ========================================================================
        // FALLBACK: Item has no ON_USE spell - try spellclick on target creature
        // This handles quests like "Fear No Evil" where item 65733's DB2 data
        // isn't loaded, but creature 50047 has a spellclick spell (93072).
        // ========================================================================
        if (isCreatureTarget && targetCreature)
        {
            uint32 creatureEntry = targetCreature->GetEntry();
            auto clickBounds = sObjectMgr->GetSpellClickInfoMapBounds(creatureEntry);

            for (auto const& [entryId, spellClickInfo] : clickBounds)
            {
                // Check if this spellclick is valid for the bot
                if (!spellClickInfo.IsFitToRequirements(bot, targetCreature))
                    continue;

                TC_LOG_DEBUG("module.playerbot.quest",
                    "🔧 UseQuestItemOnTarget: Item {} has no spell, using SPELLCLICK fallback - creature {} has spellclick spell {}",
                    questItemId, creatureEntry, spellClickInfo.spellId);

                // Use HandleSpellClick to trigger the spellclick spell
                targetCreature->HandleSpellClick(bot);

                // Track this target as used
                _usedQuestItemTargets[objective.questId].insert(targetCreature->GetGUID());
                _lastQuestItemCastTime = GameTime::GetGameTimeMS();

                TC_LOG_DEBUG("module.playerbot.quest",
                    "✅ UseQuestItemOnTarget: Bot {} triggered SPELLCLICK on creature {} (GUID: {}) - objective should progress",
                    bot->GetName(), creatureEntry, targetCreature->GetGUID().ToString());
                return;
            }

            TC_LOG_DEBUG("module.playerbot.quest",
                "❌ UseQuestItemOnTarget: Quest item {} has no ON_USE spell AND creature {} has no valid spellclick!",
                questItemId, creatureEntry);
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "❌ UseQuestItemOnTarget: Quest item {} has no ON_USE spell!",
                         questItemId);
        }
        return;
    }
    TC_LOG_DEBUG("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Quest item {} triggers spell {}",
                 questItemId, spellId);

    // Get spell info for validation
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    (void)spellInfo; // Suppress unused warning

    TC_LOG_DEBUG("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Casting spell {} on {} (entry {})",
                 spellId,
                 isCreatureTarget ? "Creature" : "GameObject",
                 isCreatureTarget ? targetCreature->GetEntry() : targetObject->GetEntry());

    // Cast the spell with the item as the source and target (Creature or GameObject)
    // Use CastSpellExtraArgs to pass the item that's being used
    CastSpellExtraArgs args;
    args.SetCastItem(questItem);
    args.SetOriginalCaster(bot->GetGUID());

    // Cast on the correct target type and track the used target
    if (isCreatureTarget)
    {
        bot->CastSpell(targetCreature, spellId, args);

        // CRITICAL FIX: Track this target as used to prevent recasting on it
        _usedQuestItemTargets[objective.questId].insert(targetCreature->GetGUID());
        _lastQuestItemCastTime = GameTime::GetGameTimeMS();

        TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Bot {} cast spell {} from item {} on CREATURE {} (GUID tracked: {}) - objective should progress",
                     bot->GetName(), spellId, questItemId, targetCreature->GetEntry(), targetCreature->GetGUID().ToString());
    }
    else
    {
        bot->CastSpell(targetObject, spellId, args);

        // CRITICAL FIX: Track this target as used to prevent recasting on it
        _usedQuestItemTargets[objective.questId].insert(targetObject->GetGUID());
        _lastQuestItemCastTime = GameTime::GetGameTimeMS();

        TC_LOG_DEBUG("module.playerbot.quest", "✅ UseQuestItemOnTarget: Bot {} cast spell {} from item {} on GAMEOBJECT {} (GUID tracked: {}) - objective should progress",
                     bot->GetName(), spellId, questItemId, targetObject->GetEntry(), targetObject->GetGUID().ToString());
    }
}

// ============================================================================
// TalkToNpc - Handler for QUEST_OBJECTIVE_TALKTO objectives
// These require the bot to find and interact with a specific NPC
// ============================================================================
void QuestStrategy::TalkToNpc(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ TalkToNpc: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ TalkToNpc: Bot not in world, aborting");
        return;
    }

    // Check for combat - combat takes priority
    if (bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚔️ TalkToNpc: Bot {} IN COMBAT - aborting, combat takes priority!",
                     bot->GetName());
        return;
    }

    // Get the quest objective details
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ TalkToNpc: Invalid quest {} or objective index {}",
                     objective.questId, objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];
    uint32 npcEntry = static_cast<uint32>(questObjective.ObjectID);

    TC_LOG_DEBUG("module.playerbot.quest", "🗣️ TalkToNpc: Bot {} looking for NPC entry {} for quest {} objective {}",
                 bot->GetName(), npcEntry, objective.questId, objective.objectiveIndex);

    // Check if we're already near the NPC
    Creature* targetNpc = bot->FindNearestCreature(npcEntry, 100.0f);

    if (targetNpc)
    {
        float distance = bot->GetExactDist(targetNpc);
        TC_LOG_DEBUG("module.playerbot.quest", "✅ TalkToNpc: Found NPC {} (entry {}) at distance {:.1f}yd",
                     targetNpc->GetName(), npcEntry, distance);

        if (distance < 5.0f)
        {
            // We're close enough - interact with the NPC via gossip
            TC_LOG_DEBUG("module.playerbot.quest", "🗣️ TalkToNpc: Bot {} interacting with NPC {} for TALKTO objective",
                         bot->GetName(), targetNpc->GetName());

            // Send gossip hello to trigger the quest objective
            // This simulates clicking on the NPC
            // Note: GossipMenuIds is a vector in modern TrinityCore - use first menu if available
            auto const& gossipMenuIds = targetNpc->GetCreatureTemplate()->GossipMenuIds;
            uint32 gossipMenuId = gossipMenuIds.empty() ? 0 : gossipMenuIds[0];
            bot->PrepareGossipMenu(targetNpc, gossipMenuId, true);
            bot->SendPreparedGossip(targetNpc);

            // For some TALKTO objectives, simply being near the NPC completes it
            // The objective tracking will update automatically via the server
            TC_LOG_DEBUG("module.playerbot.quest", "✅ TalkToNpc: Bot {} sent gossip hello to {} - objective should progress",
                         bot->GetName(), targetNpc->GetName());
        }
        else
        {
            // Move closer to the NPC
            TC_LOG_DEBUG("module.playerbot.quest", "🚶 TalkToNpc: Bot {} moving to NPC {} ({:.1f}yd away)",
                         bot->GetName(), targetNpc->GetName(), distance);
            BotMovementUtil::MoveToUnit(bot, targetNpc, 3.0f);
        }
    }
    else
    {
        // NPC not nearby - navigate to objective location
        TC_LOG_DEBUG("module.playerbot.quest", "🗺️ TalkToNpc: NPC entry {} not nearby, navigating to objective location",
                     npcEntry);
        NavigateToObjective(ai, objective);
    }
}

// ============================================================================
// HandleCurrencyObjective - Handler for QUEST_OBJECTIVE_HAVE_CURRENCY objectives
// Bot needs to have a certain amount of currency when turning in the quest
// ============================================================================
void QuestStrategy::HandleCurrencyObjective(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ HandleCurrencyObjective: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ HandleCurrencyObjective: Bot not in world, aborting");
        return;
    }

    // Get the quest objective details
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ HandleCurrencyObjective: Invalid quest {} or objective index {}",
                     objective.questId, objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];
    uint32 currencyId = static_cast<uint32>(questObjective.ObjectID);
    uint32 requiredAmount = static_cast<uint32>(questObjective.Amount);

    // Check bot's current currency amount
    uint32 currentAmount = bot->GetCurrencyQuantity(currencyId);

    TC_LOG_DEBUG("module.playerbot.quest", "💰 HandleCurrencyObjective: Bot {} checking currency {} - has {} need {}",
                 bot->GetName(), currencyId, currentAmount, requiredAmount);

    if (currentAmount >= requiredAmount)
    {
        // Bot has enough currency - quest should be completable
        // The objective is satisfied, so the quest can be turned in
        TC_LOG_DEBUG("module.playerbot.quest", "✅ HandleCurrencyObjective: Bot {} has sufficient currency {} ({}/{})",
                     bot->GetName(), currencyId, currentAmount, requiredAmount);

        // Check if quest is ready to turn in
        QuestStatus status = bot->GetQuestStatus(objective.questId);
        if (status == QUEST_STATUS_COMPLETE)
        {
            TurnInQuest(ai, objective.questId);
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "📍 HandleCurrencyObjective: Quest {} not complete yet (status={}), waiting...",
                         objective.questId, static_cast<int>(status));
        }
    }
    else
    {
        // Bot doesn't have enough currency yet
        // Currency is typically gained through gameplay (dungeons, world quests, etc.)
        TC_LOG_DEBUG("module.playerbot.quest", "⏳ HandleCurrencyObjective: Bot {} needs more currency {} ({}/{}) - continuing gameplay",
                     bot->GetName(), currencyId, currentAmount, requiredAmount);

        // Navigate to quest area to earn currency
        NavigateToObjective(ai, objective);
    }
}

// ============================================================================
// HandleMoneyObjective - Handler for QUEST_OBJECTIVE_MONEY objectives
// Bot needs to have a certain amount of gold/silver/copper
// ============================================================================
void QuestStrategy::HandleMoneyObjective(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ HandleMoneyObjective: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ HandleMoneyObjective: Bot not in world, aborting");
        return;
    }

    // Get the quest objective details
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ HandleMoneyObjective: Invalid quest {} or objective index {}",
                     objective.questId, objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];
    uint32 requiredMoney = static_cast<uint32>(questObjective.Amount);  // In copper

    // Check bot's current money
    uint32 currentMoney = bot->GetMoney();

    // Format for logging (gold.silver.copper)
    uint32 reqGold = requiredMoney / 10000;
    uint32 reqSilver = (requiredMoney % 10000) / 100;
    uint32 reqCopper = requiredMoney % 100;

    uint32 curGold = currentMoney / 10000;
    uint32 curSilver = (currentMoney % 10000) / 100;
    uint32 curCopper = currentMoney % 100;

    TC_LOG_DEBUG("module.playerbot.quest", "💵 HandleMoneyObjective: Bot {} checking money - has {}g{}s{}c need {}g{}s{}c",
                 bot->GetName(), curGold, curSilver, curCopper, reqGold, reqSilver, reqCopper);

    if (currentMoney >= requiredMoney)
    {
        // Bot has enough money - quest should be completable
        TC_LOG_DEBUG("module.playerbot.quest", "✅ HandleMoneyObjective: Bot {} has sufficient money",
                     bot->GetName());

        // Check if quest is ready to turn in
        QuestStatus status = bot->GetQuestStatus(objective.questId);
        if (status == QUEST_STATUS_COMPLETE)
        {
            TurnInQuest(ai, objective.questId);
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "📍 HandleMoneyObjective: Quest {} not complete yet (status={}), waiting...",
                         objective.questId, static_cast<int>(status));
        }
    }
    else
    {
        // Bot doesn't have enough money yet
        // Money is gained through gameplay (loot, quest rewards, selling items)
        TC_LOG_DEBUG("module.playerbot.quest", "⏳ HandleMoneyObjective: Bot {} needs more money ({} copper short) - continuing gameplay",
                     bot->GetName(), requiredMoney - currentMoney);

        // Navigate to quest area or continue normal gameplay to earn money
        NavigateToObjective(ai, objective);
    }
}

void QuestStrategy::TurnInQuest(BotAI* ai, uint32 questId)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    if (!bot->IsInWorld())
        return;

    // CRITICAL FIX: Check for combat FIRST - combat always takes priority!
    if (bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚔️ TurnInQuest: Bot {} IN COMBAT - aborting turn-in, combat takes priority!",
                     bot->GetName());
        return;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    TC_LOG_DEBUG("module.playerbot.quest", "TurnInQuest: Bot {} attempting to turn in quest {} ({})",
                 bot->GetName(), questId, quest->GetLogTitle());

    // Pre-check: does a quest ender exist in the DB at all?
    // If no creature or gameobject is registered as quest ender, abandon immediately.
    QuestRelationResult creatureEnders = sObjectMgr->GetCreatureQuestInvolvedRelations(questId);
    QuestRelationResult goEnders = sObjectMgr->GetGOQuestInvolvedRelations(questId);
    bool hasAnyEnder = creatureEnders.begin() != creatureEnders.end() ||
                       goEnders.begin() != goEnders.end();

    if (!hasAnyEnder)
    {
        TC_LOG_WARN("module.playerbot.quest",
            "TurnInQuest: Bot {} abandoning quest {} '{}' — no quest ender exists in DB",
            bot->GetName(), questId, quest->GetLogTitle());
        bot->RemoveActiveQuest(questId);
        bot->AbandonQuest(questId);
        if (_acceptanceManager)
            _acceptanceManager->BlacklistQuest(questId);
        return;
    }

    // Step 1: Find quest ender location (multi-tier fallback)
    QuestEnderLocation location;
    if (!FindQuestEnderLocation(ai, questId, location))
    {
        // Track failures to prevent infinite loop on quests with no reachable ender
        _questTurnInFailures[questId]++;
        uint32 failureCount = _questTurnInFailures[questId];

        TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to find quest ender location for quest {} (failure {}/{})",
                     bot->GetName(), questId, failureCount, MAX_QUEST_TURNIN_FAILURES);

        if (failureCount >= MAX_QUEST_TURNIN_FAILURES)
        {
            TC_LOG_WARN("module.playerbot.quest",
                "TurnInQuest: Bot {} abandoning quest {} after {} failed ender location searches",
                bot->GetName(), questId, failureCount);

            bot->RemoveActiveQuest(questId);
            bot->AbandonQuest(questId);
            _questTurnInFailures.erase(questId);

            if (_acceptanceManager)
                _acceptanceManager->BlacklistQuest(questId);
        }
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ TurnInQuest: Bot {} found quest ender {} {} at ({:.1f}, {:.1f}, {:.1f}) - foundViaSpawn={}, foundViaPOI={}, requiresSearch={}, isOnDifferentMap={}, targetMapId={}",
                 bot->GetName(),
                 location.IsGameObject() ? "GameObject" : "NPC",
                 location.objectEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 location.foundViaSpawn, location.foundViaPOI, location.requiresSearch,
                 location.isOnDifferentMap, location.targetMapId);

    // Step 1B: Handle cross-map quest enders (quest ender on different map than bot)
    if (location.RequiresMapTravel())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🗺️ TurnInQuest: Bot {} needs to travel to MAP {} to turn in quest {} (currently on map {}) - quest ender {} {} at ({:.1f}, {:.1f}, {:.1f})",
                     bot->GetName(),
                     location.targetMapId,
                     questId,
                     bot->GetMapId(),
                     location.IsGameObject() ? "GameObject" : "NPC",
                     location.objectEntry,
                     location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());

        // ========================================================================
        // CROSS-MAP TRAVEL SYSTEM - Multi-method travel resolver
        // ========================================================================
        // Priority order:
        // 1. Hearthstone (if bind is on target map and ready)
        // 2. Flight path (if flight path can reach target map)
        // 3. Defer turn-in (continue other objectives until travel is possible)

        bool travelInitiated = false;

        // ========================================================================
        // TRAVEL METHOD 1: Hearthstone (using Player's native API)
        // ========================================================================
        // Check if bot's hearthstone bind location is on the target map
        static constexpr uint32 HEARTHSTONE_SPELL_ID = 8690;
        WorldLocation const& homebindLoc = bot->m_homebind;  // Direct access to public member
        uint32 homebindMapId = homebindLoc.GetMapId();
        bool homebindValid = (homebindMapId != 0 || homebindLoc.GetPositionX() != 0.0f || homebindLoc.GetPositionY() != 0.0f);

        if (homebindValid && homebindMapId == location.targetMapId)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🏠 TurnInQuest: Bot {} hearthstone is bound to MAP {} (target map) - checking if hearthstone is ready",
                         bot->GetName(), homebindMapId);

            // Check if hearthstone is on cooldown
            SpellHistory* spellHistory = bot->GetSpellHistory();
            bool hearthstoneReady = spellHistory && !spellHistory->HasCooldown(HEARTHSTONE_SPELL_ID);

            if (hearthstoneReady)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✨ TurnInQuest: Bot {} using HEARTHSTONE to travel to MAP {} for quest {} turn-in",
                             bot->GetName(), location.targetMapId, questId);

                // Cast hearthstone spell
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(HEARTHSTONE_SPELL_ID, DIFFICULTY_NONE);
                if (spellInfo)
                {
                    Spell* spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
                    SpellCastTargets targets;
                    targets.SetUnitTarget(bot);

                    SpellCastResult result = spell->prepare(targets);
                    if (result == SPELL_CAST_OK)
                    {
                        TC_LOG_DEBUG("module.playerbot.quest", "✅ TurnInQuest: Bot {} hearthstone activated - will complete quest {} turn-in after arrival on MAP {}",
                                     bot->GetName(), questId, location.targetMapId);
                        travelInitiated = true;
                    }
                    else
                    {
                        TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} hearthstone cast failed with result {}",
                                     bot->GetName(), static_cast<uint32>(result));
                    }
                }
            }
            else
            {
                // Get cooldown remaining
                uint32 cooldownMs = 0;
                if (spellHistory)
                {
                    SpellInfo const* hsInfo = sSpellMgr->GetSpellInfo(HEARTHSTONE_SPELL_ID, DIFFICULTY_NONE);
                    if (hsInfo)
                    {
                        auto remaining = spellHistory->GetRemainingCooldown(hsInfo);
                        cooldownMs = static_cast<uint32>(remaining.count());
                    }
                }
                TC_LOG_DEBUG("module.playerbot.quest", "⏳ TurnInQuest: Bot {} hearthstone on cooldown ({} seconds remaining) - deferring quest {} turn-in",
                             bot->GetName(), cooldownMs / 1000, questId);
            }
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🏠 TurnInQuest: Bot {} hearthstone bound to MAP {} (not target MAP {}) - cannot use hearthstone for travel",
                         bot->GetName(), homebindMapId, location.targetMapId);
        }

        // ========================================================================
        // TRAVEL METHOD 2: Flight Path (includes ships, zeppelins, portals via taxi system)
        // ========================================================================
        // TrinityCore represents ships, zeppelins, and some portals as taxi nodes
        // These ARE able to cross continent/map boundaries
        if (!travelInitiated)
        {
            // Find nearest taxi node on target map to quest ender (faction-aware)
            uint32 destinationTaxiNode = FlightMasterManager::FindNearestTaxiNode(location.position, location.targetMapId, bot);

            if (destinationTaxiNode != 0)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✈️ TurnInQuest: Bot {} found taxi node {} near quest ender on MAP {} - checking for transport route",
                             bot->GetName(), destinationTaxiNode, location.targetMapId);

                // Find nearest flight master to bot on current map
                auto flightMasterOpt = FlightMasterManager::FindNearestFlightMaster(bot);

                if (flightMasterOpt.has_value())
                {
                    FlightMasterLocation const& fm = flightMasterOpt.value();

                    TC_LOG_DEBUG("module.playerbot.quest", "✈️ TurnInQuest: Bot {} found flight master '{}' (node {}) at {:.1f} yards - checking path to node {}",
                                 bot->GetName(), fm.name, fm.taxiNode, fm.distanceFromPlayer, destinationTaxiNode);

                    // CRITICAL FIX: Check if player has discovered the destination taxi node BEFORE
                    // deciding to navigate to flight master. This prevents the infinite loop where:
                    // 1. Bot walks to flight master
                    // 2. Flight fails with NODE_UNKNOWN (node not discovered)
                    // 3. Falls back to portal route, walks toward portal
                    // 4. Next update sees flight master is far, walks BACK to flight master
                    // 5. Repeat forever
                    bool destinationNodeDiscovered = FlightMasterManager::HasTaxiNode(bot, destinationTaxiNode);

                    if (!destinationNodeDiscovered)
                    {
                        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ TurnInQuest: Bot {} has NOT discovered destination taxi node {} - skipping direct flight, will use multi-station travel (portal/ship/etc)",
                                     bot->GetName(), destinationTaxiNode);
                        // Don't set travelInitiated - fall through to multi-station travel below
                    }
                    else
                    {
                        // Check if a taxi path exists (this includes zeppelin/ship routes)
                        auto pathOpt = FlightMasterManager::CalculateFlightPath(bot, fm.taxiNode, destinationTaxiNode, FlightPathStrategy::SHORTEST_DISTANCE);

                        if (pathOpt.has_value())
                        {
                            FlightPathInfo const& path = pathOpt.value();

                            TC_LOG_DEBUG("module.playerbot.quest", "✈️ TurnInQuest: Bot {} found transport route with {} stops, cost {} copper, ~{} seconds",
                                         bot->GetName(), path.stopCount, path.goldCost, path.flightTime);

                            // Check if bot is close enough to interact with flight master
                            if (fm.distanceFromPlayer < 10.0f)
                            {
                                // Bot is at flight master - initiate the flight
                                FlightMasterManager flightMgr(bot);
                                FlightResult result = flightMgr.FlyToTaxiNode(bot, destinationTaxiNode, FlightPathStrategy::SHORTEST_DISTANCE);
                                if (result == FlightResult::SUCCESS)
                                {
                                    TC_LOG_DEBUG("module.playerbot.quest", "✅ TurnInQuest: Bot {} initiated transport to MAP {} via taxi node {}",
                                                 bot->GetName(), location.targetMapId, destinationTaxiNode);
                                    travelInitiated = true;
                                }
                                else
                                {
                                    TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to initiate flight: {}",
                                                 bot->GetName(), FlightMasterManager::GetResultString(result));
                                }
                            }
                            else
                            {
                                // Navigate to flight master first
                                TC_LOG_DEBUG("module.playerbot.quest", "🚶 TurnInQuest: Bot {} navigating to flight master '{}' at ({:.1f}, {:.1f}, {:.1f})",
                                             bot->GetName(), fm.name,
                                             fm.position.GetPositionX(), fm.position.GetPositionY(), fm.position.GetPositionZ());

                                if (BotMovementUtil::MoveToPosition(bot, fm.position))
                                {
                                    TC_LOG_DEBUG("module.playerbot.quest", "✅ TurnInQuest: Bot {} moving to flight master - will take transport to MAP {} after arrival",
                                                 bot->GetName(), location.targetMapId);
                                    travelInitiated = true;
                                }
                            }
                        }
                        else
                        {
                            TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} no transport route found from node {} to node {} (may need to discover nodes or use portal)",
                                         bot->GetName(), fm.taxiNode, destinationTaxiNode);
                        }
                    }
                }
                else
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} no flight master found on current map {} - searching for transport NPCs",
                                 bot->GetName(), bot->GetMapId());
                }
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} no taxi node found near quest ender position on MAP {}",
                             bot->GetName(), location.targetMapId);
            }
        }

        // ========================================================================
        // TRAVEL METHOD 3: Multi-Station Travel (ships, zeppelins, portals)
        // ========================================================================
        // Use PERSISTENT TravelRouteManager for complex multi-hop routes.
        // The manager handles state across update ticks for proper transport boarding:
        // WALKING_TO_TRANSPORT -> WAITING_FOR_TRANSPORT -> ON_TRANSPORT -> ARRIVING
        if (!travelInitiated)
        {
            // Check for travel failure cooldown to prevent infinite loops
            uint32 now = GameTime::GetGameTimeMS();
            auto failIt = _travelFailures.find(questId);
            if (failIt != _travelFailures.end())
            {
                TravelFailureInfo& failInfo = failIt->second;

                // Check if max failures reached
                if (failInfo.failureCount >= MAX_TRAVEL_FAILURES)
                {
                    TC_LOG_WARN("module.playerbot.quest",
                        "⛔ TurnInQuest: Bot {} giving up on quest {} travel - max failures ({}) reached",
                        bot->GetName(), questId, MAX_TRAVEL_FAILURES);
                    return; // Skip this quest, try other quests
                }

                // Check cooldown
                uint32 timeSinceFailure = now - failInfo.lastFailureTime;
                if (timeSinceFailure < TRAVEL_FAILURE_COOLDOWN_MS)
                {
                    uint32 remainingCooldown = (TRAVEL_FAILURE_COOLDOWN_MS - timeSinceFailure) / 1000;
                    TC_LOG_DEBUG("module.playerbot.quest",
                        "⏳ TurnInQuest: Bot {} travel for quest {} on cooldown - {}s remaining (attempt {}/{})",
                        bot->GetName(), questId, remainingCooldown, failInfo.failureCount, MAX_TRAVEL_FAILURES);
                    return; // On cooldown, try other quests
                }
            }

            TC_LOG_DEBUG("module.playerbot.quest", "🚢 TurnInQuest: Bot {} attempting multi-station travel planning to MAP {}",
                         bot->GetName(), location.targetMapId);

            // Create persistent TravelRouteManager if needed
            if (!_travelManager)
            {
                _travelManager = std::make_unique<TravelRouteManager>(bot);
            }

            if (_travelManager->CanReachMap(bot->GetMapId(), location.targetMapId))
            {
                TravelRoute route = _travelManager->PlanRoute(location.targetMapId, location.position);

                if (!route.legs.empty() && route.overallState != TravelState::FAILED)
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "✈️ TurnInQuest: Bot {} planned {}-leg multi-station route: {}",
                                 bot->GetName(), route.totalLegs, route.description);
                    TC_LOG_DEBUG("module.playerbot.quest", "   📊 Estimated: {} seconds, {} copper",
                                 route.totalEstimatedTimeSeconds, route.totalEstimatedCostCopper);

                    // Log each leg
                    for (auto const& leg : route.legs)
                    {
                        std::string typeStr;
                        switch (leg.type)
                        {
                            case TransportType::TAXI_FLIGHT: typeStr = "Taxi"; break;
                            case TransportType::SHIP: typeStr = "Ship"; break;
                            case TransportType::ZEPPELIN: typeStr = "Zeppelin"; break;
                            case TransportType::PORTAL: typeStr = "Portal"; break;
                            case TransportType::BOAT: typeStr = "Boat"; break;
                            case TransportType::HEARTHSTONE: typeStr = "Hearthstone"; break;
                            case TransportType::WALK: typeStr = "Walk"; break;
                            default: typeStr = "Unknown"; break;
                        }
                        TC_LOG_INFO("module.playerbot.quest", "   📍 Leg {}: {} - {} (MAP {} -> MAP {})",
                                     leg.legIndex + 1, typeStr, leg.description,
                                     leg.startMapId, leg.endMapId);
                    }

                    // START THE ROUTE - TravelRouteManager handles all leg progression!
                    // The UpdateBehavior() loop will call _travelManager->Update() each tick
                    // to progress through: WALKING -> WAITING -> ON_TRANSPORT -> ARRIVING
                    if (_travelManager->StartRoute(std::move(route)))
                    {
                        _lastTravelQuestId = questId;
                        TC_LOG_INFO("module.playerbot.quest",
                            "✅ TurnInQuest: Bot {} started multi-station travel for quest {} - travel manager will handle journey",
                            bot->GetName(), questId);
                        travelInitiated = true;
                        // Return immediately - UpdateBehavior's travel update loop takes over
                        return;
                    }
                    else
                    {
                        TC_LOG_WARN("module.playerbot.quest",
                            "❌ TurnInQuest: Bot {} failed to start route for quest {}",
                            bot->GetName(), questId);
                        _travelManager.reset();
                    }
                }
                else
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to plan multi-station route to MAP {}",
                                 bot->GetName(), location.targetMapId);
                }
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} cannot reach MAP {} from MAP {} via transport network",
                             bot->GetName(), location.targetMapId, bot->GetMapId());
            }
        }

        // ========================================================================
        // TRAVEL METHOD 4: Defer Turn-in (Last Resort)
        // ========================================================================
        if (!travelInitiated)
        {
            // Log comprehensive diagnostic information for cross-map quest
            TC_LOG_DEBUG("module.playerbot.quest", "⏸️ TurnInQuest: Bot {} DEFERRING quest {} turn-in - all travel methods exhausted",
                         bot->GetName(), questId);
            TC_LOG_DEBUG("module.playerbot.quest", "   📍 Current: MAP {} at ({:.1f}, {:.1f}, {:.1f})",
                         bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            TC_LOG_DEBUG("module.playerbot.quest", "   🎯 Target:  MAP {} at ({:.1f}, {:.1f}, {:.1f}) - {} {}",
                         location.targetMapId,
                         location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                         location.IsGameObject() ? "GameObject" : "NPC", location.objectEntry);
            TC_LOG_DEBUG("module.playerbot.quest", "   🏠 Hearthstone: MAP {} ({})",
                         homebindMapId, homebindValid ? (homebindMapId == location.targetMapId ? "TARGET MAP - but on cooldown" : "different map") : "invalid");
            TC_LOG_DEBUG("module.playerbot.quest", "   🚢 Multi-station: No viable route found");
            TC_LOG_DEBUG("module.playerbot.quest", "   💡 Recommendation: Bot should acquire quests on current map or wait for travel opportunity");
        }

        // Return regardless - either travel was initiated or deferred
        return;
    }

    // Step 2: Check if quest ender (NPC or GameObject) is already in range
    if (CheckForQuestEnderInRange(ai, location))
    {
        // Quest ender is in range - complete turn-in immediately
        TC_LOG_DEBUG("module.playerbot.quest", "✅ TurnInQuest: Bot {} found quest ender {} {} in range, completing turn-in immediately",
                     bot->GetName(),
                     location.IsGameObject() ? "GameObject" : "NPC",
                     location.objectEntry);
        return; // CheckForQuestEnderInRange() handles the turn-in
    }

    // Step 3: Navigate to quest ender location
    if (!NavigateToQuestEnder(ai, location))
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to navigate to quest ender {} {}",
                     bot->GetName(),
                     location.IsGameObject() ? "GameObject" : "NPC",
                     location.objectEntry);
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "🚶 TurnInQuest: Bot {} navigating to quest ender {} {} at ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(),
                 location.IsGameObject() ? "GameObject" : "NPC",
                 location.objectEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());

    // Navigation is in progress - next UpdateBehavior() cycle will check for NPC in range
}

ObjectivePriority QuestStrategy::GetCurrentObjective(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return ObjectivePriority(0, 0, 0.0f);

    // CRITICAL FIX: Check BOTH GameSystems AND ObjectiveTracker for null
    // ObjectiveTracker is NOT created in instance-only mode (JIT bots for BG/LFG)
    // This prevents ACCESS_VIOLATION crash when ObjectiveTracker is null or destroyed
    auto* gameSystems = ai->GetGameSystems();
    if (!gameSystems)
        return ObjectivePriority(0, 0, 0.0f);

    auto* tracker = gameSystems->GetObjectiveTracker();
    if (!tracker)
        return ObjectivePriority(0, 0, 0.0f);

    Player* bot = ai->GetBot();
    return tracker->GetHighestPriorityObjective(bot);
}

bool QuestStrategy::HasActiveObjectives(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    ObjectivePriority priority = GetCurrentObjective(ai);
    return priority.questId != 0;
}

bool QuestStrategy::ShouldEngageTarget(BotAI* ai, ::Unit* target, ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot() || !target)
        return false;

    Player* bot = ai->GetBot();

    // Check if target is quest target
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return false;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // Handle MONSTER objectives (kill creature)
    if (questObjective.Type == QUEST_OBJECTIVE_MONSTER)
    {
        if (target->GetEntry() != questObjective.ObjectID)
            return false;

        // Check if already at max kills
        uint32 currentKills = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
        if (currentKills >= static_cast<uint32>(questObjective.Amount))
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ShouldEngageTarget: Bot {} - MONSTER objective complete ({}/{}) for quest {}",
                         bot->GetName(), currentKills, questObjective.Amount, objective.questId);
            return false;
        }

        TC_LOG_DEBUG("module.playerbot.quest", "✅ ShouldEngageTarget: Bot {} - MONSTER objective {}/{} for quest {}, target {} matches",
                     bot->GetName(), currentKills, questObjective.Amount, objective.questId, target->GetName());
        return true;
    }

    // Handle ITEM objectives (kill creature that drops quest item)
    // This is for quests like "Replenishing the Healing Crystals" (9280)
    // where bots need to kill Vale Moth (16520) to loot item 22889
    if (questObjective.Type == QUEST_OBJECTIVE_ITEM)
    {
        uint32 itemId = questObjective.ObjectID;

        // Look up which creature drops this item (with caching)
        static std::unordered_map<uint32, uint32> itemToCreatureCache;
        static std::recursive_mutex cacheMutex;

        uint32 targetCreatureEntry = 0;

        // Check cache first
        {
            std::lock_guard lock(cacheMutex);
            auto cacheIt = itemToCreatureCache.find(itemId);
            if (cacheIt != itemToCreatureCache.end())
            {
                targetCreatureEntry = cacheIt->second;
            }
        }

        // If not in cache, query database
        if (targetCreatureEntry == 0)
        {
            QueryResult result = WorldDatabase.PQuery("SELECT Entry FROM creature_loot_template WHERE Item = {} LIMIT 1", itemId);
            if (result)
            {
                Field* fields = result->Fetch();
                targetCreatureEntry = fields[0].GetUInt32();

                // Cache the result
                {
                    std::lock_guard lock(cacheMutex);
                    itemToCreatureCache[itemId] = targetCreatureEntry;
                }
            }
            else
            {
                // Item doesn't drop from creatures - not an engage target
                TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ShouldEngageTarget: Bot {} - ITEM {} doesn't drop from creatures",
                             bot->GetName(), itemId);
                return false;
            }
        }

        // Check if target is the creature that drops this item
        if (target->GetEntry() != targetCreatureEntry)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ShouldEngageTarget: Bot {} - Target {} (entry {}) doesn't drop item {} (need entry {})",
                         bot->GetName(), target->GetName(), target->GetEntry(), itemId, targetCreatureEntry);
            return false;
        }

        // Check if already have enough items
        uint32 currentItems = bot->GetItemCount(itemId, false);
        uint32 requiredItems = static_cast<uint32>(questObjective.Amount);
        if (currentItems >= requiredItems)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ShouldEngageTarget: Bot {} - ITEM objective complete ({}/{}) for quest {}",
                         bot->GetName(), currentItems, requiredItems, objective.questId);
            return false;
        }

        TC_LOG_DEBUG("module.playerbot.quest", "✅ ShouldEngageTarget: Bot {} - ITEM objective {}/{} for quest {}, target {} drops item {}",
                     bot->GetName(), currentItems, requiredItems, objective.questId, target->GetName(), itemId);
        return true;
    }

    // Other objective types are not engage targets
    TC_LOG_DEBUG("module.playerbot.quest", "⚠️ ShouldEngageTarget: Bot {} - Objective type {} not handled for engagement",
                 bot->GetName(), static_cast<uint32>(questObjective.Type));
    return false;
}

bool QuestStrategy::MoveToObjectiveLocation(BotAI* ai, Position const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // MINE/CAVE FIX: Use 3D distance for arrival check
    // Previously used 2D distance which caused bots to think they arrived at the mine entrance
    // when the actual spawn was directly below at a lower Z level inside the mine
    float distance2D = bot->GetExactDist2d(location.GetPositionX(), location.GetPositionY());
    float distance3D = bot->GetExactDist(location);
    float zDiff = std::abs(bot->GetPositionZ() - location.GetPositionZ());

    // Consider arrived only if within 10 yards in 3D space
    // This ensures bots will continue moving to reach proper Z level inside mines
    if (distance3D < 10.0f)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "✅ MoveToObjectiveLocation: Bot {} arrived (3D dist {:.1f} < 10yd)",
                     bot->GetName(), distance3D);
        return true;
    }

    // Log movement progress for debugging mine pathing
    if (zDiff > 5.0f)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🏔️ MoveToObjectiveLocation: Bot {} moving with significant Z difference - dist2D={:.1f} dist3D={:.1f} zDiff={:.1f}",
                     bot->GetName(), distance2D, distance3D, zDiff);
    }

    // Use centralized movement utility
    return BotMovementUtil::MoveToPosition(bot, location);
}

bool QuestStrategy::MoveToQuestGiver(BotAI* ai, uint32 questId)
{
    if (!ai || !ai->GetBot())
        return false;

    // QuestManager has the quest giver finding logic
    // For now, just return false - QuestManager will handle turn-in
    return false;
}

Position QuestStrategy::GetObjectivePosition(BotAI* ai, ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return Position();

    Player* bot = ai->GetBot();
    // Get cached position from ObjectiveTracker (set by StartTrackingObjective)
    Position cachedPos = objective.lastKnownPosition;

    // CRITICAL FIX: Check if cached position is VALID (not at origin 0,0,0 and not at bot's position)
    // If cached position is at origin or at bot's current position, it means FindObjectiveTargetLocation
    // failed to find spawn data and fell back to bot position. In this case, we need to re-query.
    bool isValidSpawnPosition = cachedPos.GetExactDist2d(0.0f, 0.0f) > 0.1f &&  // Not at origin
                                cachedPos.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) > 1.0f;  // Not at bot position

    if (!isValidSpawnPosition)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ GetObjectivePosition: Bot {} - Cached position ({:.1f}, {:.1f}, {:.1f}) is INVALID (at origin or bot position)",
                     bot->GetName(),
                     cachedPos.GetPositionX(), cachedPos.GetPositionY(), cachedPos.GetPositionZ());

        // Re-query position with QuestPOI fallback
        Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
        if (quest && objective.objectiveIndex < quest->Objectives.size())
        {
            QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

            TC_LOG_DEBUG("module.playerbot.quest", "🔄 GetObjectivePosition: Re-querying objective location with QuestPOI fallback...");

            QuestObjectiveData objData(objective.questId, objective.objectiveIndex,
                                      static_cast<QuestObjectiveType>(questObjective.Type),
                                      questObjective.ObjectID, questObjective.Amount);

            // CRITICAL FIX: Check BOTH GameSystems AND ObjectiveTracker for null
            Position newPos;
            {
                auto* gameSystems = ai->GetGameSystems();
                auto* tracker = gameSystems ? gameSystems->GetObjectiveTracker() : nullptr;
                if (tracker)
                    newPos = tracker->FindObjectiveTargetLocation(bot, objData);
            }
            // Check if we got a valid position
            if (newPos.GetExactDist2d(0.0f, 0.0f) > 0.1f)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✅ GetObjectivePosition: Found NEW position ({:.1f}, {:.1f}, {:.1f}) via re-query",
                             newPos.GetPositionX(), newPos.GetPositionY(), newPos.GetPositionZ());
                return newPos;
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.quest", "❌ GetObjectivePosition: Re-query FAILED, returning cached position anyway");
            }
        }
    }

    // Cached position is valid - return it
    TC_LOG_DEBUG("module.playerbot.quest", "✅ GetObjectivePosition: Bot {} using cached spawn position ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(),
                 cachedPos.GetPositionX(), cachedPos.GetPositionY(), cachedPos.GetPositionZ());

    return cachedPos;
}

::Unit* QuestStrategy::FindQuestTarget(BotAI* ai, ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // Determine which creature entry to search for based on objective type
    uint32 targetCreatureEntry = 0;

    if (questObjective.Type == QUEST_OBJECTIVE_MONSTER)
    {
        // MONSTER objectives: ObjectID is the creature entry directly
        targetCreatureEntry = questObjective.ObjectID;
        TC_LOG_DEBUG("module.playerbot.quest", "🎯 FindQuestTarget: MONSTER objective - looking for creature entry {}",
                     targetCreatureEntry);
    }
    else if (questObjective.Type == QUEST_OBJECTIVE_ITEM)
    {
        // ITEM objectives: Need to look up which creature drops this item
        // Query creature_loot_template to find the creature entry
        uint32 itemId = questObjective.ObjectID;

        // PERFORMANCE: Use static cache to avoid repeated database queries
        static std::unordered_map<uint32, uint32> itemToCreatureCache;
        static std::recursive_mutex cacheMutex;

        {
            std::lock_guard lock(cacheMutex);
            auto cacheIt = itemToCreatureCache.find(itemId);
            if (cacheIt != itemToCreatureCache.end())
            {
                targetCreatureEntry = cacheIt->second;
                TC_LOG_DEBUG("module.playerbot.quest", "🎯 FindQuestTarget: ITEM objective (cached) - item {} drops from creature entry {}",
                             itemId, targetCreatureEntry);
            }
        }

        // If not in cache, query database
        if (targetCreatureEntry == 0)
        {
            // Query creature_loot_template to find which creature drops this item
            QueryResult result = WorldDatabase.PQuery("SELECT Entry FROM creature_loot_template WHERE Item = {} LIMIT 1", itemId);

            if (result)
            {
                Field* fields = result->Fetch();
                targetCreatureEntry = fields[0].GetUInt32();

                // Cache the result
                {
                    std::lock_guard lock(cacheMutex);
                    itemToCreatureCache[itemId] = targetCreatureEntry;
                }

                TC_LOG_DEBUG("module.playerbot.quest", "🎯 FindQuestTarget: ITEM objective (DB lookup) - item {} drops from creature entry {}",
                             itemId, targetCreatureEntry);
            }
            else
            {
                TC_LOG_WARN("module.playerbot.quest", "⚠️ FindQuestTarget: ITEM objective - item {} NOT FOUND in creature_loot_template!",
                             itemId);
                return nullptr;
            }
        }
    }
    else
    {
        // Other objective types (GAMEOBJECT, etc.) - not handled here
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestTarget: Unsupported objective type {} for quest {}",
                     static_cast<uint32>(questObjective.Type), objective.questId);
        return nullptr;
    }

    if (targetCreatureEntry == 0)
        return nullptr;

    // DEADLOCK FIX: Use spatial grid instead of ObjectAccessor in loops
    Map* map = bot->GetMap();
    if (!map)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ FindQuestTarget: Bot {} - GetMap() returned nullptr!", bot->GetName());
        return nullptr;
    }

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ FindQuestTarget: Bot {} - No spatial grid for map!", bot->GetName());
        return nullptr;
    }

    // Query nearby creatures (lock-free!)
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 300.0f);

    TC_LOG_DEBUG("module.playerbot.quest", "🔍 FindQuestTarget: Bot {} at ({:.1f}, {:.1f}) - spatial query returned {} creatures, looking for entry {}",
                 bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), nearbyCreatures.size(), targetCreatureEntry);

    // Find first matching creature by entry
    ObjectGuid targetGuid;
    uint32 matchingEntryCount = 0;
    uint32 matchingEntryDeadCount = 0;
    for (auto const& snapshot : nearbyCreatures)
    {
        if (snapshot.entry == targetCreatureEntry)
        {
            matchingEntryCount++;
            if (snapshot.isDead)
            {
                matchingEntryDeadCount++;
                continue;
            }
            targetGuid = snapshot.guid;
            TC_LOG_DEBUG("module.playerbot.quest", "✅ FindQuestTarget: Found creature entry {} at ({:.1f}, {:.1f}, {:.1f})",
                         snapshot.entry, snapshot.position.GetPositionX(),
                         snapshot.position.GetPositionY(), snapshot.position.GetPositionZ());
            break;
        }
    }

    if (targetGuid.IsEmpty())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestTarget: Bot {} - NO ALIVE targets for entry {} (total={}, dead={}, checked {} creatures)",
                     bot->GetName(), targetCreatureEntry, matchingEntryCount, matchingEntryDeadCount, nearbyCreatures.size());

        // FALLBACK: Bot should move closer to spawn locations from FindObjectiveTargetLocation
        // The caller (EngageQuestTargets) will handle navigation to objective.lastKnownPosition
        // which contains the spawn location from StartTrackingObjective
        return nullptr;
    }

    // PHASE 5D: Thread-safe spatial grid validation
    auto snapshot = SpatialGridQueryHelpers::FindCreatureByGuid(bot, targetGuid);
    ::Unit* target = nullptr;

    if (snapshot)
    {
        // Get Unit* for quest NPC interaction (validated via snapshot first)
        // CRITICAL FIX: Actually retrieve the Creature from the snapshot!
        // The snapshot validation confirms the creature exists, now get the real pointer
        target = ObjectAccessor::GetCreature(*bot, targetGuid);
        if (!target)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestTarget: Snapshot found creature {} but ObjectAccessor::GetCreature returned nullptr",
                         targetGuid.GetCounter());
        }
    }

    // CRITICAL FIX: Distinguish between "talk to" NPCs and "attackable neutral" mobs
    // Type 0 (QUEST_OBJECTIVE_MONSTER) can be used for TWO different quest mechanics:
    //
    // 1. "Talk To" NPCs (e.g., Quest 28809 "Injured Soldier" - mob 50047):
    //    - Has npc_spellclick_spells entry
    //    - Not hostile but requires HandleSpellClick() interaction
    //    - Should NOT be attacked
    //
    // 2. "Attackable Neutral" Mobs (e.g., mob 49871 "Blackrock Worg"):
    //    - NO npc_spellclick_spells entry
    //    - Neutral faction but CAN be attacked
    //    - Should be killed for quest credit
    //
    // The key distinction: Check npc_spellclick_spells, NOT hostility!
    if (target && target->ToCreature())
    {
        Creature* creature = target->ToCreature();
        uint32 entry = targetCreatureEntry;

        // If creature is not hostile, check if it requires spell click interaction
        if (!bot->IsHostileTo(creature))
        {
            // Check if this NPC has spell click data
            if (RequiresSpellClickInteraction(entry))
            {
                TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestTarget: NPC {} (Entry: {}) requires SPELL CLICK interaction, not attack! Returning nullptr for TALKTO logic.",
                             creature->GetName(), entry);
                return nullptr;  // Return nullptr so bot uses TALKTO logic in EngageQuestTargets()
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✅ FindQuestTarget: Mob {} (Entry: {}) is NEUTRAL but ATTACKABLE (no spell click data), will be attacked!",
                             creature->GetName(), entry);
                // Fall through - return this target for attack even though it's neutral
            }
        }
    }

    return target;
}

GameObject* QuestStrategy::FindQuestObject(BotAI* ai, ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_GAMEOBJECT)
        return nullptr;

    // DEADLOCK FIX: Use spatial grid instead of ObjectAccessor
    Map* map = bot->GetMap();
    if (!map)
        return nullptr;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
        return nullptr;

    // Query nearby GameObjects (lock-free!)
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), 200.0f);

    TC_LOG_DEBUG("module.playerbot.quest", "🔍 FindQuestObject: Bot {} scanning for GameObject entry {} within 200 yards - found {} nearby objects",
                 bot->GetName(), questObjective.ObjectID, nearbyObjects.size());

    // Find first matching GameObject by entry
    ObjectGuid objectGuid;
    for (auto const& snapshot : nearbyObjects)
    {
        if (snapshot.entry == questObjective.ObjectID && snapshot.isSpawned)
        {
            objectGuid = snapshot.guid;
            TC_LOG_DEBUG("module.playerbot.quest", "✅ FindQuestObject: Found GameObject entry {} at ({:.1f}, {:.1f}, {:.1f})",
                         snapshot.entry, snapshot.position.GetPositionX(),
                         snapshot.position.GetPositionY(), snapshot.position.GetPositionZ());
            break;
        }
    }
    if (objectGuid.IsEmpty())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestObject: Bot {} - NO GameObjects found in 200-yard scan for entry {}",
                     bot->GetName(), questObjective.ObjectID);
        return nullptr;
    }
    // PHASE 5D: Thread-safe spatial grid validation
    auto snapshot = SpatialGridQueryHelpers::FindGameObjectByGuid(bot, objectGuid);
    GameObject* gameObject = nullptr;

    if (snapshot)
    {
        // CRITICAL FIX: Actually retrieve the GameObject from the snapshot!
        // The snapshot validation confirms the object exists, now get the real pointer
        gameObject = ObjectAccessor::GetGameObject(*bot, objectGuid);
        if (!gameObject)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestObject: Snapshot found GameObject {} but ObjectAccessor::GetGameObject returned nullptr",
                         objectGuid.GetCounter());
            return nullptr;
        }
    }
    else
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestObject: Bot {} - Snapshot validation failed for GameObject {}",
                     bot->GetName(), objectGuid.GetCounter());
        return nullptr;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ FindQuestObject: Bot {} found GameObject {} (Entry: {}) at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                 bot->GetName(), gameObject->GetName(), questObjective.ObjectID,
                 gameObject->GetPositionX(), gameObject->GetPositionY(), gameObject->GetPositionZ(),
                 std::sqrt(bot->GetExactDistSq(gameObject)));

    return gameObject;
}

::Item* QuestStrategy::FindQuestItem(BotAI* ai, ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_ITEM)
        return nullptr;

    // Check if bot has item in inventory
    return bot->GetItemByEntry(questObjective.ObjectID);
}

void QuestStrategy::SearchForQuestGivers(BotAI* ai)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ SearchForQuestGivers: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    // CRITICAL: Safety check for worker thread access during bot destruction
    // IsInWorld() returns false during Player destruction, preventing ACCESS_VIOLATION
    // in WorldObject::GetMap() and GetCreatureListWithEntryInGrid() grid operations
    if (!bot->IsInWorld())
        return;

    // CRITICAL FIX: Check for combat FIRST - combat always takes priority!
    if (bot->IsInCombat())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚔️ SearchForQuestGivers: Bot {} IN COMBAT - aborting search, combat takes priority!",
                     bot->GetName());
        return;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "🔍 SearchForQuestGivers: ENTRY for bot {}", bot->GetName());

    // Initialize QuestAcceptanceManager if not already done
    if (!_acceptanceManager)
    {
        _acceptanceManager = std::make_unique<QuestAcceptanceManager>(bot);
        TC_LOG_DEBUG("module.playerbot.quest",
            "🎬 SearchForQuestGivers: Initialized QuestAcceptanceManager for bot {}",
            bot->GetName());
    }

    // THROTTLING: Prevent log spam from repeated failed searches
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Calculate backoff delay based on failure count (exponential backoff)
    // 0 failures: 0ms delay
    // 1 failure: 5 seconds
    // 2 failures: 10 seconds
    // 3 failures: 20 seconds
    // 4+ failures: 30 seconds
    uint32 backoffDelay = 0;
    if (_questGiverSearchFailures > 0)
    {
        backoffDelay = std::min(30000u, 5000u * (1u << (_questGiverSearchFailures - 1)));
    }

    TC_LOG_DEBUG("module.playerbot.quest", "⏰ SearchForQuestGivers: Bot {} - failures={}, backoffDelay={}ms, timeSinceLastSearch={}ms",
                 bot->GetName(), _questGiverSearchFailures, backoffDelay, currentTime - _lastQuestGiverSearchTime);

    // Check if we're still in cooldown period
    if (currentTime - _lastQuestGiverSearchTime < backoffDelay)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⏸️ SearchForQuestGivers: Bot {} still in cooldown ({} ms remaining), skipping search",
                     bot->GetName(), backoffDelay - (currentTime - _lastQuestGiverSearchTime));
        // Still in cooldown - don't search yet (prevents log spam)
        return;
    }

    // Update last search time
    _lastQuestGiverSearchTime = currentTime;

    TC_LOG_INFO("module.playerbot.quest",
        "SearchForQuestGivers: Bot {} (Level {}) scanning via spatial grid",
        bot->GetName(), bot->GetLevel());

    // ========================================================================
    // SPATIAL GRID SCAN — lock-free, thread-safe, always uses current map data
    // ========================================================================
    // The DoubleBufferedSpatialGrid provides atomic snapshots updated by the main
    // thread. Worker threads read the inactive buffer with zero contention.
    // This replaces SafeGridOperations::GetCreatureListSafe which accessed live
    // grid data and returned stale creatures from the old map after teleports.

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(bot->GetMapId());
    if (!spatialGrid)
    {
        TC_LOG_DEBUG("module.playerbot.quest",
            "SearchForQuestGivers: No spatial grid for map {}", bot->GetMapId());
        // Fall through to hub search below
    }

    ObjectGuid bestQuestGiverGuid;
    Position bestQuestGiverPos;
    float closestDistance = 999999.0f;
    uint32 questGiverCount = 0;

    if (spatialGrid)
    {
        auto creatureSnapshots = spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 300.0f);

        TC_LOG_INFO("module.playerbot.quest",
            "SearchForQuestGivers: Bot {} spatial grid returned {} creatures within 300yd",
            bot->GetName(), creatureSnapshots.size());

        for (auto const& snapshot : creatureSnapshots)
        {
            if (!snapshot.IsValid() || snapshot.isDead)
                continue;

            // Filter to quest givers only
            if (!snapshot.hasQuestGiver)
                continue;

            // Skip NPCs that previously returned 0 quests
            if (_failedQuestGiverGuids.count(snapshot.guid))
                continue;

            questGiverCount++;

            // Check if this NPC has any quests for the bot via quest relations
            // (uses static DB data, thread-safe)
            QuestRelationResult questRelations = sObjectMgr->GetCreatureQuestRelations(snapshot.entry);
            bool hasAnyQuest = false;
            for (uint32 questId : questRelations)
            {
                Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
                if (!quest)
                    continue;
                // Skip quests the bot already has or has completed
                if (bot->GetQuestStatus(questId) != QUEST_STATUS_NONE)
                    continue;
                hasAnyQuest = true;
                break;
            }

            if (!hasAnyQuest)
                continue;

            // Verify navmesh reachability — skip NPCs on unreachable terrain (towers, cliffs)
            ::PathGenerator path(bot);
            bool pathResult = path.CalculatePath(
                snapshot.position.GetPositionX(), snapshot.position.GetPositionY(),
                snapshot.position.GetPositionZ(), false);
            if (!pathResult || (path.GetPathType() & ::PATHFIND_NOPATH))
                continue;

            float dist = bot->GetExactDist2d(snapshot.position);
            if (dist < closestDistance)
            {
                closestDistance = dist;
                bestQuestGiverGuid = snapshot.guid;
                bestQuestGiverPos.Relocate(snapshot.position);
            }
        }
    }

    TC_LOG_INFO("module.playerbot.quest",
        "SearchForQuestGivers: Bot {} found {} quest givers, closest at {:.1f}yd (guid={})",
        bot->GetName(), questGiverCount, closestDistance,
        bestQuestGiverGuid.IsEmpty() ? "NONE" : bestQuestGiverGuid.ToString());

    if (bestQuestGiverGuid.IsEmpty())
    {
        // Increment failure counter for exponential backoff
        _questGiverSearchFailures++;

        TC_LOG_DEBUG("module.playerbot.quest",
            "❌ SearchForQuestGivers: Bot {} found no quest givers within 300 yards (failures: {}, next search in {}s)",
            bot->GetName(), _questGiverSearchFailures,
            std::min(30u, 5u * (1u << (_questGiverSearchFailures - 1))));

        // PATHFINDING TO QUEST HUBS: Navigate to appropriate quest hub for bot's level
        TC_LOG_DEBUG("module.playerbot.quest",
            "🗺️ SearchForQuestGivers: Bot {} has no nearby quest givers - searching quest hub database for appropriate quest hubs",
            bot->GetName());

        // Get quest hubs appropriate for this bot's level and faction
        auto& hubDb = QuestHubDatabase::Instance();
        if (!hubDb.IsInitialized())
        {
            TC_LOG_WARN("module.playerbot.quest",
                "SearchForQuestGivers: Bot {} - QuestHubDatabase not initialized — teleporting to homebind",
                bot->GetName());

            bot->TeleportTo(bot->m_homebind);
            _teleportCooldownUntil = GameTime::GetGameTimeMS() + 5000;
            return;
        }

        auto questHubs = hubDb.GetQuestHubsForPlayer(bot, 3); // Get top 3 suitable hubs
        if (questHubs.empty())
        {
            TC_LOG_WARN("module.playerbot.quest",
                "SearchForQuestGivers: Bot {} - No quest hubs found for level {} (zone {}, faction {}) — teleporting to homebind",
                bot->GetName(), bot->GetLevel(), bot->GetZoneId(), bot->GetTeamId());

            bot->TeleportTo(bot->m_homebind);
            _teleportCooldownUntil = GameTime::GetGameTimeMS() + 5000;
            return;
        }
        // Get best quest hub (sorted by suitability — level match, distance, quest count)
        QuestHub const* nearestHub = questHubs[0];

        TC_LOG_INFO("module.playerbot.quest",
            "SearchForQuestGivers: Bot {} found quest hub '{}' (map {}, level {}-{}, {} quests, dist={:.0f}yd)",
            bot->GetName(), nearestHub->name, nearestHub->mapId,
            nearestHub->minLevel, nearestHub->maxLevel,
            nearestHub->questIds.size(), nearestHub->GetDistanceFrom(bot));

        // Already at hub?
        float hubDistance = nearestHub->GetDistanceFrom(bot);
        if (hubDistance < 10.0f)
        {
            TC_LOG_WARN("module.playerbot.quest",
                "SearchForQuestGivers: Bot {} at hub '{}' but no quest givers found — phasing issue",
                bot->GetName(), nearestHub->name);
            return;
        }

        // Cross-map travel: use hearthstone if bind point is on the hub's map
        if (nearestHub->mapId != bot->GetMapId())
        {
            WorldLocation const& homebind = bot->m_homebind;

            if (homebind.GetMapId() == nearestHub->mapId)
            {
                // Homebind is on the target map — teleport there directly
                bot->TeleportTo(homebind);
                _teleportCooldownUntil = GameTime::GetGameTimeMS() + 5000;
                TC_LOG_INFO("module.playerbot.quest",
                    "SearchForQuestGivers: Bot {} teleporting to homebind on map {} for hub '{}'",
                    bot->GetName(), nearestHub->mapId, nearestHub->name);
            }
            else
            {
                // Need multi-leg travel (ship/zeppelin/portal)
                TC_LOG_INFO("module.playerbot.quest",
                    "SearchForQuestGivers: Bot {} needs cross-map travel to hub '{}' (map {} -> {})",
                    bot->GetName(), nearestHub->name, bot->GetMapId(), nearestHub->mapId);

                if (!_travelManager)
                    _travelManager = std::make_unique<TravelRouteManager>(bot);

                TravelRoute route = _travelManager->PlanRoute(nearestHub->mapId, nearestHub->location);
                if (!route.legs.empty())
                {
                    TC_LOG_INFO("module.playerbot.quest",
                        "SearchForQuestGivers: Bot {} started travel to hub '{}'",
                        bot->GetName(), nearestHub->name);
                }
            }
            return;
        }

        // Same map — walk directly
        TC_LOG_INFO("module.playerbot.quest",
            "SearchForQuestGivers: Bot {} navigating to hub '{}' ({:.0f}yd away)",
            bot->GetName(), nearestHub->name, hubDistance);

        BotMovementUtil::MoveToPosition(bot, nearestHub->location);
        return;
    }

    // SUCCESS: Found a quest giver via spatial grid snapshot
    _questGiverSearchFailures = 0;

    // Set pending quest giver — the existing pending system handles walking + arrival
    _pendingQuestGiverGuid = bestQuestGiverGuid;
    _pendingQuestGiverPos = bestQuestGiverPos;

    if (closestDistance > INTERACTION_DISTANCE)
    {
        TC_LOG_INFO("module.playerbot.quest",
            "SearchForQuestGivers: Bot {} moving to quest giver at ({:.1f},{:.1f},{:.1f}), {:.1f}yd away",
            bot->GetName(),
            bestQuestGiverPos.GetPositionX(), bestQuestGiverPos.GetPositionY(), bestQuestGiverPos.GetPositionZ(),
            closestDistance);

        BotMovementUtil::MoveToPosition(bot, bestQuestGiverPos);
    }
    else
    {
        // Already within interaction range — resolve creature and accept quests now
        Creature* npc = ObjectAccessor::GetCreature(*bot, bestQuestGiverGuid);
        if (!npc)
        {
            // Try grid scan fallback
            uint32 entry = bestQuestGiverGuid.GetEntry();
            if (entry)
            {
                std::list<Creature*> nearby;
                if (SafeGridOperations::GetCreatureListSafe(bot, nearby, entry, 50.0f))
                    for (Creature* c : nearby)
                        if (c && c->IsAlive() && c->IsQuestGiver()) { npc = c; break; }
            }
        }

        if (npc)
        {
            TC_LOG_INFO("module.playerbot.quest",
                "SearchForQuestGivers: Bot {} at quest giver {} ({:.1f}yd) — accepting quests",
                bot->GetName(), npc->GetName(), closestDistance);

            if (!_acceptanceManager)
                _acceptanceManager = std::make_unique<QuestAcceptanceManager>(bot);

            uint32 acceptedBefore = _acceptanceManager->GetQuestsAccepted();
            _acceptanceManager->ProcessQuestGiver(npc);
            uint32 acceptedNow = _acceptanceManager->GetQuestsAccepted();

            TC_LOG_INFO("module.playerbot.quest",
                "SearchForQuestGivers: Bot {} quest acceptance done (accepted: {}, dropped: {})",
                bot->GetName(),
                _acceptanceManager->GetQuestsAccepted(),
                _acceptanceManager->GetQuestsDropped());

            // If NPC had no quests for us, blacklist it temporarily so we try other NPCs
            if (acceptedNow == acceptedBefore)
            {
                _failedQuestGiverGuids.insert(npc->GetGUID());
                TC_LOG_DEBUG("module.playerbot.quest",
                    "SearchForQuestGivers: Bot {} blacklisted NPC {} (no quests accepted)",
                    bot->GetName(), npc->GetName());
            }
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest",
                "SearchForQuestGivers: Bot {} at quest giver position but creature not found",
                bot->GetName());
        }
        _pendingQuestGiverGuid.Clear();
    }
}

// ========================================================================
// QUEST TURN-IN SYSTEM - Multi-Tier Fallback Implementation
// ========================================================================

bool QuestStrategy::FindQuestEnderLocation(BotAI* ai, uint32 questId, QuestEnderLocation& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // DIAGNOSTIC: Log bot's actual position and map to debug map mismatch issues
    TC_LOG_DEBUG("module.playerbot.quest", "🔍 FindQuestEnderLocation: Bot {} (Level {}) on MAP {} at ({:.1f}, {:.1f}, {:.1f}) searching for quest ender for quest {}",
                 bot->GetName(), bot->GetLevel(), bot->GetMapId(),
                 bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                 questId);

    // ========================================================================
    // PHASE 1: Determine quest ender type (Creature OR GameObject)
    // ========================================================================

    // First, check creature_questender table
    auto creatureQuestEnders = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questId);
    bool hasCreatureEnder = (creatureQuestEnders.begin() != creatureQuestEnders.end());

    // Second, check gameobject_questender table
    auto gameobjectQuestEnders = sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(questId);
    bool hasGameObjectEnder = (gameobjectQuestEnders.begin() != gameobjectQuestEnders.end());

    if (!hasCreatureEnder && !hasGameObjectEnder)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ FindQuestEnderLocation: No quest ender found in creature_questender OR gameobject_questender for quest {}",
                     questId);
        return false;
    }

    // Log what we found
    if (hasCreatureEnder)
    {
        uint32 creatureEntry = creatureQuestEnders.begin()->second;
        TC_LOG_DEBUG("module.playerbot.quest", "📋 FindQuestEnderLocation: Quest {} has CREATURE quest ender (entry {})",
                     questId, creatureEntry);
    }
    if (hasGameObjectEnder)
    {
        uint32 goEntry = gameobjectQuestEnders.begin()->second;
        TC_LOG_DEBUG("module.playerbot.quest", "📋 FindQuestEnderLocation: Quest {} has GAMEOBJECT quest ender (entry {})",
                     questId, goEntry);
    }

    // ========================================================================
    // PHASE 2A: Try Creature Quest Ender (if available)
    // ========================================================================
    if (hasCreatureEnder)
    {
        uint32 questEnderEntry = creatureQuestEnders.begin()->second;
        location.objectEntry = questEnderEntry;
        location.isGameObject = false;

        TC_LOG_DEBUG("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 1A - Searching CREATURE spawn data for entry {}",
                     questEnderEntry);

        // Get all spawn data for this creature
        auto const& creatureSpawnData = sObjectMgr->GetAllCreatureData();

        float closestDistance = 999999.0f;
        CreatureData const* closestSpawn = nullptr;

        // DIAGNOSTIC: Log bot's current map and total spawn data size
        uint32 botMapId = bot->GetMapId();
        uint32 matchingEntryCount = 0;
        uint32 matchingMapCount = 0;

        // Track closest cross-map spawn for diagnostic and future map travel
        CreatureData const* closestCrossMapSpawn = nullptr;
        float closestCrossMapDistance = 999999.0f;
        uint32 crossMapId = 0;

        TC_LOG_DEBUG("module.playerbot.quest", "🔬 TIER 1A DIAGNOSTIC: Bot {} on map {} searching for creature entry {} (total spawns in DB: {})",
                     bot->GetName(), botMapId, questEnderEntry, creatureSpawnData.size());

        for (auto const& pair : creatureSpawnData)
        {
            CreatureData const& data = pair.second;

            if (data.id != questEnderEntry)
                continue;

            // Found matching entry - log it
            matchingEntryCount++;

            if (data.mapId != botMapId)
            {
                // Track cross-map spawns for later reference
                // Use spawn position distance as heuristic (not accurate cross-map, but consistent)
                float pseudoDistance = data.spawnPoint.GetPositionX() + data.spawnPoint.GetPositionY();
                if (!closestCrossMapSpawn || pseudoDistance < closestCrossMapDistance)
                {
                    closestCrossMapSpawn = &data;
                    closestCrossMapDistance = pseudoDistance;
                    crossMapId = data.mapId;
                }

                // DIAGNOSTIC: Log spawn found but wrong map
                if (matchingEntryCount <= 5) // Limit logging
                {
                    TC_LOG_DEBUG("module.playerbot.quest", "🔬 TIER 1A: Found creature {} spawn on MAP {} (bot on map {}) at ({:.1f}, {:.1f}, {:.1f}) - DIFFERENT MAP",
                                 questEnderEntry, data.mapId, botMapId,
                                 data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ());
                }
                continue;
            }

            // Found matching entry AND map
            matchingMapCount++;
            float distance = bot->GetExactDist2d(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());

            TC_LOG_DEBUG("module.playerbot.quest", "🔬 TIER 1A: Found creature {} spawn on SAME MAP {} at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                         questEnderEntry, data.mapId,
                         data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(),
                         distance);

            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestSpawn = &data;
            }
        }

        TC_LOG_DEBUG("module.playerbot.quest", "🔬 TIER 1A SUMMARY: Found {} spawns with entry {}, {} on same map as bot (map {})",
                     matchingEntryCount, questEnderEntry, matchingMapCount, botMapId);

        if (closestSpawn)
        {
            location.position.Relocate(
                closestSpawn->spawnPoint.GetPositionX(),
                closestSpawn->spawnPoint.GetPositionY(),
                closestSpawn->spawnPoint.GetPositionZ()
            );
            location.targetMapId = botMapId;
            location.foundViaSpawn = true;

            TC_LOG_DEBUG("module.playerbot.quest", "✅ TIER 1A SUCCESS: Found CREATURE quest ender {} via spawn data at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                         questEnderEntry,
                         location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                         closestDistance);
            return true;
        }

        // No same-map spawn found - check if cross-map spawns exist
        if (closestCrossMapSpawn && matchingEntryCount > 0 && matchingMapCount == 0)
        {
            // Quest ender exists but ONLY on different map(s) - store info for potential map travel
            location.position.Relocate(
                closestCrossMapSpawn->spawnPoint.GetPositionX(),
                closestCrossMapSpawn->spawnPoint.GetPositionY(),
                closestCrossMapSpawn->spawnPoint.GetPositionZ()
            );
            location.targetMapId = crossMapId;
            location.isOnDifferentMap = true;
            location.foundViaSpawn = true;

            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ TIER 1A: CREATURE quest ender {} exists on MAP {} (bot on map {}) - REQUIRES MAP TRAVEL to ({:.1f}, {:.1f}, {:.1f})",
                         questEnderEntry, crossMapId, botMapId,
                         location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());
            // Don't return true yet - let caller decide how to handle cross-map
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ TIER 1A FAILED: No spawn data found for CREATURE {} on map {} (found {} total spawns, 0 on same map)",
                         questEnderEntry, botMapId, matchingEntryCount);
        }
    }

    // ========================================================================
    // PHASE 2B: Try GameObject Quest Ender (if available or creature failed)
    // ========================================================================
    if (hasGameObjectEnder)
    {
        uint32 questEnderEntry = gameobjectQuestEnders.begin()->second;
        location.objectEntry = questEnderEntry;
        location.isGameObject = true;

        TC_LOG_DEBUG("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 1B - Searching GAMEOBJECT spawn data for entry {}",
                     questEnderEntry);

        // Get all spawn data for gameobjects
        auto const& goSpawnData = sObjectMgr->GetAllGameObjectData();
        uint32 botMapId = bot->GetMapId();

        float closestDistance = 999999.0f;
        GameObjectData const* closestSpawn = nullptr;

        // Track cross-map spawns
        GameObjectData const* closestCrossMapSpawn = nullptr;
        uint32 crossMapId = 0;
        uint32 matchingEntryCount = 0;
        uint32 matchingMapCount = 0;

        for (auto const& pair : goSpawnData)
        {
            GameObjectData const& data = pair.second;

            if (data.id != questEnderEntry)
                continue;

            matchingEntryCount++;

            if (data.mapId != botMapId)
            {
                // Track cross-map spawns
                if (!closestCrossMapSpawn)
                {
                    closestCrossMapSpawn = &data;
                    crossMapId = data.mapId;
                }
                continue;
            }

            matchingMapCount++;
            float distance = bot->GetExactDist2d(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());

            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestSpawn = &data;
            }
        }

        if (closestSpawn)
        {
            location.position.Relocate(
                closestSpawn->spawnPoint.GetPositionX(),
                closestSpawn->spawnPoint.GetPositionY(),
                closestSpawn->spawnPoint.GetPositionZ()
            );
            location.targetMapId = botMapId;
            location.foundViaSpawn = true;

            TC_LOG_DEBUG("module.playerbot.quest", "✅ TIER 1B SUCCESS: Found GAMEOBJECT quest ender {} via spawn data at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                         questEnderEntry,
                         location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                         closestDistance);
            return true;
        }

        // No same-map spawn found - check if cross-map spawns exist
        if (closestCrossMapSpawn && matchingEntryCount > 0 && matchingMapCount == 0)
        {
            location.position.Relocate(
                closestCrossMapSpawn->spawnPoint.GetPositionX(),
                closestCrossMapSpawn->spawnPoint.GetPositionY(),
                closestCrossMapSpawn->spawnPoint.GetPositionZ()
            );
            location.targetMapId = crossMapId;
            location.isOnDifferentMap = true;
            location.foundViaSpawn = true;

            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ TIER 1B: GAMEOBJECT quest ender {} exists on MAP {} (bot on map {}) - REQUIRES MAP TRAVEL",
                         questEnderEntry, crossMapId, botMapId);
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ TIER 1B FAILED: No spawn data found for GAMEOBJECT {} (found {} spawns, {} on same map)",
                         questEnderEntry, matchingEntryCount, matchingMapCount);
        }
    }

    // ========================================================================
    // TIER 2: Quest POI Data (FALLBACK - Scripted/Event objects)
    // ========================================================================
    TC_LOG_DEBUG("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 2 - Searching Quest POI data for quest {}",
                 questId);

    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(questId);

    if (!poiData || poiData->Blobs.empty())
    {
        // Check if we found a cross-map location earlier
        if (location.isOnDifferentMap && location.HasValidPosition())
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ FindQuestEnderLocation: Quest ender for quest {} is on MAP {} (bot on map {}) - returning cross-map location",
                         questId, location.targetMapId, bot->GetMapId());
            // Return true with cross-map info - caller must handle map travel
            return true;
        }

        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ TIER 2 FAILED: No Quest POI data found for quest {}",
                     questId);

        // Only set requiresSearch if we have a valid entry to search for on THIS map
        if (location.objectEntry != 0 && !location.isOnDifferentMap)
        {
            location.requiresSearch = true;
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ FindQuestEnderLocation: All automated methods failed - bot will need to search 50-yard radius for {} {}",
                         location.isGameObject ? "GAMEOBJECT" : "CREATURE", location.objectEntry);
            return true;
        }

        // No valid location found at all
        TC_LOG_DEBUG("module.playerbot.quest", "❌ FindQuestEnderLocation: FAILED - No quest ender location found for quest {} (entry={}, isOnDifferentMap={})",
                     questId, location.objectEntry, location.isOnDifferentMap);
        return false;
    }

    // Find the blob on the same map as bot (prefer same-map, but track other-map as fallback)
    QuestPOIBlobData const* validBlob = nullptr;
    QuestPOIBlobData const* otherMapBlob = nullptr;

    for (auto const& blob : poiData->Blobs)
    {
        if (blob.MapID == static_cast<int32>(bot->GetMapId()))
        {
            validBlob = &blob;
            break;  // Found same-map blob, use it
        }
        else if (!otherMapBlob && !blob.Points.empty())
        {
            // Track first valid other-map blob as fallback
            otherMapBlob = &blob;
        }
    }

    if (!validBlob || validBlob->Points.empty())
    {
        // No same-map blob found - check for other-map blob
        if (otherMapBlob && !otherMapBlob->Points.empty())
        {
            // Quest ender is on a DIFFERENT map - use POI from other map
            QuestPOIBlobPoint const& point = otherMapBlob->Points[0];
            location.position.Relocate(
                static_cast<float>(point.X),
                static_cast<float>(point.Y),
                static_cast<float>(point.Z)
            );
            location.targetMapId = static_cast<uint32>(otherMapBlob->MapID);
            location.isOnDifferentMap = true;
            location.foundViaPOI = true;

            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ TIER 2 CROSS-MAP: Quest ender for quest {} is on MAP {} at ({:.1f}, {:.1f}, {:.1f}) - bot on map {}",
                         questId, location.targetMapId,
                         location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                         bot->GetMapId());
            return true;
        }

        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ TIER 2 FAILED: Quest POI data exists but no valid points on map {} or any other map, falling back to TIER 3",
                     bot->GetMapId());

        location.requiresSearch = true;
        return true;
    }

    // Use the first POI point as destination
    QuestPOIBlobPoint const& point = validBlob->Points[0];

    location.position.Relocate(
        static_cast<float>(point.X),
        static_cast<float>(point.Y),
        static_cast<float>(point.Z)
    );
    location.foundViaPOI = true;

    TC_LOG_DEBUG("module.playerbot.quest", "✅ TIER 2 SUCCESS: Found quest POI coordinates at ({:.1f}, {:.1f}, {:.1f}) for quest {} (%s ender)",
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 questId, location.isGameObject ? "GAMEOBJECT" : "CREATURE");

    return true;
}

bool QuestStrategy::NavigateToQuestEnder(BotAI* ai, QuestEnderLocation const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // SAFETY CHECK: Prevent navigation to cross-map destinations
    // This should never happen as TurnInQuest handles cross-map travel, but guard against edge cases
    if (location.isOnDifferentMap)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ NavigateToQuestEnder: SAFETY CHECK - Bot {} attempted navigation to cross-map destination (MAP {} vs current MAP {}) - this should be handled by TurnInQuest cross-map travel system",
                     bot->GetName(), location.targetMapId, bot->GetMapId());
        return false;
    }

    // SAFETY CHECK: Prevent navigation to invalid (0,0,0) positions
    // But if requiresSearch is true, perform a local search instead
    if (!location.HasValidPosition())
    {
        if (location.requiresSearch && location.objectEntry != 0)
        {
            // No valid position but we have an entry to search for - perform local search
            TC_LOG_DEBUG("module.playerbot.quest", "🔍 NavigateToQuestEnder: Bot {} performing LOCAL SEARCH (150yd radius) for {} {} - no spawn/POI data available",
                         bot->GetName(),
                         location.IsGameObject() ? "GameObject" : "NPC",
                         location.objectEntry);

            // Directly check for quest ender in range (within 50 yards)
            return CheckForQuestEnderInRange(ai, location);
        }

        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ NavigateToQuestEnder: SAFETY CHECK - Bot {} attempted navigation to invalid position (0,0,0) for {} {} - aborting",
                     bot->GetName(),
                     location.IsGameObject() ? "GameObject" : "NPC",
                     location.objectEntry);
        return false;
    }

    // Calculate distance to destination
    float distance = bot->GetExactDist2d(location.position.GetPositionX(), location.position.GetPositionY());

    TC_LOG_DEBUG("module.playerbot.quest", "🚶 NavigateToQuestEnder: Bot {} navigating to {} {} at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                 bot->GetName(),
                 location.IsGameObject() ? "GameObject" : "NPC",
                 location.objectEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 distance);

    // Check if already at destination
    if (distance < 10.0f)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "✅ NavigateToQuestEnder: Bot {} already at destination (distance={:.1f} < 10.0), checking for quest ender in range",
                     bot->GetName(), distance);

        // Check for quest ender (NPC or GameObject) in range
        return CheckForQuestEnderInRange(ai, location);
    }

    // Start navigation
    bool moveResult = BotMovementUtil::MoveToPosition(bot, location.position);
    if (!moveResult)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ NavigateToQuestEnder: Bot {} failed to start pathfinding to ({:.1f}, {:.1f}, {:.1f})",
                     bot->GetName(),
                     location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ NavigateToQuestEnder: Bot {} pathfinding started to {} {} (distance={:.1f})",
                 bot->GetName(),
                 location.IsGameObject() ? "GameObject" : "NPC",
                 location.objectEntry,
                 distance);

    return true;
}

bool QuestStrategy::CheckForCreatureQuestEnderInRange(BotAI* ai, uint32 creatureEntry)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    // Without this check, GetCreatureListWithEntryInGrid() calls GetMap() which returns nullptr
    // causing ACCESS_VIOLATION crash at 0x0 (null pointer dereference)
    if (!bot->IsInWorld())
    {
        TC_LOG_TRACE("module.playerbot.quest", "CheckForCreatureQuestEnderInRange: Bot not in world, skipping");
        return false;
    }

    // CRITICAL FIX: Validate Map pointer before grid operations
    // When called from worker thread, the bot may be in process of being removed from world
    // causing GetMap() to return nullptr or an invalid pointer. Cell::VisitGridObjects dereferences
    // GetMap() without null check, causing ACCESS_VIOLATION at address like 0x0000000200000000
    Map* map = bot->FindMap();
    if (!map)
    {
        TC_LOG_TRACE("module.playerbot.quest", "CheckForCreatureQuestEnderInRange: Bot has no valid map, skipping");
        return false;
    }

    // Validate bot position is reasonable (not NaN or extreme values)
    float posX = bot->GetPositionX();
    float posY = bot->GetPositionY();
    if (std::isnan(posX) || std::isnan(posY) || std::isinf(posX) || std::isinf(posY))
    {
        TC_LOG_DEBUG("module.playerbot.quest", "CheckForCreatureQuestEnderInRange: Bot {} has invalid position ({}, {})",
                     bot->GetName(), posX, posY);
        return false;
    }

    TC_LOG_TRACE("module.playerbot.quest", "CheckForCreatureQuestEnderInRange: Bot {} scanning for NPC entry {}",
                 bot->GetName(), creatureEntry);

    // Scan for quest ender NPC in 150-yard radius (extended for requiresSearch mode)
    // Use SafeGridOperations with SEH protection - grid ops from worker threads can cause ACCESS_VIOLATION
    std::list<Creature*> nearbyCreatures;
    if (!SafeGridOperations::GetCreatureListSafe(bot, nearbyCreatures, creatureEntry, 150.0f))
    {
        TC_LOG_TRACE("module.playerbot.quest", "CheckForCreatureQuestEnderInRange: Grid search failed for bot {}",
                     bot->GetName());
        return false;
    }

    TC_LOG_TRACE("module.playerbot.quest", "CheckForCreatureQuestEnderInRange: Bot {} found {} creatures with entry {}",
                 bot->GetName(), nearbyCreatures.size(), creatureEntry);

    if (nearbyCreatures.empty())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CheckForCreatureQuestEnderInRange: Bot {} found NO quest ender NPC {} in range",
                     bot->GetName(), creatureEntry);
        return false;
    }

    // Find the closest valid quest ender
    Creature* closestQuestEnder = nullptr;
    float closestDistance = 999999.0f;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive() || !creature->IsInWorld())
            continue;

        // CRITICAL: Double-check bot is still in world
        if (!bot->IsInWorld())
            return false;

        // CRITICAL: Re-verify creature validity (TOCTOU race)
        // NOTE: Use FindMap() instead of GetMap() - GetMap() has ASSERT(m_currMap) which crashes
        if (!creature->IsInWorld() || !creature->FindMap())
            continue;

        // NOTE: CanSeeOrDetect() is NOT SAFE to call from worker thread!
        // It accesses Map data which can cause ASSERTION FAILED: !IsInWorld() in ResetMap
        // Phase visibility will be validated when bot actually interacts with the NPC.
        // For now, we rely on same-map check which covers most cases.
        if (creature->GetMapId() != bot->GetMapId())
            continue;

        // Verify it's a quest giver (quest enders are also quest givers)
        if (!creature->IsQuestGiver())
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ CheckForCreatureQuestEnderInRange: NPC {} (Entry: {}) is NOT a quest giver, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        float distance = std::sqrt(bot->GetExactDistSq(creature)); // Calculate once from squared distance
        TC_LOG_DEBUG("module.playerbot.quest", "✅ CheckForCreatureQuestEnderInRange: Found valid quest ender {} (Entry: {}) at distance {:.1f}",
                     creature->GetName(), creature->GetEntry(), distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestQuestEnder = creature;
        }
    }

    if (!closestQuestEnder)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CheckForCreatureQuestEnderInRange: Bot {} found creatures with entry {} but none are valid quest enders (phase mismatch or not alive)",
                     bot->GetName(), creatureEntry);
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ CheckForCreatureQuestEnderInRange: Bot {} found quest ender {} at distance {:.1f}",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Check if in interaction range
    if (closestDistance > INTERACTION_DISTANCE)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🚶 CheckForCreatureQuestEnderInRange: Bot {} quest ender {} too far ({:.1f} > INTERACTION_DISTANCE), moving closer",
                     bot->GetName(), closestQuestEnder->GetName(), closestDistance);

        // Move to NPC
        Position npcPos;
        npcPos.Relocate(closestQuestEnder->GetPositionX(), closestQuestEnder->GetPositionY(), closestQuestEnder->GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, npcPos);
        return false; // Not in range yet, but moving
    }

    // NPC is in interaction range - get all completed quests and turn them in
    TC_LOG_DEBUG("module.playerbot.quest", "🎯 CheckForCreatureQuestEnderInRange: Bot {} at quest ender {} (distance {:.1f} <= INTERACTION_DISTANCE), processing quest turn-ins",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Scan ALL active quests and turn in any that are complete OR talk-to quests with this NPC
    bool anyQuestTurnedIn = false;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        QuestStatus status = bot->GetQuestStatus(questId);

        // Check if quest is complete OR is a "talk-to" quest (incomplete with no objectives)
        // Talk-to quests need NPC interaction to complete - they don't have kill/collect objectives
        bool isComplete = (status == QUEST_STATUS_COMPLETE);
        bool isTalkToQuest = (status == QUEST_STATUS_INCOMPLETE && quest->Objectives.empty());

        // Check if quest is a DELIVERY quest (incomplete, has SourceItemId, bot has the item)
        // Delivery quests remain INCOMPLETE until turned in - the item delivery IS the objective
        bool isDeliveryQuest = false;
        if (status == QUEST_STATUS_INCOMPLETE && quest->GetSrcItemId() != 0)
        {
            uint32 srcItemId = quest->GetSrcItemId();
            uint32 itemCount = bot->GetItemCount(srcItemId);
            if (itemCount > 0)
            {
                isDeliveryQuest = true;
                TC_LOG_DEBUG("module.playerbot.quest", "📬 CheckForCreatureQuestEnderInRange: Bot {} has DELIVERY quest {} ({}) with item {} (count: {})",
                             bot->GetName(), questId, quest->GetLogTitle(), srcItemId, itemCount);
            }
        }

        if (!isComplete && !isTalkToQuest && !isDeliveryQuest)
            continue;

        // Check if this NPC is a valid quest ender for this quest
        auto questEnders = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questId);
        bool isValidEnder = false;

        for (auto itr = questEnders.begin(); itr != questEnders.end(); ++itr)
        {
            if (itr->second == closestQuestEnder->GetEntry())
            {
                isValidEnder = true;
                break;
            }
        }

        if (!isValidEnder)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ CheckForCreatureQuestEnderInRange: NPC {} is NOT a valid ender for quest {} ({}), skipping",
                         closestQuestEnder->GetName(), questId, quest->GetLogTitle());
            continue;
        }

        // Turn in the quest
        if (isTalkToQuest)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🗣️ CheckForCreatureQuestEnderInRange: Bot {} turning in TALK-TO quest {} ({}) to NPC {}",
                         bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());
        }
        else if (isDeliveryQuest)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "📬 CheckForCreatureQuestEnderInRange: Bot {} turning in DELIVERY quest {} ({}) to NPC {}",
                         bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🎯 CheckForCreatureQuestEnderInRange: Bot {} turning in COMPLETE quest {} ({}) to NPC {}",
                         bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());
        }

        if (CompleteQuestTurnIn(ai, questId, closestQuestEnder))
        {
            anyQuestTurnedIn = true;
            TC_LOG_DEBUG("module.playerbot.quest", "✅ CheckForCreatureQuestEnderInRange: Bot {} successfully turned in quest {} ({})",
                         bot->GetName(), questId, quest->GetLogTitle());
        }
    }

    return anyQuestTurnedIn;
}

// ========================================================================
// QUEST ENDER IN RANGE ROUTER - Dispatches to creature or gameobject handler
// ========================================================================

bool QuestStrategy::CheckForQuestEnderInRange(BotAI* ai, QuestEnderLocation const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    if (!location.IsValid())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CheckForQuestEnderInRange: Invalid location (no objectEntry)");
        return false;
    }

    // Route to appropriate handler based on quest ender type
    if (location.IsGameObject())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🎯 CheckForQuestEnderInRange: Routing to GameObject handler for entry {}",
                     location.objectEntry);
        return CheckForGameObjectQuestEnderInRange(ai, location.objectEntry);
    }
    else
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🎯 CheckForQuestEnderInRange: Routing to Creature handler for entry {}",
                     location.objectEntry);
        return CheckForCreatureQuestEnderInRange(ai, location.objectEntry);
    }
}

// ========================================================================
// GAMEOBJECT QUEST ENDER - Scan for and interact with gameobject quest enders
// ========================================================================

bool QuestStrategy::CheckForGameObjectQuestEnderInRange(BotAI* ai, uint32 gameobjectEntry)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // CRITICAL: Must be in world before any grid/map operations
    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ CheckForGameObjectQuestEnderInRange: Bot {} is not in world, skipping grid search",
                     bot->GetName());
        return false;
    }

    TC_LOG_TRACE("module.playerbot.quest", "CheckForGameObjectQuestEnderInRange: Bot {} scanning 150-yard radius for GameObject entry {}",
                 bot->GetName(), gameobjectEntry);

    // CRITICAL FIX: Validate Map pointer before grid operations to prevent crash
    // Grid operations call Cell::VisitGridObjects which dereferences GetMap() without null check
    Map* map = bot->FindMap();
    if (!map)
    {
        TC_LOG_TRACE("module.playerbot.quest", "CheckForGameObjectQuestEnderInRange: Bot {} has no valid map, skipping", bot->GetName());
        return false;
    }

    // THREAD-SAFE: Use SafeGridOperations with SEH protection to catch access violations
    // Grid operations from worker threads can cause ACCESS_VIOLATION when map is modified
    std::list<GameObject*> nearbyGameObjects;
    if (!SafeGridOperations::GetGameObjectListSafe(bot, nearbyGameObjects, gameobjectEntry, 150.0f))
    {
        TC_LOG_TRACE("module.playerbot.quest", "CheckForGameObjectQuestEnderInRange: Grid search failed for bot {}", bot->GetName());
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "📊 CheckForGameObjectQuestEnderInRange: Bot {} found {} gameobjects with entry {} in 150-yard radius",
                 bot->GetName(), nearbyGameObjects.size(), gameobjectEntry);

    if (nearbyGameObjects.empty())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CheckForGameObjectQuestEnderInRange: Bot {} found NO quest ender GameObject {} in range",
                     bot->GetName(), gameobjectEntry);
        return false;
    }

    // Find the closest valid quest ender GameObject
    GameObject* closestQuestEnder = nullptr;
    float closestDistance = 999999.0f;

    for (GameObject* gameObject : nearbyGameObjects)
    {
        if (!gameObject || !gameObject->IsInWorld())
            continue;

        // CRITICAL: Double-check bot is still in world
        if (!bot->IsInWorld())
            return false;

        // CRITICAL: Re-verify gameobject validity (TOCTOU race)
        // NOTE: Use FindMap() instead of GetMap() - GetMap() has ASSERT(m_currMap) which crashes
        if (!gameObject->IsInWorld() || !gameObject->FindMap())
            continue;

        // Check same map
        if (gameObject->GetMapId() != bot->GetMapId())
            continue;

        // Check if GameObject is interactable (not despawned/used)
        if (gameObject->GetGoState() != GO_STATE_READY)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ CheckForGameObjectQuestEnderInRange: GameObject {} (Entry: {}) not in READY state, skipping",
                         gameObject->GetName(), gameObject->GetEntry());
            continue;
        }

        float distance = std::sqrt(bot->GetExactDistSq(gameObject));
        TC_LOG_DEBUG("module.playerbot.quest", "✅ CheckForGameObjectQuestEnderInRange: Found valid quest ender {} (Entry: {}) at distance {:.1f}",
                     gameObject->GetName(), gameObject->GetEntry(), distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestQuestEnder = gameObject;
        }
    }

    if (!closestQuestEnder)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "❌ CheckForGameObjectQuestEnderInRange: Bot {} found gameobjects with entry {} but none are valid quest enders (phase mismatch or not ready)",
                     bot->GetName(), gameobjectEntry);
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "✅ CheckForGameObjectQuestEnderInRange: Bot {} found quest ender {} at distance {:.1f}",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Check if in interaction range
    if (closestDistance > INTERACTION_DISTANCE)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "🚶 CheckForGameObjectQuestEnderInRange: Bot {} quest ender {} too far ({:.1f} > INTERACTION_DISTANCE), moving closer",
                     bot->GetName(), closestQuestEnder->GetName(), closestDistance);

        // Move to GameObject
        Position goPos;
        goPos.Relocate(closestQuestEnder->GetPositionX(), closestQuestEnder->GetPositionY(), closestQuestEnder->GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, goPos);
        return false; // Not in range yet, but moving
    }

    // GameObject is in interaction range - process quest turn-ins
    TC_LOG_DEBUG("module.playerbot.quest", "🎯 CheckForGameObjectQuestEnderInRange: Bot {} at quest ender {} (distance {:.1f} <= INTERACTION_DISTANCE), processing quest turn-ins",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Scan ALL active quests and turn in any that are complete with this GameObject as quest ender
    bool anyQuestTurnedIn = false;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        QuestStatus status = bot->GetQuestStatus(questId);

        // Check if quest is complete OR is a "talk-to" quest (incomplete with no objectives)
        bool isComplete = (status == QUEST_STATUS_COMPLETE);
        bool isTalkToQuest = (status == QUEST_STATUS_INCOMPLETE && quest->Objectives.empty());

        // Check if quest is a DELIVERY quest (incomplete, has SourceItemId, bot has the item)
        // Delivery quests remain INCOMPLETE until turned in - the item delivery IS the objective
        bool isDeliveryQuest = false;
        if (status == QUEST_STATUS_INCOMPLETE && quest->GetSrcItemId() != 0)
        {
            uint32 srcItemId = quest->GetSrcItemId();
            uint32 itemCount = bot->GetItemCount(srcItemId);
            if (itemCount > 0)
            {
                isDeliveryQuest = true;
                TC_LOG_DEBUG("module.playerbot.quest", "📬 CheckForGameObjectQuestEnderInRange: Bot {} has DELIVERY quest {} ({}) with item {} (count: {})",
                             bot->GetName(), questId, quest->GetLogTitle(), srcItemId, itemCount);
            }
        }

        if (!isComplete && !isTalkToQuest && !isDeliveryQuest)
            continue;

        // Check if this GameObject is a valid quest ender for this quest
        auto questEnders = sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(questId);
        bool isValidEnder = false;

        for (auto itr = questEnders.begin(); itr != questEnders.end(); ++itr)
        {
            if (itr->second == closestQuestEnder->GetEntry())
            {
                isValidEnder = true;
                break;
            }
        }

        if (!isValidEnder)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ CheckForGameObjectQuestEnderInRange: GameObject {} is NOT a valid ender for quest {} ({}), skipping",
                         closestQuestEnder->GetName(), questId, quest->GetLogTitle());
            continue;
        }

        // Turn in the quest
        if (isTalkToQuest)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🗣️ CheckForGameObjectQuestEnderInRange: Bot {} turning in TALK-TO quest {} ({}) to GameObject {}",
                         bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());
        }
        else if (isDeliveryQuest)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "📬 CheckForGameObjectQuestEnderInRange: Bot {} turning in DELIVERY quest {} ({}) to GameObject {}",
                         bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🎯 CheckForGameObjectQuestEnderInRange: Bot {} turning in COMPLETE quest {} ({}) to GameObject {}",
                         bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());
        }

        if (CompleteQuestTurnInAtGameObject(ai, questId, closestQuestEnder))
        {
            anyQuestTurnedIn = true;
            TC_LOG_DEBUG("module.playerbot.quest", "✅ CheckForGameObjectQuestEnderInRange: Bot {} successfully turned in quest {} ({})",
                         bot->GetName(), questId, quest->GetLogTitle());
        }
    }

    return anyQuestTurnedIn;
}

// ========================================================================
// GAMEOBJECT QUEST TURN-IN - Complete quest at GameObject quest ender
// ========================================================================

bool QuestStrategy::CompleteQuestTurnInAtGameObject(BotAI* ai, uint32 questId, GameObject* questEnder)
{
    if (!ai || !ai->GetBot() || !questEnder)
        return false;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    TC_LOG_DEBUG("module.playerbot.quest", "🏆 CompleteQuestTurnInAtGameObject: Bot {} completing quest {} ({}) at GameObject {}",
                 bot->GetName(), questId, quest->GetLogTitle(), questEnder->GetName());

    // ========================================================================
    // PHASE 1: Determine if quest has choice rewards
    // ========================================================================

    bool hasChoiceRewards = false;
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        if (quest->RewardChoiceItemId[i] > 0)
        {
            hasChoiceRewards = true;
            break;
        }
    }

    // ========================================================================
    // PHASE 2: Select best reward using EquipmentManager evaluation system
    // ========================================================================

    uint32 selectedRewardIndex = 0; // Default to first choice

    if (hasChoiceRewards)
    {
        // Get EquipmentManager via GameSystemsManager facade (Phase 6.1)
        Playerbot::EquipmentManager* equipMgr = ai->GetGameSystems()->GetEquipmentManager();
        if (!equipMgr)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ EquipmentManager not available for quest reward selection");
            selectedRewardIndex = 0;
        }
        else
        {
            float bestScore = -10000.0f;
            uint32 bestChoice = 0;
            bool foundUsableReward = false;

            TC_LOG_DEBUG("module.playerbot.quest", "🎁 Evaluating {} reward choices for quest {}",
                         QUEST_REWARD_CHOICES_COUNT, questId);

            for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
            {
                uint32 itemId = quest->RewardChoiceItemId[i];
                if (itemId == 0)
                    continue;

                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
                if (!itemTemplate)
                {
                    TC_LOG_WARN("module.playerbot.quest", "⚠️ Invalid item template for reward choice {} (itemId {})",
                                i, itemId);
                    continue;
                }

                // Check if bot can equip this item (class/level restrictions)
                if (!equipMgr->CanEquipItem(itemTemplate))
                {
                    TC_LOG_TRACE("module.playerbot.quest", "❌ Bot {} cannot equip reward choice {}: {} (class/level restriction)",
                                 bot->GetName(), i, itemTemplate->GetName(LOCALE_enUS));
                    continue;
                }

                // Calculate comprehensive item score using EquipmentManager's stat priority system
                float itemScore = equipMgr->CalculateItemTemplateScore(itemTemplate);

                TC_LOG_DEBUG("module.playerbot.quest", "   Choice {}: {} - Score: {:.2f} (ilvl {}, quality {})",
                             i,
                             itemTemplate->GetName(LOCALE_enUS),
                             itemScore,
                             itemTemplate->GetBaseItemLevel(),
                             itemTemplate->GetQuality());

                if (itemScore > bestScore)
                {
                    bestScore = itemScore;
                    bestChoice = i;
                    foundUsableReward = true;
                }
            }

            if (foundUsableReward)
            {
                selectedRewardIndex = bestChoice;
                ItemTemplate const* selectedItem = sObjectMgr->GetItemTemplate(quest->RewardChoiceItemId[bestChoice]);

                TC_LOG_DEBUG("module.playerbot.quest", "✅ Selected reward choice {}: {} (score: {:.2f})",
                             bestChoice,
                             selectedItem ? selectedItem->GetName(LOCALE_enUS) : "UNKNOWN",
                             bestScore);
            }
            else
            {
                // No usable rewards found, select first available for vendor value
                for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
                {
                    if (quest->RewardChoiceItemId[i] > 0)
                    {
                        selectedRewardIndex = i;
                        TC_LOG_WARN("module.playerbot.quest", "⚠️ No usable rewards found, selecting first available choice {} for vendor value",
                                    i);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        TC_LOG_TRACE("module.playerbot.quest", "📦 Quest {} has no choice rewards (fixed rewards only)", questId);
    }

    // ========================================================================
    // PHASE 3: Actually complete the quest and receive rewards
    // ========================================================================

    // Get the selected reward item ID (0 if no choice rewards)
    uint32 selectedItemId = 0;
    if (hasChoiceRewards && selectedRewardIndex < QUEST_REWARD_CHOICES_COUNT)
    {
        selectedItemId = quest->RewardChoiceItemId[selectedRewardIndex];
    }

    // For GameObjects, we still use the same reward API but pass nullptr for the Object* parameter
    // since TrinityCore's RewardQuest() doesn't actually use the questgiver for reward processing
    if (bot->CanRewardQuest(quest, LootItemType::Item, selectedItemId, false))
    {
        // CRITICAL: RewardQuest expects a valid Object*, but for GameObjects we need special handling
        // TrinityCore's RewardQuest signature: void RewardQuest(Quest const* quest, LootItemType lootType, uint32 rewardId, Object* questGiver, bool announce);
        // We pass nullptr as questGiver since the rewards are processed independently of the quest giver
        bot->RewardQuest(quest, LootItemType::Item, selectedItemId, nullptr, false);

        TC_LOG_DEBUG("module.playerbot.quest", "✅ CompleteQuestTurnInAtGameObject: Bot {} successfully completed quest {} with reward choice {} (itemId: {})",
                     bot->GetName(), questId, selectedRewardIndex, selectedItemId);

        // Clear failure count on success
        _questTurnInFailures.erase(questId);

        // Increment quest completion counter
        _questsCompleted++;

        return true;
    }
    else
    {
        // Track consecutive failures to prevent infinite loops
        _questTurnInFailures[questId]++;
        uint32 failureCount = _questTurnInFailures[questId];

        TC_LOG_DEBUG("module.playerbot.quest", "❌ CompleteQuestTurnInAtGameObject: Bot {} failed CanRewardQuest check for quest {} (failure #{}, missing requirements?)",
                     bot->GetName(), questId, failureCount);

        // After MAX_QUEST_TURNIN_FAILURES consecutive failures, abandon the quest
        if (failureCount >= MAX_QUEST_TURNIN_FAILURES)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "CompleteQuestTurnInAtGameObject: Bot {} abandoning quest {} after {} failures",
                         bot->GetName(), questId, failureCount);

            bot->RemoveActiveQuest(questId);
            bot->AbandonQuest(questId);
            _questTurnInFailures.erase(questId);

            // Blacklist this quest to prevent re-accepting it immediately
            if (_acceptanceManager)
                _acceptanceManager->BlacklistQuest(questId);

            TC_LOG_INFO("module.playerbot.quest", "✅ Bot {} abandoned broken quest {} - will search for new quests",
                        bot->GetName(), questId);
        }

        return false;
    }
}

bool QuestStrategy::CompleteQuestTurnIn(BotAI* ai, uint32 questId, ::Unit* questEnder)
{
    if (!ai || !ai->GetBot() || !questEnder)
        return false;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    TC_LOG_DEBUG("module.playerbot.quest", "🏆 CompleteQuestTurnIn: Bot {} completing quest {} ({}) with NPC {}",
                 bot->GetName(), questId, quest->GetLogTitle(), questEnder->GetName());

    // ========================================================================
    // PHASE 1: Determine if quest has choice rewards
    // ========================================================================

    bool hasChoiceRewards = false;
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        if (quest->RewardChoiceItemId[i] > 0)
        {
            hasChoiceRewards = true;
            break;
        }
    }

    // ========================================================================
    // PHASE 2: Select best reward using EquipmentManager evaluation system
    // ========================================================================

    uint32 selectedRewardIndex = 0; // Default to first choice

    if (hasChoiceRewards)
    {
        // Get EquipmentManager via GameSystemsManager facade (Phase 6.1)
        Playerbot::EquipmentManager* equipMgr = ai->GetGameSystems()->GetEquipmentManager();
        if (!equipMgr)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "⚠️ EquipmentManager not available for quest reward selection");
            // Fall back to first choice
            selectedRewardIndex = 0;
        }
        else
        {
            float bestScore = -10000.0f; // Start with very low score
            uint32 bestChoice = 0;
            bool foundUsableReward = false;

            TC_LOG_DEBUG("module.playerbot.quest", "🎁 Evaluating {} reward choices for quest {}",
                         QUEST_REWARD_CHOICES_COUNT, questId);

            for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
            {
                uint32 itemId = quest->RewardChoiceItemId[i];
                if (itemId == 0)
                    continue;

                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
                if (!itemTemplate)
                {
                    TC_LOG_WARN("module.playerbot.quest", "⚠️ Invalid item template for reward choice {} (itemId {})",
                                i, itemId);
                    continue;
                }

                // Check if bot can equip this item (class/level restrictions)
                if (!equipMgr->CanEquipItem(itemTemplate))
                {
                    TC_LOG_TRACE("module.playerbot.quest", "❌ Bot {} cannot equip reward choice {}: {} (class/level restriction)",
                                 bot->GetName(), i, itemTemplate->GetName(LOCALE_enUS));
                    continue;
                }

                // Calculate comprehensive item score using EquipmentManager's stat priority system
                float itemScore = equipMgr->CalculateItemTemplateScore(itemTemplate);

            TC_LOG_DEBUG("module.playerbot.quest", "   Choice {}: {} - Score: {:.2f} (ilvl {}, quality {})",
                         i,
                         itemTemplate->GetName(LOCALE_enUS),
                         itemScore,
                         itemTemplate->GetBaseItemLevel(),
                         itemTemplate->GetQuality());

            if (itemScore > bestScore)
            {
                bestScore = itemScore;
                bestChoice = i;
                foundUsableReward = true;
            }
        }

        if (foundUsableReward)
        {
            selectedRewardIndex = bestChoice;
            ItemTemplate const* selectedItem = sObjectMgr->GetItemTemplate(quest->RewardChoiceItemId[bestChoice]);

            TC_LOG_DEBUG("module.playerbot.quest", "✅ Selected reward choice {}: {} (score: {:.2f})",
                         bestChoice,
                         selectedItem ? selectedItem->GetName(LOCALE_enUS) : "UNKNOWN",
                         bestScore);
        }
        else
        {
            // No usable rewards found, select first available for vendor value
            for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
            {
                if (quest->RewardChoiceItemId[i] > 0)
                {
                    selectedRewardIndex = i;
                    TC_LOG_WARN("module.playerbot.quest", "⚠️ No usable rewards found, selecting first available choice {} for vendor value",
                                i);
                    break;
                }
            }
        }
        } // end else (equipMgr available)
    }
    else
    {
        TC_LOG_TRACE("module.playerbot.quest", "📦 Quest {} has no choice rewards (fixed rewards only)", questId);
    }

    // ========================================================================
    // PHASE 3: Actually complete the quest and receive rewards
    // ========================================================================

    // Get the selected reward item ID (0 if no choice rewards)
    uint32 selectedItemId = 0;
    if (hasChoiceRewards && selectedRewardIndex < QUEST_REWARD_CHOICES_COUNT)
    {
        selectedItemId = quest->RewardChoiceItemId[selectedRewardIndex];
    }

    if (bot->CanRewardQuest(quest, LootItemType::Item, selectedItemId, false))
    {
        bot->RewardQuest(quest, LootItemType::Item, selectedItemId, questEnder, false);

        TC_LOG_DEBUG("module.playerbot.quest", "✅ CompleteQuestTurnIn: Bot {} successfully completed quest {} with reward choice {} (itemId: {})",
                     bot->GetName(), questId, selectedRewardIndex, selectedItemId);

        // Clear failure count on success
        _questTurnInFailures.erase(questId);

        // Increment quest completion counter
        _questsCompleted++;

        return true;
    }
    else
    {
        // Track consecutive failures to prevent infinite loops
        _questTurnInFailures[questId]++;
        uint32 failureCount = _questTurnInFailures[questId];

        TC_LOG_DEBUG("module.playerbot.quest", "❌ CompleteQuestTurnIn: Bot {} failed CanRewardQuest check for quest {} (failure #{}, missing requirements?)",
                     bot->GetName(), questId, failureCount);

        // After MAX_QUEST_TURNIN_FAILURES consecutive failures, abandon the quest
        // This prevents infinite loops on quests with missing required items (e.g., delivery quests where item was lost)
        if (failureCount >= MAX_QUEST_TURNIN_FAILURES)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "CompleteQuestTurnIn: Bot {} abandoning quest {} after {} failures",
                         bot->GetName(), questId, failureCount);

            bot->RemoveActiveQuest(questId);
            bot->AbandonQuest(questId);
            _questTurnInFailures.erase(questId);

            // Blacklist this quest to prevent re-accepting it immediately
            if (_acceptanceManager)
                _acceptanceManager->BlacklistQuest(questId);

            TC_LOG_INFO("module.playerbot.quest", "✅ Bot {} abandoned broken quest {} - will search for new quests",
                        bot->GetName(), questId);
        }

        return false;
    }
}

// ========================================================================
// QUEST AREA WANDERING SYSTEM - Patrol while waiting for respawns
// ========================================================================

bool QuestStrategy::ShouldWanderInQuestArea(BotAI* ai, ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);

    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return false;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // ========================================================================
    // MINE/CAVE FIX: ALWAYS enable wandering for MONSTER/ITEM objectives
    // InitializeQuestAreaWandering will use creature spawn locations which have
    // correct Z coordinates for mine/cave interiors, not 2D POI points.
    // ========================================================================
    if (questObjective.Type == QUEST_OBJECTIVE_MONSTER || questObjective.Type == QUEST_OBJECTIVE_ITEM)
    {
        TC_LOG_DEBUG("module.playerbot.quest", "✅ ShouldWanderInQuestArea: Quest {} objective {} is MONSTER/ITEM type - wandering enabled (mine/cave fix)",
                     objective.questId, objective.objectiveIndex);
        return true;
    }

    // For other objective types, check if quest has area data (QuestPOI with multiple points)
    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(objective.questId);

    if (!poiData || poiData->Blobs.empty())
        return false;

    // Find blob for current objective and map
    for (auto const& blob : poiData->Blobs)
    {
        if (blob.MapID == static_cast<int32>(bot->GetMapId()) &&
            blob.ObjectiveIndex == static_cast<int32>(objective.objectiveIndex))
        {
            // Area wandering is only useful if there are multiple points defining a region
            if (blob.Points.size() >= 2)
            {
                TC_LOG_DEBUG("module.playerbot.quest", "✅ ShouldWanderInQuestArea: Quest {} objective {} has {} POI points - wandering enabled",
                             objective.questId, objective.objectiveIndex, blob.Points.size());
                return true;
            }
        }
    }

    return false;
}

void QuestStrategy::InitializeQuestAreaWandering(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Clear existing wander points
    _questAreaWanderPoints.clear();
    _currentWanderPointIndex = 0;

    // Get quest data to determine objective type
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // ========================================================================
    // MINE/CAVE FIX: For MONSTER objectives, use actual creature spawn locations
    // instead of POI points. POI points are 2D minimap polygons at surface level,
    // but creature spawns have correct Z coordinates inside mines/caves.
    // ========================================================================
    if (questObjective.Type == QUEST_OBJECTIVE_MONSTER || questObjective.Type == QUEST_OBJECTIVE_ITEM)
    {
        uint32 creatureEntry = 0;

        if (questObjective.Type == QUEST_OBJECTIVE_MONSTER)
        {
            creatureEntry = questObjective.ObjectID;
        }
        else if (questObjective.Type == QUEST_OBJECTIVE_ITEM)
        {
            // For ITEM objectives, find which creature drops this item
            uint32 itemId = questObjective.ObjectID;
            QueryResult result = WorldDatabase.PQuery("SELECT Entry FROM creature_loot_template WHERE Item = {} LIMIT 1", itemId);
            if (result)
            {
                Field* fields = result->Fetch();
                creatureEntry = fields[0].GetUInt32();
            }
        }

        if (creatureEntry != 0)
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ InitializeQuestAreaWandering: Bot {} - Using CREATURE SPAWN locations for entry {} (mine/cave fix)",
                         bot->GetName(), creatureEntry);

            // Query all spawn locations for this creature on bot's map
            auto const& creatureSpawnData = sObjectMgr->GetAllCreatureData();
            uint32 spawnsOnMap = 0;

            for (auto const& pair : creatureSpawnData)
            {
                CreatureData const& data = pair.second;

                if (data.id != creatureEntry)
                    continue;

                if (data.mapId != bot->GetMapId())
                    continue;

                // Check if spawn is within reasonable distance (1000 yards - covers large mines)
                float distance = bot->GetExactDist2d(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());
                if (distance > 1000.0f)
                    continue;

                Position pos;
                pos.Relocate(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ());
                _questAreaWanderPoints.push_back(pos);
                spawnsOnMap++;

                TC_LOG_DEBUG("module.playerbot.quest", "📍 Spawn point {}: ({:.1f}, {:.1f}, {:.1f}) - distance={:.1f}",
                             spawnsOnMap, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), distance);

                // Limit to 20 spawn points to avoid excessive wandering
                if (spawnsOnMap >= 20)
                    break;
            }

            if (!_questAreaWanderPoints.empty())
            {
                // Sort by distance to bot for efficient pathing
                Position botPos = bot->GetPosition();
                std::sort(_questAreaWanderPoints.begin(), _questAreaWanderPoints.end(),
                    [&botPos](Position const& a, Position const& b) {
                        return botPos.GetExactDist2d(a.GetPositionX(), a.GetPositionY()) <
                               botPos.GetExactDist2d(b.GetPositionX(), b.GetPositionY());
                    });

                _currentWanderPointIndex = 0; // Start with nearest spawn
                TC_LOG_DEBUG("module.playerbot.quest", "✅ Bot {} initialized {} SPAWN wander points (mine/cave interior)",
                             bot->GetName(), _questAreaWanderPoints.size());
                return; // Successfully initialized with spawn data
            }

            TC_LOG_WARN("module.playerbot.quest", "⚠️ Bot {} - No spawn data found for creature {} on map {}, falling back to POI",
                         bot->GetName(), creatureEntry, bot->GetMapId());
        }
    }

    // ========================================================================
    // FALLBACK: Use Quest POI data (original behavior for non-mine areas)
    // ========================================================================
    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(objective.questId);

    if (!poiData || poiData->Blobs.empty())
        return;

    // Find blob for current objective and map
    for (auto const& blob : poiData->Blobs)
    {
        if (blob.MapID == static_cast<int32>(bot->GetMapId()) &&
            blob.ObjectiveIndex == static_cast<int32>(objective.objectiveIndex))
        {
            TC_LOG_DEBUG("module.playerbot.quest", "🗺️ InitializeQuestAreaWandering: Bot {} - Found quest area with {} POI points (fallback)",
                         bot->GetName(), blob.Points.size());

            // Convert POI points to wander positions
            for (auto const& point : blob.Points)
            {
                Position pos;
                pos.Relocate(static_cast<float>(point.X), static_cast<float>(point.Y), static_cast<float>(point.Z));
                _questAreaWanderPoints.push_back(pos);

                TC_LOG_DEBUG("module.playerbot.quest", "📍 POI wander point {}: ({:.1f}, {:.1f}, {:.1f})",
                             _questAreaWanderPoints.size(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
            }

            // Start at a random point based on bot GUID (deterministic but unique per bot)
            if (!_questAreaWanderPoints.empty())
            {
                _currentWanderPointIndex = bot->GetGUID().GetCounter() % _questAreaWanderPoints.size();
                TC_LOG_DEBUG("module.playerbot.quest", "🎲 Bot {} starting wander at POI point {} of {}",
                             bot->GetName(), _currentWanderPointIndex, _questAreaWanderPoints.size());
            }

            break;
        }
    }
}

void QuestStrategy::WanderInQuestArea(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();
    // Check if wandering is initialized
    if (_questAreaWanderPoints.empty())
    {
        TC_LOG_DEBUG("module.playerbot.quest", "⚠️ WanderInQuestArea: Bot {} - No wander points initialized",
                     bot->GetName());
        return;
    }

    // Throttle wandering - only move to next point every 10 seconds
    uint32 currentTime = GameTime::GetGameTimeMS();
    constexpr uint32 WANDER_INTERVAL_MS = 10000; // 10 seconds

    if (currentTime - _lastWanderTime < WANDER_INTERVAL_MS)
    {
        // Still waiting - don't wander yet
        return;
    }

    // Update wander time
    _lastWanderTime = currentTime;

    // MINE/CAVE FIX: Use 3D distance for wander point arrival check
    // This ensures bots properly navigate to different Z levels inside mines
    Position const& currentWanderPoint = _questAreaWanderPoints[_currentWanderPointIndex];
    float distance3D = bot->GetExactDist(currentWanderPoint);
    if (distance3D < 10.0f)
    {
        // Reached current point - move to next point
        _currentWanderPointIndex = (_currentWanderPointIndex + 1) % _questAreaWanderPoints.size();

        TC_LOG_DEBUG("module.playerbot.quest", "✅ WanderInQuestArea: Bot {} reached wander point (3D dist {:.1f}), moving to next point {} of {}",
                     bot->GetName(), distance3D, _currentWanderPointIndex, _questAreaWanderPoints.size());
    }

    // Move to current wander point
    Position const& targetPoint = _questAreaWanderPoints[_currentWanderPointIndex];

    TC_LOG_DEBUG("module.playerbot.quest", "🚶 WanderInQuestArea: Bot {} wandering to point {} at ({:.1f}, {:.1f}, {:.1f}), dist3D={:.1f}",
                 bot->GetName(), _currentWanderPointIndex,
                 targetPoint.GetPositionX(), targetPoint.GetPositionY(), targetPoint.GetPositionZ(),
                 bot->GetExactDist(targetPoint));

    BotMovementUtil::MoveToPosition(bot, targetPoint);
}

bool QuestStrategy::IsItemFromCreatureLoot(uint32 itemId) const
{
    // Check if this item is dropped by creatures in creature_loot_template
    // This determines whether we need to kill creatures (EngageQuestTargets)
    // or interact with GameObjects (CollectQuestItems)

    TC_LOG_DEBUG("module.playerbot.quest", "🔍 IsItemFromCreatureLoot: Checking if item {} comes from creature loot", itemId);

    // PERFORMANCE: Use static cache to avoid repeated database queries
    // Key: itemId, Value: isCreatureLoot
    static std::unordered_map<uint32, bool> itemLootCache;
    // DEADLOCK FIX: Changed to recursive_mutex
    static std::recursive_mutex cacheMutex;

    // Check cache first for performance
    {
        std::lock_guard lock(cacheMutex);
        auto cacheIt = itemLootCache.find(itemId);
        if (cacheIt != itemLootCache.end())
        {
            TC_LOG_DEBUG("module.playerbot.quest", "✅ IsItemFromCreatureLoot: Item {} found in cache, isCreatureLoot={}",
                         itemId, cacheIt->second);
            return cacheIt->second;
        }
    }

    // Cache miss - query database
    TC_LOG_DEBUG("module.playerbot.quest", "🔍 IsItemFromCreatureLoot: Item {} NOT in cache, querying creature_loot_template", itemId);

    // Query creature_loot_template to check if this item is dropped by creatures
    // We only need to know if ANY creature drops this item (not which creatures)
    QueryResult result = WorldDatabase.PQuery("SELECT 1 FROM creature_loot_template WHERE Item = {} LIMIT 1", itemId);

    bool isCreatureLoot = (result != nullptr);

    // Cache the result for future queries (thread-safe)
    {
        std::lock_guard lock(cacheMutex);
        itemLootCache[itemId] = isCreatureLoot;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "📊 IsItemFromCreatureLoot: Item {} {} creature loot (database query complete, result cached)",
                 itemId, isCreatureLoot ? "IS" : "is NOT");

    return isCreatureLoot;
}

bool QuestStrategy::RequiresSpellClickInteraction(uint32 creatureEntry) const
{
    // Check if this creature has spell click interaction configured
    // NPCs with spell click are "talk to" NPCs (e.g., "Injured Soldier" Quest 28809)
    // NPCs without spell click are attackable mobs (even if neutral, e.g., "Blackrock Worg" mob 49871)

    TC_LOG_DEBUG("module.playerbot.quest", "🔍 RequiresSpellClickInteraction: Checking creature entry {}", creatureEntry);

    // PERFORMANCE: Use static cache to avoid repeated database queries
    static std::unordered_map<uint32, bool> spellClickCache;
    // DEADLOCK FIX: Changed to recursive_mutex
    static std::recursive_mutex cacheMutex;

    // Check cache first
    {
        std::lock_guard lock(cacheMutex);
        auto cacheIt = spellClickCache.find(creatureEntry);
        if (cacheIt != spellClickCache.end())
        {
            TC_LOG_DEBUG("module.playerbot.quest", "✅ RequiresSpellClickInteraction: Creature {} found in cache, hasSpellClick={}",
                         creatureEntry, cacheIt->second);
            return cacheIt->second;
        }
    }

    // Cache miss - query database
    TC_LOG_DEBUG("module.playerbot.quest", "🔍 RequiresSpellClickInteraction: Creature {} NOT in cache, querying npc_spellclick_spells", creatureEntry);

    // Query npc_spellclick_spells to check if this NPC has spell click interaction
    QueryResult result = WorldDatabase.PQuery("SELECT 1 FROM npc_spellclick_spells WHERE npc_entry = {} LIMIT 1", creatureEntry);

    bool hasSpellClick = (result != nullptr);

    // Cache the result
    {
        std::lock_guard lock(cacheMutex);
        spellClickCache[creatureEntry] = hasSpellClick;
    }

    TC_LOG_DEBUG("module.playerbot.quest", "📊 RequiresSpellClickInteraction: Creature {} {} spell click interaction (database query complete, result cached)",
                 creatureEntry, hasSpellClick ? "HAS" : "does NOT have");

    return hasSpellClick;
}

// ============================================================================
// OBJECTIVE FAILURE TRACKING (prevent infinite loops on unreachable objectives)
// ============================================================================

uint32 QuestStrategy::GetObjectiveFailures(uint32 questId, uint8 objectiveIndex)
{
    uint32 key = MakeObjectiveKey(questId, objectiveIndex);
    auto it = _questObjectiveFailures.find(key);
    if (it == _questObjectiveFailures.end())
        return 0;

    // Check if failures should be reset (timeout expired)
    uint32 currentTime = GameTime::GetGameTimeMS();
    if (currentTime - it->second.lastFailureTime > OBJECTIVE_FAILURE_RESET_TIME_MS)
    {
        _questObjectiveFailures.erase(it);
        return 0;
    }

    return it->second.failureCount;
}

void QuestStrategy::IncrementObjectiveFailures(uint32 questId, uint8 objectiveIndex)
{
    uint32 key = MakeObjectiveKey(questId, objectiveIndex);
    auto& info = _questObjectiveFailures[key];
    info.failureCount++;
    info.lastFailureTime = GameTime::GetGameTimeMS();
}

void QuestStrategy::ResetObjectiveFailures(uint32 questId, uint8 objectiveIndex)
{
    uint32 key = MakeObjectiveKey(questId, objectiveIndex);
    _questObjectiveFailures.erase(key);
}

bool QuestStrategy::IsObjectiveBlacklisted(uint32 questId, uint8 objectiveIndex)
{
    uint32 failures = GetObjectiveFailures(questId, objectiveIndex);
    return failures >= MAX_QUEST_OBJECTIVE_FAILURES;
}

// ============================================================================
// DYNAMIC SPAWN HANDLER INTEGRATION
// ============================================================================

bool QuestStrategy::TryTriggerDynamicSpawn(BotAI* ai, ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Initialize dynamic spawn handler if not already done
    if (!_dynamicSpawnHandler)
    {
        _dynamicSpawnHandler = std::make_unique<DynamicSpawnHandler>(bot);
        _dynamicSpawnHandler->PreloadQuestSpawnData();

        TC_LOG_DEBUG("module.playerbot.quest",
            "🔮 TryTriggerDynamicSpawn: Initialized DynamicSpawnHandler for bot {}",
            bot->GetName());
    }

    // Check if this objective requires a dynamic spawn
    auto spawnInfo = _dynamicSpawnHandler->GetSpawnInfoForObjective(objective.questId, objective.objectiveIndex);
    if (!spawnInfo || !spawnInfo->IsValid())
    {
        TC_LOG_TRACE("module.playerbot.quest",
            "TryTriggerDynamicSpawn: Bot {} - No dynamic spawn required for quest {} objective {}",
            bot->GetName(), objective.questId, objective.objectiveIndex);
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.quest",
        "🔮 TryTriggerDynamicSpawn: Bot {} - Quest {} objective {} requires dynamic spawn of creature {} (trigger type {})",
        bot->GetName(), objective.questId, objective.objectiveIndex,
        spawnInfo->creatureEntry, static_cast<uint8>(spawnInfo->triggerType));

    // Check trigger type and handle accordingly
    switch (spawnInfo->triggerType)
    {
        case SpawnTriggerType::AREA_TRIGGER:
        {
            // Check if we're already in the area trigger
            if (_dynamicSpawnHandler->IsInAreaTrigger(spawnInfo->areaTriggerDBC))
            {
                // Try to trigger it
                if (_dynamicSpawnHandler->TriggerAreaTrigger(spawnInfo->areaTriggerDBC))
                {
                    TC_LOG_DEBUG("module.playerbot.quest",
                        "✅ TryTriggerDynamicSpawn: Bot {} triggered area trigger {}",
                        bot->GetName(), spawnInfo->areaTriggerDBC);
                    return true;
                }
            }
            else
            {
                // Need to move to the area trigger
                Position atPos = _dynamicSpawnHandler->GetAreaTriggerPosition(spawnInfo->areaTriggerDBC);
                if (atPos.GetPositionX() != 0.0f || atPos.GetPositionY() != 0.0f)
                {
                    TC_LOG_DEBUG("module.playerbot.quest",
                        "📍 TryTriggerDynamicSpawn: Bot {} moving to area trigger {} at ({:.1f}, {:.1f}, {:.1f})",
                        bot->GetName(), spawnInfo->areaTriggerDBC,
                        atPos.GetPositionX(), atPos.GetPositionY(), atPos.GetPositionZ());

                    BotMovementUtil::MoveToPosition(bot, atPos);
                    return true;
                }
            }
            break;
        }

        case SpawnTriggerType::QUEST_ACCEPT:
            // Quest accept spawns happen automatically when quest is accepted
            // If we're here, the spawn should have already happened
            TC_LOG_DEBUG("module.playerbot.quest",
                "🤔 TryTriggerDynamicSpawn: Bot {} - Quest accept spawn for quest {} (creature {} should already be spawned)",
                bot->GetName(), objective.questId, spawnInfo->creatureEntry);
            return false;

        case SpawnTriggerType::GOSSIP_SELECT:
            // Would need gossip interaction - complex to implement
            TC_LOG_DEBUG("module.playerbot.quest",
                "⚠️ TryTriggerDynamicSpawn: Bot {} - Gossip spawn not yet supported (creature {} for quest {})",
                bot->GetName(), spawnInfo->creatureEntry, objective.questId);
            return false;

        case SpawnTriggerType::PHASE_SHIFT:
            // Phase-based spawns require meeting phase conditions
            // The NPC should be visible if bot has correct phase
            TC_LOG_DEBUG("module.playerbot.quest",
                "⚠️ TryTriggerDynamicSpawn: Bot {} - Phase spawn (creature {} may require phase conditions)",
                bot->GetName(), spawnInfo->creatureEntry);
            return false;

        default:
            break;
    }

    return false;
}

} // namespace Playerbot
