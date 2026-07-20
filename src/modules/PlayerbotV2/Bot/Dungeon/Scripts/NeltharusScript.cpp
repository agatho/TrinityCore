// NeltharusScript — Neltharus (map 2519, DF 60-70).
// Black-dragonflight forge dungeon in Waking Shores.
//   * Forgemaster Gorek — Heated Stomp (move out).
//   * Magmatusk — Magma Wave (interrupt) + adds.
//   * Chargath, Bane of Scales — Imprison Asunder (free captives).
//   * Warlord Sargha (final) — Molten Gold (zone) + Forgebound Mage adds.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class NeltharusScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2519; }
    char const* name() const override { return "neltharus"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            186739,  // Forgebound Mage (Sargha adds)
            186822,  // Magma Lance caster (Magmatusk)
        };
        a.mandatory_interrupt_spells = {
            372858,  // Magma Wave (Magmatusk)
            372912,  // Molten Gold cast (Sargha)
            372485,  // Imprison Asunder (Chargath)
            372325,  // Heated Stomp telegraph (Gorek)
        };
        a.cc_priority_entries = {
            186739,  // Forgebound Mage
        };
        a.dangerous_auras = {
            372912,  // Molten Gold zone
            372325,  // Heated Stomp ground
        };
        // Boss progression — Neltharus has 4 encounters.
        a.bosses = {
            189478,  // Forgemaster Gorek
            181861,  // Magmatusk
            189340,  // Chargath, Bane of Scales
            189901,  // Warlord Sargha (final)
        };
        // Progression waypoints — Neltharus is a Waking Shores black
        // dragonflight forge dungeon with cave/lava sections.
        a.progression_waypoints = {
            { -3760.0f,  4985.0f,  500.0f },   // entry
            { -3686.0f,  5072.0f,  475.0f },   // Gorek forge
            { -3598.0f,  5170.0f,  450.0f },   // Magmatusk pit
            { -3520.0f,  5273.0f,  425.0f },   // Chargath bridge
            { -3437.0f,  5380.0f,  410.0f },   // Sargha vault
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeNeltharusScript()
{
    return std::make_unique<NeltharusScript>();
}

} // namespace Playerbot
