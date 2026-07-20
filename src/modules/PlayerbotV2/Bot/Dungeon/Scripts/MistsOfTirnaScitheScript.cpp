// MistsOfTirnaScitheScript — Mists of Tirna Scithe (map 2290, SL).
//   * Ingra Maloch — Spirit Bolts (interrupt) + Soulshape phase.
//   * Mistcaller (mini) — Mind Link (silence).
//   * Tred'ova (final) — Volatile Acid Sacks (priority kill).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MistsOfTirnaScitheScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2290; }
    char const* name() const override { return "mists_of_tirna_scithe"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            166276,  // Volatile Acid Sack (Tred'ova)
        };
        a.mandatory_interrupt_spells = {
            322557,  // Spirit Bolts (Ingra Maloch)
            324293,  // Mind Link (Mistcaller)
            322193,  // Brain Spike (Tred'ova)
        };
        a.dangerous_auras = {
            322557,  // Spirit Bolts
        };
        // Boss progression — Mists of Tirna Scithe has 3 encounters.
        a.bosses = {
            164567,  // Ingra Maloch
            164501,  // Mistcaller
            164517,  // Tred'ova (final)
        };
        // Progression waypoints — MoTS is an Ardenweald wilds dungeon
        // with a maze section between bosses 2 and 3.
        a.progression_waypoints = {
            { 1300.0f, 1010.0f, 350.0f },   // entry
            { 1287.0f, 1140.0f, 350.0f },   // Ingra grove
            { 1287.0f, 1370.0f, 357.0f },   // Mistcaller maze
            { 1380.0f, 1460.0f, 360.0f },   // Tred'ova chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMistsOfTirnaScitheScript()
{
    return std::make_unique<MistsOfTirnaScitheScript>();
}

} // namespace Playerbot
