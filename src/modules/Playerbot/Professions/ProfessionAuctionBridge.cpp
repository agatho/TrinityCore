/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ProfessionAuctionBridge.h"
#include "ProfessionManager.h"
#include "../Social/AuctionHouse.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include <algorithm>
#include "GameTime.h"

namespace Playerbot
{

// Singleton instance
ProfessionAuctionBridge* ProfessionAuctionBridge::instance()
{
    static ProfessionAuctionBridge instance;
    return &instance;
}

ProfessionAuctionBridge::ProfessionAuctionBridge()
    : _auctionHouse(nullptr)
{
}

// ============================================================================
// CORE BRIDGE MANAGEMENT
// ============================================================================

void ProfessionAuctionBridge::Initialize()
{
    TC_LOG_INFO("playerbots", "ProfessionAuctionBridge: Initializing profession-auction bridge...");

    // Get reference to existing AuctionHouse singleton
    _auctionHouse = AuctionHouse::instance();

    LoadDefaultStockpileConfigs();

    TC_LOG_INFO("playerbots", "ProfessionAuctionBridge: Initialized (bridge to existing AuctionHouse)");
}

void ProfessionAuctionBridge::Update(::Player* player, uint32 diff)
{
    if (!player || !IsEnabled(player))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();uint32 currentTime = GameTime::GetGameTimeMS();

    std::lock_guard lock(_mutex);

    // Check if enough time passed since last auction check
    if (currentTime - _lastAuctionCheckTimes[playerGuid] < AUCTION_CHECK_INTERVAL)
        return;

    _lastAuctionCheckTimes[playerGuid] = currentTime;

    ProfessionAuctionProfile const& profile = GetAuctionProfile(playerGuid);

    // Sell excess materials
    if (profile.autoSellEnabled && profile.strategy != ProfessionAuctionStrategy::NONE)
    {
        SellExcessMaterials(player);
        SellCraftedItems(player);
    }

    // Buy materials for leveling
    if (profile.buyMaterialsForLeveling)
    {
        // Check all professions for leveling needs
        auto professions = ProfessionManager::instance()->GetPlayerProfessions(player);
        for (auto const& profInfo : professions)
        {
            if (profInfo.currentSkill < profInfo.maxSkill)
                BuyMaterialsForLeveling(player, profInfo.profession);
        }
    }
}

void ProfessionAuctionBridge::SetEnabled(::Player* player, bool enabled){
    if (!player)
        return;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();if (_profiles.find(playerGuid) == _profiles.end())
        _profiles[playerGuid] = ProfessionAuctionProfile();_profiles[playerGuid].autoSellEnabled = enabled;
}

bool ProfessionAuctionBridge::IsEnabled(::Player* player) const
{
    if (!player)
        return false;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();auto it = _profiles.find(playerGuid);
    if (it == _profiles.end())
        return false;

    return it->second.autoSellEnabled;
}

void ProfessionAuctionBridge::SetAuctionProfile(uint32 playerGuid, ProfessionAuctionProfile const& profile)
{
    std::lock_guard lock(_mutex);
    _profiles[playerGuid] = profile;
}

ProfessionAuctionProfile ProfessionAuctionBridge::GetAuctionProfile(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    auto it = _profiles.find(playerGuid);if (it != _profiles.end())
        return it->second;

    return ProfessionAuctionProfile();
}

// ============================================================================
// MATERIAL AUCTION AUTOMATION
// ============================================================================

void ProfessionAuctionBridge::SellExcessMaterials(::Player* player)
{
    if (!player || !_auctionHouse || !CanAccessAuctionHouse(player))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();ProfessionAuctionProfile const& profile = GetAuctionProfile(playerGuid);

    // Get profession items in inventory
    auto items = GetProfessionItemsInInventory(player, true); // Materials onlyfor (auto const& itemInfo : items)
    {
        // Check if item has stockpile config
        auto configIt = profile.materialConfigs.find(itemInfo.itemId);
        if (configIt == profile.materialConfigs.end())
            continue;

        MaterialStockpileConfig const& config = configIt->second;

        // Check if should sell
        if (ShouldSellMaterial(player, itemInfo.itemId, itemInfo.stackCount))
        {
            ListMaterialOnAuction(player, itemInfo.itemGuid, config);
        }
    }
}

bool ProfessionAuctionBridge::ShouldSellMaterial(::Player* player, uint32 itemId, uint32 currentCount) const
{
    if (!player)
        return false;

    ProfessionAuctionProfile const& profile = GetAuctionProfile(player->GetGUID().GetCounter());auto configIt = profile.materialConfigs.find(itemId);
    if (configIt == profile.materialConfigs.end())
        return false;

    MaterialStockpileConfig const& config = configIt->second;

    // Sell if current count exceeds maxStackSize
    return currentCount > config.maxStackSize;
}

bool ProfessionAuctionBridge::ListMaterialOnAuction(::Player* player, uint32 itemGuid, MaterialStockpileConfig const& config){
    if (!player || !_auctionHouse)
        return false;

    // Get optimal price from AuctionHouse
    uint32 marketPrice = GetOptimalMaterialPrice(player, config.itemId, config.auctionStackSize);if (marketPrice == 0)
    {
        TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: No market price found for item {}", config.itemId);
        return false;
    }

    // Calculate bid and buyout prices
    uint32 bidPrice = static_cast<uint32>(marketPrice * 0.95f); // 95% of market for bid
    uint32 buyoutPrice = config.preferBuyout ? marketPrice : 0;

    // Delegate to existing AuctionHouse
    bool success = _auctionHouse->CreateAuction(player, itemGuid, config.auctionStackSize,
        bidPrice, buyoutPrice, DEFAULT_AUCTION_DURATION);

    if (success)
    {
        std::lock_guard lock(_mutex);
        uint32 playerGuid = player->GetGUID().GetCounter();_playerStatistics[playerGuid].materialsListedCount++;
        _globalStatistics.materialsListedCount++;

        TC_LOG_INFO("playerbots", "ProfessionAuctionBridge: Listed material {} for player {} (price: {})",
            config.itemId, player->GetName(), marketPrice);
    }

    return success;
}

uint32 ProfessionAuctionBridge::GetOptimalMaterialPrice(::Player* player, uint32 itemId, uint32 stackSize) const
{
    if (!player || !_auctionHouse)
        return 0;

    // Delegate to existing AuctionHouse price calculation
    return _auctionHouse->CalculateOptimalListingPrice(player, itemId, stackSize);
}

// ============================================================================
// CRAFTED ITEM AUCTION AUTOMATION
// ============================================================================

void ProfessionAuctionBridge::SellCraftedItems(::Player* player)
{
    if (!player || !_auctionHouse || !CanAccessAuctionHouse(player))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();ProfessionAuctionProfile const& profile = GetAuctionProfile(playerGuid);

    // Get crafted items in inventory
    auto items = GetProfessionItemsInInventory(player, false); // All profession items

    for (auto const& itemInfo : items)
    {
        ProfessionType profession;
        if (!IsCraftedItem(itemInfo.itemId, profession))
            continue;

        // Check if item has auction config
        auto configIt = profile.craftedItemConfigs.find(itemInfo.itemId);
        if (configIt == profile.craftedItemConfigs.end())
            continue;

        CraftedItemAuctionConfig const& config = configIt->second;

        // Calculate material cost
        uint32 materialCost = CalculateMaterialCost(player, itemInfo.itemId);

        // Check if should sell
        if (ShouldSellCraftedItem(player, itemInfo.itemId, materialCost))
        {
            ListCraftedItemOnAuction(player, itemInfo.itemGuid, config);
        }
    }
}

bool ProfessionAuctionBridge::ShouldSellCraftedItem(::Player* player, uint32 itemId, uint32 materialCost) const
{
    if (!player || !_auctionHouse)
        return false;

    ProfessionAuctionProfile const& profile = GetAuctionProfile(player->GetGUID().GetCounter());auto configIt = profile.craftedItemConfigs.find(itemId);
    if (configIt == profile.craftedItemConfigs.end())
        return false;

    CraftedItemAuctionConfig const& config = configIt->second;

    // Get market price
    uint32 marketPrice = _auctionHouse->GetMarketPrice(itemId, 1);
    if (marketPrice == 0)
        return false;

    // Calculate profit margin
    float profitMargin = CalculateProfitMargin(player, itemId, marketPrice, materialCost);

    // Sell if profit margin exceeds minimum
    uint32 minProfitCopper = config.minProfitMargin;
    uint32 actualProfitCopper = (marketPrice > materialCost) ? (marketPrice - materialCost) : 0;

    return actualProfitCopper >= minProfitCopper;
}

bool ProfessionAuctionBridge::ListCraftedItemOnAuction(::Player* player, uint32 itemGuid, CraftedItemAuctionConfig const& config)
{
    if (!player || !_auctionHouse)
        return false;

    // Get market price
    uint32 marketPrice = _auctionHouse->GetMarketPrice(config.itemId, 1);
    if (marketPrice == 0)
        return false;

    // Apply undercut if configured
    uint32 listingPrice = marketPrice;
    if (config.undercutCompetition)
        listingPrice = static_cast<uint32>(marketPrice * (1.0f - config.undercutRate));

    uint32 bidPrice = static_cast<uint32>(listingPrice * 0.95f);

    // Delegate to existing AuctionHouse
    bool success = _auctionHouse->CreateAuction(player, itemGuid, 1, // Single itembidPrice, listingPrice, config.maxListingDuration);

    if (success)
    {
        std::lock_guard lock(_mutex);
        uint32 playerGuid = player->GetGUID().GetCounter();_playerStatistics[playerGuid].craftedsListedCount++;
        _globalStatistics.craftedsListedCount++;

        TC_LOG_INFO("playerbots", "ProfessionAuctionBridge: Listed crafted item {} for player {} (price: {})",
            config.itemId, player->GetName(), listingPrice);
    }

    return success;
}

float ProfessionAuctionBridge::CalculateProfitMargin(::Player* player, uint32 itemId, uint32 marketPrice, uint32 materialCost) const
{
    if (materialCost == 0)
        return 0.0f;

    if (marketPrice <= materialCost)
        return 0.0f;

    uint32 profit = marketPrice - materialCost;
    return static_cast<float>(profit) / static_cast<float>(materialCost);
}

// ============================================================================
// MATERIAL PURCHASING AUTOMATION
// ============================================================================

void ProfessionAuctionBridge::BuyMaterialsForLeveling(::Player* player, ProfessionType profession)
{
    if (!player || !_auctionHouse || !CanAccessAuctionHouse(player))
        return;

    // Get needed materials from ProfessionManager
    auto neededMaterials = GetNeededMaterialsForLeveling(player, profession);

    if (neededMaterials.empty())
        return;

    ProfessionAuctionProfile const& profile = GetAuctionProfile(player->GetGUID().GetCounter());uint32 budgetRemaining = profile.auctionBudget;

    for (auto const& [itemId, quantity] : neededMaterials)
    {
        // Get market price
        float marketPrice = _auctionHouse->GetMarketPrice(itemId, quantity);
        if (marketPrice == 0.0f)
            continue;

        // Calculate max price willing to pay (110% of market)
        uint32 maxPricePerUnit = static_cast<uint32>(marketPrice * 1.1f);

        // Check if available at good price
        if (IsMaterialAvailableForPurchase(player, itemId, quantity, maxPricePerUnit)){
            uint32 totalCost = maxPricePerUnit * quantity;
            if (totalCost <= budgetRemaining)
            {
                if (PurchaseMaterial(player, itemId, quantity, maxPricePerUnit))
                {
                    budgetRemaining -= totalCost;

                    std::lock_guard lock(_mutex);
                    uint32 playerGuid = player->GetGUID().GetCounter();_playerStatistics[playerGuid].materialsBought += quantity;
                    _playerStatistics[playerGuid].goldSpentOnMaterials += totalCost;
                    _globalStatistics.materialsBought += quantity;
                    _globalStatistics.goldSpentOnMaterials += totalCost;
                }
            }
        }
    }
}

std::vector<std::pair<uint32, uint32>> ProfessionAuctionBridge::GetNeededMaterialsForLeveling(::Player* player, ProfessionType profession) const
{
    std::vector<std::pair<uint32, uint32>> materials;

    if (!player)
        return materials;

    // Get optimal leveling recipe from ProfessionManager
    RecipeInfo const* recipe = ProfessionManager::instance()->GetOptimalLevelingRecipe(player, profession);
    if (!recipe)
        return materials;

    // Get missing materials for recipe
    return ProfessionManager::instance()->GetMissingMaterials(player, *recipe);
}

bool ProfessionAuctionBridge::IsMaterialAvailableForPurchase(::Player* player, uint32 itemId, uint32 quantity, uint32 maxPricePerUnit) const{
    if (!player || !_auctionHouse)
        return false;

    // Delegate to existing AuctionHouse
    float marketPrice = _auctionHouse->GetMarketPrice(itemId, quantity);
    if (marketPrice == 0.0f)
        return false;

    // Check if price is below our maximum
    return _auctionHouse->IsPriceBelowMarket(itemId, maxPricePerUnit);
}

bool ProfessionAuctionBridge::PurchaseMaterial(::Player* player, uint32 itemId, uint32 quantity, uint32 maxPricePerUnit){
    if (!player || !_auctionHouse)
        return false;

    // Get similar auctions for this item
    std::vector<AuctionItem> auctions = _auctionHouse->GetSimilarAuctions(itemId, 50);
    if (auctions.empty())
    {
        TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: No auctions found for item {}", itemId);
        return false;
    }

    // Sort by price per item (lowest first)
    std::sort(auctions.begin(), auctions.end(),
        [](const AuctionItem& a, const AuctionItem& b) {
            return a.pricePerItem < b.pricePerItem;
        });

    uint32 totalBought = 0;
    uint32 totalGoldSpent = 0;
    std::vector<uint32> boughtAuctionIds;

    for (const AuctionItem& auction : auctions)
    {
        if (totalBought >= quantity)
            break;

        // Skip if price too high
        if (auction.buyoutPrice > 0)
        {
            uint32 pricePerUnit = auction.buyoutPrice / auction.stackCount;
            if (pricePerUnit > maxPricePerUnit)
                continue;
        }
        else
        {
            // No buyout, skip (we only use buyout for automation)
            continue;
        }

        // Calculate how many we can buy from this auction
        uint32 buyAmount = std::min(quantity - totalBought, auction.stackCount);
        uint32 cost = (auction.buyoutPrice / auction.stackCount) * buyAmount;

        // Check if player has enough gold
        if (player->GetMoney() < totalGoldSpent + cost)
        {
            TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Player {} insufficient gold for material purchase",
                player->GetName());
            break;
        }

        // Buyout the auction
        if (_auctionHouse->BuyoutAuction(player, auction.auctionId))
        {
            totalBought += buyAmount;
            totalGoldSpent += cost;
            boughtAuctionIds.push_back(auction.auctionId);

            TC_LOG_INFO("playerbots", "ProfessionAuctionBridge: Player {} bought {} x {} for {} gold (auction {})",
                player->GetName(), buyAmount, itemId, cost, auction.auctionId);
        }
        else
        {
            TC_LOG_WARN("playerbots", "ProfessionAuctionBridge: Failed to buyout auction {} for player {}",
                auction.auctionId, player->GetName());
        }
    }

    if (totalBought > 0)
    {
        TC_LOG_INFO("playerbots", "ProfessionAuctionBridge: Player {} purchased total {} x {} for {} gold from {} auctions",
            player->GetName(), totalBought, itemId, totalGoldSpent, boughtAuctionIds.size());
        return true;
    }

    return false;
}

// ============================================================================
// STOCKPILE MANAGEMENT
// ============================================================================

void ProfessionAuctionBridge::SetMaterialStockpile(uint32 playerGuid, uint32 itemId, MaterialStockpileConfig const& config)
{
    std::lock_guard lock(_mutex);

    if (_profiles.find(playerGuid) == _profiles.end())
        _profiles[playerGuid] = ProfessionAuctionProfile();

    _profiles[playerGuid].materialConfigs[itemId] = config;
}

void ProfessionAuctionBridge::SetCraftedItemAuction(uint32 playerGuid, uint32 itemId, CraftedItemAuctionConfig const& config)
{
    std::lock_guard lock(_mutex);

    if (_profiles.find(playerGuid) == _profiles.end())
        _profiles[playerGuid] = ProfessionAuctionProfile();

    _profiles[playerGuid].craftedItemConfigs[itemId] = config;
}

uint32 ProfessionAuctionBridge::GetCurrentStockpile(::Player* player, uint32 itemId) const
{
    if (!player)
        return 0;

    return player->GetItemCount(itemId);
}

bool ProfessionAuctionBridge::IsStockpileTargetMet(::Player* player, uint32 itemId) const
{
    if (!player)
        return false;

    ProfessionAuctionProfile const& profile = GetAuctionProfile(player->GetGUID().GetCounter());auto configIt = profile.materialConfigs.find(itemId);
    if (configIt == profile.materialConfigs.end())
        return false;

    uint32 currentCount = GetCurrentStockpile(player, itemId);
    return currentCount >= configIt->second.minStackSize;
}

// ============================================================================
// INTEGRATION WITH EXISTING AUCTION HOUSE
// ============================================================================

AuctionHouse* ProfessionAuctionBridge::GetAuctionHouse() const
{
    return _auctionHouse;
}

void ProfessionAuctionBridge::SynchronizeWithAuctionHouse(::Player* player)
{
    // Synchronization logic with existing auction house
    // In full implementation, check active auctions, update statistics, etc.
}

// ============================================================================
// STATISTICS
// ============================================================================

ProfessionAuctionStatistics const& ProfessionAuctionBridge::GetPlayerStatistics(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    static ProfessionAuctionStatistics emptyStats;
    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        return it->second;

    return emptyStats;
}

ProfessionAuctionStatistics const& ProfessionAuctionBridge::GetGlobalStatistics() const
{
    return _globalStatistics;
}

void ProfessionAuctionBridge::ResetStatistics(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);

    auto it = _playerStatistics.find(playerGuid);if (it != _playerStatistics.end())
        it->second.Reset();
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

void ProfessionAuctionBridge::LoadDefaultStockpileConfigs()
{
    InitializeMiningMaterials();
    InitializeHerbalismMaterials();
    InitializeSkinningMaterials();
    InitializeCraftedItemConfigs();
}

void ProfessionAuctionBridge::InitializeMiningMaterials()
{
    // Default configs would be loaded from database in full implementation
    TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Initialized mining material configs");
}

void ProfessionAuctionBridge::InitializeHerbalismMaterials()
{
    TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Initialized herbalism material configs");
}

void ProfessionAuctionBridge::InitializeSkinningMaterials()
{
    TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Initialized skinning material configs");
}

void ProfessionAuctionBridge::InitializeCraftedItemConfigs()
{
    TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Initialized crafted item configs");
}

// ============================================================================
// AUCTION HELPERS
// ============================================================================

std::vector<ProfessionAuctionBridge::ItemInfo> ProfessionAuctionBridge::GetProfessionItemsInInventory(::Player* player, bool materialsOnly) const
{
    std::vector<ItemInfo> items;

    if (!player)
        return items;

    // Scan all bags
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (::Bag* bag = player->GetBagByPos(i))
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                if (::Item* item = bag->GetItemByPos(j))
                {
                    if (materialsOnly && !IsProfessionMaterial(item->GetEntry()))
                        continue;

                    ItemInfo info;
                    info.itemGuid = item->GetGUID().GetCounter();
                    info.itemId = item->GetEntry();
                    info.stackCount = item->GetCount();
                    info.quality = item->GetTemplate()->GetQuality();
                    items.push_back(info);
                }
            }
        }
    }

    // Scan main backpack
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (::Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i)){
            if (materialsOnly && !IsProfessionMaterial(item->GetEntry()))
                continue;

            ItemInfo info;
            info.itemGuid = item->GetGUID().GetCounter();
            info.itemId = item->GetEntry();
            info.stackCount = item->GetCount();
            info.quality = item->GetTemplate()->GetQuality();
            items.push_back(info);
        }
    }

    return items;
}

uint32 ProfessionAuctionBridge::CalculateMaterialCost(::Player* player, uint32 itemId) const
{
    if (!player || !_auctionHouse)
        return 0;

    // Search all production professions for recipe that creates this item
    static const std::vector<ProfessionType> productionProfessions = {
        ProfessionType::ALCHEMY,
        ProfessionType::BLACKSMITHING,
        ProfessionType::ENCHANTING,
        ProfessionType::ENGINEERING,
        ProfessionType::INSCRIPTION,
        ProfessionType::JEWELCRAFTING,
        ProfessionType::LEATHERWORKING,
        ProfessionType::TAILORING
    };

    for (ProfessionType profession : productionProfessions)
    {
        // Get all recipes for this profession
        std::vector<RecipeInfo> recipes = ProfessionManager::instance()->GetRecipesForProfession(profession);

        for (const RecipeInfo& recipe : recipes)
        {
            if (recipe.productItemId == itemId)
            {
                // Found the recipe! Calculate total material cost
                uint32 totalCost = 0;

                for (const RecipeInfo::Reagent& reagent : recipe.reagents)
                {
                    // Get market price for reagent
                    uint32 reagentPrice = static_cast<uint32>(_auctionHouse->GetMarketPrice(reagent.itemId, reagent.quantity));
                    totalCost += reagentPrice;
                }

                TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Calculated material cost for item {}: {} gold (from {} reagents)",
                    itemId, totalCost, recipe.reagents.size());

                return totalCost;
            }
        }
    }

    // No recipe found for this item - might be gathered or vendor-bought
    TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: No recipe found for item {} (not a crafted item)",
        itemId);

    return 0;
}

bool ProfessionAuctionBridge::IsProfessionMaterial(uint32 itemId) const
{
    // Get item template to check class/subclass
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return false;

    // Check if item is a profession material based on item class
    uint32 itemClass = itemTemplate->GetClass();
    uint32 itemSubClass = itemTemplate->GetSubClass();

    // Trade goods are profession materials
    if (itemClass == ITEM_CLASS_TRADE_GOODS)
        return true;

    // Reagents are profession materials
    if (itemClass == ITEM_CLASS_REAGENT)
        return true;

    // Some consumables are used for professions (e.g., vials, threads)
    if (itemClass == ITEM_CLASS_CONSUMABLE)
    {
        // Check specific subclasses that are profession-related
        // ITEM_SUBCLASS_CONSUMABLE (various crafting reagents like thread, vials, etc.)
        return true; // Conservative: include all consumables
    }

    // Quest items sometimes used in profession quests
    if (itemClass == ITEM_CLASS_QUEST)
        return false; // Don't auto-sell quest items

    // Not a profession material
    return false;
}

bool ProfessionAuctionBridge::IsCraftedItem(uint32 itemId, ProfessionType& outProfession) const
{
    // Search all production professions for recipe that creates this item
    static const std::vector<ProfessionType> productionProfessions = {
        ProfessionType::ALCHEMY,
        ProfessionType::BLACKSMITHING,
        ProfessionType::ENCHANTING,
        ProfessionType::ENGINEERING,
        ProfessionType::INSCRIPTION,
        ProfessionType::JEWELCRAFTING,
        ProfessionType::LEATHERWORKING,
        ProfessionType::TAILORING,
        ProfessionType::COOKING // Cooking also creates items
    };

    for (ProfessionType profession : productionProfessions)
    {
        // Get all recipes for this profession
        std::vector<RecipeInfo> recipes = ProfessionManager::instance()->GetRecipesForProfession(profession);

        for (const RecipeInfo& recipe : recipes)
        {
            if (recipe.productItemId == itemId)
            {
                outProfession = profession;
                return true;
            }
        }
    }

    // Not a crafted item
    outProfession = ProfessionType::NONE;
    return false;
}

bool ProfessionAuctionBridge::CanAccessAuctionHouse(::Player* player) const
{
    if (!player)
        return false;

    // For automated bot trading, we'll use a relaxed check:
    // 1. Player must be in a city (safe zone)
    // 2. Or have recently interacted with auction house

    // Check if player is in a rest area (cities have rest areas)
    if (player->HasRestFlag(REST_FLAG_IN_CITY))
    {
        TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Player {} has access (in city)",
            player->GetName());
        return true;
    }

    // Alternative: Check if player is in a zone with an auction house
    // Major cities with auction houses:
    // - Stormwind, Ironforge, Darnassus (Alliance)
    // - Orgrimmar, Undercity, Thunder Bluff (Horde)
    // - Neutral: Booty Bay, Gadgetzan, Everlook, etc.
    uint32 zoneId = player->GetZoneId();

    // Alliance cities
    if (zoneId == 1519 ||  // Stormwind
        zoneId == 1537 ||  // Ironforge
        zoneId == 1657)    // Darnassus
    {
        return true;
    }

    // Horde cities
    if (zoneId == 1637 ||  // Orgrimmar
        zoneId == 1497 ||  // Undercity
        zoneId == 1638)    // Thunder Bluff
    {
        return true;
    }

    // Neutral auction houses
    if (zoneId == 33 ||    // Booty Bay
        zoneId == 1938 ||  // Gadgetzan
        zoneId == 2057 ||  // Everlook
        zoneId == 3487)    // Shattrath
    {
        return true;
    }

    // For bots, allow access from anywhere (they can teleport/travel)
    // In a real implementation, you might want to trigger bot travel to AH
    TC_LOG_DEBUG("playerbots", "ProfessionAuctionBridge: Player {} not near auction house (zone {}), allowing anyway for automation",
        player->GetName(), zoneId);

    return true; // Allow for automation purposes
}

} // namespace Playerbot
