// HallsOfStoneScript — Halls of Stone (map 599, WotLK 75-80).
// 3 bosses + Tribunal of Ages event: Krystallus, Maiden of Grief,
// Sjonnir the Ironshaper, Tribunal protection event.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Ulduar/HallsOfStone/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfStoneScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 599; }
    char const* name() const override { return "halls_of_stone"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            27978,  // Iron Sludge (Sjonnir adds)
            27977,  // Tribunal Dark Matter
        };
        a.mandatory_interrupt_spells = {
            // Krystallus
            50843,  // Boulder Toss
            // Sjonnir
            59723,  // Parting Sorrow
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Krystallus
            59750,  // Ground Spike
            50827,  // Ground Slam
            50810,  // Shatter
            50812,  // Stoned debuff
            48131,  // Stomp
            // Sjonnir
            28747,  // Frenzy
        };
        // Boss progression — Halls of Stone has 3 encounters.
        // Tribunal of Ages is an escort/protection event.
        a.bosses = {
            27977,  // Krystallus
            27975,  // Maiden of Grief
            27978,  // Sjonnir The Ironshaper (final)
        };
        // Progression waypoints — HoS is a Titan-themed linear dungeon.
        a.progression_waypoints = {
            {  716.0f,  519.0f, 110.0f },   // entry
            {  874.0f,  476.0f,  90.6f },   // Maiden of Grief
            { 1175.0f,  624.0f, 215.0f },   // Krystallus
            { 1326.0f,  638.0f, 209.0f },   // Tribunal of Ages event
            { 1322.0f,  906.0f, 207.0f },   // Sjonnir's forge
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfStoneScript()
{
    return std::make_unique<HallsOfStoneScript>();
}

} // namespace Playerbot
