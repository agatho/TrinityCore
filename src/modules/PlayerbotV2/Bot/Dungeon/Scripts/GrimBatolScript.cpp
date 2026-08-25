// GrimBatolScript — Grim Batol (map 670, Cata 84-85).
//   * General Umbriss — Ground Siege (telegraphed AoE) + Modgud
//     adds (priority kill).
//   * Forgemaster Throngus — Phase Strike (frontal cone) + Shield
//     Throw (incoming damage; absorb).
//   * Drahga Shadowburner — Twilight Inferno; tail Valiona drake
//     phase.
//   * Erudax (final) — Binding Shadows (channel; interrupt).
//     Dragon Eggs hatch into adds (priority).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GrimBatolScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 670; }
    char const* name() const override { return "grim_batol"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            40657,  // Modgud Berserker (Umbriss adds)
            40683,  // Drakonid (drake-phase adds)
            40484,  // Hatchling (Erudax egg adds)
        };
        a.mandatory_interrupt_spells = {
            74806,  // Ground Siege (Umbriss)
            74938,  // Phase Strike (Throngus)
            74969,  // Twilight Inferno (Drahga)
            75254,  // Binding Shadows (Erudax)
        };
        a.dangerous_auras = {
            74806,  // Ground Siege telegraph
            74938,  // Phase Strike cone
            75254,  // Binding Shadows zone
        };
        // Boss progression — NPC entries from TC's grim_batol.h.
        a.bosses = {
            39625,  // General Umbriss
            40177,  // Forgemaster Throngus
            40319,  // Drahga Shadowburner
            40484,  // Erudax (final)
        };
        // Progression waypoints — Grim Batol is a Twilight cult dungeon
        // inside the mountain. Outdoor → cave → arena progression.
        a.progression_waypoints = {
            { -688.0f, -829.0f, 235.0f },   // entry approach
            { -603.0f, -757.0f, 236.0f },   // Umbriss courtyard
            { -688.0f, -698.0f, 236.0f },   // Throngus forge
            { -668.0f, -549.0f, 232.0f },   // Drahga bridge
            { -679.0f, -421.0f, 232.0f },   // Erudax altar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGrimBatolScript()
{
    return std::make_unique<GrimBatolScript>();
}

} // namespace Playerbot
