// EverbloomScript — The Everbloom (map 1279, WoD 90-100).
// Gorgrond botanical lab dungeon — Iron Horde corruption of plant magic.
//   * Witherbark — Choking Vines + Goren add summons.
//   * Ancient Protectors — three trees (Earthshaper, Dulhu, Life Warden).
//   * Archmage Sol — Pyroblast + Frost Bolt Volley.
//   * Yalnu (final) — Genesis (zone) + Massive Vines.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class EverbloomScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1279; }
    char const* name() const override { return "everbloom"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            81522,   // Witherbark (boss — burn through Brittle Bark windows)
            83892,   // Life Warden Gola (Ancient Protectors healer — kill first)
        };
        a.mandatory_interrupt_spells = {
            169658,  // Pyroblast (Sol)
            169179,  // Choking Vines (Witherbark)
            164965,  // Frost Bolt Volley (Sol)
            169658,
            169495,  // Genesis (Yalnu)
        };
        a.cc_priority_entries = {
            81522,
        };
        a.dangerous_auras = {
            169495,  // Genesis ground
            169179,  // Choking Vines
            164965,  // Frost zone
            169658,  // Pyroblast pool
        };
        // Progression waypoints — The Everbloom is a Frostfire Ridge
        // grove dungeon: outdoor walk through 5 encounter clearings.
        a.progression_waypoints = {
            {  170.0f,  -1054.0f, 142.0f },   // entry
            {   83.0f,   -965.0f, 142.0f },   // Witherbark grove
            {  -28.0f,   -928.0f, 142.0f },   // Ancient Protectors trio
            { -118.0f,   -892.0f, 142.0f },   // Sol's library
            {  -55.0f,   -793.0f, 167.0f },   // Yalnu final clearing
        };
        // Boss progression — Everbloom has 4 encounters. Ancient
        // Protectors is a duo (Earthshaper Telu + Dulhu) followed by
        // Witherbark; per encounter list, Witherbark precedes Ancient
        // Protectors.
        a.bosses = {
            81522,  // Witherbark
            83893,  // Ancient Protectors (Earthshaper Telu)
            83894,  // Ancient Protectors (Dulhu)
            83892,  // Ancient Protectors (Life Warden Gola)
            82682,  // Archmage Sol
            83846,  // Yalnu (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeEverbloomScript()
{
    return std::make_unique<EverbloomScript>();
}

} // namespace Playerbot
