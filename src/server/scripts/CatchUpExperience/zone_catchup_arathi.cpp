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

// Catch-Up Experience -- Arathi Highlands (server map 2927).
// PlayerScript on-enter intro-cinematic trigger + AreaTriggerScript entry-
// teleport stub for the map-2927 Catch-Up Experience, modeled on
// ExilesReach/zone_exiles_reach.cpp (player_exiles_reach_ship_crash's
// OnMovieComplete usage and the at_* AreaTriggerScript/AreaTriggerAI stubs)
// and on World/areatrigger_scripts.cpp's AreaTrigger_at_legion_teleporter
// (the plain teleport-on-trigger pattern).
//
// Entry-flow ruling (verified by wire capture, superseding the earlier
// content-branch Task 6 assumption): the canonical launch is the Adventure
// Guide "Catch Up Experience" tile making the PLAYER CAST spell 1260320
// "Teleport to Arathi Highlands" (~10s cast); on completion the player is
// teleported into map 2927 at (-1101.67, -3554.37, 48.92, o=6.26). Quest
// 90882 is NOT offered by the tile -- it is accepted in-instance from Jaina
// (creature 244643) after arrival. The Adventure Guide tile's exact client
// wiring (its AdventureJournal.db2 row) is a client-DB2/Phase-K item; the
// server-side effect that matters here is spell 1260320 landing the player
// at the known map-2927 coords. This file deliberately does NOT author a
// competing Chromie gossip menu -- an earlier task proved that authoring a
// second Chromie root menu regresses the shipped chromie-time feature
// server-wide (creature_template_gossip is a flat, last-match-wins vector).
// Any Chromie-Time launch path is an OPTION owned by the existing
// chromie-time feature, not something this scaffold creates. This file's
// job is on-enter cinematic + teleport plumbing keyed off map 2927 only.
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
    MAP_CATCHUP_ARATHI = 2927,

    // TODO Phase K: resolve the real CinematicSequences.db2 id for the
    // Arathi Catch-Up intro cinematic (fingerprint: 10-line subtitle
    // "The Arathi Highlands, once bitterly contested..." -- see plan §1.5
    // and content branch Task 5's scene/conversation authoring). 0 is a
    // safe no-op; the SendCinematicStart call below is guarded on this id.
    CINEMATIC_ARATHI_INTRO = 0,

    // TODO Phase K: resolve the real PlayerConditionID that marks a player
    // as having already seen the Arathi Catch-Up intro cinematic, so
    // OnMapChanged doesn't replay it on every re-entry to map 2927. 0
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

// The Adventure-Guide "Catch Up Experience" tile makes the player cast this
// spell (~10s cast); on completion the player is teleported into map 2927 at
// TELEPORT_DEST_X/Y/Z/O below. Verified by wire capture -- see file header
// comment above.
static constexpr uint32 SPELL_TELEPORT_TO_ARATHI = 1260320;

// Verified destination for the Catch-Up Experience entry teleport (map
// 2927), confirmed by wire capture of spell 1260320's completion effect.
// Reused here for the entry-areatrigger stub's destination; the areatrigger
// itself is still a Phase-K TODO (see AREATRIGGER_ARATHI_ENTRY above).
static constexpr float TELEPORT_DEST_X = -1101.67f;
static constexpr float TELEPORT_DEST_Y = -3554.37f;
static constexpr float TELEPORT_DEST_Z = 48.92f;
static constexpr float TELEPORT_DEST_O = 6.26f;

// Plays the map-2927 Catch-Up Experience intro cinematic the first time a
// player enters the zone (i.e. the first time they arrive via spell
// 1260320's teleport -- see file header comment above).
class player_catchup_arathi : public PlayerScript
{
public:
    player_catchup_arathi() : PlayerScript("player_catchup_arathi") { }

    void OnMapChanged(Player* player) override
    {
        // Keys on entering map 2927, the real Catch-Up Experience instance
        // map (see file header comment above).
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
