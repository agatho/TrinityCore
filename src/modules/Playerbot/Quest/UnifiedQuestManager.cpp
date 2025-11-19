/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "UnifiedQuestManager.h"
#include "Core/PlayerBotHelpers.h"  // GetBotAI, GetGameSystems
#include "Log.h"
#include "Timer.h"
#include <sstream>
#include "GameTime.h"

namespace Playerbot
{

// ============================================================================
// SINGLETON MANAGEMENT
// ============================================================================

UnifiedQuestManager* UnifiedQuestManager::instance()
{
    static UnifiedQuestManager instance;
    return &instance;
}

UnifiedQuestManager::UnifiedQuestManager()
    : _pickup(std::make_unique<PickupModule>())
    , _completion(std::make_unique<CompletionModule>())
    , _validation(std::make_unique<ValidationModule>())
    , _turnIn(std::make_unique<TurnInModule>())
    , _dynamic(std::make_unique<DynamicModule>())
{
    TC_LOG_INFO("playerbot.quest", "UnifiedQuestManager initialized - consolidating 5 quest managers");
}

UnifiedQuestManager::~UnifiedQuestManager()
{
    TC_LOG_INFO("playerbot.quest", "UnifiedQuestManager destroyed");
}

// ============================================================================
// PICKUP MODULE IMPLEMENTATION (delegates to QuestPickup)
// ============================================================================

bool UnifiedQuestManager::PickupModule::PickupQuest(uint32 questId, Player* bot, uint32 questGiverGuid)
{
    _questsPickedUp++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->PickupQuest(questId, questGiverGuid);
    return {};
}

bool UnifiedQuestManager::PickupModule::PickupQuestFromGiver(Player* bot, uint32 questGiverGuid, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->PickupQuestFromGiver(questGiverGuid, questId);
    return {};
}

void UnifiedQuestManager::PickupModule::PickupAvailableQuests(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestPickup()->PickupAvailableQuests();
}

void UnifiedQuestManager::PickupModule::PickupQuestsInArea(Player* bot, float radius)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestPickup()->PickupQuestsInArea(radius);
}

std::vector<uint32> UnifiedQuestManager::PickupModule::DiscoverNearbyQuests(Player* bot, float scanRadius)
{
    _questsDiscovered++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->DiscoverNearbyQuests(scanRadius);
    return {};
}

std::vector<QuestGiverInfo> UnifiedQuestManager::PickupModule::ScanForQuestGivers(Player* bot, float scanRadius)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->ScanForQuestGivers(scanRadius);
    return {};
}

std::vector<uint32> UnifiedQuestManager::PickupModule::GetAvailableQuestsFromGiver(uint32 questGiverGuid, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->GetAvailableQuestsFromGiver(questGiverGuid);
    return {};
}

QuestEligibility UnifiedQuestManager::PickupModule::CheckQuestEligibility(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->CheckQuestEligibility(questId);
    return {};
}

bool UnifiedQuestManager::PickupModule::CanAcceptQuest(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->CanAcceptQuest(questId);
    return {};
}

bool UnifiedQuestManager::PickupModule::MeetsQuestRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->MeetsQuestRequirements(questId);
    return {};
}

std::vector<uint32> UnifiedQuestManager::PickupModule::FilterQuests(const std::vector<uint32>& questIds, Player* bot, const QuestPickupFilter& filter)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->FilterQuests(questIds, filter);
    return {};
}

std::vector<uint32> UnifiedQuestManager::PickupModule::PrioritizeQuests(const std::vector<uint32>& questIds, Player* bot, QuestAcceptanceStrategy strategy)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->PrioritizeQuests(questIds, strategy);
    return {};
}

bool UnifiedQuestManager::PickupModule::ShouldAcceptQuest(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestPickup()->ShouldAcceptQuest(questId);
    return {};
}

// ============================================================================
// COMPLETION MODULE IMPLEMENTATION (delegates to QuestCompletion)
// ============================================================================

bool UnifiedQuestManager::CompletionModule::StartQuestCompletion(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestCompletion()->StartQuestCompletion(questId);
    return {};
}

void UnifiedQuestManager::CompletionModule::UpdateQuestProgress(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->UpdateQuestProgress();
}

void UnifiedQuestManager::CompletionModule::CompleteQuest(uint32 questId, Player* bot)
{
    _questsCompleted++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->CompleteQuest(questId);
}

bool UnifiedQuestManager::CompletionModule::TurnInQuest(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestCompletion()->TurnInQuest(questId);
    return {};
}

void UnifiedQuestManager::CompletionModule::TrackQuestObjectives(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->TrackQuestObjectives();
}

void UnifiedQuestManager::CompletionModule::ExecuteObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->ExecuteObjective(objective);
}

void UnifiedQuestManager::CompletionModule::UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->UpdateObjectiveProgress(questId, objectiveIndex);
}

bool UnifiedQuestManager::CompletionModule::IsObjectiveComplete(const QuestObjectiveData& objective)
{
    _objectivesCompleted++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestCompletion()->IsObjectiveComplete(objective);
    return {};
}

void UnifiedQuestManager::CompletionModule::HandleKillObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleKillObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleCollectObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleCollectObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleTalkToNpcObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleLocationObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleLocationObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleGameObjectObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleSpellCastObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleEmoteObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleEmoteObjective(objective);
}

void UnifiedQuestManager::CompletionModule::HandleEscortObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleEscortObjective(objective);
}

void UnifiedQuestManager::CompletionModule::NavigateToObjective(Player* bot, const QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->NavigateToObjective(objective);
}

bool UnifiedQuestManager::CompletionModule::FindObjectiveTarget(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestCompletion()->FindObjectiveTarget(objective);
    return {};
}

std::vector<Position> UnifiedQuestManager::CompletionModule::GetObjectiveLocations(const QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestCompletion()->GetObjectiveLocations(objective);
    return {};
}

Position UnifiedQuestManager::CompletionModule::GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestCompletion()->GetOptimalObjectivePosition(objective);
    return {};
}

void UnifiedQuestManager::CompletionModule::CoordinateGroupQuestCompletion(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->CoordinateGroupQuestCompletion(group, questId);
}

void UnifiedQuestManager::CompletionModule::ShareObjectiveProgress(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->ShareObjectiveProgress(group, questId);
}

void UnifiedQuestManager::CompletionModule::SynchronizeGroupObjectives(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->SynchronizeGroupObjectives(group, questId);
}

void UnifiedQuestManager::CompletionModule::HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleGroupObjectiveConflict(group, questId, objectiveIndex);
}

void UnifiedQuestManager::CompletionModule::OptimizeQuestCompletionOrder(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->OptimizeQuestCompletionOrder();
}

void UnifiedQuestManager::CompletionModule::OptimizeObjectiveSequence(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->OptimizeObjectiveSequence(questId);
}

void UnifiedQuestManager::CompletionModule::FindEfficientCompletionPath(Player* bot, const std::vector<uint32>& questIds)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->FindEfficientCompletionPath(questIds);
}

void UnifiedQuestManager::CompletionModule::MinimizeTravelTime(Player* bot, const std::vector<QuestObjectiveData>& objectives)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->MinimizeTravelTime(objectives);
}

void UnifiedQuestManager::CompletionModule::DetectStuckState(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->DetectStuckState(questId);
}

void UnifiedQuestManager::CompletionModule::HandleStuckObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->HandleStuckObjective(objective);
}

void UnifiedQuestManager::CompletionModule::RecoverFromStuckState(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->RecoverFromStuckState(questId);
}

void UnifiedQuestManager::CompletionModule::SkipProblematicObjective(Player* bot, QuestObjectiveData& objective)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->SkipProblematicObjective(objective);
}

// ============================================================================
// VALIDATION MODULE IMPLEMENTATION (delegates to QuestValidation)
// ============================================================================

bool UnifiedQuestManager::ValidationModule::ValidateQuest(uint32 questId, Player* bot)
{
    _validationsPerformed++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestValidation()->ValidateQuest(questId);
    if (result)
        _validationsPassed++;
    return result;
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestRequirements(questId);
    return {};
}

std::vector<std::string> UnifiedQuestManager::ValidationModule::GetValidationErrors(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetValidationErrors(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateLevelRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateLevelRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateClassRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateClassRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateRaceRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateRaceRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateSkillRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateSkillRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestPrerequisites(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestPrerequisites(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestChain(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestChain(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::HasCompletedPrerequisites(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->HasCompletedPrerequisites(questId);
    return {};
}

std::vector<uint32> UnifiedQuestManager::ValidationModule::GetMissingPrerequisites(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetMissingPrerequisites(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateReputationRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateReputationRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateFactionRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateFactionRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::HasRequiredReputation(uint32 questId, Player* bot, uint32 factionId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->HasRequiredReputation(questId, factionId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateItemRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateItemRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::HasRequiredItems(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->HasRequiredItems(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::HasInventorySpace(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->HasInventorySpace(questId);
    return {};
}

std::vector<uint32> UnifiedQuestManager::ValidationModule::GetMissingQuestItems(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetMissingQuestItems(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestAvailability(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestAvailability(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateSeasonalAvailability(uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateSeasonalAvailability(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateDailyQuestLimits(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateDailyQuestLimits(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestTimer(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestTimer(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateZoneRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateZoneRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateAreaRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateAreaRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::IsInCorrectZone(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->IsInCorrectZone(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::CanQuestBeStartedAtLocation(uint32 questId, const Position& location)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->CanQuestBeStartedAtLocation(questId, location);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateGroupRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateGroupRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidatePartyQuestRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidatePartyQuestRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateRaidQuestRequirements(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateRaidQuestRequirements(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->CanGroupMemberShareQuest(questId, sharer, receiver);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateWithContext(ValidationContext& context)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateWithContext(context);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestObjectives(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestObjectives(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestRewards(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestRewards(questId);
    return {};
}

bool UnifiedQuestManager::ValidationModule::ValidateQuestDifficulty(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateQuestDifficulty(questId);
    return {};
}

ValidationResult UnifiedQuestManager::ValidationModule::GetCachedValidation(uint32 questId, uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetCachedValidation(questId, botGuid);
    return {};
}

void UnifiedQuestManager::ValidationModule::CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestValidation()->CacheValidationResult(questId, botGuid, result);
}

void UnifiedQuestManager::ValidationModule::InvalidateValidationCache(uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestValidation()->InvalidateValidationCache(botGuid);
}

void UnifiedQuestManager::ValidationModule::CleanupExpiredCache()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestValidation()->CleanupExpiredCache();
}

std::unordered_map<uint32, ValidationResult> UnifiedQuestManager::ValidationModule::ValidateMultipleQuests(
    const std::vector<uint32>& questIds, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->ValidateMultipleQuests(questIds);
    return {};
}

std::vector<uint32> UnifiedQuestManager::ValidationModule::FilterValidQuests(const std::vector<uint32>& questIds, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->FilterValidQuests(questIds);
    return {};
}

std::vector<uint32> UnifiedQuestManager::ValidationModule::GetEligibleQuests(Player* bot, const std::vector<uint32>& candidates)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetEligibleQuests(candidates);
    return {};
}

std::string UnifiedQuestManager::ValidationModule::GetDetailedValidationReport(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetDetailedValidationReport(questId);
    return {};
}

void UnifiedQuestManager::ValidationModule::LogValidationFailure(uint32 questId, Player* bot, const std::string& reason)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestValidation()->LogValidationFailure(questId, reason);
}

std::vector<std::string> UnifiedQuestManager::ValidationModule::GetRecommendationsForFailedQuest(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetRecommendationsForFailedQuest(questId);
    return {};
}

ValidationMetrics UnifiedQuestManager::ValidationModule::GetValidationMetrics()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestValidation()->GetValidationMetrics();
    return {};
}

// ============================================================================
// TURNIN MODULE IMPLEMENTATION (delegates to QuestTurnIn)
// ============================================================================

bool UnifiedQuestManager::TurnInModule::TurnInQuestWithReward(uint32 questId, Player* bot)
{
    _questsTurnedIn++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->TurnInQuest(questId);
    return {};
}

void UnifiedQuestManager::TurnInModule::ProcessQuestTurnIn(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ProcessQuestTurnIn(questId);
}

void UnifiedQuestManager::TurnInModule::ProcessBatchTurnIn(Player* bot, const TurnInBatch& batch)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ProcessBatchTurnIn(batch);
}

void UnifiedQuestManager::TurnInModule::ScheduleQuestTurnIn(Player* bot, uint32 questId, uint32 delayMs)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ScheduleQuestTurnIn(questId, delayMs);
}

std::vector<uint32> UnifiedQuestManager::TurnInModule::GetCompletedQuests(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetCompletedQuests();
    return {};
}

bool UnifiedQuestManager::TurnInModule::IsQuestReadyForTurnIn(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->IsQuestReadyForTurnIn(questId);
    return {};
}

void UnifiedQuestManager::TurnInModule::MonitorQuestCompletion(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->MonitorQuestCompletion();
}

void UnifiedQuestManager::TurnInModule::HandleQuestCompletion(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->HandleQuestCompletion(questId);
}

void UnifiedQuestManager::TurnInModule::PlanOptimalTurnInRoute(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->PlanOptimalTurnInRoute();
}

TurnInBatch UnifiedQuestManager::TurnInModule::CreateTurnInBatch(Player* bot, const std::vector<uint32>& questIds)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->CreateTurnInBatch(questIds);
    return {};
}

void UnifiedQuestManager::TurnInModule::OptimizeTurnInSequence(Player* bot, std::vector<QuestTurnInData>& turnIns)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->OptimizeTurnInSequence(turnIns);
}

void UnifiedQuestManager::TurnInModule::MinimizeTurnInTravel(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->MinimizeTurnInTravel();
}

bool UnifiedQuestManager::TurnInModule::FindQuestTurnInNpc(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->FindQuestTurnInNpc(questId);
    return {};
}

Position UnifiedQuestManager::TurnInModule::GetQuestTurnInLocation(uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetQuestTurnInLocation(questId);
    return {};
}

bool UnifiedQuestManager::TurnInModule::NavigateToQuestGiver(Player* bot, uint32 questGiverGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->NavigateToQuestGiver(questGiverGuid);
    return {};
}

bool UnifiedQuestManager::TurnInModule::IsAtQuestGiver(Player* bot, uint32 questGiverGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->IsAtQuestGiver(questGiverGuid);
    return {};
}

void UnifiedQuestManager::TurnInModule::AnalyzeQuestRewards(QuestTurnInData& turnInData, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->AnalyzeQuestRewards(turnInData);
}

uint32 UnifiedQuestManager::TurnInModule::SelectOptimalReward(const std::vector<QuestRewardItem>& rewards, Player* bot, RewardSelectionStrategy strategy)
{
    _rewardsSelected++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->SelectOptimalReward(rewards, strategy);
    return {};
}

void UnifiedQuestManager::TurnInModule::EvaluateItemUpgrades(const std::vector<QuestRewardItem>& rewards, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->EvaluateItemUpgrades(rewards);
}

float UnifiedQuestManager::TurnInModule::CalculateItemValue(const QuestRewardItem& reward, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->CalculateItemValue(reward);
    return {};
}

void UnifiedQuestManager::TurnInModule::CoordinateGroupTurnIns(Group* group)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->CoordinateGroupTurnIns(group);
}

void UnifiedQuestManager::TurnInModule::SynchronizeGroupRewardSelection(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->SynchronizeGroupRewardSelection(group, questId);
}

void UnifiedQuestManager::TurnInModule::HandleGroupTurnInConflicts(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->HandleGroupTurnInConflicts(group, questId);
}

void UnifiedQuestManager::TurnInModule::ShareTurnInProgress(Group* group)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ShareTurnInProgress(group);
}

void UnifiedQuestManager::TurnInModule::HandleQuestGiverDialog(Player* bot, uint32 questGiverGuid, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->HandleQuestGiverDialog(questGiverGuid, questId);
}

void UnifiedQuestManager::TurnInModule::SelectQuestReward(Player* bot, uint32 questId, uint32 rewardIndex)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->SelectQuestReward(questId, rewardIndex);
}

void UnifiedQuestManager::TurnInModule::ConfirmQuestTurnIn(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ConfirmQuestTurnIn(questId);
}

void UnifiedQuestManager::TurnInModule::HandleTurnInDialog(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->HandleTurnInDialog(questId);
}

void UnifiedQuestManager::TurnInModule::ExecuteImmediateTurnInStrategy(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ExecuteImmediateTurnInStrategy();
}

void UnifiedQuestManager::TurnInModule::ExecuteBatchTurnInStrategy(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ExecuteBatchTurnInStrategy();
}

void UnifiedQuestManager::TurnInModule::ExecuteOptimalRoutingStrategy(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ExecuteOptimalRoutingStrategy();
}

void UnifiedQuestManager::TurnInModule::ExecuteGroupCoordinationStrategy(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ExecuteGroupCoordinationStrategy();
}

void UnifiedQuestManager::TurnInModule::ExecuteRewardOptimizationStrategy(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ExecuteRewardOptimizationStrategy();
}

void UnifiedQuestManager::TurnInModule::ExecuteChainContinuationStrategy(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ExecuteChainContinuationStrategy();
}

void UnifiedQuestManager::TurnInModule::HandleQuestChainProgression(Player* bot, uint32 completedQuestId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->HandleQuestChainProgression(completedQuestId);
}

uint32 UnifiedQuestManager::TurnInModule::GetNextQuestInChain(uint32 completedQuestId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetNextQuestInChain(completedQuestId);
    return {};
}

void UnifiedQuestManager::TurnInModule::AutoAcceptFollowUpQuests(Player* bot, uint32 completedQuestId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->AutoAcceptFollowUpQuests(completedQuestId);
}

void UnifiedQuestManager::TurnInModule::PrioritizeChainQuests(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->PrioritizeChainQuests();
}

void UnifiedQuestManager::TurnInModule::SetTurnInStrategy(uint32 botGuid, TurnInStrategy strategy)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->SetTurnInStrategy(botGuid, strategy);
}

TurnInStrategy UnifiedQuestManager::TurnInModule::GetTurnInStrategy(uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetTurnInStrategy(botGuid);
    return {};
}

void UnifiedQuestManager::TurnInModule::SetRewardSelectionStrategy(uint32 botGuid, RewardSelectionStrategy strategy)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->SetRewardSelectionStrategy(botGuid, strategy);
}

RewardSelectionStrategy UnifiedQuestManager::TurnInModule::GetRewardSelectionStrategy(uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetRewardSelectionStrategy(botGuid);
    return {};
}

void UnifiedQuestManager::TurnInModule::SetBatchTurnInThreshold(uint32 botGuid, uint32 threshold)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->SetBatchTurnInThreshold(botGuid, threshold);
}

void UnifiedQuestManager::TurnInModule::HandleTurnInError(Player* bot, uint32 questId, const std::string& error)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->HandleTurnInError(questId, error);
}

void UnifiedQuestManager::TurnInModule::RecoverFromTurnInFailure(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->RecoverFromTurnInFailure(questId);
}

void UnifiedQuestManager::TurnInModule::RetryFailedTurnIn(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->RetryFailedTurnIn(questId);
}

void UnifiedQuestManager::TurnInModule::ValidateTurnInState(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ValidateTurnInState(questId);
}

TurnInMetrics UnifiedQuestManager::TurnInModule::GetBotTurnInMetrics(uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetBotTurnInMetrics(botGuid);
    return {};
}

TurnInMetrics UnifiedQuestManager::TurnInModule::GetGlobalTurnInMetrics()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->GetGlobalTurnInMetrics();
    return {};
}

// ============================================================================
// DYNAMIC MODULE IMPLEMENTATION (delegates to DynamicQuestSystem)
// ============================================================================

std::vector<uint32> UnifiedQuestManager::DynamicModule::DiscoverAvailableQuests(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->DiscoverAvailableQuests();
    return {};
}

std::vector<uint32> UnifiedQuestManager::DynamicModule::GetRecommendedQuests(Player* bot, QuestStrategy strategy)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetRecommendedQuests(strategy);
    return {};
}

bool UnifiedQuestManager::DynamicModule::AssignQuestToBot(uint32 questId, Player* bot)
{
    _questsAssigned++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->AssignQuestToBot(questId);
    return {};
}

void UnifiedQuestManager::DynamicModule::AutoAssignQuests(Player* bot, uint32 maxQuests)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->AutoAssignQuests(maxQuests);
}

QuestPriority UnifiedQuestManager::DynamicModule::CalculateQuestPriority(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->CalculateQuestPriority(questId);
    return {};
}

std::vector<uint32> UnifiedQuestManager::DynamicModule::SortQuestsByPriority(const std::vector<uint32>& questIds, Player* bot)
{
    _questsOptimized++;
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->SortQuestsByPriority(questIds);
    return {};
}

bool UnifiedQuestManager::DynamicModule::ShouldAbandonQuest(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->ShouldAbandonQuest(questId);
    return {};
}

void UnifiedQuestManager::DynamicModule::UpdateQuestProgressDynamic(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->UpdateQuestProgress();
}

void UnifiedQuestManager::DynamicModule::ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->ExecuteQuestObjective(questId, objectiveIndex);
}

bool UnifiedQuestManager::DynamicModule::CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->CanCompleteQuestObjective(questId, objectiveIndex);
    return {};
}

void UnifiedQuestManager::DynamicModule::HandleQuestCompletionDynamic(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->HandleQuestCompletion(questId);
}

bool UnifiedQuestManager::DynamicModule::FormQuestGroup(uint32 questId, Player* initiator)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->FormQuestGroup(questId, initiator);
    return {};
}

void UnifiedQuestManager::DynamicModule::CoordinateGroupQuest(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->CoordinateGroupQuest(group, questId);
}

void UnifiedQuestManager::DynamicModule::ShareQuestProgress(Group* group, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->ShareQuestProgress(group, questId);
}

bool UnifiedQuestManager::DynamicModule::CanShareQuest(uint32 questId, Player* from, Player* to)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->CanShareQuest(questId, from, to);
    return {};
}

Position UnifiedQuestManager::DynamicModule::GetNextQuestLocation(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetNextQuestLocation(questId);
    return {};
}

std::vector<Position> UnifiedQuestManager::DynamicModule::GenerateQuestPath(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GenerateQuestPath(questId);
    return {};
}

void UnifiedQuestManager::DynamicModule::HandleQuestNavigation(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->HandleQuestNavigation(questId);
}

bool UnifiedQuestManager::DynamicModule::IsQuestLocationReachable(Player* bot, const Position& location)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->IsQuestLocationReachable(location);
    return {};
}

void UnifiedQuestManager::DynamicModule::AdaptQuestDifficulty(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->AdaptQuestDifficulty(questId);
}

void UnifiedQuestManager::DynamicModule::HandleQuestStuckState(Player* bot, uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->HandleQuestStuckState(questId);
}

void UnifiedQuestManager::DynamicModule::RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->RetryFailedObjective(questId, objectiveIndex);
}

void UnifiedQuestManager::DynamicModule::OptimizeQuestOrder(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->OptimizeQuestOrder();
}

void UnifiedQuestManager::DynamicModule::TrackQuestChains(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->TrackQuestChains();
}

std::vector<uint32> UnifiedQuestManager::DynamicModule::GetQuestChain(uint32 questId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetQuestChain(questId);
    return {};
}

uint32 UnifiedQuestManager::DynamicModule::GetNextQuestInChainDynamic(uint32 completedQuestId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetNextQuestInChain(completedQuestId);
    return {};
}

void UnifiedQuestManager::DynamicModule::AdvanceQuestChain(Player* bot, uint32 completedQuestId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->AdvanceQuestChain(completedQuestId);
}

void UnifiedQuestManager::DynamicModule::OptimizeZoneQuests(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->OptimizeZoneQuests();
}

std::vector<uint32> UnifiedQuestManager::DynamicModule::GetZoneQuests(uint32 zoneId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetZoneQuests(zoneId);
    return {};
}

void UnifiedQuestManager::DynamicModule::PlanZoneCompletion(Player* bot, uint32 zoneId)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->PlanZoneCompletion(zoneId);
}

bool UnifiedQuestManager::DynamicModule::ShouldMoveToNewZone(Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->ShouldMoveToNewZone();
    return {};
}

QuestReward UnifiedQuestManager::DynamicModule::AnalyzeQuestReward(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->AnalyzeQuestReward(questId);
    return {};
}

float UnifiedQuestManager::DynamicModule::CalculateQuestValue(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->CalculateQuestValue(questId);
    return {};
}

bool UnifiedQuestManager::DynamicModule::IsQuestWorthwhile(uint32 questId, Player* bot)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->IsQuestWorthwhile(questId);
    return {};
}

void UnifiedQuestManager::DynamicModule::SetQuestStrategy(uint32 botGuid, QuestStrategy strategy)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->SetQuestStrategy(botGuid, strategy);
}

QuestStrategy UnifiedQuestManager::DynamicModule::GetQuestStrategy(uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetQuestStrategy(botGuid);
    return {};
}

void UnifiedQuestManager::DynamicModule::SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->SetMaxConcurrentQuests(botGuid, maxQuests);
}

void UnifiedQuestManager::DynamicModule::EnableQuestGrouping(uint32 botGuid, bool enable)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->EnableQuestGrouping(botGuid, enable);
}

QuestMetrics UnifiedQuestManager::DynamicModule::GetBotQuestMetrics(uint32 botGuid)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetBotQuestMetrics(botGuid);
    return {};
}

QuestMetrics UnifiedQuestManager::DynamicModule::GetGlobalQuestMetrics()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetDynamicQuestSystem()->GetGlobalQuestMetrics();
    return {};
}

// ============================================================================
// UNIFIED QUEST MANAGER INTERFACE DELEGATION
// ============================================================================

// Pickup Module delegation
bool UnifiedQuestManager::PickupQuest(uint32 questId, Player* bot, uint32 questGiverGuid)
{ return _pickup->PickupQuest(questId, bot, questGiverGuid); }

bool UnifiedQuestManager::PickupQuestFromGiver(Player* bot, uint32 questGiverGuid, uint32 questId)
{ return _pickup->PickupQuestFromGiver(bot, questGiverGuid, questId); }

void UnifiedQuestManager::PickupAvailableQuests(Player* bot)
{ _pickup->PickupAvailableQuests(bot); }

void UnifiedQuestManager::PickupQuestsInArea(Player* bot, float radius)
{ _pickup->PickupQuestsInArea(bot, radius); }

std::vector<uint32> UnifiedQuestManager::DiscoverNearbyQuests(Player* bot, float scanRadius)
{ return _pickup->DiscoverNearbyQuests(bot, scanRadius); }

std::vector<QuestGiverInfo> UnifiedQuestManager::ScanForQuestGivers(Player* bot, float scanRadius)
{ return _pickup->ScanForQuestGivers(bot, scanRadius); }

std::vector<uint32> UnifiedQuestManager::GetAvailableQuestsFromGiver(uint32 questGiverGuid, Player* bot)
{ return _pickup->GetAvailableQuestsFromGiver(questGiverGuid, bot); }

QuestEligibility UnifiedQuestManager::CheckQuestEligibility(uint32 questId, Player* bot)
{ return _pickup->CheckQuestEligibility(questId, bot); }

bool UnifiedQuestManager::CanAcceptQuest(uint32 questId, Player* bot)
{ return _pickup->CanAcceptQuest(questId, bot); }

bool UnifiedQuestManager::MeetsQuestRequirements(uint32 questId, Player* bot)
{ return _pickup->MeetsQuestRequirements(questId, bot); }

std::vector<uint32> UnifiedQuestManager::FilterQuests(const std::vector<uint32>& questIds, Player* bot, const QuestPickupFilter& filter)
{ return _pickup->FilterQuests(questIds, bot, filter); }

std::vector<uint32> UnifiedQuestManager::PrioritizeQuests(const std::vector<uint32>& questIds, Player* bot, QuestAcceptanceStrategy strategy)
{ return _pickup->PrioritizeQuests(questIds, bot, strategy); }

bool UnifiedQuestManager::ShouldAcceptQuest(uint32 questId, Player* bot)
{ return _pickup->ShouldAcceptQuest(questId, bot); }

// Completion Module delegation
bool UnifiedQuestManager::StartQuestCompletion(uint32 questId, Player* bot)
{ return _completion->StartQuestCompletion(questId, bot); }

void UnifiedQuestManager::UpdateQuestProgress(Player* bot)
{ _completion->UpdateQuestProgress(bot); }

void UnifiedQuestManager::CompleteQuest(uint32 questId, Player* bot)
{ _completion->CompleteQuest(questId, bot); }

bool UnifiedQuestManager::TurnInQuest(uint32 questId, Player* bot)
{ return _completion->TurnInQuest(questId, bot); }

void UnifiedQuestManager::TrackQuestObjectives(Player* bot)
{ _completion->TrackQuestObjectives(bot); }

void UnifiedQuestManager::ExecuteObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->ExecuteObjective(bot, objective); }

void UnifiedQuestManager::UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex)
{ _completion->UpdateObjectiveProgress(bot, questId, objectiveIndex); }

bool UnifiedQuestManager::IsObjectiveComplete(const QuestObjectiveData& objective)
{ return _completion->IsObjectiveComplete(objective); }

void UnifiedQuestManager::HandleKillObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleKillObjective(bot, objective); }

void UnifiedQuestManager::HandleCollectObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleCollectObjective(bot, objective); }

void UnifiedQuestManager::HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleTalkToNpcObjective(bot, objective); }

void UnifiedQuestManager::HandleLocationObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleLocationObjective(bot, objective); }

void UnifiedQuestManager::HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleGameObjectObjective(bot, objective); }

void UnifiedQuestManager::HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleSpellCastObjective(bot, objective); }

void UnifiedQuestManager::HandleEmoteObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleEmoteObjective(bot, objective); }

void UnifiedQuestManager::HandleEscortObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleEscortObjective(bot, objective); }

void UnifiedQuestManager::NavigateToObjective(Player* bot, const QuestObjectiveData& objective)
{ _completion->NavigateToObjective(bot, objective); }

bool UnifiedQuestManager::FindObjectiveTarget(Player* bot, QuestObjectiveData& objective)
{ return _completion->FindObjectiveTarget(bot, objective); }

std::vector<Position> UnifiedQuestManager::GetObjectiveLocations(const QuestObjectiveData& objective)
{ return _completion->GetObjectiveLocations(objective); }

Position UnifiedQuestManager::GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective)
{ return _completion->GetOptimalObjectivePosition(bot, objective); }

void UnifiedQuestManager::CoordinateGroupQuestCompletion(Group* group, uint32 questId)
{ _completion->CoordinateGroupQuestCompletion(group, questId); }

void UnifiedQuestManager::ShareObjectiveProgress(Group* group, uint32 questId)
{ _completion->ShareObjectiveProgress(group, questId); }

void UnifiedQuestManager::SynchronizeGroupObjectives(Group* group, uint32 questId)
{ _completion->SynchronizeGroupObjectives(group, questId); }

void UnifiedQuestManager::HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex)
{ _completion->HandleGroupObjectiveConflict(group, questId, objectiveIndex); }

void UnifiedQuestManager::OptimizeQuestCompletionOrder(Player* bot)
{ _completion->OptimizeQuestCompletionOrder(bot); }

void UnifiedQuestManager::OptimizeObjectiveSequence(Player* bot, uint32 questId)
{ _completion->OptimizeObjectiveSequence(bot, questId); }

void UnifiedQuestManager::FindEfficientCompletionPath(Player* bot, const std::vector<uint32>& questIds)
{ _completion->FindEfficientCompletionPath(bot, questIds); }

void UnifiedQuestManager::MinimizeTravelTime(Player* bot, const std::vector<QuestObjectiveData>& objectives)
{ _completion->MinimizeTravelTime(bot, objectives); }

void UnifiedQuestManager::DetectStuckState(Player* bot, uint32 questId)
{ _completion->DetectStuckState(bot, questId); }

void UnifiedQuestManager::HandleStuckObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->HandleStuckObjective(bot, objective); }

void UnifiedQuestManager::RecoverFromStuckState(Player* bot, uint32 questId)
{ _completion->RecoverFromStuckState(bot, questId); }

void UnifiedQuestManager::SkipProblematicObjective(Player* bot, QuestObjectiveData& objective)
{ _completion->SkipProblematicObjective(bot, objective); }

// Validation Module delegation (abbreviated due to length - pattern continues)
bool UnifiedQuestManager::ValidateQuest(uint32 questId, Player* bot)
{ return _validation->ValidateQuest(questId, bot); }

bool UnifiedQuestManager::ValidateQuestRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateQuestRequirements(questId, bot); }

std::vector<std::string> UnifiedQuestManager::GetValidationErrors(uint32 questId, Player* bot)
{ return _validation->GetValidationErrors(questId, bot); }

bool UnifiedQuestManager::ValidateLevelRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateLevelRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateClassRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateClassRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateRaceRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateRaceRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateSkillRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateSkillRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateQuestPrerequisites(uint32 questId, Player* bot)
{ return _validation->ValidateQuestPrerequisites(questId, bot); }

bool UnifiedQuestManager::ValidateQuestChain(uint32 questId, Player* bot)
{ return _validation->ValidateQuestChain(questId, bot); }

bool UnifiedQuestManager::HasCompletedPrerequisites(uint32 questId, Player* bot)
{ return _validation->HasCompletedPrerequisites(questId, bot); }

std::vector<uint32> UnifiedQuestManager::GetMissingPrerequisites(uint32 questId, Player* bot)
{ return _validation->GetMissingPrerequisites(questId, bot); }

bool UnifiedQuestManager::ValidateReputationRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateReputationRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateFactionRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateFactionRequirements(questId, bot); }

bool UnifiedQuestManager::HasRequiredReputation(uint32 questId, Player* bot, uint32 factionId)
{ return _validation->HasRequiredReputation(questId, bot, factionId); }

bool UnifiedQuestManager::ValidateItemRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateItemRequirements(questId, bot); }

bool UnifiedQuestManager::HasRequiredItems(uint32 questId, Player* bot)
{ return _validation->HasRequiredItems(questId, bot); }

bool UnifiedQuestManager::HasInventorySpace(uint32 questId, Player* bot)
{ return _validation->HasInventorySpace(questId, bot); }

std::vector<uint32> UnifiedQuestManager::GetMissingQuestItems(uint32 questId, Player* bot)
{ return _validation->GetMissingQuestItems(questId, bot); }

bool UnifiedQuestManager::ValidateQuestAvailability(uint32 questId, Player* bot)
{ return _validation->ValidateQuestAvailability(questId, bot); }

bool UnifiedQuestManager::ValidateSeasonalAvailability(uint32 questId)
{ return _validation->ValidateSeasonalAvailability(questId); }

bool UnifiedQuestManager::ValidateDailyQuestLimits(uint32 questId, Player* bot)
{ return _validation->ValidateDailyQuestLimits(questId, bot); }

bool UnifiedQuestManager::ValidateQuestTimer(uint32 questId, Player* bot)
{ return _validation->ValidateQuestTimer(questId, bot); }

bool UnifiedQuestManager::ValidateZoneRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateZoneRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateAreaRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateAreaRequirements(questId, bot); }

bool UnifiedQuestManager::IsInCorrectZone(uint32 questId, Player* bot)
{ return _validation->IsInCorrectZone(questId, bot); }

bool UnifiedQuestManager::CanQuestBeStartedAtLocation(uint32 questId, const Position& location)
{ return _validation->CanQuestBeStartedAtLocation(questId, location); }

bool UnifiedQuestManager::ValidateGroupRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateGroupRequirements(questId, bot); }

bool UnifiedQuestManager::ValidatePartyQuestRequirements(uint32 questId, Player* bot)
{ return _validation->ValidatePartyQuestRequirements(questId, bot); }

bool UnifiedQuestManager::ValidateRaidQuestRequirements(uint32 questId, Player* bot)
{ return _validation->ValidateRaidQuestRequirements(questId, bot); }

bool UnifiedQuestManager::CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver)
{ return _validation->CanGroupMemberShareQuest(questId, sharer, receiver); }

bool UnifiedQuestManager::ValidateWithContext(ValidationContext& context)
{ return _validation->ValidateWithContext(context); }

bool UnifiedQuestManager::ValidateQuestObjectives(uint32 questId, Player* bot)
{ return _validation->ValidateQuestObjectives(questId, bot); }

bool UnifiedQuestManager::ValidateQuestRewards(uint32 questId, Player* bot)
{ return _validation->ValidateQuestRewards(questId, bot); }

bool UnifiedQuestManager::ValidateQuestDifficulty(uint32 questId, Player* bot)
{ return _validation->ValidateQuestDifficulty(questId, bot); }

ValidationResult UnifiedQuestManager::GetCachedValidation(uint32 questId, uint32 botGuid)
{ return _validation->GetCachedValidation(questId, botGuid); }

void UnifiedQuestManager::CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result)
{ _validation->CacheValidationResult(questId, botGuid, result); }

void UnifiedQuestManager::InvalidateValidationCache(uint32 botGuid)
{ _validation->InvalidateValidationCache(botGuid); }

void UnifiedQuestManager::CleanupExpiredCache()
{ _validation->CleanupExpiredCache(); }

std::unordered_map<uint32, ValidationResult> UnifiedQuestManager::ValidateMultipleQuests(
    const std::vector<uint32>& questIds, Player* bot)
{ return _validation->ValidateMultipleQuests(questIds, bot); }

std::vector<uint32> UnifiedQuestManager::FilterValidQuests(const std::vector<uint32>& questIds, Player* bot)
{ return _validation->FilterValidQuests(questIds, bot); }

std::vector<uint32> UnifiedQuestManager::GetEligibleQuests(Player* bot, const std::vector<uint32>& candidates)
{ return _validation->GetEligibleQuests(bot, candidates); }

std::string UnifiedQuestManager::GetDetailedValidationReport(uint32 questId, Player* bot)
{ return _validation->GetDetailedValidationReport(questId, bot); }

void UnifiedQuestManager::LogValidationFailure(uint32 questId, Player* bot, const std::string& reason)
{ _validation->LogValidationFailure(questId, bot, reason); }

std::vector<std::string> UnifiedQuestManager::GetRecommendationsForFailedQuest(uint32 questId, Player* bot)
{ return _validation->GetRecommendationsForFailedQuest(questId, bot); }

// TurnIn Module delegation (continuing pattern)
bool UnifiedQuestManager::TurnInQuestWithReward(uint32 questId, Player* bot)
{ return _turnIn->TurnInQuestWithReward(questId, bot); }

void UnifiedQuestManager::ProcessQuestTurnIn(Player* bot, uint32 questId)
{ _turnIn->ProcessQuestTurnIn(bot, questId); }

void UnifiedQuestManager::ProcessBatchTurnIn(Player* bot, const TurnInBatch& batch)
{ _turnIn->ProcessBatchTurnIn(bot, batch); }

void UnifiedQuestManager::ScheduleQuestTurnIn(Player* bot, uint32 questId, uint32 delayMs)
{ _turnIn->ScheduleQuestTurnIn(bot, questId, delayMs); }

std::vector<uint32> UnifiedQuestManager::GetCompletedQuests(Player* bot)
{ return _turnIn->GetCompletedQuests(bot); }

bool UnifiedQuestManager::IsQuestReadyForTurnIn(uint32 questId, Player* bot)
{ return _turnIn->IsQuestReadyForTurnIn(questId, bot); }

void UnifiedQuestManager::MonitorQuestCompletion(Player* bot)
{ _turnIn->MonitorQuestCompletion(bot); }

void UnifiedQuestManager::HandleQuestCompletion(Player* bot, uint32 questId)
{ _turnIn->HandleQuestCompletion(bot, questId); }

void UnifiedQuestManager::PlanOptimalTurnInRoute(Player* bot)
{ _turnIn->PlanOptimalTurnInRoute(bot); }

TurnInBatch UnifiedQuestManager::CreateTurnInBatch(Player* bot, const std::vector<uint32>& questIds)
{ return _turnIn->CreateTurnInBatch(bot, questIds); }

void UnifiedQuestManager::OptimizeTurnInSequence(Player* bot, std::vector<QuestTurnInData>& turnIns)
{ _turnIn->OptimizeTurnInSequence(bot, turnIns); }

void UnifiedQuestManager::MinimizeTurnInTravel(Player* bot)
{ _turnIn->MinimizeTurnInTravel(bot); }

bool UnifiedQuestManager::FindQuestTurnInNpc(Player* bot, uint32 questId)
{ return _turnIn->FindQuestTurnInNpc(bot, questId); }

Position UnifiedQuestManager::GetQuestTurnInLocation(uint32 questId)
{ return _turnIn->GetQuestTurnInLocation(questId); }

bool UnifiedQuestManager::NavigateToQuestGiver(Player* bot, uint32 questGiverGuid)
{ return _turnIn->NavigateToQuestGiver(bot, questGiverGuid); }

bool UnifiedQuestManager::IsAtQuestGiver(Player* bot, uint32 questGiverGuid)
{ return _turnIn->IsAtQuestGiver(bot, questGiverGuid); }

void UnifiedQuestManager::AnalyzeQuestRewards(QuestTurnInData& turnInData, Player* bot)
{ _turnIn->AnalyzeQuestRewards(turnInData, bot); }

uint32 UnifiedQuestManager::SelectOptimalReward(const std::vector<QuestRewardItem>& rewards, Player* bot, RewardSelectionStrategy strategy)
{ return _turnIn->SelectOptimalReward(rewards, bot, strategy); }

void UnifiedQuestManager::EvaluateItemUpgrades(const std::vector<QuestRewardItem>& rewards, Player* bot)
{ _turnIn->EvaluateItemUpgrades(rewards, bot); }

float UnifiedQuestManager::CalculateItemValue(const QuestRewardItem& reward, Player* bot)
{ return _turnIn->CalculateItemValue(reward, bot); }

void UnifiedQuestManager::CoordinateGroupTurnIns(Group* group)
{ _turnIn->CoordinateGroupTurnIns(group); }

void UnifiedQuestManager::SynchronizeGroupRewardSelection(Group* group, uint32 questId)
{ _turnIn->SynchronizeGroupRewardSelection(group, questId); }

void UnifiedQuestManager::HandleGroupTurnInConflicts(Group* group, uint32 questId)
{ _turnIn->HandleGroupTurnInConflicts(group, questId); }

void UnifiedQuestManager::ShareTurnInProgress(Group* group)
{ _turnIn->ShareTurnInProgress(group); }

void UnifiedQuestManager::HandleQuestGiverDialog(Player* bot, uint32 questGiverGuid, uint32 questId)
{ _turnIn->HandleQuestGiverDialog(bot, questGiverGuid, questId); }

void UnifiedQuestManager::SelectQuestReward(Player* bot, uint32 questId, uint32 rewardIndex)
{ _turnIn->SelectQuestReward(bot, questId, rewardIndex); }

void UnifiedQuestManager::ConfirmQuestTurnIn(Player* bot, uint32 questId)
{ _turnIn->ConfirmQuestTurnIn(bot, questId); }

void UnifiedQuestManager::HandleTurnInDialog(Player* bot, uint32 questId)
{ _turnIn->HandleTurnInDialog(bot, questId); }

void UnifiedQuestManager::ExecuteImmediateTurnInStrategy(Player* bot)
{ _turnIn->ExecuteImmediateTurnInStrategy(bot); }

void UnifiedQuestManager::ExecuteBatchTurnInStrategy(Player* bot)
{ _turnIn->ExecuteBatchTurnInStrategy(bot); }

void UnifiedQuestManager::ExecuteOptimalRoutingStrategy(Player* bot)
{ _turnIn->ExecuteOptimalRoutingStrategy(bot); }

void UnifiedQuestManager::ExecuteGroupCoordinationStrategy(Player* bot)
{ _turnIn->ExecuteGroupCoordinationStrategy(bot); }

void UnifiedQuestManager::ExecuteRewardOptimizationStrategy(Player* bot)
{ _turnIn->ExecuteRewardOptimizationStrategy(bot); }

void UnifiedQuestManager::ExecuteChainContinuationStrategy(Player* bot)
{ _turnIn->ExecuteChainContinuationStrategy(bot); }

void UnifiedQuestManager::HandleQuestChainProgression(Player* bot, uint32 completedQuestId)
{ _turnIn->HandleQuestChainProgression(bot, completedQuestId); }

uint32 UnifiedQuestManager::GetNextQuestInChain(uint32 completedQuestId)
{ return _turnIn->GetNextQuestInChain(completedQuestId); }

void UnifiedQuestManager::AutoAcceptFollowUpQuests(Player* bot, uint32 completedQuestId)
{ _turnIn->AutoAcceptFollowUpQuests(bot, completedQuestId); }

void UnifiedQuestManager::PrioritizeChainQuests(Player* bot)
{ _turnIn->PrioritizeChainQuests(bot); }

void UnifiedQuestManager::SetTurnInStrategy(uint32 botGuid, TurnInStrategy strategy)
{ _turnIn->SetTurnInStrategy(botGuid, strategy); }

TurnInStrategy UnifiedQuestManager::GetTurnInStrategy(uint32 botGuid)
{ return _turnIn->GetTurnInStrategy(botGuid); }

void UnifiedQuestManager::SetRewardSelectionStrategy(uint32 botGuid, RewardSelectionStrategy strategy)
{ _turnIn->SetRewardSelectionStrategy(botGuid, strategy); }

RewardSelectionStrategy UnifiedQuestManager::GetRewardSelectionStrategy(uint32 botGuid)
{ return _turnIn->GetRewardSelectionStrategy(botGuid); }

void UnifiedQuestManager::SetBatchTurnInThreshold(uint32 botGuid, uint32 threshold)
{ _turnIn->SetBatchTurnInThreshold(botGuid, threshold); }

void UnifiedQuestManager::HandleTurnInError(Player* bot, uint32 questId, const std::string& error)
{ _turnIn->HandleTurnInError(bot, questId, error); }

void UnifiedQuestManager::RecoverFromTurnInFailure(Player* bot, uint32 questId)
{ _turnIn->RecoverFromTurnInFailure(bot, questId); }

void UnifiedQuestManager::RetryFailedTurnIn(Player* bot, uint32 questId)
{ _turnIn->RetryFailedTurnIn(bot, questId); }

void UnifiedQuestManager::ValidateTurnInState(Player* bot, uint32 questId)
{ _turnIn->ValidateTurnInState(bot, questId); }

// Dynamic Module delegation (continuing pattern)
std::vector<uint32> UnifiedQuestManager::DiscoverAvailableQuests(Player* bot)
{ return _dynamic->DiscoverAvailableQuests(bot); }

std::vector<uint32> UnifiedQuestManager::GetRecommendedQuests(Player* bot, QuestStrategy strategy)
{ return _dynamic->GetRecommendedQuests(bot, strategy); }

bool UnifiedQuestManager::AssignQuestToBot(uint32 questId, Player* bot)
{ return _dynamic->AssignQuestToBot(questId, bot); }

void UnifiedQuestManager::AutoAssignQuests(Player* bot, uint32 maxQuests)
{ _dynamic->AutoAssignQuests(bot, maxQuests); }

QuestPriority UnifiedQuestManager::CalculateQuestPriority(uint32 questId, Player* bot)
{ return _dynamic->CalculateQuestPriority(questId, bot); }

std::vector<uint32> UnifiedQuestManager::SortQuestsByPriority(const std::vector<uint32>& questIds, Player* bot)
{ return _dynamic->SortQuestsByPriority(questIds, bot); }

bool UnifiedQuestManager::ShouldAbandonQuest(uint32 questId, Player* bot)
{ return _dynamic->ShouldAbandonQuest(questId, bot); }

void UnifiedQuestManager::UpdateQuestProgressDynamic(Player* bot)
{ _dynamic->UpdateQuestProgressDynamic(bot); }

void UnifiedQuestManager::ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex)
{ _dynamic->ExecuteQuestObjective(bot, questId, objectiveIndex); }

bool UnifiedQuestManager::CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex)
{ return _dynamic->CanCompleteQuestObjective(bot, questId, objectiveIndex); }

void UnifiedQuestManager::HandleQuestCompletionDynamic(Player* bot, uint32 questId)
{ _dynamic->HandleQuestCompletionDynamic(bot, questId); }

bool UnifiedQuestManager::FormQuestGroup(uint32 questId, Player* initiator)
{ return _dynamic->FormQuestGroup(questId, initiator); }

void UnifiedQuestManager::CoordinateGroupQuest(Group* group, uint32 questId)
{ _dynamic->CoordinateGroupQuest(group, questId); }

void UnifiedQuestManager::ShareQuestProgress(Group* group, uint32 questId)
{ _dynamic->ShareQuestProgress(group, questId); }

bool UnifiedQuestManager::CanShareQuest(uint32 questId, Player* from, Player* to)
{ return _dynamic->CanShareQuest(questId, from, to); }

Position UnifiedQuestManager::GetNextQuestLocation(Player* bot, uint32 questId)
{ return _dynamic->GetNextQuestLocation(bot, questId); }

std::vector<Position> UnifiedQuestManager::GenerateQuestPath(Player* bot, uint32 questId)
{ return _dynamic->GenerateQuestPath(bot, questId); }

void UnifiedQuestManager::HandleQuestNavigation(Player* bot, uint32 questId)
{ _dynamic->HandleQuestNavigation(bot, questId); }

bool UnifiedQuestManager::IsQuestLocationReachable(Player* bot, const Position& location)
{ return _dynamic->IsQuestLocationReachable(bot, location); }

void UnifiedQuestManager::AdaptQuestDifficulty(uint32 questId, Player* bot)
{ _dynamic->AdaptQuestDifficulty(questId, bot); }

void UnifiedQuestManager::HandleQuestStuckState(Player* bot, uint32 questId)
{ _dynamic->HandleQuestStuckState(bot, questId); }

void UnifiedQuestManager::RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex)
{ _dynamic->RetryFailedObjective(bot, questId, objectiveIndex); }

void UnifiedQuestManager::OptimizeQuestOrder(Player* bot)
{ _dynamic->OptimizeQuestOrder(bot); }

void UnifiedQuestManager::TrackQuestChains(Player* bot)
{ _dynamic->TrackQuestChains(bot); }

std::vector<uint32> UnifiedQuestManager::GetQuestChain(uint32 questId)
{ return _dynamic->GetQuestChain(questId); }

uint32 UnifiedQuestManager::GetNextQuestInChainDynamic(uint32 completedQuestId)
{ return _dynamic->GetNextQuestInChainDynamic(completedQuestId); }

void UnifiedQuestManager::AdvanceQuestChain(Player* bot, uint32 completedQuestId)
{ _dynamic->AdvanceQuestChain(bot, completedQuestId); }

void UnifiedQuestManager::OptimizeZoneQuests(Player* bot)
{ _dynamic->OptimizeZoneQuests(bot); }

std::vector<uint32> UnifiedQuestManager::GetZoneQuests(uint32 zoneId, Player* bot)
{ return _dynamic->GetZoneQuests(zoneId, bot); }

void UnifiedQuestManager::PlanZoneCompletion(Player* bot, uint32 zoneId)
{ _dynamic->PlanZoneCompletion(bot, zoneId); }

bool UnifiedQuestManager::ShouldMoveToNewZone(Player* bot)
{ return _dynamic->ShouldMoveToNewZone(bot); }

QuestReward UnifiedQuestManager::AnalyzeQuestReward(uint32 questId, Player* bot)
{ return _dynamic->AnalyzeQuestReward(questId, bot); }

float UnifiedQuestManager::CalculateQuestValue(uint32 questId, Player* bot)
{ return _dynamic->CalculateQuestValue(questId, bot); }

bool UnifiedQuestManager::IsQuestWorthwhile(uint32 questId, Player* bot)
{ return _dynamic->IsQuestWorthwhile(questId, bot); }

void UnifiedQuestManager::SetQuestStrategy(uint32 botGuid, QuestStrategy strategy)
{ _dynamic->SetQuestStrategy(botGuid, strategy); }

QuestStrategy UnifiedQuestManager::GetQuestStrategy(uint32 botGuid)
{ return _dynamic->GetQuestStrategy(botGuid); }

void UnifiedQuestManager::SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests)
{ _dynamic->SetMaxConcurrentQuests(botGuid, maxQuests); }

void UnifiedQuestManager::EnableQuestGrouping(uint32 botGuid, bool enable)
{ _dynamic->EnableQuestGrouping(botGuid, enable); }

// ============================================================================
// UNIFIED OPERATIONS IMPLEMENTATION
// ============================================================================

void UnifiedQuestManager::ProcessCompleteQuestFlow(Player* bot)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    auto startTime = GameTime::GetGameTimeMS();
    _totalOperations++;

    // 1. Discovery and validation
    auto availableQuests = _pickup->DiscoverNearbyQuests(bot, 100.0f);
    auto validQuests = _validation->FilterValidQuests(availableQuests, bot);

    // 2. Assignment and prioritization
    auto recommendedQuests = _dynamic->GetRecommendedQuests(bot, QuestStrategy::LEVEL_PROGRESSION);

    // 3. Execution and tracking
    _completion->UpdateQuestProgress(bot);

    // 4. Turn-in and reward selection
    auto completedQuests = _turnIn->GetCompletedQuests(bot);
    if (!completedQuests.empty())
    {
        _turnIn->ExecuteImmediateTurnInStrategy(bot);
    }

    auto endTime = GameTime::GetGameTimeMS();
    _totalProcessingTimeMs += (endTime - startTime);
}

std::string UnifiedQuestManager::GetQuestRecommendation(Player* bot, uint32 questId)
{
    std::ostringstream oss;

    // Validate eligibility
    bool isValid = _validation->ValidateQuest(questId, bot);
    auto eligibility = _pickup->CheckQuestEligibility(questId, bot);

    // Calculate priority
    auto priority = _dynamic->CalculateQuestPriority(questId, bot);

    // Analyze reward
    auto reward = _dynamic->AnalyzeQuestReward(questId, bot);
    float questValue = _dynamic->CalculateQuestValue(questId, bot);

    oss << "Quest " << questId << " Recommendation:\n";
    oss << "  Valid: " << (isValid ? "Yes" : "No") << "\n";
    oss << "  Priority: " << static_cast<uint32>(priority) << "\n";
    oss << "  Value: " << questValue << "\n";
    oss << "  Experience: " << reward.experience << "\n";
    oss << "  Gold: " << reward.gold << "\n";

    return oss.str();
}

void UnifiedQuestManager::OptimizeBotQuestLoad(Player* bot)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    // Optimize quest order
    _dynamic->OptimizeQuestOrder(bot);

    // Optimize completion paths
    _completion->OptimizeQuestCompletionOrder(bot);

    // Optimize turn-in routing
    _turnIn->PlanOptimalTurnInRoute(bot);

    // Optimize zone quests
    _dynamic->OptimizeZoneQuests(bot);
}

std::string UnifiedQuestManager::GetQuestStatistics() const
{
    std::ostringstream oss;
    oss << "=== Unified Quest Manager Statistics ===\n";
    oss << "Total Operations: " << _totalOperations.load() << "\n";
    oss << "Total Processing Time (ms): " << _totalProcessingTimeMs.load() << "\n";
    oss << "Quests Picked Up: " << _pickup->_questsPickedUp.load() << "\n";
    oss << "Quests Discovered: " << _pickup->_questsDiscovered.load() << "\n";
    oss << "Objectives Completed: " << _completion->_objectivesCompleted.load() << "\n";
    oss << "Quests Completed: " << _completion->_questsCompleted.load() << "\n";
    oss << "Validations Performed: " << _validation->_validationsPerformed.load() << "\n";
    oss << "Validations Passed: " << _validation->_validationsPassed.load() << "\n";
    oss << "Quests Turned In: " << _turnIn->_questsTurnedIn.load() << "\n";
    oss << "Rewards Selected: " << _turnIn->_rewardsSelected.load() << "\n";
    oss << "Quests Assigned: " << _dynamic->_questsAssigned.load() << "\n";
    oss << "Quests Optimized: " << _dynamic->_questsOptimized.load() << "\n";
    return oss.str();
}

QuestMetrics UnifiedQuestManager::GetBotQuestMetrics(uint32 botGuid)
{
    return _dynamic->GetBotQuestMetrics(botGuid);
}

QuestMetrics UnifiedQuestManager::GetGlobalQuestMetrics()
{
    return _dynamic->GetGlobalQuestMetrics();
}

TurnInMetrics UnifiedQuestManager::GetBotTurnInMetrics(uint32 botGuid)
{
    return _turnIn->GetBotTurnInMetrics(botGuid);
}

TurnInMetrics UnifiedQuestManager::GetGlobalTurnInMetrics()
{
    return _turnIn->GetGlobalTurnInMetrics();
}

ValidationMetrics UnifiedQuestManager::GetValidationMetrics()
{
    return _validation->GetValidationMetrics();
}

void UnifiedQuestManager::Update(uint32 diff)
{
    // Delegate updates to individual managers
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestCompletion()->Update(diff);
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->Update(diff);
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->Update(diff);
}

void UnifiedQuestManager::UpdateBotTurnIns(Player* bot, uint32 diff)
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->UpdateBotTurnIns(diff);
}

void UnifiedQuestManager::ProcessScheduledTurnIns()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ProcessScheduledTurnIns();
}

void UnifiedQuestManager::CleanupCompletedTurnIns()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->CleanupCompletedTurnIns();
}

void UnifiedQuestManager::CleanupCompletedQuests()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->CleanupCompletedQuests();
}

void UnifiedQuestManager::ValidateQuestStates()
{
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetDynamicQuestSystem()->ValidateQuestStates();
}

} // namespace Playerbot
