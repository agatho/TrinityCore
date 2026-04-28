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
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Log.h"

namespace Delves
{

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
    QueryResult result = WorldDatabase.Query("SELECT id, mapId, scenarioId, mapChallengeModeId, zoneId, factionId, "
        "companionSpawnX, companionSpawnY, companionSpawnZ, companionSpawnO FROM delve_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 delve templates. DB table `delve_template` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        DelveTemplate tmpl;
        tmpl.Id                   = fields[0].GetUInt32();
        tmpl.MapId                = fields[1].GetUInt32();
        tmpl.ScenarioId           = fields[2].GetUInt32();
        tmpl.MapChallengeModeId   = fields[3].GetUInt32();
        tmpl.ZoneId               = fields[4].GetUInt32();
        tmpl.FactionId            = fields[5].GetUInt32();
        tmpl.CompanionSpawnX      = fields[6].GetFloat();
        tmpl.CompanionSpawnY      = fields[7].GetFloat();
        tmpl.CompanionSpawnZ      = fields[8].GetFloat();
        tmpl.CompanionSpawnO      = fields[9].GetFloat();

        _delveTemplatesByMap[tmpl.MapId] = tmpl;
        _delveTemplatesList.push_back(tmpl);
    }
    while (result->NextRow());

    // Build secondary index by MapChallengeModeId
    for (DelveTemplate const& tmpl : _delveTemplatesList)
        if (tmpl.MapChallengeModeId != 0)
            _delveTemplatesByChallengeModeId[tmpl.MapChallengeModeId] = &_delveTemplatesByMap[tmpl.MapId];
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

bool DelveMgr::IsDelveCurrentlyBountiful(uint32 mapChallengeModeId) const
{
    std::vector<uint32> bountiful = GetTodaysBountifulDelves();
    return std::find(bountiful.begin(), bountiful.end(), mapChallengeModeId) != bountiful.end();
}

std::vector<uint32> DelveMgr::GetTodaysBountifulDelves() const
{
    std::vector<uint32> result;

    if (_delveTemplatesList.empty())
        return result;

    // Rotate through all delves: 4 per day, cycling so all delves appear before repeating
    uint32 totalDelves = static_cast<uint32>(_delveTemplatesList.size());
    if (totalDelves == 0)
        return result;

    // Day number since epoch
    uint32 dayNumber = static_cast<uint32>(GameTime::GetGameTime() / DAY);
    // How many full cycles through all delves
    uint32 startIdx = (dayNumber * BOUNTIFUL_DELVES_PER_DAY) % totalDelves;

    for (uint32 i = 0; i < BOUNTIFUL_DELVES_PER_DAY && i < totalDelves; ++i)
    {
        uint32 idx = (startIdx + i) % totalDelves;
        result.push_back(_delveTemplatesList[idx].MapChallengeModeId);
    }

    return result;
}

} // namespace Delves
