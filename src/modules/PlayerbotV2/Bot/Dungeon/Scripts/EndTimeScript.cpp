// EndTimeScript — The End Time (map 938, Cata 4.3 patch). Echo of...
// time-shifted instances. Random echo bosses (Echo of Sylvanas,
// Echo of Tyrande, Echo of Jaina, Echo of Baine), then Murozond.
//   * Each Echo has unique mechanic (Sylvanas Banshee form, Jaina
//     Glacial Spike, Baine Pulverize).
//   * Murozond (final) — Time Distortion (telegraphed AoE) +
//     Hourglass mechanic.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class EndTimeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 938; }
    char const* name() const override { return "end_time"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            102069,  // Distortion Beam (Murozond)
            102117,  // Glacial Spike (Echo of Jaina)
            102131,  // Pulverize (Echo of Baine)
            102146,  // Banshee Wail (Echo of Sylvanas)
            102167,  // Tears of Elune (Echo of Tyrande)
        };
        a.dangerous_auras = {
            102069,  // Distortion Beam (Murozond)
            102117,  // Glacial Spike telegraph
        };
        // Boss progression — End Time has 3 echo encounters (random pool of 4)
        // + final Murozond. NPC entries from TC's CavernsOfTime/EndTime scripts.
        a.bosses = {
            54123,   // Echo of Sylvanas
            54544,   // Echo of Tyrande
            54445,   // Echo of Jaina
            54431,   // Echo of Baine
            54432,   // Murozond (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeEndTimeScript()
{
    return std::make_unique<EndTimeScript>();
}

} // namespace Playerbot
