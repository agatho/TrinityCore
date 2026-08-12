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

#ifndef TRINITYCORE_PREY_MGR_H
#define TRINITYCORE_PREY_MGR_H

#include "Define.h"
#include <unordered_map>
#include <vector>

class Player;

//
// Prey system + Voidforge — Midnight Season 1 solo-hunt progression.
//
// SCOPE: this is the framework spine only. Every id below is DB2-anchored
// (wago.tools 12.0.7.68887 CSV). The hunt *activation wire* is CAPTURE-BLOCKED
// (Hunt Table creature 245824 uses a mission-table-style opcode flow that was
// not captured), so StartHunt/CompleteHunt are documented no-op seams. See
// C:\dumps\PREY_VOIDFORGE_BLUEPRINT.md for the full evidence inventory.
//

// -------- DB2 / world anchors (all [DB2] 12.0.7.68887 unless noted) --------
namespace Prey
{
    // Faction table: 2764 "Prey: Season 1" (ParentFactionID 2698, RenownCurrencyID 3386).
    constexpr uint32 FACTION_PREY_SEASON_1        = 2764;

    // CurrencyTypes:
    constexpr uint32 CURRENCY_RENOWN_PREY         = 3386; // "Renown - Prey"  (faction 2764 renown display, Quality 6)
    constexpr uint32 CURRENCY_PREYSEEKERS_JOURNEY = 3387; // "Preyseeker's Journey" (FactionID 2764, AwardConditionID 150246) — the seasonal track
    constexpr uint32 CURRENCY_REMNANT_OF_ANGUISH  = 3392; // "Remnant of Anguish" — Construct V'anore cosmetic sink
    constexpr uint32 CURRENCY_NEBULOUS_VOIDCORE   = 3418; // "Nebulous Voidcore" — Voidforge bonus-roll currency (MaxQtyWorldStateID 30744)

    // Per-difficulty gear-upgrade Dawncrests earned from Prey Hunts (see blueprint §1):
    constexpr uint32 CURRENCY_DAWNCREST_ADVENTURER = 3383; // Normal
    constexpr uint32 CURRENCY_DAWNCREST_VETERAN     = 3341; // Hard
    constexpr uint32 CURRENCY_DAWNCREST_CHAMPION    = 3343; // Nightmare
    constexpr uint32 CURRENCY_DAWNCREST_HERO        = 3345; // Nightmare

    // Voidforge turn-in tracker currencies (worldstate-backed counters):
    constexpr uint32 CURRENCY_VOIDFORGE_UNLOCK_TRACKER  = 3409; // WS 30474
    constexpr uint32 CURRENCY_VOIDFORGE_UPGRADE_TRACKER = 3419; // WS 30731

    // Hunt Table NPC — [SNIFF 68974] SMSG_QUERY_CREATURE_RESPONSE: 245824
    // "Hunt Table", subname "missions". NOT present in Creature.db2@68887 (added
    // 68887->68974). Activation opcode family CAPTURE-BLOCKED.
    constexpr uint32 NPC_HUNT_TABLE = 245824;

    // Preyseeker's Journey: first 4 weekly qualifiers award 1000 pts, later hunts 50 pts.
    // Rank 4 gates Nightmare difficulty. [RESEARCH wowcarry] — cadence not DB2-confirmed.
    constexpr uint32 JOURNEY_POINTS_WEEKLY_BONUS = 1000;
    constexpr uint32 JOURNEY_POINTS_PER_HUNT     = 50;
    constexpr uint32 JOURNEY_RANK_NIGHTMARE_GATE = 4;
}

enum class PreyDifficulty : uint8
{
    Normal    = 0, // unlocked at intro; drops Adventurer, fills Veteran vault slot
    Hard      = 1, // quest "One Hero's Prey"; drops Veteran, fills Champion vault slot
    Nightmare = 2, // Rank 4 Journey + "Dark Mending"; drops Champion, fills Hero + Nebulous Voidcore

    Max
};

// Shipped world table row (prey_hunt_template). Content TODO where CAPTURE-BLOCKED.
struct PreyHuntTemplate
{
    uint32         Id            = 0;
    uint32         ZoneId        = 0; // Midnight zone the hunt runs in
    PreyDifficulty Difficulty    = PreyDifficulty::Normal;
    uint32         ContentTuningId = 0; // scaling (CAPTURE-BLOCKED — not yet identified)
    uint32         VaultActivityId = 0; // which Great Vault row this hunt credits
};

class TC_GAME_API PreyMgr
{
    private:
        PreyMgr();
        ~PreyMgr();

    public:
        PreyMgr(PreyMgr const&) = delete;
        PreyMgr(PreyMgr&&) = delete;
        PreyMgr& operator=(PreyMgr const&) = delete;
        PreyMgr& operator=(PreyMgr&&) = delete;

        static PreyMgr* instance();

        // World load path (World::SetInitialWorldSettings / World::Update).
        // LoadFromDB tolerates absent tables (realm-safe no-op).
        void LoadFromDB();
        void Update(uint32 diff);

        // Player load path (Player::LoadFromDB).
        void OnPlayerLogin(Player* player);

        PreyHuntTemplate const* GetHuntTemplate(uint32 huntId) const;

        // ---- Progression seams (LIVE-safe helpers over stock currency/faction) ----
        // Grant Preyseeker's Journey progress (currency 3387 + renown display 3386).
        void GrantJourneyProgress(Player* player, uint32 points);

        // ---- CAPTURE-BLOCKED seams (documented no-ops until the wire is captured) ----
        // The Hunt Table open/activate flow is a mission-table opcode set not yet
        // captured; StartHunt is the future entry point for that handler.
        void StartHunt(Player* player, uint32 huntId, PreyDifficulty difficulty);
        // On hunt completion: grant Dawncrest/Journey, credit the Great Vault row,
        // and (Nightmare) award Nebulous Voidcore. Vault credit rides the fork's
        // weekly-reward framework — wired in a later phase.
        void CompleteHunt(Player* player, uint32 huntId, PreyDifficulty difficulty);

    private:
        std::unordered_map<uint32, PreyHuntTemplate> _huntTemplates;
        bool _enabled = false;
};

#define sPreyMgr PreyMgr::instance()

#endif // TRINITYCORE_PREY_MGR_H
