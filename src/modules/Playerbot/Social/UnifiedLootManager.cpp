/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "UnifiedLootManager.h"
#include "LootAnalysis.h"
#include "LootCoordination.h"
#include "LootDistribution.h"
#include "Log.h"
#include <sstream>
#include "GameTime.h"

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
    // Delegate to existing LootAnalysis for now
    // TODO: Move logic here after migration
    _itemsAnalyzed++;
    return LootAnalysis::instance()->CalculateItemValue(player, item);
}

float UnifiedLootManager::AnalysisModule::CalculateUpgradeValue(Player* player, LootItem const& item)
{
    // Delegate to existing LootAnalysis
    auto value = LootAnalysis::instance()->CalculateUpgradeValue(player, item);
    if (value >= 15.0f) // Significant upgrade threshold
        _upgradesDetected++;
    return value;
}

bool UnifiedLootManager::AnalysisModule::IsSignificantUpgrade(Player* player, LootItem const& item)
{
    return LootAnalysis::instance()->IsSignificantUpgrade(player, item);
}

float UnifiedLootManager::AnalysisModule::CalculateStatWeight(Player* player, uint32 statType)
{
    return LootAnalysis::instance()->CalculateStatWeight(player, statType);
}

float UnifiedLootManager::AnalysisModule::CompareItems(Player* player, LootItem const& newItem, Item const* currentItem)
{
    return LootAnalysis::instance()->CompareItems(player, newItem, currentItem);
}

float UnifiedLootManager::AnalysisModule::CalculateItemScore(Player* player, LootItem const& item)
{
    return LootAnalysis::instance()->CalculateItemScore(player, item);
}

std::vector<std::pair<uint32, float>> UnifiedLootManager::AnalysisModule::GetStatPriorities(Player* player)
{
    return LootAnalysis::instance()->GetStatPriorities(player);
}

// ============================================================================
// COORDINATION MODULE IMPLEMENTATION
// ============================================================================

void UnifiedLootManager::CoordinationModule::InitiateLootSession(Group* group, Loot* loot)
{
    std::lock_guard<decltype(_sessionMutex)> lock(_sessionMutex);

    // Delegate to existing LootCoordination
    LootCoordination::instance()->InitiateLootSession(group, loot);

    // Track in unified system
    uint32 sessionId = _nextSessionId++;
    LootSession session(sessionId, group ? group->GetGUID().GetCounter() : 0);
    _activeSessions[sessionId] = session;
    _sessionsCreated++;
}

void UnifiedLootManager::CoordinationModule::ProcessLootSession(Group* group, uint32 lootSessionId)
{
    LootCoordination::instance()->ProcessLootSession(group, lootSessionId);
}

void UnifiedLootManager::CoordinationModule::CompleteLootSession(uint32 lootSessionId)
{
    std::lock_guard<decltype(_sessionMutex)> lock(_sessionMutex);

    LootCoordination::instance()->CompleteLootSession(lootSessionId);

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
    LootCoordination::instance()->HandleLootSessionTimeout(lootSessionId);
    CompleteLootSession(lootSessionId); // Cleanup
}

void UnifiedLootManager::CoordinationModule::OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items)
{
    LootCoordination::instance()->OrchestrateLootDistribution(group, items);
}

void UnifiedLootManager::CoordinationModule::PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items)
{
    LootCoordination::instance()->PrioritizeLootDistribution(group, items);
}

void UnifiedLootManager::CoordinationModule::OptimizeLootSequence(Group* group, std::vector<LootItem>& items)
{
    LootCoordination::instance()->OptimizeLootSequence(group, items);
}

void UnifiedLootManager::CoordinationModule::FacilitateGroupLootDiscussion(Group* group, LootItem const& item)
{
    LootCoordination::instance()->FacilitateGroupLootDiscussion(group, item);
}

void UnifiedLootManager::CoordinationModule::HandleLootConflictResolution(Group* group, LootItem const& item)
{
    LootCoordination::instance()->HandleLootConflictResolution(group, item);
}

void UnifiedLootManager::CoordinationModule::BroadcastLootRecommendations(Group* group, LootItem const& item)
{
    LootCoordination::instance()->BroadcastLootRecommendations(group, item);
}

void UnifiedLootManager::CoordinationModule::OptimizeLootEfficiency(Group* group)
{
    LootCoordination::instance()->OptimizeLootEfficiency(group);
}

void UnifiedLootManager::CoordinationModule::MinimizeLootTime(Group* group, uint32 sessionId)
{
    LootCoordination::instance()->MinimizeLootTime(group, sessionId);
}

void UnifiedLootManager::CoordinationModule::MaximizeLootFairness(Group* group, uint32 sessionId)
{
    LootCoordination::instance()->MaximizeLootFairness(group, sessionId);
}

// ============================================================================
// DISTRIBUTION MODULE IMPLEMENTATION
// ============================================================================

void UnifiedLootManager::DistributionModule::DistributeLoot(Group* group, LootItem const& item)
{
    LootDistribution::instance()->DistributeLoot(group, item);
    _itemsDistributed++;
}

void UnifiedLootManager::DistributionModule::HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType)
{
    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    LootDistribution::instance()->HandleLootRoll(player, rollId, rollType);
    _rollsProcessed++;
}

LootRollType UnifiedLootManager::DistributionModule::DetermineLootDecision(
    Player* player, LootItem const& item, LootDecisionStrategy strategy)
{
    return LootDistribution::instance()->DetermineLootDecision(player, item, strategy);
}

LootPriority UnifiedLootManager::DistributionModule::CalculateLootPriority(Player* player, LootItem const& item)
{
    return LootDistribution::instance()->CalculateLootPriority(player, item);
}

bool UnifiedLootManager::DistributionModule::ShouldRollNeed(Player* player, LootItem const& item)
{
    return LootDistribution::instance()->ShouldRollNeed(player, item);
}

bool UnifiedLootManager::DistributionModule::ShouldRollGreed(Player* player, LootItem const& item)
{
    return LootDistribution::instance()->ShouldRollGreed(player, item);
}

bool UnifiedLootManager::DistributionModule::IsItemForClass(Player* player, LootItem const& item)
{
    return LootDistribution::instance()->IsItemForClass(player, item);
}

bool UnifiedLootManager::DistributionModule::IsItemForMainSpec(Player* player, LootItem const& item)
{
    return LootDistribution::instance()->IsItemForMainSpec(player, item);
}

bool UnifiedLootManager::DistributionModule::IsItemForOffSpec(Player* player, LootItem const& item)
{
    return LootDistribution::instance()->IsItemForOffSpec(player, item);
}

void UnifiedLootManager::DistributionModule::ExecuteLootDistribution(Group* group, uint32 rollId)
{
    LootDistribution::instance()->ExecuteLootDistribution(group, rollId);
}

void UnifiedLootManager::DistributionModule::ResolveRollTies(Group* group, uint32 rollId)
{
    LootDistribution::instance()->ResolveRollTies(group, rollId);
}

void UnifiedLootManager::DistributionModule::HandleLootNinja(Group* group, uint32 suspectedPlayer)
{
    LootDistribution::instance()->HandleLootNinja(group, suspectedPlayer);
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
