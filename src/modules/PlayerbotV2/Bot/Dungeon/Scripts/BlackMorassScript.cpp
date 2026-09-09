// BlackMorassScript — The Black Morass / Caverns of Time 1 (map 269,
// TBC 66-72). 18-wave portal defense. Aeonus is final boss; Medivh NPC
// must survive.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/CavernsOfTime/TheBlackMorass/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackMorassScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 269; }
    char const* name() const override { return "black_morass"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17839,  // Rift Keeper / Rift Lord
            21104,  // Time Rift Add
        };
        a.mandatory_interrupt_spells = {
            // Aeonus
            40504,  // Cleave
            31422,  // Time Stop
            31473,  // Sand Breath
            // Chrono Lord Deja
            31457,  // Arcane Blast
            31472,  // Arcane Discharge
            31467,  // Time Lapse
            // Temporus
            31458,  // Haste
            31464,  // Mortal Wound
            31475,  // Wing Buffet
        };
        a.cc_priority_entries = {
            17839,
        };
        a.dangerous_auras = {
            // Aeonus
            37605,  // Enrage
            // Temporus / Chrono Lord
            38540,  // Attraction (heroic)
            38592,  // Reflect (heroic)
        };
        // Boss progression — NPC entries from TC's the_black_morass.h.
        // Correct template IDs; bosses are event-summoned at the time
        // rifts (0 spawn rows on map 269) — navigator falls back to
        // waypoints.
        a.bosses = {
            17879,  // Chrono-Lord Deja
            17880,  // Temporus
            17881,  // Aeonus (final)
        };
        // Progression waypoints — Black Morass is a wave-defense
        // event around the Caverns of Time portal. Bots stay near
        // the central portal anchor between waves.
        a.progression_waypoints = {
            { -2010.0f, 7110.0f,  30.0f },   // entry
            { -2034.0f, 7104.0f,  30.0f },   // central portal anchor
            { -2055.0f, 7115.0f,  30.0f },   // alternative defense pos
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackMorassScript()
{
    return std::make_unique<BlackMorassScript>();
}

} // namespace Playerbot
