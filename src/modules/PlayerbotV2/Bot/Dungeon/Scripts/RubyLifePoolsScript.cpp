// RubyLifePoolsScript — Ruby Life Pools (map 2521, DF 60-70).
// Waking Shores dungeon.
//   * Melidrussa Chillworn — Power of Ice phase shift.
//   * Kokia Blazehoof — Babbling Flames + Searing Wrath.
//   * Kyrakka & Erkhart Stormvein (final) — Fire / Storm phase
//     duo; both dragons must die together.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RubyLifePoolsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2521; }
    char const* name() const override { return "ruby_life_pools"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            372749,  // Babbling Flames (Kokia)
            372655,  // Searing Wrath (Kokia)
            372858,  // Power of Ice (Melidrussa)
            372456,  // Flame Dance (Kyrakka)
            372458,  // Stormbreath (Erkhart)
        };
        a.dangerous_auras = {
            372456,  // Flame Dance zone
            372458,  // Stormbreath cone
        };
        // Boss progression — entries from TC's ruby_life_pools.h.
        // The final encounter is a duo (Kyrakka + Erkhart); either entry
        // resolves the encounter — Kyrakka is canonically listed.
        a.bosses = {
            188252,  // Melidrussa Chillworn
            189232,  // Kokia Blazehoof
            199790,  // Kyrakka (final — paired with Erkhart Stormvein)
        };
        // Progression waypoints — Ruby Life Pools is a Waking Shores
        // outdoor dragon nest with 3 ring tiers.
        a.progression_waypoints = {
            {  -160.0f, 1825.0f,  93.0f },   // entry
            {   -84.0f, 1782.0f, 108.0f },   // Melidrussa ice nest
            {    24.0f, 1700.0f, 122.0f },   // Kokia fire nest
            {   135.0f, 1640.0f, 140.0f },   // Kyrakka platform
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRubyLifePoolsScript()
{
    return std::make_unique<RubyLifePoolsScript>();
}

} // namespace Playerbot
