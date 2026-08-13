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

#ifndef TRINITYCORE_ABYSS_ANGLERS_MGR_H
#define TRINITYCORE_ABYSS_ANGLERS_MGR_H

#include "Define.h"

class Player;

//
// Abyss Anglers — Midnight 12.0.7 underwater diving activity.
//
// WHAT IT IS (evidence): an organisation of divers. Players speak to Depthdiver
// Jeju to start an "Abyss Anglers dive" in the Zul'Aman Depths: they don a diving
// suit / vehicle, harpoon fish, recover Sunken/Ancient relics, and use the
// "Surface!" ability to return safely before their oxygen runs out. Points earned
// pay out as Angler Pearls, spent with Depthdiver Jeju / Depthdiver Tu'nakit on
// diving-suit upgrades and cosmetics.
//   - Currency 3373 "Angler Pearls" ("...used exclusively in trade with Depthdiver
//                    Jeju and Depthdiver Tu'nakit.")          [DB2 CurrencyTypes @68887]
//   - Currency 3506 "[DNT] Diver Display Currency" (mirrors the player's most recent
//                    Angler Pearls reward for the reward toast) [DB2 CurrencyTypes @68887]
//   - AreaPOI  8584 "Abyss Anglers" ("Speak with Depthdiver Jeju to dive with the
//                    Abyss Anglers.")                          [DB2 AreaPOI @68887]
//   - Spells   1253017 / 1253021 "Abyss Anglers - Vehicle" (the dive vehicle),
//              1260426 "Surface!" (the escape ability),
//              1286703 / 1286820 / 1293613.. "Abyss Angler Fishing" (harpoon cast),
//              1277035/6/7 "[DNT] Abyss Angler - Reward A/B/C", 1293593 scaling reward,
//              1269894 "[DNT] Abyss Angler - Quest Complete"   [DB2 SpellName @68887]
//   - Achievements category 15574..15553: vendor-unlock chain (Finnow Chum, Reinforced
//     Joints, Shallows Net, harpoon turrets...) purchased from Depthdiver Jeju/Tu'nakit.
//   - Quest text [SNIFF 12.1.0.69273]: "Joining the Abyss Anglers" (Talk to Depthdiver
//     Jeju to join an Abyss Anglers dive), "Catch any fish with your harpoon, recover
//     Sunken Artifact, and use the Surface! ability to return safely."
//
// See C:\dumps\MIDNIGHT_SMALL_ACTIVITIES_BLUEPRINT.md for the full inventory.
//
// BUILDABLE SPINE: this manager owns the reward SEAM — the DB2-anchored Angler
// Pearls payout plus the display-currency mirror used by the reward toast. The DIVE
// itself (the underwater vehicle scenario, oxygen depletion, harpoon fishing, fish
// schools, relic collection) is a scripted vehicle scenario that is CAPTURE-BLOCKED:
// no capture we hold contains the dive activation/scoring wire, and Depthdiver Jeju /
// Tu'nakit are world-DB creatures whose ids are not in DB2 @68887 (RESEARCH-BLOCKED).
// StartDive is therefore a documented no-op seam. LoadFromDB tolerates an absent
// table so the shared realm is a no-op.
//

namespace AbyssAnglers
{
    // -------- DB2 anchors (all [DB2] 12.0.7.68887) --------
    constexpr uint32 CURRENCY_ANGLER_PEARLS   = 3373; // "Angler Pearls"
    constexpr uint32 CURRENCY_DIVER_DISPLAY   = 3506; // "[DNT] Diver Display Currency" (reward toast mirror)
    constexpr uint32 AREAPOI_ABYSS_ANGLERS    = 8584; // "Abyss Anglers"

    // Dive vehicle + abilities (provenance; used by the future StartDive handler).
    constexpr uint32 SPELL_DIVE_VEHICLE_A     = 1253017; // "Abyss Anglers - Vehicle"
    constexpr uint32 SPELL_DIVE_VEHICLE_B     = 1253021; // "Abyss Anglers - Vehicle"
    constexpr uint32 SPELL_SURFACE            = 1260426; // "Surface!"
    constexpr uint32 SPELL_REWARD_SCALING     = 1293593; // "[DNT] Abyss Angler - Reward - Scaling"

    // =====================================================================
    // TODO(CAPTURE-BLOCKED) — PROVISIONAL PLACEHOLDER PAYOUT.
    // The payout MECHANISM (ModifyCurrency onto 3373 + mirror onto 3506) is
    // DB2-anchored and LIVE; the points->pearls curve was never captured (no dive
    // scoring/reward packet in any sniff). This constant exists ONLY so the reward
    // toast runs end-to-end on a disposable test DB.
    // =====================================================================
    constexpr uint32 PLACEHOLDER_PEARLS_PER_DIVE = 25; // TODO(CAPTURE-BLOCKED)
}

class TC_GAME_API AbyssAnglersMgr
{
    private:
        AbyssAnglersMgr();
        ~AbyssAnglersMgr();

    public:
        AbyssAnglersMgr(AbyssAnglersMgr const&) = delete;
        AbyssAnglersMgr(AbyssAnglersMgr&&) = delete;
        AbyssAnglersMgr& operator=(AbyssAnglersMgr const&) = delete;
        AbyssAnglersMgr& operator=(AbyssAnglersMgr&&) = delete;

        static AbyssAnglersMgr* instance();

        // World load path. Tolerates an absent table (realm-safe no-op).
        void LoadFromDB();

        // True once abyss_angler_dive_template is present + non-empty. Gates every
        // grant so the shared realm is a hard no-op.
        bool IsEnabled() const { return _enabled; }

        // ---- Dive reward (LIVE — rides stock currency APIs) ----
        // Pays out Angler Pearls (3373) for a completed dive and mirrors the amount
        // into the display currency (3506) that drives the reward toast. `pearls`==0
        // uses the PLACEHOLDER payout. AMOUNTS are PLACEHOLDER (CAPTURE-BLOCKED).
        void AwardDiveReward(Player* player, uint32 pearls = 0);

        // ---- CAPTURE-BLOCKED seam (documented no-op until the wire is captured) ----
        // The Depthdiver Jeju gossip -> dive-vehicle -> underwater scenario activation
        // is a vehicle-scenario flow not present in any capture we hold. This is the
        // future entry point for that handler.
        void StartDive(Player* player);

    private:
        bool _enabled = false;
};

#define sAbyssAnglersMgr AbyssAnglersMgr::instance()

#endif // TRINITYCORE_ABYSS_ANGLERS_MGR_H
