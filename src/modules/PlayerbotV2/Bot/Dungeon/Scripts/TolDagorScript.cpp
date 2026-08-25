// TolDagorScript — Tol Dagor (map 1771, BfA 110-120).
// Kul Tiran prison dungeon.
//   * The Sand Queen — Blinding Sand + Sand Trap.
//   * Knight Captain Valyri — Heartstopper Venom + adds.
//   * Jes Howlis (mid) — Howling Fear AoE.
//   * Overseer Korgus (final) — Igniting Charge (zone) + Heartstopper Venom.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TolDagorScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1771; }
    char const* name() const override { return "tol_dagor"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            130028,  // Ashvane Priest (healer caster trash)
            136665,  // Ashvane Spotter (calls reinforcements)
        };
        a.mandatory_interrupt_spells = {
            257791,  // Howling Fear (Jes Howlis)
            257791,
            257397,  // Heartstopper Venom (Valyri)
            258128,  // Debilitating Shout (Korgus)
            258313,  // Handcuff (Stillwater)
        };
        a.cc_priority_entries = {
            130028,
            136665,
        };
        a.dangerous_auras = {
            258128,  // Igniting Charge zone (Korgus)
            257791,  // Howling Fear AoE
            260729,  // Cursed Chest (Stillwater)
        };
        // Boss progression — Tol Dagor has 4 encounters.
        // Progression waypoints — Tol Dagor is a Tiragarde prison
        // with vertical cell tiers.
        a.progression_waypoints = {
            { 1067.0f,  -283.0f,   77.0f },   // entry
            { 1085.0f,  -331.0f,   77.0f },   // Sand Queen / Valyri intro
            { 1129.0f,  -397.0f,   58.0f },   // Jes Howlis cellblock
            { 1213.0f,  -377.0f,  131.0f },   // Stillwater warden hall
            { 1233.0f,  -240.0f,  171.0f },   // Korgus tower top
        };
        a.bosses = {
            127479,  // The Sand Queen
            127490,  // Knight Captain Valyri
            127484,  // Jes Howlis
            127503,  // Overseer Korgus (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTolDagorScript()
{
    return std::make_unique<TolDagorScript>();
}

} // namespace Playerbot
