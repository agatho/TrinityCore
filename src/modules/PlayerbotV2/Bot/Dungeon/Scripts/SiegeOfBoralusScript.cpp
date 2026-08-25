// SiegeOfBoralusScript — Siege of Boralus (map 1822, BfA 110-120).
// Boralus harbor siege — faction-mirrored intro fight.
//   * Chopper Redhook (A) / Sergeant Bainbridge (H) — pre-cannon adds.
//   * Dread Captain Lockwood — Adaptive Camouflage + cannon barrage.
//   * Hadal Darkfathom — Shattering Bellow (cone) + Squallshapers.
//   * Viq'Goth (final) — Tentacle Strike + Slime Spit + Slimy Eruption.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SiegeOfBoralusScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1822; }
    char const* name() const override { return "siege_of_boralus"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            136549,  // Ashvane Cannoneer (ranged, Lockwood deck)
            138255,  // Ashvane Spotter (calls reinforcements)
            128969,  // Ashvane Commander
        };
        a.mandatory_interrupt_spells = {
            256849,  // Shattering Bellow (Hadal)
            257293,  // Suppressing Fire (Lockwood)
            257544,  // Lashing Tentacle (Viq'Goth)
            274715,  // Tempest cast (squallshaper)
        };
        a.cc_priority_entries = {
            129367,  // Bilge Rat Tempest (caster)
            138255,  // Ashvane Spotter
        };
        a.dangerous_auras = {
            257293,  // Suppressing Fire zone
            256849,  // Shattering Bellow cone
            257562,  // Slimy Eruption (Viq'Goth)
            257699,  // Slimy Spit pool
        };
        // Boss progression — Siege of Boralus has 4 encounters.
        a.bosses = {
            144160,  // Chopper Redhook (Horde mirror: Sergeant Bainbridge 128649)
            129208,  // Dread Captain Lockwood
            130836,  // Hadal Darkfathom
            128652,  // Viq'Goth (final)
        };
        // Progression waypoints — Siege of Boralus is a Kul Tiras
        // harbor with linear pier-to-keep progression.
        a.progression_waypoints = {
            { -100.0f,  -10.0f,  20.0f },   // entry pier
            {   30.0f,   55.0f,  20.0f },   // Bainbridge/Redhook plaza
            {  155.0f,  130.0f,  60.0f },   // Lockwood cannon hall
            {  255.0f,  195.0f,  85.0f },   // Hadal harbor floor
            {  370.0f,  255.0f,  85.0f },   // Viq'Goth ship deck
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSiegeOfBoralusScript()
{
    return std::make_unique<SiegeOfBoralusScript>();
}

} // namespace Playerbot
