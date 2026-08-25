// ZonesByLevel - Map level brackets to candidate quest zones for the bot
// distribution / setup pipeline (Phase B9 of WORLD_POPULATION_PLAN.md).
//
// After a bot is geared/talented/mounted in the capital, it teleports
// to a level-appropriate zone to begin autonomous quest behavior.
//
// Zones are filtered by faction (some zones are race-locked starting
// areas, others are contested/PvP-shared). The bot picks a random
// candidate matching its current level + faction.

#pragma once

#include "Bot/BotTypes.h"
#include <span>

namespace Playerbot::V2::World {

struct ZoneEntry
{
    uint16 zone_id;     // AreaTable.db2 zone id (used for membership tests)
    uint8  level_lo;
    uint8  level_hi;
    uint32 map_id;
    float  x, y, z;
    bool   alliance;    // true = Alliance can quest here
    bool   horde;       // true = Horde can quest here
    char const* name;   // diagnostics
};

// Returns a random zone entry that matches (level ∈ [level_lo, level_hi])
// AND the faction. Hash-based deterministic pick from `seed` so the same
// bot always lands at the same zone.
ZoneEntry const* PickZoneForLevel(uint8 level, bool alliance, uint64 seed);

// Whole table for diagnostics.
std::span<ZoneEntry const> AllZones();

} // namespace Playerbot::V2::World
