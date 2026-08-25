// LowerBlackrockSpireScript — LBRS (map 229, vanilla 55-60).
// Bosses: Highlord Omokk, Shadow Hunter Vosh'gajin, War Master Voone,
// Mother Smolderweb, Urok Doomhowl, Quartermaster Zigris, Halycon,
// Gizrul the Slavener, Overlord Wyrmthalak.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/BlackrockMountain/BlackrockSpire/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class LowerBlackrockSpireScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 229; }
    char const* name() const override { return "lbrs"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            10375,  // Spire Spiderling (summoned by Mother Smolderweb, spell 16103)
            9218,   // Spirestone Battle Lord
            10601,  // Urok Enforcer (Urok Doomhowl event summon)
        };
        a.mandatory_interrupt_spells = {
            // Highlord Omokk
            23462,  // Fire Nova
            20691,  // Cleave
            16805,  // Conflagration
            15548,  // Thunderclap
            // Mother Smolderweb
            16104,  // Crystallize
            16468,  // Mother's Milk
            16103,  // Summon Spire Spiderling
            // Gizrul
            16128,  // Infected Bite
            // Quartermaster Zigris
            10101,  // Knock Away
            // Pyroguard Emberseer
            17274,  // Pyroblast
            23341,  // Flame Buffet
            15580,  // Strike
        };
        a.cc_priority_entries = {
            9033,   // UNVERIFIED: 9033 = General Angerforge (a Blackrock DEPTHS boss) —
                    // authored intent unknown, no LBRS counterpart derivable; needs review.
            10374,  // Spire Spider (12 spawns on map 229; was 10324, a nonexistent entry)
        };
        a.dangerous_auras = {
            // Halycon / dragon adds
            16167,  // Rend Mounts
            16359,  // Corrosive Acid
            16390,  // Flame Breath
            // Smolderweb / general
            8269,   // Frenzy
            16495,  // Fatal Bite
            13738,  // Rend
            3391,   // Thrash
            // Emberseer
            13376,  // Fire Shield
            23462,  // Fire Nova
        };
        // Boss progression — entries from TC's blackrock_spire.h.
        a.bosses = {
            9196,   // Highlord Omokk
            9236,   // Shadow Hunter Vosh'gajin
            9237,   // War Master Voone
            10596,  // Mother Smolderweb
            10584,  // Urok Doomhowl
            9736,   // Quartermaster Zigris
            10220,  // Halycon
            10268,  // Gizrul the Slavener
            9568,   // Overlord Wyrmthalak (final)
            9816,   // Pyroguard Emberseer (intro / event)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 229), in encounter order. Urok Doomhowl and Gizrul are
        // event/script summons with no creature rows and are skipped;
        // the pathfinder routes the corridors between waypoints.
        a.progression_waypoints = {
            {  -22.8f, -300.7f,   31.8f },   // Highlord Omokk
            { -121.2f, -482.2f,   24.7f },   // Shadow Hunter Vosh'gajin
            {  -17.0f, -459.1f,  -18.6f },   // War Master Voone
            { -135.5f, -565.8f,   10.2f },   // Mother Smolderweb
            { -194.2f, -458.3f,   87.6f },   // Quartermaster Zigris
            { -193.9f, -338.1f,   64.5f },   // Halycon
            {  -27.7f, -486.6f,   90.8f },   // Overlord Wyrmthalak (final)
            {  144.4f, -258.0f,   96.4f },   // Pyroguard Emberseer (event)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeLowerBlackrockSpireScript()
{
    return std::make_unique<LowerBlackrockSpireScript>();
}

} // namespace Playerbot
