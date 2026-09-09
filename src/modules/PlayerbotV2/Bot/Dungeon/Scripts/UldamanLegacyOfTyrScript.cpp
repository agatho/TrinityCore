// UldamanLegacyOfTyrScript — Uldaman: Legacy of Tyr (map 2451, DF 60-70).
// DF revisit of classic Uldaman with Tyr storyline.
//   * The Lost Dwarves — three-add fight (Olaf, Eric, Baelog).
//   * Bromach — Stone Spike (interrupt) + Earthen Shards (zone).
//   * Sentinel Talondras — Targeted Shockwave (move).
//   * Emberon — Searing Carve (telegraph) + adds.
//   * Chrono-Lord Deios (final) — Chrono Burst (zone) + Time Sink.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UldamanLegacyOfTyrScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2451; }
    char const* name() const override { return "uldaman_legacy_of_tyr"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            184580,  // Olaf (tank-buff dwarf — kill last)
            184582,  // Eric the Swift (caster — kill first)
            184581,  // Baelog (archer — kill second)
            205363,  // Time-Lost Waveshaper (Deios)
        };
        a.mandatory_interrupt_spells = {
            369362,  // Stone Spike (Bromach)
            369502,  // Searing Carve telegraph (Emberon)
            369596,  // Chrono Burst (Deios)
            369318,  // Targeted Shockwave (Talondras)
        };
        a.cc_priority_entries = {
            205363,  // Time-Lost Waveshaper
            184582,  // Eric the Swift
        };
        a.dangerous_auras = {
            369362,  // Stone Spike zone
            369502,  // Searing Carve ground
            369596,  // Chrono Burst zone
        };
        // Boss progression — Uldaman: Legacy of Tyr has 5 encounters.
        // The Lost Dwarves uses Eric the Swift as fight-lead.
        a.bosses = {
            184582,  // The Lost Dwarves (Eric the Swift as fight-lead)
            184018,  // Bromach
            184124,  // Sentinel Talondras
            184422,  // Emberon
            184125,  // Chrono-Lord Deios (final)
        };
        // Progression waypoints — boss spawn positions from
        // world.creature (map 2451), in encounter order.
        a.progression_waypoints = {
            {  -15.5f, -1017.4f, 224.0f },   // The Lost Dwarves (Eric the Swift spawn)
            {   96.7f,  -861.5f, 226.6f },   // Bromach
            {   59.4f,  -653.6f, 218.1f },   // Sentinel Talondras
            {  294.8f,  -607.6f, 211.2f },   // Emberon
            {  593.3f,  -630.7f, 213.7f },   // Chrono-Lord Deios (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUldamanLegacyOfTyrScript()
{
    return std::make_unique<UldamanLegacyOfTyrScript>();
}

} // namespace Playerbot
