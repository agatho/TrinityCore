// MagtheridonsLairScript — Magtheridon's Lair raid (map 544, TBC 25-man).
// Single boss Magtheridon, requires Hellfire Channelers' grasp-channel
// to release him from his prison via Manticron Cubes.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/HellfireCitadel/MagtheridonsLair/boss_magtheridon.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MagtheridonsLairScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 544; }
    char const* name() const override { return "magtheridons_lair"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17256,  // Hellfire Channeler
            17646,  // Burning Abyssal (infernal from channeler summon)
        };
        a.mandatory_interrupt_spells = {
            // Channelers (interrupt their utility/heal casts)
            30510,  // Shadow Bolt Volley
            30528,  // Dark Mending
            30530,  // Fear
            30531,  // Soul Transfer
            30511,  // Burning Abyssal (summon)
            // Magtheridon
            30616,  // Blast Nova (must be interrupted/knocked back to survive)
            30619,  // Cleave
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Channelers + adds
            30530,  // Fear
            30531,  // Soul Transfer
            // Magtheridon
            30616,  // Blast Nova (massive shadow AoE)
            30657,  // Quake (knock player; cancel cube channel)
            30542,  // Blaze (ground fire)
            30541,  // Blaze Target
            30168,  // Shadow Cage (Magtheridon prison aura, informational)
            44032,  // Mind Exhaustion (cube-cooldown debuff)
            30631,  // Debris Damage
            30632,  // Debris Visual
            36449,  // Debris Knockdown
            36455,  // Camera Shake
        };
        // Boss progression — Magtheridon entry from TC's
        // magtheridons_lair.h. Single-encounter raid.
        a.bosses = {
            17257,  // Magtheridon
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMagtheridonsLairScript()
{
    return std::make_unique<MagtheridonsLairScript>();
}

} // namespace Playerbot
