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

    // The 68275 wire carries the entrance ObjectGuid, not a MapID — re-derive the
    // delve map server-side from the entrance creature. This used to read
    // Creature::GetGossipMenuId() directly, which is always 0 because nothing in the core ever
    // calls SetGossipMenuId(); DelveMgr::GetDelveTemplateForEntrance() does the real resolution.
    uint32 mapId = 0;
    if (packet.EntranceGUID.IsCreatureOrVehicle())
        if (Creature const* entrance = ObjectAccessor::GetCreature(*player, packet.EntranceGUID))
            if (Delves::DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplateForEntrance(entrance))
                mapId = tmpl->MapId;

    if (!mapId)
        TC_LOG_DEBUG("network", "CMSG_SELECT_DELVE_ENTRANCE_TIER: could not resolve entrance {} to a delve template",
            packet.EntranceGUID.ToString());

    // Selection is consumed by the subsequent entrance-open flow; the client
    // re-sends the tier on entrance. We accept and validate here so eligibility is logged.
    player->m_delveSelectedTier = tier;
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

    Delves::DelveTemplate const* tmpl = nullptr;
    if (packet.EntranceGUID.IsCreatureOrVehicle())
        if (Creature const* entrance = ObjectAccessor::GetCreature(*player, packet.EntranceGUID))
            tmpl = sDelveMgr->GetDelveTemplateForEntrance(entrance);

    if (!tmpl)
    {
        TC_LOG_DEBUG("network", "CMSG_TIERED_ENTRANCE_OPEN: could not resolve entrance {} to a delve template",
            packet.EntranceGUID.ToString());
        return;
    }

    Delves::DelveProgress progress;
    Delves::DelvesRewards::LoadProgress(GetBattlenetAccountId(), progress);
    bool meetsLevel = Delves::DelvesSeason::MeetsMinimumLevelRequirement(player);

    // 12.1.0.69497 captures (Gulf of Memory, Shadow Enclave; 12.0.7 The Darkway agrees): every delve
    // entrance reports EntranceType 1, field 7 is 234 for every delve, field 6 is 0, fields 3/4/8 and
    // the widget sets are per entrance (delve_template), the tier rows are season data
    // (delve_tiered_entrance_tier). The client matches the response by the echoed entrance GUID.
    WorldPackets::Delves::TieredEntranceOpenResponse response;
    response.EntranceGUID = packet.EntranceGUID;
    response.EntranceType = Delves::TIERED_ENTRANCE_TYPE_DELVE;
    response.MapID = tmpl->MapId;
    response.Unknown3 = tmpl->TieredEntranceUnknown3;
    response.Unknown4 = tmpl->EntranceUiWidgetSetId;
    response.Unknown6 = 0;
    response.Unknown7 = Delves::DELVE_TIERED_ENTRANCE_FIELD7;
    response.Unknown8 = tmpl->TieredEntranceId;

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(tmpl->MapId))
        response.EntranceDescription = mapEntry->MapName[GetSessionDbcLocale()];

    std::vector<Delves::TieredEntranceTierData> const& tiers = sDelveMgr->GetTieredEntranceTiers();
    response.Tiers.reserve(tiers.size());
    for (Delves::TieredEntranceTierData const& tierRow : tiers)
    {
        WorldPackets::Delves::TieredEntranceTier& tierData = response.Tiers.emplace_back();
        tierData.TieredEntranceTierID = tierRow.Id;
        tierData.Tier = tierRow.Tier;
        tierData.SuggestedILvl = tierRow.SuggestedILvl;
        tierData.OverrideTooltipSpellID = tierRow.OverrideTooltipSpellId;
        tierData.UnlockPlayerConditionID = tierRow.UnlockPlayerConditionId;
        tierData.DynamicUnlockPlayerConditionID = tierRow.DynamicUnlockPlayerConditionId;
        tierData.ModifierUIWidgetSetID = tmpl->ModifierUiWidgetSetTier1 && tierRow.Tier ? tmpl->ModifierUiWidgetSetTier1 - (tierRow.Tier - 1) : 0;
        tierData.Unlocked = meetsLevel && tierRow.Tier <= progress.HighestTierUnlocked;
        tierData.TierDescription = tierRow.Description;
        for (Delves::TieredEntranceRewardData const& reward : tierRow.Rewards)
        {
            WorldPackets::Delves::TieredEntranceReward& rewardData = tierData.PreviewTreasureList.emplace_back();
            rewardData.RewardType = reward.RewardType;
            rewardData.Id = reward.Id;
            rewardData.Quantity = reward.Quantity;
            rewardData.Context = reward.Context;
        }
    }

    SendPacket(response.Write());
}
