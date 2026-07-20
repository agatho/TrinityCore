// TheaterOfPainScript — Theater of Pain (map 2293, SL 50-60).
// Maldraxxus arena dungeon.
//   * An Affront of Challengers — three-pack mini final tier;
//     CC priority caster.
//   * Gorechop — Hateful Strike + Tenderizing Smash.
//   * Xav the Unfallen — Massive Cleave (telegraphed) + Brutal
//     Combo wave.
//   * Kul'tharok — Death Spiral (telegraphed) + Spectral Hands
//     (priority kill).
//   * Mordretha (final) — Echos of Carnage (telegraphed) +
//     Manifest Death (priority kill ghosts).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TheaterOfPainScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2293; }
    char const* name() const override { return "theater_of_pain"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            167998,  // Portal Guardian (Kul'tharok wing gate)
            166524,  // Deathwalker (Mordretha — Manifest Death)
        };
        a.mandatory_interrupt_spells = {
            320069,  // Massive Cleave (Xav)
            320248,  // Brutal Combo (Xav)
            319626,  // Death Spiral (Kul'tharok)
            319626,  // Echos of Carnage (Mordretha)
            319447,  // Hateful Strike (Gorechop)
        };
        a.cc_priority_entries = {
            174197,  // Battlefield Ritualist (Affront arena caster)
        };
        a.dangerous_auras = {
            320248,  // Brutal Combo wave
            319626,  // Death Spiral
        };
        // Boss progression — Theater of Pain has 5 encounters
        // (Affront is a multi-mob first encounter; listing lead mob).
        // Progression waypoints — Theater of Pain is a Maldraxxus
        // gladiator arena with 5 distinct sub-arenas (random order
        // after the first two; this set picks canonical M+ path).
        a.progression_waypoints = {
            { -1822.0f, -2030.0f, 154.0f },   // entry
            { -1762.0f, -2086.0f, 155.0f },   // Affront ring
            { -1880.0f, -2113.0f, 105.0f },   // Gorechop hooks
            { -1798.0f, -2092.0f, 105.0f },   // Xav arena
            { -1862.0f, -2055.0f,  47.0f },   // Kul'tharok depths
            { -1922.0f, -2080.0f,  47.0f },   // Mordretha throne
        };
        a.bosses = {
            164461,  // An Affront of Challengers (Sathel the Accursed; Dessia 164451 / Paceran 164463)
            162317,  // Gorechop
            162329,  // Xav the Unfallen
            162309,  // Kul'tharok
            165946,  // Mordretha, the Endless Empress (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTheaterOfPainScript()
{
    return std::make_unique<TheaterOfPainScript>();
}

} // namespace Playerbot
