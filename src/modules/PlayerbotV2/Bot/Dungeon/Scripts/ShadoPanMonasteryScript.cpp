// ShadoPanMonasteryScript — Shado-Pan Monastery (map 959, MoP 90).
// Tier 1.
//   * Gu Cloudstrike — Lightning bolt (interrupt) + Eye of the
//     Storm (telegraphed AoE, knockback).
//   * Master Snowdrift — Quivering Palm stack debuff;
//     phase-channeled pillars-of-light AoE.
//   * Sha of Violence — Anger phase (frenzy mechanic).
//   * Taran Zhu (final) — Sha curse (HP cap + DoT) — must dispel
//     critical curses fast.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ShadoPanMonasteryScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 959; }
    char const* name() const override { return "shado_pan_monastery"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            106823,  // Lightning Bolt (Gu)
            106824,  // Eye of the Storm (Gu)
            107485,  // Quivering Palm (Snowdrift)
            107529,  // Pillar of Light (Snowdrift)
            107796,  // Sha Curse (Taran Zhu) — critical
            107915,  // Frenzy (Sha of Violence)
        };
        a.dangerous_auras = {
            106824,  // Eye of the Storm zone
            107529,  // Pillar of Light
            107796,  // Sha Curse
        };
        // Boss progression — Shado-Pan Monastery has 4 encounters.
        // Progression waypoints — Shado-Pan Monastery is an outdoor
        // Kun-Lai monastery: outer courtyard → snowy stairs →
        // Snowdrift's terrace → inner gate → Taran Zhu sanctum.
        a.progression_waypoints = {
            { 3795.0f, 2954.0f, 750.0f },   // entry
            { 3719.0f, 2954.0f, 791.0f },   // Gu Cloudstrike platform
            { 3577.0f, 2904.0f, 791.0f },   // Snowdrift terrace
            { 3530.0f, 3081.0f, 803.0f },   // Sha of Violence arena
            { 3614.0f, 3137.0f, 815.0f },   // Taran Zhu sanctum
        };
        a.bosses = {
            56747,  // Gu Cloudstrike
            56541,  // Master Snowdrift
            56719,  // Sha of Violence
            56884,  // Taran Zhu (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeShadoPanMonasteryScript()
{
    return std::make_unique<ShadoPanMonasteryScript>();
}

} // namespace Playerbot
