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
 * @brief Interface for Equipment Management
 *
 * Abstracts equipment evaluation, comparison, and auto-equip functionality
 * to enable dependency injection and testing.
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
 * auto equipMgr = Services::Container().Resolve<IEquipmentManager>();
 * equipMgr->AutoEquipBestGear(player);
 * auto junkItems = equipMgr->IdentifyJunkItems(player);
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
     * @brief Auto-equip best gear from inventory
     * @param player Player to equip
     */
    virtual void AutoEquipBestGear(::Player* player) = 0;

    /**
     * @brief Compare two items for upgrade
     * @param player Player context
     * @param currentItem Currently equipped item
     * @param newItem New item to compare
     * @return Comparison result
     */
    virtual ItemComparisonResult CompareItems(::Player* player, ::Item* currentItem, ::Item* newItem) = 0;

    /**
     * @brief Calculate item score based on stat priorities
     * @param player Player context
     * @param item Item to score
     * @return Item score
     */
    virtual float CalculateItemScore(::Player* player, ::Item* item) = 0;

    /**
     * @brief Check if item is an upgrade
     * @param player Player context
     * @param item Item to check
     * @return true if upgrade, false otherwise
     */
    virtual bool IsItemUpgrade(::Player* player, ::Item* item) = 0;

    /**
     * @brief Calculate score for item template (quest rewards, vendors)
     * @param player Player context
     * @param itemTemplate Item template to score
     * @return Item score
     */
    virtual float CalculateItemTemplateScore(::Player* player, ItemTemplate const* itemTemplate) = 0;

    /**
     * @brief Identify junk items in inventory
     * @param player Player to scan
     * @return List of junk item GUIDs
     */
    virtual std::vector<ObjectGuid> IdentifyJunkItems(::Player* player) = 0;

    /**
     * @brief Check if item is junk
     * @param player Player context
     * @param item Item to check
     * @return true if junk, false otherwise
     */
    virtual bool IsJunkItem(::Player* player, ::Item* item) = 0;

    /**
     * @brief Check if item is protected from selling
     * @param player Player context
     * @param item Item to check
     * @return true if protected, false otherwise
     */
    virtual bool IsProtectedItem(::Player* player, ::Item* item) = 0;

    /**
     * @brief Check if BoE item is valuable for AH
     * @param item Item to check
     * @return true if valuable, false otherwise
     */
    virtual bool IsValuableBoE(::Item* item) = 0;

    /**
     * @brief Get consumable needs for player
     * @param player Player to check
     * @return Map of itemId -> quantity needed
     */
    virtual std::unordered_map<uint32, uint32> GetConsumableNeeds(::Player* player) = 0;

    /**
     * @brief Check if player needs consumable restocking
     * @param player Player to check
     * @return true if needs restocking, false otherwise
     */
    virtual bool NeedsConsumableRestocking(::Player* player) = 0;

    /**
     * @brief Get player-specific metrics
     * @param playerGuid Player GUID
     * @return Equipment metrics
     */
    virtual EquipmentMetrics const& GetPlayerMetrics(uint32 playerGuid) = 0;

    /**
     * @brief Get global metrics
     * @return Global equipment metrics
     */
    virtual EquipmentMetrics const& GetGlobalMetrics() = 0;
};

} // namespace Playerbot
