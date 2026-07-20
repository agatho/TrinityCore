// VortexPinnacleScript — The Vortex Pinnacle (map 657, Cata 80-85).
// Skywall sky-fortress. Tier 1.
//   * Grand Vizier Ertan — Cyclone Shield (interrupt) + Lightning
//     Storm phase (PvE; bots stay close).
//   * Altairus — Sirocco wind direction mechanic; bots move with wind.
//   * Asaad (final) — Static Cling root + Storm Cloud trinkets.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class VortexPinnacleScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 657; }
    char const* name() const override { return "vortex_pinnacle"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            45724,  // Howling Gale (Altairus add)
            45722,  // Lurking Tempest (trash caster)
        };
        a.mandatory_interrupt_spells = {
            86292,  // Cyclone Shield (Ertan)
            86281,  // Lightning Storm (Ertan)
            87618,  // Static Cling (Asaad)
            87622,  // Chain Lightning (Asaad)
        };
        a.dangerous_auras = {
            87618,  // Static Cling root — break by jumping
            86281,  // Lightning Storm zone
        };
        // Boss progression — NPC entries from TC's vortex_pinnacle.h.
        a.bosses = {
            43878,  // Grand Vizier Ertan
            43873,  // Altairus
            43875,  // Asaad (final)
        };
        // Progression waypoints — Vortex Pinnacle is a sky temple with
        // jump-pad transitions between floating platforms.
        a.progression_waypoints = {
            {  -288.0f,   3.0f, 631.0f },   // entry pad
            {  -437.0f,  -1.5f, 633.0f },   // Ertan pad
            {  -713.0f, 124.0f, 631.0f },   // Altairus pad
            {  -914.0f,  43.0f, 642.0f },   // Asaad pad
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeVortexPinnacleScript()
{
    return std::make_unique<VortexPinnacleScript>();
}

} // namespace Playerbot
