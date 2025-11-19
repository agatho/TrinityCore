/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_BOT_VENDOR_INTERACTION_H
#define TRINITYCORE_BOT_VENDOR_INTERACTION_H

#include "../Core/InteractionTypes.h"
#include "Threading/LockHierarchy.h"
#include "Define.h"
#include <unordered_map>
#include <vector>
#include <shared_mutex>

class Player;
class Creature;
class Item;
class WorldPacket;

namespace Playerbot
{
    class RepairManager;
    class VendorDatabase;

    /**
     * @class VendorInteraction
     * @brief Handles all vendor-related interactions for bots
     *
     * Features:
     * - Intelligent item purchasing decisions
     * - Automatic junk selling
     * - Equipment repair management
     * - Reagent restocking
     * - Item upgrade evaluation
     * - Vendor list parsing and analysis
     */
    class VendorInteraction
    {
    public:
        VendorInteraction();
        ~VendorInteraction();

        /**
         * @brief Process vendor interaction
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @return Result of the interaction
         */
        InteractionResult ProcessInteraction(Player* bot, Creature* vendor);

        /**
         * @brief Buy specific item from vendor
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @param itemId Item to purchase
         * @param count Number to buy
         * @return Result of purchase
         */
        InteractionResult BuyItem(Player* bot, Creature* vendor, uint32 itemId, uint32 count);

        /**
         * @brief Buy all needed reagents for bot's class
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @return Result of reagent purchases
         */
        InteractionResult BuyReagents(Player* bot, Creature* vendor);

        /**
         * @brief Sell all junk items to vendor
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @return Result of selling
         */
        InteractionResult SellJunkItems(Player* bot, Creature* vendor);

        /**
         * @brief Repair all damaged equipment
         * @param bot The bot player
         * @param vendor The vendor NPC (must have repair flag)
         * @return Result of repair
         */
        InteractionResult RepairAllItems(Player* bot, Creature* vendor);

        /**
         * @brief Handle vendor list packet from server
         * @param bot The bot player
         * @param packet The vendor list packet
         */
        void HandleVendorList(Player* bot, WorldPacket const& packet);

        /**
         * @brief Analyze vendor inventory for useful items
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @return List of items worth buying
         */
        std::vector<VendorInteractionData::ItemToBuy> AnalyzeVendorInventory(Player* bot, Creature* vendor);

        /**
         * @brief Check if item is an upgrade for bot
         * @param bot The bot player
         * @param itemId The item to evaluate
         * @return True if item is an upgrade
         */
        bool IsItemUpgrade(Player* bot, uint32 itemId) const;

        /**
         * @brief Calculate total cost for planned purchases
         * @param bot The bot player
         * @param items Items to buy
         * @return Total cost in copper
         */
        uint32 CalculateTotalCost(Player* bot, const std::vector<VendorInteractionData::ItemToBuy>& items) const;

        /**
         * @brief Get items bot should sell
         * @param bot The bot player
         * @return List of items to sell
         */
        std::vector<VendorInteractionData::ItemToSell> GetItemsToSell(Player* bot) const;

        /**
         * @brief Check if bot needs specific item
         * @param bot The bot player
         * @param itemId The item to check
         * @return True if bot needs the item
         */
        bool NeedsItem(Player* bot, uint32 itemId) const;

        /**
         * @brief Get optimal purchase quantity for item
         * @param bot The bot player
         * @param itemId The item to buy
         * @return Optimal quantity to purchase
         */
        uint32 GetOptimalPurchaseQuantity(Player* bot, uint32 itemId) const;

        /**
         * @brief Sell specific item to vendor
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @param itemGuid Item to sell
         * @param count Number to sell
         * @return Result of sale
         */
        InteractionResult SellItem(Player* bot, Creature* vendor, ObjectGuid itemGuid, uint32 count);

        /**
         * @brief Buy back item from vendor
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @param slot Buyback slot
         * @return Result of buyback
         */
        InteractionResult BuyBackItem(Player* bot, Creature* vendor, uint32 slot);

        /**
         * @brief Check if vendor sells item
         * @param vendor The vendor NPC
         * @param itemId Item to check
         * @return Vendor slot if found, -1 otherwise
         */
        int32 GetVendorItemSlot(Creature* vendor, uint32 itemId) const;

        /**
         * @brief Set automatic behavior configuration
         * @param autoSell Enable auto-sell junk
         * @param autoRepair Enable auto-repair
         * @param autoBuyReagents Enable auto-buy reagents
         */
        void SetAutoBehavior(bool autoSell, bool autoRepair, bool autoBuyReagents);

        /**
         * @brief Get vendor interaction metrics
         * @return Current metrics
         */
        struct VendorMetrics
        {
            uint32 totalTransactions = 0;
            uint32 itemsBought = 0;
            uint32 itemsSold = 0;
            uint32 repairsDone = 0;
            uint64 goldSpent = 0;
            uint64 goldEarned = 0;
            float avgTransactionTime = 0.0f;
        };
        VendorMetrics GetMetrics() const { return m_metrics; }

    private:
        /**
         * @brief Initialize class-specific reagent lists
         */
        void InitializeReagentLists();

        /**
         * @brief Process automatic vendor behaviors
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @return Combined result of auto actions
         */
        InteractionResult ProcessAutoBehaviors(Player* bot, Creature* vendor);

        /**
         * @brief Check if item is junk
         * @param item The item to check
         * @return True if item is junk
         */
        bool IsJunkItem(Item* item) const;

        /**
         * @brief Check if item is a reagent for bot's class
         * @param bot The bot player
         * @param itemId The item to check
         * @return True if item is a class reagent
         */
        bool IsClassReagent(Player* bot, uint32 itemId) const;

        /**
         * @brief Get reagent stock target for item
         * @param bot The bot player
         * @param itemId The reagent item
         * @return Target stock quantity
         */
        uint32 GetReagentStockTarget(Player* bot, uint32 itemId) const;

        /**
         * @brief Execute vendor transaction
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @param data Transaction data
         * @return Result of transaction
         */
        InteractionResult ExecuteTransaction(Player* bot, Creature* vendor, const VendorInteractionData& data);

        /**
         * @brief Validate vendor transaction
         * @param bot The bot player
         * @param vendor The vendor NPC
         * @param data Transaction data
         * @return True if transaction is valid
         */
        bool ValidateTransaction(Player* bot, Creature* vendor, const VendorInteractionData& data) const;

        /**
         * @brief Record transaction metrics
         * @param type Type of transaction
         * @param value Value in copper
         */
        void RecordTransaction(VendorAction type, uint64 value);

        /**
         * @brief Sort items by sell priority
         * @param items Items to sort
         */
        void SortBySellPriority(std::vector<VendorInteractionData::ItemToSell>& items) const;

        /**
         * @brief Sort items by buy priority
         * @param items Items to sort
         */
        void SortByBuyPriority(std::vector<VendorInteractionData::ItemToBuy>& items) const;

    private:
        // Thread safety
        mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::ACTION_PRIORITY> m_mutex;

        // Sub-managers
        std::unique_ptr<RepairManager> m_repairManager;
        std::unique_ptr<VendorDatabase> m_vendorDatabase;

        // Configuration
        bool m_autoSellJunk = true;
        bool m_autoRepair = true;
        bool m_autoBuyReagents = true;

        // Class reagent lists
        std::unordered_map<uint8, std::vector<uint32>> m_classReagents = {
            {CLASS_WARRIOR, {}},
            {CLASS_PALADIN, {17033}},  // Symbol of Kings
            {CLASS_HUNTER, {2512, 2515, 3030, 3033}},  // Arrows and bullets
            {CLASS_ROGUE, {5140, 5530, 8923, 8924}},  // Poisons and flash powder
            {CLASS_PRIEST, {17056}},  // Light Feather
            {CLASS_DEATH_KNIGHT, {37201}},  // Corpse Dust
            {CLASS_SHAMAN, {17057, 17058}},  // Fish Oil, Shiny Fish Scales
            {CLASS_MAGE, {17031, 17032}},  // Rune of Teleportation/Portals
            {CLASS_WARLOCK, {6265, 5565, 16583}},  // Soul Shards, Infernal Stones
            {CLASS_DRUID, {17034, 17035, 17036}},  // Seeds
        };

        // Item evaluation cache
        mutable std::unordered_map<uint32, bool> m_upgradeCache;
        mutable std::unordered_map<uint32, bool> m_junkCache;

        // Active vendor sessions
        struct VendorSession
        {
            ObjectGuid vendorGuid;
            std::vector<VendorInteractionData::ItemToBuy> plannedPurchases;
            std::vector<VendorInteractionData::ItemToSell> plannedSales;
            bool needsRepair = false;
            uint32 repairCost = 0;
            uint32 totalCost = 0;
            std::chrono::steady_clock::time_point startTime;
        };
        std::unordered_map<ObjectGuid, VendorSession> m_activeSessions;

        // Metrics
        VendorMetrics m_metrics;

        // Price limits
        const uint32 MAX_ITEM_PRICE = 10000000;  // 1000 gold
        const uint32 MAX_REPAIR_COST = 5000000;  // 500 gold

        // Stock levels
        const uint32 MIN_REAGENT_STOCK = 20;
        const uint32 MAX_REAGENT_STOCK = 100;
        const uint32 MIN_CONSUMABLE_STOCK = 10;
        const uint32 MAX_CONSUMABLE_STOCK = 40;

        bool m_initialized = false;
    };
}

#endif // TRINITYCORE_BOT_VENDOR_INTERACTION_H