// PrioryOfTheSacredFlameScript — Priory of the Sacred Flame (map 2652,
// TWW 70-80). Hallowfall priory turned undead by Arathi heresy.
//   * Captain Dailcry — Battle Cry + Castigate.
//   * Baron Braunpyke — Sacrificial Pyre + Holy Fire.
//   * Prioress Murrpray (final) — Lethal Current + Inner Fire.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class PrioryOfTheSacredFlameScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2649; }   // Priory of the Sacred Flame (was cross-wired to Stonevault 2652; audit B32, DB-verified: Prioress Murrpray on 2649)
    char const* name() const override { return "priory_of_the_sacred_flame"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            207946,  // Sacred Templar (Murrpray adds)
            213165,  // Lightspawn (Braunpyke)
            207944,  // Risen Mage
        };
        a.mandatory_interrupt_spells = {
            424421,  // Holy Fire (Braunpyke)
            424414,  // Lethal Current (Murrpray)
            424414,
            424421,
            424414,
            424414,
        };
        a.cc_priority_entries = {
            207944,
            213165,
        };
        a.dangerous_auras = {
            424421,  // Holy Fire pool
            424414,  // Lethal Current zone
            424421,
        };
        // Boss progression — Priory of the Sacred Flame has 3 encounters.
        a.bosses = {
            207946,  // Captain Dailcry
            207939,  // Baron Braunpyke
            207940,  // Prioress Murrpray (final)
        };
        // Progression waypoints — Priory of the Sacred Flame is a
        // Hallowfall priory with outdoor courtyard + chapel interior.
        a.progression_waypoints = {
            { -485.0f,  580.0f, 1965.0f },   // entry courtyard
            { -413.0f,  658.0f, 1968.0f },   // Dailcry plaza
            { -358.0f,  730.0f, 1975.0f },   // Braunpyke chapel
            { -288.0f,  800.0f, 1990.0f },   // Murrpray inner sanctum
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakePrioryOfTheSacredFlameScript()
{
    return std::make_unique<PrioryOfTheSacredFlameScript>();
}

} // namespace Playerbot
