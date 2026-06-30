// ZulFarrakScript — Zul'Farrak (map 209, vanilla 41-51).
// Troll dungeon in Tanaris. Bosses: Antu'sul, Theka the Martyr,
// Sergeant Bly, Hydromancer Velratha, Sandfury Executioner,
// Witch Doctor Zum'rah, Chief Ukorz Sandscalp, Nekrum Gutchewer,
// Ruuzlu, Gahz'rilla.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/ZulFarrak/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ZulFarrakScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 209; }
    char const* name() const override { return "zul_farrak"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            8156,   // Servant of Antu'sul
            7796,   // Sandfury Witch Doctor
        };
        a.mandatory_interrupt_spells = {
            // Witch Doctor Zum'rah
            12739,  // Shadow Bolt
            15245,  // Shadowbolt Volley
            11086,  // Ward of Zum'rah
            12491,  // Healing Wave
        };
        a.cc_priority_entries = {
            7796,
            7798,
        };
        a.dangerous_auras = {
        };
        // Boss progression — Zul'Farrak has 7+ encounters.
        a.bosses = {
            8127,   // Antu'sul
            7272,   // Theka the Martyr
            7271,   // Witch Doctor Zum'rah
            7604,   // Sergeant Bly (Pyramid event lead)
            7796,   // Nekrum Gutchewer (Pyramid event)
            7795,   // Hydromancer Velratha
            7797,   // Ruuzlu (gladiator boss)
            7267,   // Chief Ukorz Sandscalp (final)
            7273,   // Gahz'rilla (summonable rare)
        };
        // Progression waypoints — Zul'Farrak is an outdoor desert
        // dungeon (no roof in most sections). Linear walk: entry →
        // Antu'sul → west wing (Theka, Zum'rah) → pyramid stairs →
        // pyramid event (Bly's crew) → Velratha pool → Ruuzlu arena →
        // Chief Ukorz final platform.
        a.progression_waypoints = {
            { 1226.0f,  827.5f,    9.0f },   // entry
            { 1245.5f,  747.4f,   28.7f },   // Antu'sul plateau
            { 1273.7f,  837.7f,    9.0f },   // central path
            { 1357.6f,  833.9f,   16.7f },   // Theka chamber
            { 1432.7f,  833.5f,   31.0f },   // Zum'rah's altar
            { 1882.5f,  837.0f,   12.0f },   // pyramid base (Bly event)
            { 1860.4f,  708.4f,   14.0f },   // Velratha's pool
            { 1864.7f, 1232.3f,   45.0f },   // Ruuzlu arena
            { 1882.5f, 1262.7f,   42.0f },   // Chief Ukorz platform
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeZulFarrakScript()
{
    return std::make_unique<ZulFarrakScript>();
}

} // namespace Playerbot
