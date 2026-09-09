// StormstoutBreweryScript — Stormstout Brewery (map 961, MoP 85-90).
// Pandaren brewery; comedy boss mechanics.
//   * Ook-Ook — Goes Bananas (charge), barrel-pull positioning.
//   * Hoptallus — Carrot Breath (frontal cone) + Hopper adds
//     (priority kill).
//   * Yan-Zhu the Uncasked (final) — alementals (Bloating Yeast,
//     Fizzy Bubblefroth) — priority kill.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class StormstoutBreweryScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 961; }
    char const* name() const override { return "stormstout_brewery"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            58910,  // Hopper (Hoptallus)
            59479,  // Bloating Yeast (Yan-Zhu)
            59480,  // Fizzy Bubblefroth (Yan-Zhu)
        };
        a.mandatory_interrupt_spells = {
            106543,  // Goes Bananas (Ook-Ook)
            115621,  // Carrot Breath (Hoptallus)
            114777,  // Yeasty Brew Alemental (Yan-Zhu)
        };
        a.dangerous_auras = {
            115621,  // Carrot Breath cone
        };
        // Boss progression — Stormstout Brewery has 3 encounters.
        a.bosses = {
            56637,  // Ook-Ook
            56717,  // Hoptallus
            59479,  // Yan-Zhu the Uncasked (final)
        };
        // Progression waypoints — Stormstout Brewery is a 3-floor
        // pandaren brewery: hozen courtyard → rabbit warren → cellars.
        a.progression_waypoints = {
            { -711.0f, 1359.0f, 152.0f },   // entry
            { -670.0f, 1414.0f, 152.0f },   // Ook-Ook hozen yard
            { -724.0f, 1296.0f, 152.0f },   // Hoptallus warren
            { -800.0f, 1351.0f, 130.0f },   // Yan-Zhu cellar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeStormstoutBreweryScript()
{
    return std::make_unique<StormstoutBreweryScript>();
}

} // namespace Playerbot
