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
