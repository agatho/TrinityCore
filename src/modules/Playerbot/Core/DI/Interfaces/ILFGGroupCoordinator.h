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
#include "ObjectGuid.h"

class Player;
class Group;

namespace Playerbot
{

/**
 * @brief Interface for LFG Group Coordinator
 *
 * Handles group formation and dungeon teleportation for bots
 * in Looking For Group (LFG) system.
 *
 * Features:
 * - Group formation coordination after LFG proposal acceptance
 * - Dungeon teleportation management
 * - Teleport state tracking
 * - Thread-safe operations
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API ILFGGroupCoordinator
{
public:
    virtual ~ILFGGroupCoordinator() = default;

    // ====================================================================
    // INITIALIZATION & LIFECYCLE
    // ====================================================================

    /**
     * @brief Initialize the coordinator
     * @note Called once during server startup
     */
    virtual void Initialize() = 0;

    /**
     * @brief Update coordinator state
     * @param diff Time since last update in milliseconds
     * @note Called every world update tick
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Shutdown the coordinator
     * @note Called during server shutdown
     */
    virtual void Shutdown() = 0;

    // ====================================================================
    // GROUP FORMATION
    // ====================================================================

    /**
     * @brief Handle group formation after LFG proposal is accepted
     * @param groupGuid GUID of the formed group
     * @param dungeonId LFG dungeon ID
     * @return true if group was successfully formed
     */
    virtual bool OnGroupFormed(ObjectGuid groupGuid, uint32 dungeonId) = 0;

    /**
     * @brief Handle group ready check completion
     * @param groupGuid GUID of the group
     * @return true if teleportation was initiated
     */
    virtual bool OnGroupReady(ObjectGuid groupGuid) = 0;

    // ====================================================================
    // DUNGEON TELEPORTATION
    // ====================================================================

    /**
     * @brief Teleport player to dungeon entrance
     * @param player Player to teleport
     * @param dungeonId LFG dungeon ID
     * @return true if teleportation was successful
     */
    virtual bool TeleportPlayerToDungeon(Player* player, uint32 dungeonId) = 0;

    /**
     * @brief Teleport entire group to dungeon
     * @param group Group to teleport
     * @param dungeonId LFG dungeon ID
     * @return true if teleportation was initiated for all members
     */
    virtual bool TeleportGroupToDungeon(Group* group, uint32 dungeonId) = 0;

    /**
     * @brief Check if player can be teleported to dungeon
     * @param player Player to check
     * @param dungeonId LFG dungeon ID
     * @return true if player can be teleported
     */
    virtual bool CanTeleportToDungeon(Player const* player, uint32 dungeonId) const = 0;

    /**
     * @brief Get dungeon entrance location
     * @param dungeonId LFG dungeon ID
     * @param mapId Output: Map ID of the dungeon
     * @param x Output: X coordinate
     * @param y Output: Y coordinate
     * @param z Output: Z coordinate
     * @param orientation Output: Orientation
     * @return true if dungeon entrance was found
     */
    virtual bool GetDungeonEntrance(uint32 dungeonId, uint32& mapId, float& x, float& y, float& z, float& orientation) const = 0;

    // ====================================================================
    // TELEPORT STATE MANAGEMENT
    // ====================================================================

    /**
     * @brief Track player teleport request
     * @param playerGuid Player GUID
     * @param dungeonId LFG dungeon ID
     * @param timestamp Time of teleport request
     */
    virtual void TrackTeleport(ObjectGuid playerGuid, uint32 dungeonId, uint32 timestamp) = 0;

    /**
     * @brief Clear player teleport tracking
     * @param playerGuid Player GUID
     */
    virtual void ClearTeleport(ObjectGuid playerGuid) = 0;

    /**
     * @brief Check if player has pending teleport
     * @param playerGuid Player GUID
     * @return true if player has a pending teleport
     */
    virtual bool HasPendingTeleport(ObjectGuid playerGuid) const = 0;

    /**
     * @brief Get pending teleport dungeon ID
     * @param playerGuid Player GUID
     * @return Dungeon ID, or 0 if no pending teleport
     */
    virtual uint32 GetPendingTeleportDungeon(ObjectGuid playerGuid) const = 0;

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    /**
     * @brief Enable/disable coordinator
     * @param enabled Enable state
     */
    virtual void SetEnabled(bool enabled) = 0;

    /**
     * @brief Check if coordinator is enabled
     * @return true if enabled
     */
    virtual bool IsEnabled() const = 0;

    /**
     * @brief Set teleport timeout (milliseconds)
     * @param timeout Timeout in milliseconds
     */
    virtual void SetTeleportTimeout(uint32 timeout) = 0;

    /**
     * @brief Get teleport timeout
     * @return Timeout in milliseconds
     */
    virtual uint32 GetTeleportTimeout() const = 0;
};

} // namespace Playerbot
