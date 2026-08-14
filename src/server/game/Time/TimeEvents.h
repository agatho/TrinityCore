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

#ifndef TRINITY_TIMEEVENTS_H
#define TRINITY_TIMEEVENTS_H

#include "Define.h"
#include "Optional.h"
#include <ctime>
#include <vector>

// Blizzard keeps the TimeEvent table serverside - it is not shipped in any client db2 (see the
// comment on ChallengeModeItemBonusOverrideEntry::RequiredTimeEventPassed). Everything we know
// about it is the handful of ids below, recovered from observed content unlocks.
//
// This is the single source of truth for that knowledge. Consumers:
//  - ModifierTreeType::HasTimeEventPassed (CriteriaHandler)
//  - ChallengeModeItemBonusOverride season selection (ItemBonusMgr)
namespace TimeEvents
{
    // Timestamp at which the given time event happens/happened.
    // Returns an empty optional when we do not know the event at all, or know of it but have no
    // date for it. Callers must not assume anything about the relative order of such an event.
    TC_GAME_API Optional<time_t> GetTimestamp(int32 timeEventId);

    // Whether the event has already happened.
    // Events with no known timestamp are reported as passed so that an unknown gate never blocks
    // content - this matches the long standing HasTimeEventPassed criteria behaviour.
    TC_GAME_API bool HasPassed(int32 timeEventId);

    // All events we have a date for that have already happened, oldest first.
    // Only usable for ordering when every event of interest is known - use GetTimestamp() to
    // verify that first, otherwise a newer but unknown event would silently sort before an older
    // known one.
    TC_GAME_API std::vector<int32> GetPassedEventsOldestFirst();
}

#endif // TRINITY_TIMEEVENTS_H
