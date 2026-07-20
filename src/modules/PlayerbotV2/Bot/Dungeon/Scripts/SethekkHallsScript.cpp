// SethekkHallsScript — Sethekk Halls (map 556, TBC 67-72).
// Auchindoun wing. 2 bosses: Darkweaver Syth, Talon King Ikiss.
// Optional: Anzu (raven god, summoned bonus boss).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/Auchindoun/SethekkHalls/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SethekkHallsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 556; }
    char const* name() const override { return "sethekk_halls"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            19203,  // Syth Fire elemental
            19204,  // Syth Frost elemental
            19205,  // Syth Arcane elemental
            19206,  // Syth Shadow elemental
        };
        a.mandatory_interrupt_spells = {
            // Darkweaver Syth
            21401,  // Frost Shock
            34354,  // Flame Shock
            30138,  // Shadow Shock
            37132,  // Arcane Shock
            15659,  // Chain Lightning
            // Anzu
            40184,  // Paralyzing Screech
            40303,  // Spell Bomb
            40321,  // Cyclone of Feathers
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Syth elemental buffets — debuffs / damage zones
            33526,  // Flame Buffet
            33527,  // Arcane Buffet
            33528,  // Frost Buffet
            33529,  // Shadow Buffet
            // Anzu
            40199,  // Flesh Rip
        };
        // Boss progression — entries from TC sethekk_halls.h + boss .cpp.
        // Anzu is a heroic-only bonus boss summoned via the Druid Flight
        // Form quest.
        a.bosses = {
            18472,  // Darkweaver Syth
            18473,  // Talon King Ikiss (final)
            23035,  // Anzu (heroic bonus)
        };
        // Progression waypoints — Sethekk Halls is the southern
        // Auchindoun wing: bird-themed linear path with two bosses.
        a.progression_waypoints = {
            { -149.0f,  140.0f,  -75.0f },   // entry
            {  -78.0f,  117.0f,  -75.0f },   // Syth's circle
            {   46.0f,  290.0f,  -89.0f },   // Ikiss antechamber
            {   90.0f,  402.0f,  -89.0f },   // Ikiss's perch
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSethekkHallsScript()
{
    return std::make_unique<SethekkHallsScript>();
}

} // namespace Playerbot
