// SlavePensScript — Slave Pens (map 547, TBC 62-68).
// Coilfang Reservoir wing.
//   * Mennu the Betrayer — Earthbind Totem, Healing Ward (priority
//     kill totem, interrupt heal).
//   * Rokmar the Crackler — Crystal Spike (telegraphed AoE root).
//   * Quagmirran (final) — Slime Spray (frontal cone).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SlavePensScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 547; }
    char const* name() const override { return "slave_pens"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17283,  // Earthbind Totem (Mennu)
            17304,  // Healing Ward (Mennu)
        };
        a.mandatory_interrupt_spells = {
            31600,  // Lightning Bolt (Mennu)
            31610,  // Healing Wave (Mennu)
        };
        a.dangerous_auras = {
            31611,  // Crystal Spike (Rokmar) — root + step out
            31603,  // Slime Spray (Quagmirran) — cone
        };
        // Boss progression — Slave Pens has 3 bosses (Mennu/Rokmar/Quag).
        a.bosses = {
            17941,  // Mennu the Betrayer
            17991,  // Rokmar the Crackler
            17942,  // Quagmirran (final)
        };
        // Progression waypoints — Slave Pens is a linear Coilfang
        // canal: entry → naga slavers → Mennu's chamber → underwater
        // corridor → Rokmar's pool → Quagmirran's deep cavern.
        a.progression_waypoints = {
            {   42.0f,    8.3f,  -20.0f },   // entry
            {   60.0f,  -53.0f,  -20.0f },   // Mennu chamber
            {  104.0f, -150.0f,  -16.0f },   // canal corridor
            {  -16.0f, -266.0f,  -10.0f },   // Rokmar pool
            { -176.0f, -377.0f,  -10.0f },   // Quagmirran deep cavern
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSlavePensScript()
{
    return std::make_unique<SlavePensScript>();
}

} // namespace Playerbot
