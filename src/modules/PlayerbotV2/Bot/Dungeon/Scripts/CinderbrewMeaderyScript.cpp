// CinderbrewMeaderyScript — Cinderbrew Meadery (map 2649, TWW 70-80).
// Isle of Dorn kobold meadery — fire-themed.
//   * Brew Master Aldryr — Throw Cinderbrew (zone) + Cash Cannon.
//   * I'pa — Bee-stial Wrath + Honey Marinade (debuff).
//   * Benk Buzzbee — Honeypot adds.
//   * Goldie Baronbottom (final) — Burning Brew (zone) + Bee Swarm.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class CinderbrewMeaderyScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2661; }   // Cinderbrew Meadery (was cross-wired to Priory 2649; audit B32, DB-verified: Brew Master Aldryr on 2661)
    char const* name() const override { return "cinderbrew_meadery"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // mandatory_interrupt_spells previously contained 441627 SEVEN
        // times (no real encounter has one spell ID across 7 of 8 slots —
        // this was placeholder fill). dangerous_auras also reused 441627
        // for two distinct effects (Cinderbrew pool + Burning Brew —
        // those are different spells). Cleared until upstream TC source
        // exposes real IDs OR live spell sniffing captures them.
        // The kill / cc priorities are plausible-looking unverified
        // entries — left in but flagged. Generic interrupt rules still
        // fire on visible Role::Caster enemies.
        a.high_priority_kill_entries = {
            210164,  // Bee Hive (Goldie) — UNVERIFIED
            218338,  // Honey Bee (Benk)  — UNVERIFIED
            210264,  // Cinderbrew Lackey — UNVERIFIED
        };
        a.cc_priority_entries = {
            210264,
            218338,
        };
        // Boss progression — Cinderbrew Meadery has 4 encounters.
        a.bosses = {
            210271,  // Brew Master Aldryr
            218671,  // I'pa
            218002,  // Benk Buzzbee
            218523,  // Goldie Baronbottom (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeCinderbrewMeaderyScript()
{
    return std::make_unique<CinderbrewMeaderyScript>();
}

} // namespace Playerbot
