// MountsByRace - Faction-default racial mount spell ids and riding-skill
// training spells per tier. Phase B7 of WORLD_POPULATION_PLAN.md.
//
// Spell ids are the riding-mount spells (cast = mount up). All bots learn
// the spell directly via Player::LearnSpell — no need to add an item to
// inventory and click it.

#pragma once

#include "Bot/BotTypes.h"

namespace Playerbot::V2::World {

// Riding skill tiers (apprentice -> master). Spell ids from SkillLineAbility.
// Player learns these via LearnSpell, then can mount.
namespace RidingSpell {
    constexpr uint32 Apprentice  = 33388;  // 60% ground (L20)
    constexpr uint32 Journeyman  = 33391;  // 100% ground (L30)
    constexpr uint32 Expert      = 34090;  // 60% flight (L60)
    constexpr uint32 Artisan     = 34091;  // 280% flight (L70)
    constexpr uint32 Master      = 90265;  // 310% flight (L80)
}

// Returns the appropriate riding spell to train at the given level. Returns 0
// for levels under 20.
uint32 RidingSpellForLevel(uint8 level);

// Race -> default ground mount spell id. Picked from racial mount vendors.
uint32 GroundMountSpellForRace(uint32 race);

// Race -> default flying mount spell id. Picked when riding tier >= Expert.
// Returns 0 if the race has no native flying mount (most races); use a
// generic flying mount as fallback (e.g. Drake of the East Wind).
uint32 FlyingMountSpellForRace(uint32 race);

// Generic flying mount spell id used as fallback for races without a
// faction-default flying mount.
constexpr uint32 GenericFlyingMountAlliance = 32289;  // Swift Blue Gryphon
constexpr uint32 GenericFlyingMountHorde    = 32243;  // Swift Red Wind Rider

} // namespace Playerbot::V2::World
