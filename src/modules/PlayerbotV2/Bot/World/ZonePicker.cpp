// ZonePicker — distribution destinations for population spread.
//
// Modern WoW (post-squish, no Chromie Time on this server) uses
// per-zone DYNAMIC LEVEL SCALING: a zone has a bracket (e.g. Hellfire
// Peninsula 10-30) and mobs/quests scale to the bot's actual level
// within that bracket. So a L25 bot entering Hellfire gets L25 mobs;
// a L15 bot in the same zone gets L15 mobs. The bracket gates ENTRY
// (some entry quests have minimum-level prereqs), not the leveling
// content itself.
//
// Goal of this picker: SPREAD bots across the entire game world so
// every expansion's zones get populated, not just the latest one.
// "Linear-progression-only" routing was wrong — it sent every L40 bot
// to vanilla EPL and left Outland / Northrend / Pandaria / Draenor /
// Legion / BfA / Shadowlands empty.
//
// Each (level-bracket, faction) entry advertises ONE candidate zone
// hub. The picker walks the table, collects all rows that match the
// bot's level-bracket + faction, and round-robins via the bot's GUID
// seed. Add as many candidates as you want — more candidates = wider
// spread.
//
// Some expansion entry-quests have minimum-level prereqs (e.g. WoD
// Tanaan 90+, Legion Broken Isles 98+ retail-historical). The bracket
// `lo` field encodes that floor; the picker won't route a L20 bot to
// Khaz Algar even if other candidates exist. Brackets overlap heavily:
// L40 has Outland + Northrend + Pandaria + WoD + Legion + BfA + SL +
// vanilla Tanaris + others all valid, so 200 L40 bots split ~25 ways.

#include "ZonePicker.h"

#include <array>

namespace Playerbot::V2::World {

namespace {

struct BracketEntry
{
    uint8  lo, hi;             // inclusive level bracket
    bool   alliance;           // true = Alliance only, false = Horde only
    uint32 map_id;
    float  x, y, z;
    char const* zone;
    uint32 starter_quest;
};

// Comprehensive candidate list. Brackets overlap to spread bots across
// every expansion's zone instead of clustering them by level.
constexpr std::array<BracketEntry, 88> kBrackets = {{
    // ===== L0-9 catch-all (faction capital) =====
    {  0,  9, true,  0, -8833.0f,   628.0f,   94.0f, "Stormwind",                            0 },
    {  0,  9, false, 1,  1633.0f, -4439.0f,   16.0f, "Orgrimmar",                            0 },

    // ===== Vanilla zones — Eastern Kingdoms / Kalimdor (L10-60) =====
    // Alliance starter chains
    { 10, 60, true,  0, -10752.0f,  1252.0f,   42.0f, "Westfall (Sentinel Hill)",            0 },
    { 10, 60, true,  0,  -5455.0f, -3413.0f,  306.0f, "Loch Modan (Thelsamar)",              0 },
    { 10, 60, true,  1,   6256.0f,    16.0f,    9.0f, "Darkshore (Auberdine)",               0 },
    { 15, 60, true,  0,  -3796.0f,  -777.0f,   11.0f, "Wetlands (Menethil Harbor)",          0 },
    { 15, 60, true,  0,  -2110.0f, -2480.0f,   99.0f, "Hillsbrad Foothills (Southshore)",    0 },
    { 20, 60, true,  0, -14470.0f,   471.0f,   23.0f, "Northern Stranglethorn (Rebel Camp)", 0 },
    { 25, 60, true,  1,  -7173.0f, -3791.0f,    8.0f, "Tanaris (Gadgetzan)",                 0 },
    { 25, 60, true,  1,  -4413.0f,   324.0f,   47.0f, "Feralas (Feathermoon Stronghold)",    0 },
    { 30, 60, true,  0,  -6831.0f, -2030.0f,  244.0f, "Searing Gorge (Thorium Point)",       0 },
    { 35, 60, true,  0,   2283.0f, -5318.0f,   89.0f, "Eastern Plaguelands (Light's Hope)",  0 },
    { 40, 60, true,  1,   7479.0f, -4738.0f,  687.0f, "Winterspring (Everlook)",             0 },
    { 40, 60, true,  1,  -7045.0f,  1242.0f,    7.0f, "Silithus (Cenarion Hold)",            0 },
    // Horde starter chains
    { 10, 60, false, 0,   1822.0f,  1599.0f,   90.0f, "Tirisfal Glades (Brill)",             0 },
    { 10, 60, false, 0,    367.0f,  1452.0f,  124.0f, "Silverpine Forest (Sepulcher)",       0 },
    { 10, 60, false, 1,   -444.0f, -2645.0f,   96.0f, "Northern Barrens (Crossroads)",       0 },
    { 15, 60, false, 1,   2266.0f, -2532.0f,  104.0f, "Stonetalon Mountains (Sun Rock Retreat)", 0 },
    { 20, 60, false, 0, -11919.0f, -1209.0f,   92.0f, "Northern Stranglethorn (Grom'gol)",   0 },
    { 25, 60, false, 1,    194.0f,  1819.0f,   85.0f, "Desolace (Shadowprey Village)",       0 },
    { 25, 60, false, 1,  -7173.0f, -3791.0f,    8.0f, "Tanaris (Gadgetzan)",                 0 },
    { 30, 60, false, 0,  -7906.0f, -1135.0f,  133.0f, "Burning Steppes (Flame Crest)",       0 },
    { 35, 60, false, 0,  -1283.0f,   140.0f,   58.0f, "Western Plaguelands (The Bulwark)",   0 },
    { 40, 60, false, 1,   7479.0f, -4738.0f,  687.0f, "Winterspring (Everlook)",             0 },
    { 40, 60, false, 1,  -7045.0f,  1242.0f,    7.0f, "Silithus (Cenarion Hold)",            0 },

    // ===== Outland (TBC) — L10-30 retail-historical, scales =====
    { 10, 60, true,  530,   -319.0f,  1697.0f,   71.0f, "Hellfire Peninsula (Honor Hold)",   9476 },
    { 15, 60, true,  530,  -3025.0f,  3441.0f,   55.0f, "Zangarmarsh (Telredor)",            9991 },
    { 20, 60, true,  530,  -1838.0f,  5301.0f,   46.0f, "Nagrand (Telaar)",                  9978 },
    { 10, 60, false, 530,   -371.0f,  3082.0f,   90.0f, "Hellfire Peninsula (Thrallmar)",    10119 },
    { 15, 60, false, 530,  -1262.0f,  5544.0f,   22.0f, "Zangarmarsh (Zabra'jin)",           10001 },
    { 20, 60, false, 530,  -1374.0f,  7282.0f,   31.0f, "Nagrand (Garadar)",                 9991 },

    // ===== Northrend (WotLK) — L10-30 retail-historical, scales =====
    { 10, 60, true,  571,   2730.0f,  2169.0f,    7.0f, "Borean Tundra (Valiance Keep)",     0 },
    { 10, 60, true,  571,   1196.6f, -2890.1f,   39.1f, "Howling Fjord (Valgarde)",          0 },
    { 25, 60, true,  571,   3722.0f,  -706.0f,  213.0f, "Dragonblight (Wintergarde Keep)",   0 },
    { 10, 60, false, 571,   2078.0f,  5258.0f,   18.0f, "Borean Tundra (Warsong Hold)",      0 },
    { 10, 60, false, 571,   1832.0f, -6131.0f,   47.0f, "Howling Fjord (Vengeance Landing)", 0 },
    { 25, 60, false, 571,   3633.0f,  -758.0f,  211.0f, "Dragonblight (Agmar's Hammer)",     0 },

    // ===== Cataclysm zones (Hyjal, Vashj'ir, Twilight Highlands, Uldum, Deepholm) =====
    { 30, 60, true,  1,    4665.0f, -1825.0f,  1086.0f, "Mount Hyjal (Nordrassil)",          25372 },
    { 30, 60, true,  0,   -5408.0f, -2434.0f,   23.7f, "Twilight Highlands (Highbank)",      27302 },
    { 30, 60, true,  0,  -10864.0f,  1656.0f,   55.0f, "Uldum (Schnottz's Landing)",         27787 },
    { 30, 60, true,  646,  -724.0f,   846.0f,  207.0f, "Deepholm (Temple of Earth)",         27148 },
    { 30, 60, false, 1,    4665.0f, -1825.0f,  1086.0f, "Mount Hyjal (Nordrassil)",          25372 },
    { 30, 60, false, 0,   -3792.0f, -4655.0f,    9.0f, "Twilight Highlands (Bloodgulch)",    27518 },
    { 30, 60, false, 0,  -10864.0f,  1656.0f,   55.0f, "Uldum (Schnottz's Landing)",         27787 },
    { 30, 60, false, 646,  -724.0f,   846.0f,  207.0f, "Deepholm (Temple of Earth)",         27148 },

    // ===== Pandaria (MoP) =====
    { 30, 60, true,  870,   -700.0f, -4506.0f,   14.0f, "Jade Forest (Paw'don Village)",     31482 },
    { 30, 60, true,  870,   3200.0f,  3019.0f,  120.0f, "Townlong Steppes (Longying Outpost)", 0 },
    { 30, 60, false, 870,   1326.0f, -4566.0f,   21.0f, "Jade Forest (Honeydew Village)",    31781 },
    { 30, 60, false, 870,   3200.0f,  3019.0f,  120.0f, "Townlong Steppes (Longying Outpost)", 0 },

    // ===== Draenor (WoD) =====
    { 40, 60, true,  1116, 5710.0f,   4504.0f,  133.0f, "Frostfire Ridge (Lunarfall)",       34362 },
    { 40, 60, true,  1116, 1804.0f,   2032.0f,  191.0f, "Talador (Vol'mar)",                 0 },
    { 40, 60, false, 1116, 5710.0f,   4504.0f,  133.0f, "Frostfire Ridge (Frostwall)",       34480 },
    { 40, 60, false, 1116, 1804.0f,   2032.0f,  191.0f, "Talador (Vol'jin's Pride)",         0 },

    // ===== Legion (Broken Isles) =====
    { 45, 60, true,  1220, -1791.0f,  1428.0f,  246.0f, "Dalaran (Broken Isles)",            40519 },
    { 45, 60, true,  1220,  1300.0f,  4500.0f,  100.0f, "Stormheim (Greywatch)",             40519 },
    { 45, 60, true,  1220,  4392.0f,  3891.0f,  168.0f, "Highmountain (Thunder Totem)",      40519 },
    { 45, 60, false, 1220, -1791.0f,  1428.0f,  246.0f, "Dalaran (Broken Isles)",            40519 },
    { 45, 60, false, 1220,  1300.0f,  4500.0f,  100.0f, "Stormheim (Greywatch)",             40519 },
    { 45, 60, false, 1220,  4392.0f,  3891.0f,  168.0f, "Highmountain (Thunder Totem)",      40519 },

    // ===== BfA (Kul Tiras / Zandalar) =====
    { 50, 60, true,  1643,  -1058.0f, -2691.0f,    8.0f, "Tiragarde Sound (Boralus)",        51571 },
    { 50, 60, true,  1643,  -2030.0f, -1650.0f,   40.0f, "Drustvar (Falconhurst)",           51571 },
    { 50, 60, false, 1642,  -1126.0f,  3158.0f,   29.0f, "Zuldazar (Dazar'alor)",            52133 },
    { 50, 60, false, 1642,   2456.0f,   824.0f,   80.0f, "Vol'dun (Vorrik's Sanctum)",       52133 },

    // ===== Shadowlands (Bastion / Maldraxxus / Ardenweald / Revendreth) =====
    { 50, 60, true,  1550,  -1646.0f,  -641.0f,  280.0f, "Bastion (Aspirant's Rest)",        62130 },
    { 50, 60, true,  1550,    1190.0f,  -772.0f,  -27.0f, "Maldraxxus (Theater of Pain)",    62130 },
    { 50, 60, true,  1550,    3142.0f,  3997.0f, 1010.0f, "Ardenweald (Heart of the Forest)", 62130 },
    { 50, 60, true,  1550,    1820.0f,   805.0f,    8.0f, "Revendreth (Sinfall)",            62130 },
    { 50, 60, false, 1550,  -1646.0f,  -641.0f,  280.0f, "Bastion (Aspirant's Rest)",        62130 },
    { 50, 60, false, 1550,    1190.0f,  -772.0f,  -27.0f, "Maldraxxus (Theater of Pain)",    62130 },
    { 50, 60, false, 1550,    3142.0f,  3997.0f, 1010.0f, "Ardenweald (Heart of the Forest)", 62130 },
    { 50, 60, false, 1550,    1820.0f,   805.0f,    8.0f, "Revendreth (Sinfall)",            62130 },

    // ===== Dragonflight (Dragon Isles) =====
    { 60, 70, true,  2444,  -1850.0f,  5448.0f,   34.0f, "Forbidden Reach (DF intro)",       65437 },
    { 60, 70, true,  2444,   3550.0f,  3950.0f,   55.0f, "Waking Shores (Wingrest)",         65437 },
    { 60, 70, true,  2444,   2600.0f,   100.0f,  300.0f, "Thaldraszus (Valdrakken)",         65437 },
    { 60, 70, false, 2444,  -1850.0f,  5448.0f,   34.0f, "Forbidden Reach (DF intro)",       66775 },
    { 60, 70, false, 2444,   3550.0f,  3950.0f,   55.0f, "Waking Shores (Wingrest)",         66775 },
    { 60, 70, false, 2444,   2600.0f,   100.0f,  300.0f, "Thaldraszus (Valdrakken)",         66775 },

    // ===== TWW (Khaz Algar) =====
    { 70, 80, true,  2552,   -100.0f, -2700.0f,  105.0f, "Isle of Dorn (Dornogal)",          78714 },
    { 70, 80, true,  2552,    1800.0f,  -150.0f,   50.0f, "Hallowfall (Mereldar)",           78714 },
    { 70, 80, false, 2552,   -100.0f, -2700.0f,  105.0f, "Isle of Dorn (Dornogal)",          78714 },
    { 70, 80, false, 2552,    1800.0f,  -150.0f,   50.0f, "Hallowfall (Mereldar)",           78714 },

    // ===== Midnight (Quel'Thalas) =====
    { 80, 90, true,  0,    9487.0f, -7277.0f,   14.0f, "Silvermoon City (Quel'Thalas)",     0 },
    { 80, 90, true,  0,    7600.0f, -6200.0f,   38.0f, "Eversong Woods (Falconwing Square)", 0 },
    { 80, 90, false, 0,    9487.0f, -7277.0f,   14.0f, "Silvermoon City (Quel'Thalas)",     0 },
    { 80, 90, false, 0,    7600.0f, -6200.0f,   38.0f, "Eversong Woods (Falconwing Square)", 0 },
}};

} // anonymous

DestinationPick PickDestination(uint8 level, bool alliance, uint64 seed)
{
    // Collect indices of brackets matching (level, faction). Bracket
    // overlap means dozens of candidates per level — bots distribute
    // widely instead of clustering.
    std::array<size_t, 32> matches{};
    size_t nmatches = 0;
    for (size_t i = 0; i < kBrackets.size(); ++i)
    {
        BracketEntry const& b = kBrackets[i];
        if (level < b.lo || level > b.hi) continue;
        if (b.alliance != alliance) continue;
        if (nmatches < matches.size()) matches[nmatches++] = i;
    }
    if (nmatches == 0)
    {
        // Defensive fallback — should not hit because L0-9 catch-all
        // covers below the lowest leveling bracket.
        DestinationPick d;
        d.map_id = alliance ? 0 : 1;
        d.x = alliance ? -8833.0f : 1633.0f;
        d.y = alliance ?   628.0f :-4439.0f;
        d.z = alliance ?    94.0f :   16.0f;
        d.zone = alliance ? "Stormwind (fallback)" : "Orgrimmar (fallback)";
        return d;
    }
    BracketEntry const& chosen = kBrackets[matches[seed % nmatches]];
    DestinationPick d;
    d.map_id        = chosen.map_id;
    d.x             = chosen.x;
    d.y             = chosen.y;
    d.z             = chosen.z;
    d.zone          = chosen.zone;
    d.starter_quest = chosen.starter_quest;
    return d;
}

} // namespace Playerbot::V2::World
