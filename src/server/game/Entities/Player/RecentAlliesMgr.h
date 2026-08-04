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

#ifndef TRINITYCORE_RECENT_ALLIES_MGR_H
#define TRINITYCORE_RECENT_ALLIES_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>

class Player;

// "Recent Allies" — the players you recently grouped with (party/raid), so you can add a note and be reminded who
// they were. This is a lightweight, query-on-demand system (no in-memory session cache): grouping records a
// persistent entry, and the client's request re-reads it. Notes and a "let recent allies see my location" toggle
// are stored per character.
namespace RecentAllies
{
    struct AllyRecord
    {
        ObjectGuid Guid;
        uint64 WowAccount = 0;
        std::string Note;
    };

    // Record that two players were grouped together (writes both directions). No-op for the same player.
    void RecordGrouping(Player const* a, Player const* b);

    // Read a character's recent allies (most-recent first, capped). Synchronous query — used only when the client
    // opens the recent-allies UI.
    std::vector<AllyRecord> GetAllies(ObjectGuid owner);

    // Set/clear a personal note on a recent ally (persisted).
    void SetNote(ObjectGuid owner, ObjectGuid ally, std::string const& note);

    // Per-character "allow recent allies to see my location" toggle (persisted).
    void SetAllowSeeLocation(ObjectGuid owner, bool allow);
    bool GetAllowSeeLocation(ObjectGuid owner);
}

#endif // TRINITYCORE_RECENT_ALLIES_MGR_H
