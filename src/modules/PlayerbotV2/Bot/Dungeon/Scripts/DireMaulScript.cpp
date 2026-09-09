// DireMaulScript — Dire Maul (map 429, vanilla 55-60+).
// Three wings: East (DM:E), West (DM:W), North (DM:N).
//   * DM:E — Pusillin chase (rogue, runs). Lethtendris's Imp Mojo
//     party-buff side-mechanic.
//   * DM:W — Tendris Warpwood AoE summon Whirlwind. Magister Doane
//     stacking armor; Illyanna Ravenoak Hunter's Mark.
//   * DM:N — Tribute run (multiple guard groups left alive grants
//     better loot — bots burn everything; suboptimal but completable).
//     King Gordok charges; Cho'Rush dispels boss buffs.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DireMaulScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 429; }
    char const* name() const override { return "dire_maul"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            14358,  // Smaller Tribute Generals (King Gordok adds)
            14323,  // Stomper Kreeg (DM:N caster)
        };
        a.mandatory_interrupt_spells = {
            22417,  // Mortal Strike (King Gordok / DM:N tribunes)
            22336,  // Whirlwind (Magister Doane)
            22456,  // Hunter's Mark (Illyanna Ravenoak)
            22640,  // Polymorph (DM:W casters mind-control)
        };
        a.cc_priority_entries = {
            11457,  // Dire Maul Druid trash (DM:W heals)
            11460,  // Skullcrusher Ogre Mage (DM:N caster)
        };
        a.dangerous_auras = {
            22425,  // Charge (King Gordok knockback)
        };
        // Boss progression — Dire Maul has 3 wings, ~17 encounters total.
        // East/West/North wings; tank-advance picks closest alive boss.
        a.bosses = {
            // East wing
            14354,  // Pusillin
            13280,  // Hydrospawn
            14327,  // Lethtendris
            11490,  // Zevrim Thornhoof
            11492,  // Alzzin the Wildshaper (East final)
            // West wing
            11489,  // Tendris Warpwood
            11487,  // Magister Kalendris
            11488,  // Illyanna Ravenoak
            11467,  // Tsu'zee
            11496,  // Immol'thar
            11486,  // Prince Tortheldrin (West final)
            // North wing
            14326,  // Guard Mol'dar
            14322,  // Stomper Kreeg
            14321,  // Guard Fengus
            14323,  // Guard Slip'kik
            14325,  // Captain Kromcrush
            14324,  // Cho'Rush the Observer
            11501,  // King Gordok (North final)
        };
        // Progression waypoints — Dire Maul has 3 separate wings
        // sharing one map_id. Default path covers the North wing
        // (King Gordok) which is the most-run for the tribute trick.
        a.progression_waypoints = {
            {  -36.0f,  340.0f,   25.0f },   // entry North
            {  -36.0f,  315.0f,   12.0f },   // ogre courtyard (Mol'dar)
            { -100.0f,  349.0f,   -5.0f },   // Fengus chamber
            { -190.0f,  328.0f,   -5.0f },   // Slip'kik (CC immune)
            { -100.0f,  269.0f,   -5.0f },   // Kromcrush + Cho'rush
            {  -36.0f,  269.0f,  -10.0f },   // Gordok throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDireMaulScript()
{
    return std::make_unique<DireMaulScript>();
}

} // namespace Playerbot
