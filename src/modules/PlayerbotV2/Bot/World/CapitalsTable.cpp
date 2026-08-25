#include "CapitalsTable.h"
#include "RaceMask.h"
#include "SharedDefines.h"

#include <limits>
#include <iterator>

namespace Playerbot::V2::World {

namespace {

// Coordinates picked at the main entry / inn of each city. Verified against
// retail spawn points; safe-to-teleport (no falling through floors).
constexpr CapitalEntry kStormwind  = { RACE_HUMAN,           0,  -8842.10f,    626.36f,    94.10f,  0.66f, "Stormwind"   };
constexpr CapitalEntry kIronforge  = { RACE_DWARF,           0,  -4980.00f,   -881.00f,   502.00f,  5.40f, "Ironforge"   };
constexpr CapitalEntry kDarnassus  = { RACE_NIGHTELF,        1,   9947.50f,   2482.70f,  1316.20f,  0.00f, "Darnassus"   };
constexpr CapitalEntry kExodar     = { RACE_DRAENEI,       530,  -3987.00f, -11843.00f,    -1.70f,  5.70f, "Exodar"      };
constexpr CapitalEntry kOrgrimmar  = { RACE_ORC,             1,   1633.00f,  -4439.00f,    16.00f,  1.00f, "Orgrimmar"   };
constexpr CapitalEntry kThunderBluff = { RACE_TAUREN,        1,  -1196.00f,     29.00f,   175.00f,  5.60f, "Thunder Bluff" };
constexpr CapitalEntry kUndercity  = { RACE_UNDEAD_PLAYER,   0,   1812.00f,    239.00f,    62.00f,  3.10f, "Undercity"   };
constexpr CapitalEntry kSilvermoon = { RACE_BLOODELF,      530,   9398.00f,  -7278.00f,    14.40f,  0.00f, "Silvermoon"  };

constexpr CapitalEntry kCapitals[] = {
    kStormwind, kIronforge, kDarnassus, kExodar,
    kOrgrimmar, kThunderBluff, kUndercity, kSilvermoon,
};

// Race -> capital association. Pandaren and the new Allied / Midnight races
// fall back to faction default via CapitalForFaction. Hybrid races
// (Worgen, Gnome riding from Ironforge) map to their lore home city.
constexpr struct { uint32 race; CapitalEntry const* cap; } kRaceMap[] = {
    { RACE_HUMAN,                       &kStormwind   },
    { RACE_DWARF,                       &kIronforge   },
    { RACE_GNOME,                       &kIronforge   },  // Gnomes shelter at IF since Cata
    { RACE_NIGHTELF,                    &kDarnassus   },
    { RACE_DRAENEI,                     &kExodar      },
    { RACE_WORGEN,                      &kStormwind   },
    { RACE_PANDAREN_ALLIANCE,           &kStormwind   },
    { RACE_VOID_ELF,                    &kStormwind   },
    { RACE_LIGHTFORGED_DRAENEI,         &kStormwind   },
    { RACE_DARK_IRON_DWARF,             &kStormwind   },
    { RACE_KUL_TIRAN,                   &kStormwind   },
    { RACE_MECHAGNOME,                  &kStormwind   },
    { RACE_DRACTHYR_ALLIANCE,           &kStormwind   },
    { RACE_EARTHEN_DWARF_ALLIANCE,      &kStormwind   },

    { RACE_ORC,                         &kOrgrimmar   },
    { RACE_TROLL,                       &kOrgrimmar   },
    { RACE_TAUREN,                      &kThunderBluff },
    { RACE_UNDEAD_PLAYER,               &kUndercity   },
    { RACE_BLOODELF,                    &kSilvermoon  },
    { RACE_GOBLIN,                      &kOrgrimmar   },
    { RACE_PANDAREN_HORDE,              &kOrgrimmar   },
    { RACE_NIGHTBORNE,                  &kOrgrimmar   },
    { RACE_HIGHMOUNTAIN_TAUREN,         &kThunderBluff },
    { RACE_ZANDALARI_TROLL,             &kOrgrimmar   },
    { RACE_VULPERA,                     &kOrgrimmar   },
    { RACE_MAGHAR_ORC,                  &kOrgrimmar   },
    { RACE_DRACTHYR_HORDE,              &kOrgrimmar   },
    { RACE_EARTHEN_DWARF_HORDE,         &kOrgrimmar   },
};

} // anonymous

CapitalEntry const* CapitalForRace(uint32 race)
{
    for (auto const& m : kRaceMap)
        if (m.race == race) return m.cap;
    return nullptr;
}

CapitalEntry const& CapitalForFaction(bool alliance)
{
    return alliance ? kStormwind : kOrgrimmar;
}

std::span<CapitalEntry const> AllCapitals()
{
    return std::span<CapitalEntry const>(kCapitals);
}

namespace {
bool IsAllianceRace(uint32 race)
{
    switch (race)
    {
        case RACE_HUMAN: case RACE_DWARF: case RACE_GNOME: case RACE_NIGHTELF:
        case RACE_DRAENEI: case RACE_WORGEN: case RACE_PANDAREN_ALLIANCE:
        case RACE_VOID_ELF: case RACE_LIGHTFORGED_DRAENEI: case RACE_DARK_IRON_DWARF:
        case RACE_KUL_TIRAN: case RACE_MECHAGNOME: case RACE_DRACTHYR_ALLIANCE:
        case RACE_EARTHEN_DWARF_ALLIANCE:
            return true;
        default:
            return false;
    }
}
} // anonymous

CapitalEntry const* NearestCapital(uint32 map, float x, float y, uint32 race)
{
    // kCapitals order is Alliance (SW, IF, Darnassus, Exodar) then Horde
    // (Orgrimmar, Thunder Bluff, Undercity, Silvermoon) — first 4 are Alliance.
    const bool alliance = IsAllianceRace(race);
    CapitalEntry const* best = nullptr;
    float best_dsq = std::numeric_limits<float>::max();
    for (size_t i = 0; i < std::size(kCapitals); ++i)
    {
        const bool cap_alliance = (i < 4);
        if (cap_alliance != alliance) continue;
        if (kCapitals[i].map_id != map) continue;   // same-map only (cross-map = flight, elsewhere)
        const float dx = kCapitals[i].x - x, dy = kCapitals[i].y - y;
        const float dsq = dx * dx + dy * dy;
        if (dsq < best_dsq) { best_dsq = dsq; best = &kCapitals[i]; }
    }
    return best;
}

} // namespace Playerbot::V2::World
