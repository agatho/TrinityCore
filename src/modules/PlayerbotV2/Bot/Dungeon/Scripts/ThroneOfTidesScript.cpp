// ThroneOfTidesScript — Throne of the Tides (map 643, Cata 80-85).
// Naga underwater citadel.
//   * Lady Naz'jar — Geyser (telegraphed AoE) + Waterspout adds.
//   * Commander Ulthok — Curse of Fatigue (dispel).
//   * Mindbender Ghur'sha — Mind Fog channel (interrupt critical;
//     mind controls party).
//   * Ozumat (final) — phase change underwater + tentacle adds.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ThroneOfTidesScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 643; }
    char const* name() const override { return "throne_of_tides"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            40828,  // Waterspout (Naz'jar add)
            40825,  // Tentacle (Ozumat phase 2)
        };
        a.mandatory_interrupt_spells = {
            76334,  // Geyser (Naz'jar)
            76337,  // Tidal Surge (Naz'jar)
            76321,  // Curse of Fatigue (Ulthok)
            76341,  // Mind Fog channel (Ghur'sha) — critical!
        };
        a.dangerous_auras = {
            76334,  // Geyser ground AoE
            76341,  // Mind Fog zone
        };
        // Boss progression — NPC entries from TC's throne_of_the_tides.h.
        a.bosses = {
            40586,  // Lady Naz'jar
            40765,  // Commander Ulthok
            40788,  // Mindbender Ghur'sha
            44566,  // Ozumat (final)
        };
        // Progression waypoints — ToT is an underwater Naga citadel
        // with elevator drops between levels.
        a.progression_waypoints = {
            { -36.5f,  984.0f,  815.7f },   // entry
            {  18.0f, 1112.0f,  814.5f },   // Naz'jar arena
            { -84.0f, 1247.0f,  806.0f },   // Ulthok pool
            {-260.0f, 1245.0f,  806.0f },   // Ghur'sha bridge
            {-447.0f, 1141.0f,  789.0f },   // Ozumat final platform
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeThroneOfTidesScript()
{
    return std::make_unique<ThroneOfTidesScript>();
}

} // namespace Playerbot
