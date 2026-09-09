#include "MountsByRace.h"
#include "RaceMask.h"

namespace Playerbot::V2::World {

uint32 RidingSpellForLevel(uint8 level)
{
    if (level >= 80) return RidingSpell::Master;
    if (level >= 70) return RidingSpell::Artisan;
    if (level >= 60) return RidingSpell::Expert;
    if (level >= 30) return RidingSpell::Journeyman;
    if (level >= 20) return RidingSpell::Apprentice;
    return 0;
}

uint32 GroundMountSpellForRace(uint32 race)
{
    // Faction-default ground mount per race. Spell id corresponds to the
    // mount learning spell sold by the race's mount vendor.
    switch (race)
    {
        // Alliance
        case RACE_HUMAN:                    return 458;    // Brown Horse
        case RACE_DWARF:                    return 6898;   // Brown Ram
        case RACE_NIGHTELF:                 return 10789;  // Spotted Frostsaber
        case RACE_GNOME:                    return 17453;  // Green Mechanostrider
        case RACE_DRAENEI:                  return 35711;  // Brown Elekk
        case RACE_WORGEN:                   return 87840;  // Running Wild (worgen lupine sprint)
        case RACE_PANDAREN_ALLIANCE:        return 87694;  // Black Riding Yak
        case RACE_VOID_ELF:                 return 253101; // Starcursed Voidstrider
        case RACE_LIGHTFORGED_DRAENEI:      return 253108; // Lightforged Felcrusher
        case RACE_DARK_IRON_DWARF:          return 253102; // Dark Iron Core Hound
        case RACE_KUL_TIRAN:                return 295431; // Kul Tiran Charger
        case RACE_MECHAGNOME:               return 312762; // Mechagon Mechanostrider
        case RACE_DRACTHYR_ALLIANCE:        return 369536; // Soar (racial flight)
        case RACE_EARTHEN_DWARF_ALLIANCE:   return 458;    // fall back to Brown Horse

        // Horde
        case RACE_ORC:                      return 580;    // Brown Wolf
        case RACE_TAUREN:                   return 18995;  // Brown Kodo
        case RACE_TROLL:                    return 10796;  // Mottled Red Raptor
        case RACE_UNDEAD_PLAYER:            return 17463;  // Skeletal Horse
        case RACE_BLOODELF:                 return 33660;  // Red Hawkstrider
        case RACE_GOBLIN:                   return 87090;  // Goblin Trike
        case RACE_PANDAREN_HORDE:           return 87695;  // Brown Riding Yak
        case RACE_NIGHTBORNE:               return 253107; // Nightborne Manasaber
        case RACE_HIGHMOUNTAIN_TAUREN:      return 253099; // Highmountain Thunderhoof
        case RACE_ZANDALARI_TROLL:          return 295435; // Zandalari Direhorn
        case RACE_VULPERA:                  return 312764; // Caravan Hyena
        case RACE_MAGHAR_ORC:               return 253098; // Mag'har Direwolf
        case RACE_DRACTHYR_HORDE:           return 369536; // Soar
        case RACE_EARTHEN_DWARF_HORDE:      return 580;    // fall back to Brown Wolf

        default:                            return 458;    // Brown Horse default
    }
}

uint32 FlyingMountSpellForRace(uint32 race)
{
    // Most races don't have a faction-default flying mount; return 0 and
    // caller falls back to GenericFlyingMount{Alliance,Horde}.
    switch (race)
    {
        case RACE_DRACTHYR_ALLIANCE:
        case RACE_DRACTHYR_HORDE:
            return 369536;  // Soar (Dracthyr racial flight, doesn't need riding tier)
        default:
            return 0;
    }
}

} // namespace Playerbot::V2::World
