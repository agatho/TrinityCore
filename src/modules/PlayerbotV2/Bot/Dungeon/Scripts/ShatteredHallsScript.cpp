// ShatteredHallsScript — The Shattered Halls (map 540, TBC 70).
// 3 bosses: Grand Warlock Nethekurse, Warbringer O'mrogg, Warchief
// Kargath Bladefist. Heroic-only: Blood Guard Porung.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/HellfireCitadel/ShatteredHalls/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ShatteredHallsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 540; }
    char const* name() const override { return "shattered_halls"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17296,  // Shattered Hand Sentry (Kargath adds)
        };
        a.mandatory_interrupt_spells = {
            // Nethekurse
            30500,  // Death Coil
            30496,  // Shadow Fissure (summon void zone)
            30495,  // Shadow Cleave
            30478,  // Hemorrhage
            30497,  // Consumption
            // O'mrogg
            30600,  // Blast Wave
            30584,  // Fear
            30633,  // Thunderclap
            30598,  // Burning Maul
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Nethekurse
            30496,  // Shadow Fissure zone
            // O'mrogg
            30598,  // Burning Maul ground
            // Kargath
            30739,  // Blade Dance (whirlwind)
        };
        // Boss progression — NPC entries from TC's shattered_halls.h.
        // Heroic-only: Blood Guard Porung (20923). Warbringer O'mrogg
        // is missing from the TC header but his entry is 16809.
        a.bosses = {
            16807,  // Grand Warlock Nethekurse
            20923,  // Blood Guard Porung (heroic only)
            16809,  // Warbringer O'mrogg
            16808,  // Warchief Kargath Bladefist (final)
        };
        // Progression waypoints — Shattered Halls is a 4-section orc
        // fortress: entry hall (Nethekurse) → execution courtyard
        // (heroic-only prisoner save event) → upper barracks
        // (O'mrogg) → grand arena (Kargath).
        a.progression_waypoints = {
            {  170.0f,   84.5f,  -16.2f },   // entry hall
            {  149.4f,  -71.0f,  -16.4f },   // Nethekurse altar
            {  117.5f, -163.0f,  -16.4f },   // execution courtyard
            {   16.0f,  -89.7f,    7.0f },   // upper barracks ramp
            {   13.0f,  -91.0f,    7.0f },   // O'mrogg's throne
            {  -83.7f,  -88.3f,    7.0f },   // grand arena (Kargath)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeShatteredHallsScript()
{
    return std::make_unique<ShatteredHallsScript>();
}

} // namespace Playerbot
