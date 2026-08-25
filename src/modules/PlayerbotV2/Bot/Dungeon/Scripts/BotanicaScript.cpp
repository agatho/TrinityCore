// BotanicaScript — The Botanica (map 553, TBC 67-72).
// Tempest Keep botanical wing.
//   * Commander Sarannis — adds Brackenfern Sentinel (priority kill).
//   * High Botanist Freywinn — phases (Plant form root, Tree form
//     heals); CC priority for Tree form heal.
//   * Thorngrin the Tender — Hellfire (large AoE)→ stand far.
//   * Laj — phase shifts (water/fire/poison); resists swap.
//   * Warp Splinter (final) — Saplings (small adds, prio kill).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BotanicaScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 553; }
    char const* name() const override { return "botanica"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17091,  // Brackenfern Sentinel (Sarannis adds)
            17097,  // Warp Splinter Sapling (small adds)
        };
        a.mandatory_interrupt_spells = {
            34579,  // Tranquility (Freywinn Tree form heal)
            34580,  // Hellfire (Thorngrin)
            34587,  // Summoning Lasso (Warp Splinter pulls)
        };
        a.dangerous_auras = {
            34580,  // Hellfire — step away 30y
            34589,  // Toxic Pool (Laj)
        };
        // Boss progression — NPC entries from TC's the_botanica.h.
        a.bosses = {
            17976,  // Commander Sarannis
            17975,  // High Botanist Freywinn
            17978,  // Thorngrin the Tender
            17980,  // Laj
            17977,  // Warp Splinter (final)
        };
        // Progression waypoints — Botanica is the Tempest Keep east
        // satellite, a 4-floor vertical greenhouse. Bots ride lifts
        // (handled by use_game_object on lift GO) between floors;
        // waypoints land the tank in each floor's combat area.
        a.progression_waypoints = {
            {  152.4f,   17.6f, -22.5f },   // entry
            {  173.4f,  -57.5f, -22.5f },   // Sarannis platform
            {  273.4f, -130.0f,   1.5f },   // Freywinn level
            {  371.0f, -176.0f,  24.5f },   // Thorngrin floor
            {  434.0f, -224.0f,  56.7f },   // Laj level
            {  493.5f, -287.0f,  86.6f },   // Warp Splinter top
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBotanicaScript()
{
    return std::make_unique<BotanicaScript>();
}

} // namespace Playerbot
