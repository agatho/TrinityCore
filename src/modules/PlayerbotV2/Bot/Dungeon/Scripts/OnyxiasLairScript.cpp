// OnyxiasLairScript — Onyxia's Lair raid (map 249, classic 40-man + WotLK 10/25).
// Single boss Onyxia in 3 phases (ground / air / ground+whelps).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/OnyxiasLair/boss_onyxia.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class OnyxiasLairScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 249; }
    char const* name() const override { return "onyxias_lair"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            11262,  // Onyxian Whelp
            36561,  // Onyxian Lair Guard (P3)
        };
        a.mandatory_interrupt_spells = {
            68868,  // Cleave
            68867,  // Tail Sweep
            18500,  // Wing Buffet
            18435,  // Flame Breath
            18392,  // Fireball (air phase ground target)
            18431,  // Bellowing Roar (fear AoE — ground)
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            18392,  // Fireball
            18500,  // Wing Buffet
            18435,  // Flame Breath
            23461,  // Deep Breath
            // Air-phase breath sweeps (multiple cardinal directions)
            17086, 18351, 18576, 18609,
            18564, 18584, 18596, 18617,
            18431,  // Bellowing Roar
        };
        // Boss progression — Onyxia is the sole encounter (3-phase fight).
        // Entry 10184 from TC boss_onyxia.cpp.
        a.bosses = {
            10184,  // Onyxia
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeOnyxiasLairScript()
{
    return std::make_unique<OnyxiasLairScript>();
}

} // namespace Playerbot
