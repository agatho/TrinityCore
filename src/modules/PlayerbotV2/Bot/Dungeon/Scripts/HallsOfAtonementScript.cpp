// HallsOfAtonementScript — Halls of Atonement (map 2287, SL 50-60).
// Revendreth dungeon.
//   * Halkias, the Sin-Stained Goliath — Heave Debris (telegraphed
//     boulder roll).
//   * Echelon — Stone Call (telegraphed) + Curse of Stone.
//   * High Adjudicator Aleez — Mass Resurrection (interrupt critical;
//     resurrects mini-bosses).
//   * Lord Chamberlain (final) — Telekinetic Toss (knockback).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfAtonementScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2287; }
    char const* name() const override { return "halls_of_atonement"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            319603,  // Mass Resurrection (Aleez) — wipe-causing
            319603,  // Stone Call (Echelon)
            319611,  // Curse of Stone (Echelon)
            326450,  // Telekinetic Toss (Chamberlain)
        };
        a.dangerous_auras = {
            319611,  // Curse of Stone
        };
        // Boss progression — Halls of Atonement has 4 encounters.
        a.bosses = {
            164218,  // Halkias, the Sin-Stained Goliath
            164185,  // Echelon
            165410,  // High Adjudicator Aleez
            165408,  // Lord Chamberlain (final)
        };
        // Progression waypoints — HoA is a Revendreth gothic manor
        // with descending stairs.
        a.progression_waypoints = {
            { 950.0f, 1175.0f, 50.0f },   // entry
            { 938.0f, 1245.0f, 36.0f },   // Halkias courtyard
            { 887.0f, 1310.0f, 25.0f },   // Echelon stairs
            { 855.0f, 1390.0f,  8.0f },   // Aleez plaza
            { 855.0f, 1442.0f, 22.0f },   // Chamberlain throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfAtonementScript()
{
    return std::make_unique<HallsOfAtonementScript>();
}

} // namespace Playerbot
