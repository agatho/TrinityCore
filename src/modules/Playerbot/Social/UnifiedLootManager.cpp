/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "UnifiedLootManager.h"
#include "Core/PlayerBotHelpers.h"  // GetBotAI, GetGameSystems
#include "LootDistribution.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Random.h"
#include <algorithm>
#include <sstream>
#include "GameTime.h"

// Note: LootAnalysis and LootCoordination were stub headers with no implementations.
// They have been removed during consolidation. Real implementations needed here.

namespace Playerbot
{

// ============================================================================
// Singleton Management
// ============================================================================

UnifiedLootManager* UnifiedLootManager::instance()
{
    static UnifiedLootManager instance;
    return &instance;
}

UnifiedLootManager::UnifiedLootManager()
    : _analysis(std::make_unique<AnalysisModule>())
    , _coordination(std::make_unique<CoordinationModule>())
    , _distribution(std::make_unique<DistributionModule>())
{
    TC_LOG_INFO("playerbot.loot", "UnifiedLootManager initialized");
}

UnifiedLootManager::~UnifiedLootManager()
{
    TC_LOG_INFO("playerbot.loot", "UnifiedLootManager shutting down");
}

// ============================================================================
// ANALYSIS MODULE IMPLEMENTATION
// ============================================================================

float UnifiedLootManager::AnalysisModule::CalculateItemValue(Player* player, LootItem const& item)
{
    // TODO: Implement item value calculation
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    _itemsAnalyzed++;
    return 0.0f;  // Placeholder - needs real implementation
}

float UnifiedLootManager::AnalysisModule::CalculateUpgradeValue(Player* player, LootItem const& item)
{
    // TODO: Implement upgrade value calculation
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    _upgradesDetected++;
    return 0.0f;  // Placeholder - needs real implementation
}

bool UnifiedLootManager::AnalysisModule::IsSignificantUpgrade(Player* player, LootItem const& item)
{
    // TODO: Implement upgrade significance check
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    return false;  // Placeholder - needs real implementation
}

float UnifiedLootManager::AnalysisModule::CalculateStatWeight(Player* player, uint32 statType)
{
    // TODO: Implement stat weight calculation
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    return 0.0f;  // Placeholder - needs real implementation
}

float UnifiedLootManager::AnalysisModule::CompareItems(Player* player, LootItem const& newItem, Item const* currentItem)
{
    // TODO: Implement item comparison
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    return 0.0f;  // Placeholder - needs real implementation
}

float UnifiedLootManager::AnalysisModule::CalculateItemScore(Player* player, LootItem const& item)
{
    // TODO: Implement item scoring
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    return 0.0f;  // Placeholder - needs real implementation
}

std::vector<std::pair<uint32, float>> UnifiedLootManager::AnalysisModule::GetStatPriorities(Player* player)
{
    // TODO: Implement stat priority calculation
    // LootAnalysis was a stub interface with no implementation - removed during consolidation
    return {};  // Placeholder - needs real implementation
}

// ============================================================================
// COORDINATION MODULE IMPLEMENTATION
// ============================================================================

void UnifiedLootManager::CoordinationModule::InitiateLootSession(Group* group, Loot* loot)
{
    std::lock_guard<decltype(_sessionMutex)> lock(_sessionMutex);

    // TODO: Implement loot session initiation
    // LootCoordination was a stub interface with no implementation - removed during consolidation

    // Track in unified system
    uint32 sessionId = _nextSessionId++;
    LootSession session(sessionId, group ? group->GetGUID().GetCounter() : 0);
    _activeSessions[sessionId] = session;
    _sessionsCreated++;
}

void UnifiedLootManager::CoordinationModule::ProcessLootSession(Group* group, uint32 lootSessionId)
{
    // TODO: Implement loot session processing
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::CompleteLootSession(uint32 lootSessionId)
{
    std::lock_guard<decltype(_sessionMutex)> lock(_sessionMutex);

    // TODO: Implement loot session completion
    // LootCoordination was a stub interface with no implementation - removed during consolidation

    // Clean up from unified tracking
    auto it = _activeSessions.find(lootSessionId);
    if (it != _activeSessions.end())
    {
        _activeSessions.erase(it);
        _sessionsCompleted++;
    }
}

void UnifiedLootManager::CoordinationModule::HandleLootSessionTimeout(uint32 lootSessionId)
{
    // TODO: Implement loot session timeout handling
    // LootCoordination was a stub interface with no implementation - removed during consolidation
    CompleteLootSession(lootSessionId); // Cleanup
}

void UnifiedLootManager::CoordinationModule::OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items)
{
    // TODO: Implement loot distribution orchestration
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items)
{
    // TODO: Implement loot distribution prioritization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::OptimizeLootSequence(Group* group, std::vector<LootItem>& items)
{
    // TODO: Implement loot sequence optimization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::FacilitateGroupLootDiscussion(Group* group, LootItem const& item)
{
    // TODO: Implement group loot discussion facilitation
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::HandleLootConflictResolution(Group* group, LootItem const& item)
{
    // TODO: Implement loot conflict resolution
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::BroadcastLootRecommendations(Group* group, LootItem const& item)
{
    // TODO: Implement loot recommendation broadcasting
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::OptimizeLootEfficiency(Group* group)
{
    // TODO: Implement loot efficiency optimization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::MinimizeLootTime(Group* group, uint32 sessionId)
{
    // TODO: Implement loot time minimization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::MaximizeLootFairness(Group* group, uint32 sessionId)
{
    // TODO: Implement loot fairness maximization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

// ============================================================================
// DISTRIBUTION MODULE IMPLEMENTATION
// ============================================================================

void UnifiedLootManager::DistributionModule::DistributeLoot(Group* group, LootItem const& item)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "DistributeLoot called with null group");
        return;
    }

    LootMethod method = group->GetLootMethod();

    TC_LOG_DEBUG("playerbot.loot", "Distributing loot item {} (id: {}) for group {} using method {}",
        item.itemName, item.itemId, group->GetGUID().GetCounter(), static_cast<uint32>(method));

    switch (method)
    {
        case MASTER_LOOT:
            HandleMasterLoot(group, item);
            break;

        case GROUP_LOOT:
        case NEED_BEFORE_GREED:
            HandleGroupLoot(group, item);
            break;

        case FREE_FOR_ALL:
            // In free-for-all, first bot to loot gets it - no coordination needed
            TC_LOG_DEBUG("playerbot.loot", "Free-for-all loot - no distribution needed");
            break;

        case ROUND_ROBIN:
            // Round robin is handled by game's loot system
            TC_LOG_DEBUG("playerbot.loot", "Round-robin loot - handled by game system");
            break;

        case PERSONAL_LOOT:
            // Personal loot is auto-assigned by game
            TC_LOG_DEBUG("playerbot.loot", "Personal loot - auto-assigned by game");
            break;

        default:
            TC_LOG_WARN("playerbot.loot", "Unknown loot method: {}", static_cast<uint32>(method));
            break;
    }

    _itemsDistributed++;
}

void UnifiedLootManager::DistributionModule::HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType)
{
    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->HandleLootRoll(rollId, rollType) : decltype(GetGameSystems(player)->GetLootDistribution()->HandleLootRoll(rollId, rollType))());
    _rollsProcessed++;
}

LootRollType UnifiedLootManager::DistributionModule::DetermineLootDecision(
    Player* player, LootItem const& item, LootDecisionStrategy strategy)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->DetermineLootDecision(item, strategy) : decltype(GetGameSystems(player)->GetLootDistribution()->DetermineLootDecision(item, strategy))());
}

LootPriority UnifiedLootManager::DistributionModule::CalculateLootPriority(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->CalculateLootPriority(item) : decltype(GetGameSystems(player)->GetLootDistribution()->CalculateLootPriority(item))());
}

bool UnifiedLootManager::DistributionModule::ShouldRollNeed(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->ShouldRollNeed(item) : decltype(GetGameSystems(player)->GetLootDistribution()->ShouldRollNeed(item))());
}

bool UnifiedLootManager::DistributionModule::ShouldRollGreed(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->ShouldRollGreed(item) : decltype(GetGameSystems(player)->GetLootDistribution()->ShouldRollGreed(item))());
}

bool UnifiedLootManager::DistributionModule::IsItemForClass(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->IsItemForClass(item) : decltype(GetGameSystems(player)->GetLootDistribution()->IsItemForClass(item))());
}

bool UnifiedLootManager::DistributionModule::IsItemForMainSpec(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->IsItemForMainSpec(item) : decltype(GetGameSystems(player)->GetLootDistribution()->IsItemForMainSpec(item))());
}

bool UnifiedLootManager::DistributionModule::IsItemForOffSpec(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->IsItemForOffSpec(item) : decltype(GetGameSystems(player)->GetLootDistribution()->IsItemForOffSpec(item))());
}

// ============================================================================
// HELPER METHODS - Group Loot Coordination
// ============================================================================

void UnifiedLootManager::DistributionModule::HandleMasterLoot(Group* group, LootItem const& item)
{
    if (!group)
        return;

    // Get master looter
    ObjectGuid masterLooterGuid = group->GetMasterLooterGuid();
    Player* masterLooter = ObjectAccessor::FindPlayer(masterLooterGuid);

    if (!masterLooter)
    {
        TC_LOG_ERROR("playerbot.loot", "Master looter not found for group {}", group->GetGUID().GetCounter());
        return;
    }

    // Evaluate all bots in the group
    std::vector<BotRollEvaluation> evaluations;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !GetBotAI(member))
            continue; // Skip non-bots and human players

        IGameSystemsManager* systems = GetGameSystems(member);
        if (!systems)
            continue;

        // Calculate upgrade value for this bot
        float upgradeValue = UnifiedLootManager::instance()->CalculateUpgradeValue(member, item);
        LootPriority priority = CalculateLootPriority(member, item);
        LootRollType recommendedRoll = DetermineLootDecision(member, item, LootDecisionStrategy::UPGRADE_PRIORITY);

        BotRollEvaluation eval;
        eval.bot = member;
        eval.rollType = recommendedRoll;
        eval.rollValue = 0; // Master loot doesn't use random rolls
        eval.upgradeValue = upgradeValue;
        eval.priority = priority;

        evaluations.push_back(eval);
    }

    if (evaluations.empty())
    {
        TC_LOG_DEBUG("playerbot.loot", "No eligible bots for master loot item {}", item.itemId);
        return;
    }

    // Sort by priority (lower enum value = higher priority) then by upgrade value
    std::sort(evaluations.begin(), evaluations.end(),
        [](BotRollEvaluation const& a, BotRollEvaluation const& b)
        {
            if (a.priority != b.priority)
                return a.priority < b.priority; // Higher priority (lower enum)
            return a.upgradeValue > b.upgradeValue; // Higher upgrade value
        });

    // Award to highest priority bot
    Player* winner = evaluations[0].bot;
    TC_LOG_INFO("playerbot.loot", "Master loot: Awarding item {} to {} (priority: {}, upgrade: {:.1f}%)",
        item.itemId, winner->GetName(), static_cast<uint32>(evaluations[0].priority), evaluations[0].upgradeValue);

    // TODO: Actually award the item via game's loot system
    // This requires integration with TrinityCore's loot distribution
}

void UnifiedLootManager::DistributionModule::HandleGroupLoot(Group* group, LootItem const& item)
{
    if (!group)
        return;

    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    // Create a new loot roll session
    uint32 rollId = _nextRollId++;
    LootRoll roll(rollId, item.itemId, item.lootSlot, group->GetGUID().GetCounter());

    // Collect rolls from all bots in the group
    std::vector<BotRollEvaluation> evaluations;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !GetBotAI(member))
            continue; // Skip non-bots and human players

        IGameSystemsManager* systems = GetGameSystems(member);
        if (!systems)
            continue;

        // Determine bot's roll decision
        LootRollType rollType = DetermineLootDecision(member, item,
            group->GetLootMethod() == NEED_BEFORE_GREED ?
                LootDecisionStrategy::NEED_BEFORE_GREED :
                LootDecisionStrategy::UPGRADE_PRIORITY);

        // Generate roll value (1-100) for Need/Greed
        uint32 rollValue = 0;
        if (rollType == LootRollType::NEED || rollType == LootRollType::GREED)
        {
            rollValue = urand(1, 100);
        }

        // Calculate upgrade metrics
        float upgradeValue = UnifiedLootManager::instance()->CalculateUpgradeValue(member, item);
        LootPriority priority = CalculateLootPriority(member, item);

        BotRollEvaluation eval;
        eval.bot = member;
        eval.rollType = rollType;
        eval.rollValue = rollValue;
        eval.upgradeValue = upgradeValue;
        eval.priority = priority;

        evaluations.push_back(eval);

        // Record in roll tracking
        roll.playerRolls[member->GetGUID().GetCounter()] = rollType;
        roll.rollValues[member->GetGUID().GetCounter()] = rollValue;
        roll.eligiblePlayers.insert(member->GetGUID().GetCounter());

        TC_LOG_DEBUG("playerbot.loot", "Bot {} rolled {} (value: {}) for item {}",
            member->GetName(), static_cast<uint32>(rollType), rollValue, item.itemId);
    }

    if (evaluations.empty())
    {
        TC_LOG_DEBUG("playerbot.loot", "No eligible bots for group loot item {}", item.itemId);
        return;
    }

    // Store roll for potential tie resolution
    _activeRolls[rollId] = roll;

    // Determine winner
    Player* winner = DetermineGroupLootWinner(evaluations);
    if (winner)
    {
        roll.winnerGuid = winner->GetGUID().GetCounter();
        roll.isCompleted = true;

        TC_LOG_INFO("playerbot.loot", "Group loot: {} won item {} with roll type {}",
            winner->GetName(), item.itemId,
            static_cast<uint32>(roll.playerRolls[roll.winnerGuid]));

        // TODO: Actually award the item via game's loot system
    }

    _rollsProcessed++;
}

Player* UnifiedLootManager::DistributionModule::DetermineGroupLootWinner(std::vector<BotRollEvaluation>& rolls)
{
    if (rolls.empty())
        return nullptr;

    // Separate rolls by type: Need > Greed > Disenchant > Pass
    std::vector<BotRollEvaluation*> needRolls;
    std::vector<BotRollEvaluation*> greedRolls;
    std::vector<BotRollEvaluation*> disenchantRolls;

    for (auto& roll : rolls)
    {
        if (roll.rollType == LootRollType::NEED)
            needRolls.push_back(&roll);
        else if (roll.rollType == LootRollType::GREED)
            greedRolls.push_back(&roll);
        else if (roll.rollType == LootRollType::DISENCHANT)
            disenchantRolls.push_back(&roll);
        // PASS rolls are excluded
    }

    // Determine winning pool (Need takes priority over Greed over Disenchant)
    std::vector<BotRollEvaluation*>* winningPool = nullptr;
    const char* rollTypeName = "None";

    if (!needRolls.empty())
    {
        winningPool = &needRolls;
        rollTypeName = "NEED";
    }
    else if (!greedRolls.empty())
    {
        winningPool = &greedRolls;
        rollTypeName = "GREED";
    }
    else if (!disenchantRolls.empty())
    {
        winningPool = &disenchantRolls;
        rollTypeName = "DISENCHANT";
    }

    if (!winningPool || winningPool->empty())
        return nullptr;

    // Sort by roll value (highest wins)
    std::sort(winningPool->begin(), winningPool->end(),
        [](BotRollEvaluation* a, BotRollEvaluation* b)
        {
            return a->rollValue > b->rollValue;
        });

    // Check for ties
    BotRollEvaluation* winner = (*winningPool)[0];
    std::vector<BotRollEvaluation*> tiedRolls;
    tiedRolls.push_back(winner);

    for (size_t i = 1; i < winningPool->size(); ++i)
    {
        if ((*winningPool)[i]->rollValue == winner->rollValue)
            tiedRolls.push_back((*winningPool)[i]);
        else
            break;
    }

    // If there's a tie, resolve by upgrade value
    if (tiedRolls.size() > 1)
    {
        TC_LOG_DEBUG("playerbot.loot", "Tie detected between {} bots with roll value {}, resolving by upgrade value",
            tiedRolls.size(), winner->rollValue);

        std::sort(tiedRolls.begin(), tiedRolls.end(),
            [](BotRollEvaluation* a, BotRollEvaluation* b)
            {
                return a->upgradeValue > b->upgradeValue;
            });

        winner = tiedRolls[0];
    }

    TC_LOG_DEBUG("playerbot.loot", "Winner determined: {} ({} roll: {}, upgrade: {:.1f}%)",
        winner->bot->GetName(), rollTypeName, winner->rollValue, winner->upgradeValue);

    return winner->bot;
}

void UnifiedLootManager::DistributionModule::ExecuteLootDistribution(Group* group, uint32 rollId)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "ExecuteLootDistribution called with null group");
        return;
    }

    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    auto it = _activeRolls.find(rollId);
    if (it == _activeRolls.end())
    {
        TC_LOG_WARN("playerbot.loot", "ExecuteLootDistribution: Roll ID {} not found", rollId);
        return;
    }

    LootRoll& roll = it->second;

    if (roll.isCompleted)
    {
        TC_LOG_DEBUG("playerbot.loot", "Roll {} already completed, winner: {}", rollId, roll.winnerGuid);
        // TODO: Award item to winner via game's loot system
        // Player* winner = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, roll.winnerGuid));
        // if (winner) { /* Award item */ }
    }
    else
    {
        TC_LOG_WARN("playerbot.loot", "ExecuteLootDistribution: Roll {} not yet completed", rollId);
    }

    // Clean up completed roll
    _activeRolls.erase(it);
}

void UnifiedLootManager::DistributionModule::ResolveRollTies(Group* group, uint32 rollId)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "ResolveRollTies called with null group");
        return;
    }

    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    auto it = _activeRolls.find(rollId);
    if (it == _activeRolls.end())
    {
        TC_LOG_WARN("playerbot.loot", "ResolveRollTies: Roll ID {} not found", rollId);
        return;
    }

    LootRoll& roll = it->second;

    // Find all players with the highest roll value
    uint32 highestRoll = 0;
    std::vector<uint32> tiedPlayers;

    for (auto const& [playerGuid, rollValue] : roll.rollValues)
    {
        if (rollValue > highestRoll)
        {
            highestRoll = rollValue;
            tiedPlayers.clear();
            tiedPlayers.push_back(playerGuid);
        }
        else if (rollValue == highestRoll && rollValue > 0)
        {
            tiedPlayers.push_back(playerGuid);
        }
    }

    if (tiedPlayers.size() <= 1)
    {
        TC_LOG_DEBUG("playerbot.loot", "No tie to resolve for roll {}", rollId);
        return;
    }

    TC_LOG_INFO("playerbot.loot", "Resolving tie for roll {} between {} players with value {}",
        rollId, tiedPlayers.size(), highestRoll);

    // Re-roll for tied players
    std::vector<std::pair<uint32, uint32>> rerolls; // playerGuid, rollValue
    for (uint32 playerGuid : tiedPlayers)
    {
        uint32 newRoll = urand(1, 100);
        rerolls.push_back({playerGuid, newRoll});
        TC_LOG_DEBUG("playerbot.loot", "Player {} re-rolled {}", playerGuid, newRoll);
    }

    // Find highest re-roll
    std::sort(rerolls.begin(), rerolls.end(),
        [](std::pair<uint32, uint32> const& a, std::pair<uint32, uint32> const& b)
        {
            return a.second > b.second;
        });

    // Update roll with winner
    roll.winnerGuid = rerolls[0].first;
    roll.isCompleted = true;

    TC_LOG_INFO("playerbot.loot", "Tie resolved: Player {} won with re-roll {}",
        roll.winnerGuid, rerolls[0].second);
}

void UnifiedLootManager::DistributionModule::HandleLootNinja(Group* group, uint32 suspectedPlayer)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "HandleLootNinja called with null group");
        return;
    }

    Player* suspect = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, suspectedPlayer));
    if (!suspect)
    {
        TC_LOG_WARN("playerbot.loot", "HandleLootNinja: Suspected player {} not found", suspectedPlayer);
        return;
    }

    // Log the ninja loot attempt
    TC_LOG_WARN("playerbot.loot", "Potential ninja loot detected: Player {} in group {}",
        suspect->GetName(), group->GetGUID().GetCounter());

    // For bots, this is primarily for logging and detection
    // Human players would handle actual consequences

    // Check recent loot history for suspicious patterns
    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    uint32 recentNeedRolls = 0;
    uint32 recentWins = 0;

    for (auto const& [rollId, roll] : _activeRolls)
    {
        if (roll.groupId != group->GetGUID().GetCounter())
            continue;

        auto playerRollIt = roll.playerRolls.find(suspectedPlayer);
        if (playerRollIt != roll.playerRolls.end())
        {
            if (playerRollIt->second == LootRollType::NEED)
                ++recentNeedRolls;

            if (roll.winnerGuid == suspectedPlayer)
                ++recentWins;
        }
    }

    // Simple heuristic: If player has won more than 50% of recent rolls, flag as suspicious
    if (recentNeedRolls > 0 && recentWins > (recentNeedRolls / 2))
    {
        TC_LOG_WARN("playerbot.loot", "Player {} has suspicious loot pattern: {} wins out of {} need rolls",
            suspect->GetName(), recentWins, recentNeedRolls);

        // For bots, we could implement automatic remediation:
        // - Reduce their loot priority temporarily
        // - Flag for review
        // - Notify group leader (if human)

        // For now, just log for monitoring
    }
    else
    {
        TC_LOG_DEBUG("playerbot.loot", "Player {} loot pattern appears normal: {} wins out of {} need rolls",
            suspect->GetName(), recentWins, recentNeedRolls);
    }
}

// ============================================================================
// UNIFIED MANAGER PUBLIC INTERFACE - Analysis
// ============================================================================

float UnifiedLootManager::CalculateItemValue(Player* player, LootItem const& item)
{
    return _analysis->CalculateItemValue(player, item);
}

float UnifiedLootManager::CalculateUpgradeValue(Player* player, LootItem const& item)
{
    return _analysis->CalculateUpgradeValue(player, item);
}

bool UnifiedLootManager::IsSignificantUpgrade(Player* player, LootItem const& item)
{
    return _analysis->IsSignificantUpgrade(player, item);
}

float UnifiedLootManager::CalculateStatWeight(Player* player, uint32 statType)
{
    return _analysis->CalculateStatWeight(player, statType);
}

float UnifiedLootManager::CompareItems(Player* player, LootItem const& newItem, Item const* currentItem)
{
    return _analysis->CompareItems(player, newItem, currentItem);
}

float UnifiedLootManager::CalculateItemScore(Player* player, LootItem const& item)
{
    return _analysis->CalculateItemScore(player, item);
}

std::vector<std::pair<uint32, float>> UnifiedLootManager::GetStatPriorities(Player* player)
{
    return _analysis->GetStatPriorities(player);
}

// ============================================================================
// UNIFIED MANAGER PUBLIC INTERFACE - Coordination
// ============================================================================

void UnifiedLootManager::InitiateLootSession(Group* group, Loot* loot)
{
    _coordination->InitiateLootSession(group, loot);
}

void UnifiedLootManager::ProcessLootSession(Group* group, uint32 lootSessionId)
{
    _coordination->ProcessLootSession(group, lootSessionId);
}

void UnifiedLootManager::CompleteLootSession(uint32 lootSessionId)
{
    _coordination->CompleteLootSession(lootSessionId);
}

void UnifiedLootManager::HandleLootSessionTimeout(uint32 lootSessionId)
{
    _coordination->HandleLootSessionTimeout(lootSessionId);
}

void UnifiedLootManager::OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items)
{
    _coordination->OrchestrateLootDistribution(group, items);
}

void UnifiedLootManager::PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items)
{
    _coordination->PrioritizeLootDistribution(group, items);
}

void UnifiedLootManager::OptimizeLootSequence(Group* group, std::vector<LootItem>& items)
{
    _coordination->OptimizeLootSequence(group, items);
}

void UnifiedLootManager::FacilitateGroupLootDiscussion(Group* group, LootItem const& item)
{
    _coordination->FacilitateGroupLootDiscussion(group, item);
}

void UnifiedLootManager::HandleLootConflictResolution(Group* group, LootItem const& item)
{
    _coordination->HandleLootConflictResolution(group, item);
}

void UnifiedLootManager::BroadcastLootRecommendations(Group* group, LootItem const& item)
{
    _coordination->BroadcastLootRecommendations(group, item);
}

void UnifiedLootManager::OptimizeLootEfficiency(Group* group)
{
    _coordination->OptimizeLootEfficiency(group);
}

void UnifiedLootManager::MinimizeLootTime(Group* group, uint32 sessionId)
{
    _coordination->MinimizeLootTime(group, sessionId);
}

void UnifiedLootManager::MaximizeLootFairness(Group* group, uint32 sessionId)
{
    _coordination->MaximizeLootFairness(group, sessionId);
}

// ============================================================================
// UNIFIED MANAGER PUBLIC INTERFACE - Distribution
// ============================================================================

void UnifiedLootManager::DistributeLoot(Group* group, LootItem const& item)
{
    _distribution->DistributeLoot(group, item);
}

void UnifiedLootManager::HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType)
{
    _distribution->HandleLootRoll(player, rollId, rollType);
}

LootRollType UnifiedLootManager::DetermineLootDecision(
    Player* player, LootItem const& item, LootDecisionStrategy strategy)
{
    return _distribution->DetermineLootDecision(player, item, strategy);
}

LootPriority UnifiedLootManager::CalculateLootPriority(Player* player, LootItem const& item)
{
    return _distribution->CalculateLootPriority(player, item);
}

bool UnifiedLootManager::ShouldRollNeed(Player* player, LootItem const& item)
{
    return _distribution->ShouldRollNeed(player, item);
}

bool UnifiedLootManager::ShouldRollGreed(Player* player, LootItem const& item)
{
    return _distribution->ShouldRollGreed(player, item);
}

bool UnifiedLootManager::IsItemForClass(Player* player, LootItem const& item)
{
    return _distribution->IsItemForClass(player, item);
}

bool UnifiedLootManager::IsItemForMainSpec(Player* player, LootItem const& item)
{
    return _distribution->IsItemForMainSpec(player, item);
}

bool UnifiedLootManager::IsItemForOffSpec(Player* player, LootItem const& item)
{
    return _distribution->IsItemForOffSpec(player, item);
}

void UnifiedLootManager::ExecuteLootDistribution(Group* group, uint32 rollId)
{
    _distribution->ExecuteLootDistribution(group, rollId);
}

void UnifiedLootManager::ResolveRollTies(Group* group, uint32 rollId)
{
    _distribution->ResolveRollTies(group, rollId);
}

void UnifiedLootManager::HandleLootNinja(Group* group, uint32 suspectedPlayer)
{
    _distribution->HandleLootNinja(group, suspectedPlayer);
}

// ============================================================================
// UNIFIED OPERATIONS
// ============================================================================

void UnifiedLootManager::ProcessCompleteLootFlow(Group* group, Loot* loot)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto startTime = GameTime::GetGameTimeMS();
    _totalOperations++;

    // Step 1: Initiate session (Coordination)
    InitiateLootSession(group, loot);

    // Step 2: Analyze items (Analysis)
    // This happens automatically when players evaluate items

    // Step 3: Execute distribution (Distribution)
    // This happens when players roll

    auto endTime = GameTime::GetGameTimeMS();
    _totalProcessingTimeMs += (endTime - startTime);

    TC_LOG_DEBUG("playerbot.loot", "Processed complete loot flow in {} ms", endTime - startTime);
}

std::string UnifiedLootManager::GetLootRecommendation(Player* player, LootItem const& item)
{
    std::ostringstream oss;

    // Analysis
    float itemValue = CalculateItemValue(player, item);
    float upgradeValue = CalculateUpgradeValue(player, item);
    bool isSignificantUpgrade = IsSignificantUpgrade(player, item);

    // Distribution decision
    LootPriority priority = CalculateLootPriority(player, item);
    LootRollType recommendedRoll = DetermineLootDecision(player, item, LootDecisionStrategy::UPGRADE_PRIORITY);

    // Format recommendation
    oss << "Item Value: " << itemValue << "/100\n";
    oss << "Upgrade: " << upgradeValue << "% improvement\n";
    oss << "Significant Upgrade: " << (isSignificantUpgrade ? "Yes" : "No") << "\n";
    oss << "Priority: " << static_cast<uint32>(priority) << "\n";
    oss << "Recommended Action: ";

    switch (recommendedRoll)
    {
        case LootRollType::NEED:       oss << "NEED"; break;
        case LootRollType::GREED:      oss << "GREED"; break;
        case LootRollType::PASS:       oss << "PASS"; break;
        case LootRollType::DISENCHANT: oss << "DISENCHANT"; break;
        default:                       oss << "UNKNOWN"; break;
    }

    return oss.str();
}

std::string UnifiedLootManager::GetLootStatistics() const
{
    std::ostringstream oss;

    oss << "=== UnifiedLootManager Statistics ===\n";
    oss << "Total Operations: " << _totalOperations.load() << "\n";
    oss << "Total Processing Time: " << _totalProcessingTimeMs.load() << " ms\n";

    if (_totalOperations > 0)
    {
        oss << "Average Processing Time: "
            << (_totalProcessingTimeMs.load() / _totalOperations.load()) << " ms/operation\n";
    }

    oss << "\n--- Analysis Module ---\n";
    oss << "Items Analyzed: " << _analysis->_itemsAnalyzed.load() << "\n";
    oss << "Upgrades Detected: " << _analysis->_upgradesDetected.load() << "\n";

    oss << "\n--- Coordination Module ---\n";
    oss << "Sessions Created: " << _coordination->_sessionsCreated.load() << "\n";
    oss << "Sessions Completed: " << _coordination->_sessionsCompleted.load() << "\n";
    oss << "Active Sessions: " << (_coordination->_sessionsCreated.load() - _coordination->_sessionsCompleted.load()) << "\n";

    oss << "\n--- Distribution Module ---\n";
    oss << "Rolls Processed: " << _distribution->_rollsProcessed.load() << "\n";
    oss << "Items Distributed: " << _distribution->_itemsDistributed.load() << "\n";

    return oss.str();
}

} // namespace Playerbot
