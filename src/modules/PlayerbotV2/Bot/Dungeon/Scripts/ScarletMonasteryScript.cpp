// ScarletMonasteryScript — Scarlet Monastery (map 189, vanilla 28-40).
// Four wings (Graveyard/Library/Armory/Cathedral). Bosses include
// Whitemane + Mograine duo, Headmaster Houndmaster Loksey, Arcanist
// Doan, Bloodmage Thalnos, Herod, Interrogator Vishas, Headless
// Horseman (event).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/ScarletMonastery/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ScarletMonasteryScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1004; }   // Scarlet Monastery MoP remake (189 = classic-era SM; audit B31)
    char const* name() const override { return "scarlet_monastery"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            60033,  // Frenzied Spirit (Thalnos the Soulrender adds)
        };
        a.mandatory_interrupt_spells = {
            // Arcanist Doan
            8988,   // Silence
            9433,   // Arcane Explosion
            9435,   // Detonation
            9438,   // Arcane Bubble
            13323,  // Polymorph
            // Bloodmage Thalnos
            17831,  // Call of the Grave
            7399,   // Terrify
            7290,   // Soul Siphon
            // Herod
            8053,   // Flame Shock
            1106,   // Shadowbolt
            8814,   // Flame Spike
            16079,  // Fire Nova
        };
        a.cc_priority_entries = {
            58590,  // Scarlet Zealot (heals)
            58569,  // Scarlet Purifier (Flash of Steel / Purifying Flames)
        };
        a.dangerous_auras = {
            // Headless Horseman event
            42413,  // Head Visual
            42587,  // Cleave
            42380,  // Conflagration
            43116,  // Horseman's Whirlwind
        };
        // Boss progression — map 1004 is the MoP-remake Scarlet
        // Monastery (Graveyard + Cathedral). Classic-era boss entries
        // (Loksey/Doan/Herod/Mograine, old map 189) have no spawns
        // here; the remake roster is spawn-verified on map 1004.
        a.bosses = {
            59789,  // Thalnos the Soulrender (Graveyard)
            59223,  // Brother Korloff (training grounds)
            60040,  // Commander Durand (Cathedral, duo with Whitemane)
            3977,   // High Inquisitor Whitemane (final)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 1004), in encounter order. Coarse route skeleton; the
        // pathfinder handles corridors between them.
        a.progression_waypoints = {
            { 1124.7f,  688.5f,   1.3f },   // Thalnos the Soulrender (Graveyard)
            {  858.5f,  598.6f,  10.1f },   // Brother Korloff (training grounds)
            {  748.0f,  606.0f,  15.1f },   // Commander Durand (Cathedral)
            {  700.5f,  605.8f,  11.6f },   // High Inquisitor Whitemane (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeScarletMonasteryScript()
{
    return std::make_unique<ScarletMonasteryScript>();
}

} // namespace Playerbot
