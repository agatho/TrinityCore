// MechanarScript — The Mechanar (map 554, TBC 67-72).
// Tempest Keep wing. 4 bosses: Gatewatcher Gyro-Kill, Gatewatcher
// Iron-Hand, Mechano-Lord Capacitus, Nethermancer Sepethrea,
// Pathaleon the Calculator.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/TempestKeep/Mechanar/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MechanarScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 554; }
    char const* name() const override { return "mechanar"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            21105,  // Ragin' Flames (Sepethrea)
        };
        a.mandatory_interrupt_spells = {
            // Gatewatcher (Gyro-Kill / Iron-Hand)
            35326,  // Hammer Punch
            35327,  // Jackhammer
            35318,  // Saw Blade
            // Capacitus
            35161,  // Headcrack
            35158,  // Reflective Magic Shield
            35159,  // Reflective Damage Shield
            39096,  // Polarity Shift
            // Sepethrea
            45196,  // Frost Attack
            35314,  // Arcane Blast
            35275,  // Summon Raging Flames
            // Pathaleon
            36021,  // Mana Tap
            36022,  // Arcane Torrent
            35280,  // Domination (mind control)
            35034,  // Arcane Missiles (Nether Wraith)
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Capacitus
            35311,  // Stream of Machine Fluid (ground)
            35150,  // Nether Charge passive
            39089,  // Positive Charge Stack
            39092,  // Negative Charge Stack
            // Sepethrea
            35250,  // Dragon's Breath
            35268,  // Inferno
            35283,  // Inferno Damage
            35281,  // Raging Flames Area Aura
            // Pathaleon
            36992,  // Frenzy
        };
        // Boss progression — Mechanar has 4 bosses.
        a.bosses = {
            19218,  // Gatewatcher Gyro-Kill
            19710,  // Gatewatcher Iron-Hand
            19219,  // Mechano-Lord Capacitus
            19221,  // Nethermancer Sepethrea
            19220,  // Pathaleon the Calculator (final)
        };
        // Progression waypoints — Mechanar is the Tempest Keep west
        // satellite, a vertical factory with lifts between floors.
        a.progression_waypoints = {
            {   30.6f,   25.0f,   1.5f },   // entry
            {  -42.0f,    1.0f,   3.0f },   // Gyro-Kill
            {  120.0f,  -27.0f,   3.0f },   // Iron-Hand corridor
            {  178.0f, -130.0f,  18.0f },   // Capacitus arena
            {  262.0f, -176.0f,  39.0f },   // Sepethrea hall
            {  329.0f, -240.0f,  77.0f },   // Pathaleon top
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMechanarScript()
{
    return std::make_unique<MechanarScript>();
}

} // namespace Playerbot
