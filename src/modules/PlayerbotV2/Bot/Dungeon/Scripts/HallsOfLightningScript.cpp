// HallsOfLightningScript — Halls of Lightning (map 602, WotLK 78-80).
// 4 bosses: General Bjarngrim, Volkhan, Ionar, Loken.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Ulduar/HallsOfLightning/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfLightningScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 602; }
    char const* name() const override { return "halls_of_lightning"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            28586,  // Molten Golem (Volkhan)
            28587,  // Brittle Golem (Volkhan)
            28528,  // Spark of Ionar
        };
        a.mandatory_interrupt_spells = {
            // Bjarngrim
            52029,  // Knock Away
            36096,  // Spell Reflection
            16856,  // Mortal Strike
            52027,  // Whirlwind
            52026,  // Slam
            15284,  // Cleave
            58769,  // Intercept
            // Volkhan
            52237,  // Shattering Stomp
            52387,  // Heat
            // Ionar
            52780,  // Ball Lightning
            52658,  // Static Overload
            // Loken
            52921,  // Arc Lightning
            52960,  // Lightning Nova
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Loken — close melee actually safer than range
            59414,  // Pulsing Shockwave Aura (stay close!)
            52960,  // Lightning Nova radial damage
            // Volkhan
            52237,  // Shattering Stomp ground
            // Bjarngrim
            52092,  // Temporary Electrical Charge (debuff)
            52098,  // Charge Up
            59085,  // Arc Weld
            59097,  // Arc Weld damage
        };
        // Boss progression — Halls of Lightning has 4 encounters.
        a.bosses = {
            28586,  // General Bjarngrim
            28587,  // Volkhan
            28546,  // Ionar
            28923,  // Loken (final)
        };
        // Progression waypoints — HoL is a Storm Peaks Titan dungeon.
        a.progression_waypoints = {
            { 1308.0f, -88.0f,  56.4f },   // entry
            { 1305.0f,  225.0f,  68.0f },   // Bjarngrim platform
            { 1290.0f,  490.0f,  60.0f },   // Volkhan forge
            { 1051.0f,  513.0f,  19.0f },   // Ionar arena
            {  852.0f,  554.0f, 105.0f },   // Loken's altar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfLightningScript()
{
    return std::make_unique<HallsOfLightningScript>();
}

} // namespace Playerbot
