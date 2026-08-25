// LostCityOfTolvirScript — Lost City of the Tol'vir (map 755, Cata 84-85).
// Uldum. 4 bosses: General Husam, Lockmaw + Augh, High Prophet Barim,
// Siamat.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/LostCityOfTheTolvir/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class LostCityOfTolvirScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 755; }
    char const* name() const override { return "lost_city_of_tolvir"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            44929,  // Crocolisk (Lockmaw)
            44320,  // Soul Fragment (Barim phase 2)
        };
        a.mandatory_interrupt_spells = {
            // General Husam
            83654,  // Hammer Fist
            83445,  // Shockwave
            83122,  // Throw Land Mines
            83113,  // Bad Intentions
            83236,  // Hurl
            // Lockmaw
            81630,  // Viscous Poison
            81690,  // Scent of Blood
            81652,  // Dust Flail
            // Augh
            84799,  // Paralytic Blow Dart
            84768,  // Smoke Bomb
            84784,  // Whirlwind (Augh)
            83776,  // Dragon's Breath
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Husam
            83644,  // Mystic Trap
            83171,  // Mystic Trap Damage
            85523,  // Land Mine Periodic
            83454,  // Shockwave Damage
            // Lockmaw
            81706,  // Venomous Rage
            81646,  // Dust Flail Periodic
            91415,  // Frenzy
            91408,  // Whirlwind Boss
        };
        // Boss progression — NPC entries from TC's lost_city_of_the_tolvir.h.
        a.bosses = {
            44577,  // General Husam
            43614,  // Lockmaw
            43612,  // High Prophet Barim
            44819,  // Siamat (final)
        };
        // Progression waypoints — LCT is an Uldum outdoor desert ruins.
        a.progression_waypoints = {
            { -10797.0f, -1696.0f,  16.0f },   // entry
            { -10708.0f, -1747.0f,  16.0f },   // Husam temple
            { -10645.0f, -1858.0f,  16.0f },   // Lockmaw pit
            { -10727.0f, -1986.0f,  20.0f },   // Barim plaza
            { -10817.0f, -1953.0f,  43.0f },   // Siamat altar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeLostCityOfTolvirScript()
{
    return std::make_unique<LostCityOfTolvirScript>();
}

} // namespace Playerbot
