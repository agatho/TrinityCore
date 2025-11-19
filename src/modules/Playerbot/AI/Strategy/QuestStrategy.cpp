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
#include "Group.h"
#include "QuestDef.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Item.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SharedDefines.h"
#include "../../Game/QuestAcceptanceManager.h"
#include "../../Quest/QuestHubDatabase.h"
#include "../../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "../../Spatial/SpatialGridQueryHelpers.h"  // Thread-safe spatial queries
#include "../../Equipment/EquipmentManager.h"  // For reward evaluation
#include "../../Movement/UnifiedMovementCoordinator.h  // Phase 2: Unified movement system"
#include "../../Movement/Arbiter/MovementPriorityMapper.h"
#include "LootItemType.h"  // For LootItemType enum used in RewardQuest
#include "UnitAI.h"
#include <limits>
#include "GameTime.h"

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
    TC_LOG_ERROR("module.playerbot.quest", "🔍 QuestStrategy::IsActive ENTRY: ai={}, _active={}",
                 (void*)ai, activeFlag);

    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ QuestStrategy::IsActive: NULL check failed, returning false");
        return false;
    }

    Player* bot = ai->GetBot();
    // NOT active during combat (combat takes priority)
    if (bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚔️ QuestStrategy::IsActive: Bot {} in combat, returning false", bot->GetName());
        return false;
    }

    // Active for ALL levels - questing is always valuable
    // - Below max level: Quest for XP (high priority)
    // - At max level: Quest for gold, reputation, achievements (lower priority)
    TC_LOG_ERROR("module.playerbot.quest", "📋 QuestStrategy::IsActive: Bot {} - _active={}, returning {}",
                 bot->GetName(), activeFlag, activeFlag);
    return activeFlag;
}

float QuestStrategy::GetRelevance(BotAI* ai) const
{
    // CRITICAL DEBUG: Force ERROR level logging to see this!
    TC_LOG_ERROR("module.playerbot.quest", "🔍 QuestStrategy::GetRelevance ENTRY: ai={}, ai->GetBot()={}",
                 (void*)ai, ai ? (void*)ai->GetBot() : nullptr);

    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ QuestStrategy::GetRelevance: NULL check failed, returning 0.0f");
        return 0.0f;
    }

    Player* bot = ai->GetBot();
    // Combat has higher priority - return 0 if in combat
    if (bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚔️ QuestStrategy::GetRelevance: Bot {} in combat, returning 0.0f", bot->GetName());
        return 0.0f;
    }

    bool isMaxLevel = (bot->GetLevel() >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    bool hasObjectives = HasActiveObjectives(ai);

    TC_LOG_ERROR("module.playerbot.quest", "📊 QuestStrategy::GetRelevance: Bot {} - Level={}, MaxLevel={}, hasObjectives={}",
                 bot->GetName(), bot->GetLevel(), isMaxLevel, hasObjectives);

    // HIGH PRIORITY: Has active quest objectives to complete
    if (hasObjectives)
    {
        // Pre-max level: Very high priority (quest for XP)
        // Max level: Medium-high priority (quest for gold/rep)
        float relevance = isMaxLevel ? 60.0f : 70.0f;
        TC_LOG_ERROR("module.playerbot.quest", "✅ QuestStrategy::GetRelevance: Bot {} has objectives, returning {:.1f}",
                     bot->GetName(), relevance);
        return relevance;
    }

    // MEDIUM/LOW PRIORITY: No active quests - search for quest givers
    float relevance;
    if (isMaxLevel)
    {
        // Max level: Lower priority (quests are optional for gold/rep)
        relevance = 30.0f; // Lower than loot=60, solo=10-50
    }
    else
    {
        // Pre-max level: Medium-high priority (actively seek quests to level up)
        relevance = 50.0f; // Higher than solo, actively search for quests
    }

    TC_LOG_ERROR("module.playerbot.quest", "📍 QuestStrategy::GetRelevance: Bot {} (Level {}, MaxLevel={}) no objectives, returning {:.1f}",
                 bot->GetName(), bot->GetLevel(), isMaxLevel, relevance);
    return relevance;
}

void QuestStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UpdateBehavior: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🚀 UpdateBehavior: Bot {} starting quest behavior update", bot->GetName());

    // Don't interrupt combat
    if (bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚔️ UpdateBehavior: Bot {} in combat, skipping", bot->GetName());
        return;
    }
    // Update objective tracker periodically
    uint32 currentTime = GameTime::GetGameTimeMS();
    if (currentTime - _lastObjectiveUpdate > 2000) // Every 2 seconds
    {
        (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->UpdateBotTracking(diff) : (void)0);
        _lastObjectiveUpdate = currentTime;
    }

    // Check if bot has active quests
    bool hasActiveQuests = false;
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId != 0)
        {
            hasActiveQuests = true;
            TC_LOG_ERROR("module.playerbot.quest", "📋 UpdateBehavior: Bot {} has active quest {} in slot {}",
                         bot->GetName(), questId, slot);
            break;
        }
    }

    TC_LOG_ERROR("module.playerbot.quest", "📊 UpdateBehavior: Bot {} hasActiveQuests={}",
                 bot->GetName(), hasActiveQuests);

    if (hasActiveQuests)
    {
        TC_LOG_ERROR("module.playerbot.quest", "🎯 UpdateBehavior: Bot {} processing quest objectives", bot->GetName());
        // Process quest objectives
        ProcessQuestObjectives(ai);
    }
    else
    {
        TC_LOG_ERROR("module.playerbot.quest", "🔍 UpdateBehavior: Bot {} has NO active quests - calling SearchForQuestGivers", bot->GetName());
        // No active quests - search for quest givers to accept new quests
        SearchForQuestGivers(ai);
    }
    TC_LOG_ERROR("module.playerbot.quest", "✅ UpdateBehavior: Bot {} quest behavior update complete", bot->GetName());
}

void QuestStrategy::ProcessQuestObjectives(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;
    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "📍 ProcessQuestObjectives: Bot {} starting objective processing", bot->GetName());

    // Get highest priority objective from ObjectiveTracker
    ObjectiveTracker::ObjectivePriority priority = (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->GetHighestPriorityObjective() : ObjectiveTracker::ObjectivePriority());

    TC_LOG_ERROR("module.playerbot.quest", "🎯 ProcessQuestObjectives: Bot {} - priority.questId={}, priority.objectiveIndex={}",
                 bot->GetName(), priority.questId, priority.objectiveIndex);

    if (priority.questId == 0)
    {
        // ObjectiveTracker doesn't know about bot's quests - initialize it
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} ObjectiveTracker returned questId=0, initializing from quest log",
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

            TC_LOG_ERROR("module.playerbot.quest", "🔄 ProcessQuestObjectives: Bot {} registering quest {} with ObjectiveTracker (Quest has {} objectives)",
                         bot->GetName(), questId, quest->Objectives.size());

            // Skip quests with no objectives (autocomplete/scripted quests)
            if (quest->Objectives.empty())
            {
                TC_LOG_ERROR("module.playerbot.quest", "⚠️ Quest {} has NO objectives - skipping registration (likely autocomplete/scripted quest)",
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

                (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->StartTrackingObjective(objData) : (void)0);
            }
        }
        // Try again after initialization
        priority = (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->GetHighestPriorityObjective() : ObjectiveTracker::ObjectivePriority());
        TC_LOG_ERROR("module.playerbot.quest", "🔄 ProcessQuestObjectives: Bot {} after initialization - priority.questId={}, priority.objectiveIndex={}",
                     bot->GetName(), priority.questId, priority.objectiveIndex);

        if (priority.questId == 0)
        {
            // Still no objectives - bot has only autocomplete/scripted quests with no trackable objectives
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} has NO trackable objectives (only autocomplete/scripted quests)",
                         bot->GetName());

            // CRITICAL FIX: Check if any of these quests are COMPLETE and ready to turn in
            // Don't fall back to searching for new quests if we have completed quests!
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 questId = bot->GetQuestSlotQuestId(slot);
                if (questId == 0)
                    continue;

                // Check if quest is complete
                if (bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
                {
                    TC_LOG_ERROR("module.playerbot.quest", "✅ ProcessQuestObjectives: Bot {} has COMPLETE autocomplete quest {} - turning it in!",
                                 bot->GetName(), questId);
                    TurnInQuest(ai, questId);
                    return;
                }
            }

            // No completed quests - fall back to searching for new quests
            TC_LOG_ERROR("module.playerbot.quest", "📍 ProcessQuestObjectives: Bot {} has no completed quests, searching for new quests",
                         bot->GetName());
            SearchForQuestGivers(ai);
            return;
        }
    }
    // Get objective state
    ObjectiveTracker::ObjectiveState objective = (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->GetObjectiveState( priority.questId, priority.objectiveIndex) : ObjectiveTracker::ObjectiveState());

    // Cache current objective info
    _currentQuestId = objective.questId;
    _currentObjectiveIndex = objective.objectiveIndex;

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest)
        return;
    // Check if quest is complete - turn it in
    if (bot->GetQuestStatus(objective.questId) == QUEST_STATUS_COMPLETE)
    {
        TurnInQuest(ai, objective.questId);
        return;
    }

    // Determine objective type and execute appropriate behavior
    QuestObjective const* questObjective = quest->Objectives.size() > objective.objectiveIndex
        ? &quest->Objectives[objective.objectiveIndex]
        : nullptr;

    if (!questObjective)
        return;

    // Route to appropriate handler based on objective type
    TC_LOG_ERROR("module.playerbot.quest", "🔀 ProcessQuestObjectives: Bot {} - Routing objective type {} for quest {}",
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
                TC_LOG_ERROR("module.playerbot.quest", "🎯 ProcessQuestObjectives: Bot {} - Quest {} is USE ITEM quest (StartItem={}), calling UseQuestItemOnTarget",
                             bot->GetName(), objective.questId, quest->GetSrcItemId());
                UseQuestItemOnTarget(ai, objective);
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.quest", "⚔️ ProcessQuestObjectives: Bot {} - Quest {} is KILL TARGET quest, calling EngageQuestTargets",
                             bot->GetName(), objective.questId);
                EngageQuestTargets(ai, objective);
            }
            break;

        case QUEST_OBJECTIVE_ITEM:
        {
            // CRITICAL FIX: ITEM objectives can require either:
            // 1. Looting from killed creatures (check creature_loot_template)
            // 2. Interacting with GameObjects (check gameobject_loot_template)
            // We need to check which one and route appropriately!

            // Check if item comes from creature loot
            bool isLootFromCreature = IsItemFromCreatureLoot(questObjective->ObjectID);

            if (isLootFromCreature)
            {
                TC_LOG_ERROR("module.playerbot.quest", "⚔️ ProcessQuestObjectives: Bot {} - Item {} comes from CREATURE LOOT, calling EngageQuestTargets for quest {}",
                             bot->GetName(), questObjective->ObjectID, objective.questId);

                // Route to EngageQuestTargets to kill the creature that drops this item
                EngageQuestTargets(ai, objective);
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.quest", "📦 ProcessQuestObjectives: Bot {} - Item {} comes from GAMEOBJECT or ground loot, calling CollectQuestItems for quest {}",
                             bot->GetName(), questObjective->ObjectID, objective.questId);
                // Route to CollectQuestItems for GameObject interaction
                CollectQuestItems(ai, objective);
            }
            break;
        }

        case QUEST_OBJECTIVE_GAMEOBJECT:
            TC_LOG_ERROR("module.playerbot.quest", "📦 ProcessQuestObjectives: Bot {} - Calling CollectQuestItems for quest {}",
                         bot->GetName(), objective.questId);
            CollectQuestItems(ai, objective);
            break;

        case QUEST_OBJECTIVE_AREATRIGGER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
            TC_LOG_ERROR("module.playerbot.quest", "🗺️ ProcessQuestObjectives: Bot {} - Calling ExploreQuestArea for quest {}",
                         bot->GetName(), objective.questId);
            ExploreQuestArea(ai, objective);
            break;

        default:
            TC_LOG_ERROR("module.playerbot.quest", "❓ ProcessQuestObjectives: Bot {} - Unknown objective type {}, calling NavigateToObjective for quest {}",
                         bot->GetName(), questObjective->Type, objective.questId);
            // Unknown objective type - try to navigate to objective location
            NavigateToObjective(ai, objective);
            break;
    }
}

void QuestStrategy::NavigateToObjective(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ NavigateToObjective: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🗺️ NavigateToObjective: Bot {} navigating to quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Get objective position from tracker
    Position objectivePos = GetObjectivePosition(ai, objective);

    TC_LOG_ERROR("module.playerbot.quest", "📍 NavigateToObjective: Bot {} - Objective position: ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(),
                 objectivePos.GetPositionX(), objectivePos.GetPositionY(), objectivePos.GetPositionZ());

    if (objectivePos.GetExactDist2d(0.0f, 0.0f) < 0.1f)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ NavigateToObjective: Bot {} - NO VALID position for objective {} of quest {} (position is at origin)",
                     bot->GetName(), objective.objectiveIndex, objective.questId);
        return;
    }

    float distance = bot->GetExactDist2d(objectivePos.GetPositionX(), objectivePos.GetPositionY());
    TC_LOG_ERROR("module.playerbot.quest", "🚶 NavigateToObjective: Bot {} moving to objective (distance: {:.1f})",
                 bot->GetName(), distance);

    // CRITICAL FIX: Add randomness to prevent all bots standing at exact same spot
    // When moving to quest POI (waiting for respawns), spread bots out in a radius
    // This prevents bot clumping and looks more natural
    Position randomizedPos = objectivePos;

    // Generate random offset within 15-yard radius
    // Use bot GUID as seed for deterministic but unique positioning per bot
    uint32 botSeed = bot->GetGUID().GetCounter();
    float randomAngle = (botSeed % 360) * (M_PI / 180.0f); // Convert bot GUID to angle
    float randomDistance = 5.0f + ((botSeed % 1000) / 1000.0f) * 10.0f; // 5-15 yards

    randomizedPos.Relocate(
        objectivePos.GetPositionX() + cos(randomAngle) * randomDistance,
        objectivePos.GetPositionY() + sin(randomAngle) * randomDistance,
        objectivePos.GetPositionZ()
    );

    TC_LOG_ERROR("module.playerbot.quest", "🎲 NavigateToObjective: Bot {} - Randomized position offset: angle={:.1f}°, distance={:.1f}yd → ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(), randomAngle * (180.0f / M_PI), randomDistance,
                 randomizedPos.GetPositionX(), randomizedPos.GetPositionY(), randomizedPos.GetPositionZ());

    // Move to randomized objective location
    bool moveResult = MoveToObjectiveLocation(ai, randomizedPos);
    TC_LOG_ERROR("module.playerbot.quest", "🚶 NavigateToObjective: Bot {} MoveToObjectiveLocation result: {}",
                 bot->GetName(), moveResult ? "SUCCESS" : "FAILED");
}

void QuestStrategy::EngageQuestTargets(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ EngageQuestTargets: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🎯 EngageQuestTargets: Bot {} searching for quest targets for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Find quest target near bot
    ::Unit* target = FindQuestTarget(ai, objective);

    if (!target)
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - NO hostile target found",
                     bot->GetName());

        // CRITICAL FIX: Check if this is a FRIENDLY NPC interaction quest (like Quest 28809)
        // FindQuestTarget returns nullptr for friendly NPCs to prevent attacking them
        // We need to check if there's a friendly NPC nearby that matches the objective
        Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
        if (quest && objective.objectiveIndex < quest->Objectives.size())
        {
            QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];
            // Scan for friendly NPCs with this entry in range (300 yards to match hostile creature scan)
            std::list<Creature*> nearbyCreatures;
            bot->GetCreatureListWithEntryInGrid(nearbyCreatures, questObjective.ObjectID, 300.0f);
            for (Creature* creature : nearbyCreatures)
            {
                if (!creature || !creature->IsAlive())
                    continue;

                // Check if this is a FRIENDLY NPC (not hostile to bot)
                if (!bot->IsHostileTo(creature))
                {
                    // CRITICAL: Only interact with NPCs that have spell click data
                    // Neutral mobs WITHOUT spell click should be attacked, not interacted with!
                    if (!RequiresSpellClickInteraction(questObjective.ObjectID))
                    {
                        TC_LOG_ERROR("module.playerbot.quest", "⚠️ EngageQuestTargets: Mob {} (Entry: {}) is neutral but has NO spell click - should be ATTACKED, not interacted with!",
                                     creature->GetName(), questObjective.ObjectID);
                        continue;  // Skip this creature - it should be attacked via FindQuestTarget(), not interacted with
                    }

                    float distance = std::sqrt(bot->GetExactDistSq(creature)); // Calculate once from squared distance
                    TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} found FRIENDLY quest NPC {} (Entry: {}) with spell click at distance {:.1f}",
                                 bot->GetName(), creature->GetName(), questObjective.ObjectID, distance);

                    // CRITICAL: Check if objective is already complete BEFORE interacting
                    // GetQuestObjectiveData returns the current progress count for this objective
                    uint32 currentProgress = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
                    uint32 requiredAmount = static_cast<uint32>(questObjective.Amount);

                    if (currentProgress >= requiredAmount)
                    {
                        TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} OBJECTIVE COMPLETE ({} / {}) - skipping interaction with {}",
                                     bot->GetName(), currentProgress, requiredAmount, creature->GetName());
                        return;  // Objective complete - stop interacting
                    }

                    TC_LOG_ERROR("module.playerbot.quest", "📊 EngageQuestTargets: Bot {} objective progress: {} / {} - interaction needed",
                                 bot->GetName(), currentProgress, requiredAmount);

                    // Check if in interaction range
                    if (distance <= INTERACTION_DISTANCE)
                    {
                        // INTERACT with the friendly NPC using spell click (right-click interaction)
                        TC_LOG_ERROR("module.playerbot.quest", "🤝 EngageQuestTargets: Bot {} INTERACTING with friendly NPC {} for quest {} (using HandleSpellClick)",
                                     bot->GetName(), creature->GetName(), objective.questId);

                        // Right-click on the NPC triggers HandleSpellClick
                        // This is used for quest NPCs like "Injured Stormwind Infantry" that have npc_spellclick_spells
                        creature->HandleSpellClick(bot);
                        TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} sent spell click interaction to {} - quest objective should progress",
                                     bot->GetName(), creature->GetName());
                        return;
                    }
                    else
                    {
                        // Move closer to the friendly NPC
                        TC_LOG_ERROR("module.playerbot.quest", "🚶 EngageQuestTargets: Bot {} moving to friendly NPC {} (distance: {:.1f} > INTERACTION_DISTANCE)",
                                     bot->GetName(), creature->GetName(), distance);

                        Position npcPos;
                        npcPos.Relocate(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ());
                        BotMovementUtil::MoveToPosition(bot, npcPos);
                        return;
                    }
                }
            }
        }

        // No friendly NPC found either - wait for respawns or navigate to quest area
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - NO target found (waiting for respawns)",
                     bot->GetName());

        // CRITICAL FIX: Check if quest has an area to wander in
        // If quest has multiple POI points defining an area, wander through it to search for spawns
        // Otherwise, just move to the single POI point with randomness
        if (ShouldWanderInQuestArea(ai, objective))
        {
            // Initialize wandering if not already done
            if (_questAreaWanderPoints.empty())
            {
                TC_LOG_ERROR("module.playerbot.quest", "🗺️ EngageQuestTargets: Bot {} - Initializing quest area wandering",
                             bot->GetName());
                InitializeQuestAreaWandering(ai, objective);
            }
            // Wander through quest area to find respawns
            TC_LOG_ERROR("module.playerbot.quest", "🚶 EngageQuestTargets: Bot {} - Wandering in quest area to search for spawns",
                         bot->GetName());
            WanderInQuestArea(ai);
        }
        else
        {
            // No quest area - just navigate to objective POI with randomness
            TC_LOG_ERROR("module.playerbot.quest", "📍 EngageQuestTargets: Bot {} - No quest area, navigating to objective POI",
                         bot->GetName());
            NavigateToObjective(ai, objective);
        }

        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} found target {} (Entry: {}) at distance {:.1f}",
                 bot->GetName(), target->GetName(), target->GetEntry(), std::sqrt(bot->GetExactDistSq(target)));

    // Check if we should engage this target
    if (!ShouldEngageTarget(ai, target, objective))
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ EngageQuestTargets: Bot {} - Should NOT engage target {} (already at max kills or wrong target)",
                     bot->GetName(), target->GetName());
        return;
    }
    TC_LOG_ERROR("module.playerbot.quest", "⚔️ EngageQuestTargets: Bot {} setting combat target to {} (Entry: {})",
                 bot->GetName(), target->GetName(), target->GetEntry());

    // Set as combat target
    bot->SetTarget(target->GetGUID());

    // CRITICAL: Actually initiate combat with the target!
    // Solution from mod-playerbots: Set bot to combat state, THEN call Attack().
    // When bot is in combat state, ClassAI/combat rotation will automatically
    // start casting spells, which will damage the neutral mob and make it hostile.
    if (!bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚡ EngageQuestTargets: Bot {} not in combat - initiating attack on {} to start combat",
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

        // 4. CRITICAL: Position ranged classes at optimal range
        // This mirrors BotAI::UpdateSoloBehaviors (lines 645-659) for autonomous combat
        // Without this, casters will stand 40+ yards away and never cast spells!
        if (bot->GetClass() == CLASS_HUNTER ||
            bot->GetClass() == CLASS_MAGE ||
            bot->GetClass() == CLASS_WARLOCK ||
            bot->GetClass() == CLASS_PRIEST)
        {
            // Move to optimal range instead of melee
            float optimalRange = 25.0f; // Standard ranged distance
            float currentDistance = std::sqrt(bot->GetExactDistSq(target)); // Calculate once from squared distance

            TC_LOG_ERROR("module.playerbot.quest", "📏 EngageQuestTargets: Bot {} is RANGED class ({}), currentDistance={:.1f}yd, optimalRange={:.1f}yd",
                         bot->GetName(), bot->GetClass(), currentDistance, optimalRange);

            if (currentDistance > optimalRange)
            {
                // Move closer but stay at range
                Position pos = target->GetNearPosition(optimalRange, 0.0f);

                // PHASE 5 MIGRATION: Use Movement Arbiter with QUEST priority (50)
                BotAI* botAI = dynamic_cast<BotAI*>(bot->GetAI());
                if (botAI && botAI->GetUnifiedMovementCoordinator())
                {
                    bool accepted = botAI->RequestPointMovement(
                        PlayerBotMovementPriority::QUEST,  // Priority 50 - LOW tier
                        pos,
                        "Quest objective positioning",
                        "QuestStrategy");

                    if (accepted)
                    {
                        TC_LOG_ERROR("module.playerbot.quest", "🏃 EngageQuestTargets: Bot {} moving TO optimal range (from {:.1f}yd → {:.1f}yd)",
                                     bot->GetName(), currentDistance, optimalRange);
                    }
                    else
                    {
                        TC_LOG_TRACE("playerbot.movement.arbiter",
                            "QuestStrategy: Movement rejected for bot {} - higher priority active",
                            bot->GetName());
                    }
                }
                else
                {
                    // FALLBACK: Direct MotionMaster call if arbiter not available
                    bot->GetMotionMaster()->MovePoint(0, pos);
                    TC_LOG_ERROR("module.playerbot.quest", "🏃 EngageQuestTargets: Bot {} moving TO optimal range (from {:.1f}yd → {:.1f}yd)",
                                 bot->GetName(), currentDistance, optimalRange);
                }
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} already in range ({:.1f}yd <= {:.1f}yd), ready to cast",
                             bot->GetName(), currentDistance, optimalRange);
            }
        }
        else
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚔️ EngageQuestTargets: Bot {} is MELEE class ({}), no range positioning needed",
                         bot->GetName(), bot->GetClass());
        }

        TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} set to combat state and initiated attack on {} - ClassAI will handle rotation",
                     bot->GetName(), target->GetName());
    }
    else
    {
        TC_LOG_ERROR("module.playerbot.quest", "ℹ️ EngageQuestTargets: Bot {} already in combat, letting combat AI handle target {}",
                     bot->GetName(), target->GetName());
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ EngageQuestTargets: Bot {} successfully engaged quest mob {} for quest {}",
                 bot->GetName(), target->GetName(), objective.questId);
}

void QuestStrategy::CollectQuestItems(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CollectQuestItems: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "📦 CollectQuestItems: Bot {} starting item collection for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Check if bot already has required items
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CollectQuestItems: Bot {} - Invalid quest {} or objective index {}",
                     bot->GetName(), objective.questId, objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    TC_LOG_ERROR("module.playerbot.quest", "🔍 CollectQuestItems: Bot {} - Quest {} requires item {} (Amount: {})",
                 bot->GetName(), objective.questId, questObjective.ObjectID, questObjective.Amount);

    // Check item count
    uint32 itemCount = bot->GetItemCount(questObjective.ObjectID, false);
    TC_LOG_ERROR("module.playerbot.quest", "📊 CollectQuestItems: Bot {} currently has {} / {} of item {}",
                 bot->GetName(), itemCount, questObjective.Amount, questObjective.ObjectID);

    if (itemCount >= questObjective.Amount)
    {
        // Objective complete
        TC_LOG_ERROR("module.playerbot.quest", "✅ CollectQuestItems: Bot {} completed item objective {} for quest {} ({} / {})",
                     bot->GetName(), objective.objectiveIndex, objective.questId, itemCount, questObjective.Amount);
        return;
    }

    // Find quest object to interact with
    TC_LOG_ERROR("module.playerbot.quest", "🔎 CollectQuestItems: Bot {} searching for quest object...",
                 bot->GetName());
    GameObject* questObject = FindQuestObject(ai, objective);

    if (!questObject)
    {
        // No object found - navigate to objective area
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ CollectQuestItems: Bot {} - NO quest object found, navigating to objective area",
                     bot->GetName());
        NavigateToObjective(ai, objective);
        return;
    }

    float distance = std::sqrt(bot->GetExactDistSq(questObject)); // Calculate once from squared distance
    TC_LOG_ERROR("module.playerbot.quest", "✅ CollectQuestItems: Bot {} found quest object {} at distance {:.1f}",
                 bot->GetName(), questObject->GetEntry(), distance);

    // Move to object
    if (distance > INTERACTION_DISTANCE)
    {
        Position objPos;
        objPos.Relocate(questObject->GetPositionX(), questObject->GetPositionY(), questObject->GetPositionZ());
        TC_LOG_ERROR("module.playerbot.quest", "🚶 CollectQuestItems: Bot {} moving to object {} (distance: {:.1f}, pos: {:.1f}, {:.1f}, {:.1f})",
                     bot->GetName(), questObject->GetEntry(), distance,
                     objPos.GetPositionX(), objPos.GetPositionY(), objPos.GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, objPos);
        return;
    }

    // Interact with object
    TC_LOG_ERROR("module.playerbot.quest", "🎯 CollectQuestItems: Bot {} interacting with object {} (distance: {:.1f} <= INTERACTION_DISTANCE)",
                 bot->GetName(), questObject->GetEntry(), distance);
    bot->PrepareGossipMenu(questObject, questObject->GetGOInfo()->type == GAMEOBJECT_TYPE_QUESTGIVER ? 0 : questObject->GetGOInfo()->entry);
    bot->SendPreparedGossip(questObject);

    TC_LOG_ERROR("module.playerbot.quest", "✅ CollectQuestItems: Bot {} interaction sent for object {}",
                 bot->GetName(), questObject->GetEntry());
}

void QuestStrategy::ExploreQuestArea(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    // Simply navigate to the objective area
    NavigateToObjective(ai, objective);
}

void QuestStrategy::UseQuestItemOnTarget(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UseQuestItemOnTarget: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Bot {} using quest item for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Get quest and verify it has a source item
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || quest->GetSrcItemId() == 0)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UseQuestItemOnTarget: Quest {} has NO source item!",
                     objective.questId);
        return;
    }

    uint32 questItemId = quest->GetSrcItemId();
    TC_LOG_ERROR("module.playerbot.quest", "📦 UseQuestItemOnTarget: Quest {} requires item {} to complete objective",
                 objective.questId, questItemId);

    // Check if bot has the quest item
    Item* questItem = bot->GetItemByEntry(questItemId);
    if (!questItem)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UseQuestItemOnTarget: Bot {} does NOT have quest item {}!",
                     bot->GetName(), questItemId);
        return;
    }

    // Get objective details
    if (objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UseQuestItemOnTarget: Invalid objective index {}",
                     objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // Check if objective is already complete
    uint32 currentProgress = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
    uint32 requiredAmount = static_cast<uint32>(questObjective.Amount);

    if (currentProgress >= requiredAmount)
    {
        TC_LOG_ERROR("module.playerbot.quest", "✅ UseQuestItemOnTarget: Objective COMPLETE ({} / {}), nothing to do",
                     currentProgress, requiredAmount);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "📊 UseQuestItemOnTarget: Progress: {} / {} - need to use item {} more times",
                 currentProgress, requiredAmount, requiredAmount - currentProgress);

    // Find the target GameObject (e.g., fire for Quest 26391)
    uint32 targetObjectId = questObjective.ObjectID;

    // Scan for target GameObject in 200-yard radius (same as FindQuestObject)
    std::vector<uint32> objects = (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->ScanForGameObjects( targetObjectId, 200.0f) : std::vector<uint32>());

    TC_LOG_ERROR("module.playerbot.quest", "🔍 UseQuestItemOnTarget: Scanning for GameObject {} - found {} objects",
                 targetObjectId, objects.size());

    if (objects.empty())
    {
        // No target found - navigate to objective area
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: NO target GameObject {} found, navigating to objective area",
                     targetObjectId);
        NavigateToObjective(ai, objective);
        return;
    }

    // DEADLOCK FIX: Use spatial grid snapshots instead of ObjectAccessor in loop
    Map* map = bot->GetMap();
    if (!map)
        return;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
        return;

    // Query nearby GameObjects (lock-free!)
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), 200.0f);

    // Find the nearest valid GameObject matching target entry
    ObjectGuid targetGuid;
    float nearestDistance = std::numeric_limits<float>::max();
    uint32 validObjectsFound = 0;

    for (auto const& snapshot : nearbyObjects)
    {
        // Filter by entry ID
        if (snapshot.entry != targetObjectId)
            continue;

        // Check if GameObject is spawned and ready using snapshot fields
        if (!snapshot.isSpawned || snapshot.goState != GO_STATE_READY)
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: GameObject entry={} is NOT ready (spawned={}, state={}), skipping",
                         snapshot.entry, snapshot.isSpawned, snapshot.goState);
            continue;
        }

        validObjectsFound++;

        // Find nearest valid GameObject using snapshot position
        float distance = bot->GetExactDist(snapshot.position);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            targetGuid = snapshot.guid;
        }
    }

    // PHASE 5D: Thread-safe spatial grid validation
    auto snapshot = SpatialGridQueryHelpers::FindGameObjectByGuid(bot, targetGuid);
    GameObject* targetObject = nullptr;

    if (snapshot)
    {
        // Get GameObject* for quest item use (validated via snapshot first)
    }

    if (!targetObject)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UseQuestItemOnTarget: No valid (spawned & ready) GameObject {} found (scanned {} nearby objects, {} were valid but GUID resolution failed)",
                     targetObjectId, nearbyObjects.size(), validObjectsFound);

        // All gameobjects might be despawned/used - navigate to objective area to wait for respawn
        NavigateToObjective(ai, objective);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ UseQuestItemOnTarget: Found nearest valid GameObject {} at ({:.1f}, {:.1f}, {:.1f}) - distance {:.1f}yd ({} valid nearby)",
                 targetObject->GetEntry(),
                 targetObject->GetPositionX(), targetObject->GetPositionY(), targetObject->GetPositionZ(),
                 nearestDistance, validObjectsFound);

    // CRITICAL: Calculate SAFE distance from target based on GameObject type and damage radius
    // We need to detect if the GameObject causes damage and how large the damage area is

    GameObjectTemplate const* goInfo = targetObject->GetGOInfo();
    float damageRadius = 0.0f;
    bool causesDamage = false;

    // Detect damage-causing GameObjects and their radius
    if (goInfo)
    {
        switch (goInfo->type)
        {
            case GAMEOBJECT_TYPE_TRAP: // Type 6 - Traps (like "Bonfire Damage")
            {
                // For traps: data1 is lockId, data2 is radius, data3 is spellId
                damageRadius = static_cast<float>(goInfo->trap.radius);
                causesDamage = (goInfo->trap.spell != 0); // Has damage spell

                TC_LOG_ERROR("module.playerbot.quest", "🔥 UseQuestItemOnTarget: GameObject is TRAP type - radius={:.1f}yd, spellId={}, causes damage={}",
                             damageRadius, goInfo->trap.spell, causesDamage);
                break;
            }
            case GAMEOBJECT_TYPE_SPELL_FOCUS: // Type 8 - Spell Focus (like "Fire", "Campfire")
            {
                // For spell focus: data0 is spellFocusType, data1 is radius, data2 is linkedTrap
                damageRadius = static_cast<float>(goInfo->spellFocus.radius);

                // Check if linked to a trap GameObject (indicates it causes damage)
                if (goInfo->spellFocus.linkedTrap != 0)
                {
                    causesDamage = true;
                    TC_LOG_ERROR("module.playerbot.quest", "🔥 UseQuestItemOnTarget: GameObject is SPELL_FOCUS with LINKED TRAP {} - radius={:.1f}yd",
                                 goInfo->spellFocus.linkedTrap, damageRadius);
                }
                else
                {
                    TC_LOG_ERROR("module.playerbot.quest", "✅ UseQuestItemOnTarget: GameObject is SPELL_FOCUS without trap - radius={:.1f}yd, safe",
                                 damageRadius);
                }
                break;
            }
            case GAMEOBJECT_TYPE_AURA_GENERATOR: // Type 30 - Aura Generator
            {
                // For aura generator: data1 is radius, data2/data4 are auraID1/auraID2
                damageRadius = static_cast<float>(goInfo->auraGenerator.radius);
                causesDamage = (goInfo->auraGenerator.auraID1 != 0 || goInfo->auraGenerator.auraID2 != 0);

                TC_LOG_ERROR("module.playerbot.quest", "🔮 UseQuestItemOnTarget: GameObject is AURA_GENERATOR - radius={:.1f}yd, auraID1={}, auraID2={}, causes damage={}",
                             damageRadius, goInfo->auraGenerator.auraID1, goInfo->auraGenerator.auraID2, causesDamage);
                break;
            }
            default:
            {
                TC_LOG_ERROR("module.playerbot.quest", "ℹ️ UseQuestItemOnTarget: GameObject type {} - assuming safe, no damage detection",
                             goInfo->type);
                break;
            }
        }
    }

    // Calculate safe positioning
    float gameObjectScale = targetObject->GetObjectScale();
    float baseRadius = damageRadius > 0.0f ? damageRadius : (gameObjectScale * 2.0f);
    // Safe distance calculation:
    // - If causes damage: damage radius + 3 yards safety buffer + item use margin
    // - If no damage: just use reasonable item casting distance
    float safeDistance;
    if (causesDamage && damageRadius > 0.0f)
    {
        // Stay OUTSIDE damage radius with safety buffer
        safeDistance = damageRadius + 3.0f; // 3 yard safety buffer outside damage zone
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: GameObject CAUSES DAMAGE - staying {:.1f}yd outside damage radius ({:.1f}yd + 3yd safety)",
                     safeDistance, damageRadius);
    }
    else
    {
        // No damage - can get closer
        safeDistance = baseRadius + 5.0f; // Standard item use distance
        TC_LOG_ERROR("module.playerbot.quest", "✅ UseQuestItemOnTarget: GameObject SAFE - using standard distance {:.1f}yd",
                     safeDistance);
    }

    float minSafeDistance = safeDistance;
    float maxUseDistance = 30.0f; // Maximum range for item usage

    TC_LOG_ERROR("module.playerbot.quest", "📏 UseQuestItemOnTarget: GameObject scale={:.1f}, damage radius={:.1f}yd, safe distance={:.1f}yd",
                 gameObjectScale, damageRadius, safeDistance);

    // Calculate bot's current distance to target
    float currentDistance = std::sqrt(bot->GetExactDistSq(targetObject)); // Calculate once from squared distance

    TC_LOG_ERROR("module.playerbot.quest", "📏 UseQuestItemOnTarget: Bot distance to target={:.1f}yd (safe range: {:.1f}-{:.1f}yd)",
                 currentDistance, minSafeDistance, maxUseDistance);

    // Check if bot is in safe range
    if (currentDistance < minSafeDistance)
    {
        // TOO CLOSE - move away to safe distance
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: Bot {} TOO CLOSE to target ({:.1f}yd < {:.1f}yd safe distance) - MOVING AWAY",
                     bot->GetName(), currentDistance, minSafeDistance);

        // Calculate position AWAY from target at safe distance
        // Get angle from target to bot
        float angleToBot = targetObject->GetRelativeAngle(bot);
        // Create position at safe distance in that direction
        Position safePos;
        targetObject->GetNearPoint(bot, safePos.m_positionX, safePos.m_positionY, safePos.m_positionZ,
                                   safeDistance, angleToBot);

        TC_LOG_ERROR("module.playerbot.quest", "🏃 UseQuestItemOnTarget: Moving AWAY to safe position ({:.1f}, {:.1f}, {:.1f}) at distance {:.1f}yd",
                     safePos.GetPositionX(), safePos.GetPositionY(), safePos.GetPositionZ(), safeDistance);

        BotMovementUtil::MoveToPosition(bot, safePos);
        return;
    }
    else if (currentDistance > maxUseDistance)
    {
        // TOO FAR - move closer to safe distance
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ UseQuestItemOnTarget: Bot {} TOO FAR from target ({:.1f}yd > {:.1f}yd max use distance) - MOVING CLOSER",
                     bot->GetName(), currentDistance, maxUseDistance);

        // Calculate position TOWARD target at safe distance
        float angleFromBot = bot->GetRelativeAngle(targetObject);

        Position closePos;
        bot->GetNearPoint(bot, closePos.m_positionX, closePos.m_positionY, closePos.m_positionZ,
                          safeDistance, angleFromBot);

        TC_LOG_ERROR("module.playerbot.quest", "🏃 UseQuestItemOnTarget: Moving CLOSER to safe position ({:.1f}, {:.1f}, {:.1f}) at distance {:.1f}yd",
                     closePos.GetPositionX(), closePos.GetPositionY(), closePos.GetPositionZ(), safeDistance);

        BotMovementUtil::MoveToPosition(bot, closePos);
        return;
    }

    // FIRE DAMAGE EVASION: Check bot health and retreat if taking damage
    float healthPercent = (bot->GetHealth() * 100.0f) / bot->GetMaxHealth();
    if (healthPercent < 70.0f)
    {
        // Bot is taking significant damage - move away to safer distance
        TC_LOG_ERROR("module.playerbot.quest", "🔥 UseQuestItemOnTarget: Bot {} health LOW ({:.1f}%) - RETREATING from fire damage!",
                     bot->GetName(), healthPercent);

        // Move further away (increase safe distance by 5 yards)
        float retreatDistance = safeDistance + 5.0f;
        float angleToBot = targetObject->GetRelativeAngle(bot);
        Position retreatPos;
        targetObject->GetNearPoint(bot, retreatPos.m_positionX, retreatPos.m_positionY, retreatPos.m_positionZ,
                                   retreatDistance, angleToBot);

        TC_LOG_ERROR("module.playerbot.quest", "🏃 UseQuestItemOnTarget: RETREATING to safer position ({:.1f}, {:.1f}, {:.1f}) at distance {:.1f}yd",
                     retreatPos.GetPositionX(), retreatPos.GetPositionY(), retreatPos.GetPositionZ(), retreatDistance);

        BotMovementUtil::MoveToPosition(bot, retreatPos);
        return;
    }

    // Bot is in safe range and healthy - stop, face target, and use item
    TC_LOG_ERROR("module.playerbot.quest", "✅ UseQuestItemOnTarget: Bot {} in safe range ({:.1f}yd), health {:.1f}%, preparing to use item {}",
                 bot->GetName(), currentDistance, healthPercent, questItemId);

    // CRITICAL: Stop all movement before using the item (required for spell casting)
    bot->StopMoving();
    bot->GetMotionMaster()->Clear();
    bot->GetMotionMaster()->MoveIdle();
    TC_LOG_ERROR("module.playerbot.quest", "🛑 UseQuestItemOnTarget: Bot {} stopped moving",
                 bot->GetName());

    // Face the target GameObject
    bot->SetFacingToObject(targetObject);

    TC_LOG_ERROR("module.playerbot.quest", "👁️ UseQuestItemOnTarget: Bot {} now facing target GameObject {}",
                 bot->GetName(), targetObject->GetEntry());
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
        TC_LOG_ERROR("module.playerbot.quest", "❌ UseQuestItemOnTarget: Quest item {} has no ON_USE spell!",
                     questItemId);
        return;
    }
    TC_LOG_ERROR("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Quest item {} triggers spell {}",
                 questItemId, spellId);

    // Get spell info for validation
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);

    TC_LOG_ERROR("module.playerbot.quest", "🎯 UseQuestItemOnTarget: Casting spell {} on GameObject {} (entry {})",
                 spellId, targetObject->GetGUID().ToString(), targetObject->GetEntry());

    // Cast the spell with the item as the source and GameObject as target
    // Use CastSpellExtraArgs to pass the item that's being used
    CastSpellExtraArgs args;
    args.SetCastItem(questItem);
    args.SetOriginalCaster(bot->GetGUID());
    bot->CastSpell(targetObject, spellId, args);
    TC_LOG_ERROR("module.playerbot.quest", "✅ UseQuestItemOnTarget: Bot {} cast spell {} from item {} on GameObject {} - objective should progress",
                 bot->GetName(), spellId, questItemId, targetObject->GetEntry());
}
void QuestStrategy::TurnInQuest(BotAI* ai, uint32 questId)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    TC_LOG_ERROR("module.playerbot.quest", "🎯 TurnInQuest: Bot {} attempting to turn in quest {} ({})",
                 bot->GetName(), questId, quest->GetLogTitle());

    // Step 1: Find quest ender location (multi-tier fallback)
    QuestEnderLocation location;
    if (!FindQuestEnderLocation(ai, questId, location))
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to find quest ender location for quest {}",
                     bot->GetName(), questId);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ TurnInQuest: Bot {} found quest ender NPC {} at ({:.1f}, {:.1f}, {:.1f}) - foundViaSpawn={}, foundViaPOI={}, requiresSearch={}",
                 bot->GetName(), location.npcEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 location.foundViaSpawn, location.foundViaPOI, location.requiresSearch);

    // Step 2: Check if NPC is already in range
    if (CheckForQuestEnderInRange(ai, location.npcEntry))
    {
        // NPC is in range - complete turn-in immediately
        TC_LOG_ERROR("module.playerbot.quest", "✅ TurnInQuest: Bot {} found quest ender NPC {} in range, completing turn-in immediately",
                     bot->GetName(), location.npcEntry);
        return; // CheckForQuestEnderInRange() handles the turn-in
    }

    // Step 3: Navigate to quest ender location
    if (!NavigateToQuestEnder(ai, location))
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to navigate to quest ender NPC {}",
                     bot->GetName(), location.npcEntry);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "🚶 TurnInQuest: Bot {} navigating to quest ender NPC {} at ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(), location.npcEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());

    // Navigation is in progress - next UpdateBehavior() cycle will check for NPC in range
}

ObjectiveTracker::ObjectivePriority QuestStrategy::GetCurrentObjective(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return ObjectiveTracker::ObjectivePriority(0, 0);

    Player* bot = ai->GetBot();
    return (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->GetHighestPriorityObjective() : ObjectiveTracker::ObjectivePriority());
}

bool QuestStrategy::HasActiveObjectives(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    ObjectiveTracker::ObjectivePriority priority = GetCurrentObjective(ai);
    return priority.questId != 0;
}

bool QuestStrategy::ShouldEngageTarget(BotAI* ai, ::Unit* target, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot() || !target)
        return false;

    Player* bot = ai->GetBot();

    // Check if target is quest target
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return false;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_MONSTER)
        return false;

    if (target->GetEntry() != questObjective.ObjectID)
        return false;

    // Check if already at max kills
    uint32 currentKills = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
    if (currentKills >= static_cast<uint32>(questObjective.Amount))
        return false;

    return true;
}

bool QuestStrategy::MoveToObjectiveLocation(BotAI* ai, Position const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Check if already at location
    float distance = bot->GetExactDist2d(location.GetPositionX(), location.GetPositionY());
    if (distance < 10.0f) // Within 10 yards
        return true;

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

Position QuestStrategy::GetObjectivePosition(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
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
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ GetObjectivePosition: Bot {} - Cached position ({:.1f}, {:.1f}, {:.1f}) is INVALID (at origin or bot position)",
                     bot->GetName(),
                     cachedPos.GetPositionX(), cachedPos.GetPositionY(), cachedPos.GetPositionZ());

        // Re-query position with QuestPOI fallback
        Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
        if (quest && objective.objectiveIndex < quest->Objectives.size())
        {
            QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

            TC_LOG_ERROR("module.playerbot.quest", "🔄 GetObjectivePosition: Re-querying objective location with QuestPOI fallback...");

            QuestObjectiveData objData(objective.questId, objective.objectiveIndex,
                                      static_cast<QuestObjectiveType>(questObjective.Type),
                                      questObjective.ObjectID, questObjective.Amount);

            Position newPos = (GetGameSystems(bot) ? GetGameSystems(bot)->GetObjectiveTracker()->FindObjectiveTargetLocation(objData) : Position());
            // Check if we got a valid position
            if (newPos.GetExactDist2d(0.0f, 0.0f) > 0.1f)
            {
                TC_LOG_ERROR("module.playerbot.quest", "✅ GetObjectivePosition: Found NEW position ({:.1f}, {:.1f}, {:.1f}) via re-query",
                             newPos.GetPositionX(), newPos.GetPositionY(), newPos.GetPositionZ());
                return newPos;
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.quest", "❌ GetObjectivePosition: Re-query FAILED, returning cached position anyway");
            }
        }
    }

    // Cached position is valid - return it
    TC_LOG_ERROR("module.playerbot.quest", "✅ GetObjectivePosition: Bot {} using cached spawn position ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(),
                 cachedPos.GetPositionX(), cachedPos.GetPositionY(), cachedPos.GetPositionZ());

    return cachedPos;
}

::Unit* QuestStrategy::FindQuestTarget(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_MONSTER)
        return nullptr;

    // DEADLOCK FIX: Use spatial grid instead of ObjectAccessor in loops
    Map* map = bot->GetMap();
    if (!map)
        return nullptr;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
        return nullptr;

    // Query nearby creatures (lock-free!)
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 300.0f);

    // Find first matching creature by entry
    ObjectGuid targetGuid;
    for (auto const& snapshot : nearbyCreatures)
    {
        if (snapshot.entry == questObjective.ObjectID && !snapshot.isDead)
        {
            targetGuid = snapshot.guid;
            TC_LOG_ERROR("module.playerbot.quest", "✅ FindQuestTarget: Found creature entry {} at ({:.1f}, {:.1f}, {:.1f})",
                         snapshot.entry, snapshot.position.GetPositionX(),
                         snapshot.position.GetPositionY(), snapshot.position.GetPositionZ());
            break;
        }
    }

    if (targetGuid.IsEmpty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ FindQuestTarget: Bot {} - NO targets found in 300-yard scan for entry {}",
                     bot->GetName(), questObjective.ObjectID);

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
        uint32 entry = questObjective.ObjectID;

        // If creature is not hostile, check if it requires spell click interaction
        if (!bot->IsHostileTo(creature))
        {
            // Check if this NPC has spell click data
            if (RequiresSpellClickInteraction(entry))
            {
                TC_LOG_ERROR("module.playerbot.quest", "⚠️ FindQuestTarget: NPC {} (Entry: {}) requires SPELL CLICK interaction, not attack! Returning nullptr for TALKTO logic.",
                             creature->GetName(), entry);
                return nullptr;  // Return nullptr so bot uses TALKTO logic in EngageQuestTargets()
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.quest", "✅ FindQuestTarget: Mob {} (Entry: {}) is NEUTRAL but ATTACKABLE (no spell click data), will be attacked!",
                             creature->GetName(), entry);
                // Fall through - return this target for attack even though it's neutral
            }
        }
    }

    return target;
}

GameObject* QuestStrategy::FindQuestObject(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
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

    TC_LOG_ERROR("module.playerbot.quest", "🔍 FindQuestObject: Bot {} scanning for GameObject entry {} within 200 yards - found {} nearby objects",
                 bot->GetName(), questObjective.ObjectID, nearbyObjects.size());

    // Find first matching GameObject by entry
    ObjectGuid objectGuid;
    for (auto const& snapshot : nearbyObjects)
    {
        if (snapshot.entry == questObjective.ObjectID && snapshot.isSpawned)
        {
            objectGuid = snapshot.guid;
            TC_LOG_ERROR("module.playerbot.quest", "✅ FindQuestObject: Found GameObject entry {} at ({:.1f}, {:.1f}, {:.1f})",
                         snapshot.entry, snapshot.position.GetPositionX(),
                         snapshot.position.GetPositionY(), snapshot.position.GetPositionZ());
            break;
        }
    }
    if (objectGuid.IsEmpty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ FindQuestObject: Bot {} - NO GameObjects found in 200-yard scan for entry {}",
                     bot->GetName(), questObjective.ObjectID);
        return nullptr;
    }
    // PHASE 5D: Thread-safe spatial grid validation
    auto snapshot = SpatialGridQueryHelpers::FindGameObjectByGuid(bot, objectGuid);
    GameObject* gameObject = nullptr;

    if (snapshot)
    {
        // Get GameObject* for quest object interaction (validated via snapshot first)
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ FindQuestObject: Bot {} found GameObject {} (Entry: {}) at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                 bot->GetName(), gameObject->GetName(), questObjective.ObjectID,
                 gameObject->GetPositionX(), gameObject->GetPositionY(), gameObject->GetPositionZ(),
                 std::sqrt(bot->GetExactDistSq(gameObject)));

    return gameObject;
}

::Item* QuestStrategy::FindQuestItem(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
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
        TC_LOG_ERROR("module.playerbot.quest", "❌ SearchForQuestGivers: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🔍 SearchForQuestGivers: ENTRY for bot {}", bot->GetName());

    // Initialize QuestAcceptanceManager if not already done
    if (!_acceptanceManager)
    {
        _acceptanceManager = std::make_unique<QuestAcceptanceManager>(bot);
        TC_LOG_ERROR("module.playerbot.quest",
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

    TC_LOG_ERROR("module.playerbot.quest", "⏰ SearchForQuestGivers: Bot {} - failures={}, backoffDelay={}ms, timeSinceLastSearch={}ms",
                 bot->GetName(), _questGiverSearchFailures, backoffDelay, currentTime - _lastQuestGiverSearchTime);

    // Check if we're still in cooldown period
    if (currentTime - _lastQuestGiverSearchTime < backoffDelay)
    {
        TC_LOG_ERROR("module.playerbot.quest", "⏸️ SearchForQuestGivers: Bot {} still in cooldown ({} ms remaining), skipping search",
                     bot->GetName(), backoffDelay - (currentTime - _lastQuestGiverSearchTime));
        // Still in cooldown - don't search yet (prevents log spam)
        return;
    }

    // Update last search time
    _lastQuestGiverSearchTime = currentTime;

    TC_LOG_ERROR("module.playerbot.quest", "🔎 SearchForQuestGivers: Bot {} starting quest giver scan (50 yard radius)",
                 bot->GetName());

    TC_LOG_DEBUG("module.playerbot.strategy",
        "QuestStrategy: Bot {} (Level {}) searching for quest givers (no active quests)",
        bot->GetName(), bot->GetLevel());

    // Search for nearby creatures that might offer quests
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, 0, 50.0f); // 50 yard radius

    TC_LOG_ERROR("module.playerbot.quest", "🔬 SearchForQuestGivers: Bot {} found {} nearby creatures",
                 bot->GetName(), nearbyCreatures.size());

    Creature* closestQuestGiver = nullptr;
    float closestDistance = 999999.0f;
    uint32 questGiverCount = 0;
    uint32 questGiversWithEligibleQuests = 0;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        // CRITICAL: Check if bot can actually see this creature (phase check)
        if (!bot->CanSeeOrDetect(creature))
        {
            TC_LOG_ERROR("module.playerbot.quest", "👻 Creature {} (Entry: {}) is in different phase, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        TC_LOG_ERROR("module.playerbot.quest", "🔬 Checking creature: {} (Entry: {}), IsQuestGiver={}",
                     creature->GetName(), creature->GetEntry(), creature->IsQuestGiver());

        // Check if creature is a quest giver
        if (!creature->IsQuestGiver())
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ Creature {} (Entry: {}) is NOT a quest giver, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }
        questGiverCount++;

        // CRITICAL FIX: Check if this quest giver has any eligible quests for the bot
        // Don't waste time moving to NPCs that have no quests we can accept!
        QuestRelationResult objectQR = sObjectMgr->GetCreatureQuestRelations(creature->GetEntry());
        bool hasEligibleQuests = false;

        for (uint32 questId : objectQR)
        {
            Quest const* questTemplate = sObjectMgr->GetQuestTemplate(questId);
            if (!questTemplate)
                continue;

            // Check if quest is eligible using QuestAcceptanceManager
            if (_acceptanceManager->IsQuestEligible(questTemplate))
            {
                hasEligibleQuests = true;
                TC_LOG_ERROR("module.playerbot.quest", "✅ Quest giver {} (Entry: {}) has eligible quest {} ({})",
                             creature->GetName(), creature->GetEntry(), questId, questTemplate->GetLogTitle());
                break; // Found at least one eligible quest, this NPC is valid
            }
        }

        if (!hasEligibleQuests)
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ Quest giver {} (Entry: {}) has NO eligible quests for bot {}, skipping",
                         creature->GetName(), creature->GetEntry(), bot->GetName());
            continue;
        }

        questGiversWithEligibleQuests++;

        // Find the closest quest giver WITH ELIGIBLE QUESTS
        float distance = std::sqrt(bot->GetExactDistSq(creature)); // Calculate once from squared distance
        TC_LOG_ERROR("module.playerbot.quest", "✅ Found quest giver WITH ELIGIBLE QUESTS: {} (Entry: {}) at distance {:.1f}",
                     creature->GetName(), creature->GetEntry(), distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestQuestGiver = creature;
        }
    }

    TC_LOG_ERROR("module.playerbot.quest", "📊 SearchForQuestGivers: Bot {} found {} quest givers ({} with eligible quests) - closest with eligible quests: {} at {:.1f} yards",
                 bot->GetName(), questGiverCount, questGiversWithEligibleQuests,
                 closestQuestGiver ? closestQuestGiver->GetName() : "NONE",
                 closestQuestGiver ? closestDistance : 0.0f);

    if (!closestQuestGiver)
    {
        // Increment failure counter for exponential backoff
        _questGiverSearchFailures++;

        TC_LOG_ERROR("module.playerbot.quest",
            "❌ SearchForQuestGivers: Bot {} found no quest givers within 50 yards (failures: {}, next search in {}s)",
            bot->GetName(), _questGiverSearchFailures,
            std::min(30u, 5u * (1u << (_questGiverSearchFailures - 1))));

        // PATHFINDING TO QUEST HUBS: Navigate to appropriate quest hub for bot's level
        TC_LOG_ERROR("module.playerbot.quest",
            "🗺️ SearchForQuestGivers: Bot {} has no nearby quest givers - searching quest hub database for appropriate quest hubs",
            bot->GetName());

        // Get quest hubs appropriate for this bot's level and faction
        auto& hubDb = QuestHubDatabase::Instance();
        if (!hubDb.IsInitialized())
        {
            TC_LOG_ERROR("module.playerbot.quest",
                "⚠️ SearchForQuestGivers: QuestHubDatabase not initialized, cannot navigate to quest hubs");
            return;
        }

        auto questHubs = hubDb.GetQuestHubsForPlayer(bot, 3); // Get top 3 suitable hubs
        if (questHubs.empty())
        {
            TC_LOG_ERROR("module.playerbot.quest",
                "⚠️ SearchForQuestGivers: Bot {} - No appropriate quest hubs found for level {} (zone {}, faction {})",
                bot->GetName(), bot->GetLevel(), bot->GetZoneId(), bot->GetTeamId());
            return;
        }
        // Get nearest quest hub
        QuestHub const* nearestHub = questHubs[0]; // Already sorted by suitability (includes distance)

        TC_LOG_ERROR("module.playerbot.quest",
            "✅ SearchForQuestGivers: Bot {} found quest hub '{}' at distance {:.1f} yards (Level range: {}-{}, {} quests available)",
            bot->GetName(), nearestHub->name, nearestHub->GetDistanceFrom(bot),
            nearestHub->minLevel, nearestHub->maxLevel, nearestHub->questIds.size());
        // Check if hub is already within range
        float hubDistance = nearestHub->GetDistanceFrom(bot);
        if (hubDistance < 10.0f)
        {
            TC_LOG_ERROR("module.playerbot.quest",
                "⚠️ SearchForQuestGivers: Bot {} already at quest hub '{}' but no quest givers found - may be phasing issue",
                bot->GetName(), nearestHub->name);
            return;
        }

        // Navigate to quest hub using existing PathfindingAdapter
        TC_LOG_ERROR("module.playerbot.quest",
            "🚶 SearchForQuestGivers: Bot {} navigating to quest hub '{}' at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f} yards",
            bot->GetName(), nearestHub->name,
            nearestHub->location.GetPositionX(), nearestHub->location.GetPositionY(), nearestHub->location.GetPositionZ(),
            hubDistance);

        // Use BotMovementUtil for navigation (integrates with existing pathfinding)
        bool moveResult = BotMovementUtil::MoveToPosition(bot, nearestHub->location);
        if (moveResult)
        {
            TC_LOG_ERROR("module.playerbot.quest",
                "✅ SearchForQuestGivers: Bot {} successfully started pathfinding to quest hub '{}' ({} quest givers expected)",
                bot->GetName(), nearestHub->name, nearestHub->creatureIds.size());
        }
        else
        {
            TC_LOG_ERROR("module.playerbot.quest",
                "❌ SearchForQuestGivers: Bot {} failed to start pathfinding to quest hub '{}'",
                bot->GetName(), nearestHub->name);
        }

        return;
    }

    // SUCCESS: Found a quest giver - reset failure counter
    _questGiverSearchFailures = 0;

    TC_LOG_ERROR("module.playerbot.quest",
        "✅ SearchForQuestGivers: Bot {} found quest giver {} at distance {:.1f}",
        bot->GetName(), closestQuestGiver->GetName(), closestDistance);

    // Move to quest giver
    if (closestDistance > INTERACTION_DISTANCE)
    {
        Position questGiverPos;
        questGiverPos.Relocate(closestQuestGiver->GetPositionX(),
                              closestQuestGiver->GetPositionY(),
                              closestQuestGiver->GetPositionZ());

        TC_LOG_ERROR("module.playerbot.quest",
            "🚶 SearchForQuestGivers: Bot {} moving to quest giver {} (distance: {:.1f}, pos: {:.1f}, {:.1f}, {:.1f})",
            bot->GetName(), closestQuestGiver->GetName(), closestDistance,
            questGiverPos.GetPositionX(), questGiverPos.GetPositionY(), questGiverPos.GetPositionZ());

        bool moveResult = BotMovementUtil::MoveToPosition(bot, questGiverPos);
        TC_LOG_ERROR("module.playerbot.quest",
            "🚶 SearchForQuestGivers: Bot {} MoveToPosition result: {}",
            bot->GetName(), moveResult ? "SUCCESS" : "FAILED");
        return;
    }

    // Bot is close enough - use ENTERPRISE-GRADE quest acceptance system
    TC_LOG_ERROR("module.playerbot.quest",
        "🎯 SearchForQuestGivers: Bot {} at quest giver {} (distance {:.1f} <= INTERACTION_DISTANCE) - Processing with QuestAcceptanceManager",
        bot->GetName(), closestQuestGiver->GetName(), closestDistance);

    // Let the enterprise-grade manager handle eligibility, scoring, and acceptance
    _acceptanceManager->ProcessQuestGiver(closestQuestGiver);

    TC_LOG_ERROR("module.playerbot.quest",
        "🏆 SearchForQuestGivers: Bot {} quest acceptance complete (Total accepted: {}, Dropped: {})",
        bot->GetName(),
        _acceptanceManager->GetQuestsAccepted(),
        _acceptanceManager->GetQuestsDropped());
}

// ========================================================================
// QUEST TURN-IN SYSTEM - Multi-Tier Fallback Implementation
// ========================================================================

bool QuestStrategy::FindQuestEnderLocation(BotAI* ai, uint32 questId, QuestEnderLocation& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🔍 FindQuestEnderLocation: Bot {} searching for quest ender for quest {}",
                 bot->GetName(), questId);

    // Get quest ender NPC entry from quest relations
    auto questEnders = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questId);

    if (questEnders.begin() == questEnders.end())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ FindQuestEnderLocation: No quest ender found in creature_questender table for quest {}",
                     questId);
        return false;
    }

    // Get the first quest ender NPC entry
    uint32 questEnderEntry = questEnders.begin()->second;
    location.npcEntry = questEnderEntry;

    TC_LOG_ERROR("module.playerbot.quest", "✅ FindQuestEnderLocation: Quest {} ender is NPC entry {}",
                 questId, questEnderEntry);

    // ========================================================================
    // TIER 1: Creature Spawn Data (PREFERRED - Most Reliable)
    // ========================================================================
    TC_LOG_ERROR("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 1 - Searching creature spawn data for NPC {}",
                 questEnderEntry);

    // Get all spawn data for this creature
    auto const& spawnData = sObjectMgr->GetAllCreatureData();

    // Find the closest spawn of this creature to the bot
    float closestDistance = 999999.0f;
    CreatureData const* closestSpawn = nullptr;

    for (auto const& pair : spawnData)
    {
        CreatureData const& data = pair.second;

        // Check if this spawn is the quest ender we're looking for
        if (data.id != questEnderEntry)
            continue;

        // Check if spawn is on the same map as bot
        if (data.mapId != bot->GetMapId())
            continue;

        // Calculate distance from bot to this spawn
        float distance = bot->GetExactDist2d(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());

        TC_LOG_ERROR("module.playerbot.quest", "📍 FindQuestEnderLocation: Found spawn of NPC {} at ({:.1f}, {:.1f}, {:.1f}) on map {}, distance={:.1f}",
                     questEnderEntry,
                     data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(),
                     data.mapId, distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestSpawn = &data;
        }
    }

    if (closestSpawn)
    {
        // SUCCESS - Found via creature spawn data
        location.position.Relocate(
            closestSpawn->spawnPoint.GetPositionX(),
            closestSpawn->spawnPoint.GetPositionY(),
            closestSpawn->spawnPoint.GetPositionZ()
        );
        location.foundViaSpawn = true;

        TC_LOG_ERROR("module.playerbot.quest", "✅ TIER 1 SUCCESS: Found quest ender NPC {} via spawn data at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                     questEnderEntry,
                     location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                     closestDistance);
        return true;
    }

    TC_LOG_ERROR("module.playerbot.quest", "⚠️ TIER 1 FAILED: No spawn data found for NPC {}, falling back to TIER 2 (Quest POI)",
                 questEnderEntry);

    // ========================================================================
    // TIER 2: Quest POI Data (FALLBACK - Scripted/Event NPCs)
    // ========================================================================
    TC_LOG_ERROR("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 2 - Searching Quest POI data for quest {}",
                 questId);

    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(questId);

    if (!poiData || poiData->Blobs.empty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ TIER 2 FAILED: No Quest POI data found for quest {}, falling back to TIER 3 (Area Search)",
                     questId);

        // TIER 3: Area search required
        location.requiresSearch = true;

        TC_LOG_ERROR("module.playerbot.quest", "⚠️ FindQuestEnderLocation: All automated methods failed - bot will need to search 50-yard radius for NPC {}",
                     questEnderEntry);

        // Return true with requiresSearch flag - bot will search area during navigation
        return true;
    }

    // Extract coordinates from POI blobs
    // Find the blob on the same map as bot
    QuestPOIBlobData const* validBlob = nullptr;

    for (auto const& blob : poiData->Blobs)
    {
        if (blob.MapID == static_cast<int32>(bot->GetMapId()))
        {
            validBlob = &blob;
            break;
        }
    }

    if (!validBlob || validBlob->Points.empty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ TIER 2 FAILED: Quest POI data exists but no valid points on map {}, falling back to TIER 3",
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

    TC_LOG_ERROR("module.playerbot.quest", "✅ TIER 2 SUCCESS: Found quest POI coordinates at ({:.1f}, {:.1f}, {:.1f}) for quest {}",
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 questId);

    return true;
}

bool QuestStrategy::NavigateToQuestEnder(BotAI* ai, QuestEnderLocation const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Calculate distance to destination
    float distance = bot->GetExactDist2d(location.position.GetPositionX(), location.position.GetPositionY());

    TC_LOG_ERROR("module.playerbot.quest", "🚶 NavigateToQuestEnder: Bot {} navigating to NPC {} at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                 bot->GetName(), location.npcEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 distance);

    // Check if already at destination
    if (distance < 10.0f)
    {
        TC_LOG_ERROR("module.playerbot.quest", "✅ NavigateToQuestEnder: Bot {} already at destination (distance={:.1f} < 10.0), checking for NPC in range",
                     bot->GetName(), distance);

        // Check for NPC in range
        return CheckForQuestEnderInRange(ai, location.npcEntry);
    }

    // Start navigation
    bool moveResult = BotMovementUtil::MoveToPosition(bot, location.position);
    if (!moveResult)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ NavigateToQuestEnder: Bot {} failed to start pathfinding to ({:.1f}, {:.1f}, {:.1f})",
                     bot->GetName(),
                     location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());
        return false;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ NavigateToQuestEnder: Bot {} pathfinding started to NPC {} (distance={:.1f})",
                 bot->GetName(), location.npcEntry, distance);

    return true;
}

bool QuestStrategy::CheckForQuestEnderInRange(BotAI* ai, uint32 npcEntry)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    TC_LOG_ERROR("module.playerbot.quest", "🔎 CheckForQuestEnderInRange: Bot {} scanning 50-yard radius for NPC entry {}",
                 bot->GetName(), npcEntry);

    // Scan for quest ender NPC in 50-yard radius
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, npcEntry, 50.0f);

    TC_LOG_ERROR("module.playerbot.quest", "📊 CheckForQuestEnderInRange: Bot {} found {} creatures with entry {} in 50-yard radius",
                 bot->GetName(), nearbyCreatures.size(), npcEntry);

    if (nearbyCreatures.empty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CheckForQuestEnderInRange: Bot {} found NO quest ender NPC {} in range",
                     bot->GetName(), npcEntry);
        return false;
    }

    // Find the closest valid quest ender
    Creature* closestQuestEnder = nullptr;
    float closestDistance = 999999.0f;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        // CRITICAL: Phase validation to prevent Ilario-type issues
        if (!bot->CanSeeOrDetect(creature))
        {
            TC_LOG_ERROR("module.playerbot.quest", "👻 CheckForQuestEnderInRange: NPC {} (Entry: {}) is in different phase, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        // Verify it's a quest giver (quest enders are also quest givers)
        if (!creature->IsQuestGiver())
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ CheckForQuestEnderInRange: NPC {} (Entry: {}) is NOT a quest giver, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        float distance = std::sqrt(bot->GetExactDistSq(creature)); // Calculate once from squared distance
        TC_LOG_ERROR("module.playerbot.quest", "✅ CheckForQuestEnderInRange: Found valid quest ender {} (Entry: {}) at distance {:.1f}",
                     creature->GetName(), creature->GetEntry(), distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestQuestEnder = creature;
        }
    }

    if (!closestQuestEnder)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CheckForQuestEnderInRange: Bot {} found creatures with entry {} but none are valid quest enders (phase mismatch or not alive)",
                     bot->GetName(), npcEntry);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ CheckForQuestEnderInRange: Bot {} found quest ender {} at distance {:.1f}",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Check if in interaction range
    if (closestDistance > INTERACTION_DISTANCE)
    {
        TC_LOG_ERROR("module.playerbot.quest", "🚶 CheckForQuestEnderInRange: Bot {} quest ender {} too far ({:.1f} > INTERACTION_DISTANCE), moving closer",
                     bot->GetName(), closestQuestEnder->GetName(), closestDistance);

        // Move to NPC
        Position npcPos;
        npcPos.Relocate(closestQuestEnder->GetPositionX(), closestQuestEnder->GetPositionY(), closestQuestEnder->GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, npcPos);
        return false; // Not in range yet, but moving
    }

    // NPC is in interaction range - get all completed quests and turn them in
    TC_LOG_ERROR("module.playerbot.quest", "🎯 CheckForQuestEnderInRange: Bot {} at quest ender {} (distance {:.1f} <= INTERACTION_DISTANCE), processing quest turn-ins",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Scan ALL active quests and turn in any that are complete with this NPC
    bool anyQuestTurnedIn = false;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        // Check if quest is complete
        if (bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE)
            continue;

        // Verify this NPC can end this quest
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
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
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ CheckForQuestEnderInRange: NPC {} is NOT a valid ender for quest {} ({}), skipping",
                         closestQuestEnder->GetName(), questId, quest->GetLogTitle());
            continue;
        }

        // Turn in the quest
        TC_LOG_ERROR("module.playerbot.quest", "🎯 CheckForQuestEnderInRange: Bot {} turning in quest {} ({}) to NPC {}",
                     bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());

        if (CompleteQuestTurnIn(ai, questId, closestQuestEnder))
        {
            anyQuestTurnedIn = true;
            TC_LOG_ERROR("module.playerbot.quest", "✅ CheckForQuestEnderInRange: Bot {} successfully turned in quest {} ({})",
                         bot->GetName(), questId, quest->GetLogTitle());
        }
    }

    return anyQuestTurnedIn;
}

bool QuestStrategy::CompleteQuestTurnIn(BotAI* ai, uint32 questId, ::Unit* questEnder)
{
    if (!ai || !ai->GetBot() || !questEnder)
        return false;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    TC_LOG_ERROR("module.playerbot.quest", "🏆 CompleteQuestTurnIn: Bot {} completing quest {} ({}) with NPC {}",
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
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ EquipmentManager not available for quest reward selection");
            // Fall back to first choice
            selectedRewardIndex = 0;
        }
        else
        {
            float bestScore = -10000.0f; // Start with very low score
            uint32 bestChoice = 0;
            bool foundUsableReward = false;

            TC_LOG_ERROR("module.playerbot.quest", "🎁 Evaluating {} reward choices for quest {}",
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

            TC_LOG_ERROR("module.playerbot.quest", "   Choice {}: {} - Score: {:.2f} (ilvl {}, quality {})",
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

            TC_LOG_ERROR("module.playerbot.quest", "✅ Selected reward choice {}: {} (score: {:.2f})",
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

        TC_LOG_ERROR("module.playerbot.quest", "✅ CompleteQuestTurnIn: Bot {} successfully completed quest {} with reward choice {} (itemId: {})",
                     bot->GetName(), questId, selectedRewardIndex, selectedItemId);

        // Increment quest completion counter
        _questsCompleted++;

        return true;
    }
    else
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CompleteQuestTurnIn: Bot {} failed CanRewardQuest check for quest {} (missing requirements?)",
                     bot->GetName(), questId);
        return false;
    }
}

// ========================================================================
// QUEST AREA WANDERING SYSTEM - Patrol while waiting for respawns
// ========================================================================

bool QuestStrategy::ShouldWanderInQuestArea(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);

    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return false;

    // Check if quest has area data (QuestPOI with multiple points)
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
                TC_LOG_ERROR("module.playerbot.quest", "✅ ShouldWanderInQuestArea: Quest {} objective {} has {} POI points - wandering enabled",
                             objective.questId, objective.objectiveIndex, blob.Points.size());
                return true;
            }
        }
    }

    return false;
}

void QuestStrategy::InitializeQuestAreaWandering(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Clear existing wander points
    _questAreaWanderPoints.clear();
    _currentWanderPointIndex = 0;

    // Get quest POI data
    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(objective.questId);

    if (!poiData || poiData->Blobs.empty())
        return;

    // Find blob for current objective and map
    for (auto const& blob : poiData->Blobs)
    {
        if (blob.MapID == static_cast<int32>(bot->GetMapId()) &&
            blob.ObjectiveIndex == static_cast<int32>(objective.objectiveIndex))
        {
            TC_LOG_ERROR("module.playerbot.quest", "🗺️ InitializeQuestAreaWandering: Bot {} - Found quest area with {} POI points",
                         bot->GetName(), blob.Points.size());

            // Convert POI points to wander positions
            for (auto const& point : blob.Points)
            {
                Position pos;
                pos.Relocate(static_cast<float>(point.X), static_cast<float>(point.Y), static_cast<float>(point.Z));
                _questAreaWanderPoints.push_back(pos);

                TC_LOG_ERROR("module.playerbot.quest", "📍 Wander point {}: ({:.1f}, {:.1f}, {:.1f})",
                             _questAreaWanderPoints.size(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
            }

            // Start at a random point based on bot GUID (deterministic but unique per bot)
            if (!_questAreaWanderPoints.empty())
            {
                _currentWanderPointIndex = bot->GetGUID().GetCounter() % _questAreaWanderPoints.size();
                TC_LOG_ERROR("module.playerbot.quest", "🎲 Bot {} starting wander at point {} of {}",
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
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ WanderInQuestArea: Bot {} - No wander points initialized",
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

    // Check if bot is already at current wander point
    Position const& currentWanderPoint = _questAreaWanderPoints[_currentWanderPointIndex];
    float distance = bot->GetExactDist2d(currentWanderPoint.GetPositionX(), currentWanderPoint.GetPositionY());
    if (distance < 10.0f)
    {
        // Reached current point - move to next point
        _currentWanderPointIndex = (_currentWanderPointIndex + 1) % _questAreaWanderPoints.size();

        TC_LOG_ERROR("module.playerbot.quest", "✅ WanderInQuestArea: Bot {} reached wander point, moving to next point {} of {}",
                     bot->GetName(), _currentWanderPointIndex, _questAreaWanderPoints.size());
    }

    // Move to current wander point
    Position const& targetPoint = _questAreaWanderPoints[_currentWanderPointIndex];

    TC_LOG_ERROR("module.playerbot.quest", "🚶 WanderInQuestArea: Bot {} wandering to point {} at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                 bot->GetName(), _currentWanderPointIndex,
                 targetPoint.GetPositionX(), targetPoint.GetPositionY(), targetPoint.GetPositionZ(),
                 bot->GetExactDist2d(targetPoint.GetPositionX(), targetPoint.GetPositionY()));

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

} // namespace Playerbot
