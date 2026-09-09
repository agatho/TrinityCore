#include "HunterPetsByRace.h"
#include "RaceMask.h"

namespace Playerbot::V2::World {

uint32 HunterPetEntryForRace(uint32 race)
{
    // Per-race starter pet matching what each racial intro questline hands
    // a hunter when they reach the relevant quest. Verified against TC's
    // creature_template entries that have stayed stable across patches.
    switch (race)
    {
        // Alliance
        case RACE_HUMAN:                    return 299;    // Stormpike Wolf — Northshire Hunter pet
        case RACE_DWARF:                    return 1132;   // Crag Boar — Coldridge Valley boar
        case RACE_NIGHTELF:                 return 2031;   // Webwood Spider — Shadowglen spider (NE classic pet)
        case RACE_GNOME:                    return 299;    // Gnome hunters are modern-only; default to faction wolf
        case RACE_DRAENEI:                  return 17091;  // Wrath Hawk — Azuremyst Isle bird (Draenei racial pet)
        case RACE_WORGEN:                   return 35040;  // Lost Mastiff Pup — Gilneas mastiff
        case RACE_PANDAREN_ALLIANCE:        return 54269;  // Forest Spiderling — Wandering Isle
        // Allied Alliance races default to faction-generic
        case RACE_VOID_ELF:
        case RACE_LIGHTFORGED_DRAENEI:
        case RACE_DARK_IRON_DWARF:
        case RACE_KUL_TIRAN:
        case RACE_MECHAGNOME:
        case RACE_DRACTHYR_ALLIANCE:
        case RACE_EARTHEN_DWARF_ALLIANCE:   return 0;      // fall through to GenericAllianceHunterPet

        // Horde
        case RACE_ORC:                      return 5826;   // Mottled Boar — Valley of Trials (Orc classic)
        case RACE_UNDEAD_PLAYER:            return 1908;   // Mangy Wolf — Tirisfal Glades (Forsaken Hunter pet)
        case RACE_TAUREN:                   return 2956;   // Adult Plainstrider — Mulgore (iconic Tauren pet)
        case RACE_TROLL:                    return 2952;   // Bloodtalon Taillasher — Echo Isles raptor (Troll classic)
        case RACE_GOBLIN:                   return 5827;   // Mottled Boar variant — Lost Isles fallback
        case RACE_BLOODELF:                 return 16920;  // Springpaw Cub — Eversong Woods cat (BE Hunter classic)
        case RACE_PANDAREN_HORDE:           return 54269;  // Forest Spiderling — Wandering Isle
        // Allied Horde races default to faction-generic
        case RACE_NIGHTBORNE:
        case RACE_HIGHMOUNTAIN_TAUREN:
        case RACE_ZANDALARI_TROLL:
        case RACE_VULPERA:
        case RACE_MAGHAR_ORC:
        case RACE_DRACTHYR_HORDE:
        case RACE_EARTHEN_DWARF_HORDE:      return 0;      // fall through to GenericHordeHunterPet

        default:                            return 0;
    }
}

} // namespace Playerbot::V2::World
