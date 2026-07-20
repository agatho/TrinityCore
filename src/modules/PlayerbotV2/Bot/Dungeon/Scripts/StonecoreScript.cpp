// StonecoreScript — The Stonecore (map 725 — note: shares with
// Throne of the Tides; engine routes by sub-area). Cata 80-85.
//   * Corborus — Crystal Barrage (telegraphed AoE) + Submerge phase.
//   * Slabhide — Stalactites (overhead drop telegraph).
//   * Ozruk (final) — Shatter (telegraphed AoE) + Spike Shield
//     (don't melee during shield).
//   * High Priestess Azil — Force Grip (interrupt) + Earth Shards
//     (random ground AoE).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class StonecoreScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 725; }     // shared map id
    uint32_t  difficulty_id() const override { return 1; } // heroic difficulty
    char const* name() const override { return "stonecore"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            81628,  // Force Grip (Azil)
            81542,  // Crystal Barrage (Corborus)
            80721,  // Stalactite (Slabhide)
            80813,  // Shatter (Ozruk)
        };
        a.dangerous_auras = {
            81542,  // Crystal Barrage zone
            80721,  // Stalactite drop telegraph
            80813,  // Shatter telegraph
            80721,  // Spike Shield (Ozruk) — don't melee
        };
        // Boss progression — The Stonecore has 4 encounters.
        a.bosses = {
            43438,  // Corborus
            43214,  // Slabhide
            42188,  // Ozruk
            42333,  // High Priestess Azil (final)
        };
        // Progression waypoints — Stonecore is a Deepholm earth temple.
        a.progression_waypoints = {
            { 1062.0f, 1075.0f, 304.0f },   // entry
            { 1148.0f, 1119.0f, 304.0f },   // Corborus burrow
            { 1235.0f, 1138.0f, 274.0f },   // Slabhide platform
            { 1268.0f, 1185.0f, 257.0f },   // Ozruk hall
            { 1320.0f, 1191.0f, 247.0f },   // Azil sanctuary
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeStonecoreScript()
{
    return std::make_unique<StonecoreScript>();
}

} // namespace Playerbot
