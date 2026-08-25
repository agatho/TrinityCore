// GateOfSettingSunScript — Gate of the Setting Sun (map 962, MoP 90).
// Mantid Wall defense.
//   * Saboteur Kip'tilak — Sabotage (telegraphed AoE).
//   * Striker Ga'dok — Cannonball Smash + Hurl Amber.
//   * Commander Ri'mok — Skirmish phase summons swarms.
//   * Raigonn (final) — vehicle phase; bots use cannon.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GateOfSettingSunScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 962; }
    char const* name() const override { return "gate_of_setting_sun"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            56237,  // Mantid Swarm (Ri'mok)
        };
        a.mandatory_interrupt_spells = {
            107268,  // Sabotage (Kip'tilak)
            107268,  // Hurl Amber (Ga'dok)
            107299,  // Cannonball Smash (Ga'dok)
            107569,  // Spread Skirmish (Ri'mok)
        };
        a.dangerous_auras = {
            107268,  // Sabotage telegraph
            107299,  // Cannonball telegraph
        };
        // Boss progression — Gate of the Setting Sun has 4 encounters.
        a.bosses = {
            56906,  // Saboteur Kip'tilak
            56589,  // Striker Ga'dok
            56636,  // Commander Ri'mok
            56877,  // Raigonn (final)
        };
        // Progression waypoints — Gate of the Setting Sun is a
        // pandaren wall-defense dungeon: gate room → cannon turret →
        // wall walk → Raigonn siege battle.
        a.progression_waypoints = {
            { 1245.0f, -290.0f, 305.0f },   // entry gate
            { 1239.0f, -260.0f, 305.0f },   // Kip'tilak sabotage
            { 1304.0f, -224.0f, 359.0f },   // Ga'dok cannon
            { 1322.0f, -245.0f, 359.0f },   // Ri'mok wall walk
            { 1422.0f, -284.0f, 384.0f },   // Raigonn siege
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGateOfSettingSunScript()
{
    return std::make_unique<GateOfSettingSunScript>();
}

} // namespace Playerbot
