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

    // =====================================================================
    // TODO(CAPTURE-BLOCKED) — PROVISIONAL PLACEHOLDER REWARD AMOUNTS.
    // The grant MECHANISM below (currency 3387, ReputationMgr rep for 2764,
    // Dawncrest / Nebulous Voidcore currency writes) is DB2-anchored and LIVE.
    // The exact per-difficulty AMOUNTS and the Journey point/reputation CADENCE
    // are NOT DB2-confirmed — no hunt-completion reward packet was ever captured
    // (blueprint §2/§7 capture ask #2 & #5). These constants exist ONLY so the
    // chain compiles and runs end-to-end on a disposable test DB. DO NOT treat
    // them as correct — they must be replaced once a completion capture lands.
    // =====================================================================

    // Per-difficulty Preyseeker's Journey (currency 3387) award per completed hunt.
    constexpr uint32 PLACEHOLDER_JOURNEY_POINTS_NORMAL    = 50;   // TODO(CAPTURE-BLOCKED)
    constexpr uint32 PLACEHOLDER_JOURNEY_POINTS_HARD      = 75;   // TODO(CAPTURE-BLOCKED)
    constexpr uint32 PLACEHOLDER_JOURNEY_POINTS_NIGHTMARE = 100;  // TODO(CAPTURE-BLOCKED)

    // Per-difficulty faction-2764 reputation granted per hunt. This is what moves
    // the 3386 renown *level* (ReputationMgr crosses per-level thresholds and bumps
    // 3386 via CurrencyGainSource::RenownRepGain). Amount is a pure guess.
    constexpr int32  PLACEHOLDER_RENOWN_REP_NORMAL    = 250;  // TODO(CAPTURE-BLOCKED)
    constexpr int32  PLACEHOLDER_RENOWN_REP_HARD      = 375;  // TODO(CAPTURE-BLOCKED)
    constexpr int32  PLACEHOLDER_RENOWN_REP_NIGHTMARE = 500;  // TODO(CAPTURE-BLOCKED)

    // Per-difficulty direct Dawncrest / Voidcore currency counts per hunt.
    constexpr int32  PLACEHOLDER_DAWNCREST_COUNT_NORMAL    = 1;  // TODO(CAPTURE-BLOCKED)
    constexpr int32  PLACEHOLDER_DAWNCREST_COUNT_HARD      = 1;  // TODO(CAPTURE-BLOCKED)
    constexpr int32  PLACEHOLDER_DAWNCREST_COUNT_NIGHTMARE = 1;  // TODO(CAPTURE-BLOCKED) each of Champion+Hero
    constexpr int32  PLACEHOLDER_NEBULOUS_VOIDCORE_COUNT   = 1;  // TODO(CAPTURE-BLOCKED) Nightmare only

    // character_prey_hunt.Status values.
    constexpr uint8  HUNT_STATUS_AVAILABLE = 0;
    constexpr uint8  HUNT_STATUS_ACTIVE    = 1;
    constexpr uint8  HUNT_STATUS_COMPLETED = 2;
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

        // True once prey_hunt_template is present + non-empty. Gates every economy
        // grant and every character_prey_hunt query so the shared realm (which never
        // has the table) is a hard no-op.
        bool IsEnabled() const { return _enabled; }

        // ---- Progression grants (LIVE — ride stock currency/faction APIs) ----
        // Award Preyseeker's Journey progress for a completed hunt of this difficulty:
        //   currency 3387 (per-difficulty points) + faction-2764 reputation via
        //   ReputationMgr (which drives the 3386 renown display currency by level).
        // Amounts are PLACEHOLDER — see Prey::PLACEHOLDER_* / TODO(CAPTURE-BLOCKED).
        void GrantJourneyProgress(Player* player, PreyDifficulty difficulty);

        // Grant the per-difficulty direct rewards for a completed hunt:
        //   Dawncrest currency (Adventurer/Veteran/Champion+Hero) and, on Nightmare,
        //   Nebulous Voidcore 3418. Then record the weekly hunt state. Amounts are
        //   PLACEHOLDER — see TODO(CAPTURE-BLOCKED). Great Vault credit is a documented
        //   no-op here (needs WeeklyRewardsMgr from feature/mythic-plus — blueprint §5).
        void CompleteHunt(Player* player, PreyDifficulty difficulty);

        // ---- CAPTURE-BLOCKED seam (documented no-op until the wire is captured) ----
        // The Hunt Table open/activate flow is a mission-table opcode set not yet
        // captured; StartHunt is the future entry point for that handler. The temporary
        // .prey grant debug command stands in for it (calls CompleteHunt directly).
        void StartHunt(Player* player, uint32 huntId, PreyDifficulty difficulty);

    private:
        // Persist a completed hunt into character_prey_hunt (gated on IsEnabled()).
        // Tolerant of an absent table (async Execute logs, never crashes).
        void RecordHuntCompletion(Player* player, PreyDifficulty difficulty);

        std::unordered_map<uint32, PreyHuntTemplate> _huntTemplates;
        bool _enabled = false;
};

#define sPreyMgr PreyMgr::instance()

#endif // TRINITYCORE_PREY_MGR_H
