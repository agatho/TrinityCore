// ScarletHallsScript — Scarlet Halls (map 878, MoP 87-90).
// MoP revamp of Scarlet Monastery upper halls.
//   * Houndmaster Braun — Battle Trained dog adds + Disorienting Bark.
//   * Armsmaster Harlan — Blades of Light + Heroic Strike.
//   * Flameweaver Koegler (final) — Greater Pyroblast + Drakes (book event).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ScarletHallsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1001; }   // Scarlet Halls (878 was the SM-graveyard guess; audit B31, DB-verified: Armsmaster Harlan on 1001)
    char const* name() const override { return "scarlet_halls"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            59303,   // Battle-Trained Hound (Braun)
            59243,   // Scarlet Adept (caster trash)
            59239,   // Scarlet Crusader Dragon (Koegler)
        };
        a.mandatory_interrupt_spells = {
            113563,  // Greater Pyroblast (Koegler)
            113669,  // Disorienting Bark (Braun)
            114030,  // Vengeance (Harlan)
            113563,
        };
        a.cc_priority_entries = {
            59303,
            59243,
        };
        a.dangerous_auras = {
            113563,  // Greater Pyroblast zone
            113669,  // Disorienting Bark cone
            114030,  // Heroic Strike telegraph
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 1001), in encounter order. Previous values were fabricated
        // (400-500y from every spawn; audit B34). Armsmaster Harlan is
        // script-spawned (no creature row) so his arena has no waypoint;
        // it lies on the Braun->Koegler route anyway.
        a.progression_waypoints = {
            { 1206.4f, 444.0f,  1.1f },   // Houndmaster Braun (58632)
            { 1308.0f, 549.1f, 12.9f },   // Flameweaver Koegler (59150)
        };
        // Boss progression — Scarlet Halls has 3 encounters.
        // DB-verified on map 1001: Braun is 59303, Harlan is 58632
        // (the old list had Harlan's entry on the Braun slot).
        a.bosses = {
            59303,  // Houndmaster Braun
            58632,  // Armsmaster Harlan
            59150,  // Flameweaver Koegler (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeScarletHallsScript()
{
    return std::make_unique<ScarletHallsScript>();
}

} // namespace Playerbot
