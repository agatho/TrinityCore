// ShrineOfTheStormScript — Shrine of the Storm (map 1864, BfA 110-120).
// 4 bosses: Aqu'sirr, Tidesage Council, Lord Stormsong, Vol'zith.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/KulTiras/ShrineOfTheStorm/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ShrineOfTheStormScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1864; }
    char const* name() const override { return "shrine_of_the_storm"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            135215,  // Voidtouched Drowned (Vol'zith adds)
        };
        a.mandatory_interrupt_spells = {
            // Aqu'sirr
            274359,  // Requiem of the Abyss
            265001,  // Sea Blast
            264101,  // Surging Rush
            264560,  // Choking Brine
            264714,  // Choking Brine Missile
            264522,  // Grasp from the Depths Summon
            // Vol'zith
            269131,  // Ancient Mindbender
            274646,  // Dark Binding
            269289,  // Disciple of the Vol'zith
            268896,  // Mind Rend
            269104,  // Explosive Void
            268347,  // Void Bolt
            269242,  // Surrender to the Void
        };
        a.cc_priority_entries = {
            135215,
        };
        a.dangerous_auras = {
            // Aqu'sirr
            274364,  // Requiem Periodic
            274367,  // Requiem Knockback
            264144,  // Undertow
            264155,  // Surging Rush Damage
            264941,  // Erupting Waters Damage
            264526,  // Grasp from the Depths Damage
            // Vol'zith
            269097,  // Waken the Void Area
            269094,  // Waken the Void AreaTrigger
        };
        // Boss progression — entries from TC's shrine_of_the_storm.h.
        // The Tidesage Council is a multi-boss encounter (Ironhull + Faye);
        // listing both so tank-advance finds whichever is closer.
        // Progression waypoints — Shrine of the Storm is a Stormsong
        // tidesage temple with underwater corridors.
        a.progression_waypoints = {
            {  -29.0f, -195.0f,  31.0f },   // entry
            {   38.0f, -195.0f,  31.0f },   // Aqu'sirr pool
            {  120.0f, -147.0f,  16.0f },   // Tidesage Council chamber
            {  178.0f, -190.0f,  44.0f },   // Stormsong altar
            {  256.0f, -212.0f,  66.0f },   // Vol'zith void sanctum
        };
        a.bosses = {
            134056,  // Aqu'sirr
            134063,  // Brother Ironhull (Tidesage Council)
            134058,  // Galecaller Faye (Tidesage Council)
            134060,  // Lord Stormsong
            134069,  // Vol'zith the Whisperer (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeShrineOfTheStormScript()
{
    return std::make_unique<ShrineOfTheStormScript>();
}

} // namespace Playerbot
