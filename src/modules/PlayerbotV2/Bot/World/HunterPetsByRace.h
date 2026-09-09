// HunterPetsByRace - Race -> default Hunter starter pet (creature template entry).
//
// Distribution-leveled Hunter bots skip their racial intro questline, which is
// where retail Hunters get their first pet (Northshire Vineyards "Hunter
// Training" / Coldridge Valley / Shadowglen / etc. all hand the player a
// race-themed pet). Without that quest, the bot has no pet — Call Pet (883)
// fires every tick into nothing.
//
// This table maps each Hunter-eligible race to the creature template a real
// player would tame as their first pet on that race's starter zone. The
// pipeline summons one of these for every distribution-leveled Hunter bot,
// scaled to the bot's current level.
//
// Creature entries are stable, well-known low-level beasts in TC's
// creature_template; renames are rare across patches.

#pragma once

#include "Bot/BotTypes.h"

namespace Playerbot::V2::World {

// Returns the creature template entry to use as a starter Hunter pet for
// `race`, or 0 if no race-specific default exists. The caller falls back to
// a faction-generic pet (Stormpike Wolf for Alliance, Mottled Boar for Horde)
// when this returns 0.
uint32 HunterPetEntryForRace(uint32 race);

// Faction-generic fallback pets, used when HunterPetEntryForRace returns 0
// (allied races / unmapped races). Standard starter beasts that exist in
// every TC creature_template.
constexpr uint32 GenericAllianceHunterPet = 299;   // Stormpike Wolf (Dun Morogh wolf, lvl 1-2)
constexpr uint32 GenericHordeHunterPet    = 5826;  // Mottled Boar (Valley of Trials, lvl 1-2)

} // namespace Playerbot::V2::World
