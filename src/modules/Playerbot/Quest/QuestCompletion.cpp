/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "QuestCompletion.h"
#include "Core/PlayerBotHelpers.h"  // GetBotAI, GetGameSystems
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "Group.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "QuestPickup.h"
#include "QuestTurnIn.h"
#include "ObjectiveTracker.h"
#include "Movement/LeaderFollowBehavior.h"
#include "../AI/BotAI.h"
#include "Movement/BotMovementUtil.h"
#include "Interaction/Core/InteractionManager.h"
#include "../Combat/TargetSelector.h"
#include "Config/PlayerbotConfig.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"
#include "../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// Quest giver interaction range constant
constexpr float QUEST_GIVER_INTERACTION_RANGE = 5.0f;

/**
 * @brief Singleton instance implementation
 */
QuestCompletion::QuestCompletion(Player* bot) : _bot(bot) {
    if (!_bot) TC_LOG_ERROR("playerbot.quest", "QuestCompletion: null bot!");
}

QuestCompletion::~QuestCompletion() {}

/**
 * @brief Constructor
 */
QuestCompletion::QuestCompletion()
{
    _globalMetrics.Reset();
}

/**
 * @brief Start tracking quest completion for a bot
 * @param questId Quest ID to track
 * @param bot Bot player
 * @return True if quest tracking started successfully
 */
bool QuestCompletion::StartQuestCompletion(uint32 questId, Player* bot)
{
    if (!bot || !questId)
        return false;

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    // Check if bot has the quest
    if (bot->GetQuestStatus(questId) == QUEST_STATUS_NONE)
    {
        TC_LOG_DEBUG("playerbot", "QuestCompletion::StartQuestCompletion - Bot %s doesn't have quest %u",
            bot->GetName().c_str(), questId);
        return false;
    }

    // Initialize quest progress tracking
    QuestProgressData progress(questId, bot->GetGUID().GetCounter());
    progress.questGiverGuid = 0; // Will be populated when quest giver is found
    // Parse quest objectives
    ParseQuestObjectives(progress, quest);
    // Set appropriate completion strategy based on bot configuration
    if (bot->GetGroup())
        progress.strategy = QuestCompletionStrategy::GROUP_COORDINATION;
    else
        progress.strategy = QuestCompletionStrategy::EFFICIENT_COMPLETION;

    // Store progress data
    _botQuestProgress[bot->GetGUID().GetCounter()].push_back(progress);
    _botsInQuestMode.insert(bot->GetGUID().GetCounter());

    // Update metrics
    _globalMetrics.questsStarted++;
    _botMetrics[bot->GetGUID().GetCounter()].questsStarted++;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::StartQuestCompletion - Started tracking quest %u for bot %s",
        questId, bot->GetName().c_str());

    return true;
}

/**
 * @brief Update quest progress for a bot
 * @param bot Bot player
 */
void QuestCompletion::UpdateQuestProgress(Player* bot)
{
    if (!bot)
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    for (auto& progress : it->second)
    {
        // Skip completed quests
        if (progress.completionPercentage >= 100.0f)
            continue;

        // Update each objective
        bool anyProgress = false;
        for (auto& objective : progress.objectives)
        {
            if (objective.status == ObjectiveStatus::COMPLETED)
                continue;

            UpdateQuestObjectiveFromProgress(objective, bot);

            if (objective.currentCount > 0)
                anyProgress = true;
        }

        // Calculate overall completion percentage
        float totalProgress = 0.0f;
        uint32 completedObjectives = 0;
        for (const auto& objective : progress.objectives)
        {
            if (objective.requiredCount > 0)
            {
                float objProgress = std::min(1.0f,
                    static_cast<float>(objective.currentCount) / objective.requiredCount);
                totalProgress += objProgress;

                if (objective.status == ObjectiveStatus::COMPLETED)
                    completedObjectives++;
            }
        }

        if (!progress.objectives.empty())
        {
            progress.completionPercentage = (totalProgress / progress.objectives.size()) * 100.0f;
        }
        // Check for stuck state
        if (!anyProgress)
        {
            uint32 currentTime = GameTime::GetGameTimeMS();
            if (currentTime - progress.lastUpdateTime > STUCK_DETECTION_TIME)
            {
                progress.isStuck = true;
                progress.stuckTime = currentTime;
                DetectStuckState(bot, progress.questId);
            }
        }
        else
        {
            progress.isStuck = false;
            progress.stuckTime = 0;
            progress.lastUpdateTime = GameTime::GetGameTimeMS();
        }

        // Check if quest is ready for turn-in
        if (progress.completionPercentage >= 100.0f && progress.requiresTurnIn)
        {
            CompleteQuest(progress.questId, bot);
        }
    }
}

/**
 * @brief Complete a quest for the bot
 * @param questId Quest ID
 * @param bot Bot player
 */
void QuestCompletion::CompleteQuest(uint32 questId, Player* bot)
{
    if (!bot || !questId)
        return;

    // Check if quest can be completed
    if (!bot->CanCompleteQuest(questId))
    {
        TC_LOG_DEBUG("playerbot", "QuestCompletion::CompleteQuest - Bot %s cannot complete quest %u yet",
            bot->GetName().c_str(), questId);
        return;
    }

    // Mark quest as complete
    bot->CompleteQuest(questId);

    // Update progress tracking
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt != it->second.end())
        {
            questIt->completionPercentage = 100.0f;
            questIt->requiresTurnIn = true;

            // Add completion log entry
            questIt->completionLog.push_back("Quest completed at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    // Update metrics
    _globalMetrics.questsCompleted++;
    _botMetrics[bot->GetGUID().GetCounter()].questsCompleted++;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::CompleteQuest - Bot %s completed quest %u",
        bot->GetName().c_str(), questId);

    // Schedule turn-in through QuestTurnIn system (per-bot)
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ScheduleQuestTurnIn(questId);
}

/**
 * @brief Turn in a completed quest
 * @param questId Quest ID
 * @param bot Bot player
 * @return True if quest was turned in successfully
 */
bool QuestCompletion::TurnInQuest(uint32 questId, Player* bot)
{
    if (!bot || !questId)
        return false;

    // Delegate to QuestTurnIn system (per-bot)
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->TurnInQuest(questId);
    return false;
}

/**
 * @brief Track quest objectives for a bot
 * @param bot Bot player
 */
void QuestCompletion::TrackQuestObjectives(Player* bot)
{
    if (!bot)
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    for (auto& progress : it->second)
    {
        // Skip completed quests
        if (progress.completionPercentage >= 100.0f)
            continue;

        // Execute objectives based on strategy
        switch (progress.strategy)
        {
            case QuestCompletionStrategy::EFFICIENT_COMPLETION:
                ExecuteEfficientStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::SAFE_COMPLETION:
                ExecuteSafeStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::GROUP_COORDINATION:
                ExecuteGroupStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::SOLO_FOCUS:
                ExecuteSoloStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::EXPERIENCE_MAXIMIZING:
                ExecuteExperienceStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::SPEED_COMPLETION:
                ExecuteSpeedStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::THOROUGH_EXPLORATION:
                ExecuteExplorationStrategy(bot, progress);
                break;
        }
    }
}

/**
 * @brief Execute a quest objective
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::ExecuteObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot || objective.status == ObjectiveStatus::COMPLETED)
        return;

    // Check if we can execute this objective
    if (!CanExecuteObjective(bot, objective))
    {
        objective.status = ObjectiveStatus::BLOCKED;
        return;
    }

    objective.status = ObjectiveStatus::IN_PROGRESS;

    // Execute based on objective type
    switch (objective.type)
    {
        case QuestObjectiveType::KILL_CREATURE:
            HandleKillObjective(bot, objective);
            break;
        case QuestObjectiveType::COLLECT_ITEM:
            HandleCollectObjective(bot, objective);
            break;
        case QuestObjectiveType::TALK_TO_NPC:
            HandleTalkToNpcObjective(bot, objective);
            break;
        case QuestObjectiveType::REACH_LOCATION:
            HandleLocationObjective(bot, objective);
            break;
        case QuestObjectiveType::USE_GAMEOBJECT:
            HandleGameObjectObjective(bot, objective);
            break;
        case QuestObjectiveType::CAST_SPELL:
            HandleSpellCastObjective(bot, objective);
            break;
        case QuestObjectiveType::EMOTE_AT_TARGET:
            HandleEmoteObjective(bot, objective);
            break;
        case QuestObjectiveType::ESCORT_NPC:
            HandleEscortObjective(bot, objective);
            break;
        default:
            TC_LOG_DEBUG("playerbot", "QuestCompletion::ExecuteObjective - Unknown objective type %u",
                static_cast<uint32>(objective.type));
            break;
    }

    // Update time spent
    objective.timeSpent += GameTime::GetGameTimeMS() - objective.lastUpdateTime;
    objective.lastUpdateTime = GameTime::GetGameTimeMS();
}

/**
 * @brief Update objective progress
 * @param bot Bot player
 * @param questId Quest ID
 * @param objectiveIndex Objective index
 */
void QuestCompletion::UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (!bot)
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt == it->second.end() || objectiveIndex >= questIt->objectives.size())
        return;

    auto& objective = questIt->objectives[objectiveIndex];

    // Get quest from ObjectMgr to access objectives
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return;

    QuestObjectives const& questObjectives = quest->GetObjectives();
    if (objectiveIndex >= questObjectives.size())
        return;

    QuestObjective const& questObj = questObjectives[objectiveIndex];

    // Update objective count using Player::GetQuestObjectiveData
    objective.currentCount = bot->GetQuestObjectiveData(questObj);
    if (objective.currentCount >= objective.requiredCount)
    {
        objective.status = ObjectiveStatus::COMPLETED;
        _globalMetrics.objectivesCompleted++;
        _botMetrics[bot->GetGUID().GetCounter()].objectivesCompleted++;
    }
}

/**
 * @brief Check if an objective is complete
 * @param objective Objective data
 * @return True if objective is complete
 */
bool QuestCompletion::IsObjectiveComplete(const QuestObjectiveData& objective)
{
    return objective.status == ObjectiveStatus::COMPLETED ||
           (objective.requiredCount > 0 && objective.currentCount >= objective.requiredCount);
}

/**
 * @brief Handle kill objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleKillObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find creatures to kill
    if (!FindKillTarget(bot, objective))
    {
        // No targets found, move to spawn location if known
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
        return;
    }

    // Get nearest target
    Unit* target = nullptr;
    float minDistance = objective.searchRadius;

    // DEADLOCK FIX: Use lock-free spatial grid
    Map* map = bot->GetMap();
    if (map)
    {
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
        }

        if (spatialGrid)
        {
            // DEADLOCK FIX: Use snapshot-based query (thread-safe, lock-free)
            std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
                spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

            ObjectGuid targetGuid;  // Store GUID instead of pointer

            for (auto const& snapshot : nearbyCreatures)
            {
                // Use snapshot fields instead of calling methods on pointers
                if (snapshot.isDead || !snapshot.isVisible)
                    continue;
                if (snapshot.entry != objective.targetId)
                    continue;

                float distance = bot->GetExactDist(snapshot.position);
                if (distance < minDistance)
                {
                    minDistance = distance;
                    targetGuid = snapshot.guid;  // Store GUID
                }
            }

            // Store the GUID for later use (main thread will resolve)
            if (!targetGuid.IsEmpty())
                target = ObjectAccessor::GetCreature(*bot, targetGuid);  // Safe: this method may run on main thread context
        }
    }

    if (target)
    {
        // Engage target through combat system
        if (BotAI* ai = dynamic_cast<BotAI*>(bot->GetAI()))
        {
            // Set the bot's target to the creature
            bot->SetSelection(target->GetGUID());
            // Attack the target
            bot->Attack(target, true);
        }

        // Track kill credit - Update using proper API
        Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
        if (quest)
        {
            QuestObjectives const& questObjectives = quest->GetObjectives();
            if (objective.objectiveIndex < questObjectives.size())
            {
                objective.currentCount = bot->GetQuestObjectiveData(questObjectives[objective.objectiveIndex]);
            }
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleKillObjective - Bot %s engaging %s for quest %u",
            bot->GetName().c_str(), target->GetName().c_str(), objective.questId);
    }
}

/**
 * @brief Handle collect objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleCollectObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Check if we already have the items
    uint32 itemCount = bot->GetItemCount(objective.targetId);
    if (itemCount >= objective.requiredCount)
    {
        objective.currentCount = itemCount;
        objective.status = ObjectiveStatus::COMPLETED;
        return;
    }

    // Find items to collect (from creatures or game objects)
    if (!FindCollectibleItem(bot, objective))
    {
        // Move to collection area
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
        return;
    }

    // Update collection progress
    objective.currentCount = bot->GetItemCount(objective.targetId);
}

/**
 * @brief Handle talk to NPC objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find the NPC
    Creature* npc = nullptr;
    if (objective.targetId)
    {
        // DEADLOCK FIX: Use lock-free spatial grid
        Map* map = bot->GetMap();
        if (map)
        {
            DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
            {
                sSpatialGridManager.CreateGrid(map);
                spatialGrid = sSpatialGridManager.GetGrid(map);
            }
            if (spatialGrid)
            {
                // DEADLOCK FIX: Use snapshot-based query (thread-safe, lock-free)
                std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
                    spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);
                ObjectGuid npcGuid;
                float minDistance = objective.searchRadius;

                for (auto const& snapshot : nearbyCreatures)
                {
                    // Use snapshot fields instead of pointer method calls
                    if (!snapshot.isVisible)
                        continue;
                    if (snapshot.entry != objective.targetId)
                        continue;
                    float distance = bot->GetExactDist(snapshot.position);
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        npcGuid = snapshot.guid;
                    }
                }

                // Resolve GUID to pointer after loop (safe if method runs on main thread context)
                if (!npcGuid.IsEmpty())
                    npc = ObjectAccessor::GetCreature(*bot, npcGuid);
            }
        }
    }

    if (npc)
    {
        // Move to NPC if not in range
        if (bot->GetDistance(npc) > QUEST_GIVER_INTERACTION_RANGE)
        {
            BotMovementUtil::MoveToUnit(bot, npc, QUEST_GIVER_INTERACTION_RANGE - 1.0f);
            return;
        }

        // Interact with NPC - use StartInteraction instead of InteractWithNPC
        InteractionManager::Instance()->StartInteraction(bot, npc, InteractionType::None);

        // Mark objective as complete
        objective.status = ObjectiveStatus::COMPLETED;
        objective.currentCount = objective.requiredCount;

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleTalkToNpcObjective - Bot %s talked to %s for quest %u",
            bot->GetName().c_str(), npc->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to NPC location if known
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Handle location objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleLocationObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Check if we're at the location
    float distance = bot->GetDistance(objective.targetLocation);
    if (distance <= OBJECTIVE_COMPLETION_RADIUS)
    {
        // Trigger area exploration
        bot->AreaExploredOrEventHappens(objective.questId);

        objective.status = ObjectiveStatus::COMPLETED;
        objective.currentCount = objective.requiredCount;

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleLocationObjective - Bot %s reached location for quest %u",
            bot->GetName().c_str(), objective.questId);
    }
    else
    {
        // Navigate to location
        NavigateToObjective(bot, objective);
    }
}
/**
 * @brief Handle game object objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // DEADLOCK FIX: Use lock-free spatial grid to find game object
    GameObject* gameObject = nullptr;
    if (objective.targetId)
    {
        Map* map = bot->GetMap();
        if (!map)
            return;

        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
                return;
        }

        // Query nearby GameObjects (lock-free!)
        std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
            spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), objective.searchRadius);
        // Find matching GameObject using snapshots
        ObjectGuid targetGuid;
        for (auto const& snapshot : nearbyObjects)
        {
            if (snapshot.entry == objective.targetId && snapshot.isSpawned)
            {
                targetGuid = snapshot.guid;
                break;
            }
        }

        // Resolve GUID after loop
        if (!targetGuid.IsEmpty())
            gameObject = ObjectAccessor::GetGameObject(*bot, targetGuid);
    }

    if (gameObject)
    {
        // Move to object if not in range
        if (bot->GetDistance(gameObject) > QUEST_GIVER_INTERACTION_RANGE)
        {
            BotMovementUtil::MoveToPosition(bot, gameObject->GetPosition());
            return;
        }

        // Use the game object
        gameObject->Use(bot);

        // Update progress
        objective.currentCount++;
        if (objective.currentCount >= objective.requiredCount)
        {
            objective.status = ObjectiveStatus::COMPLETED;
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleGameObjectObjective - Bot %s used %s for quest %u",
            bot->GetName().c_str(), gameObject->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to object location if known
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Handle spell cast objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Check if bot knows the spell
    if (!bot->HasSpell(objective.targetId))
    {
        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleSpellCastObjective - Bot %s doesn't know spell %u for quest %u",
            bot->GetName().c_str(), objective.targetId, objective.questId);
        objective.status = ObjectiveStatus::BLOCKED;
        return;
    }

    // Find target for spell if needed
    Unit* target = bot->GetSelectedUnit();
    if (!target && objective.targetLocation.GetPositionX() != 0)
    {
        // Move to cast location
        NavigateToObjective(bot, objective);
        return;
    }

    // Cast the spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(objective.targetId, DIFFICULTY_NONE);
    if (spellInfo)
    {
        bot->CastSpell(target ? target : bot, objective.targetId, false);

        // Update progress
        objective.currentCount++;
        if (objective.currentCount >= objective.requiredCount)
        {
            objective.status = ObjectiveStatus::COMPLETED;
        }
        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleSpellCastObjective - Bot %s cast spell %u for quest %u",
            bot->GetName().c_str(), objective.targetId, objective.questId);
    }
}

/**
 * @brief Handle emote objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleEmoteObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find target for emote
    Unit* target = nullptr;
    if (objective.targetId)
    {
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
        Map* map = bot->GetMap();
        if (!map)
            return;

        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
                return;
        }

        // Use snapshot-based query (thread-safe, lock-free)
        std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
            spatialGrid->QueryNearbyCreatures(bot->GetPosition(), QUEST_GIVER_INTERACTION_RANGE);

        ObjectGuid targetGuid;
        for (auto const& snapshot : nearbyCreatures)
        {
            if (snapshot.entry == objective.targetId && snapshot.isVisible)
            {
                targetGuid = snapshot.guid;
                break;  // Use first matching creature
            }
        }

        // Resolve GUID to pointer after loop
        if (!targetGuid.IsEmpty())
            target = ObjectAccessor::GetCreature(*bot, targetGuid);
    }

    if (target)
    {
        // Perform emote (cast targetId to Emote enum)
        bot->HandleEmoteCommand(static_cast<Emote>(objective.targetId));

        // Update progress
        objective.currentCount++;
        if (objective.currentCount >= objective.requiredCount)
        {
            objective.status = ObjectiveStatus::COMPLETED;
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleEmoteObjective - Bot %s performed emote for quest %u",
            bot->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to target location
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Handle escort objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleEscortObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find escort target
    Creature* escortTarget = nullptr;
    if (objective.targetId)
    {
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
        Map* map = bot->GetMap();
        if (!map)
            return;

        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
                return;
        }
        // Use snapshot-based query (thread-safe, lock-free)
        std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
            spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

        ObjectGuid escortGuid;
        for (auto const& snapshot : nearbyCreatures)
        {
            if (snapshot.entry == objective.targetId && !snapshot.isDead)
            {
                escortGuid = snapshot.guid;
                break;  // Use first alive matching creature
            }
        }

        // Resolve GUID to pointer after loop
        if (!escortGuid.IsEmpty())
            escortTarget = ObjectAccessor::GetCreature(*bot, escortGuid);
    }

    if (escortTarget)
    {
        // Follow the escort target
        if (bot->GetDistance(escortTarget) > 10.0f)
        {
            BotMovementUtil::MoveToUnit(bot, escortTarget, 5.0f);
        }

        // Check if escort is complete (handled by quest system)
        if (bot->IsQuestObjectiveComplete(objective.questId, objective.objectiveIndex))
        {
            objective.status = ObjectiveStatus::COMPLETED;
            objective.currentCount = objective.requiredCount;
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleEscortObjective - Bot %s escorting %s for quest %u",
            bot->GetName().c_str(), escortTarget->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to escort start location
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Navigate to quest objective
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::NavigateToObjective(Player* bot, const QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Get optimal position for objective
    Position targetPos = GetOptimalObjectivePosition(bot, objective);
    // Use movement utility to navigate
    BotMovementUtil::MoveToPosition(bot, targetPos);

    TC_LOG_DEBUG("playerbot", "QuestCompletion::NavigateToObjective - Bot %s moving to objective for quest %u",
        bot->GetName().c_str(), objective.questId);
}

/**
 * @brief Find target for kill objective
 * @param bot Bot player
 * @param objective Objective data
 * @return True if target found
 */
bool QuestCompletion::FindKillTarget(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return false;
    // DEADLOCK FIX: Use lock-free spatial grid with snapshots
    Map* map = bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // Use snapshot-based query (thread-safe, lock-free)
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

    // Filter valid targets using snapshot data
    DoubleBufferedSpatialGrid::CreatureSnapshot nearestSnapshot;
    float minDistance = objective.searchRadius;
    bool found = false;

    for (auto const& snapshot : nearbyCreatures)
    {
        // Filter: must match entry, be alive, visible, and hostile
        if (snapshot.entry != objective.targetId)
            continue;
        if (snapshot.isDead || !snapshot.isVisible)
            continue;
        // Note: faction check would require additional snapshot field or be done after GUID resolution

        float distance = bot->GetExactDist(snapshot.position);
        if (distance < minDistance)
        {
            minDistance = distance;
            nearestSnapshot = snapshot;
            found = true;
        }
    }

    if (found)
    {
        // Update objective location to nearest target (using snapshot position)
        objective.targetLocation = nearestSnapshot.position;
        return true;
    }

    return false;
}

/**
 * @brief Find collectible item for objective
 * @param bot Bot player
 * @param objective Objective data
 * @return True if item source found
 */
bool QuestCompletion::FindCollectibleItem(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return false;

    Map* map = bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // DEADLOCK FIX: First check if item drops from creatures using snapshots
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

    for (auto const& snapshot : nearbyCreatures)
    {
        // Filter: must be alive and not friendly (assume non-friendly if we can't check faction)
        if (snapshot.isDead)
            continue;

        // Check if creature can drop the item (would need loot template access)
        // For now, assume creatures in quest area can drop quest items
        float distance = bot->GetExactDist(snapshot.position);
        if (distance < objective.searchRadius)
        {
            objective.targetLocation = snapshot.position;
            return true;
        }
    }

    // DEADLOCK FIX: Check for game objects using snapshots
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), objective.searchRadius);

    for (auto const& snapshot : nearbyObjects)
    {
        // Check if game object is lootable using snapshot data
        if (snapshot.goType == GAMEOBJECT_TYPE_CHEST ||
            snapshot.goType == GAMEOBJECT_TYPE_GOOBER)
        {
            objective.targetLocation = snapshot.position;
            return true;
        }
    }

    return false;
}

/**
 * @brief Get optimal position for objective
 * @param bot Bot player
 * @param objective Objective data
 * @return Optimal position
 */
Position QuestCompletion::GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective)
{
    if (!bot)
        return Position();

    // If we have a specific target location, use it
    if (objective.targetLocation.GetPositionX() != 0)
        return objective.targetLocation;

    // Otherwise, try to find objective locations from database
    std::vector<Position> locations = GetObjectiveLocations(objective);
    if (!locations.empty())
    {
        // Return nearest location
        return *std::min_element(locations.begin(), locations.end(),
            [bot](const Position& a, const Position& b)
            {
                return bot->GetDistance(a) < bot->GetDistance(b);
            });
    }

    // Default to bot's current position
    return bot->GetPosition();
}

/**
 * @brief Get possible locations for objective
 * @param objective Objective data
 * @return Vector of possible positions
 */
std::vector<Position> QuestCompletion::GetObjectiveLocations(const QuestObjectiveData& objective)
{
    std::vector<Position> locations;

    // Add primary location if set
    if (objective.targetLocation.GetPositionX() != 0)
        locations.push_back(objective.targetLocation);

    // Could query database for spawn points of objective creatures/objects
    // For now, return what we have

    return locations;
}

/**
 * @brief Parse quest objectives from quest template
 * @param progress Quest progress data
 * @param quest Quest template
 */
void QuestCompletion::ParseQuestObjectives(QuestProgressData& progress, const Quest* quest)
{
    if (!quest)
        return;

    // Parse objectives from Quest::Objectives vector (modern TrinityCore API)
    QuestObjectives const& questObjectives = quest->GetObjectives();

    for (size_t i = 0; i < questObjectives.size(); ++i)
    {
        QuestObjective const& obj = questObjectives[i];

        // Map TrinityCore objective types to our internal types
        QuestObjectiveType internalType;
        switch (obj.Type)
        {
            case QUEST_OBJECTIVE_MONSTER:
                internalType = QuestObjectiveType::KILL_CREATURE;
                break;
            case QUEST_OBJECTIVE_ITEM:
                internalType = QuestObjectiveType::COLLECT_ITEM;
                break;
            case QUEST_OBJECTIVE_GAMEOBJECT:
                internalType = QuestObjectiveType::USE_GAMEOBJECT;
                break;
            case QUEST_OBJECTIVE_TALKTO:
                internalType = QuestObjectiveType::TALK_TO_NPC;
                break;
            case QUEST_OBJECTIVE_AREATRIGGER:
            case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
            case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
                internalType = QuestObjectiveType::REACH_LOCATION;
                break;
            case QUEST_OBJECTIVE_LEARNSPELL:
                internalType = QuestObjectiveType::LEARN_SPELL;
                break;
            default:
                internalType = QuestObjectiveType::CUSTOM_OBJECTIVE;
                break;
        }

        if (obj.Amount > 0)
        {
            QuestObjectiveData objective(quest->GetQuestId(), static_cast<uint32>(i),
                                        internalType, obj.ObjectID, obj.Amount);
            objective.description = obj.Description;
            objective.isOptional = (obj.Flags & QUEST_OBJECTIVE_FLAG_OPTIONAL) != 0;
            progress.objectives.push_back(objective);
        }
    }
}

/**
 * @brief Update quest objective from game progress
 * @param objective Objective data to update
 * @param bot Bot player
 */
void QuestCompletion::UpdateQuestObjectiveFromProgress(QuestObjectiveData& objective, Player* bot)
{
    if (!bot)
        return;

    // Get quest from ObjectMgr to access objectives
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest)
        return;

    QuestObjectives const& questObjectives = quest->GetObjectives();
    if (objective.objectiveIndex >= questObjectives.size())
        return;

    QuestObjective const& questObj = questObjectives[objective.objectiveIndex];

    // Update based on objective type
    switch (objective.type)
    {
        case QuestObjectiveType::KILL_CREATURE:
        case QuestObjectiveType::USE_GAMEOBJECT:
        case QuestObjectiveType::TALK_TO_NPC:
            // Use GetQuestObjectiveData to retrieve progress
            objective.currentCount = bot->GetQuestObjectiveData(questObj);
            break;

        case QuestObjectiveType::COLLECT_ITEM:
            objective.currentCount = bot->GetItemCount(objective.targetId);
            break;

        default:
            // Check if objective is complete through quest system
            if (bot->IsQuestObjectiveComplete(objective.questId, objective.objectiveIndex))
            {
                objective.currentCount = objective.requiredCount;
            }
            else
            {
                objective.currentCount = bot->GetQuestObjectiveData(questObj);
            }
            break;
    }

    // Update status
    if (objective.currentCount >= objective.requiredCount)
    {
        objective.status = ObjectiveStatus::COMPLETED;
    }
}

/**
 * @brief Check if objective can be executed
 * @param bot Bot player
 * @param objective Objective data
 * @return True if objective can be executed
 */
bool QuestCompletion::CanExecuteObjective(Player* bot, const QuestObjectiveData& objective)
{
    if (!bot)
        return false;

    // Check if objective is already complete
    if (objective.status == ObjectiveStatus::COMPLETED)
        return false;

    // Check if bot is alive
    if (!bot->IsAlive())
        return false;

    // Check if bot is in combat for non-combat objectives
    if (bot->IsInCombat() && objective.type != QuestObjectiveType::KILL_CREATURE)
        return false;

    // Check if objective has failed too many times
    if (objective.retryCount >= MAX_OBJECTIVE_RETRIES)
        return false;

    // Check group requirements
    if (objective.requiresGroup && !bot->GetGroup())
        return false;

    return true;
}

/**
 * @brief Execute efficient completion strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteEfficientStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Find nearest incomplete objective
    QuestObjectiveData* nearestObjective = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (auto& objective : progress.objectives)
    {
        if (objective.status == ObjectiveStatus::COMPLETED)
            continue;

        float distance = bot->GetDistance(objective.targetLocation);
        if (distance < minDistance)
        {
            minDistance = distance;
            nearestObjective = &objective;
        }
    }

    if (nearestObjective)
    {
        ExecuteObjective(bot, *nearestObjective);
    }
}

/**
 * @brief Execute safe completion strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteSafeStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Prioritize non-combat objectives when health is low
    if (bot->GetHealthPct() < 50.0f)
    {
        for (auto& objective : progress.objectives)
        {
            if (objective.status != ObjectiveStatus::COMPLETED &&
                objective.type != QuestObjectiveType::KILL_CREATURE)
            {
                ExecuteObjective(bot, objective);
                return;
            }
        }
    }

    // Otherwise execute normally
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Execute group coordination strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteGroupStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot || !bot->GetGroup())
    {
        ExecuteEfficientStrategy(bot, progress);
        return;
    }

    // Coordinate with group members
    Group* group = bot->GetGroup();
    // Share objective progress
    ShareObjectiveProgress(group, progress.questId);

    // Find objectives that group is working on
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED)
        {
            // Check if other group members are near this objective
            bool groupNearby = false;
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (member && member != bot && member->IsAlive())
                {
                    if (member->GetDistance(objective.targetLocation) < 50.0f)
                    {
                        groupNearby = true;
                        break;
                    }
                }
            }

            if (groupNearby)
            {
                ExecuteObjective(bot, objective);
                return;
            }
        }
    }

    // No group objectives, execute normally
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Share objective progress with group
 * @param group Group pointer
 * @param questId Quest ID
 */
void QuestCompletion::ShareObjectiveProgress(Group* group, uint32 questId)
{
    if (!group)
        return;

    // Update group quest sharing data
    _groupQuestSharing[group->GetGUID().GetCounter()].push_back(questId);
    _groupObjectiveSync[group->GetGUID().GetCounter()][questId] = GameTime::GetGameTimeMS();
}

/**
 * @brief Detect stuck state for quest
 * @param bot Bot player
 * @param questId Quest ID
 */
void QuestCompletion::DetectStuckState(Player* bot, uint32 questId)
{
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::DetectStuckState - Bot %s stuck on quest %u",
        bot->GetName().c_str(), questId);

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt != it->second.end())
    {
        questIt->isStuck = true;
        questIt->stuckTime = GameTime::GetGameTimeMS();
        questIt->consecutiveFailures++;

        // Update metrics
        _globalMetrics.stuckInstances++;
        _botMetrics[bot->GetGUID().GetCounter()].stuckInstances++;

        // Try to recover
        RecoverFromStuckState(bot, questId);
    }
}

/**
 * @brief Recover from stuck state
 * @param bot Bot player
 * @param questId Quest ID
 */
void QuestCompletion::RecoverFromStuckState(Player* bot, uint32 questId)
{
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::RecoverFromStuckState - Attempting recovery for bot %s on quest %u",
        bot->GetName().c_str(), questId);

    // Try different recovery strategies
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt != it->second.end())
    {
        // Reset stuck objectives
        for (auto& objective : questIt->objectives)
        {
            if (objective.status == ObjectiveStatus::IN_PROGRESS)
            {
                objective.status = ObjectiveStatus::NOT_STARTED;
                objective.retryCount++;
            }
        }

        // Change strategy
        if (questIt->strategy == QuestCompletionStrategy::EFFICIENT_COMPLETION)
            questIt->strategy = QuestCompletionStrategy::SAFE_COMPLETION;

        // Reset stuck flag
        questIt->isStuck = false;
        questIt->stuckTime = 0;
    }
}

/**
 * @brief Get bot completion metrics
 * @param botGuid Bot GUID
 * @return Completion metrics
 */
QuestCompletion::QuestCompletionMetrics::Snapshot QuestCompletion::GetBotCompletionMetrics(uint32 botGuid)
{
    auto it = _botMetrics.find(botGuid);
    if (it != _botMetrics.end())
        return it->second.CreateSnapshot();

    QuestCompletionMetrics defaultMetrics;
    return defaultMetrics.CreateSnapshot();
}

/**
 * @brief Get global completion metrics
 * @return Global completion metrics
 */
QuestCompletion::QuestCompletionMetrics::Snapshot QuestCompletion::GetGlobalCompletionMetrics()
{
    return _globalMetrics.CreateSnapshot();
}

/**
 * @brief Set quest completion strategy for bot
 * @param botGuid Bot GUID
 * @param strategy Completion strategy
 */
void QuestCompletion::SetQuestCompletionStrategy(uint32 botGuid, QuestCompletionStrategy strategy)
{
    _botStrategies[botGuid] = strategy;
}

/**
 * @brief Get quest completion strategy for bot
 * @param botGuid Bot GUID
 * @return Completion strategy
 */
QuestCompletionStrategy QuestCompletion::GetQuestCompletionStrategy(uint32 botGuid)
{
    auto it = _botStrategies.find(botGuid);
    if (it != _botStrategies.end())
        return it->second;

    return QuestCompletionStrategy::EFFICIENT_COMPLETION;
}

/**
 * @brief Update quest completion system
 * @param diff Time difference
 */
void QuestCompletion::Update(uint32 diff)
{
    // Clean up completed quests periodically
    static uint32 cleanupTimer = 0;
    cleanupTimer += diff;

    if (cleanupTimer >= 60000) // Every minute
    {
        CleanupCompletedQuests();
        cleanupTimer = 0;
    }

    // Note: Quest turn-in processing is handled by QuestTurnIn system
}

/**
 * @brief Clean up completed quests
 */
void QuestCompletion::CleanupCompletedQuests()
{
    uint32 currentTime = GameTime::GetGameTimeMS();

    for (auto& [botGuid, progressList] : _botQuestProgress)
    {
        progressList.erase(
            std::remove_if(progressList.begin(), progressList.end(),
                [currentTime](const QuestProgressData& progress)
                {
                    return progress.completionPercentage >= 100.0f &&
                           (currentTime - progress.lastUpdateTime) > 300000; // 5 minutes
                }),
            progressList.end()
        );
    }
}

/**
 * @brief Execute solo focus strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteSoloStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Focus on objectives that don't require group
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED && !objective.requiresGroup)
        {
            ExecuteObjective(bot, objective);
            return;
        }
    }

    // If all require group, try anyway
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Execute experience maximizing strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteExperienceStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Prioritize kill objectives for experience
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED &&
            objective.type == QuestObjectiveType::KILL_CREATURE)
        {
            ExecuteObjective(bot, objective);
            return;
        }
    }

    // Then do other objectives
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Execute speed completion strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteSpeedStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Execute multiple objectives in parallel if possible
    uint32 activeObjectives = 0;

    for (auto& objective : progress.objectives)
    {
        if (objective.status == ObjectiveStatus::IN_PROGRESS)
            activeObjectives++;
    }

    if (activeObjectives < MAX_CONCURRENT_OBJECTIVES)
    {
        for (auto& objective : progress.objectives)
        {
            if (objective.status == ObjectiveStatus::NOT_STARTED)
            {
                ExecuteObjective(bot, objective);
                activeObjectives++;

                if (activeObjectives >= MAX_CONCURRENT_OBJECTIVES)
                    break;
            }
        }
    }
}

/**
 * @brief Execute exploration strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteExplorationStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Explore widely while completing objectives
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED)
        {
            // Increase search radius for exploration
            objective.searchRadius = OBJECTIVE_SEARCH_RADIUS * 1.5f;
            ExecuteObjective(bot, objective);
            return;
        }
    }
}

} // namespace Playerbot