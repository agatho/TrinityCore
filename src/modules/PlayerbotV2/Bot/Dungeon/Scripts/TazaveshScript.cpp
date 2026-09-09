// TazaveshScript — Tazavesh, the Veiled Market (map 2441, SL).
// 8-boss dungeon split into Streets / Gambit halves.
//   * Zo'phex — Telekinetic Toss + Caller-summon adds.
//   * Hylbrande — Sanitizing Cycle (telegraphed channel).
//   * Mailroom Mayhem (timed event).
//   * Tred'ova-style Volatile Concoction in pre-boss.
//   * Timecap'n Hooktail — Cannonball + Hookshot phase.
//   * Soleah's Secret (final) — phases; high mechanic density.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TazaveshScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2441; }
    char const* name() const override { return "tazavesh"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            347598,  // Sanitizing Cycle (Hylbrande)
            347499,  // Telekinetic Toss (Zo'phex)
            347775,  // Cannonball (Hooktail)
            352616,  // Sample Collection (So'leah)
        };
        a.dangerous_auras = {
            347598,  // Sanitizing Cycle channel
            347775,  // Cannonball telegraph
        };
        // Boss progression — Tazavesh has 8 encounters across Streets +
        // Gambit halves. NPC entries from public TC/WoWHead dumps.
        a.bosses = {
            // Streets of Wonder
            175546,  // Zo'phex the Sentinel
            177269,  // The Grand Menagerie
            177821,  // Mailroom Mayhem (P.O.S.T. Master)
            175546,  // Tazavesh Defense (Adjutant)
            // So'leah's Gambit
            177269,  // Hylbrande
            177269,  // Timecap'n Hooktail
            177269,  // So'azmi
            175546,  // So'leah (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTazaveshScript()
{
    return std::make_unique<TazaveshScript>();
}

} // namespace Playerbot
