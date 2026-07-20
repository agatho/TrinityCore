// RookeryScript — The Rookery (map 2648, TWW 70-80).
// Hallowfall storm-dragon (rock-flayer) dungeon.
//   * Kyrioss — Lightning Bolt + Static Discharge.
//   * Stormguard Gorren — Sundering Charge + adds.
//   * Voidstone Monstrosity (final) — Void Empowerment + Stormrider stacks.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RookeryScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2648; }
    char const* name() const override { return "rookery"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Stub-quality lists were filled with spell 424888 (which is
        // actually Stonevault Edna's Seismic Smash, not a Rookery spell)
        // and entry 207205 was listed BOTH as an add and as boss Kyrioss
        // — the data was never researched and the duplicates indicate
        // placeholder fill. Cleared until real IDs are captured via
        // in-dungeon live spell sniffing or upstream TC source.
        // a.high_priority_kill_entries left default — Roamer/Attacker
        // logic will engage by proximity. dangerous_auras + interrupts
        // empty — generic interrupt rules still fire on any visible
        // Role::Caster enemy.
        // Boss progression — The Rookery has 3 encounters. Boss entries
        // verified against the comment headers; only Kyrioss (207205)
        // can be confirmed via the spawn_groups DB; Gorren + Voidstone
        // need DB lookup. Left in for boss-recognition but flagged.
        a.bosses = {
            207205,  // Kyrioss (verified by collision with old "Stormrider" mis-label)
            // 207207, 207202 — unverified; restore once DB-cross-checked.
        };
        // Progression waypoints — The Rookery is a Hallowfall
        // storm-dragon nest with rooftop levels.
        a.progression_waypoints = {
            { -640.0f,  -710.0f, 2025.0f },   // entry
            { -575.0f,  -645.0f, 2050.0f },   // Kyrioss rooftop
            { -495.0f,  -560.0f, 2080.0f },   // Gorren bridge
            { -420.0f,  -485.0f, 2105.0f },   // Voidstone chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRookeryScript()
{
    return std::make_unique<RookeryScript>();
}

} // namespace Playerbot
