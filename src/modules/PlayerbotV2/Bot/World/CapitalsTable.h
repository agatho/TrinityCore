// CapitalsTable - Race -> faction capital coordinates for distribution-spawned
// bots and JIT queue-fill bots placed in a city after setup.
//
// Phase B8 of WORLD_POPULATION_PLAN.md. Read-only after process init.

#pragma once

#include "Bot/BotTypes.h"

#include <span>

namespace Playerbot::V2::World {

struct CapitalEntry
{
    uint32 race;     // CharacterEnums.h Races (RACE_HUMAN = 1, etc.)
    uint32 map_id;   // 0 = Eastern Kingdoms, 1 = Kalimdor, 530 = Outland (for Exodar/Silvermoon)
    float  x, y, z;
    float  o;        // facing
    char const* name;
};

// Race index 0 is unused (RACE_NONE in core). Lookup by race id.
CapitalEntry const* CapitalForRace(uint32 race);

// Faction-default fallback when race not in table (new races, Devourer DH, etc).
CapitalEntry const& CapitalForFaction(bool alliance);

// Nearest SAME-MAP capital of the bot's FACTION (faction derived from `race`).
// Returns nullptr when no same-faction capital exists on `map` (a cross-map
// capital run needs flight routing — handled elsewhere). Used by the bag-full
// capital run: "go to bank/AH" should head to the NEAREST faction capital, not
// the race-home one (a dwarf in Stormwind banks in Stormwind, not trek to IF).
CapitalEntry const* NearestCapital(uint32 map, float x, float y, uint32 race);

// Iterate every distinct capital (8 entries). Used by UnifiedTravelGraph
// to seed Capital nodes — these are the well-known anchor points every
// long-haul route benefits from passing through (flight masters,
// portals, banker/AH cluster).
std::span<CapitalEntry const> AllCapitals();

} // namespace Playerbot::V2::World
