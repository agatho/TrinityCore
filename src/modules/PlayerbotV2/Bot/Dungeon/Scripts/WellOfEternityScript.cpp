// WellOfEternityScript — Well of Eternity (map 939, Cata 4.3).
// Caverns of Time #5. Time-warp to Sundering era.
//   * Peroth'arn — Eyes of Peroth'arn (stealth phase; bots stay
//     out of LoS; eye adds reveal players).
//   * Queen Azshara — Stay of Execution (interrupt) + Wrath of
//     Azshara (telegraphed AoE).
//   * Mannoroth + Varo'then — chained twin fight; both die together.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class WellOfEternityScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 939; }
    char const* name() const override { return "well_of_eternity"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            55289,  // Eye of Peroth'arn (stealth detector add)
        };
        a.mandatory_interrupt_spells = {
            102931,  // Stay of Execution (Azshara)
            102933,  // Total Obedience (Azshara)
            102943,  // Wrath of Azshara (Azshara)
            102982,  // Mannoroth's Gaze (final) — fear
        };
        a.dangerous_auras = {
            102943,  // Wrath of Azshara zone
            102982,  // Mannoroth's Gaze zone
        };
        // Boss progression — Well of Eternity has 3 encounters.
        // Mannoroth+Varo'then is a chained twin fight; Mannoroth as lead.
        a.bosses = {
            55085,   // Peroth'arn
            54853,   // Queen Azshara
            54969,   // Mannoroth (with Varo'then, final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeWellOfEternityScript()
{
    return std::make_unique<WellOfEternityScript>();
}

} // namespace Playerbot
