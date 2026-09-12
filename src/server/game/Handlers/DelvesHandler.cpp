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
#include "DatabaseEnv.h"
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
#include "QueryCallback.h"
#include "StringFormat.h"

void WorldSession::HandleDelveTeleportOut(WorldPackets::Delves::DelveTeleportOut& /*delveTeleportOut*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_DELVE_TELEPORT_OUT received from player {}", player->GetName());

    // Never sent by the 12.x client in any of the five captured runs (REPORT.md 1.1 / 5); retail leaves through the
    // Leave-O-Bot spell-click or the "Leave Delve" object, both of which call DelveMgr::LeaveDelve from scripts.
    // Kept as a safety net with the same exit semantics (originating map, template exit, homebind).
    if (sDelveMgr->GetDelveTemplate(player->GetMapId()))
        sDelveMgr->LeaveDelve(player);
}

void WorldSession::HandleRequestPartyEligibilityForDelveTiers(WorldPackets::Delves::RequestPartyEligibilityForDelveTiers& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS received from player {} for mapId {}",
        player->GetName(), packet.MapID);

    // REPORT.md 1.1 / 6.1: SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE was never sent in any of the four solo
    // runs (0 frames in five captures) - the client sends the request right after the picker opens (gulf 2964,
    // eversong 2952) and gets nothing back. Only a party has rows to fill in.
    Group const* group = player->GetGroup();
    if (!group || group->isRaidGroup())
        return;

    // One packet per member (68275: PackedGUID + uint32 + uint32 + bool, no count framing; semantics UNVERIFIED, see
    // DelvesPackets.h). Tier progress lives in delve_progress per battlenet account - read it asynchronously, one
    // query per member, and answer from the callback; no synchronous DB round trip on the world thread.
    for (GroupReference const& itr : group->GetMembers())
    {
        Player const* member = itr.GetSource();
        if (!member || !member->GetSession())
            continue;

        ObjectGuid const memberGuid = member->GetGUID();
        uint8 const levelEligible = Delves::DelvesSeason::MeetsMinimumLevelRequirement(member) ? 1 : 0;

        // same SELECT as CHAR_SEL_DELVE_PROGRESS, which is prepared on the synchronous connection only
        std::string const query = Trinity::StringFormat("SELECT highestTierUnlocked FROM delve_progress WHERE battlenetAccountId = {}",
            member->GetSession()->GetBattlenetAccountId());

        GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(query.c_str()).WithCallback([this, memberGuid, levelEligible](QueryResult result)
        {
            if (!GetPlayer() || !GetPlayer()->IsInWorld())
                return;

            // default progress: tiers 1-3 are open for every fresh character (REPORT.md 1.3 unlock state)
            uint8 highestTierUnlocked = Delves::DelveProgress().HighestTierUnlocked;
            if (result)
                highestTierUnlocked = (*result)[0].GetUInt8();

            uint8 const maxTier = levelEligible ? std::min<uint8>(highestTierUnlocked, Delves::MAX_DELVE_TIER) : 0;

            WorldPackets::Delves::PartyEligibilityForDelveTiersResponse response;
            response.PlayerGUID = memberGuid;
            response.MaxEligibleTier = maxTier;
            response.ReasonOrFlags = 0;             // UNVERIFIED - needs a group capture
            response.IsEligible = maxTier > 0;      // UNVERIFIED - needs a group capture
            SendPacket(response.Write());
        }));
    }
}

void WorldSession::HandleSelectDelveEntranceTier(WorldPackets::Delves::SelectDelveEntranceTier& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_SELECT_DELVE_ENTRANCE_TIER received from player {} entrance {} tier {}",
        player->GetName(), packet.EntranceGUID.ToString(), packet.Tier);

    // 12.1 wire: the client echoes the TieredEntranceTierID the picker advertised (75..85 on the
    // 12.1 delve season), not the 1-based tier. Accept a bare tier number too (legacy pickers).
    uint8 tier = 0;
    if (Delves::TieredEntranceTierData const* tierRow = sDelveMgr->GetTieredEntranceTier(packet.Tier))
        tier = tierRow->Tier;
    else if (packet.Tier >= 1 && packet.Tier <= Delves::MAX_DELVE_TIER)
        tier = uint8(packet.Tier);

    if (!tier)
        return;

    if (!Delves::DelvesSeason::MeetsMinimumLevelRequirement(player))
        return;

    Delves::DelveProgress progress;
    Delves::DelvesRewards::LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
    if (tier > progress.HighestTierUnlocked)
        return;

    // The wire carries the entrance ObjectGuid, not a MapID - re-derive the delve from the entrance spawn
    // (DelveMgr::GetDelveTemplateForEntrance; Creature::GetGossipMenuId() is always 0, see DelveMgr.cpp).
    Delves::DelveTemplate const* tmpl = nullptr;
    if (packet.EntranceGUID.IsCreatureOrVehicle())
        if (Creature const* entrance = ObjectAccessor::GetCreature(*player, packet.EntranceGUID))
            tmpl = sDelveMgr->GetDelveTemplateForEntrance(entrance);

    if (!tmpl)
    {
        TC_LOG_DEBUG("network", "CMSG_SELECT_DELVE_ENTRANCE_TIER: could not resolve entrance {} to a delve template",
            packet.EntranceGUID.ToString());
        return;
    }

    // REPORT.md 1.4 - 1.6 (work item 5): the select IS the entry. Retail answers it with the phase shift,
    // SMSG_SUSPEND_TOKEN + SMSG_PLAYER_CHOICE_CLEAR and, ~1.9 s later, a seamless SMSG_NEW_WORLD (reason 21, no
    // SMSG_TRANSFER_PENDING) to the delve's entry position (gulf 100129 -> 101829). Nothing else is requested from
    // the client in between. Shared with the entrance gossip script.
    sDelveMgr->EnterDelve(player, *tmpl, tier);
}

void WorldSession::HandleTieredEntranceOpen(WorldPackets::Delves::TieredEntranceOpen& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_TIERED_ENTRANCE_OPEN received from player {} entrance {}",
        player->GetName(), packet.EntranceGUID.ToString());

    // REPORT.md 1.1: in 12.1 delve entrances (EntranceType 1) open by proximity - the server pushes the picker
    // (DelveMgr::OpenEntranceByProximity from the entrance AI). This CMSG is still sent by click-to-open entrances
    // (69273 Sites witness: creature 264322 -> map 3075 "Naigtal") and is kept for them; the response is built by
    // the same code either way.
    Creature const* entrance = packet.EntranceGUID.IsCreatureOrVehicle() ? ObjectAccessor::GetCreature(*player, packet.EntranceGUID) : nullptr;
    if (!entrance)
    {
        TC_LOG_DEBUG("network", "CMSG_TIERED_ENTRANCE_OPEN: entrance {} is not a creature in range of player {}",
            packet.EntranceGUID.ToString(), player->GetName());
        return;
    }

    sDelveMgr->SendTieredEntranceOpen(player, entrance);
}
