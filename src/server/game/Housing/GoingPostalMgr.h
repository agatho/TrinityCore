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

#ifndef TRINITYCORE_GOING_POSTAL_MGR_H
#define TRINITYCORE_GOING_POSTAL_MGR_H

// =============================================================================
// "Going Postal" (housing) — timed mail-delivery RACE minigame — NET-NEW SYSTEM
// -----------------------------------------------------------------------------
// A repeatable housing activity: the player talks to Vaeli "<Postal Worker>"
// (creature 233064), picks one of THREE routes for their faction, races a mail
// delivery course against the clock, and — when they beat their stored time —
// records a new personal best and is granted the route's personal-best-record
// currency.
//
// EVIDENCE DISCIPLINE — CONFIRMED @68887 (wago.tools DB2 build 12.0.7.68887)
// vs CAPTURE-BLOCKED. Nothing below invents a DB2 id or a coordinate.
//
//   CONFIRMED @68887:
//     * Creature 233064 "Vaeli" <Postal Worker>          [Creature.db2]
//     * The 6 personal-best-record currencies, mapped 1:1 to (faction, route):
//         3431 "Housing - Going Postal - Personal Best Record - Alliance - Rt1"
//         3432 "…- Alliance - Rt2"      3433 "…- Alliance - Rt3"
//         3434 "…- Horde - Rt1"         3435 "…- Horde - Rt2"    3436 "…- Horde - Rt3"
//       all under CurrencyCategory 251 "Dragon Racing UI (Hidden)" — i.e. this
//       reuses the retail timed-race framework.                    [CurrencyTypes.db2]
//     * SpellName 1285429 / 1287479 "[DNT] Postal Race Complete - Cover" — the
//       race-completion cover spells (DNT placeholders — RESEARCH-ONLY, NOT wired
//       here because they cannot be mapped to a specific route/checkpoint).
//
//   CAPTURE-BLOCKED (documented, never invented):
//     * The route CHECKPOINT COORDS / trigger GO ids — no AreaPOI/Vignette rows
//       carry "Going Postal" routes @68887. Ship the checkpoint table EMPTY.
//     * Vaeli's real world-DB gossip menu ids + her spawn (world-DB, not DB2).
//     * The exact time thresholds / reward tuning per route.
//     * The in-world race-completion wire (the [DNT] cover spell / final trigger).
//
// The MECHANISM is real and complete: gossip start, a per-player race timer,
// an ordered-checkpoint proximity progression, personal-best comparison, the
// currency award, and realm-safe persistence. Only the DATA that requires a
// capture (checkpoint coords, completion opcode) is left as a flagged seam, so
// the real wire can be dropped in without touching the call sites.
//
// REALM-SAFE: everything routes through IsEnabled() and tolerates absent data
// (no going_postal_route rows, no character_going_postal table → the manager
// loads disabled and every seam is a harmless no-op). Never touches the central
// integration realm.
// =============================================================================

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Player;

// DB2-confirmed ids (see the header block above for provenance).
enum GoingPostalData : uint32
{
    NPC_VAELI_POSTAL_WORKER              = 233064, // "Vaeli" <Postal Worker>  [Creature.db2]

    GOING_POSTAL_CURRENCY_CATEGORY       = 251,    // "Dragon Racing UI (Hidden)" [CurrencyCategory.db2]

    // Personal-best-record currencies — Alliance routes 1..3.
    GOING_POSTAL_CURRENCY_ALLIANCE_RT1   = 3431,
    GOING_POSTAL_CURRENCY_ALLIANCE_RT2   = 3432,
    GOING_POSTAL_CURRENCY_ALLIANCE_RT3   = 3433,
    // Personal-best-record currencies — Horde routes 1..3.
    GOING_POSTAL_CURRENCY_HORDE_RT1      = 3434,
    GOING_POSTAL_CURRENCY_HORDE_RT2      = 3435,
    GOING_POSTAL_CURRENCY_HORDE_RT3      = 3436,

    // "[DNT] Postal Race Complete - Cover" spells [SpellName.db2] — RESEARCH-ONLY.
    // Listed for provenance; NOT cast here (route/checkpoint mapping is unknown).
    GOING_POSTAL_SPELL_RACE_COMPLETE_A   = 1285429,
    GOING_POSTAL_SPELL_RACE_COMPLETE_B   = 1287479,
};

// TeamId-aligned faction side for a route (0 Alliance / 1 Horde), matching
// Player::GetTeamId() and the going_postal_route.team column.
enum class GoingPostalTeam : uint8
{
    Alliance = 0,
    Horde    = 1,
};

// One ordered checkpoint of a route. Coords are CAPTURE-BLOCKED (the checkpoint
// table normally ships EMPTY); the struct exists so a future capture can seed it
// and the proximity mechanism starts working with no code change.
struct GoingPostalCheckpoint
{
    uint32   seq = 0;      // 0-based order within the route
    Position pos;          // world position (CAPTURE-BLOCKED until seeded)
    uint32   mapId = 0;    // map the checkpoint lives on (CAPTURE-BLOCKED)
};

// A single race route: (team, index 1..3) → its personal-best currency, plus the
// ordered checkpoint list (usually empty = coords CAPTURE-BLOCKED).
struct GoingPostalRoute
{
    uint32          id = 0;          // going_postal_route.id
    GoingPostalTeam team = GoingPostalTeam::Alliance;
    uint8           routeIndex = 0;  // 1..3
    uint32          currencyId = 0;  // 3431..3436
    bool            enabled = false;
    std::string     name;
    std::vector<GoingPostalCheckpoint> checkpoints;

    bool HasCheckpoints() const { return !checkpoints.empty(); }
};

// Per-player in-flight race state.
struct GoingPostalActiveRace
{
    uint32   routeId = 0;
    uint32   currencyId = 0;
    uint32   startMS = 0;        // getMSTime() at StartRace
    uint32   nextCheckpoint = 0; // index into GoingPostalRoute::checkpoints
    uint32   totalCheckpoints = 0;
};

// Result of a race-completion attempt.
enum class GoingPostalResult : uint8
{
    NotRacing        = 0, // player had no active race
    RecordedBest     = 1, // completed AND beat (or set first) personal best
    Completed        = 2, // completed but did NOT beat the stored best
    Disabled         = 3, // manager disabled / route not found
};

class TC_GAME_API GoingPostalMgr
{
public:
    static GoingPostalMgr& Instance();

    GoingPostalMgr(GoingPostalMgr const&) = delete;
    GoingPostalMgr(GoingPostalMgr&&) = delete;
    GoingPostalMgr& operator=(GoingPostalMgr const&) = delete;
    GoingPostalMgr& operator=(GoingPostalMgr&&) = delete;

    // Tolerant load of going_postal_route (+ optional going_postal_route_checkpoint).
    // Leaves the manager disabled if the table is absent or no route is enabled.
    void Initialize();

    // Gate for the in-world race flow. True once at least one route row is enabled.
    // (The personal-best/currency accounting still works when driven by the debug
    //  driver even if disabled, mirroring the on-branch DecorDuel/Prey idiom.)
    bool IsEnabled() const { return _enabled; }

    // --- Static route lookup (DB2-anchored) --------------------------------

    // Map (team, routeIndex 1..3) → the DB2 personal-best currency id, or 0.
    static uint32 GetRouteCurrencyId(GoingPostalTeam team, uint8 routeIndex);

    // A player's faction side, from Player::GetTeamId().
    static GoingPostalTeam GetPlayerTeam(Player const* player);

    // The routes available to a player's faction (0..3), in route-index order.
    std::vector<GoingPostalRoute const*> GetRoutesForTeam(GoingPostalTeam team) const;

    GoingPostalRoute const* GetRoute(uint32 routeId) const;
    GoingPostalRoute const* GetRoute(GoingPostalTeam team, uint8 routeIndex) const;

    // --- Race lifecycle -----------------------------------------------------

    // Begin a race for (player, route). Starts the timer and resets checkpoint
    // progress. Returns false if the route is unknown or the player is already
    // racing. Safe to call from the Vaeli gossip.
    bool StartRace(Player* player, uint32 routeId);
    bool StartRace(Player* player, GoingPostalTeam team, uint8 routeIndex);

    bool IsRacing(Player const* player) const;
    GoingPostalActiveRace const* GetActiveRace(Player const* player) const;

    // Proximity checkpoint mechanism. Called as the player moves (or by a trigger):
    // if the player is within GOING_POSTAL_CHECKPOINT_RADIUS of the next expected
    // checkpoint, advance; when the last checkpoint is passed, auto-complete.
    // No-op when the active race has no seeded checkpoints (coords CAPTURE-BLOCKED).
    // Returns true if a checkpoint was consumed.
    bool TryAdvanceCheckpoint(Player* player);

    // Force-advance the next checkpoint irrespective of position (debug driver).
    bool ForceAdvanceCheckpoint(Player* player);

    // Finish the active race. elapsedOverrideMS lets the debug driver supply a
    // deterministic time; otherwise the elapsed time is measured from StartRace.
    // Compares against the stored personal best, updates it + grants the currency
    // on improvement, and clears the active race.
    GoingPostalResult CompleteRace(Player* player, std::optional<uint32> elapsedOverrideMS = std::nullopt);

    // Cancel without recording (leaving the course / logout / debug).
    void AbandonRace(Player* player);

    // --- Persistence (realm-safe, tolerant of an absent table) --------------

    // Stored personal-best time (ms) for (player, route), if any.
    std::optional<uint32> GetPersonalBest(ObjectGuid guid, uint32 routeId) const;

    uint32 GetRouteCount() const { return uint32(_routes.size()); }

private:
    GoingPostalMgr() = default;

    void LoadRoutes();
    void LoadCheckpoints();

    // Write a new personal best to character_going_postal (REPLACE INTO) and set
    // the route currency to reflect the record. Tolerant of an absent table.
    void StorePersonalBest(Player* player, GoingPostalRoute const& route, uint32 timeMS);

    bool _enabled = false;
    std::unordered_map<uint32, GoingPostalRoute> _routes;                 // routeId → route
    std::unordered_map<ObjectGuid, GoingPostalActiveRace> _activeRaces;   // player → race
};

#define sGoingPostalMgr GoingPostalMgr::Instance()

#endif // TRINITYCORE_GOING_POSTAL_MGR_H
