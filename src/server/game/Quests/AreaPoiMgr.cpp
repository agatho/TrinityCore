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

#include "AreaPoiMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "QuestPackets.h"
#include "Timer.h"

namespace
{
    constexpr uint32 AREA_POI_UPDATE_INTERVAL = 10 * IN_MILLISECONDS;
    constexpr uint32 AREA_POI_DEFAULT_DURATION = 1 * HOUR;
}

AreaPoiMgr::AreaPoiMgr() = default;
AreaPoiMgr::~AreaPoiMgr() = default;

AreaPoiMgr* AreaPoiMgr::instance()
{
    static AreaPoiMgr instance;
    return &instance;
}

void AreaPoiMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    _templates.clear();
    _active.clear();

    //                                             0          1         2           3
    QueryResult result = WorldDatabase.Query("SELECT AreaPoiID, Duration, VariableID, Value FROM area_poi_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 area POIs. DB table `area_poi_template` is empty.");
        return;
    }

    time_t const now = GameTime::GetGameTime();
    do
    {
        Field* fields = result->Fetch();

        AreaPoiTemplate tmpl;
        tmpl.AreaPoiID = fields[0].GetUInt32();
        tmpl.Duration = fields[1].GetUInt32();
        if (!tmpl.Duration)
            tmpl.Duration = AREA_POI_DEFAULT_DURATION;
        tmpl.VariableID = fields[2].GetInt32();
        tmpl.Value = fields[3].GetInt32();

        _templates[tmpl.AreaPoiID] = tmpl;
        Activate(tmpl, now);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} area POIs ({} active) in {} ms",
        _templates.size(), _active.size(), GetMSTimeDiffToNow(oldMSTime));
}

void AreaPoiMgr::Activate(AreaPoiTemplate const& tmpl, time_t now)
{
    ActiveAreaPoi& active = _active[tmpl.AreaPoiID];
    active.AreaPoiID = tmpl.AreaPoiID;
    active.StartTime = now;
    active.EndTime = now + tmpl.Duration;
    active.VariableID = tmpl.VariableID;
    active.Value = tmpl.Value;
}

void AreaPoiMgr::Update(uint32 diff)
{
    if (_templates.empty())
        return;

    _updateAccumulator += diff;
    if (_updateAccumulator < AREA_POI_UPDATE_INTERVAL)
        return;
    _updateAccumulator = 0;

    time_t const now = GameTime::GetGameTime();
    for (auto& [areaPoiId, active] : _active)
    {
        if (now < active.EndTime)
            continue;

        auto itr = _templates.find(areaPoiId);
        if (itr != _templates.end())
            Activate(itr->second, now);
    }
}

void AreaPoiMgr::FillActiveAreaPois(std::vector<WorldPackets::Quest::AreaPoiUpdateInfo>& pois) const
{
    pois.reserve(pois.size() + _active.size());
    for (auto const& [areaPoiId, active] : _active)
    {
        // Timer is the full active duration; the client derives remaining time from LastUpdate + Timer.
        pois.emplace_back(active.StartTime, active.AreaPoiID,
            uint32(active.EndTime - active.StartTime), active.VariableID, active.Value);
    }
}
