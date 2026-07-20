// BlackwingDescentScript — Blackwing Descent raid (map 669, Cata 10/25).
// 6 bosses canonically: Magmaw, Omnotron, Maloriak, Atramedes, Chimaeron, Nefarian.
//
// TC has only the instance script (instance_blackwing_descent.cpp); NO boss
// scripts exist. All boss combat data would be fabricated. Generic combat
// fallback carries the encounter via DungeonScriptMgr null-script path.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackwingDescentScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 669; }
    char const* name() const override { return "blackwing_descent"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // TC has no boss scripts for BWD; generic combat applies.
        // Boss progression — NPC entries from TC's blackwing_descent.h
        // (the instance script defines them even though boss AI doesn't).
        // Omnotron Defense System isn't in the TC header — standard
        // entries 42180-42183 (Arcanotron/Electron/Magmatron/Toxitron);
        // listing the lead controller entry only.
        a.bosses = {
            41570,  // Magmaw
            42179,  // Omnotron Defense System (lead Arcanotron)
            41442,  // Atramedes
            43296,  // Chimaeron
            41378,  // Maloriak
            41376,  // Nefarian (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackwingDescentScript()
{
    return std::make_unique<BlackwingDescentScript>();
}

} // namespace Playerbot
