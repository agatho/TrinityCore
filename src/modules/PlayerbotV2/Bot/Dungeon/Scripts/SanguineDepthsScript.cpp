// SanguineDepthsScript — Sanguine Depths (map 2284, SL 50-60).
// Revendreth depths.
//   * Kryxis the Voracious — Sanguine Eruption.
//   * Executor Tarvold — Castigate (DoT) + Inquisitor's Call.
//   * Grand Proctor Beryllia — adds + Anima Egress (interrupt).
//   * General Kaal (final) — Sinfall Boon transfer mechanic.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SanguineDepthsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2284; }
    char const* name() const override { return "sanguine_depths"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            322992,  // Castigate (Tarvold)
            323019,  // Inquisitor's Call (Tarvold)
            323030,  // Anima Egress (Beryllia)
            322554,  // Sanguine Eruption (Kryxis)
        };
        a.dangerous_auras = {
            322554,  // Sanguine Eruption zone
        };
        // Boss progression — Sanguine Depths has 4 encounters.
        a.bosses = {
            162103,  // Kryxis the Voracious
            162102,  // Executor Tarvold
            162100,  // Grand Proctor Beryllia
            162099,  // General Kaal (final)
        };
        // Progression waypoints — Sanguine Depths is a Revendreth
        // prison crypt with a sin-orb collection mid-run.
        a.progression_waypoints = {
            { 1227.0f,  866.0f, -33.0f },   // entry
            { 1276.0f,  927.0f, -33.0f },   // Kryxis pit
            { 1158.0f,  928.0f, -49.0f },   // Tarvold cellblock
            { 1056.0f,  961.0f, -33.0f },   // Beryllia chapel
            { 1037.0f, 1003.0f,  -2.0f },   // Kaal final platform
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSanguineDepthsScript()
{
    return std::make_unique<SanguineDepthsScript>();
}

} // namespace Playerbot
