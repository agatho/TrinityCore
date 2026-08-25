#include "BotComposition.h"

#include "BotNamePool.h"
#include "CharacterCache.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "RaceMask.h"
#include "SharedDefines.h"

#include <algorithm>
#include <array>
#include <fmt/format.h>
#include <random>
#include <sstream>
#include <vector>

namespace Playerbot::V2 {

namespace {

// Local RNG. Each call seeds from random_device — fine for a low-frequency
// command-driven path. Not deterministic; not security-relevant.
std::mt19937& Rng()
{
    static thread_local std::mt19937 rng{std::random_device{}()};
    return rng;
}

// ---- Faction / race tables ------------------------------------------------

enum class Faction : uint8 { Alliance, Horde };

struct RaceWeight { uint8 race; uint16 weight; };

// Numbers loosely from public WoW census (warcraftrealms / wowprogress /
// similar) — orders of magnitude over years of patches. Within ~5pp accuracy.
// Reweight freely; the picker is only as good as its weights.
//
// Allied races (Legion+/BfA+) carry lower weights than core races since
// census shows lighter adoption. Earthen (TWW) gets a small boost as a
// recent addition. Haranir (Midnight) is referenced by literal id 86 since
// TC's RACE_HARRONIR enum entry is currently commented out (RaceMask.h:79);
// the PickClassForRace -> sObjectMgr->GetPlayerInfo check filters it out
// until TC enables it, at which point it lights up automatically.
constexpr std::array<RaceWeight, 14> kAllianceRaces = { {
    { RACE_HUMAN,                18 },
    { RACE_NIGHTELF,             12 },
    { RACE_DRAENEI,               8 },
    { RACE_DWARF,                 6 },
    { RACE_GNOME,                 4 },
    { RACE_WORGEN,                4 },
    { RACE_PANDAREN_ALLIANCE,     2 },
    { RACE_VOID_ELF,              2 },
    { RACE_LIGHTFORGED_DRAENEI,   2 },
    { RACE_KUL_TIRAN,             2 },
    { RACE_DARK_IRON_DWARF,       2 },
    { RACE_MECHAGNOME,            1 },
    { RACE_EARTHEN_DWARF_ALLIANCE,2 },
    { 86,                         1 },   // Haranir (Midnight) — currently disabled in TC
} };

constexpr std::array<RaceWeight, 14> kHordeRaces = { {
    { RACE_BLOODELF,             16 },
    { RACE_ORC,                  10 },
    { RACE_UNDEAD_PLAYER,        10 },
    { RACE_TAUREN,                8 },
    { RACE_TROLL,                 6 },
    { RACE_GOBLIN,                4 },
    { RACE_PANDAREN_HORDE,        2 },
    { RACE_DRACTHYR_HORDE,        3 },
    { RACE_NIGHTBORNE,            2 },
    { RACE_HIGHMOUNTAIN_TAUREN,   2 },
    { RACE_ZANDALARI_TROLL,       2 },
    { RACE_VULPERA,               2 },
    { RACE_MAGHAR_ORC,            2 },
    { RACE_EARTHEN_DWARF_HORDE,   2 },
} };

// Dracthyr Alliance gets the same weight on the Alliance side. We bolt it on
// outside the constexpr array (its enum value 52 is far away from the rest
// of the Alliance race ids; keeping it separate avoids a stray-init landmine).
constexpr uint16 kDracthyrAllianceWeight = 3;

// ---- Class weights --------------------------------------------------------

struct ClassWeight { uint8 cls; uint16 weight; };

constexpr std::array<ClassWeight, 13> kClassWeights = { {
    { CLASS_HUNTER,        12 },
    { CLASS_DEATH_KNIGHT,  11 },
    { CLASS_MAGE,          10 },
    { CLASS_WARRIOR,        9 },
    { CLASS_DRUID,          9 },
    { CLASS_PALADIN,        9 },
    { CLASS_PRIEST,         8 },
    { CLASS_SHAMAN,         7 },
    { CLASS_WARLOCK,        7 },
    { CLASS_ROGUE,          7 },
    { CLASS_MONK,           6 },
    { CLASS_DEMON_HUNTER,   4 },
    { CLASS_EVOKER,         1 },     // Dracthyr only — gets renormalized hard
} };

uint8 PickFromRaceWeights(std::vector<RaceWeight> const& v)
{
    uint32 total = 0;
    for (RaceWeight const& rw : v) total += rw.weight;
    if (total == 0) return 0;
    std::uniform_int_distribution<uint32> dist(0, total - 1);
    uint32 roll = dist(Rng());
    for (RaceWeight const& rw : v)
    {
        if (roll < rw.weight) return rw.race;
        roll -= rw.weight;
    }
    return 0;
}

uint8 PickFromClassWeights(std::vector<ClassWeight> const& v)
{
    uint32 total = 0;
    for (ClassWeight const& cw : v) total += cw.weight;
    if (total == 0) return 0;
    std::uniform_int_distribution<uint32> dist(0, total - 1);
    uint32 roll = dist(Rng());
    for (ClassWeight const& cw : v)
    {
        if (roll < cw.weight) return cw.cls;
        roll -= cw.weight;
    }
    return 0;
}

// Faction-bias for race weights. Some races (Dracthyr) split — picked here
// after the faction is chosen.
uint8 PickRace(Faction f)
{
    if (f == Faction::Alliance)
    {
        // Build Alliance set + Dracthyr_Alliance bolt-on.
        std::vector<RaceWeight> v(kAllianceRaces.begin(), kAllianceRaces.end());
        v.push_back({ RACE_DRACTHYR_ALLIANCE, kDracthyrAllianceWeight });
        return PickFromRaceWeights(v);
    }
    std::vector<RaceWeight> v(kHordeRaces.begin(), kHordeRaces.end());
    return PickFromRaceWeights(v);
}

uint8 PickClassForRace(uint8 race)
{
    // Filter the class table to entries that have a valid PlayerInfo for
    // this race (sObjectMgr->GetPlayerInfo == authoritative). Then pick
    // weighted from the legal subset.
    std::vector<ClassWeight> legal;
    legal.reserve(kClassWeights.size());
    for (ClassWeight cw : kClassWeights)
    {
        if (sObjectMgr->GetPlayerInfo(race, cw.cls))
            legal.push_back(cw);
    }
    if (legal.empty()) return 0;
    return PickFromClassWeights(legal);
}

uint8 PickGender()
{
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(Rng()) == 0 ? GENDER_MALE : GENDER_FEMALE;
}

// ---- Name generator -------------------------------------------------------

constexpr std::array<char const*, 28> kPrefix = {
    "Ar","Bal","Cor","Dar","Em","Fal","Gor","Hal","Ir","Jor",
    "Kel","Lor","Mar","Nor","Or","Pyr","Quor","Ral","Sar","Tor",
    "Ul","Var","Wyl","Xan","Yor","Zor","Tha","Bran"
};

constexpr std::array<char const*, 12> kMiddle = {
    "a","e","i","o","u","ash","eth","ith","oth","an","en","in"
};

constexpr std::array<char const*, 18> kSuffix = {
    "ar","en","ir","on","us","an","el","in","oth","ius","ek","ius",
    "ius","ar","en","yn","or","is"
};

std::string GenerateName()
{
    std::uniform_int_distribution<size_t> p(0, kPrefix.size() - 1);
    std::uniform_int_distribution<size_t> m(0, kMiddle.size() - 1);
    std::uniform_int_distribution<size_t> s(0, kSuffix.size() - 1);
    std::uniform_int_distribution<int>    parts(0, 1);    // 2 or 3 syllables

    std::string out;
    out += kPrefix[p(Rng())];
    if (parts(Rng())) out += kMiddle[m(Rng())];
    out += kSuffix[s(Rng())];

    // Trinity caps player names at MAX_PLAYER_NAME (12). Trim if our
    // generator landed long.
    if (out.size() > MAX_PLAYER_NAME)
        out.resize(MAX_PLAYER_NAME);
    return out;
}

} // anonymous

std::string BotComposition::RollUniqueName()
{
    // A generated name must pass BOTH:
    //   1. Unique vs sCharacterCache (no collision with existing chars)
    //   2. ObjectMgr::CheckPlayerName (length/charset/triple-consonant/banned)
    // Earlier versions only checked (1); the downstream BotCharacterFactory
    // ::Create then failed on (2) with reason "name rejected by ObjectMgr
    // ::CheckPlayerName" — observed 2026-05-15 when JIT-spawn requested
    // 3 DPS for an LFG queue and all 3 failed silently. The fix is to
    // validate up-front so the name we hand back is guaranteed loadable.
    auto valid = [](std::string const& n) {
        if (n.empty()) return false;
        if (sCharacterCache->GetCharacterCacheByName(n)) return false;
        return ObjectMgr::CheckPlayerName(n, LOCALE_enUS, true) == CHAR_NAME_SUCCESS;
    };

    for (int attempt = 0; attempt < 64; ++attempt)
    {
        std::string n = GenerateName();
        if (valid(n)) return n;
    }
    // Fallback: append numeric suffix.
    for (int suffix = 1; suffix < 9999; ++suffix)
    {
        std::string n = GenerateName();
        if (n.size() + 5 > MAX_PLAYER_NAME) n.resize(MAX_PLAYER_NAME - 5);
        n += std::to_string(suffix);
        if (valid(n)) return n;
    }
    TC_LOG_ERROR("playerbot.v2", "[BotComposition] Could not generate unique name after exhaustion.");
    return std::string{};
}

namespace {

// Rolls a race that supports the given class (any faction). Used when the
// caller hinted a class but not a race. Iterates the union of race tables
// and filters by sObjectMgr->GetPlayerInfo. Cheap; ~25 entries max.
uint8 PickRaceForClass(uint8 cls)
{
    std::vector<RaceWeight> legal;
    legal.reserve(kAllianceRaces.size() + kHordeRaces.size() + 2);
    for (RaceWeight rw : kAllianceRaces)
        if (sObjectMgr->GetPlayerInfo(rw.race, cls)) legal.push_back(rw);
    for (RaceWeight rw : kHordeRaces)
        if (sObjectMgr->GetPlayerInfo(rw.race, cls)) legal.push_back(rw);
    if (sObjectMgr->GetPlayerInfo(RACE_DRACTHYR_ALLIANCE, cls))
        legal.push_back({RACE_DRACTHYR_ALLIANCE, kDracthyrAllianceWeight});
    if (legal.empty()) return 0;
    return PickFromRaceWeights(legal);
}

} // anonymous (extension)

namespace {

char const* RaceName(uint8 r)
{
    switch (r)
    {
        case RACE_HUMAN:               return "Human";
        case RACE_ORC:                 return "Orc";
        case RACE_DWARF:               return "Dwarf";
        case RACE_NIGHTELF:            return "NightElf";
        case RACE_UNDEAD_PLAYER:       return "Undead";
        case RACE_TAUREN:              return "Tauren";
        case RACE_GNOME:               return "Gnome";
        case RACE_TROLL:               return "Troll";
        case RACE_GOBLIN:              return "Goblin";
        case RACE_BLOODELF:            return "BloodElf";
        case RACE_DRAENEI:             return "Draenei";
        case RACE_WORGEN:              return "Worgen";
        case RACE_PANDAREN_ALLIANCE:   return "Pandaren-A";
        case RACE_PANDAREN_HORDE:      return "Pandaren-H";
        case RACE_NIGHTBORNE:          return "Nightborne";
        case RACE_HIGHMOUNTAIN_TAUREN: return "HighmountainTauren";
        case RACE_VOID_ELF:            return "VoidElf";
        case RACE_LIGHTFORGED_DRAENEI: return "LightforgedDraenei";
        case RACE_ZANDALARI_TROLL:     return "ZandalariTroll";
        case RACE_KUL_TIRAN:           return "KulTiran";
        case RACE_DARK_IRON_DWARF:     return "DarkIronDwarf";
        case RACE_VULPERA:             return "Vulpera";
        case RACE_MAGHAR_ORC:          return "MagharOrc";
        case RACE_MECHAGNOME:          return "Mechagnome";
        case RACE_DRACTHYR_ALLIANCE:   return "Dracthyr-A";
        case RACE_DRACTHYR_HORDE:      return "Dracthyr-H";
        case RACE_EARTHEN_DWARF_ALLIANCE: return "Earthen-A";
        case RACE_EARTHEN_DWARF_HORDE: return "Earthen-H";
        case 86:                       return "Haranir";  // RACE_HARRONIR — pending TC enable
        default:                       return "?";
    }
}

char const* ClassName(uint8 c)
{
    switch (c)
    {
        case CLASS_WARRIOR:        return "Warrior";
        case CLASS_PALADIN:        return "Paladin";
        case CLASS_HUNTER:         return "Hunter";
        case CLASS_ROGUE:          return "Rogue";
        case CLASS_PRIEST:         return "Priest";
        case CLASS_DEATH_KNIGHT:   return "DeathKnight";
        case CLASS_SHAMAN:         return "Shaman";
        case CLASS_MAGE:           return "Mage";
        case CLASS_WARLOCK:        return "Warlock";
        case CLASS_MONK:           return "Monk";
        case CLASS_DRUID:          return "Druid";
        case CLASS_DEMON_HUNTER:   return "DemonHunter";
        case CLASS_EVOKER:         return "Evoker";
        default:                   return "?";
    }
}

} // anonymous (extension)

std::string BotComposition::DescribeWeights()
{
    std::ostringstream out;
    out << "Faction: ~50/50 Alliance/Horde\n";
    out << "Alliance races: ";
    for (auto const& rw : kAllianceRaces)
        out << fmt::format("{}={} ", RaceName(rw.race), rw.weight);
    out << fmt::format("{}={}\n", RaceName(RACE_DRACTHYR_ALLIANCE), kDracthyrAllianceWeight);
    out << "Horde races: ";
    for (auto const& rw : kHordeRaces)
        out << fmt::format("{}={} ", RaceName(rw.race), rw.weight);
    out << "\nClasses: ";
    for (auto const& cw : kClassWeights)
        out << fmt::format("{}={} ", ClassName(cw.cls), cw.weight);
    out << "\nGender: 50/50; race/class restrictions enforced via sObjectMgr->GetPlayerInfo.";
    return out.str();
}

BotComposition::Pick BotComposition::Roll(uint8 race_hint, uint8 cls_hint,
                                           uint8 gender_hint, std::string name_hint)
{
    Pick p;

    // Per-failure tally so the Roll-exhausted error tells us WHY the
    // attempts failed (was it race-pick? class-pick? PlayerInfo gap?
    // name generator?). Previously the log just said "exhausted" with
    // no indication which inner step was the bottleneck.
    uint32 fail_no_race = 0, fail_no_class = 0, fail_no_player_info = 0, fail_empty_name = 0;
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        // Resolve race: hint → use; else if class hinted → roll for that
        // class; else faction-then-race weighted roll.
        uint8 race = race_hint;
        if (!race)
        {
            if (cls_hint)
            {
                race = PickRaceForClass(cls_hint);
            }
            else
            {
                Faction f = (std::uniform_int_distribution<int>(0, 1)(Rng()) == 0)
                            ? Faction::Alliance : Faction::Horde;
                race = PickRace(f);
            }
        }
        if (!race) { ++fail_no_race; continue; }

        // Resolve class: hint → use; else roll from race's legal subset.
        uint8 cls = cls_hint;
        if (!cls) cls = PickClassForRace(race);
        if (!cls) { ++fail_no_class; continue; }

        // Validate the (race, class) pair — defensive in case caller hinted
        // both with an illegal combo.
        if (!sObjectMgr->GetPlayerInfo(race, cls)) { ++fail_no_player_info; continue; }

        p.race   = race;
        p.cls    = cls;
        p.gender = (gender_hint == 0xFF) ? PickGender() : gender_hint;
        // Prefer the curated name pool (107K names in
        // wowc_playerbot.playerbots_names, all pre-validated). Falls back
        // to the syllable generator if the pool is exhausted or returns
        // empty for the gender. Pool names are guaranteed to pass
        // ObjectMgr::CheckPlayerName, fixing the create_fail observed
        // 2026-05-15 where 3/3 JIT DPS bots died on
        // CHAR_NAME_THREE_CONSECUTIVE.
        if (!name_hint.empty())
            p.name = std::move(name_hint);
        else
        {
            p.name = Fleet::BotNamePool::Acquire(p.gender);
            if (p.name.empty())
                p.name = RollUniqueName();
        }
        if (p.name.empty()) { ++fail_empty_name; continue; }
        return p;
    }

    TC_LOG_ERROR("playerbot.v2",
        "[BotComposition] Roll exhausted (hints: race={} cls={} gender={} name='{}'); "
        "fail counts: no_race={} no_class={} no_player_info={} empty_name={}",
        race_hint, cls_hint, gender_hint, name_hint,
        fail_no_race, fail_no_class, fail_no_player_info, fail_empty_name);
    return {};
}

} // namespace Playerbot::V2
