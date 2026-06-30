// TempleOfJadeSerpentScript — Temple of the Jade Serpent
// (map 960, MoP 85-90). Sha-corrupted temple in Jade Forest.
//   * Wise Mari — Wash Away (telegraphed; bots step out).
//   * Lorewalker Stonestep — multiple-narrative encounter.
//   * Liu Flameheart + Strife/Peril — monk twin transformation.
//   * Sha of Doubt (final) — Bound by Doubt (mind control;
//     interrupt).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TempleOfJadeSerpentScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 960; }
    char const* name() const override { return "temple_of_jade_serpent"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            106055,  // Wash Away (Wise Mari)
            106113,  // Snowblossom Strike (Lorewalker)
            106205,  // Way of the Tiger (Liu)
            106062,  // Bound by Doubt (Sha of Doubt)
        };
        a.dangerous_auras = {
            106055,  // Wash Away zone
            106062,  // Bound by Doubt
        };
        // Boss progression — Temple of the Jade Serpent has 4 encounters.
        a.bosses = {
            56448,  // Wise Mari
            56843,  // Lorewalker Stonestep
            56732,  // Liu Flameheart
            56439,  // Sha of Doubt (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTempleOfJadeSerpentScript()
{
    return std::make_unique<TempleOfJadeSerpentScript>();
}

} // namespace Playerbot
