// DragonSoulScript — Dragon Soul raid (map 967, Cata final 10/25).
// 8 bosses canonically: Morchok, Zon'ozz, Yor'sahj, Hagara, Ultraxion, Blackhorn,
// Spine of Deathwing, Madness of Deathwing.
//
// TC has only the instance script (instance_dragon_soul.cpp); NO boss scripts
// exist. Generic combat applies.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DragonSoulScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 967; }
    char const* name() const override { return "dragon_soul"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Boss progression — Dragon Soul has 8 encounters.
        // TC has no boss AI; entries from WoWHead community data.
        a.bosses = {
            55265,  // Morchok
            55308,  // Warlord Zon'ozz
            55312,  // Yor'sahj the Unsleeping
            55689,  // Hagara the Stormbinder
            55294,  // Ultraxion
            56427,  // Warmaster Blackhorn
            53890,  // Spine of Deathwing (Hideous Amalgamation — the killable target)
            56173,  // Madness of Deathwing (final; Deathwing, selectable vehicle entry)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDragonSoulScript()
{
    return std::make_unique<DragonSoulScript>();
}

} // namespace Playerbot
