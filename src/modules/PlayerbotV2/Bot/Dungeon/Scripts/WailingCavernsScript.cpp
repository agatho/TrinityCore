// WailingCavernsScript — Wailing Caverns (map 43, vanilla 17-27).
// Druid-themed Kalimdor cave dungeon. Notable:
//   * Lord Cobrahn shapeshifts into Snake Form mid-fight; while in
//     snake form he gains poison + faster attacks. Ranged DPS keep
//     distance; melee should not be in front of him.
//   * Lady Anacondra heals other Druids of the Fang adds (CC her or
//     burn her quickly).
//   * Kresh — random patrolling Devilsaur-style turtle; high HP,
//     no special mechanic.
//   * Skum — pull from Lord Pythas; chain-pull risk.
//   * Mutanus the Devourer (final) — hard-hitter; tank-and-spank.
// Generic logic clears most of this. Script value:
//   - Kill priority: Druids of the Fang (entry 3850, 3851, 3852)
//     first because Lady Anacondra heals them.
//   - CC priority: Lady Anacondra (3669) because she heals.
//   - Cobrahn's Mind Spike (8133) is interruptible and worth kicking.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class WailingCavernsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 43; }
    char const* name() const override { return "wailing_caverns"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            3850,   // Druid of the Fang
            3851,   // Druid of the Fang (variant)
            3852,   // Druid of the Fang (variant)
        };
        a.mandatory_interrupt_spells = {
            8133,   // Mind Spike (Cobrahn) — interruptible nuke
            8040,   // Sleep (Lady Anacondra) — disables a party member
        };
        a.cc_priority_entries = {
            3669,   // Lady Anacondra
        };
        a.dangerous_auras = {};
        // Progression waypoints — WC is a winding druid-themed cave;
        // tank walks an outer spiral hitting each Druid of the Fang
        // chamber in turn before Mutanus at the deep end. Coords from
        // public dumps; Detour validates so off-mesh points stall.
        a.progression_waypoints = {
            { -161.0f,  130.0f, -78.0f },   // entry corridor
            { -141.0f,  186.0f, -76.0f },   // first Druid chamber (Cobrahn)
            {  -82.0f,  152.0f, -75.0f },   // Pythas chamber
            {  -45.0f,  103.0f, -75.0f },   // Serpentbloom turn
            { -109.0f,   60.0f, -77.0f },   // Anacondra chamber
            { -200.0f,  131.0f, -76.0f },   // back to entrance hub
            { -160.0f,  175.0f, -76.0f },   // Mutanus's lake — final
        };
        // Boss progression order (encounter sequence). Used by the
        // tank-advance rule for cross-room navigation when no enemies
        // are in the snapshot's nearby_enemies range — the tank
        // path-finds toward the first live boss it can reach.
        a.bosses = {
            3669,   // Lord Cobrahn
            3671,   // Lady Anacondra
            3670,   // Lord Pythas
            3674,   // Skum
            3673,   // Lord Serpentis
            3654,   // Mutanus the Devourer (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeWailingCavernsScript()
{
    return std::make_unique<WailingCavernsScript>();
}

} // namespace Playerbot
