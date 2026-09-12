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

#include "DelveMgr.h"
#include <algorithm>
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DelvesPackets.h"
#include "DelvesRewards.h"
#include "DelvesSeason.h"
#include "Duration.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "Log.h"
#include "Map.h"
#include "NPCPackets.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellPackets.h"
#include "Timer.h"
#include "WorldSession.h"

namespace Delves
{

// How close an entrance NPC has to stand to a delve's stored overworld exit for the proximity
// fallback in GetDelveTemplateForEntrance() to claim it. Measured in the captures: 42 yd for The
// Shadow Enclave, 35 yd for The Gulf of Memory.
static constexpr float ENTRANCE_EXIT_MATCH_RADIUS = 200.0f;

DelveMgr::DelveMgr() = default;
DelveMgr::~DelveMgr() = default;

DelveMgr* DelveMgr::Instance()
{
    static DelveMgr instance;
    return &instance;
}

void DelveMgr::Initialize()
{
    uint32 oldMSTime = getMSTime();

    DetermineActiveSeason();
    LoadDelveTemplates();
    LoadTierRewards();
    LoadTieredEntranceTiers();

    TC_LOG_INFO("server.loading", ">> Loaded {} delve templates and {} tier rewards in {} ms",
        _delveTemplatesList.size(), _tierRewards.size(), GetMSTimeDiffToNow(oldMSTime));
}

void DelveMgr::DetermineActiveSeason()
{
    // The DelvesSeason DB2 (LayoutHash 0xD8CA312, build 67186) only carries
    // (ID, FactionID, VerifiedBuild) — no start/end date columns. Retail
    // determines the active season from server-side configuration that isn't
    // visible in the DB2 alone, so we fall back to "highest known ID" as a
    // proxy. This matches the audit MED gap #5 limitation: time-driving the
    // season selection isn't possible with the data the client ships in DB2.
    // To override, set delves_season.VerifiedBuild filtering in a future tier
    // or expose `DelveMgr.ActiveSeasonOverride` in worldserver.conf.
    uint32 highestSeasonId = 0;
    for (DelvesSeasonEntry const* entry : sDelvesSeasonStore)
    {
        if (entry->ID > highestSeasonId)
            highestSeasonId = entry->ID;
    }

    _activeSeasonId = highestSeasonId;

    if (_activeSeasonId > 0)
        TC_LOG_INFO("server.loading", ">> Active Delves Season: {}", _activeSeasonId);
    else
        TC_LOG_INFO("server.loading", ">> No Delves Season data found in DB2");
}

void DelveMgr::LoadDelveTemplates()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT id, mapId, scenarioId, mapChallengeModeId, zoneId, factionId, "
        "companionSpawnX, companionSpawnY, companionSpawnZ, companionSpawnO, "
        "gossipMenuId, lfgDungeonsId, broadcastTextId, firstTierGossipOptionId, "
        "entryX, entryY, entryZ, entryO, "
        "exitX, exitY, exitZ, exitO, "
        "activeScenarioId, rewardScenarioId, worldState26903, finalBossEntry, "
        "tieredEntranceId, tieredEntranceUnknown3, entranceUiWidgetSetId, modifierUiWidgetSetTier1, exitMapId "
        "FROM delve_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 delve templates. DB table `delve_template` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        DelveTemplate tmpl;
        tmpl.Id                       = fields[0].GetUInt32();
        tmpl.MapId                    = fields[1].GetUInt32();
        tmpl.ScenarioId               = fields[2].GetUInt32();
        tmpl.MapChallengeModeId       = fields[3].GetUInt32();
        tmpl.ZoneId                   = fields[4].GetUInt32();
        tmpl.FactionId                = fields[5].GetUInt32();
        tmpl.CompanionSpawnX          = fields[6].GetFloat();
        tmpl.CompanionSpawnY          = fields[7].GetFloat();
        tmpl.CompanionSpawnZ          = fields[8].GetFloat();
        tmpl.CompanionSpawnO          = fields[9].GetFloat();
        tmpl.GossipMenuId             = fields[10].GetUInt32();
        tmpl.LfgDungeonsId            = fields[11].GetUInt32();
        tmpl.BroadcastTextId          = fields[12].GetUInt32();
        tmpl.FirstTierGossipOptionId  = fields[13].GetUInt32();
        tmpl.EntryX                   = fields[14].GetFloat();
        tmpl.EntryY                   = fields[15].GetFloat();
        tmpl.EntryZ                   = fields[16].GetFloat();
        tmpl.EntryO                   = fields[17].GetFloat();
        tmpl.ExitX                    = fields[18].GetFloat();
        tmpl.ExitY                    = fields[19].GetFloat();
        tmpl.ExitZ                    = fields[20].GetFloat();
        tmpl.ExitO                    = fields[21].GetFloat();
        tmpl.ActiveScenarioId         = fields[22].GetUInt32();
        tmpl.RewardScenarioId         = fields[23].GetUInt32();
        tmpl.WorldState26903          = fields[24].GetUInt32();
        tmpl.FinalBossEntry           = fields[25].GetUInt32();
        tmpl.TieredEntranceId         = fields[26].GetUInt32();
        tmpl.TieredEntranceUnknown3   = fields[27].GetUInt32();
        tmpl.EntranceUiWidgetSetId    = fields[28].GetUInt32();
        tmpl.ModifierUiWidgetSetTier1 = fields[29].GetUInt32();
        tmpl.ExitMapId                = fields[30].GetInt32();

        _delveTemplatesByMap[tmpl.MapId] = tmpl;
        _delveTemplatesList.push_back(tmpl);
    }
    while (result->NextRow());

    // Build secondary index by MapChallengeModeId
    for (DelveTemplate const& tmpl : _delveTemplatesList)
        if (tmpl.MapChallengeModeId != 0)
            _delveTemplatesByChallengeModeId[tmpl.MapChallengeModeId] = &_delveTemplatesByMap[tmpl.MapId];

    // Build tertiary index by GossipMenuId (used by the entrance NPC script
    // to route gossip clicks to the correct delve template).
    for (DelveTemplate const& tmpl : _delveTemplatesList)
        if (tmpl.GossipMenuId != 0)
            _delveTemplatesByGossipMenuId[tmpl.GossipMenuId] = &_delveTemplatesByMap[tmpl.MapId];
}

void DelveMgr::LoadTieredEntranceTiers()
{
    _tieredEntranceTiers.clear();

    QueryResult result = WorldDatabase.Query("SELECT id, tier, suggestedILvl, overrideTooltipSpellId, unlockPlayerConditionId, "
        "dynamicUnlockPlayerConditionId, description FROM delve_tiered_entrance_tier ORDER BY tier");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 tiered entrance tiers. DB table `delve_tiered_entrance_tier` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        TieredEntranceTierData& tier = _tieredEntranceTiers.emplace_back();
        tier.Id                             = fields[0].GetUInt32();
        tier.Tier                           = fields[1].GetUInt8();
        tier.SuggestedILvl                  = fields[2].GetUInt32();
        tier.OverrideTooltipSpellId         = fields[3].GetUInt32();
        tier.UnlockPlayerConditionId        = fields[4].GetUInt32();
        tier.DynamicUnlockPlayerConditionId = fields[5].GetUInt32();
        tier.Description                    = fields[6].GetString();
    }
    while (result->NextRow());

    if (QueryResult rewards = WorldDatabase.Query("SELECT tierId, rewardType, id, quantity, context FROM delve_tiered_entrance_tier_reward ORDER BY tierId, orderIndex"))
    {
        do
        {
            Field* fields = rewards->Fetch();
            uint32 tierId = fields[0].GetUInt32();
            auto itr = std::find_if(_tieredEntranceTiers.begin(), _tieredEntranceTiers.end(), [tierId](TieredEntranceTierData const& t) { return t.Id == tierId; });
            if (itr == _tieredEntranceTiers.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `delve_tiered_entrance_tier_reward` references unknown tier {}, skipped.", tierId);
                continue;
            }
            TieredEntranceRewardData& reward = itr->Rewards.emplace_back();
            reward.RewardType = fields[1].GetUInt8();
            reward.Id         = fields[2].GetUInt32();
            reward.Quantity   = fields[3].GetUInt32();
            reward.Context    = fields[4].GetUInt8();
        }
        while (rewards->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} tiered entrance tiers", _tieredEntranceTiers.size());
}

TieredEntranceTierData const* DelveMgr::GetTieredEntranceTier(uint32 tieredEntranceTierId) const
{
    for (TieredEntranceTierData const& tier : _tieredEntranceTiers)
        if (tier.Id == tieredEntranceTierId)
            return &tier;
    return nullptr;
}

void DelveMgr::LoadTierRewards()
{
    QueryResult result = WorldDatabase.Query("SELECT tier, itemContext, maxRevives, crestType, crestCount FROM delve_tier_rewards");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 delve tier rewards. DB table `delve_tier_rewards` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        DelveTierReward reward;
        reward.Tier        = fields[0].GetUInt8();
        reward.ItemContext  = fields[1].GetUInt8();
        reward.MaxRevives  = fields[2].GetUInt8();
        reward.CrestType   = fields[3].GetUInt8();
        reward.CrestCount  = fields[4].GetUInt8();

        _tierRewards[reward.Tier] = reward;
    }
    while (result->NextRow());
}

DelvesSeasonEntry const* DelveMgr::GetActiveSeason() const
{
    return sDelvesSeasonStore.LookupEntry(_activeSeasonId);
}

DelveTemplate const* DelveMgr::GetDelveTemplate(uint32 mapId) const
{
    auto itr = _delveTemplatesByMap.find(mapId);
    return itr != _delveTemplatesByMap.end() ? &itr->second : nullptr;
}

DelveTemplate const* DelveMgr::GetDelveTemplateByChallengeModeId(uint32 mapChallengeModeId) const
{
    auto itr = _delveTemplatesByChallengeModeId.find(mapChallengeModeId);
    return itr != _delveTemplatesByChallengeModeId.end() ? itr->second : nullptr;
}

DelveTemplate const* DelveMgr::GetDelveTemplateByGossipMenuId(uint32 gossipMenuId) const
{
    auto itr = _delveTemplatesByGossipMenuId.find(gossipMenuId);
    return itr != _delveTemplatesByGossipMenuId.end() ? itr->second : nullptr;
}

/*
 * Resolve an entrance NPC to the delve it opens.
 *
 * The obvious lookup - GetDelveTemplateByGossipMenuId(entrance->GetGossipMenuId()) - never worked:
 * Creature::SetGossipMenuId() has no call site anywhere in the core, so _gossipMenuId is 0 on every
 * creature. That single miss killed all three entrance paths at once:
 * npc_delve_entrance::OnGossipHello, WorldSession::HandleTieredEntranceOpen (the 12.0.7 path the
 * live client actually uses) and WorldSession::HandleSelectDelveEntranceTier. The tiered-entrance
 * handler in particular bailed out with "could not resolve entrance ... to a delve template" for
 * every delve, every time.
 *
 * The fallback chain below, in order:
 *   1. The script-set menu override, if some script ever does call SetGossipMenuId().
 *   2. The map the NPC stands on, for entrance NPCs placed inside a delve instance.
 *   3. Proximity to a delve's stored overworld exit. Delve entrances stand next to the point the
 *      delve returns you to - measured in the captures: the Shadow Enclave entrance sits 42 yd from
 *      delve_template(2952).exit and the Gulf of Memory entrance 35 yd from delve_template(2964).
 *      exit. Templates with a zeroed exit are skipped so unfilled rows cannot capture a lookup.
 *   4. The creature template's gossip menus (creature_template_gossip -> CreatureTemplate::
 *      GossipMenuIds), e.g. 212407 "Enter Delve" -> 39751 (Atal'Aman).
 *
 * ORDER MATTERS, and it is not the obvious one. Step 4 looks like it should come first - it is the
 * only explicitly authored link - but creature_template_gossip is keyed on the creature TEMPLATE,
 * and 212407 "Enter Delve" is one shared entry used by every delve. Putting it first would make
 * every "Enter Delve" spawn in the world open Atal'Aman. Position is the only thing that differs
 * between those spawns, so proximity has to win, with the template menu as the last-resort default
 * for a spawn that is nowhere near any known exit.
 *
 * Step 3 deliberately does NOT compare map ids: delve_template has no column for the overworld map,
 * only for the delve's own. The 200 yd radius plus the "exit must be non-zero" guard keeps it
 * unambiguous for every delve currently in the table - the filled exits are thousands of yards
 * apart. If two delves ever land within 200 yd of each other this needs an exitMapId column rather
 * than a wider heuristic. Step 2 runs ahead of step 3 for the mirror-image reason: Atal'Aman's
 * stored "exit" (5121, -5861, 217.1) is actually a coordinate INSIDE map 2962, so an NPC standing
 * in that instance must be resolved by map before proximity can agree with it by accident.
 */
DelveTemplate const* DelveMgr::GetDelveTemplateForEntrance(Creature const* entrance) const
{
    if (!entrance)
        return nullptr;

    // 1. Script override.
    if (uint32 scriptMenuId = entrance->GetGossipMenuId())
        if (DelveTemplate const* tmpl = GetDelveTemplateByGossipMenuId(scriptMenuId))
            return tmpl;

    // 2. The NPC stands inside the delve itself.
    if (DelveTemplate const* tmpl = GetDelveTemplate(entrance->GetMapId()))
        return tmpl;

    // 3. Nearest stored overworld exit.
    DelveTemplate const* closest = nullptr;
    float bestDistSq = ENTRANCE_EXIT_MATCH_RADIUS * ENTRANCE_EXIT_MATCH_RADIUS;
    for (DelveTemplate const& candidate : _delveTemplatesList)
    {
        if (candidate.ExitX == 0.0f && candidate.ExitY == 0.0f)
            continue;

        float const dx = entrance->GetPositionX() - candidate.ExitX;
        float const dy = entrance->GetPositionY() - candidate.ExitY;
        float const distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            closest = &candidate;
        }
    }

    if (closest)
        return closest;

    // 4. Last resort: whatever delve menu the creature template carries.
    if (CreatureTemplate const* creatureTemplate = entrance->GetCreatureTemplate())
        for (uint32 menuId : creatureTemplate->GossipMenuIds)
            if (DelveTemplate const* tmpl = GetDelveTemplateByGossipMenuId(menuId))
                return tmpl;

    return nullptr;
}

DelveTierReward const* DelveMgr::GetTierReward(uint8 tier) const
{
    auto itr = _tierRewards.find(tier);
    return itr != _tierRewards.end() ? &itr->second : nullptr;
}

Optional<uint32> DelveMgr::GetTieredEntrancePDEID(uint32 tieredEntranceId)
{
    // Stubbed — see TieredEntranceCVarNames comment block in DelvesDefines.h.
    // The retail client computes this via an anti-analysis-obfuscated Lua
    // binding (IDA `0x7FF75C96A1EC`). Until a retail sniff captures
    // SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED for a tier completion, we
    // cannot statically populate per-tier PDE records.
    TC_LOG_TRACE("scripts.delves",
        "DelveMgr::GetTieredEntrancePDEID({}) — encoding not yet decoded, returning nullopt",
        tieredEntranceId);
    return std::nullopt;
}

TieredEntranceType DelveMgr::GetTieredEntranceType(uint32 tieredEntranceId)
{
    // The Lua binding GetTieredEntranceType (IDA `0x7FF75C96A80C`) is also
    // obfuscated. As a server-side fallback, callers should resolve the
    // tieredEntranceID to a mapId via their own context (e.g. delve_template
    // join) and return TIERED_ENTRANCE_TYPE_DELVE for any registered delve.
    // This stub returns Invalid for now.
    TC_LOG_TRACE("scripts.delves",
        "DelveMgr::GetTieredEntranceType({}) — lookup not yet decoded, returning Invalid",
        tieredEntranceId);
    return TIERED_ENTRANCE_TYPE_INVALID;
}

bool DelveMgr::IsTieredEntranceScenarioMap(uint32 mapId) const
{
    return _delveTemplatesByMap.find(mapId) != _delveTemplatesByMap.end();
}

bool DelveMgr::IsDelveCurrentlyBountiful(uint32 delveTemplateId) const
{
    std::vector<uint32> bountiful = GetTodaysBountifulDelves();
    return std::find(bountiful.begin(), bountiful.end(), delveTemplateId) != bountiful.end();
}

std::vector<uint32> DelveMgr::GetTodaysBountifulDelves() const
{
    // Keyed on delve_template.Id — delves do not use MapChallengeMode ids (that column is 0 for every row,
    // which previously made every delve "bountiful" through the 0 == 0 match).
    std::vector<uint32> result;

    if (_delveTemplatesList.empty())
        return result;

    // Rotate through all delves: 4 per day, cycling so all delves appear before repeating
    uint32 totalDelves = static_cast<uint32>(_delveTemplatesList.size());

    // Day number since epoch
    uint32 dayNumber = static_cast<uint32>(GameTime::GetGameTime() / DAY);
    // How many full cycles through all delves
    uint32 startIdx = (dayNumber * BOUNTIFUL_DELVES_PER_DAY) % totalDelves;

    for (uint32 i = 0; i < BOUNTIFUL_DELVES_PER_DAY && i < totalDelves; ++i)
    {
        uint32 idx = (startIdx + i) % totalDelves;
        result.push_back(_delveTemplatesList[idx].Id);
    }

    return result;
}

// ---------------------------------------------------------------------------------------------
// Retail run flow (12.1.0.69497 captures, C:\sniff\tcharvest\out\delve_research\REPORT.md)
// ---------------------------------------------------------------------------------------------

void DelveMgr::SendTieredEntranceOpen(Player* player, Creature const* entrance)
{
    if (!player || !entrance)
        return;

    DelveTemplate const* tmpl = GetDelveTemplateForEntrance(entrance);
    if (!tmpl)
    {
        TC_LOG_DEBUG("scripts.delves", "DelveMgr::SendTieredEntranceOpen: could not resolve entrance {} (entry {}, map {}) to a delve template",
            entrance->GetGUID().ToString(), entrance->GetEntry(), entrance->GetMapId());
        return;
    }

    DelveProgress progress;
    DelvesRewards::LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
    bool const meetsLevel = DelvesSeason::MeetsMinimumLevelRequirement(player);

    // REPORT.md 1.3 (gulf 88718 == 98437, eversong 1842501; 12.0.7 The Darkway agrees): every delve entrance reports
    // EntranceType 1, field 7 is 234 for every delve, field 6 is 0, fields 3/4/8 and the widget sets are per entrance
    // (delve_template), the tier rows are season data (delve_tiered_entrance_tier). The client matches the response
    // by the entrance GUID - two spawns of 212407 can be visible at once (REPORT.md 1.2), so echo this spawn's guid.
    WorldPackets::Delves::TieredEntranceOpenResponse response;
    response.EntranceGUID = entrance->GetGUID();
    response.EntranceType = TIERED_ENTRANCE_TYPE_DELVE;
    response.MapID = tmpl->MapId;
    response.Unknown3 = tmpl->TieredEntranceUnknown3;
    response.Unknown4 = tmpl->EntranceUiWidgetSetId;
    response.Unknown6 = 0;
    response.Unknown7 = DELVE_TIERED_ENTRANCE_FIELD7;
    response.Unknown8 = tmpl->TieredEntranceId;

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(tmpl->MapId))
        response.EntranceDescription = mapEntry->MapName[player->GetSession()->GetSessionDbcLocale()];

    response.Tiers.reserve(_tieredEntranceTiers.size());
    for (TieredEntranceTierData const& tierRow : _tieredEntranceTiers)
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
        for (TieredEntranceRewardData const& reward : tierRow.Rewards)
        {
            WorldPackets::Delves::TieredEntranceReward& rewardData = tierData.PreviewTreasureList.emplace_back();
            rewardData.RewardType = reward.RewardType;
            rewardData.Id = reward.Id;
            rewardData.Quantity = reward.Quantity;
            rewardData.Context = reward.Context;
        }
    }

    player->SendDirectMessage(response.Write());
}

void DelveMgr::OpenEntranceByProximity(Player* player, Creature const* entrance)
{
    if (!player || !entrance || !player->IsInWorld() || player->IsBeingTeleported())
        return;

    // an entrance NPC standing inside a delve never opens the picker
    if (GetDelveTemplate(player->GetMapId()))
        return;

    // once per approach (REPORT.md 1.1: gulf opened at 88718, CMSG_CLOSE_INTERACTION at 95751 when the player walked
    // away, opened again at 98205). WorldSession::HandleCloseInteraction resets the interaction for this guid.
    InteractionData& interaction = player->PlayerTalkClass->GetInteractionData();
    PlayerInteractionType const interactionType = PlayerInteractionType(DELVE_ENTRANCE_INTERACTION_TYPE);
    if (interaction.IsInteractingWith(entrance->GetGUID(), interactionType))
        return;

    if (!GetDelveTemplateForEntrance(entrance))
        return;

    interaction.StartInteraction(entrance->GetGUID(), interactionType);

    // REPORT.md 1.1, gulf 98205: `PackedGUID(212407) | 4f000000 | 00` = entrance guid + int32 InteractionType 79 + bit Success
    WorldPackets::NPC::NPCInteractionOpenResult openResult;
    openResult.Npc = entrance->GetGUID();
    openResult.InteractionType = interactionType;
    openResult.Success = true;
    player->SendDirectMessage(openResult.Write());

    SendTieredEntranceOpen(player, entrance);
}

void DelveMgr::CloseEntranceByProximity(Player* player, Creature const* entrance)
{
    if (!player || !entrance)
        return;

    InteractionData& interaction = player->PlayerTalkClass->GetInteractionData();
    if (interaction.IsInteractingWith(entrance->GetGUID(), PlayerInteractionType(DELVE_ENTRANCE_INTERACTION_TYPE)))
        interaction.Reset();
}

void DelveMgr::EnterDelve(Player* player, DelveTemplate const& tmpl, uint8 tier)
{
    if (!player || !player->IsInWorld() || tier == 0 || tier > MAX_DELVE_TIER)
        return;

    // eversong sent CMSG_SELECT_DELVE_ENTRANCE_TIER twice (1844955 and 1846555, the second after
    // CMSG_AUTH_CONTINUED_SESSION); retail had already acted on the first (REPORT.md 1.4)
    if (player->IsBeingTeleported() || player->GetMapId() == tmpl.MapId)
        return;

    // Remember where to come back to: retail returns the player to the delve's exit coordinates on the map
    // they entered from - Harandar 2694 for the Gulf of Memory, map 0 for the Shadow Enclave (REPORT.md 1.5 / 5).
    if (!GetDelveTemplate(player->GetMapId()))
    {
        if (tmpl.ExitX != 0.0f || tmpl.ExitY != 0.0f)
            player->m_delveReturnLocation = WorldLocation(player->GetMapId(), tmpl.ExitX, tmpl.ExitY, tmpl.ExitZ, tmpl.ExitO);
        else
            player->m_delveReturnLocation = player->GetWorldLocation();
    }

    player->m_delveSelectedMapId = tmpl.MapId;
    player->m_delveSelectedTier = tier;

    // the picker closes with the selection; the choice-clear that precedes SMSG_NEW_WORLD is sent by Player::TeleportTo
    player->PlayerTalkClass->GetInteractionData().Reset();

    // Keep the client-side JamDelveData progression mirror's last-selected delve current.
    DelvesRewards::PublishProgress(player);

    // REPORT.md 1.6: the per-run world states. Retail delivers them with SMSG_INIT_WORLD_STATES of the delve map and
    // re-sends them as SMSG_UPDATE_WORLD_STATE at the same tick (gulf 101842); OnPlayerEnteredDelve puts them on the
    // delve Map for that. Sending them ahead of the transfer keeps the picker / HUD consistent while the transfer is
    // pending (branch behaviour inherited from npc_delve_entrance::OnGossipSelect, not on the wire).
    player->SendUpdateWorldState(WS_DELVE_TIER, tier);
    player->SendUpdateWorldState(WS_DELVE_IN_DELVE_FLAG, 1);
    player->SendUpdateWorldState(WS_DELVE_MAP_ID, tmpl.MapId);
    player->SendUpdateWorldState(WS_DELVE_TIER_SPELL, GetTierSpellId(tier));
    if (tmpl.WorldState26903)
        player->SendUpdateWorldState(WS_DELVE_UNKNOWN_26903, tmpl.WorldState26903);

    TC_LOG_DEBUG("scripts.delves", "DelveMgr::EnterDelve: player {} -> map {} tier {} (return to map {})",
        player->GetName(), tmpl.MapId, tier, player->m_delveReturnLocation.GetMapId());

    // REPORT.md 1.5: no SMSG_TRANSFER_PENDING, SMSG_NEW_WORLD reason 21 (seamless) ~1.9 s after the select.
    // Player::TeleportTo keeps TELE_TO_SEAMLESS for delve maps although they are not cosmetic children of the
    // outdoor map. The map difficulty resolves to 208 through the MapDifficulty downscale fallback (REPORT.md 1.7).
    player->TeleportTo(tmpl.MapId, tmpl.EntryX, tmpl.EntryY, tmpl.EntryZ, tmpl.EntryO, TELE_TO_SEAMLESS);
}

namespace
{
// REPORT.md 4 / work item 6: the hidden entry quest. Preferred path is the real quest template (so the quest log /
// criteria side effects match retail); when it is not in the world DB - or cannot be taken again - the same rewards
// are granted directly with DisplayToastMethod::QuestComplete (16) toasts, as observed in toasts_items.txt.
void GrantDelveEntryRewards(Player* player)
{
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(DELVE_ENTRY_REWARD_QUEST_ID))
    {
        if (player->CanTakeQuest(quest, false) && player->CanAddQuest(quest, false))
        {
            player->AddQuestAndCheckCompletion(quest, nullptr);
            if (player->GetQuestStatus(DELVE_ENTRY_REWARD_QUEST_ID) == QUEST_STATUS_COMPLETE && player->CanRewardQuest(quest, false))
                player->RewardQuest(quest, LootItemType::Item, 0, nullptr, true);
            return;
        }
    }

    player->AddCurrency(CURRENCY_COFFER_KEY_SHARDS, DELVE_ENTRY_REWARD_SHARDS, CurrencyGainSource::QuestReward);
    player->SendDisplayToast(CURRENCY_COFFER_KEY_SHARDS, DisplayToastType::NewCurrency, false, DELVE_ENTRY_REWARD_SHARDS,
        DisplayToastMethod::QuestComplete, DELVE_ENTRY_REWARD_QUEST_ID);

    player->AddCurrency(CURRENCY_VOIDLIGHT_MARL, DELVE_ENTRY_REWARD_MARL, CurrencyGainSource::QuestReward);
    player->SendDisplayToast(CURRENCY_VOIDLIGHT_MARL, DisplayToastType::NewCurrency, false, DELVE_ENTRY_REWARD_MARL,
        DisplayToastMethod::QuestComplete, DELVE_ENTRY_REWARD_QUEST_ID);

    if (sObjectMgr->GetItemTemplate(DELVE_ENTRY_REWARD_ITEM))
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, DELVE_ENTRY_REWARD_ITEM, 1) == EQUIP_ERR_OK)
        {
            if (Item* item = player->StoreNewItem(dest, DELVE_ENTRY_REWARD_ITEM, true, GenerateItemRandomBonusListId(DELVE_ENTRY_REWARD_ITEM), {}, ItemContext::Quest_Reward))
            {
                player->SendNewItem(item, 1, true, false);
                player->SendDisplayToast(0, DisplayToastType::NewItem, false, 1, DisplayToastMethod::QuestComplete, DELVE_ENTRY_REWARD_QUEST_ID, item);
            }
        }
        else
            player->SendItemRetrievalMail(DELVE_ENTRY_REWARD_ITEM, 1, ItemContext::Quest_Reward);
    }
}
}

void DelveMgr::OnPlayerEnteredDelve(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    Map* map = player->GetMap();
    DelveTemplate const* tmpl = GetDelveTemplate(map->GetId());
    if (!tmpl)
        return;

    // The run's tier: what the first entrant put on the map, else this player's selection (group members who were
    // brought in without selecting keep the instance's tier).
    uint8 tier = uint8(std::clamp<int32>(map->GetWorldStateValue(WS_DELVE_TIER), 0, MAX_DELVE_TIER));
    if (!tier)
        tier = std::clamp<uint8>(player->m_delveSelectedTier, 1, MAX_DELVE_TIER);
    player->m_delveSelectedTier = tier;
    player->m_delveSelectedMapId = tmpl->MapId;

    // REPORT.md 1.6: SMSG_INIT_WORLD_STATES of the delve map carries these and they are re-sent as
    // SMSG_UPDATE_WORLD_STATE at the same tick. Setting them on the Map makes INIT carry them for everyone who
    // enters later and broadcasts the UPDATE on change; an unchanged value is re-sent to this player only.
    auto publish = [&](uint32 worldStateId, int32 value)
    {
        if (!map->GetWorldStateValues().contains(int32(worldStateId)) || map->GetWorldStateValue(int32(worldStateId)) != value)
            map->SetWorldStateValue(int32(worldStateId), value, false);
        else
            player->SendUpdateWorldState(worldStateId, uint32(value));
    };

    publish(WS_DELVE_TIER, tier);
    publish(WS_DELVE_IN_DELVE_FLAG, 1);
    publish(WS_DELVE_MAP_ID, int32(tmpl->MapId));
    publish(WS_DELVE_TIER_SPELL, int32(GetTierSpellId(tier)));
    if (tmpl->WorldState26903)
        publish(WS_DELVE_UNKNOWN_26903, int32(tmpl->WorldState26903));
    if (tmpl->LfgDungeonsId)
        publish(WS_DELVE_LFG_DUNGEONS_ID, int32(tmpl->LfgDungeonsId));
    publish(WS_DELVE_COMPLETE, 0);
    publish(WS_DELVE_ENCOUNTER_IN_PROGRESS, 0);

    // REPORT.md 4 (work item 6): the hidden entry quest completes ~6.6 s after SMSG_NEW_WORLD (gulf: NEW_WORLD 101829,
    // quest credit + toasts ~108400). Once per run: keyed on the instance id so a relog into the same run does not
    // grant twice. The event lives on the player's own processor and dies with the player.
    if (player->m_delveEntryRewardInstanceId == map->GetInstanceId())
        return;

    player->m_delveEntryRewardInstanceId = map->GetInstanceId();

    uint32 const mapId = map->GetId();
    uint32 const instanceId = map->GetInstanceId();
    player->m_Events.AddEventAtOffset([player, mapId, instanceId]()
    {
        if (!player->IsInWorld() || player->GetMapId() != mapId || player->GetInstanceId() != instanceId)
            return;

        GrantDelveEntryRewards(player);
    }, Milliseconds(DELVE_ENTRY_REWARD_DELAY_MS));
}

void DelveMgr::LeaveDelve(Player* player)
{
    if (!player || !player->IsInWorld() || player->IsBeingTeleported())
        return;

    DelveTemplate const* tmpl = GetDelveTemplate(player->GetMapId());
    if (!tmpl)
        return;

    // Destination: the map the player entered from at the exit coordinates (REPORT.md 5 step 6: gulf back to 2694
    // (46.226, 810.912, 1109.843), eversong back to 0 (4780.455, -4118.306, 32.133)), else the template's exit map,
    // else homebind. Never another delve.
    WorldLocation destination = player->m_delveReturnLocation;
    if (destination.GetMapId() == MAPID_INVALID || GetDelveTemplate(destination.GetMapId()))
    {
        if (tmpl->ExitMapId >= 0 && (tmpl->ExitX != 0.0f || tmpl->ExitY != 0.0f))
            destination = WorldLocation(uint32(tmpl->ExitMapId), tmpl->ExitX, tmpl->ExitY, tmpl->ExitZ, tmpl->ExitO);
        else
            destination = player->m_homebind;
    }

    // REPORT.md 5 step 6: SMSG_SPELL_VISUAL_LOAD_SCREEN (kit 79917, 1500 ms) right after the spell-click on the
    // Leave-O-Bot - a seamless SMSG_NEW_WORLD shows no loading screen of its own, this kit covers the swap.
    WorldPackets::Spells::SpellVisualLoadScreen loadScreen{ int32(DELVE_EXIT_LOAD_SCREEN_KIT_ID), Milliseconds(DELVE_EXIT_LOAD_SCREEN_DURATION_MS) };
    player->SendDirectMessage(loadScreen.Write());

    // Branch behaviour kept from go_leave_delve (not on the wire: retail only sends SMSG_INIT_WORLD_STATES of the
    // outdoor map after the transfer): zero the per-run states so the HUD drops out of delve mode.
    player->SendUpdateWorldState(WS_DELVE_TIER, 0);
    player->SendUpdateWorldState(WS_DELVE_IN_DELVE_FLAG, 0);
    player->SendUpdateWorldState(WS_DELVE_MAP_ID, 0);
    player->SendUpdateWorldState(WS_DELVE_TIER_SPELL, 0);
    player->SendUpdateWorldState(WS_DELVE_UNKNOWN_26903, 0);

    player->ClearDelveData(int32(tmpl->MapId));
    player->m_delveReturnLocation = WorldLocation();
    player->m_delveSelectedTier = 0;
    player->m_delveSelectedMapId = 0;

    TC_LOG_DEBUG("scripts.delves", "DelveMgr::LeaveDelve: player {} leaves map {} -> map {} ({:.1f} {:.1f} {:.1f})",
        player->GetName(), tmpl->MapId, destination.GetMapId(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());

    // REPORT.md 5: the transfer back is seamless as well (SMSG_NEW_WORLD, no SMSG_TRANSFER_PENDING). Retail's
    // NEW_WORLD followed the load screen by several seconds (phase / object despawns and a server hop); here the
    // transfer runs once the load-screen kit has faded in, on the player's own event processor.
    uint32 const mapId = player->GetMapId();
    uint32 const instanceId = player->GetInstanceId();
    player->m_Events.AddEventAtOffset([player, mapId, instanceId, destination]()
    {
        if (!player->IsInWorld() || player->IsBeingTeleported() || player->GetMapId() != mapId || player->GetInstanceId() != instanceId)
            return;

        player->TeleportTo(destination, TELE_TO_SEAMLESS);
    }, Milliseconds(DELVE_EXIT_LOAD_SCREEN_DURATION_MS));
}

} // namespace Delves
