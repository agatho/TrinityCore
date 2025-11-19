/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AuctionMaterialsBridge.h"
#include "GatheringMaterialsBridge.h"
#include "ProfessionAuctionBridge.h"
#include "ProfessionManager.h"
#include "../Professions/GatheringManager.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Log.h"
#include <algorithm>
#include <sstream>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

AuctionMaterialsBridge* AuctionMaterialsBridge::instance()
{
    static AuctionMaterialsBridge instance;
    return &instance;
}

AuctionMaterialsBridge::AuctionMaterialsBridge()
{
}

// ============================================================================
// CORE BRIDGE MANAGEMENT
// ============================================================================

void AuctionMaterialsBridge::Initialize()
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    TC_LOG_INFO("playerbot", "AuctionMaterialsBridge::Initialize - Initializing smart material sourcing system");

    LoadVendorMaterials();
    InitializeDefaultEconomicParameters();

    TC_LOG_INFO("playerbot", "AuctionMaterialsBridge::Initialize - Smart material sourcing system initialized");
}

void AuctionMaterialsBridge::Update(::Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    // Check if enabled for this player
    auto profileItr = _economicProfiles.find(playerGuid);
    if (profileItr == _economicProfiles.end())
        return;

    // Throttle updates
    uint32 now = GameTime::GetGameTimeMS();
    auto lastUpdateItr = _lastUpdateTimes.find(playerGuid);
    if (lastUpdateItr != _lastUpdateTimes.end())
    {
        if (now - lastUpdateItr->second < DECISION_UPDATE_INTERVAL)
            return;
    }
    _lastUpdateTimes[playerGuid] = now;

    // Check if we have an active plan to execute
    auto planItr = _activePlans.find(playerGuid);
    if (planItr != _activePlans.end() && profileItr->second.autoExecutePlans)
    {
        ExecuteAcquisitionPlan(player, planItr->second);
    }
}

void AuctionMaterialsBridge::SetEnabled(::Player* player, bool enabled)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    if (enabled)
    {
        // Create default profile if doesn't exist
        if (_economicProfiles.find(playerGuid) == _economicProfiles.end())
        {
            _economicProfiles[playerGuid] = BotEconomicProfile();
        }
    }
    else
    {
        _economicProfiles.erase(playerGuid);
        _activePlans.erase(playerGuid);
    }
}

bool AuctionMaterialsBridge::IsEnabled(::Player* player) const
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    return _economicProfiles.find(playerGuid) != _economicProfiles.end();
}

void AuctionMaterialsBridge::SetEconomicProfile(uint32 playerGuid, BotEconomicProfile const& profile)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _economicProfiles[playerGuid] = profile;
}

BotEconomicProfile AuctionMaterialsBridge::GetEconomicProfile(uint32 playerGuid) const
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _economicProfiles.find(playerGuid);
    if (itr != _economicProfiles.end())
        return itr->second;

    return BotEconomicProfile();
}

// ============================================================================
// MATERIAL SOURCING DECISIONS
// ============================================================================

MaterialSourcingDecision AuctionMaterialsBridge::GetBestMaterialSource(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    MaterialSourcingDecision decision;
    decision.itemId = itemId;
    decision.quantityNeeded = quantity;

    if (!player)
    {
        decision.rationale = "Invalid player";
        return decision;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    // Get economic profile
    auto profileItr = _economicProfiles.find(playerGuid);
    EconomicParameters params;
    MaterialSourcingStrategy strategy = MaterialSourcingStrategy::BALANCED;

    if (profileItr != _economicProfiles.end())
    {
        params = profileItr->second.parameters;
        strategy = profileItr->second.strategy;
    }

    // Analyze all acquisition methods
    decision.canGather = CanGatherMaterial(player, itemId);
    decision.canBuyAuction = IsMaterialAvailableOnAH(player, itemId, quantity);
    decision.canCraft = CanCraftMaterial(player, itemId);
    decision.canBuyVendor = IsAvailableFromVendor(itemId);

    // Calculate costs for each method
    if (decision.canGather)
    {
        decision.gatheringTimeEstimate = EstimateGatheringTime(player, itemId, quantity);
        decision.gatheringTimeCost = CalculateGatheringTimeCost(player, itemId, quantity);
    }

    if (decision.canBuyAuction)
    {
        decision.auctionCost = GetAuctionPrice(player, itemId, quantity);
        decision.auctionTimeEstimate = EstimateAuctionPurchaseTime(player);
    }

    if (decision.canCraft)
    {
        decision.craftingCost = CalculateCraftingCost(player, itemId, quantity);
        decision.craftingTimeEstimate = EstimateCraftingTime(player, itemId, quantity);
    }

    if (decision.canBuyVendor)
    {
        decision.vendorCost = GetVendorPrice(itemId) * quantity;
    }

    // Apply strategy-based decision logic
    switch (strategy)
    {
        case MaterialSourcingStrategy::ALWAYS_GATHER:
            if (decision.canGather)
            {
                decision.recommendedMethod = MaterialAcquisitionMethod::GATHER;
                decision.alternativeMethod = decision.canBuyAuction ? MaterialAcquisitionMethod::BUY_AUCTION : MaterialAcquisitionMethod::NONE;
            }
            break;

        case MaterialSourcingStrategy::ALWAYS_BUY:
            if (decision.canBuyAuction)
                decision.recommendedMethod = MaterialAcquisitionMethod::BUY_AUCTION;
            else if (decision.canBuyVendor)
                decision.recommendedMethod = MaterialAcquisitionMethod::VENDOR;
            else if (decision.canGather)
                decision.recommendedMethod = MaterialAcquisitionMethod::GATHER;
            break;

        case MaterialSourcingStrategy::COST_OPTIMIZED:
        {
            // Choose cheapest method considering time value
            float bestScore = -1.0f;
            MaterialAcquisitionMethod bestMethod = MaterialAcquisitionMethod::NONE;

            if (decision.canGather)
            {
                float score = ScoreAcquisitionMethod(player, MaterialAcquisitionMethod::GATHER, itemId, quantity, params);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestMethod = MaterialAcquisitionMethod::GATHER;
                }
            }

            if (decision.canBuyAuction)
            {
                float score = ScoreAcquisitionMethod(player, MaterialAcquisitionMethod::BUY_AUCTION, itemId, quantity, params);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestMethod = MaterialAcquisitionMethod::BUY_AUCTION;
                }
            }

            if (decision.canCraft)
            {
                float score = ScoreAcquisitionMethod(player, MaterialAcquisitionMethod::CRAFT, itemId, quantity, params);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestMethod = MaterialAcquisitionMethod::CRAFT;
                }
            }

            if (decision.canBuyVendor)
            {
                float score = ScoreAcquisitionMethod(player, MaterialAcquisitionMethod::VENDOR, itemId, quantity, params);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestMethod = MaterialAcquisitionMethod::VENDOR;
                }
            }

            decision.recommendedMethod = bestMethod;
            break;
        }

        case MaterialSourcingStrategy::TIME_OPTIMIZED:
        {
            // Choose fastest method
            uint32 fastestTime = UINT32_MAX;
            MaterialAcquisitionMethod fastestMethod = MaterialAcquisitionMethod::NONE;

            if (decision.canBuyVendor)
            {
                fastestTime = 10; // Instant
                fastestMethod = MaterialAcquisitionMethod::VENDOR;
            }

            if (decision.canBuyAuction && decision.auctionTimeEstimate < fastestTime)
            {
                fastestTime = decision.auctionTimeEstimate;
                fastestMethod = MaterialAcquisitionMethod::BUY_AUCTION;
            }

            if (decision.canCraft && decision.craftingTimeEstimate < fastestTime)
            {
                fastestTime = decision.craftingTimeEstimate;
                fastestMethod = MaterialAcquisitionMethod::CRAFT;
            }

            if (decision.canGather && decision.gatheringTimeEstimate < fastestTime)
            {
                fastestTime = decision.gatheringTimeEstimate;
                fastestMethod = MaterialAcquisitionMethod::GATHER;
            }

            decision.recommendedMethod = fastestMethod;
            break;
        }

        case MaterialSourcingStrategy::BALANCED:
        default:
        {
            // Balance between cost and time
            // Vendor is always preferred if available (instant + cheap)
            if (decision.canBuyVendor)
            {
                decision.recommendedMethod = MaterialAcquisitionMethod::VENDOR;
            }
            // Buy from AH if significantly cheaper than gathering time value
            else if (decision.canBuyAuction && decision.canGather)
            {
                uint32 totalAuctionCost = decision.auctionCost + (decision.auctionTimeEstimate * params.goldPerHour / 3600);
                uint32 totalGatheringCost = decision.gatheringTimeCost;

                if (totalAuctionCost < totalGatheringCost * 0.8f) // 20% savings threshold
                {
                    decision.recommendedMethod = MaterialAcquisitionMethod::BUY_AUCTION;
                    decision.alternativeMethod = MaterialAcquisitionMethod::GATHER;
                }
                else
                {
                    decision.recommendedMethod = MaterialAcquisitionMethod::GATHER;
                    decision.alternativeMethod = MaterialAcquisitionMethod::BUY_AUCTION;
                }
            }
            else if (decision.canBuyAuction)
            {
                decision.recommendedMethod = MaterialAcquisitionMethod::BUY_AUCTION;
            }
            else if (decision.canGather)
            {
                decision.recommendedMethod = MaterialAcquisitionMethod::GATHER;
            }
            else if (decision.canCraft)
            {
                decision.recommendedMethod = MaterialAcquisitionMethod::CRAFT;
            }
            break;
        }
    }

    // Calculate opportunity cost and net benefit
    decision.opportunityCost = CalculateOpportunityCost(player, decision.recommendedMethod, itemId, quantity);
    decision.netBenefit = -decision.opportunityCost; // Simplified

    // Generate rationale and confidence
    decision.rationale = GenerateDecisionRationale(decision);
    decision.decisionConfidence = CalculateDecisionConfidence(player, decision);

    // Update statistics
    switch (decision.recommendedMethod)
    {
        case MaterialAcquisitionMethod::GATHER:
            _globalStatistics.decisionsGather++;
            _playerStatistics[playerGuid].decisionsGather++;
            break;
        case MaterialAcquisitionMethod::BUY_AUCTION:
            _globalStatistics.decisionsBuy++;
            _playerStatistics[playerGuid].decisionsBuy++;
            break;
        case MaterialAcquisitionMethod::CRAFT:
            _globalStatistics.decisionsCraft++;
            _playerStatistics[playerGuid].decisionsCraft++;
            break;
        case MaterialAcquisitionMethod::VENDOR:
            _globalStatistics.decisionsVendor++;
            _playerStatistics[playerGuid].decisionsVendor++;
            break;
        default:
            break;
    }

    return decision;
}

MaterialAcquisitionPlan AuctionMaterialsBridge::GetMaterialAcquisitionPlan(
    ::Player* player,
    uint32 recipeId)
{
    MaterialAcquisitionPlan plan;
    plan.recipeId = recipeId;

    if (!player)
        return plan;

    // Get recipe info from ProfessionManager
    ProfessionManager* profMgr = ProfessionManager::instance();
    if (!profMgr)
        return plan;

    // Find recipe across all professions
    static const std::vector<ProfessionType> allProfessions = {
        ProfessionType::ALCHEMY, ProfessionType::BLACKSMITHING,
        ProfessionType::ENCHANTING, ProfessionType::ENGINEERING,
        ProfessionType::INSCRIPTION, ProfessionType::JEWELCRAFTING,
        ProfessionType::LEATHERWORKING, ProfessionType::TAILORING,
        ProfessionType::COOKING
    };

    RecipeInfo const* recipe = nullptr;
    for (ProfessionType profession : allProfessions)
    {
        auto recipes = profMgr->GetRecipesForProfession(profession);
        for (const RecipeInfo& r : recipes)
        {
            if (r.recipeId == recipeId)
            {
                recipe = &r;
                plan.profession = profession;
                break;
            }
        }
        if (recipe)
            break;
    }

    if (!recipe)
        return plan;

    // Get material decisions for each reagent
    for (const RecipeInfo::Reagent& reagent : recipe->reagents)
    {
        MaterialSourcingDecision decision = GetBestMaterialSource(player, reagent.itemId, reagent.quantity);
        plan.materialDecisions.push_back(decision);

        // Accumulate costs and time
        switch (decision.recommendedMethod)
        {
            case MaterialAcquisitionMethod::GATHER:
                plan.totalCost += decision.gatheringTimeCost;
                plan.totalTime += decision.gatheringTimeEstimate;
                break;
            case MaterialAcquisitionMethod::BUY_AUCTION:
                plan.totalCost += decision.auctionCost;
                plan.totalTime += decision.auctionTimeEstimate;
                break;
            case MaterialAcquisitionMethod::CRAFT:
                plan.totalCost += decision.craftingCost;
                plan.totalTime += decision.craftingTimeEstimate;
                break;
            case MaterialAcquisitionMethod::VENDOR:
                plan.totalCost += decision.vendorCost;
                plan.totalTime += 10; // Instant
                break;
            default:
                break;
        }
    }

    // Calculate efficiency scores
    if (plan.totalTime > 0)
    {
        plan.timeScore = (float)plan.totalTime / 600.0f; // Normalize to 10 minutes
        plan.costScore = (float)plan.totalCost / 100000.0f; // Normalize to 10 gold

        // Overall efficiency (lower is better)
        plan.efficiencyScore = 1.0f / (1.0f + plan.timeScore + plan.costScore);
    }

    std::lock_guard<decltype(_mutex)> lock(_mutex);
    _globalStatistics.plansGenerated++;

    return plan;
}

MaterialAcquisitionPlan AuctionMaterialsBridge::GetLevelingMaterialPlan(
    ::Player* player,
    ProfessionType profession,
    uint32 targetSkill)
{
    MaterialAcquisitionPlan plan;
    plan.profession = profession;

    if (!player)
        return plan;

    ProfessionManager* profMgr = ProfessionManager::instance();
    if (!profMgr)
        return plan;

    // Get optimal leveling recipe
    RecipeInfo const* recipe = profMgr->GetOptimalLevelingRecipe(player, profession);
    if (!recipe)
        return plan;

    return GetMaterialAcquisitionPlan(player, recipe->recipeId);
}

// ============================================================================
// ECONOMIC ANALYSIS
// ============================================================================

bool AuctionMaterialsBridge::IsBuyingCheaperThanGathering(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return false;

    // Get auction price
    uint32 auctionCost = GetAuctionPrice(player, itemId, quantity);
    if (auctionCost == 0)
        return false; // Not available

    // Get gathering time cost
    uint32 gatheringCost = CalculateGatheringTimeCost(player, itemId, quantity);
    if (gatheringCost == 0)
        return true; // Can't gather, so buying is the only option

    // Add auction travel time
    uint32 auctionTime = EstimateAuctionPurchaseTime(player);
    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    EconomicParameters params;
    auto profileItr = _economicProfiles.find(playerGuid);
    if (profileItr != _economicProfiles.end())
        params = profileItr->second.parameters;

    uint32 totalAuctionCost = auctionCost + (auctionTime * params.goldPerHour / 3600);

    return totalAuctionCost < gatheringCost;
}

uint32 AuctionMaterialsBridge::CalculateGatheringTimeCost(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return 0;

    // Estimate gathering time
    uint32 gatheringTime = EstimateGatheringTime(player, itemId, quantity);
    if (gatheringTime == 0)
        return 0;

    // Get bot's gold per hour rate
    float goldPerHour = GetBotGoldPerHour(player);

    // Time value cost = (time in seconds) * (gold per hour / 3600)
    uint32 timeCost = static_cast<uint32>(gatheringTime * (goldPerHour / 3600.0f));

    return timeCost;
}

float AuctionMaterialsBridge::CalculateOpportunityCost(
    ::Player* player,
    MaterialAcquisitionMethod method,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return 0.0f;

    float goldPerHour = GetBotGoldPerHour(player);
    uint32 timeSpent = 0;

    switch (method)
    {
        case MaterialAcquisitionMethod::GATHER:
            timeSpent = EstimateGatheringTime(player, itemId, quantity);
            break;
        case MaterialAcquisitionMethod::BUY_AUCTION:
            timeSpent = EstimateAuctionPurchaseTime(player);
            break;
        case MaterialAcquisitionMethod::CRAFT:
            timeSpent = EstimateCraftingTime(player, itemId, quantity);
            break;
        case MaterialAcquisitionMethod::VENDOR:
            timeSpent = 10; // Minimal time
            break;
        default:
            break;
    }

    // Opportunity cost = what else could be done with that time
    float opportunityCost = timeSpent * (goldPerHour / 3600.0f);

    return opportunityCost;
}

float AuctionMaterialsBridge::GetBotGoldPerHour(::Player* player)
{
    if (!player)
        return DEFAULT_GOLD_PER_HOUR;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _economicProfiles.find(playerGuid);
    if (profileItr != _economicProfiles.end())
        return profileItr->second.parameters.goldPerHour;

    // Estimate based on level
    uint8 level = player->GetLevel();
    if (level >= 80)
        return 150.0f * 10000.0f; // 150 gold/hour
    else if (level >= 70)
        return 100.0f * 10000.0f; // 100 gold/hour
    else if (level >= 60)
        return 75.0f * 10000.0f;  // 75 gold/hour
    else if (level >= 50)
        return 50.0f * 10000.0f;  // 50 gold/hour
    else
        return 25.0f * 10000.0f;  // 25 gold/hour

    return DEFAULT_GOLD_PER_HOUR;
}

// ============================================================================
// GATHERING FEASIBILITY ANALYSIS
// ============================================================================

bool AuctionMaterialsBridge::CanGatherMaterial(::Player* player, uint32 itemId)
{
    if (!player)
        return false;

    GatheringMaterialsBridge* gatherBridge = GetGatheringBridge();
    if (!gatherBridge)
        return false;

    return gatherBridge->IsItemNeededForCrafting(player, itemId);
}

uint32 AuctionMaterialsBridge::EstimateGatheringTime(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return 0;

    // Base time per node (average)
    uint32 timePerNode = 60; // 1 minute per node

    // Estimate yield per node
    uint32 yieldPerNode = 2; // Conservative estimate

    // Calculate number of nodes needed
    uint32 nodesNeeded = (quantity + yieldPerNode - 1) / yieldPerNode;

    // Total time = nodes * (travel time + gather time)
    uint32 totalTime = nodesNeeded * timePerNode;

    // Apply gathering efficiency
    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    float efficiency = DEFAULT_GATHERING_EFFICIENCY;
    auto profileItr = _economicProfiles.find(playerGuid);
    if (profileItr != _economicProfiles.end())
        efficiency = profileItr->second.parameters.gatheringEfficiency;

    // Adjust for efficiency
    totalTime = static_cast<uint32>(totalTime / efficiency);

    return totalTime;
}

float AuctionMaterialsBridge::GetGatheringSuccessProbability(
    ::Player* player,
    uint32 itemId)
{
    if (!player)
        return 0.0f;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto profileItr = _economicProfiles.find(playerGuid);
    if (profileItr != _economicProfiles.end())
        return profileItr->second.parameters.gatheringEfficiency;

    return DEFAULT_GATHERING_EFFICIENCY;
}

// ============================================================================
// AUCTION HOUSE ANALYSIS
// ============================================================================

bool AuctionMaterialsBridge::IsMaterialAvailableOnAH(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return false;

    ProfessionAuctionBridge* auctionBridge = GetAuctionBridge();
    if (!auctionBridge)
        return false;

    uint32 maxPricePerUnit = UINT32_MAX;

    return auctionBridge->IsMaterialAvailableForPurchase(player, itemId, quantity, maxPricePerUnit);
}

uint32 AuctionMaterialsBridge::GetAuctionPrice(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return 0;

    ProfessionAuctionBridge* auctionBridge = GetAuctionBridge();
    if (!auctionBridge)
        return 0;

    return auctionBridge->GetOptimalMaterialPrice(player, itemId, quantity);
}

uint32 AuctionMaterialsBridge::EstimateAuctionPurchaseTime(::Player* player)
{
    if (!player)
        return 0;

    // Travel time to AH + transaction time
    return AUCTION_HOUSE_TRAVEL_TIME + 30; // 2.5 minutes total
}

// ============================================================================
// CRAFTING ANALYSIS
// ============================================================================

bool AuctionMaterialsBridge::CanCraftMaterial(::Player* player, uint32 itemId)
{
    if (!player)
        return false;

    // Some materials can be crafted (e.g., enchanting reagents)
    // Check if player knows any recipe that produces this item
    ProfessionManager* profMgr = ProfessionManager::instance();
    if (!profMgr)
        return false;

    static const std::vector<ProfessionType> allProfessions = {
        ProfessionType::ALCHEMY, ProfessionType::BLACKSMITHING,
        ProfessionType::ENCHANTING, ProfessionType::ENGINEERING,
        ProfessionType::INSCRIPTION, ProfessionType::JEWELCRAFTING,
        ProfessionType::LEATHERWORKING, ProfessionType::TAILORING
    };

    for (ProfessionType profession : allProfessions)
    {
        auto recipes = profMgr->GetRecipesForProfession(profession);
        for (const RecipeInfo& recipe : recipes)
        {
            if (recipe.productItemId == itemId)
                return true;
        }
    }

    return false;
}

uint32 AuctionMaterialsBridge::CalculateCraftingCost(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return 0;

    // Find recipe and calculate reagent costs
    ProfessionManager* profMgr = ProfessionManager::instance();
    if (!profMgr)
        return 0;

    static const std::vector<ProfessionType> allProfessions = {
        ProfessionType::ALCHEMY, ProfessionType::BLACKSMITHING,
        ProfessionType::ENCHANTING, ProfessionType::ENGINEERING,
        ProfessionType::INSCRIPTION, ProfessionType::JEWELCRAFTING,
        ProfessionType::LEATHERWORKING, ProfessionType::TAILORING
    };

    for (ProfessionType profession : allProfessions)
    {
        auto recipes = profMgr->GetRecipesForProfession(profession);
        for (const RecipeInfo& recipe : recipes)
        {
            if (recipe.productItemId == itemId)
            {
                uint32 totalCost = 0;
                for (const RecipeInfo::Reagent& reagent : recipe.reagents)
                {
                    uint32 reagentCost = GetAuctionPrice(player, reagent.itemId, reagent.quantity * quantity);
                    totalCost += reagentCost;
                }

                // Add time value of crafting
                uint32 craftingTime = EstimateCraftingTime(player, itemId, quantity);
                float goldPerHour = GetBotGoldPerHour(player);
                totalCost += static_cast<uint32>(craftingTime * (goldPerHour / 3600.0f));

                return totalCost;
            }
        }
    }

    return 0;
}

uint32 AuctionMaterialsBridge::EstimateCraftingTime(
    ::Player* player,
    uint32 itemId,
    uint32 quantity)
{
    if (!player)
        return 0;

    // Crafting takes ~3 seconds per item
    return quantity * 3;
}

// ============================================================================
// VENDOR ANALYSIS
// ============================================================================

bool AuctionMaterialsBridge::IsAvailableFromVendor(uint32 itemId)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    return _vendorMaterials.find(itemId) != _vendorMaterials.end();
}

uint32 AuctionMaterialsBridge::GetVendorPrice(uint32 itemId)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _vendorMaterials.find(itemId);
    if (itr != _vendorMaterials.end())
        return itr->second;

    return 0;
}

// ============================================================================
// PLAN EXECUTION
// ============================================================================

bool AuctionMaterialsBridge::ExecuteAcquisitionPlan(
    ::Player* player,
    MaterialAcquisitionPlan const& plan)
{
    if (!player)
        return false;

    bool allSuccessful = true;

    for (const MaterialSourcingDecision& decision : plan.materialDecisions)
    {
        if (!AcquireMaterial(player, decision))
            allSuccessful = false;
    }

    if (allSuccessful)
    {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        _globalStatistics.plansExecuted++;
    }

    return allSuccessful;
}

bool AuctionMaterialsBridge::AcquireMaterial(
    ::Player* player,
    MaterialSourcingDecision const& decision)
{
    if (!player)
        return false;

    switch (decision.recommendedMethod)
    {
        case MaterialAcquisitionMethod::GATHER:
        {
            GatheringMaterialsBridge* gatherBridge = GetGatheringBridge();
            if (gatherBridge)
                return gatherBridge->StartGatheringForMaterial(player, decision.itemId, decision.quantityNeeded);
            break;
        }

        case MaterialAcquisitionMethod::BUY_AUCTION:
        {
            ProfessionAuctionBridge* auctionBridge = GetAuctionBridge();
            if (auctionBridge)
                return auctionBridge->PurchaseMaterial(player, decision.itemId, decision.quantityNeeded, UINT32_MAX);
            break;
        }

        case MaterialAcquisitionMethod::CRAFT:
        {
            // Delegate to ProfessionManager
            ProfessionManager* profMgr = ProfessionManager::instance();
            if (profMgr)
            {
                // This would trigger crafting automation
                // Implementation depends on ProfessionManager API
            }
            break;
        }

        case MaterialAcquisitionMethod::VENDOR:
        {
            // Bots can travel to vendors and purchase
            // Implementation would be in bot movement/vendor interaction
            break;
        }

        default:
            return false;
    }

    return false;
}

// ============================================================================
// STATISTICS
// ============================================================================

MaterialSourcingStatistics const& AuctionMaterialsBridge::GetPlayerStatistics(uint32 playerGuid) const
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _playerStatistics.find(playerGuid);
    if (itr != _playerStatistics.end())
        return itr->second;

    static MaterialSourcingStatistics empty;
    return empty;
}

MaterialSourcingStatistics const& AuctionMaterialsBridge::GetGlobalStatistics() const
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);
    return _globalStatistics;
}

void AuctionMaterialsBridge::ResetStatistics(uint32 playerGuid)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto itr = _playerStatistics.find(playerGuid);
    if (itr != _playerStatistics.end())
        itr->second.Reset();
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

void AuctionMaterialsBridge::LoadVendorMaterials()
{
    // Common vendor materials with prices (in copper)
    // This is a subset - full implementation would load from database

    // Crafting reagents
    _vendorMaterials[2880] = 10;    // Weak Flux (10 copper)
    _vendorMaterials[2901] = 100;   // Mining Pick (1 silver)
    _vendorMaterials[3371] = 400;   // Crystal Vial (4 silver)
    _vendorMaterials[4289] = 50;    // Salt (50 copper)
    _vendorMaterials[4340] = 350;   // Gray Dye (3.5 silver)
    _vendorMaterials[4341] = 500;   // Yellow Dye (5 silver)
    _vendorMaterials[4342] = 350;   // Purple Dye (3.5 silver)
    _vendorMaterials[5956] = 18;    // Blacksmith Hammer (18 copper)
    _vendorMaterials[6217] = 124;   // Copper Rod (1.24 silver)
    _vendorMaterials[6256] = 50;    // Fishing Pole (50 copper)
    _vendorMaterials[30817] = 2500; // Simple Flour (25 silver)

    // Thread
    _vendorMaterials[2320] = 10;    // Coarse Thread (10 copper)
    _vendorMaterials[2321] = 100;   // Fine Thread (1 silver)
    _vendorMaterials[4291] = 500;   // Silken Thread (5 silver)
    _vendorMaterials[8343] = 2000;  // Heavy Silken Thread (20 silver)
    _vendorMaterials[14341] = 10000; // Rune Thread (1 gold)

    TC_LOG_INFO("playerbot", "AuctionMaterialsBridge::LoadVendorMaterials - Loaded {} vendor materials",
        _vendorMaterials.size());
}

void AuctionMaterialsBridge::InitializeDefaultEconomicParameters()
{
    // Default economic parameters are set in EconomicParameters constructor
    TC_LOG_INFO("playerbot", "AuctionMaterialsBridge::InitializeDefaultEconomicParameters - Initialized default parameters");
}

// ============================================================================
// DECISION ALGORITHM HELPERS
// ============================================================================

float AuctionMaterialsBridge::ScoreAcquisitionMethod(
    ::Player* player,
    MaterialAcquisitionMethod method,
    uint32 itemId,
    uint32 quantity,
    EconomicParameters const& params)
{
    if (!player)
        return 0.0f;

    float score = 100.0f;

    switch (method)
    {
        case MaterialAcquisitionMethod::GATHER:
        {
            uint32 timeCost = CalculateGatheringTimeCost(player, itemId, quantity);
            // Lower time cost = higher score
            score = 100.0f / (1.0f + timeCost / 10000.0f);

            if (params.preferGathering)
                score *= 1.5f;
            break;
        }

        case MaterialAcquisitionMethod::BUY_AUCTION:
        {
            uint32 goldCost = GetAuctionPrice(player, itemId, quantity);
            // Lower gold cost = higher score
            score = 100.0f / (1.0f + goldCost / 10000.0f);

            if (params.preferSpeed)
                score *= 1.3f;
            break;
        }

        case MaterialAcquisitionMethod::CRAFT:
        {
            uint32 totalCost = CalculateCraftingCost(player, itemId, quantity);
            score = 100.0f / (1.0f + totalCost / 10000.0f);
            break;
        }

        case MaterialAcquisitionMethod::VENDOR:
        {
            uint32 vendorCost = GetVendorPrice(itemId) * quantity;
            // Vendor is always reliable and instant - bonus score
            score = 150.0f / (1.0f + vendorCost / 10000.0f);
            break;
        }

        default:
            score = 0.0f;
            break;
    }

    return score;
}

std::string AuctionMaterialsBridge::GenerateDecisionRationale(
    MaterialSourcingDecision const& decision)
{
    std::ostringstream oss;

    oss << "Recommended: ";

    switch (decision.recommendedMethod)
    {
        case MaterialAcquisitionMethod::GATHER:
            oss << "GATHER (time: " << decision.gatheringTimeEstimate
                << "s, cost: " << decision.gatheringTimeCost / 10000.0f << "g)";
            break;
        case MaterialAcquisitionMethod::BUY_AUCTION:
            oss << "BUY_AUCTION (time: " << decision.auctionTimeEstimate
                << "s, cost: " << decision.auctionCost / 10000.0f << "g)";
            break;
        case MaterialAcquisitionMethod::CRAFT:
            oss << "CRAFT (time: " << decision.craftingTimeEstimate
                << "s, cost: " << decision.craftingCost / 10000.0f << "g)";
            break;
        case MaterialAcquisitionMethod::VENDOR:
            oss << "VENDOR (instant, cost: " << decision.vendorCost / 10000.0f << "g)";
            break;
        default:
            oss << "NONE (no feasible method)";
            break;
    }

    if (decision.alternativeMethod != MaterialAcquisitionMethod::NONE)
    {
        oss << " | Alternative: ";
        switch (decision.alternativeMethod)
        {
            case MaterialAcquisitionMethod::GATHER:
                oss << "GATHER";
                break;
            case MaterialAcquisitionMethod::BUY_AUCTION:
                oss << "BUY_AUCTION";
                break;
            case MaterialAcquisitionMethod::CRAFT:
                oss << "CRAFT";
                break;
            default:
                break;
        }
    }

    return oss.str();
}

float AuctionMaterialsBridge::CalculateDecisionConfidence(
    ::Player* player,
    MaterialSourcingDecision const& decision)
{
    if (!player)
        return 0.0f;

    float confidence = 1.0f;

    // Reduce confidence if no feasible method
    if (decision.recommendedMethod == MaterialAcquisitionMethod::NONE)
        return 0.0f;

    // Reduce confidence if costs are estimated (not real data)
    if (decision.recommendedMethod == MaterialAcquisitionMethod::GATHER)
    {
        if (decision.gatheringTimeEstimate == 0)
            confidence *= 0.5f;
    }

    // High confidence for vendor purchases (always reliable)
    if (decision.recommendedMethod == MaterialAcquisitionMethod::VENDOR)
        confidence = 1.0f;

    return confidence;
}

// ============================================================================
// INTEGRATION HELPERS
// ============================================================================

GatheringMaterialsBridge* AuctionMaterialsBridge::GetGatheringBridge() const
{
    return GatheringMaterialsBridge::instance();
}

ProfessionAuctionBridge* AuctionMaterialsBridge::GetAuctionBridge() const
{
    return ProfessionAuctionBridge::instance();
}

} // namespace Playerbot
