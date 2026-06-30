// NokhudOffensiveScript — The Nokhud Offensive (map 2516, DF 60-70).
// Outdoor centaur dungeon. Multi-phase outdoor encounter.
//   * Granyth (drake mini) — Eye Beam (telegraphed line).
//   * The Raging Tempest — Lightning Storm; air phase.
//   * Teera & Maruuk — twin paired fight; Eye of the Storm channel.
//   * Balakar Khan (final) — vehicle phase ride.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class NokhudOffensiveScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2516; }
    char const* name() const override { return "nokhud_offensive"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            385330,  // Eye Beam (Granyth)
            385452,  // Lightning Storm (Raging Tempest)
            386449,  // Centaur Stomp (Maruuk)
            386478,  // Spear Charge (Teera)
            386547,  // Iron Spear (Balakar)
        };
        a.dangerous_auras = {
            385330,  // Eye Beam line
            385452,  // Lightning Storm zone
        };
        // Boss progression — Nokhud Offensive has 4 encounters.
        // Teera & Maruuk is a paired fight; lead entry is Teera
        // (the spawned half of the duo).
        a.bosses = {
            186616,  // Granyth (drake mini-boss)
            186615,  // The Raging Tempest
            186339,  // Teera and Maruuk (Teera as fight-lead)
            186151,  // Balakar Khan (final)
        };
        // Progression waypoints — Nokhud Offensive is an outdoor
        // Ohn'ahran Plains centaur camp; 5 distinct outdoor areas.
        a.progression_waypoints = {
            { 1612.0f,   320.0f,   62.0f },   // entry
            { 1495.0f,   220.0f,   60.0f },   // Granyth drake nest
            { 1380.0f,   110.0f,   56.0f },   // Raging Tempest plain
            { 1270.0f,    -5.0f,   54.0f },   // Teera+Maruuk camp
            { 1150.0f,  -120.0f,   65.0f },   // Balakar throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeNokhudOffensiveScript()
{
    return std::make_unique<NokhudOffensiveScript>();
}

} // namespace Playerbot
