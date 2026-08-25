// BastionOfTwilightScript — Bastion of Twilight raid (map 671, Cata 10/25).
// 4-5 bosses: Halfus Wyrmbreaker, Twilight Ascendant Council, Theralion + Valiona,
// Cho'gall, Sinestra (heroic).
//
// TC has only `instance_bastion_of_twilight.cpp` + the .h header; NO boss
// scripts exist. The encounters are not implemented in core. The advice
// here is intentionally minimal — generic combat carries the encounter.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BastionOfTwilightScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 671; }
    char const* name() const override { return "bastion_of_twilight"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // TC has no boss scripts for BoT; all spell IDs would be fabricated.
        // Leave empty — generic combat (interrupt-any, target-lowest-HP)
        // applies via the DungeonScriptMgr fallback path.
        // Boss progression — entries from TC's bastion_of_twilight.h.
        a.bosses = {
            44600,  // Halfus Wyrmbreaker
            45992,  // Valiona (Theralion+Valiona duo)
            45993,  // Theralion
            43686,  // Ignacious (Ascendant Council)
            43687,  // Feludius
            43689,  // Terrastra
            43688,  // Arion
            43735,  // Elementium Monstrosity
            43324,  // Cho'gall (final)
            45213,  // Sinestra (heroic-only)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBastionOfTwilightScript()
{
    return std::make_unique<BastionOfTwilightScript>();
}

} // namespace Playerbot
