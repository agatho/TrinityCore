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
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"

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
        "activeScenarioId, rewardScenarioId, worldState26903, finalBossEntry "
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

} // namespace Delves
