// GundrakScript — Gundrak (map 604, WotLK 76-80).
// 4 bosses + Eck (heroic): Slad'ran, Drakkari Colossus, Moorabi,
// Gal'darah, plus Eck the Ferocious (heroic-only).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Gundrak/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GundrakScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 604; }
    char const* name() const override { return "gundrak"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            29682,  // Slad'ran Constrictor (snake add)
            29683,  // Slad'ran Viper
        };
        a.mandatory_interrupt_spells = {
            // Slad'ran (snake boss)
            54849,  // Mojo Volley
            // Drakkari Colossus / Drakkari Elemental
            54801,  // Surge (charge cast)
            54719,  // Mighty Blow
            55627,  // Mojo Puddle (elemental phase)
            55626,  // Mojo Wave
            // Moorabi
            54956,  // Impaling Charge
            55292,  // Stomp
            55276,  // Puncture
            55250,  // Whirling Slash
            // Gal'darah (final)
            55218,  // Stampede
        };
        a.cc_priority_entries = {
            29682,
            29683,
        };
        a.dangerous_auras = {
            // Moorabi
            54956,  // Impaling Charge telegraph
            55218,  // Stampede ground
            // Gal'darah
            55285,  // Enrage
            55250,  // Whirling Slash zone
            // Eck (heroic)
            55814,  // Eck Spit cone
            55813,  // Eck Bite
        };
        // Boss progression — NPC entries from TC's gundrak.h.
        a.bosses = {
            29304,  // Slad'ran
            29305,  // Moorabi
            29307,  // Drakkari Colossus
            29932,  // Eck the Ferocious (heroic-only optional)
            29306,  // Gal'darah (final)
        };
        // Progression waypoints — Gundrak is a 3-altar troll dungeon.
        a.progression_waypoints = {
            { 1788.0f,  663.0f,  -57.0f },   // entry plaza
            { 1639.0f,  643.0f,  -78.0f },   // Slad'ran altar
            { 1858.0f,  592.0f,  -64.5f },   // Moorabi altar
            { 1779.0f,  778.0f,  -71.0f },   // Colossus altar
            { 1647.0f,  759.0f,  -90.0f },   // Eck pool (heroic)
            { 1769.0f,  570.0f, -119.0f },   // Gal'darah arena
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGundrakScript()
{
    return std::make_unique<GundrakScript>();
}

} // namespace Playerbot
