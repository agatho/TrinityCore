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
#include "Creature.h"
#include "DelveMgr.h"
#include "DelvesDefines.h"
#include "DelvesPackets.h"
#include "DelvesRewards.h"
#include "DelvesSeason.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
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

    // 68275 wire: the response carries exactly ONE member per packet
    // (PackedGUID + uint32 + uint32 + bool — no count framing), so we send one
    // packet per party member. Field semantics UNVERIFIED — see DelvesPackets.h.
    auto sendMember = [&](Player const* member)
    {
        uint8 maxTier = computeMaxEligibleTier(member);

        WorldPackets::Delves::PartyEligibilityForDelveTiersResponse response;
        response.PlayerGUID = member->GetGUID();
        response.MaxEligibleTier = maxTier;
        response.ReasonOrFlags = 0;             // UNVERIFIED — needs sniff
        response.IsEligible = maxTier > 0;      // UNVERIFIED — needs sniff
        SendPacket(response.Write());
    };

    // Always emit at least the requesting player so the client populates its own row.
    sendMember(player);

    if (Group const* group = player->GetGroup(); group && !group->isRaidGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player const* member = itr.GetSource();
            if (!member || member == player)
                continue;
            sendMember(member);
        }
    }
}

void WorldSession::HandleSelectDelveEntranceTier(WorldPackets::Delves::SelectDelveEntranceTier& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_SELECT_DELVE_ENTRANCE_TIER received from player {} entrance {} tier {}",
        player->GetName(), packet.EntranceGUID.ToString(), packet.Tier);

    if (packet.Tier == 0 || packet.Tier > Delves::MAX_DELVE_TIER)
        return;

    if (!Delves::DelvesSeason::MeetsMinimumLevelRequirement(player))
        return;

    Delves::DelveProgress progress;
    Delves::DelvesRewards::LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
    if (packet.Tier > progress.HighestTierUnlocked)
        return;

    // The 68275 wire carries the entrance ObjectGuid, not a MapID — re-derive the
    // delve map server-side. Our entrances are gossip NPCs, so resolve the creature
    // and match its gossip menu against the delve templates.
    uint32 mapId = 0;
    if (packet.EntranceGUID.IsCreatureOrVehicle())
        if (Creature const* entrance = ObjectAccessor::GetCreature(*player, packet.EntranceGUID))
            if (Delves::DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplateByGossipMenuId(entrance->GetGossipMenuId()))
                mapId = tmpl->MapId;

    if (!mapId)
        TC_LOG_DEBUG("network", "CMSG_SELECT_DELVE_ENTRANCE_TIER: could not resolve entrance {} to a delve template",
            packet.EntranceGUID.ToString());

    // Selection is consumed by the subsequent entrance-open flow; the client
    // re-sends the tier on entrance. We accept and validate here so eligibility is logged.
    player->m_delveSelectedTier = uint8(packet.Tier);
    player->m_delveSelectedMapId = mapId;

    // Republish progression so the mirror's last-selected delve map stays current.
    Delves::DelvesRewards::PublishProgress(player, progress);
}
