// NecroticWakeScript — The Necrotic Wake (map 2286, SL 50-60).
//   * Blightbone — Carrion Eruption (telegraphed AoE) + Heaving
//     Retch (cone).
//   * Amarth, the Reaper (mini) — Land of the Dead phase.
//   * Surgeon Stitchflesh — Stitchflesh's Creation summons (priority
//     kill); Meat Hooks (telegraphed).
//   * Nalthor the Rimebinder (final) — Frozen Binds + Comet Storm.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class NecroticWakeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2286; }
    char const* name() const override { return "necrotic_wake"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            166236,  // Stitchflesh's Creation (Surgeon adds)
            165821,  // Bone Magus (Amarth's adds)
        };
        a.mandatory_interrupt_spells = {
            320168,  // Carrion Eruption (Blightbone)
            320446,  // Meat Hooks (Stitchflesh)
            320772,  // Frozen Binds (Nalthor)
            320788,  // Comet Storm (Nalthor)
        };
        a.dangerous_auras = {
            320168,  // Carrion Eruption zone
            320788,  // Comet Storm telegraph
        };
        // Boss progression — Necrotic Wake has 4 encounters.
        // Nalthor (162693) has no spawn row on map 2286 (summoned on
        // the necropolis platform) — navigator falls back to waypoints.
        a.bosses = {
            162691,  // Blightbone
            163157,  // Amarth
            162689,  // Surgeon Stitchflesh
            162693,  // Nalthor the Rimebinder (final)
        };
        // Progression waypoints — NW is a 4-room Maldraxxus dungeon.
        a.progression_waypoints = {
            { -3127.0f, -3933.0f, 80.0f },   // entry
            { -3196.0f, -3956.0f, 85.0f },   // Blightbone bone yard
            { -3296.0f, -3941.0f, 88.0f },   // Amarth scythe room
            { -3375.0f, -3868.0f, 117.0f },  // Stitchflesh surgery
            { -3416.0f, -3953.0f, 144.0f },  // Nalthor ice chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeNecroticWakeScript()
{
    return std::make_unique<NecroticWakeScript>();
}

} // namespace Playerbot
