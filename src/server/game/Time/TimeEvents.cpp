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

#include "TimeEvents.h"
#include "GameTime.h"
#include <algorithm>
#include <array>

namespace
{
struct TimeEventInfo
{
    int32 Id;
    time_t Timestamp;   // 0 = we know the event exists but not when it happens
    char const* Name;
};

// Sorted by date. Undated entries are listed next to the event they belong to for readability,
// they never take part in any ordering (GetTimestamp returns nothing for them).
constexpr std::array<TimeEventInfo, 8> KnownTimeEvents =
{ {
    { 111, time_t(1579618800), "Battle for Azeroth Season 4 Start" },   // January 21, 2020 8:00
    { 120, time_t(1602601200), "Patch 9.0.1"                       },   // October 13, 2020 8:00
    { 121, time_t(1607439600), "Shadowlands Season 1 Start"        },   // December 8, 2020 8:00
    { 123, time_t(0),          "Shadowlands Season 1 End"          },
    { 149, time_t(0),          "Shadowlands Season 2 End"          },
    { 349, time_t(1699340400), "Dragonflight Season 3 Start (pre-season)" },  // November 7, 2023 8:00
    { 350, time_t(1699945200), "Dragonflight Season 3 Start"       },   // November 14, 2023 8:00
    { 352, time_t(0),          "Dragonflight Season 3 End"         },
} };

TimeEventInfo const* FindTimeEvent(int32 timeEventId)
{
    auto itr = std::ranges::find(KnownTimeEvents, timeEventId, &TimeEventInfo::Id);
    return itr != KnownTimeEvents.end() ? &*itr : nullptr;
}
}

Optional<time_t> TimeEvents::GetTimestamp(int32 timeEventId)
{
    if (TimeEventInfo const* timeEvent = FindTimeEvent(timeEventId))
        if (timeEvent->Timestamp)
            return timeEvent->Timestamp;

    return {};
}

bool TimeEvents::HasPassed(int32 timeEventId)
{
    Optional<time_t> timestamp = GetTimestamp(timeEventId);
    return !timestamp || GameTime::GetGameTime() >= *timestamp;
}

std::vector<int32> TimeEvents::GetPassedEventsOldestFirst()
{
    time_t const now = GameTime::GetGameTime();

    std::vector<int32> passed;
    passed.reserve(KnownTimeEvents.size());
    for (TimeEventInfo const& timeEvent : KnownTimeEvents)
        if (timeEvent.Timestamp && now >= timeEvent.Timestamp)
            passed.push_back(timeEvent.Id);

    // KnownTimeEvents is maintained in chronological order, no extra sort needed
    return passed;
}
