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

#ifndef TRINITY_DELVES_DEFINES_H
#define TRINITY_DELVES_DEFINES_H

#include "Define.h"
#include "ObjectGuid.h"
#include <array>

namespace Delves
{

// ---------------------------------------------------------------------------
// Difficulty & Scenario Type (from DB2 data)
// ---------------------------------------------------------------------------

static constexpr uint32 DELVE_DIFFICULTY_ID = 208;              // DifficultyID for "Delves" (1-5 players)
static constexpr uint8  DELVE_SCENARIO_TYPE = 8;                // ScenarioType for delves (not Solo=2)

// ---------------------------------------------------------------------------
// Tier System
// ---------------------------------------------------------------------------

static constexpr uint8 MAX_DELVE_TIER = 11;
static constexpr uint8 DELVE_TIER_UNLIMITED_REVIVES_MAX = 3;   // Tiers 1-3 have no death limit
static constexpr uint8 DELVE_TIER_ENDGAME_START = 4;           // Tier 4+ requires max level

// Revive limits by tier bracket
static constexpr uint8 DELVE_REVIVES_TIER_4_8   = 5;
static constexpr uint8 DELVE_REVIVES_TIER_9     = 4;
static constexpr uint8 DELVE_REVIVES_TIER_10_11 = 3;
static constexpr uint8 DELVE_REVIVES_UNLIMITED  = 255;

inline uint8 GetMaxRevivesForTier(uint8 tier)
{
    if (tier <= DELVE_TIER_UNLIMITED_REVIVES_MAX)
        return DELVE_REVIVES_UNLIMITED;
    if (tier <= 8)
        return DELVE_REVIVES_TIER_4_8;
    if (tier == 9)
        return DELVE_REVIVES_TIER_9;
    return DELVE_REVIVES_TIER_10_11;
}

// 11 tier-scaling spells, captured from packet sniffs at build 12.0.1.66527 and
// merged in from the DoomCore drop (`origin/master` of agatho/StefalWoW, file
// `src/server/scripts/Custom/DoomcoreCustom/Delves/DelveData.h`).
//
// These spell IDs are **universal across all delves**: when a player picks tier
// N from the gossip menu, the server casts TIER_SPELL_IDS[N-1] on them. The
// spell carries the entry teleport effect plus the per-tier difficulty aura.
// (Also stored in gossip_menu_option.SpellID per-tier, so Player::OnGossipSelect
// fires them automatically; see commit 727dfb710b.)
//
// This is the data layer that previous research (project_tiered_entrance_obfuscation
// memory) flagged as Unknown — DelvesSeasonXSpell.db2 ships empty in the live
// client; the actual spell list arrives via packet sniff. Now captured.
static constexpr uint32 TIER_SPELL_IDS[MAX_DELVE_TIER] =
{
    1260938,  // Tier 1
    1260942,  // Tier 2
    1260946,  // Tier 3
    1260950,  // Tier 4
    1260954,  // Tier 5
    1260957,  // Tier 6
    1260960,  // Tier 7
    1260963,  // Tier 8
    1260967,  // Tier 9
    1260970,  // Tier 10
    1260973,  // Tier 11
};

// Returns the universal tier-scaling spell ID for a 1-based tier (1..11).
// Returns 0 for invalid tiers.
inline uint32 GetTierSpellId(uint8 tier)
{
    if (tier == 0 || tier > MAX_DELVE_TIER)
        return 0;
    return TIER_SPELL_IDS[tier - 1];
}

// ---------------------------------------------------------------------------
// Bountiful Delves
// ---------------------------------------------------------------------------

static constexpr uint8  BOUNTIFUL_DELVES_PER_DAY = 4;
static constexpr uint32 MAX_COFFER_KEY_SHARDS_PER_WEEK = 600;
static constexpr uint32 COFFER_KEY_SHARDS_PER_KEY = 100;

// ---------------------------------------------------------------------------
// Companion
// ---------------------------------------------------------------------------

static constexpr uint32 MAX_COMPANION_LEVEL_S1 = 60;
static constexpr uint32 MAX_COMPANION_LEVEL_S2 = 80;
static constexpr uint32 MAX_COMPANION_LEVEL_S3 = 100;
static constexpr uint32 BOUNTIFUL_DELVES_XP_WEEKLY_CAP = 28;
static constexpr uint8  MAX_COMPANION_GROUP_SIZE = 4;           // Companion joins groups of 1-4, not 5

// ---------------------------------------------------------------------------
// Great Vault
// ---------------------------------------------------------------------------

static constexpr uint32 VAULT_SLOT_1_COMPLETIONS = 2;
static constexpr uint32 VAULT_SLOT_2_COMPLETIONS = 4;
static constexpr uint32 VAULT_SLOT_3_COMPLETIONS = 8;

// ---------------------------------------------------------------------------
// Well-Known IDs (from DelvesConstantsDocumentation.lua + WoWDBDefs)
// ---------------------------------------------------------------------------

static constexpr uint32 CONTENT_TUNING_DELVE_MIN_LEVEL = 2677;
static constexpr uint32 CURRENCY_RESTORED_COFFER_KEY = 3028;
static constexpr uint32 PDE_COMPANION_INFO_SELECTION = 13;
static constexpr uint32 WIDGET_SET_COMPANION_TOOLTIP = 1331;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class CompanionRole : uint8
{
    Dps     = 0,
    Healer  = 1,
    Tank    = 2,

    Max
};

enum class CurioSlotType : uint8
{
    Combat  = 0,
    Utility = 1,

    Max
};

enum class CurioRarity : uint8
{
    Common   = 1,
    Uncommon = 2,
    Rare     = 3,
    Epic     = 4,
};

enum class CompanionConfigSlotType : uint8
{
    Role    = 0,
    Utility = 1,
    Combat  = 2,

    Max
};

// TieredEntranceType from DelvesConstantsDocumentation.lua (TieredEntranceType enum)
enum TieredEntranceType : int32
{
    TIERED_ENTRANCE_TYPE_INVALID  = 0,
    TIERED_ENTRANCE_TYPE_DELVE    = 1,
    TIERED_ENTRANCE_TYPE_SITES    = 2,
    TIERED_ENTRANCE_TYPE_RESERVED = 3,
};

// ---------------------------------------------------------------------------
// Tiered-entrance client CVars (build 12.0.5.67186, IDA dump)
// ---------------------------------------------------------------------------
// Three Per-Character CVars drive the tiered-entrance UI on the client. They
// are *client-local* state (saved to WTF) populated from PlayerDataElement
// values pushed by the server.
//
//   lastSelectedTieredEntranceTier         (cvar_storage @ 0x7FF75FAA6410)
//   highestUnlockedTieredEntranceTier      (cvar_storage @ 0x7FF75FAA64E0)
//   lastLockedTieredEntranceCompanionAbilities (cvar_storage @ 0x7FF75FAA6340)
//
// The mapping (tieredEntranceID → PDE record id) is computed by the Lua
// binding GetTieredEntrancePDEID at IDA `0x7FF75C96A1EC`. That function and
// its siblings (GetTieredEntranceType, GetDelveEntranceMapID,
// IsTieredEntranceScenario, GetTieredEntranceActiveSpells) are protected by
// the WoW anti-analysis pass — control-flow flattening, runtime constant
// decryption (xor-ror chains via `g_Data_414E700`), `secret`/`sub_7FF75BCF90*`
// helpers, junk `rdtsc`/`xchg ch,ch` instructions — so neither IDA's
// decompiler nor manual disasm yields the lookup formula. The function does
// perform a `lower_bound` scan of a sorted `uint32[]` (see disasm at
// 0x7FF75C96A3C0) keyed on a runtime-derived hash of the input arg, so the
// encoding is almost certainly a static map present in the binary's data
// segment, but its key derivation is unrecoverable without symbolic
// execution or a runtime debugger.
//
// Practical consequences for the server:
//   * We cannot statically populate per-tier PDE records during login.
//   * The client falls back gracefully when a PDE record is missing — it
//     reports the tier as "locked" / "not-yet-completed", which is the
//     expected default state until the player actually clears a tier.
//   * When a retail sniff captures SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED
//     for a tier completion, we can populate a hardcoded
//     (mapId, tier) → PDE_record_id table in a future patch.
struct TieredEntranceCVarNames
{
    static constexpr char const* LastSelectedTier              = "lastSelectedTieredEntranceTier";
    static constexpr char const* HighestUnlockedTier           = "highestUnlockedTieredEntranceTier";
    static constexpr char const* LastLockedCompanionAbilities  = "lastLockedTieredEntranceCompanionAbilities";
};

enum class DelveState : uint8
{
    Inactive    = 0,
    Entering    = 1,
    InProgress  = 2,
    BossFight   = 3,
    Completed   = 4,
    Failed      = 5,
};

// ---------------------------------------------------------------------------
// Data Structs
// ---------------------------------------------------------------------------

struct DelveTemplate
{
    uint32 Id = 0;
    uint32 MapId = 0;
    uint32 ScenarioId = 0;
    uint32 MapChallengeModeId = 0;
    uint32 ZoneId = 0;
    uint32 FactionId = 0;
    float CompanionSpawnX = 0.0f;
    float CompanionSpawnY = 0.0f;
    float CompanionSpawnZ = 0.0f;
    float CompanionSpawnO = 0.0f;

    // Gossip-based tier selection (sniff-derived 12.0.1.66527 via DoomCore). The
    // retail client renders the Blizzard_DelvesDifficultyPicker UI when it
    // receives a SMSG_GOSSIP_MESSAGE whose addon row carries a non-zero
    // LfgDungeonsID. The 11 tier options each carry one of TIER_SPELL_IDS[] as
    // their gossip_menu_option.SpellID; clicking a tier triggers
    // CMSG_GOSSIP_SELECT_OPTION, and Player::OnGossipSelect casts the SpellID.
    uint32 GossipMenuId = 0;
    uint32 LfgDungeonsId = 0;
    uint32 BroadcastTextId = 0;
    uint32 FirstTierGossipOptionId = 0;

    // Player teleport positions. Entry = inside-instance landing spot.
    // Exit = overworld position the player is sent to on CMSG_DELVE_TELEPORT_OUT.
    // (CompanionSpawn{X,Y,Z,O} above is independent — that's where Brann appears
    // inside the instance, not where the player lands.)
    float EntryX = 0.0f;
    float EntryY = 0.0f;
    float EntryZ = 0.0f;
    float EntryO = 0.0f;
    float ExitX = 0.0f;
    float ExitY = 0.0f;
    float ExitZ = 0.0f;
    float ExitO = 0.0f;

    // Per-delve scenario IDs. ActiveScenarioId is the in-progress scenario;
    // RewardScenarioId is the completion scenario (often shared across delves —
    // 3424 is reused by both Atal'Aman and Shadow Enclave in DoomCore data).
    uint32 ActiveScenarioId = 0;
    uint32 RewardScenarioId = 0;

    // WorldState 26903 — controls the center spell display in the
    // Blizzard_DelvesDifficultyPicker tier-selection UI. One value per delve
    // (sniff-derived; e.g. 1278258 for Atal'Aman, 1265777 for Shadow Enclave).
    uint32 WorldState26903 = 0;
};

struct DelveTierReward
{
    uint8  Tier = 0;
    uint8  ItemContext = 0;         // Maps to enum ItemContext (Delves_1..Delves_Bonus_10)
    uint8  MaxRevives = 5;
    uint8  CrestType = 0;
    uint8  CrestCount = 0;
};

} // namespace Delves

#endif // TRINITY_DELVES_DEFINES_H
