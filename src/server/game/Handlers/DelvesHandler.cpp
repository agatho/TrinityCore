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
#include "DB2Stores.h"
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

void WorldSession::HandleTieredEntranceOpen(WorldPackets::Delves::TieredEntranceOpen& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_TIERED_ENTRANCE_OPEN received from player {} entrance {}",
        player->GetName(), packet.EntranceGUID.ToString());

    // Resolve the entrance NPC to a delve template (same pattern as
    // HandleSelectDelveEntranceTier: entrances are gossip NPCs).
    Delves::DelveTemplate const* tmpl = nullptr;
    if (packet.EntranceGUID.IsCreatureOrVehicle())
        if (Creature const* entrance = ObjectAccessor::GetCreature(*player, packet.EntranceGUID))
            tmpl = sDelveMgr->GetDelveTemplateByGossipMenuId(entrance->GetGossipMenuId());

    if (!tmpl)
    {
        TC_LOG_DEBUG("network", "CMSG_TIERED_ENTRANCE_OPEN: could not resolve entrance {} to a delve template",
            packet.EntranceGUID.ToString());
        return;
    }

    Delves::DelveProgress progress;
    Delves::DelvesRewards::LoadProgress(GetBattlenetAccountId(), progress);
    bool meetsLevel = Delves::DelvesSeason::MeetsMinimumLevelRequirement(player);

    // Suggested item level per tier. Tiers 1-6 are the sniffed Midnight S1
    // values (rated BG 12.0.7.pkt, Daggerspine Point: 215/231/244/257/264/274);
    // tiers 7-11 extrapolate the observed +10/tier endgame slope — ASSUMED.
    static constexpr uint32 SUGGESTED_ILVL[Delves::MAX_DELVE_TIER] =
    {
        215, 231, 244, 257, 264, 274, 284, 294, 304, 314, 324
    };

    WorldPackets::Delves::TieredEntranceOpenResponse response;
    // The client matches the response to its pending open request by the
    // entrance GUID — the echo must be byte-exact.
    response.EntranceGUID = packet.EntranceGUID;
    // Sniffed Midnight entrance reports TieredEntranceType 2 (Sites) — all
    // Midnight S1 tiered entrances are "sites" on the wire.
    response.EntranceType = Delves::TIERED_ENTRANCE_TYPE_SITES;
    response.MapID = tmpl->MapId;
    // Header unknowns (see TIERED_ENTRANCE_RE_68275.md): Unknown3 observed to
    // equal the last tier record's TieredEntranceTierID; the rest have no
    // resolvable server-side source yet and default to 0.
    response.Unknown3 = Delves::MAX_DELVE_TIER;

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(tmpl->MapId))
        response.EntranceDescription = mapEntry->MapName[GetSessionDbcLocale()];

    response.Tiers.reserve(Delves::MAX_DELVE_TIER);
    for (uint8 tier = 1; tier <= Delves::MAX_DELVE_TIER; ++tier)
    {
        WorldPackets::Delves::TieredEntranceTier& tierData = response.Tiers.emplace_back();
        // No TieredEntranceTier DB2 exists client-side (server-data driven) —
        // use the tier number as a stable row id.
        tierData.TieredEntranceTierID = tier;
        tierData.Tier = tier;
        tierData.SuggestedILvl = SUGGESTED_ILVL[tier - 1];
        // Player conditions 0 = no client-side gate; unlock state is carried
        // by the Unlocked bit below instead (sniff shows retail sends both).
        tierData.UnlockPlayerConditionID = 0;
        tierData.DynamicUnlockPlayerConditionID = 0;
        tierData.ModifierUIWidgetSetID = 0;
        tierData.Unlocked = meetsLevel && tier <= progress.HighestTierUnlocked;
        tierData.TierDescription = Delves::TIER_NAMES[tier - 1];
        // PreviewTreasureList left empty: the sniffed item ids are
        // entrance-specific content we cannot generically source yet.
    }

    SendPacket(response.Write());
}
