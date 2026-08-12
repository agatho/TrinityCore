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

// ============================================================================
// Delve Nemesis layer (Midnight 12.0.7 "Midnight" Season 1)
// ----------------------------------------------------------------------------
// The Nemesis layer is the higher-tier delve ESCALATION that extends the base
// delve system (feature/delves). It has three additive mechanisms:
//
//   1. Pactsworn packs  - tougher elite packs that appear in EVERY regular
//      delve at Tier 4+ (DELVE_TIER_ENDGAME_START). Killing them upgrades the
//      quality of the end-of-run Nemesis Strongbox. [RESEARCH warcraft.wiki.gg,
//      wowhead, method.gg]
//   2. Nemesis Strongbox - a bonus reward chest granted at the end of a Tier 4+
//      delve run; its loot quality scales with the number of Pactsworn slain in
//      that run.
//   3. Torment's Rise / Nullaeus - the Nemesis boss delve in Voidstorm
//      [DB2 Map 2966 "Torment's Rise", Creature 255108 "Nullaeus"], featuring a
//      SOLO challenge (Tier 8 / Tier 11) that awards the "My Shady Nemesis"
//      (61797), "Lighting the Dark" (61798), "Let Me Solo Him: Nullaeus" (61799)
//      and "Fabled Let Me Solo Him: Nullaeus" (61808) achievements
//      [all DB2 Achievement @68887]. The solo challenge is the Midnight analogue
//      of TWW's Zekvir (precedent Achievement 40433 "Let Me Solo Him: Zekvir").
//
// ----------------------------------------------------------------------------
// BASING DECISION (see C:\dumps\DELVE_NEMESIS_BLUEPRINT.md §c):
// This branch is based off the golden-source BASELINE 560165c0a6 - the same
// baseline every other net-new feature branch uses (void-assaults / prey-
// voidforge / quelthalas-zone-events) - NOT off feature/delves and NOT off any
// integration branch. The delve manager/instance/rewards code lives only on
// feature/delves, which is NOT present on this baseline. Per the golden-source
// rule we do NOT cherry-pick that code here.
//
// So, exactly like feature/void-assaults mirrored ZoneEventMgr, this branch
// ships a SELF-CONTAINED NemesisMgr that compiles green off the baseline. The
// delve-integration SEAM is the set of three public entry points below
// (OnDelveStarted / OnPactswornKilled / OnDelveCompleted); on this branch they
// are driven only by the .delvenemesis GM debug command (cs_delvenemesis.cpp).
// At the integration merge (AFTER feature/delves lands - see blueprint §f) the
// seam is wired to the real delve code:
//     DelveInstance::OnPlayerEnter(...)          -> sNemesisMgr->OnDelveStarted(map, tier)
//     <Pactsworn creature script>::OnUnitKilled  -> sNemesisMgr->OnPactswornKilled(killer)
//     DelvesRewards::AwardDelveCompletion(...)   -> sNemesisMgr->OnDelveCompleted(player, tier, mapId, isSolo)
// The Nemesis layer keys strictly off the INTEGER delve tier (4/8/11), never the
// tier-scaling spell ladder (TIER_SPELL_IDS T2-11, flagged for re-verification in
// the delve backlog) - so that open question does NOT block this hook.
// ============================================================================

#ifndef TRINITY_DELVE_NEMESIS_MGR_H
#define TRINITY_DELVE_NEMESIS_MGR_H

#include "Define.h"
#include <unordered_map>
#include <vector>

class Map;
class Player;
class Creature;

namespace DelveNemesis
{

// ---------------------------------------------------------------------------
// DB2-anchored constants (build 12.0.7.68887; every id verified in wago.tools
// CSV this pass - see blueprint §b evidence inventory). Do NOT edit without a
// fresh DB2 verification.
// ---------------------------------------------------------------------------

// Tier gating - the Nemesis layer is the Tier 4+ escalation. This MIRRORS
// Delves::DELVE_TIER_ENDGAME_START (=4) on feature/delves; kept as a local
// constant so this branch compiles without the delve headers.
static constexpr uint8  NEMESIS_PACTSWORN_MIN_TIER = 4;   // [SRC feature/delves DelvesDefines.h]
static constexpr uint8  NULLAEUS_TIER_LOW          = 8;   // [DB2/RESEARCH] Torment's Rise low tier ("?")
static constexpr uint8  NULLAEUS_TIER_HIGH         = 11;  // [DB2/RESEARCH] Torment's Rise high tier ("??")

// Torment's Rise identity
static constexpr uint32 MAP_TORMENTS_RISE = 2966;   // [DB2 Map] "Torment's Rise" (InstanceType 5, Expansion 11)
static constexpr uint32 MAP_VOIDSTORM     = 2771;   // [DB2 Map] "Voidstorm" (parent zone)
static constexpr uint32 CREATURE_NULLAEUS = 255108; // [DB2 Creature] "Nullaeus" (Classification 1, DisplayID 137340)

// Nemesis achievements [all DB2 Achievement @68887]
static constexpr uint32 ACH_MY_SHADY_NEMESIS       = 61797; // Reward: Nullaeus Domaneye (item 263413)
static constexpr uint32 ACH_LIGHTING_THE_DARK      = 61798; // Reward: Title "the Ominous"
static constexpr uint32 ACH_LET_ME_SOLO_NULLAEUS   = 61799; // Reward: Arcanovoid Construct (mount item 263222)
static constexpr uint32 ACH_FABLED_LET_ME_SOLO     = 61808; // Reward: Title "Fabled Vanquisher of Nullaeus" (first 4000/region)

// Nemesis reward items [all DB2 ItemSparse @68887] - delivered by achievement_reward content, listed for reference
static constexpr uint32 ITEM_NULLAEUS_DOMANEYE     = 263413; // helm transmog
static constexpr uint32 ITEM_DOMINATING_VICTORY    = 264413; // toy
static constexpr uint32 ITEM_ARCANOVOID_CONSTRUCT  = 263222; // mount

// First-kill currency reward: 30 Hero Dawncrest [DB2 CurrencyTypes 3345 "Hero Dawncrest"]
static constexpr uint32 CURRENCY_HERO_DAWNCREST    = 3345;
static constexpr uint32 NULLAEUS_FIRST_KILL_DAWNCRESTS = 30; // [RESEARCH method.gg] first-defeat payout

// ---------------------------------------------------------------------------
// Data structs (shipped tables; both tolerate absent data - realm-safe)
// ---------------------------------------------------------------------------

// nemesis_pactsworn_pack: a Pactsworn elite pack that may spawn in a Tier 4+
// delve. Ships EMPTY - the pack creature entries live in the world DB
// (creature_template; Pactsworn are NOT in Creature.db2, matching the known
// Midnight "elites are world-DB only" pattern) and their in-delve spawn coords
// are CAPTURE-BLOCKED. When the table is empty OnDelveStarted is a no-op.
struct PactswornPack
{
    uint32 Id       = 0;
    uint8  MinTier  = NEMESIS_PACTSWORN_MIN_TIER;
    uint32 MapId    = 0;    // 0 = any delve map
    uint32 Entry    = 0;    // creature_template entry
    uint8  PackSize = 1;
    float  PosX     = 0.0f;
    float  PosY     = 0.0f;
    float  PosZ     = 0.0f;
    float  Orientation = 0.0f;
};

class TC_GAME_API NemesisMgr
{
    NemesisMgr();
    ~NemesisMgr();

public:
    NemesisMgr(NemesisMgr const&) = delete;
    NemesisMgr& operator=(NemesisMgr const&) = delete;

    static NemesisMgr* Instance();

    // Lifecycle (wired in World.cpp, mirroring VoidAssaultMgr / WorldStateMgr)
    void LoadFromDB();
    void Update(uint32 diff);

    // -----------------------------------------------------------------------
    // DELVE-INTEGRATION SEAM (see header banner). On this baseline branch these
    // are driven by the .delvenemesis GM command; at integration they are called
    // from the real delve code on feature/delves.
    // -----------------------------------------------------------------------

    // Called when a delve run starts. Spawns the Pactsworn packs for the map if
    // tier >= NEMESIS_PACTSWORN_MIN_TIER (LIVE spawn spine; no-op while the pack
    // table ships empty).
    void OnDelveStarted(Map* map, uint8 tier);

    // Called when a Pactsworn elite dies. Increments the per-run kill counter
    // (keyed on the instance id) that scales the Nemesis Strongbox quality.
    void OnPactswornKilled(Player* killer);

    // Called at delve completion. Awards the Nemesis Strongbox (quality scaled by
    // Pactsworn kills this run) and, if the run was Torment's Rise (map 2966),
    // resolves the Nullaeus solo-challenge achievements.
    void OnDelveCompleted(Player* player, uint8 tier, uint32 mapId, bool isSolo);

    // Direct Nullaeus resolution (also reachable standalone for testing / from a
    // Nullaeus creature script at integration).
    void OnNullaeusDefeated(Player* player, uint8 tier, bool isSolo);

    // Queries
    uint32 GetPactswornKills(uint32 instanceId) const;
    bool IsTormentsRiseMap(uint32 mapId) const { return mapId == MAP_TORMENTS_RISE; }

private:
    // Strongbox quality (Pactsworn kills -> tier). Placeholder banding; the loot
    // itself is content (config Nemesis.Strongbox.LootId, defaults 0 = inert).
    uint8 ComputeStrongboxQuality(uint32 pactswornKills) const;
    void AwardNemesisStrongbox(Player* player, uint8 tier, uint32 pactswornKills);

    std::vector<PactswornPack> _packs;
    // per-instance Pactsworn kill counters (cleared on completion)
    std::unordered_map<uint32 /*instanceId*/, uint32 /*kills*/> _pactswornKills;
};

} // namespace DelveNemesis

#define sNemesisMgr DelveNemesis::NemesisMgr::Instance()

#endif // TRINITY_DELVE_NEMESIS_MGR_H
