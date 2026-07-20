// TrialOfChampionScript — Trial of the Champion (map 650, WotLK 80).
// Coliseum 5-man (precursor to ToC raid).
//   * Mounted Combat phase — 3 vs 3 jousting; bots use lance abilities.
//   * Eadric the Pure / Argent Confessor Paletress (random) —
//     Eadric: Radiance (interrupt), Holy Wrath (telegraphed cone).
//     Paletress: Memory of Mistress (add) — priority kill.
//   * Black Knight (final) — undead phases (Plague Nova).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TrialOfChampionScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 650; }
    char const* name() const override { return "trial_of_champion"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            34935,  // Memory of Mistress (Paletress add)
        };
        a.mandatory_interrupt_spells = {
            66935,  // Radiance (Eadric)
            66940,  // Holy Smite (Eadric)
            67517,  // Renew (Paletress)
            66881,  // Plague Nova (Black Knight)
            67518,  // Holy Fire (Paletress)
        };
        a.dangerous_auras = {
            66940,  // Holy Wrath cone
            66881,  // Plague Nova range
        };
        // Boss progression — Trial of the Champion has 3 encounters.
        // The Grand Champions / Eadric+Paletress / Black Knight progression
        // is faction-dependent (different Champions vs Horde/Alliance).
        // Progression waypoints — Trial of the Champion is a single
        // arena: bots stay near center for all 3 encounters (the
        // Champions ride in, Eadric/Paletress spawn at center,
        // Black Knight rises from the rubble).
        a.progression_waypoints = {
            { 743.0f, 619.0f, 411.0f },   // entry tunnel
            { 743.0f, 632.0f, 411.0f },   // arena center
        };
        // Correct template IDs; ALL ToC bosses are event-summoned into
        // the arena (0 spawn rows on map 650) — navigator falls back
        // to waypoints.
        a.bosses = {
            34705,  // Grand Champions encounter (lead - Marshal Jacob Alerius)
            35119,  // Eadric the Pure (random one slot)
            34928,  // Argent Confessor Paletress (random one slot)
            35451,  // The Black Knight (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTrialOfChampionScript()
{
    return std::make_unique<TrialOfChampionScript>();
}

} // namespace Playerbot
