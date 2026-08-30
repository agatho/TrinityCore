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

#ifndef TRINITYCORE_RITUAL_SITE_MGR_H
#define TRINITYCORE_RITUAL_SITE_MGR_H

#include "Define.h"
#include <unordered_map>
#include <vector>

class Player;

//
// Ritual Sites — Midnight 12.0.5/12.0.7 open-world dark-ritual activity.
//
// WHAT IT IS (evidence): a rotating set of world "Ritual Site" points where a
// dangerous ritual is being conducted; players clear the site (defeat enemies,
// destroy dark obelisks, use Lady Darkglen's device) for reputation and renown
// with the Ritual Sites faction, culminating in the "Ritual Breaker" title.
//   - Faction  2792 "Ritual Sites" (+ paragon 2793)   [DB2 Faction @68887]
//   - Currency 3428 "Renown - Ritual Sites"           [DB2 CurrencyTypes @68887]
//   - Title    1291 "Ritual Breaker %s"               [DB2 CharTitles @68887]
//   - AreaPOI  8614 "Ritual Site: Broken Throne", 8615 "Ritual Site: Daggerspine
//              Point" ("A dangerous ritual is being conducted nearby."), plus the
//              "Void Strike" ritual POIs 8708/8723/8727/8766  [DB2 AreaPOI @68887]
//   - Quest text [SNIFF 12.1.0.69273]: "Dark Obelisk Investigation", "Manifested
//     Density" (Complete a Ritual Site), "Thin Their Ranks" (Defeat enemies in a
//     Ritual Site), "Raising Magical Alarms" (Use Lady Darkglen's device).
//
// See C:\dumps\MIDNIGHT_SMALL_ACTIVITIES_BLUEPRINT.md for the full inventory.
//
// BUILDABLE SPINE: this manager owns the reward SEAM — the DB2-anchored currency /
// reputation / title grant that a completed site fires. The ritual encounter itself
// (creatures, dark-obelisk gameobjects, Lady Darkglen's device, the specific quest
// ids) is WORLD-DB content not present in DB2 @68887; those quest ids are
// RESEARCH-BLOCKED (the sniff carries quest *text*, not ids) and are the documented
// no-op edge. LoadFromDB tolerates an absent table so the shared realm is a no-op.
//

namespace RitualSites
{
    // -------- DB2 anchors (all [DB2] 12.0.7.68887) --------
    constexpr uint32 FACTION_RITUAL_SITES          = 2792; // Faction "Ritual Sites"
    constexpr uint32 FACTION_RITUAL_SITES_PARAGON  = 2793; // Faction "Ritual Sites (Paragon)"
    constexpr uint32 CURRENCY_RENOWN_RITUAL_SITES  = 3428; // CurrencyTypes "Renown - Ritual Sites"
    constexpr uint32 TITLE_RITUAL_BREAKER          = 1291; // CharTitles "Ritual Breaker %s"

    // Known site AreaPOIs (the "world altars"). Used only for provenance/logging;
    // the shipped ritual_site_template row carries the AreaPoiId that is live on the
    // realm's world DB so the seam stays data-driven, not hardcoded.
    constexpr uint32 AREAPOI_BROKEN_THRONE     = 8614;
    constexpr uint32 AREAPOI_DAGGERSPINE_POINT = 8615;

    // =====================================================================
    // TODO(CAPTURE-BLOCKED) — PROVISIONAL PLACEHOLDER REWARD AMOUNTS.
    // The grant MECHANISM (ModifyCurrency on 3428, ReputationMgr rep for 2792,
    // SetTitle 1291 at renown cap) is DB2-anchored and LIVE. The exact per-site
    // renown/reputation amounts and the renown level that unlocks the title were
    // never captured (no completion reward packet in any sniff we hold). These
    // constants exist ONLY so the chain runs end-to-end on a disposable test DB.
    // =====================================================================
    constexpr uint32 PLACEHOLDER_RENOWN_PER_SITE = 25;   // TODO(CAPTURE-BLOCKED)
    constexpr int32  PLACEHOLDER_REP_PER_SITE    = 250;  // TODO(CAPTURE-BLOCKED)
    // Renown level at which "Ritual Breaker" is awarded — pure guess.
    constexpr uint32 PLACEHOLDER_TITLE_RENOWN_LEVEL = 20; // TODO(CAPTURE-BLOCKED)
}

// Shipped world table row (ritual_site_template). AreaPoiId + ZoneId are the only
// data that vary per site; the reward seam is uniform. Keyed by AreaPoiId (the
// site's stable world identity).
struct RitualSiteTemplate
{
    uint32 AreaPoiId    = 0; // DB2 AreaPOI id of the site (e.g. 8614/8615)
    uint32 ZoneId       = 0; // Midnight zone the site sits in (0 = unscoped)
    uint32 WorldStateId = 0; // optional worldstate that drives the site's UI banner
};

class TC_GAME_API RitualSiteMgr
{
    private:
        RitualSiteMgr();
        ~RitualSiteMgr();

    public:
        RitualSiteMgr(RitualSiteMgr const&) = delete;
        RitualSiteMgr(RitualSiteMgr&&) = delete;
        RitualSiteMgr& operator=(RitualSiteMgr const&) = delete;
        RitualSiteMgr& operator=(RitualSiteMgr&&) = delete;

        static RitualSiteMgr* instance();

        // World load path. Tolerates an absent table (realm-safe no-op).
        void LoadFromDB();

        RitualSiteTemplate const* GetSite(uint32 areaPoiId) const;

        // True once ritual_site_template is present + non-empty. Gates every grant
        // so the shared realm is a hard no-op.
        bool IsEnabled() const { return _enabled; }

        // ---- Completion grant (LIVE — rides stock currency/faction/title APIs) ----
        // Fires when a player finishes a Ritual Site. Grants renown currency 3428,
        // faction-2792 reputation, and — once renown crosses the (placeholder) cap —
        // the Ritual Breaker title. AMOUNTS are PLACEHOLDER (CAPTURE-BLOCKED).
        void CompleteRitualSite(Player* player, uint32 areaPoiId = 0);

    private:
        void TryAwardTitle(Player* player);

        std::unordered_map<uint32, RitualSiteTemplate> _sites; // by AreaPoiId
        bool _enabled = false;
};

#define sRitualSiteMgr RitualSiteMgr::instance()

#endif // TRINITYCORE_RITUAL_SITE_MGR_H
