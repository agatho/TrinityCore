// HourOfTwilightScript — Hour of Twilight (map 940, Cata 4.3).
// Twilight Highlands; Thrall escort.
//   * Arcurion — Hour of Frost (telegraphed nova).
//   * Asira Dawnslayer — Adjacent Strike chain.
//   * Archbishop Benedictus (final) — phase swap; Holy & Twilight
//     phases (different mechanics).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HourOfTwilightScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 940; }
    char const* name() const override { return "hour_of_twilight"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            103312,  // Hour of Frost (Arcurion)
            103325,  // Adjacent Strike (Asira)
            103379,  // Twilight Strike (Benedictus)
            103354,  // Twilight Inferno (Benedictus)
        };
        a.dangerous_auras = {
            103312,  // Hour of Frost telegraph
            103354,  // Twilight Inferno
        };
        // Boss progression — Hour of Twilight has 3 encounters.
        a.bosses = {
            54590,   // Arcurion
            54968,   // Asira Dawnslayer
            54938,   // Archbishop Benedictus (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHourOfTwilightScript()
{
    return std::make_unique<HourOfTwilightScript>();
}

} // namespace Playerbot
