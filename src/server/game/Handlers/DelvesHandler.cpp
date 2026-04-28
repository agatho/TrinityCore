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
#include "DelvesDefines.h"
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
        player->TeleportTo(player->m_homebind);
}

void WorldSession::HandleRequestPartyEligibilityForDelveTiers(WorldPackets::Delves::RequestPartyEligibilityForDelveTiers& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS received from player {} for mapId {}",
        player->GetName(), packet.MapID);

    auto computeMaxEligibleTier = [&](Player const* member) -> uint8
    {
        if (!Delves::DelvesSeason::MeetsMinimumLevelRequirement(member))
            return 0;
        Delves::DelveProgress progress;
        Delves::DelvesRewards::LoadProgress(member->GetSession()->GetBattlenetAccountId(), progress);
        return std::min<uint8>(progress.HighestTierUnlocked, Delves::MAX_DELVE_TIER);
    };

    auto sendForMember = [&](Player const* member)
    {
        WorldPackets::Delves::PartyEligibilityForDelveTiersResponse response;
        response.PlayerName = member->GetName();
        response.MaxEligibleTier = computeMaxEligibleTier(member);
        SendPacket(response.Write());
    };

    // Always emit at least the requesting player so the client populates its own row.
    sendForMember(player);

    if (Group const* group = player->GetGroup())
    {
        if (group->isRaidGroup())
            return;

        for (GroupReference const& itr : group->GetMembers())
        {
            Player const* member = itr.GetSource();
            if (!member || member == player)
                continue;
            sendForMember(member);
        }
    }
}

void WorldSession::HandleSelectDelveEntranceTier(WorldPackets::Delves::SelectDelveEntranceTier& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_SELECT_DELVE_ENTRANCE_TIER received from player {} mapId {} tier {}",
        player->GetName(), packet.MapID, packet.Tier);

    if (packet.Tier == 0 || packet.Tier > Delves::MAX_DELVE_TIER)
        return;

    if (!Delves::DelvesSeason::MeetsMinimumLevelRequirement(player))
        return;

    Delves::DelveProgress progress;
    Delves::DelvesRewards::LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
    if (packet.Tier > progress.HighestTierUnlocked)
        return;

    // Selection is consumed by the subsequent CMSG_TIERED_ENTRANCE_OPEN flow; the client
    // re-sends the tier on entrance. We accept and validate here so eligibility is logged.
    player->m_delveSelectedTier = packet.Tier;
    player->m_delveSelectedMapId = packet.MapID;
}
