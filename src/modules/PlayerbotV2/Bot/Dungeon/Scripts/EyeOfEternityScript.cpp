// EyeOfEternityScript — Eye of Eternity raid (map 616, WotLK 10/25).
// Single boss Malygos in 3 phases (ground / sparks / vehicle).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Nexus/EyeOfEternity/boss_malygos.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class EyeOfEternityScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 616; }
    char const* name() const override { return "eye_of_eternity"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            30084,  // Power Spark (P1 — drag away from Malygos)
            30245,  // Nexus Lord (P2)
            30248,  // Scion of Eternity (P2)
        };
        a.mandatory_interrupt_spells = {
            56272,  // Arcane Breath
            61693,  // Arcane Storm Phase I
            57058,  // Arcane Shock (Nexus Lord)
            57060,  // Haste (Nexus Lord)
            56397,  // Arcane Barrage (Scion of Eternity)
            56505,  // Surge of Power Phase II
            57407,  // Surge of Power Phase III (10)
            60936,  // Surge of Power Phase III (25)
            57430,  // Static Field Missile
            57432,  // Arcane Pulse
            57459,  // Arcane Storm Phase III
            56429,  // Summon Arcane Bomb
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            56105,  // Vortex (whirl mechanic)
            73040,  // Vortex Teleport
            56272,  // Arcane Breath
            56505,  // Surge of Power
            56432,  // Arcane Overload
            57430,  // Static Field
            56142,  // Summon Power Spark
            56152,  // Power Spark Malygos buff (boss has it = bad)
            56431,  // Arcane Bomb Knockback Damage
            61693,  // Arcane Storm Phase I
            57459,  // Arcane Storm Phase III
            60670,  // Berserk
        };
        // Boss progression — Eye of Eternity is a single-boss raid.
        // Progression waypoints — Eye of Eternity is a single-room
        // arena with Malygos's flying phase. Bot lands at the center
        // platform; air phase is server-controlled.
        a.progression_waypoints = {
            {  754.0f, 1301.0f, 268.0f },   // entry platform
            {  754.0f, 1287.0f, 268.0f },   // Malygos center
        };
        a.bosses = {
            28859,  // Malygos
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeEyeOfEternityScript()
{
    return std::make_unique<EyeOfEternityScript>();
}

} // namespace Playerbot
