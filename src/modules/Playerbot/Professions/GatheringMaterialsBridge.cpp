/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GatheringMaterialsBridge.h"
#include "ProfessionManager.h"
#include "GatheringManager.h"
#include "Player.h"
#include "Log.h"
#include "Session/BotSession.h"
#include "AI/BotAI.h"
#include <algorithm>

namespace Playerbot
{

// Singleton instance
GatheringMaterialsBridge* GatheringMaterialsBridge::instance()
{
    static GatheringMaterialsBridge instance;
    return &instance;
}

GatheringMaterialsBridge::GatheringMaterialsBridge()
{
}

// ============================================================================
// CORE BRIDGE MANAGEMENT
// ============================================================================

void GatheringMaterialsBridge::Initialize()
{
    TC_LOG_INFO("playerbots", "GatheringMaterialsBridge: Initializing gathering-crafting bridge...");

    LoadNodeMaterialMappings();
    InitializeGatheringProfessionMaterials();

    TC_LOG_INFO("playerbots", "GatheringMaterialsBridge: Initialized with {} material-node mappings",
        _materialToNodeType.size());
}

void GatheringMaterialsBridge::Update(::Player* player, uint32 diff)
{
    if (!player || !IsEnabled(player))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard lock(_mutex);

    // Update material requirements every 30 seconds
    static std::unordered_map<uint32, uint32> lastRequirementUpdate;
    uint32 currentTime = GameTime::GetGameTimeMS();

    if (currentTime - lastRequirementUpdate[playerGuid] >= REQUIREMENT_UPDATE_INTERVAL)
    {
        UpdateMaterialRequirements(player);
        lastRequirementUpdate[playerGuid] = currentTime;
    }

    // Update active gathering session
    auto sessionIt = _playerActiveSessions.find(playerGuid);
    if (sessionIt != _playerActiveSessions.end())
    {
        UpdateGatheringSession(sessionIt->second);
    }
}

void GatheringMaterialsBridge::SetEnabled(::Player* player, bool enabled)
{
    if (!player)
        return;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();
    _enabledState[playerGuid] = enabled;
}

bool GatheringMaterialsBridge::IsEnabled(::Player* player) const
{
    if (!player)
        return false;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    auto it = _enabledState.find(playerGuid);
    if (it != _enabledState.end())
        return it->second;

    return false; // Disabled by default
}

// ============================================================================
// MATERIAL REQUIREMENT ANALYSIS
// ============================================================================

std::vector<MaterialRequirement> GatheringMaterialsBridge::GetNeededMaterials(::Player* player)
{
    std::vector<MaterialRequirement> needed;

    if (!player)
        return needed;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    // Return cached requirements if available
    auto it = _materialRequirements.find(playerGuid);
    if (it != _materialRequirements.end())
        return it->second;

    return needed;
}

bool GatheringMaterialsBridge::IsItemNeededForCrafting(::Player* player, uint32 itemId)
{
    if (!player)
        return false;

    // Check if player knows any recipes that use this material
    return PlayerKnowsRecipesUsingMaterial(player, itemId);
}

MaterialPriority GatheringMaterialsBridge::GetMaterialPriority(::Player* player, uint32 itemId)
{
    if (!player)
        return MaterialPriority::NONE;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    auto it = _materialRequirements.find(playerGuid);
    if (it == _materialRequirements.end())
        return MaterialPriority::NONE;

    // Find material in requirements
    for (const MaterialRequirement& req : it->second)
    {
        if (req.itemId == itemId)
            return req.priority;
    }

    return MaterialPriority::NONE;
}

void GatheringMaterialsBridge::UpdateMaterialRequirements(::Player* player)
{
    if (!player)
        return;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    std::vector<MaterialRequirement> requirements;

    // Get all player's professions
    auto professions = ProfessionManager::instance()->GetPlayerProfessions(player);

    for (const ProfessionSkillInfo& profInfo : professions)
    {
        // Skip if profession is maxed
        if (profInfo.currentSkill >= profInfo.maxSkill)
            continue;

        // Get optimal leveling recipe for profession
        RecipeInfo const* recipe = ProfessionManager::instance()->GetOptimalLevelingRecipe(player, profInfo.profession);
        if (!recipe)
            continue;

        // Get missing materials for this recipe
        auto missingMaterials = ProfessionManager::instance()->GetMissingMaterials(player, *recipe);

        for (const auto& [itemId, quantity] : missingMaterials)
        {
            MaterialRequirement req;
            req.itemId = itemId;
            req.quantityNeeded = quantity;
            req.quantityHave = player->GetItemCount(itemId);
            req.forProfession = profInfo.profession;
            req.forRecipeId = recipe->recipeId;

            // Determine priority based on skill-up potential
            float skillUpChance = ProfessionManager::instance()->GetSkillUpChance(player, *recipe);
            if (skillUpChance >= 1.0f)
                req.priority = MaterialPriority::HIGH;      // Orange recipe
            else if (skillUpChance >= 0.5f)
                req.priority = MaterialPriority::MEDIUM;    // Yellow recipe
            else if (skillUpChance > 0.0f)
                req.priority = MaterialPriority::LOW;       // Green recipe
            else
                req.priority = MaterialPriority::NONE;      // Gray recipe - skip

            if (req.priority != MaterialPriority::NONE && !req.IsFulfilled())
                requirements.push_back(req);
        }
    }

    // Sort by priority (highest first)
    std::sort(requirements.begin(), requirements.end(),
        [](const MaterialRequirement& a, const MaterialRequirement& b) {
            return static_cast<uint8>(a.priority) > static_cast<uint8>(b.priority);
        });

    _materialRequirements[playerGuid] = requirements;

    TC_LOG_DEBUG("playerbots", "GatheringMaterialsBridge: Updated material requirements for player {} - {} materials needed",
        player->GetName(), requirements.size());
}

// ============================================================================
// GATHERING AUTOMATION
// ============================================================================

std::vector<GatheringNode> GatheringMaterialsBridge::PrioritizeNodesByNeeds(
    ::Player* player,
    std::vector<GatheringNode> const& nodes)
{
    if (!player || nodes.empty())
        return nodes;

    // Create copy to sort
    std::vector<GatheringNode> prioritized = nodes;

    // Sort by score (highest first)
    std::sort(prioritized.begin(), prioritized.end(),
        [this, player](const GatheringNode& a, const GatheringNode& b) {
            return CalculateNodeScore(player, a) > CalculateNodeScore(player, b);
        });

    return prioritized;
}

bool GatheringMaterialsBridge::StartGatheringForMaterial(::Player* player, uint32 itemId, uint32 quantity)
{
    if (!player)
        return false;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    // Check if already has active session
    if (_playerActiveSessions.find(playerGuid) != _playerActiveSessions.end())
    {
        TC_LOG_DEBUG("playerbots", "GatheringMaterialsBridge: Player {} already has active gathering session",
            player->GetName());
        return false;
    }

    // Create gathering session
    uint32 sessionId = CreateGatheringSession(player, itemId, quantity);
    if (sessionId == 0)
        return false;

    // Configure gathering manager
    GatheringManager* gatheringMgr = GetGatheringManager(player);
    if (gatheringMgr)
    {
        ConfigureGatheringForMaterials(player, true);
        gatheringMgr->SetPrioritizeSkillUps(false); // Prioritize materials over skill-ups
    }

    _globalStatistics.gatheringSessionsStarted++;
    _playerStatistics[playerGuid].gatheringSessionsStarted++;

    TC_LOG_INFO("playerbots", "GatheringMaterialsBridge: Started gathering session {} for player {} - target: {} x {}",
        sessionId, player->GetName(), quantity, itemId);

    return true;
}

void GatheringMaterialsBridge::StopGatheringSession(uint32 sessionId)
{
    std::lock_guard lock(_mutex);

    auto it = _activeSessions.find(sessionId);
    if (it == _activeSessions.end())
        return;

    CompleteGatheringSession(sessionId, false);
}

GatheringMaterialSession const* GatheringMaterialsBridge::GetActiveSession(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    auto it = _playerActiveSessions.find(playerGuid);
    if (it == _playerActiveSessions.end())
        return nullptr;

    auto sessionIt = _activeSessions.find(it->second);
    if (sessionIt == _activeSessions.end())
        return nullptr;

    return &sessionIt->second;
}

void GatheringMaterialsBridge::OnMaterialGathered(::Player* player, uint32 itemId, uint32 quantity)
{
    if (!player)
        return;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    // Check if this material is for an active gathering session
    auto sessionIdIt = _playerActiveSessions.find(playerGuid);
    if (sessionIdIt == _playerActiveSessions.end())
        return;

    auto sessionIt = _activeSessions.find(sessionIdIt->second);
    if (sessionIt == _activeSessions.end())
        return;

    GatheringMaterialSession& session = sessionIt->second;

    // Check if this is the target item
    if (session.targetItemId == itemId)
    {
        session.gatheredSoFar += quantity;

        _globalStatistics.materialsGatheredForCrafting += quantity;
        _playerStatistics[playerGuid].materialsGatheredForCrafting += quantity;

        TC_LOG_DEBUG("playerbots", "GatheringMaterialsBridge: Session {} gathered {} x {} ({}/{})",
            session.sessionId, quantity, itemId, session.gatheredSoFar, session.targetQuantity);

        // Check if session complete
        if (session.gatheredSoFar >= session.targetQuantity)
        {
            CompleteGatheringSession(session.sessionId, true);
        }
    }
}

// ============================================================================
// GATHERING NODE SCORING
// ============================================================================

float GatheringMaterialsBridge::CalculateNodeScore(::Player* player, GatheringNode const& node)
{
    if (!player)
        return 0.0f;

    float score = 100.0f; // Base score

    // Factor 1: Distance (closer = higher score)
    float distanceFactor = 1.0f / (1.0f + node.distance / 10.0f); // Normalize to 0-1
    score *= distanceFactor;

    // Factor 2: Material need priority
    // We don't know exact item yield from node, so we estimate based on node type
    MaterialPriority priority = MaterialPriority::NONE;
    auto requirements = GetNeededMaterials(player);

    for (const MaterialRequirement& req : requirements)
    {
        GatheringNodeType nodeType = GetNodeTypeForMaterial(req.itemId);
        if (nodeType == node.nodeType)
        {
            priority = req.priority;
            break;
        }
    }

    // Apply priority multiplier
    float priorityMultiplier = 1.0f;
    switch (priority)
    {
        case MaterialPriority::CRITICAL: priorityMultiplier = 5.0f; break;
        case MaterialPriority::HIGH:     priorityMultiplier = 3.0f; break;
        case MaterialPriority::MEDIUM:   priorityMultiplier = 2.0f; break;
        case MaterialPriority::LOW:      priorityMultiplier = 1.2f; break;
        case MaterialPriority::NONE:     priorityMultiplier = 1.0f; break;
    }

    score *= priorityMultiplier;

    // Factor 3: Skill-up potential (if no material need, prefer skill-ups)
    if (priority == MaterialPriority::NONE)
    {
        // Would need skill-up chance calculation here
        // For now, just give slight bonus to nodes at appropriate skill level
        score *= 1.1f;
    }

    return score;
}

bool GatheringMaterialsBridge::DoeNodeProvideNeededMaterial(::Player* player, GatheringNode const& node)
{
    if (!player)
        return false;

    auto requirements = GetNeededMaterials(player);

    for (const MaterialRequirement& req : requirements)
    {
        GatheringNodeType nodeType = GetNodeTypeForMaterial(req.itemId);
        if (nodeType == node.nodeType)
            return true;
    }

    return false;
}

uint32 GatheringMaterialsBridge::GetEstimatedYield(GatheringNode const& node)
{
    // Estimate based on node type
    // This is a simplification - real implementation would query node spawn data
    switch (node.nodeType)
    {
        case GatheringNodeType::MINING_VEIN:
            return 3;  // Average 3 ore per vein
        case GatheringNodeType::HERB_NODE:
            return 2;  // Average 2 herbs per node
        case GatheringNodeType::CREATURE_CORPSE:
            return 1;  // 1 leather per skinnable corpse
        case GatheringNodeType::FISHING_POOL:
            return 5;  // Average 5 fish per pool
        default:
            return 1;
    }
}

// ============================================================================
// INTEGRATION WITH GATHERING MANAGER
// ============================================================================

void GatheringMaterialsBridge::ConfigureGatheringForMaterials(::Player* player, bool prioritizeMaterials)
{
    GatheringManager* gatheringMgr = GetGatheringManager(player);
    if (!gatheringMgr)
        return;

    gatheringMgr->SetPrioritizeSkillUps(!prioritizeMaterials);
}

GatheringManager* GatheringMaterialsBridge::GetGatheringManager(::Player* player)
{
    if (!player)
        return nullptr;

    // Get the player's session
    WorldSession* session = player->GetSession();
    if (!session || !session->IsBot())
        return nullptr;

    // Cast to BotSession and get BotAI
    BotSession* botSession = static_cast<BotSession*>(session);
    BotAI* ai = botSession->GetAI();
    if (!ai)
        return nullptr;

    // Return the GatheringManager from BotAI
    return ai->GetGatheringManager();
}

void GatheringMaterialsBridge::SynchronizeWithGatheringManager(::Player* player)
{
    // Synchronization logic if needed
}

// ============================================================================
// STATISTICS
// ============================================================================

GatheringMaterialsStatistics const& GatheringMaterialsBridge::GetPlayerStatistics(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    static GatheringMaterialsStatistics emptyStats;
    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        return it->second;

    return emptyStats;
}

GatheringMaterialsStatistics const& GatheringMaterialsBridge::GetGlobalStatistics() const
{
    return _globalStatistics;
}

void GatheringMaterialsBridge::ResetStatistics(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);

    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        it->second.Reset();
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

void GatheringMaterialsBridge::LoadNodeMaterialMappings()
{
    // Map common crafting materials to their gathering node types
    // This would ideally be loaded from database in full implementation

    // Mining materials
    // Copper, Tin, Iron, Gold, Mithril, Thorium, Fel Iron, Adamantite, Cobalt, Saronite, Titanium, etc.
    // For now, we'll handle this dynamically by checking item class

    TC_LOG_DEBUG("playerbots", "GatheringMaterialsBridge: Loaded node-material mappings");
}

void GatheringMaterialsBridge::InitializeGatheringProfessionMaterials()
{
    TC_LOG_DEBUG("playerbots", "GatheringMaterialsBridge: Initialized gathering profession materials");
}

// ============================================================================
// MATERIAL ANALYSIS HELPERS
// ============================================================================

GatheringNodeType GatheringMaterialsBridge::GetNodeTypeForMaterial(uint32 itemId) const
{
    // Check if this is a known mining material
    // This is simplified - real implementation would check item properties
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return GatheringNodeType::NONE;

    uint32 itemClass = itemTemplate->GetClass();
    uint32 itemSubClass = itemTemplate->GetSubClass();

    // Trade goods
    if (itemClass == ITEM_CLASS_TRADE_GOODS)
    {
        // Metal & Stone = Mining
        if (itemSubClass == 7 || itemSubClass == 12)
            return GatheringNodeType::MINING_VEIN;

        // Herb = Herbalism
        if (itemSubClass == 9)
            return GatheringNodeType::HERB_NODE;

        // Leather = Skinning
        if (itemSubClass == 6)
            return GatheringNodeType::CREATURE_CORPSE;

        // Fish = Fishing
        if (itemSubClass == 8)
            return GatheringNodeType::FISHING_POOL;
    }

    return GatheringNodeType::NONE;
}

std::vector<RecipeInfo> GatheringMaterialsBridge::GetRecipesThatUseMaterial(::Player* player, uint32 itemId)
{
    std::vector<RecipeInfo> recipes;

    if (!player)
        return recipes;

    // Get all player's professions
    auto professions = ProfessionManager::instance()->GetPlayerProfessions(player);

    for (const ProfessionSkillInfo& profInfo : professions)
    {
        // Get all recipes for this profession
        auto professionRecipes = ProfessionManager::instance()->GetRecipesForProfession(profInfo.profession);

        for (const RecipeInfo& recipe : professionRecipes)
        {
            // Check if recipe uses this material
            for (const RecipeInfo::Reagent& reagent : recipe.reagents)
            {
                if (reagent.itemId == itemId)
                {
                    recipes.push_back(recipe);
                    break;
                }
            }
        }
    }

    return recipes;
}

bool GatheringMaterialsBridge::PlayerKnowsRecipesUsingMaterial(::Player* player, uint32 itemId)
{
    if (!player)
        return false;

    auto recipes = GetRecipesThatUseMaterial(player, itemId);

    // Check if player knows any of these recipes
    for (const RecipeInfo& recipe : recipes)
    {
        if (ProfessionManager::instance()->KnowsRecipe(player, recipe.recipeId))
            return true;
    }

    return false;
}

float GatheringMaterialsBridge::CalculateGatheringOpportunityCost(::Player* player, uint32 itemId, uint32 quantity)
{
    // Opportunity cost = time to gather vs time to earn gold to buy
    // This is simplified - real implementation would consider:
    // - Gathering speed (nodes per hour)
    // - Yield per node
    // - Travel time to gathering zones
    // - Gold earning rate (questing, other activities)
    // - Current auction house prices

    return 1.0f; // Placeholder
}

// ============================================================================
// GATHERING SESSION MANAGEMENT
// ============================================================================

uint32 GatheringMaterialsBridge::CreateGatheringSession(::Player* player, uint32 itemId, uint32 quantity)
{
    if (!player)
        return 0;

    uint32 sessionId = _nextSessionId++;
    uint32 playerGuid = player->GetGUID().GetCounter();

    GatheringMaterialSession session;
    session.sessionId = sessionId;
    session.playerGuid = playerGuid;
    session.targetItemId = itemId;
    session.targetQuantity = quantity;
    session.nodeType = GetNodeTypeForMaterial(itemId);
    session.startPosition = player->GetPosition();

    _activeSessions[sessionId] = session;
    _playerActiveSessions[playerGuid] = sessionId;

    return sessionId;
}

void GatheringMaterialsBridge::UpdateGatheringSession(uint32 sessionId)
{
    auto it = _activeSessions.find(sessionId);
    if (it == _activeSessions.end())
        return;

    GatheringMaterialSession& session = it->second;

    // Check if session has timed out
    if (session.GetDuration() > MAX_GATHERING_SESSION_DURATION)
    {
        TC_LOG_WARN("playerbots", "GatheringMaterialsBridge: Session {} timed out after {} ms",
            sessionId, session.GetDuration());
        CompleteGatheringSession(sessionId, false);
    }
}

void GatheringMaterialsBridge::CompleteGatheringSession(uint32 sessionId, bool success)
{
    auto it = _activeSessions.find(sessionId);
    if (it == _activeSessions.end())
        return;

    GatheringMaterialSession& session = it->second;
    session.isActive = false;
    session.endTime = GameTime::GetGameTimeMS();

    uint32 playerGuid = session.playerGuid;

    if (success)
    {
        _globalStatistics.gatheringSessionsCompleted++;
        _playerStatistics[playerGuid].gatheringSessionsCompleted++;
        _globalStatistics.timeSpentGathering += session.GetDuration();
        _playerStatistics[playerGuid].timeSpentGathering += session.GetDuration();

        TC_LOG_INFO("playerbots", "GatheringMaterialsBridge: Session {} completed successfully - gathered {}/{} in {} ms",
            sessionId, session.gatheredSoFar, session.targetQuantity, session.GetDuration());
    }
    else
    {
        TC_LOG_WARN("playerbots", "GatheringMaterialsBridge: Session {} failed - only gathered {}/{} in {} ms",
            sessionId, session.gatheredSoFar, session.targetQuantity, session.GetDuration());
    }

    // Remove from active sessions
    _playerActiveSessions.erase(playerGuid);
    _activeSessions.erase(sessionId);
}

} // namespace Playerbot
