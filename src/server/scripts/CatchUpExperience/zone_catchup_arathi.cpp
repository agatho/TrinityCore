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

// Catch-Up Experience -- Arathi Highlands (server map 2796).
// PlayerScript on-enter intro-cinematic trigger + AreaTriggerScript entry-
// teleport stub for the map-2796 Catch-Up Experience, modeled on
// ExilesReach/zone_exiles_reach.cpp (player_exiles_reach_ship_crash's
// OnMovieComplete usage and the at_* AreaTriggerScript/AreaTriggerAI stubs)
// and on World/areatrigger_scripts.cpp's AreaTrigger_at_legion_teleporter
// (the plain teleport-on-trigger pattern).
//
// Entry-flow ruling (content branch Task 6, carried into this scaffold):
// the canonical launch is the Adventure Guide -> Tutorials tile (an
// AdventureJournal DB2 row with QuestID 90882, served by the existing
// AdventureJournalHandler). This file deliberately does NOT author a
// competing Chromie gossip menu -- an earlier task proved that authoring a
// second Chromie root menu regresses the shipped chromie-time feature
// server-wide (creature_template_gossip is a flat, last-match-wins vector).
// Any Chromie-Time launch path is an OPTION owned by the existing
// chromie-time feature, not something this scaffold creates. This file's
// job is on-enter cinematic + teleport plumbing keyed off quest 90882 /
// map 2796 only.
//
// This CANNOT be compiled in the authoring environment -- compilation and
// realm testing are an explicit Phase-K step (see
// CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN task-7-brief.md).

#include "ScriptMgr.h"
#include "ConditionMgr.h"
#include "DB2Structure.h"
#include "Player.h"

enum CatchUpArathiData
{
    MAP_CATCHUP_ARATHI = 2796,

    // TODO Phase K: resolve the real CinematicSequences.db2 id for the
    // Arathi Catch-Up intro cinematic (fingerprint: 10-line subtitle
    // "The Arathi Highlands, once bitterly contested..." -- see plan §1.5
    // and content branch Task 5's scene/conversation authoring). 0 is a
    // safe no-op; the SendCinematicStart call below is guarded on this id.
    CINEMATIC_ARATHI_INTRO = 0,

    // TODO Phase K: resolve the real PlayerConditionID that marks a player
    // as having already seen the Arathi Catch-Up intro cinematic, so
    // OnMapChanged doesn't replay it on every re-entry to map 2796. 0
    // disables the gate (treated as "not yet seen") until Phase K supplies
    // a real id.
    PLAYERCONDITION_ARATHI_INTRO_SEEN = 0,

    // TODO Phase K: resolve the real entry areatrigger id for the Catch-Up
    // Experience entry teleport. Reference: the Hammerfall arrival cluster
    // authored in content branch Task 2 (20_creature_spawns.sql, PhaseId
    // 15901 "Arrival / Hammerfall warzone"). 0 is a safe no-op; OnTrigger
    // below returns false without teleporting while this is 0.
    AREATRIGGER_ARATHI_ENTRY = 0,
};

// TODO Phase K: resolve the real destination WorldSafeLoc for the entry
// teleport (Hammerfall, map 2796) once the entry areatrigger id above is
// captured/decoded and cross-referenced against Task 2's spawn coordinates.
// These 0.0f placeholders are never dereferenced: OnTrigger returns before
// calling TeleportTo while AREATRIGGER_ARATHI_ENTRY == 0.
static constexpr float TELEPORT_DEST_X = 0.0f;
static constexpr float TELEPORT_DEST_Y = 0.0f;
static constexpr float TELEPORT_DEST_Z = 0.0f;
static constexpr float TELEPORT_DEST_O = 0.0f;

// Plays the map-2796 Catch-Up Experience intro cinematic the first time a
// player enters the zone.
class player_catchup_arathi : public PlayerScript
{
public:
    player_catchup_arathi() : PlayerScript("player_catchup_arathi") { }

    void OnMapChanged(Player* player) override
    {
        if (player->GetMapId() != MAP_CATCHUP_ARATHI)
            return;

        // TODO Phase K: once PLAYERCONDITION_ARATHI_INTRO_SEEN is real, skip
        // replaying the intro for players who have already completed it.
        if (PLAYERCONDITION_ARATHI_INTRO_SEEN && sConditionMgr->IsPlayerMeetingCondition(player, PLAYERCONDITION_ARATHI_INTRO_SEEN))
            return;

        // Guarded: CINEMATIC_ARATHI_INTRO == 0 is a safe no-op until Phase K
        // resolves the real CinematicSequences.db2 id.
        if (CINEMATIC_ARATHI_INTRO)
            player->SendCinematicStart(CINEMATIC_ARATHI_INTRO);
    }
};

// Entry-teleport stub for the Catch-Up Experience. Model:
// World/areatrigger_scripts.cpp's AreaTrigger_at_legion_teleporter.
class at_catchup_arathi_entry : public AreaTriggerScript
{
public:
    at_catchup_arathi_entry() : AreaTriggerScript("at_catchup_arathi_entry") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        // Guarded: AREATRIGGER_ARATHI_ENTRY == 0 is a safe no-op until
        // Phase K resolves the real areatrigger id + destination
        // WorldSafeLoc (see TODOs above).
        if (!AREATRIGGER_ARATHI_ENTRY)
            return false;

        player->TeleportTo(MAP_CATCHUP_ARATHI, TELEPORT_DEST_X, TELEPORT_DEST_Y, TELEPORT_DEST_Z, TELEPORT_DEST_O);
        return true;
    }
};

void AddSC_zone_catchup_arathi()
{
    new player_catchup_arathi();
    new at_catchup_arathi_entry();
}
