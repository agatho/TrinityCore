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

#include "ScheduleClock.h"
#include "Common.h"
#include "Util.h"
#include "World.h"

// World.cpp computes the same two boundaries with file-static helpers, but those cannot be reused here:
// this runs during LoadFromDB, long before World::InitQuestResetTimes, so the persisted m_Next*QuestReset
// values are still 0 and sWorld->GetNextDailyQuestsResetTime() would answer with them. These depend only
// on config, which is loaded first, so they are correct at any point in startup.
time_t ScheduleClock::GetNextDailyReset(time_t t)
{
    return GetLocalHourTimestamp(t, sWorld->getIntConfig(CONFIG_DAILY_QUEST_RESET_TIME_HOUR), true);
}

time_t ScheduleClock::GetNextWeeklyReset(time_t t)
{
    t = GetNextDailyReset(t);
    tm time = TimeBreakdown(t);
    int wday = time.tm_wday;
    int target = sWorld->getIntConfig(CONFIG_WEEKLY_QUEST_RESET_TIME_WDAY);
    if (target < wday)
        wday -= 7;

    return t + (DAY * (target - wday));
}

time_t ScheduleClock::GetCycleStart(uint32 duration, time_t now)
{
    if (!duration)
        return now;

    time_t anchor = (duration >= WEEK || (duration % DAY) != 0)
        ? GetNextWeeklyReset(now) - WEEK
        : GetNextDailyReset(now) - DAY;

    // Both helpers return a boundary strictly after `now` and at most one period out, so `anchor` is
    // always <= now. Step whole cycles forward until the window contains `now`; this both handles a
    // duration shorter than its anchor period and stops rounding error accumulating across refreshes.
    while (anchor + time_t(duration) <= now)
        anchor += time_t(duration);

    return anchor;
}
