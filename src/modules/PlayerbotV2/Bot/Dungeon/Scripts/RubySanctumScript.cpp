// RubySanctumScript — Ruby Sanctum raid (map 724, WotLK 10/25).
// Final WotLK raid mini-zone. 4 bosses leading to Halion:
//   Baltharus the Warborn, General Zarithrian, Saviana Ragefire, Halion.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/ChamberOfAspects/RubySanctum/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RubySanctumScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 724; }
    char const* name() const override { return "ruby_sanctum"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            40683,  // Living Ember (Halion heroic add)
            40681,  // Living Inferno (Halion heroic add)
            40142,  // Twilight Halion (twilight realm half)
            39814,  // Onyx Flamecaller (Zarithrian adds)
            40083,  // Shadow Orb (Twilight Cutter source)
            39899,  // Baltharus the Warborn Clone
        };
        a.mandatory_interrupt_spells = {
            // Halion
            74525,  // Flame Breath
            74524,  // Cleave
            74637,  // Meteor Strike
            74531,  // Tail Lash
            // Baltharus
            74508,  // Enervating Smash
            74525,  // Cleave Spike Flurry
            // General Zarithrian
            74367,  // Cleave
            74369,  // Intimidating Roar
            // Saviana Ragefire
            74452,  // Conflagration
            74454,  // Flame Beacon
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Halion
            74567,  // Mark of Combustion
            74562,  // Fiery Combustion
            74795,  // Mark of Consumption
            74792,  // Soul Consumption
            74607,  // Fiery Combustion (debuff)
            74769,  // Twilight Cutter
            74641,  // Meteor Strike Damage
            // Baltharus
            74456,  // Enervating Brand
            74505,  // Repeating Spike Flurry
            // Zarithrian
            74367,  // Cleave
            // Saviana
            74455,  // Conflagration
            74452,  // Engulfing Flames
        };
        // Boss progression — NPC entries from TC's ruby_sanctum.h.
        a.bosses = {
            39751,  // Baltharus the Warborn
            39747,  // Saviana Ragefire
            39746,  // General Zarithrian
            39863,  // Halion (final, twin-phase encounter)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 724), in encounter order. Halion (39863) is summoned at the
        // central sanctum and has no creature row, so it is skipped.
        a.progression_waypoints = {
            { 3153.1f, 389.5f, 86.3f },  // Baltharus the Warborn
            { 3151.4f, 636.9f, 78.7f },  // Saviana Ragefire
            { 3049.7f, 528.1f, 89.5f },  // General Zarithrian
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRubySanctumScript()
{
    return std::make_unique<RubySanctumScript>();
}

} // namespace Playerbot
