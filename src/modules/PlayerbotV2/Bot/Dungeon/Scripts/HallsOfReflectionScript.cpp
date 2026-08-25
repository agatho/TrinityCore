// HallsOfReflectionScript — Halls of Reflection (map 668, WotLK 80).
// Frozen Halls #3. Falric + Marwyn intermission waves, then Lich King
// escape chase.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/FrozenHalls/HallsOfReflection/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfReflectionScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 668; }
    char const* name() const override { return "halls_of_reflection"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            38061,  // Spiritual Reflection (Frostsworn General)
            38133,  // Phantom (intermission wave)
        };
        a.mandatory_interrupt_spells = {
            // Falric
            72422,  // Quivering Strike
            72426,  // Impending Despair
            72435,  // Defiling Horror
            // Marwyn
            72360,  // Obliterate
            72363,  // Corrupted Flesh
            72368,  // Shared Suffering
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Marwyn
            72362,  // Well of Corruption (ground zone)
            // Falric
            72435,  // Defiling Horror aura (channel)
        };
        // Boss progression — NPC entries from TC's halls_of_reflection.h.
        // The Lich King chase sequence isn't a killable boss — the run
        // ends when the player escapes — so only Falric+Marwyn surface.
        a.bosses = {
            38112,  // Falric
            38113,  // Marwyn
        };
        // Progression waypoints — HoR is a wave-defense room + escape.
        a.progression_waypoints = {
            { 5267.0f, 1990.0f, 707.7f },   // entry
            { 5311.0f, 2009.0f, 709.3f },   // Falric/Marwyn arena
            { 5403.0f, 2046.0f, 707.5f },   // post-fight escape ramp
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfReflectionScript()
{
    return std::make_unique<HallsOfReflectionScript>();
}

} // namespace Playerbot
