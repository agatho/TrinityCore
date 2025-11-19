/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include <vector>
#include <unordered_map>
#include <string>

// Forward declarations
class Player;
class Item;
class ItemTemplate;
struct ObjectGuid;

namespace Playerbot
{
    enum class StatType : uint8;
    enum class ItemCategory : uint8;

/**
 * @brief Interface for Equipment Management (Phase 6.1 - Per-Bot Pattern)
 *
 * Abstracts equipment evaluation, comparison, and auto-equip functionality
 * to enable dependency injection and testing.
 *
 * **Phase 6.1 Conversion**: Singleton → Per-Bot Instance
 * - Each bot has its own EquipmentManager instance
 * - No Player* parameters needed (manager owns bot reference)
 * - Zero mutex locking (per-bot isolation)
 * - Owned by GameSystemsManager (24th manager)
 *
 * **Responsibilities:**
 * - Evaluate and compare items for upgrades
 * - Auto-equip better gear
 * - Identify junk items for selling
 * - Manage consumable needs
 * - Provide stat priorities for class/spec
 *
 * **Testability:**
 * - Can be mocked for testing without real items/database
 * - Enables testing gear optimization logic in isolation
 *
 * Example:
 * @code
 * auto equipMgr = botAI->GetGameSystems()->GetEquipmentManager();
 * equipMgr->AutoEquipBestGear();
 * auto junkItems = equipMgr->IdentifyJunkItems();
 * @endcode
 */
class TC_GAME_API IEquipmentManager
{
public:
    /**
     * @brief Item comparison result
     */
    struct ItemComparisonResult
    {
        bool isUpgrade = false;
        float scoreDifference = 0.0f;
        float currentItemScore = 0.0f;
        float newItemScore = 0.0f;
        uint32 currentItemLevel = 0;
        uint32 newItemLevel = 0;
        std::string upgradeReason;
    };

    /**
     * @brief Equipment metrics
     */
    struct EquipmentMetrics
    {
        uint32 itemsEquipped = 0;
        uint32 upgradesFound = 0;
        uint32 junkItemsSold = 0;
        uint32 totalGoldFromJunk = 0;
        float averageItemScore = 0.0f;
    };

    virtual ~IEquipmentManager() = default;

    /**
     * @brief Auto-equip best gear from inventory (operates on bot instance)
     */
    virtual void AutoEquipBestGear() = 0;

    /**
     * @brief Compare two items for upgrade
     * @param currentItem Currently equipped item
     * @param newItem New item to compare
     * @return Comparison result
     */
    virtual ItemComparisonResult CompareItems(::Item* currentItem, ::Item* newItem) = 0;

    /**
     * @brief Calculate item score based on stat priorities
     * @param item Item to score
     * @return Item score
     */
    virtual float CalculateItemScore(::Item* item) = 0;

    /**
     * @brief Check if item is an upgrade
     * @param item Item to check
     * @return true if upgrade, false otherwise
     */
    virtual bool IsItemUpgrade(::Item* item) = 0;

    /**
     * @brief Calculate score for item template (quest rewards, vendors)
     * @param itemTemplate Item template to score
     * @return Item score
     */
    virtual float CalculateItemTemplateScore(ItemTemplate const* itemTemplate) = 0;

    /**
     * @brief Identify junk items in inventory
     * @return List of junk item GUIDs
     */
    virtual std::vector<ObjectGuid> IdentifyJunkItems() = 0;

    /**
     * @brief Check if item is junk
     * @param item Item to check
     * @return true if junk, false otherwise
     */
    virtual bool IsJunkItem(::Item* item) = 0;

    /**
     * @brief Check if item is protected from selling
     * @param item Item to check
     * @return true if protected, false otherwise
     */
    virtual bool IsProtectedItem(::Item* item) = 0;

    /**
     * @brief Check if BoE item is valuable for AH
     * @param item Item to check
     * @return true if valuable, false otherwise
     */
    virtual bool IsValuableBoE(::Item* item) = 0;

    /**
     * @brief Get consumable needs for this bot
     * @return Map of itemId -> quantity needed
     */
    virtual std::unordered_map<uint32, uint32> GetConsumableNeeds() = 0;

    /**
     * @brief Check if this bot needs consumable restocking
     * @return true if needs restocking, false otherwise
     */
    virtual bool NeedsConsumableRestocking() = 0;

    /**
     * @brief Get metrics for this bot
     * @return Equipment metrics
     */
    virtual EquipmentMetrics const& GetMetrics() = 0;

    /**
     * @brief Get global metrics across all bots
     * @return Global equipment metrics
     */
    virtual EquipmentMetrics const& GetGlobalMetrics() = 0;
};

} // namespace Playerbot
