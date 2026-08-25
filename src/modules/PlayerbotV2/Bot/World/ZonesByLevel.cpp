#include "ZonesByLevel.h"

namespace Playerbot::V2::World {

namespace {

// Curated zone list spanning L1 to current cap (L80). Coordinates picked at
// each zone's main quest hub / flight master. Faction flags allow a zone
// to be Alliance-only (Westfall, Loch Modan), Horde-only (Durotar, Barrens)
// or contested/shared (Stranglethorn, Hillsbrad after revamp, all later
// expansions). Bot's faction is checked at pick time.
//
// Bracket coverage rationale: each 10-level band has 4-8 candidate zones
// so the distribution doesn't clump 50 bots into one zone. PickZoneForLevel
// hashes (bot_id, bucket) to spread the population deterministically.

constexpr ZoneEntry kZones[] = {
    // L1-10 (starter / early)
    { 12,  1, 10,    0,  -8949.95f,  -132.49f,    83.53f, true,  false, "Elwynn Forest" },
    { 1,   1, 10,    1,  -618.51f,  -4251.49f,    38.72f, false, true,  "Durotar" },
    { 14,  1, 10,    1,  -2358.92f,  -332.83f,    -8.85f, false, true,  "Mulgore" },
    { 38,  1, 10,    0,  -6240.32f,   333.34f,   384.66f, true,  false, "Loch Modan" },
    { 85,  1, 10,    0,   2266.75f,   285.08f,    35.07f, false, true,  "Tirisfal Glades" },
    { 141, 1, 10,    1,   9947.50f,  2482.70f,  1316.21f, true,  false, "Teldrassil" },

    // L10-20
    { 40,  10, 20,   0,  -10491.35f, 1027.12f,    33.56f, true,  false, "Westfall" },
    { 17,  10, 20,   1,  -2400.00f, -2700.00f,    91.00f, false, true,  "Northern Barrens" },
    { 11,  10, 20,   0,  -3779.00f, -823.00f,    14.00f, false, true,  "Northern Tirisfal / Silverpine Forest" },
    { 130, 10, 20,   0,   271.00f,   1465.00f,   125.00f, false, true,  "Silverpine Forest" },
    { 215, 10, 20,   1,  -3040.00f, -2300.00f,   30.00f,  false, true,  "Mulgore (low)" },

    // L20-30
    { 33, 20, 30,   0,  -14289.00f,   556.00f,    9.00f,  true, true,  "Stranglethorn Vale (north)" },
    { 44, 20, 30,   0,   1750.00f, -1659.00f,    60.00f,  true, false, "Redridge Mountains" },
    { 10, 20, 30,   0,  -5200.00f,  -2500.00f,   470.00f, true, false, "Duskwood" },
    { 4,  20, 30,   1,  -2918.00f, -1800.00f,   33.00f,   true, true,  "Stonetalon Mountains" },
    { 16, 20, 30,   1,   3530.00f,  -3650.00f,   136.00f, false, true, "Azshara" },
    { 8,  20, 30,   0,  -2000.00f,  -2400.00f,    77.00f, false, true, "Swamp of Sorrows" },

    // L30-40
    { 45, 30, 40,   0,   -800.00f,  -3300.00f,   77.00f,  true, true,  "Arathi Highlands" },
    { 267, 30, 40,  0,    -300.00f,  -2900.00f,    9.00f, true, false, "Hillsbrad Foothills" },
    { 357, 30, 40,  1,   -3450.00f,  2900.00f,    11.00f, true, true,  "Feralas" },
    { 47, 30, 40,   0,    4670.00f, -3680.00f,   964.00f, true, false, "Hinterlands" },
    { 405, 30, 40,  1,   -4500.00f,   400.00f,    35.00f, true, true,  "Desolace" },

    // L40-50
    { 51,  40, 50,   0,   -7600.00f,  -2200.00f,   135.00f, true, true,  "Searing Gorge" },
    { 46,  40, 50,   0,    3200.00f, -3290.00f,   188.00f, true, true,  "Burning Steppes" },
    { 440, 40, 50,  1,    -5070.00f, -3100.00f,    62.00f, true, true,  "Tanaris" },
    { 1377,40, 50,  1,    -4670.00f,  2330.00f,    34.00f, true, true,  "Silithus" },

    // L50-60
    { 1377, 50, 60, 1,    -7170.00f,  1300.00f,     1.00f, true, true,  "Silithus (mid)" },
    { 28,   50, 60, 0,    2790.00f,  -3920.00f,   100.00f, true, true,  "Western Plaguelands" },
    { 139,  50, 60, 0,    2280.00f,  -5290.00f,   83.00f,  true, true,  "Eastern Plaguelands" },
    { 490,  50, 60, 530, -1700.00f,  5110.00f,    -10.00f, true, true,  "Hellfire Peninsula" },

    // L60-70 (Outland / Northrend leveling)
    { 3483, 60, 70, 530, -1370.00f,  5410.00f,     0.00f,  true, true,  "Hellfire Peninsula (mid)" },
    { 3518, 60, 70, 530, -3270.00f,  4910.00f,    -10.00f, true, true,  "Nagrand" },
    { 3519, 60, 70, 530, -3360.00f,  1610.00f,    71.00f,  true, true,  "Terokkar Forest" },
    { 3537, 60, 70, 571, 5880.00f,   590.00f,    640.00f,  true, true,  "Borean Tundra" },
    { 65,   60, 70, 571, 1450.00f,  -4400.00f,   100.00f,  true, true,  "Dragonblight" },

    // L70-80 (Cap content — Midnight zones)
    { 14771, 70, 80, 2552, -200.00f, 1250.00f,   200.00f,  true, true,  "Isle of Dorn" },
    { 14774, 70, 80, 2552,  300.00f, 2300.00f,    50.00f,  true, true,  "Hallowfall" },
    { 14771, 75, 80, 2552, -200.00f, 1250.00f,   200.00f,  true, true,  "Isle of Dorn (cap)" },
    // Midnight zones (12.0.5) — placeholder coords until verified
    { 14772, 75, 80, 2553, 1000.00f, -1000.00f,  100.00f,  true, true,  "Midnight Zone 1" },
    { 14773, 75, 80, 2553, -1000.00f, 1000.00f,  100.00f,  true, true,  "Midnight Zone 2" },
};

uint64 fnv1a64(uint64 v)
{
    uint64 h = 0xcbf29ce484222325ULL;
    while (v)
    {
        h ^= (v & 0xff);
        h *= 0x100000001b3ULL;
        v >>= 8;
    }
    return h;
}

} // anonymous

ZoneEntry const* PickZoneForLevel(uint8 level, bool alliance, uint64 seed)
{
    // Build candidate list per call — small N (~50 zones), simple linear scan.
    ZoneEntry const* candidates[32];
    uint32 n = 0;
    for (auto const& z : kZones)
    {
        if (level < z.level_lo || level > z.level_hi) continue;
        if (alliance && !z.alliance) continue;
        if (!alliance && !z.horde) continue;
        if (n < 32) candidates[n++] = &z;
    }
    if (!n) return nullptr;
    return candidates[fnv1a64(seed ^ uint64(level)) % n];
}

std::span<ZoneEntry const> AllZones()
{
    return std::span<ZoneEntry const>(kZones, sizeof(kZones)/sizeof(kZones[0]));
}

} // namespace Playerbot::V2::World
