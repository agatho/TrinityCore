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
#include <array>
#include <map>
#include <span>
#include <unordered_map>
#include <vector>

class Player;

//
// Prey system + Voidforge — Midnight Season 1 solo-hunt progression.
//
// SCOPE: framework spine plus the hunt content skeleton. Ids are either
// DB2-anchored (wago.tools 12.0.7.68887 CSV) or quest-database keys recovered
// from the 12.1.0.69273 capture and re-verified against our own 12.0.7 rows.
// The hunt *activation wire* is still CAPTURE-BLOCKED (Hunt Table creature
// 245824 uses a mission-table-style opcode flow that no capture we hold
// contains), so StartHunt remains a documented no-op seam; CompleteHunt is
// live, because completion turns out to be plain quest kill credit.
// See C:\dumps\PREY_VOIDFORGE_BLUEPRINT.md for the full evidence inventory.
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

    // ---- Hunt content [SNIFF 12.1.0.69273 quest-info decode] ----------------
    // These are database keys, not wire offsets, which is why they cross the
    // patch boundary. All ten hunt quests and their objectives were compared
    // against our own VerifiedBuild-66384 rows and match field for field; see
    // sql/updates/world/master/2026_08_13_20_world_prey_hunts.sql.

    // The two shared kill-credit bunnies. Every hunt, both tiers, uses the same
    // pair — the named target creature is NOT the credit source.
    //   objective 0, Flags 0          "Hunt your Prey"
    //   objective 1, Flags 2 SEQUENCED "<target> slain" (shown once obj 0 fills)
    constexpr uint32 NPC_CREDIT_HUNT_PROGRESS = 246472; // "Credit: Hunt your Prey"
    constexpr uint32 NPC_CREDIT_TARGET_SLAIN  = 253450; // "Credit: Multiple Credit"

    // quest_template.RewardSpell, identical on all ten hunts.
    constexpr uint32 SPELL_HUNT_REWARD = 1244010;

    // ContentTuningID per tier — this is the value 2026_08_12_00 shipped as a
    // CAPTURE-BLOCKED TODO. Nightmare has no captured quest and stays unknown.
    constexpr uint32 CONTENT_TUNING_HUNT_NORMAL = 5224;
    constexpr uint32 CONTENT_TUNING_HUNT_HARD   = 5223;

    // Rotation pools. Used as the fallback when prey_hunt_template is empty, so
    // the rotation still behaves sanely on a realm that has not applied the SQL.
    inline constexpr std::array<uint32, 4> HUNT_QUESTS_NORMAL = { 91098, 91102, 91109, 91123 };
    inline constexpr std::array<uint32, 6> HUNT_QUESTS_HARD   = { 91210, 91212, 91214, 91242, 91248, 91255 };

    // The world-quest variant (QuestSortID -656 QUEST_SORT_PREY,
    // QuestInfoID 295 QUEST_INFO_PREY_WORLD_QUEST). It is a progress-bar quest
    // handled entirely by the stock quest system — it is NOT part of the hunt
    // rotation and needs no PreyMgr involvement. Listed for provenance only.
    constexpr uint32 QUEST_PREY_WQ_VENOM_AMBUSH = 96591;
}

enum class PreyDifficulty : uint8
{
    Normal    = 0, // unlocked at intro; drops Adventurer, fills Veteran vault slot
    Hard      = 1, // quest "One Hero's Prey"; drops Veteran, fills Champion vault slot
    Nightmare = 2, // Rank 4 Journey + "Dark Mending"; drops Champion, fills Hero + Nebulous Voidcore

    Max
};

// Shipped world table row (prey_hunt_template). Content TODO where CAPTURE-BLOCKED.
//
// Id is the hunt's *quest id* — the hunt has no identity of its own on the wire
// or in the database, it is an ordinary quest, so keying the registry by quest
// id keeps one source of truth. ZoneId 0 means "not zone-scoped": the capture
// carries no zone for any hunt, so every shipped row currently reads 0 and the
// per-zone rotation collapses to a single bucket. That is the honest degenerate
// case, not a bug — fill ZoneId in once a 12.0.7 capture supplies it.
struct PreyHuntTemplate
{
    uint32         Id            = 0;
    uint32         ZoneId        = 0; // Midnight zone the hunt runs in (0 = unscoped)
    PreyDifficulty Difficulty    = PreyDifficulty::Normal;
    uint32         ContentTuningId = 0; // 5224 Normal / 5223 Hard; Nightmare unknown
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

        // ---- Weekly rotation ----------------------------------------------
        // Hunts rotate per difficulty and per zone. The index advances with the
        // world's weekly quest reset so a hunt stays put for a whole week and
        // flips with everything else, rather than on its own clock.
        static uint32 GetCurrentWeekIndex();

        // The hunt (== quest id) advertised this week for a tier in a zone.
        // Returns 0 when the tier has no content — Nightmare, today.
        uint32 GetWeeklyHuntQuest(PreyDifficulty difficulty, uint32 zoneId) const;

        // Every hunt advertised this week, one per registered (tier, zone)
        // bucket. Empty registry falls back to one hunt per populated tier.
        std::vector<uint32> GetWeeklyHuntQuests() const;

        bool IsHuntQuest(uint32 questId) const;
        // PreyDifficulty::Max when questId is not a hunt.
        PreyDifficulty GetHuntDifficulty(uint32 questId) const;

        // ---- Kill credit ---------------------------------------------------
        // Both objectives of every hunt are MONSTER objectives against shared
        // credit bunnies, so the whole server side of "the hunt progressed" and
        // "the target died" is these two calls. They are safe to call for a
        // player who is not on a hunt: KilledMonsterCredit is a no-op then.
        void CreditHuntProgress(Player* player);   // objective 0, 246472
        void CreditHuntTargetSlain(Player* player); // objective 1, 253450

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
        // Static fallback pool for a tier, used when the registry is empty.
        static std::span<uint32 const> GetStaticHuntPool(PreyDifficulty difficulty);
        // Registry-backed pool for a (tier, zone) bucket, or empty if unknown.
        std::vector<uint32> const* GetRegisteredHuntPool(PreyDifficulty difficulty, uint32 zoneId) const;

        std::unordered_map<uint32, PreyHuntTemplate> _huntTemplates;
        // (difficulty, zoneId) -> hunt ids, ascending. Keyed so the rotation is
        // reproducible across restarts and across worldservers.
        std::map<std::pair<uint8, uint32>, std::vector<uint32>> _huntsByBucket;
        uint32 _weekIndex = 0;
        uint32 _rotationCheckTimer = 0;
        bool _enabled = false;
};

#define sPreyMgr PreyMgr::instance()

#endif // TRINITYCORE_PREY_MGR_H
