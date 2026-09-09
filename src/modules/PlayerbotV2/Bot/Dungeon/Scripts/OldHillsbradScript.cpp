// OldHillsbradScript — Old Hillsbrad Foothills (map 560, TBC 64-70).
// Caverns of Time #2. Escort Thrall through camp + flee.
//   * Lieutenant Drake — interruptible heroic strike.
//   * Captain Skarloc — adds; tank-heavy.
//   * Epoch Hunter (final) — phases (Time Stop, Curse of Exhaustion).
// Escort mechanic — Thrall NPC must survive; bots assist on adds.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class OldHillsbradScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 560; }
    char const* name() const override { return "old_hillsbrad"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            18164,  // Skarloc Honor Guard
        };
        a.mandatory_interrupt_spells = {
            31623,  // Heroic Strike (Drake)
            31627,  // Time Stop (Epoch Hunter)
            31635,  // Curse of Exhaustion (Epoch Hunter)
        };
        a.dangerous_auras = {
            31627,  // Time Stop AoE
        };
        // Boss progression — Old Hillsbrad has 3 encounters.
        a.bosses = {
            17848,  // Lieutenant Drake
            17862,  // Captain Skarloc
            18096,  // Epoch Hunter (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeOldHillsbradScript()
{
    return std::make_unique<OldHillsbradScript>();
}

} // namespace Playerbot
