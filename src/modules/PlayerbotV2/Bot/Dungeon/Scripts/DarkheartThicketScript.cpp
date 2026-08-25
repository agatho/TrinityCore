// DarkheartThicketScript — Darkheart Thicket (map 1466, Legion 110).
//   * Archdruid Glaidalis — Primal Rampage charge.
//   * Oakheart — Strangling Roots channel.
//   * Dresaron (drake) — Wrath cone + Down Draft (knockback).
//   * Shade of Xavius (final) — Apocalyptic Nightmare AoE +
//     Nightmare Bolt (interrupt critical).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DarkheartThicketScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1466; }
    char const* name() const override { return "darkheart_thicket"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            200449,  // Primal Rampage (Glaidalis)
            199706,  // Strangling Roots (Oakheart)
            200329,  // Down Draft (Dresaron)
            200273,  // Nightmare Bolt (Xavius)
            200825,  // Apocalyptic Nightmare (Xavius)
        };
        a.dangerous_auras = {
            200825,  // Apocalyptic Nightmare zone
            200329,  // Down Draft knockback
        };
        // Boss progression — Darkheart Thicket has 4 encounters.
        a.bosses = {
            96512,   // Archdruid Glaidalis
            103344,  // Oakheart
            99200,   // Dresaron
            99192,   // Shade of Xavius (final)
        };
        // Progression waypoints — Darkheart Thicket is a corrupted
        // Val'sharah forest path with a chase event mid-run.
        a.progression_waypoints = {
            { 1900.0f, 1099.0f, 168.0f },   // entry
            { 1857.0f, 1142.0f, 161.0f },   // Glaidalis clearing
            { 1700.0f, 1230.0f, 191.0f },   // Oakheart grove
            { 1700.0f, 1390.0f, 215.0f },   // chase ramp
            { 1530.0f, 1450.0f, 200.0f },   // Dresaron platform
            { 1370.0f, 1372.0f, 174.0f },   // Xavius corrupted heart
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDarkheartThicketScript()
{
    return std::make_unique<DarkheartThicketScript>();
}

} // namespace Playerbot
