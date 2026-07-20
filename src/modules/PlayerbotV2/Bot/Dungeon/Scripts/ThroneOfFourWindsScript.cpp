// ThroneOfFourWindsScript — Throne of the Four Winds raid (map 754, Cata 10/25).
// 2 bosses canonically: Conclave of Wind (Anshal/Nezir/Rohash trio), Al'Akir.
//
// TC has only the instance script (instance_throne_of_the_four_winds.cpp);
// NO boss scripts exist. Generic combat applies.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ThroneOfFourWindsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 754; }
    char const* name() const override { return "throne_of_the_four_winds"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Boss progression — ToFW has 2 encounters
        // (Conclave of Wind is a 3-boss platform fight, listing all).
        a.bosses = {
            45870,  // Anshal (Conclave of Wind)
            45871,  // Nezir (Conclave of Wind)
            45872,  // Rohash (Conclave of Wind)
            46753,  // Al'Akir (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeThroneOfFourWindsScript()
{
    return std::make_unique<ThroneOfFourWindsScript>();
}

} // namespace Playerbot
