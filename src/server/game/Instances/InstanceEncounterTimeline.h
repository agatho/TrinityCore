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

#ifndef TRINITYCORE_INSTANCE_ENCOUNTER_TIMELINE_H
#define TRINITYCORE_INSTANCE_ENCOUNTER_TIMELINE_H

#include "Define.h"
#include "Duration.h"
#include "ObjectGuid.h"
#include <span>
#include <unordered_map>
#include <vector>

// One planned entry of an encounter timeline, as loaded from the world table `instance_encounter_timeline`.
// Everything the wire needs beyond these four numbers (spell, icon, severity, icon mask) comes from
// EncounterEvent.db2 and is not duplicated here.
struct EncounterTimelineTemplate
{
    uint32 EncounterEventID = 0;
    Milliseconds Delay = Milliseconds::zero();       // offset from the start of the encounter
    Milliseconds Duration = Milliseconds::zero();    // how long the cast or effect itself takes
    bool IsApproximation = false;
    uint32 Flags = 0;                                // bit 0 makes the client skip the entry
};

// One entry of the timeline the instance is currently showing.
struct EncounterTimelineEventState
{
    uint32 EventInstanceID = 0;                      // server assigned, never 0 (ENCOUNTER_TIMELINE_INVALID_EVENT)
    uint32 EncounterEventID = 0;                     // EncounterEvent.ID
    uint32 DungeonEncounterID = 0;                   // the encounter this entry was queued for - the unit of clearing
    ObjectGuid CasterGUID;
    Milliseconds Delay = Milliseconds::zero();
    Milliseconds Duration = Milliseconds::zero();
    bool IsApproximation = false;
    uint32 Flags = 0;
    bool Paused = false;
    bool IsBlockedByCondition = false;
    TimePoint QueuedAt = TimePoint::min();           // server time the entry was queued, used to compute the remaining delay
};

class TC_GAME_API EncounterTimelineMgr
{
public:
    static EncounterTimelineMgr* instance();

    void LoadEncounterTimelines();

    std::span<EncounterTimelineTemplate const> GetEncounterTimeline(uint32 dungeonEncounterId) const;

private:
    std::unordered_map<uint32 /*dungeonEncounterId*/, std::vector<EncounterTimelineTemplate>> _timelines;
};

#define sEncounterTimelineMgr EncounterTimelineMgr::instance()

#endif // TRINITYCORE_INSTANCE_ENCOUNTER_TIMELINE_H
