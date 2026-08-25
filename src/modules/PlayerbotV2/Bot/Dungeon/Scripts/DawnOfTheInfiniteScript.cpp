// DawnOfTheInfiniteScript — Dawn of the Infinite (DF mega-dungeon).
// Two halves: Galakrond's Fall (map 2579) and Murozond's Rise (map 2581).
// Reissued split as M+ in TWW S3.
// Galakrond's Fall:
//   * Chronikar — Sand Breath + Eternity Zone.
//   * Manifested Timeways — Iridikron channel + four Bronzes.
//   * Blight of Galakrond — Necro-Burst.
//   * Iridikron the Stonescaled — Stalagmite Conjuring.
// Murozond's Rise:
//   * Tyr, the Infinite Keeper — Tyr's Vanguard.
//   * Morchie — Mirror Image + Sands of Time.
//   * Time-Lost Battlefield — Soridormi assist event.
//   * Chrono-Lord Deios (final) — Chrono Burst Extreme.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DawnOfTheInfiniteFallScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2579; }
    char const* name() const override { return "dawn_of_the_infinite_fall"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            204918,  // Iridikron's Creation (Iridikron add)
            205384,  // Infinite Chronoweaver (caster trash)
            205804,  // Risen Dragon (Galakrond gauntlet)
        };
        a.mandatory_interrupt_spells = {
            411498,  // Chrono Burst (Deios)
            412221,  // Necro-Burst (Galakrond Blight)
            411609,  // Stalagmite Conjuring (Iridikron)
            411498,
        };
        a.cc_priority_entries = {
            204918,
            205384,
        };
        a.dangerous_auras = {
            411498,  // Chrono Burst zone
            412221,  // Necro-Burst pool
            411609,  // Stalagmite zone
        };
        // Galakrond's Fall progression — 4 encounters.
        a.bosses = {
            198995,  // Chronikar
            206238,  // Manifested Timeways
            207639,  // Blight of Galakrond
            204459,  // Iridikron (final)
        };
        return a;
    }
};

class DawnOfTheInfiniteRiseScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2581; }
    char const* name() const override { return "dawn_of_the_infinite_rise"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            201756,  // Familiar Face (Morchie add)
            205151,  // Tyr's Vanguard add
            205384,  // Infinite Chronoweaver (caster trash)
        };
        a.mandatory_interrupt_spells = {
            411498,  // Chrono Burst (Deios)
            412768,  // Sands of Time (Morchie)
            412768,
            411498,
        };
        a.cc_priority_entries = {
            201756,
            205151,
        };
        a.dangerous_auras = {
            411498,  // Chrono Burst zone
            412768,  // Sands of Time pool
        };
        // Murozond's Rise progression — 4 encounters.
        // Time-Lost Battlefield uses Soridormi as fight-lead.
        a.bosses = {
            198998,  // Tyr, the Infinite Keeper
            202789,  // Morchie
            199001,  // Time-Lost Battlefield (Soridormi lead)
            199000,  // Chrono-Lord Deios (final phase)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDawnOfTheInfiniteFallScript()
{
    return std::make_unique<DawnOfTheInfiniteFallScript>();
}

std::unique_ptr<DungeonScript> MakeDawnOfTheInfiniteRiseScript()
{
    return std::make_unique<DawnOfTheInfiniteRiseScript>();
}

} // namespace Playerbot
