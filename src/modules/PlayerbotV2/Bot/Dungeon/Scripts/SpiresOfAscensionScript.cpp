// SpiresOfAscensionScript — Spires of Ascension (map 2285, SL 50-60).
// Bastion dungeon.
//   * Kin-Tara — Charged Spear; Spear of Sin Lifting (knockback).
//   * Ventunax — Dark Bolt (interrupt) + Recharge Anima.
//   * Oryphrion — Charged Smash; Empyreal Ordnance (telegraphed AoE).
//   * Devos, Paragon of Doubt (final) — phase swap; Forced Doubt.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SpiresOfAscensionScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2285; }
    char const* name() const override { return "spires_of_ascension"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            317661,  // Dark Bolt (Ventunax)
            322818,  // Empyreal Ordnance (Oryphrion)
            317898,  // Charged Spear (Kin-Tara)
            324205,  // Forced Doubt (Devos)
            317918,  // Recharge Anima (Ventunax)
        };
        a.dangerous_auras = {
            322818,  // Empyreal Ordnance zone
            317898,  // Spear telegraph
        };
        // Boss progression — Spires of Ascension has 4 encounters.
        a.bosses = {
            162059,  // Kin-Tara (with Azules)
            162058,  // Ventunax
            162060,  // Oryphrion
            162061,  // Devos, Paragon of Doubt (final)
        };
        // Progression waypoints — SoA is a Bastion ascendance temple
        // with anima-orb-collect events between bosses.
        a.progression_waypoints = {
            { 4068.0f, -4974.0f, 340.0f },   // entry
            { 4138.0f, -4974.0f, 359.0f },   // Kin-Tara platform
            { 4275.0f, -4942.0f, 372.0f },   // Ventunax stairs
            { 4385.0f, -4925.0f, 410.0f },   // Oryphrion bridge
            { 4445.0f, -4925.0f, 471.0f },   // Devos top
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSpiresOfAscensionScript()
{
    return std::make_unique<SpiresOfAscensionScript>();
}

} // namespace Playerbot
