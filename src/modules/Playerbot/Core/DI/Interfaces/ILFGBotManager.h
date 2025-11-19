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

#ifndef _ILFGBOTMANAGER_H
#define _ILFGBOTMANAGER_H

#include "Define.h"
#include "ObjectGuid.h"
#include "LFG.h"

class Player;

namespace lfg
{
    struct LfgProposal;
}

/**
 * @brief Interface for LFG Bot Manager
 *
 * Manages automatic bot recruitment for LFG (Looking For Group) system.
 * Monitors human player queue joins and automatically populates groups
 * with appropriate bots based on role requirements.
 *
 * **Responsibilities:**
 * - Detection of missing roles in queued groups
 * - Selection and queueing of suitable bots
 * - Automatic proposal acceptance for bots
 * - Role check confirmation for bots
 * - Tracking of bot assignments to prevent double-queueing
 */
class TC_GAME_API ILFGBotManager
{
public:
    virtual ~ILFGBotManager() = default;

    /**
     * @brief Initialize the LFG Bot Manager
     * Must be called once during server startup
     */
    virtual void Initialize() = 0;

    /**
     * @brief Shutdown and cleanup the LFG Bot Manager
     * Called during server shutdown
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Update manager state (called from world update loop)
     * @param diff Time elapsed since last update in milliseconds
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Called when a human player joins the LFG queue
     * Triggers bot recruitment if needed
     *
     * @param player The human player joining queue
     * @param playerRole The role(s) the player selected (PLAYER_ROLE_TANK/HEALER/DAMAGE)
     * @param dungeons Set of dungeon IDs the player queued for
     */
    virtual void OnPlayerJoinQueue(uint8 playerRole, lfg::LfgDungeonSet const& dungeons) = 0;

    /**
     * @brief Called when a player (human or bot) leaves the LFG queue
     * Cleanup bot assignments if needed
     *
     * @param playerGuid GUID of the player leaving queue
     */
    virtual void OnPlayerLeaveQueue(ObjectGuid playerGuid) = 0;

    /**
     * @brief Called when an LFG proposal is received
     * Bots automatically accept proposals
     *
     * @param proposalId The proposal ID
     * @param proposal The proposal data structure
     */
    virtual void OnProposalReceived(uint32 proposalId, lfg::LfgProposal const& proposal) = 0;

    /**
     * @brief Called when a role check begins
     * Bots automatically confirm their assigned roles
     *
     * @param groupGuid The group GUID
     * @param botGuid The bot GUID (if specific bot, otherwise empty for all bots)
     */
    virtual void OnRoleCheckReceived(ObjectGuid groupGuid, ObjectGuid botGuid = ObjectGuid::Empty) = 0;

    /**
     * @brief Called when a group is formed successfully
     * Cleanup tracking data for the group
     *
     * @param groupGuid The group GUID
     */
    virtual void OnGroupFormed(ObjectGuid groupGuid) = 0;

    /**
     * @brief Called when a proposal fails or is declined
     * Remove bots from queue and allow them to be selected again
     *
     * @param proposalId The failed proposal ID
     */
    virtual void OnProposalFailed(uint32 proposalId) = 0;

    /**
     * @brief Manually populate queue with bots for a specific player
     * Used for testing or manual control
     *
     * @param playerGuid The player to populate bots for
     * @param neededRoles Bitmask of needed roles (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE)
     * @param dungeons Set of dungeons to queue for
     * @return Number of bots successfully queued
     */
    virtual uint32 PopulateQueue(ObjectGuid playerGuid, uint8 neededRoles, lfg::LfgDungeonSet const& dungeons) = 0;

    /**
     * @brief Check if a bot is currently assigned to an LFG queue
     *
     * @param botGuid The bot's GUID
     * @return true if bot is queued, false otherwise
     */
    virtual bool IsBotQueued(ObjectGuid botGuid) const = 0;

    /**
     * @brief Get statistics about current bot assignments
     *
     * @param totalQueued Output: Total number of bots currently queued
     * @param totalAssignments Output: Total number of active human->bot assignments
     */
    virtual void GetStatistics(uint32& totalQueued, uint32& totalAssignments) const = 0;

    /**
     * @brief Enable or disable the LFG bot system
     *
     * @param enable true to enable, false to disable
     */
    virtual void SetEnabled(bool enable) = 0;

    /**
     * @brief Check if the LFG bot system is enabled
     *
     * @return true if enabled, false otherwise
     */
    virtual bool IsEnabled() const = 0;

    /**
     * @brief Clean up stale queue assignments
     * Removes bots that have been queued for too long without forming a group
     */
    virtual void CleanupStaleAssignments() = 0;
};

#endif // _ILFGBOTMANAGER_H
