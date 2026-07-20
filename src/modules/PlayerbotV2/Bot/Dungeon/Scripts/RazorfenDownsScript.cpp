// RazorfenDownsScript — Razorfen Downs (map 129, vanilla 32-43).
// Quilboar / undead. Bosses: Tuten'kash, Glutton, Mordresh Fire Eye,
// Amnennar the Coldbringer, Plaguemaw the Rotting, Ragglesnout.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/RazorfenDowns/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RazorfenDownsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 129; }
    char const* name() const override { return "razorfen_downs"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            7458,   // Spider Egg (Tuten'kash)
            7459,   // Skeletal Frostweaver
        };
        a.mandatory_interrupt_spells = {
            // Amnennar the Coldbringer
            13009,  // Amnennar's Wrath
            15530,  // Frostbolt
            15531,  // Frost Nova
            12642,  // Frost Spectres
            // Glutton
            12627,  // Disease Cloud
            // Mordresh Fire Eye
            12466,  // Fireball
            12470,  // Fire Nova
            // Tuten'kash
            8876,   // Thrash
            12252,  // Web Spray
            12254,  // Virulent Poison
            12255,  // Curse of Tuten'kash
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            12795,  // Frenzy
        };
        // Boss progression — Razorfen Downs has 5 encounters.
        a.bosses = {
            7355,   // Tuten'kash
            8567,   // Glutton
            7356,   // Plaguemaw the Rotting
            7357,   // Mordresh Fire Eye
            7358,   // Amnennar the Coldbringer (final)
            7354,   // Ragglesnout (rare side spawn)
        };
        // Progression waypoints — Razorfen Downs is a quilboar-undead
        // outdoor dungeon: spiral path up to Amnennar's throne.
        a.progression_waypoints = {
            { 2592.0f,  962.0f,  53.0f },   // entry
            { 2614.0f, 1014.0f,  41.0f },   // Tuten'kash
            { 2647.0f, 1089.0f,  36.0f },   // Glutton pit
            { 2522.0f, 1100.0f,  47.0f },   // Plaguemaw altar
            { 2480.0f, 1135.0f,  46.0f },   // Mordresh skull
            { 2521.0f, 1226.0f,  49.0f },   // Amnennar throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRazorfenDownsScript()
{
    return std::make_unique<RazorfenDownsScript>();
}

} // namespace Playerbot
