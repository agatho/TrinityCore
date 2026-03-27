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

#include "WorldSession.h"
#include "DelveMgr.h"
#include "DelvesPackets.h"
#include "DelvesRewards.h"
#include "DelvesSeason.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"

void WorldSession::HandleDelveTeleportOut(WorldPackets::Delves::DelveTeleportOut& /*delveTeleportOut*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_DELVE_TELEPORT_OUT received from player {}", player->GetName());

    // Teleport player out of the delve instance to their bind point
    if (player->GetMap()->Instanceable())
        player->TeleportTo(player->GetHomebind());
}

void WorldSession::HandleRequestPartyEligibilityForDelveTiers(WorldPackets::Delves::RequestPartyEligibilityForDelveTiers& requestPartyEligibilityForDelveTiers)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS received from player {} with id {}",
        player->GetName(), requestPartyEligibilityForDelveTiers.GossipOptionOrMapChallengeID);

    // Validate: cannot enter delves in a raid group
    Group* group = player->GetGroup();
    if (group && group->isRaidGroup())
    {
        // SPELL_CUSTOM_ERROR_YOU_CANNOT_ENTER_A_DELVE_WHILE_IN_A_RAID_GROUP = 871
        TC_LOG_DEBUG("network", "Player {} denied delve entry: in raid group", player->GetName());
    }

    // Check minimum level requirement
    if (!Delves::DelvesSeason::MeetsMinimumLevelRequirement(player))
    {
        // SPELL_CUSTOM_ERROR_NOT_HIGH_ENOUGH_LEVEL_TO_ENTER_A_DELVE = 1040
        TC_LOG_DEBUG("network", "Player {} denied delve entry: below minimum level", player->GetName());
    }

    // Load account progress to check tier unlock
    Delves::DelveProgress progress;
    Delves::DelvesRewards::LoadProgress(player->GetBattlenetAccountId(), progress);

    TC_LOG_DEBUG("network", "Player {} delve eligibility: highest tier unlocked = {}, group size = {}",
        player->GetName(), progress.HighestTierUnlocked,
        group ? group->GetMembersCount() : 1);

    // Send response
    WorldPackets::Delves::PartyEligibilityForDelveTiersResponse response;
    SendPacket(response.Write());
}
