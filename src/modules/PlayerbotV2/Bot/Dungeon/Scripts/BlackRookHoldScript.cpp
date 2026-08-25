// BlackRookHoldScript — Black Rook Hold (map 1501, Legion 110).
// 4 bosses: The Amalgam of Souls, Illysanna Ravencrest, Smashspite the
// Hateful, Lord Kur'talos Ravencrest.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/BrokenIsles/BlackRookHold/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackRookHoldScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1501; }
    char const* name() const override { return "black_rook_hold"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            98637,   // Risen Soldier (Ravencrest)
        };
        a.mandatory_interrupt_spells = {
            // The Amalgam of Souls
            194956,  // Reap Soul
            194966,  // Soul Echoes
            194981,  // Soul Echoes Clone Caster
            196930,  // Soulgorge
            196078,  // Call Souls
            196587,  // Soul Burst
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Amalgam
            195254,  // Swirling Scythe
            196517,  // Swirling Scythe Damage
            194960,  // Soul Echoes Damage
            196925,  // Call Souls AreaTrigger
        };
        // Boss progression — entries from TC's black_rook_hold.h.
        // Progression waypoints — Black Rook Hold is a multi-floor
        // night elf fortress in Val'sharah.
        a.progression_waypoints = {
            { 2725.0f, 6175.0f, 87.0f },   // entry
            { 2723.0f, 6275.0f, 92.0f },   // Amalgam altar
            { 2718.0f, 6388.0f, 92.0f },   // Illysanna stairs
            { 2705.0f, 6541.0f, 117.0f },  // Smashspite arena
            { 2638.0f, 6608.0f, 144.0f },  // Ravencrest throne
        };
        a.bosses = {
            98542,   // The Amalgam of Souls
            98696,   // Illysanna Ravencrest
            98949,   // Smashspite the Hateful
            94923,   // Lord Kur'talos Ravencrest (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackRookHoldScript()
{
    return std::make_unique<BlackRookHoldScript>();
}

} // namespace Playerbot
