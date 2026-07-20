// UnderbogScript — The Underbog (map 546, TBC 63-69).
// Coilfang Reservoir wing.
//   * Hungarfen — Foul Spores (random AoE; step out).
//   * Ghaz'an — Acid Breath (frontal cone) + Tail Sweep.
//   * Swamplord Musel'ek + Claw — Claw Frenzy (priority kill Claw).
//   * The Black Stalker (final) — Levitate pulls + Static Charge.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UnderbogScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 546; }
    char const* name() const override { return "underbog"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17827,  // Claw (Musel'ek pet)
        };
        a.mandatory_interrupt_spells = {
            31698,  // Acid Breath (Ghaz'an)
            31696,  // Aimed Shot (Musel'ek)
            31706,  // Levitate (Black Stalker)
        };
        a.dangerous_auras = {
            31705,  // Foul Spores (Hungarfen) — random ground zone
            31702,  // Tail Sweep (Ghaz'an) — back-cone
            31711,  // Static Charge (Black Stalker) — DoT debuff
        };
        // Boss progression — Underbog has 4 encounters.
        a.bosses = {
            17770,  // Hungarfen
            18105,  // Ghaz'an
            17826,  // Swamplord Musel'ek (with pet Claw)
            17882,  // The Black Stalker (final)
        };
        // Progression waypoints — Underbog is a swampy outdoor-feel
        // dungeon: entry shore → Hungarfen mushroom grove → Ghaz'an
        // pool → Musel'ek camp → Black Stalker's swamp.
        a.progression_waypoints = {
            {  -73.0f, -373.0f,  -3.0f },   // entry
            {   33.0f, -287.0f,  -2.0f },   // Hungarfen grove
            {  -90.0f,  -83.0f,  -3.5f },   // Ghaz'an pool
            { -159.0f,   38.0f,  -1.5f },   // Musel'ek camp
            {   28.0f,  186.0f,  -3.5f },   // Black Stalker swamp
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUnderbogScript()
{
    return std::make_unique<UnderbogScript>();
}

} // namespace Playerbot
