// NeltharionsLairScript — Neltharion's Lair (map 1458, Legion 110).
// 4 bosses: Rokmora, Ularogg Cragshaper, Naraxas, Dargrul.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/BrokenIsles/NeltharionsLair/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class NeltharionsLairScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1458; }
    char const* name() const override { return "neltharions_lair"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            96475,   // Naraxas Worm
        };
        a.mandatory_interrupt_spells = {
            // Ularogg Cragshaper
            192800,  // Choking Dust Damage
            188114,  // Shatter
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Ularogg
            198024,  // Crystalline Ground
            198028,  // Crystalline Ground Damage
            215898,  // Crystalline Ground Periodic
            216488,  // Crystalline Ground Visual
            // Razor Shards / Skitter
            188169,  // Razor Shards
            215929,  // Rupturing Skitter
        };
        // Boss progression — entries from TC's neltharions_lair.h.
        // Progression waypoints — Neltharion's Lair is a Highmountain
        // earth tunnel: entry → Rokmora pit → spider cave → Ularogg
        // forge → Naraxas swallow → Dargrul's throne.
        a.progression_waypoints = {
            {  407.0f, 1413.0f, -76.0f },   // entry
            {  511.0f, 1391.0f, -72.0f },   // Rokmora pit
            {  590.0f, 1402.0f, -55.0f },   // Ularogg forge
            {  686.0f, 1442.0f, -64.0f },   // Naraxas chamber
            {  720.0f, 1572.0f, -69.0f },   // Dargrul throne
        };
        a.bosses = {
            91003,   // Rokmora
            91004,   // Ularogg Cragshaper
            91005,   // Naraxas
            91007,   // Dargrul the Underking (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeNeltharionsLairScript()
{
    return std::make_unique<NeltharionsLairScript>();
}

} // namespace Playerbot
