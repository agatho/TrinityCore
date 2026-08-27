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

#include "InstanceEncounterTimeline.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Timer.h"

EncounterTimelineMgr* EncounterTimelineMgr::instance()
{
    static EncounterTimelineMgr instance;
    return &instance;
}

// There is no EncounterTimeline.db2 - 846 client DB2 meta blocks were checked and none carries "Timeline"
// in its name, so the order of an encounter timeline is entirely server defined and has to live in a world
// table. The only client table involved is EncounterEvent.db2, which supplies spell, icon and severity.
void EncounterTimelineMgr::LoadEncounterTimelines()
{
    uint32 oldMSTime = getMSTime();

    _timelines.clear();

    //                                                0                   1      2                  3        4           5                  6
    QueryResult result = WorldDatabase.Query("SELECT DungeonEncounterID, `Index`, EncounterEventID, Delay, Duration, IsApproximation, Flags "
        "FROM instance_encounter_timeline ORDER BY DungeonEncounterID, `Index`");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 instance encounter timeline entries. DB table `instance_encounter_timeline` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 dungeonEncounterId = fields[0].GetUInt32();
        uint32 index = fields[1].GetUInt32();
        uint32 encounterEventId = fields[2].GetUInt32();

        if (!sDungeonEncounterStore.LookupEntry(dungeonEncounterId))
        {
            TC_LOG_ERROR("sql.sql", "Table `instance_encounter_timeline` has an entry for a non existing DungeonEncounterID {} (Index {}), skipped.",
                dungeonEncounterId, index);
            continue;
        }

        // The client silently discards an event whose EncounterEventID it cannot resolve, so a bad row would
        // be invisible in the log and invisible in the game.
        EncounterEventEntry const* encounterEvent = sEncounterEventStore.LookupEntry(encounterEventId);
        if (!encounterEvent)
        {
            TC_LOG_ERROR("sql.sql", "Table `instance_encounter_timeline` references a non existing EncounterEventID {} "
                "(DungeonEncounterID {}, Index {}), skipped.", encounterEventId, dungeonEncounterId, index);
            continue;
        }

        if (uint32(encounterEvent->DungeonEncounterID) != dungeonEncounterId)
        {
            TC_LOG_ERROR("sql.sql", "Table `instance_encounter_timeline` uses EncounterEventID {} for DungeonEncounterID {} (Index {}), "
                "but that event belongs to DungeonEncounterID {}, skipped.", encounterEventId, dungeonEncounterId, index,
                encounterEvent->DungeonEncounterID);
            continue;
        }

        EncounterTimelineTemplate& timelineTemplate = _timelines[dungeonEncounterId].emplace_back();
        timelineTemplate.EncounterEventID = encounterEventId;
        timelineTemplate.Delay = Milliseconds(fields[3].GetUInt32());
        timelineTemplate.Duration = Milliseconds(fields[4].GetUInt32());
        timelineTemplate.IsApproximation = fields[5].GetBool();
        timelineTemplate.Flags = fields[6].GetUInt32();

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} instance encounter timeline entries in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

std::span<EncounterTimelineTemplate const> EncounterTimelineMgr::GetEncounterTimeline(uint32 dungeonEncounterId) const
{
    auto itr = _timelines.find(dungeonEncounterId);
    return itr != _timelines.end() ? std::span<EncounterTimelineTemplate const>(itr->second) : std::span<EncounterTimelineTemplate const>();
}
