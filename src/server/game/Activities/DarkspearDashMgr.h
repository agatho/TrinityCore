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

#ifndef TRINITYCORE_DARKSPEAR_DASH_MGR_H
#define TRINITYCORE_DARKSPEAR_DASH_MGR_H

#include "Define.h"

class Player;

//
// Darkspear Dash — Midnight 12.0.7 Horde troll micro-holiday (RESEARCH-ONLY).
//
// WHAT IT IS (evidence): a timed weekend micro-holiday (added 12.0.7) in which the
// Darkspear trolls and Zandalari race from Echo Isles to Silvermoon City, running,
// leaping, and "spreading rainbow cheer". Rewards a tabard, pets, and the temporary
// "Darkspear Dasher" title + buff.
//   - Title  1207 "Darkspear Dasher %s"           [DB2 CharTitles @68887]
//   - Spells 1280078 / 1280079 "Darkspear Dasher" (the buff),
//            1279458 "Darkspear Dashing",
//            1279936 "Portal to the Darkspear Dash" [DB2 SpellName @68887]
//   - Quests "Doing the Dash", "Darkspear Dash" [WEB warcraft.wiki.gg] — WORLD-DB,
//     quest ids RESEARCH-BLOCKED (no id source; no sniff hit in any capture we hold).
//   - Holiday/game_event window: NO Holidays.db2 row found @68887; the event is a
//     game_event (world-DB) whose id/window is RESEARCH-BLOCKED.
//
// VERDICT: RESEARCH-ONLY. Every mechanic (the race course, checkpoints, timer, the
// two quests, the game_event window, tabard/pet rewards) is WORLD-DB content that is
// NOT present in DB2 @68887 and NOT in any capture. The ONLY DB2-anchored, buildable
// piece is the title/buff grant. This class is therefore a documented SKELETON: it
// carries the confirmed ids and a single live grant helper, and is a hard no-op until
// the quests + game_event are imported. It is NOT wired into the world tick.
//
// See C:\dumps\MIDNIGHT_SMALL_ACTIVITIES_BLUEPRINT.md for the full inventory + the
// capture/research asks needed to promote this to a real spine.
//

namespace DarkspearDash
{
    // -------- DB2 anchors (all [DB2] 12.0.7.68887) --------
    constexpr uint32 TITLE_DARKSPEAR_DASHER = 1207;    // CharTitles "Darkspear Dasher %s"
    constexpr uint32 SPELL_DASHER_BUFF_A    = 1280078; // "Darkspear Dasher"
    constexpr uint32 SPELL_DASHER_BUFF_B    = 1280079; // "Darkspear Dasher"
    constexpr uint32 SPELL_DASHING          = 1279458; // "Darkspear Dashing"
    constexpr uint32 SPELL_PORTAL_TO_DASH   = 1279936; // "Portal to the Darkspear Dash"
}

class TC_GAME_API DarkspearDashMgr
{
    private:
        DarkspearDashMgr();
        ~DarkspearDashMgr();

    public:
        DarkspearDashMgr(DarkspearDashMgr const&) = delete;
        DarkspearDashMgr(DarkspearDashMgr&&) = delete;
        DarkspearDashMgr& operator=(DarkspearDashMgr const&) = delete;
        DarkspearDashMgr& operator=(DarkspearDashMgr&&) = delete;

        static DarkspearDashMgr* instance();

        // The race window is a game_event (world-DB) whose id is RESEARCH-BLOCKED, so
        // there is nothing to load and this is always false until that is known. Kept
        // for API symmetry with the buildable activities.
        bool IsEventActive() const { return false; }

        // ---- Title grant (LIVE — rides stock title API) ----
        // The one DB2-anchored, buildable piece: award "Darkspear Dasher" (1207) on
        // race completion. The COMPLETION TRIGGER (finishing quest "Darkspear Dash")
        // is RESEARCH-BLOCKED, so this is only reachable today via the debug driver.
        void GrantDasherTitle(Player* player);
};

#define sDarkspearDashMgr DarkspearDashMgr::instance()

#endif // TRINITYCORE_DARKSPEAR_DASH_MGR_H
