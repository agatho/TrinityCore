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

// Forward declarations
class Player;

namespace Playerbot
{

/**
 * @brief Interface for Guild Bank Management System
 *
 * Automated guild bank interactions and intelligent organization.
 *
 * Features:
 * - Deposit and withdrawal operations
 * - Automated organization
 * - Item categorization
 * - Access control
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API IGuildBankManager
{
public:
    virtual ~IGuildBankManager() = default;

    /**
     * @brief Deposit item to guild bank
     * @param player Player performing deposit
     * @param itemGuid Item GUID
     * @param tabId Tab index
     * @param stackCount Number to deposit
     * @return true if successful
     */
    virtual bool DepositItem(Player* player, uint32 itemGuid, uint32 tabId, uint32 stackCount) = 0;

    /**
     * @brief Withdraw item from guild bank
     * @param player Player performing withdrawal
     * @param tabId Tab index
     * @param slotId Slot index
     * @param stackCount Number to withdraw
     * @return true if successful
     */
    virtual bool WithdrawItem(Player* player, uint32 tabId, uint32 slotId, uint32 stackCount) = 0;

    /**
     * @brief Move item between bank slots
     * @param player Player performing move
     * @param fromTab Source tab
     * @param fromSlot Source slot
     * @param toTab Destination tab
     * @param toSlot Destination slot
     * @return true if successful
     */
    virtual bool MoveItem(Player* player, uint32 fromTab, uint32 fromSlot, uint32 toTab, uint32 toSlot) = 0;

    /**
     * @brief Check if player can access tab
     * @param player Player to check
     * @param tabId Tab index
     * @return true if player has access
     */
    virtual bool CanAccessGuildBank(Player* player, uint32 tabId) = 0;

    /**
     * @brief Auto-organize guild bank
     * @param player Player context
     */
    virtual void AutoOrganizeGuildBank(Player* player) = 0;

    /**
     * @brief Optimize item placement
     * @param player Player context
     */
    virtual void OptimizeItemPlacement(Player* player) = 0;

    /**
     * @brief Analyze guild bank contents
     * @param player Player context
     */
    virtual void AnalyzeGuildBankContents(Player* player) = 0;

    /**
     * @brief Auto-deposit items from inventory
     * @param player Player to deposit from
     */
    virtual void AutoDepositItems(Player* player) = 0;

    /**
     * @brief Deposit excess consumables
     * @param player Player to deposit from
     */
    virtual void DepositExcessConsumables(Player* player) = 0;

    /**
     * @brief Deposit crafting materials
     * @param player Player to deposit from
     */
    virtual void DepositCraftingMaterials(Player* player) = 0;

    /**
     * @brief Auto-withdraw needed items
     * @param player Player to withdraw for
     */
    virtual void AutoWithdrawNeededItems(Player* player) = 0;

    /**
     * @brief Withdraw consumables
     * @param player Player to withdraw for
     */
    virtual void WithdrawConsumables(Player* player) = 0;

    /**
     * @brief Withdraw crafting materials
     * @param player Player to withdraw for
     */
    virtual void WithdrawCraftingMaterials(Player* player) = 0;
};

} // namespace Playerbot
