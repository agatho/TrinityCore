// ScarletMonasteryMoPScript — Scarlet Monastery (map 1004, MoP 87-90).
// MoP revamp of the Scarlet Monastery cathedral half (Scarlet Halls is
// the library/armory companion at map 1001).
//   * Thalnos the Soulrender — Channel Soul Sever + Reanimating Skeletons.
//   * Brother Korloff — Burning Fists + Inflammatory Aura.
//   * High Inquisitor Whitemane (final) — Mass Resurrection on Mograine
//     (kill priority shift) + Powerful Smite.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ScarletMonasteryMoPScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1004; }   // DB-verified: Thalnos/Korloff/Durand/Whitemane spawn on 1004; 1001 is Scarlet Halls
    char const* name() const override { return "scarlet_monastery_mop"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            59930,   // Empowered Zombie (Thalnos summon)
            59884,   // Fallen Crusader (Thalnos summon)
            59705,   // Scarlet Flamethrower (Korloff gauntlet, spawns on 1004)
        };
        a.mandatory_interrupt_spells = {
            115297,  // Mass Resurrection (Whitemane — must interrupt)
            115297,
            114925,  // Powerful Smite (Whitemane)
            114866,  // Soul Sever (Thalnos)
            115161,  // Burning Fists (Korloff)
        };
        a.cc_priority_entries = {
            59930,   // Empowered Zombie
            59884,   // Fallen Crusader
        };
        a.dangerous_auras = {
            114866,  // Soul Sever ground
            115161,  // Burning Fists pool
            115161,
            115161,
            // Progression waypoints — SM MoP revamp (Cathedral wing).
            // Bots default to Cathedral path; Library is a separate
            // dungeon (ScarletHallsScript).
            // (waypoints defined below outside the aura list)
        };
        // Boss progression — MoP Scarlet Monastery has 3 encounters.
        // Whitemane resurrects Durand — both entries listed.
        // Entries DB-verified as spawns on map 1004.
        a.bosses = {
            59789,  // Thalnos the Soulrender
            59223,  // Brother Korloff
            60040,  // Commander Durand (duo with Whitemane)
            3977,   // High Inquisitor Whitemane (final)
        };
        // Progression waypoints — DB-truth boss spawn positions on map 1004
        // (world.creature), one per encounter in order. Old values sat at
        // y~1380-1400, 700+ yards outside every actual spawn (fabricated).
        a.progression_waypoints = {
            { 1124.7f,  688.5f,   1.3f },   // Thalnos the Soulrender
            {  858.5f,  598.6f,  10.1f },   // Brother Korloff
            {  748.0f,  606.0f,  15.1f },   // Commander Durand
            {  700.5f,  605.8f,  11.6f },   // High Inquisitor Whitemane (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeScarletMonasteryMoPScript()
{
    return std::make_unique<ScarletMonasteryMoPScript>();
}

} // namespace Playerbot
