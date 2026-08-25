// BloodmaulSlagMinesScript — Bloodmaul Slag Mines (map 1208, WoD 90-100).
// Frostfire Ridge ogre slave-mine dungeon.
//   * Slave Watcher Crushto — Frenzy + Crushing Blow.
//   * Forgemaster Gog'duh — Magma Shield + Magma Eruption.
//   * Roltall — Burning Slag (zone) + Magma Barrage.
//   * Gug'rokk (final) — Cave-in (zone) + Magma Eruption.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BloodmaulSlagMinesScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1175; }   // Bloodmaul Slag Mines (1208 is Grimrail Depot; audit B31)
    char const* name() const override { return "bloodmaul_slag_mines"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            75198,   // Bloodmaul Geomancer (caster pull)
            75820,   // Vengeful Magma Elemental (Forgemaster)
        };
        a.mandatory_interrupt_spells = {
            162486,  // Magma Eruption (Gog'duh)
            162058,  // Burning Slag (Roltall)
            162490,  // Magma Barrage (Roltall)
            165296,  // Crushing Blow (Crushto)
        };
        a.cc_priority_entries = {
            75198,
            75820,
        };
        a.dangerous_auras = {
            162486,  // Magma Eruption pool
            162058,  // Burning Slag zone
            162457,  // Cave-in (Gug'rokk)
        };
        // Boss progression — entries from WoWHead.
        a.bosses = {
            74787,  // Slave Watcher Crushto
            74366,  // Forgemaster Gog'duh
            75786,  // Roltall
            74790,  // Gug'rokk (final)
        };
        // Progression waypoints — BSM is an ogre slag mine: outdoor
        // ramp → Crushto camp → mine descent → Roltall cliff →
        // Gog'duh forge → Gug'rokk arena.
        a.progression_waypoints = {
            { -290.0f,  2025.0f, 178.0f },   // entry ramp
            { -344.0f,  2079.0f, 178.0f },   // Crushto camp
            { -468.0f,  2025.0f, 155.0f },   // mine descent
            { -490.0f,  2168.0f, 124.0f },   // Roltall cliff
            { -554.0f,  2018.0f,  73.0f },   // Gog'duh forge
            { -639.0f,  1996.0f,  74.5f },   // Gug'rokk arena
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBloodmaulSlagMinesScript()
{
    return std::make_unique<BloodmaulSlagMinesScript>();
}

} // namespace Playerbot
