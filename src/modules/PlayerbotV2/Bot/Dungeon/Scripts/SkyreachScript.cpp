// SkyreachScript — Skyreach (map 1209, WoD 90-100).
// Spires of Arak Arakkoa sun-temple dungeon.
//   * Ranjit — Wind Breath + Four Winds (move).
//   * Araknath — Solar Energy stacks + Solar Beam.
//   * Rukhran — Smoldering Bonespike + adds.
//   * High Sage Viryx (final) — Cleanse + Solar Burst (zone).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SkyreachScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1209; }
    char const* name() const override { return "skyreach"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            76143,   // Solar Zealot (Viryx)
            76266,   // Phoenix Hatchling (Rukhran)
            74849,   // Skyreach Sun Priest
        };
        a.mandatory_interrupt_spells = {
            156060,  // Solar Burst (Viryx)
            155994,  // Solar Beam (Araknath)
            156297,  // Wind Breath (Ranjit)
            156143,  // Smoldering Bonespike (Rukhran)
            156311,  // Inferno (Skyreach Sun Priest)
        };
        a.cc_priority_entries = {
            74849,
            76143,
        };
        a.dangerous_auras = {
            156060,  // Solar Burst zone
            156143,  // Smoldering Bonespike pool
            156297,  // Wind Breath cone
            155985,  // Solar Energy ground
        };
        // Boss progression — Skyreach has 4 encounters.
        a.bosses = {
            76143,  // Ranjit (note: same as Phoenix Hatchling priority?
                    //   Skyreach boss entries vary by source; this is
                    //   Ranjit per WoWHead — verify in-game)
            75964,  // Araknath
            76143,  // Rukhran (same entry collision possible — verify)
            76266,  // High Sage Viryx (final)
        };
        // Progression waypoints — Skyreach is an Arrakoa sky temple
        // with wind elevators between tiers.
        a.progression_waypoints = {
            { 882.0f, 1903.0f,  243.0f },   // entry
            { 980.0f, 1942.0f,  243.0f },   // Ranjit arena
            { 942.0f, 2107.0f,  290.0f },   // Araknath chamber
            { 996.0f, 2230.0f,  291.0f },   // Rukhran perch
            { 825.0f, 2200.0f,  354.0f },   // Viryx high temple
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSkyreachScript()
{
    return std::make_unique<SkyreachScript>();
}

} // namespace Playerbot
