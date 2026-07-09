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

#include "WorldQuestMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "QuestPackets.h"
#include "Timer.h"

namespace
{
    // How often the expiry sweep runs (ms). World quest timers are hours long, so a coarse tick is fine.
    constexpr uint32 WORLD_QUEST_UPDATE_INTERVAL = 10 * IN_MILLISECONDS;
    // Fallback active duration when a template row specifies 0 (72h, the retail default observed on the wire).
    constexpr uint32 WORLD_QUEST_DEFAULT_DURATION = 3 * DAY;
}

WorldQuestMgr::WorldQuestMgr() = default;
WorldQuestMgr::~WorldQuestMgr() = default;

WorldQuestMgr* WorldQuestMgr::instance()
{
    static WorldQuestMgr instance;
    return &instance;
}

void WorldQuestMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    _templates.clear();
    _active.clear();

    //                                             0        1          2           3
    QueryResult result = WorldDatabase.Query("SELECT QuestID, Duration, VariableID, Value FROM world_quest_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 world quests. DB table `world_quest_template` is empty.");
        return;
    }

    time_t const now = GameTime::GetGameTime();
    do
    {
        Field* fields = result->Fetch();
        uint32 questId = fields[0].GetUInt32();

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
        {
            TC_LOG_ERROR("sql.sql", "Table `world_quest_template` contains reference to non-existing quest {}. Skipped.", questId);
            continue;
        }

        WorldQuestTemplate tmpl;
        tmpl.QuestID = questId;
        tmpl.Duration = fields[1].GetUInt32();
        if (!tmpl.Duration)
            tmpl.Duration = WORLD_QUEST_DEFAULT_DURATION;
        tmpl.VariableID = fields[2].GetInt32();
        tmpl.Value = fields[3].GetInt32();

        _templates[questId] = tmpl;
        Activate(tmpl, now);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} world quests ({} active) in {} ms",
        _templates.size(), _active.size(), GetMSTimeDiffToNow(oldMSTime));
}

void WorldQuestMgr::Activate(WorldQuestTemplate const& tmpl, time_t now)
{
    ActiveWorldQuest& active = _active[tmpl.QuestID];
    active.QuestID = tmpl.QuestID;
    active.StartTime = now;
    active.EndTime = now + tmpl.Duration;
    active.VariableID = tmpl.VariableID;
    active.Value = tmpl.Value;
}

void WorldQuestMgr::Update(uint32 diff)
{
    if (_templates.empty())
        return;

    _updateAccumulator += diff;
    if (_updateAccumulator < WORLD_QUEST_UPDATE_INTERVAL)
        return;
    _updateAccumulator = 0;

    time_t const now = GameTime::GetGameTime();
    for (auto& [questId, active] : _active)
    {
        if (now < active.EndTime)
            continue;

        // Timer expired: refresh the world quest for another cycle (always-available rotation model).
        auto itr = _templates.find(questId);
        if (itr != _templates.end())
            Activate(itr->second, now);
    }
}

void WorldQuestMgr::FillActiveWorldQuests(std::vector<WorldPackets::Quest::WorldQuestUpdateInfo>& updates) const
{
    updates.reserve(updates.size() + _active.size());
    for (auto const& [questId, active] : _active)
    {
        // Timer is the full active duration; the client derives remaining time from LastUpdate + Timer.
        updates.emplace_back(active.StartTime, active.QuestID,
            uint32(active.EndTime - active.StartTime), active.VariableID, active.Value);
    }
}
