// ZulAmanScript — Zul'Aman (map 568, TBC 10-man / Cata 5-man heroic rework).
// 6 bosses: Akil'zon, Nalorakk, Jan'alai, Halazzi, Hex Lord Malacrass, Daakara.
//
// TC's ZulAman boss scripts are STUBS — only the Hexlord boss has any
// spell defines (43522 Unstable Affliction trigger, 43523 dispel reaction).
// All others (boss_akilzon, boss_nalorakk, etc.) are skeleton classes with
// no combat code in TC's current implementation. Bots in ZA therefore run
// on generic combat rules (interrupt-any-interruptible, target lowest HP).
// The advice below ships only the verifiable IDs; remaining bosses fall
// back to the generic path.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ZulAmanScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 568; }
    char const* name() const override { return "zul_aman"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {};
        a.mandatory_interrupt_spells = {
            // Hex Lord Malacrass (only TC-verified ZA spell)
            43522,  // Warlock-class Unstable Affliction
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            43522,  // Unstable Affliction (dispel-with-care debuff)
        };
        // Boss progression — Zul'Aman has 6 encounters (4 timed Lynx
        // mini-bosses + Hex Lord + Daakara in the Cata rework).
        a.bosses = {
            23574,  // Akil'zon (Eagle)
            23576,  // Nalorakk (Bear)
            23578,  // Jan'alai (Dragonhawk)
            23577,  // Halazzi (Lynx)
            24239,  // Hex Lord Malacrass
            23863,  // Daakara / Zul'jin (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeZulAmanScript()
{
    return std::make_unique<ZulAmanScript>();
}

} // namespace Playerbot
