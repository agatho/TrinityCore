// HallsOfInfusionScript — Halls of Infusion (map 2527, DF 60-70).
// Ohn'ahran Plains/Azure Span infusion-themed final dungeon.
//   * Watcher Irideus — Refracting Beam (front cone) + adds.
//   * Gulping Goliath — Devour Water (heal interrupt).
//   * Khajin the Unyielding — Frost Spike Volley (interrupt).
//   * Primal Tsunami (final) — Tidal Burst + Crashing Wave (move out).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfInfusionScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2527; }
    char const* name() const override { return "halls_of_infusion"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            190407,  // Infused Goliath spawn (Watcher Irideus)
            191164,  // Containment Apparatus (adds at Gulping)
        };
        a.mandatory_interrupt_spells = {
            374427,  // Refracting Beam (Irideus)
            374719,  // Devour Water (Goliath heal)
            374570,  // Frost Spike Volley (Khajin)
            374853,  // Tidal Burst (Tsunami)
        };
        a.cc_priority_entries = {
            190407,  // Infused Goliath
        };
        a.dangerous_auras = {
            374853,  // Tidal Burst zone
            374570,  // Frost Spike ground
        };
        // Boss progression — Halls of Infusion has 4 encounters.
        a.bosses = {
            190407,  // Watcher Irideus
            189722,  // Gulping Goliath
            189727,  // Khajin the Unyielding
            189729,  // Primal Tsunami (final)
        };
        // Progression waypoints — Halls of Infusion is an Azure Span
        // titan facility with vertical chamber descent.
        a.progression_waypoints = {
            { 4290.0f,  -660.0f,  -25.0f },   // entry
            { 4192.0f,  -715.0f,  -45.0f },   // Irideus circle
            { 4100.0f,  -805.0f,  -75.0f },   // Goliath pool
            { 4012.0f,  -890.0f,  -95.0f },   // Khajin chamber
            { 3915.0f,  -985.0f, -125.0f },   // Tsunami platform
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfInfusionScript()
{
    return std::make_unique<HallsOfInfusionScript>();
}

} // namespace Playerbot
