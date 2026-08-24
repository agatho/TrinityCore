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

#ifndef TRINITYCORE_SCHEDULE_CLOCK_H
#define TRINITYCORE_SCHEDULE_CLOCK_H

#include "Define.h"
#include <ctime>

// The realm's rotation clock: where the boundaries of a repeating cycle fall.
//
// Anything that rotates on a timer - world quests, area POI blips, and everything reported by
// SMSG_ACTIVE_SCHEDULED_WORLD_STATE_INFO - has to agree on those boundaries, and they have to be a
// property of the wall clock rather than of when the process happened to start. The 12.0.7 captures
// settle this: across two months of SMSG_ACTIVE_SCHEDULED_WORLD_STATE_INFO, every scheduled world state
// keeps `StartTime % Duration` constant and every observed StartTime for a given state differs from the
// others by a whole number of Durations. Retail cycles are phase-locked to the reset clock and step in
// exact periods; they do not drift when a realm restarts.
namespace ScheduleClock
{
    // First daily/weekly reset boundary strictly after `t`, on the realm's configured reset clock.
    TC_GAME_API time_t GetNextDailyReset(time_t t);
    TC_GAME_API time_t GetNextWeeklyReset(time_t t);

    // Start of the `duration`-long cycle that currently contains `now`, phase-locked to the reset clock.
    // The returned value is always <= now, and now < result + duration.
    //
    // Which clock a cycle rides is decided by its length: a whole number of days shorter than a week
    // rides the daily reset; a week or longer, or any length that is not a whole number of days
    // (302400 - half a week - is a real value in the data), rides the weekly reset, so that half- and
    // multi-week cycles stay in phase with reset day.
    TC_GAME_API time_t GetCycleStart(uint32 duration, time_t now);
}

#endif // TRINITYCORE_SCHEDULE_CLOCK_H
