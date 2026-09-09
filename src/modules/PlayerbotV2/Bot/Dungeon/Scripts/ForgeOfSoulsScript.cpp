// ForgeOfSoulsScript — The Forge of Souls (map 632, WotLK 80).
// Frozen Halls #1. 2 bosses: Bronjahm and Devourer of Souls.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/FrozenHalls/ForgeOfSouls/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ForgeOfSoulsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 632; }
    char const* name() const override { return "forge_of_souls"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            36535,  // Corrupted Soul Fragment (Bronjahm)
        };
        a.mandatory_interrupt_spells = {
            // Bronjahm
            68793,  // Magic's Bane
            70043,  // Shadow Bolt
            68839,  // Corrupt Soul
            68950,  // Fear
            // Devourer of Souls
            68982,  // Phantom Blast
            68820,  // Well of Souls
            68939,  // Unleashed Souls
            68873,  // Wailing Souls (channel)
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Bronjahm
            68872,  // Soulstorm (spinning AoE)
            // Devourer
            69034,  // Mirrored Soul Damage (don't attack!)
            69051,  // Mirrored Soul Buff
            68875,  // Wailing Souls Beam
        };
        // Boss progression — NPC entries from TC's forge_of_souls.h.
        a.bosses = {
            36497,  // Bronjahm
            36502,  // Devourer of Souls (final)
        };
        // Progression waypoints — FoS is a short linear ICC forge.
        a.progression_waypoints = {
            { 5613.0f, 2541.0f, 707.7f },   // entry
            { 5294.0f, 2002.0f, 709.3f },   // central corridor
            { 5300.0f, 1995.0f, 709.3f },   // Bronjahm arena
            { 5662.0f, 2502.0f, 712.0f },   // Devourer chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeForgeOfSoulsScript()
{
    return std::make_unique<ForgeOfSoulsScript>();
}

} // namespace Playerbot
