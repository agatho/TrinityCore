// GrimrailDepotScript — Grimrail Depot (map 1208, WoD 90-100).
// Gorgrond Iron Horde train dungeon.
//   * Rocketspark & Borka — twin-fight bomb-turret + brute.
//   * Nitrogg Thundertower — Bombs + Cannon (turret mechanic).
//   * Skylord Tovra — Drakes + Lightning.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GrimrailDepotScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1208; }   // Grimrail Depot (1228 is Shadowmoon Burial Grounds; audit B31)
    char const* name() const override { return "grimrail_depot"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            79720,   // Grom'kar Boomer (Nitrogg encounter suicide-bomber add)
            81407,   // Grimrail Bombardier (final-car trash, top kill priority)
        };
        a.mandatory_interrupt_spells = {
            165152,  // Cannon Barrage (Nitrogg)
            165134,  // Bombs Away (Rocketspark)
            165164,  // Lightning Storm (Tovra)
            173287,  // Slag Bomb (Rocketspark)
        };
        a.cc_priority_entries = {
            79720,   // Grom'kar Boomer
            81407,   // Grimrail Bombardier
        };
        a.dangerous_auras = {
            165152,  // Cannon Barrage zone
            165134,  // Bombs Away pool
            165164,  // Lightning Storm ground
        };
        // Progression waypoints — Grimrail Depot is a moving Iron
        // Horde train through Gorgrond — vehicle-style transit.
        a.progression_waypoints = {
            {  1450.0f,  130.0f,   60.0f },   // entry station
            {  1380.0f,  117.0f,   60.0f },   // Rocketspark/Borka car
            {  1217.0f,  130.0f,   60.0f },   // Nitrogg car
            {  1086.0f,  117.0f,   60.0f },   // Tovra final car
        };
        // Boss progression — Grimrail Depot has 3 encounters
        // (Rocketspark+Borka duo as one slot).
        a.bosses = {
            77803,  // Railmaster Rocketspark (duo with Borka)
            77816,  // Borka the Brute (duo)
            79545,  // Nitrogg Thundertower
            80005,  // Skylord Tovra (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGrimrailDepotScript()
{
    return std::make_unique<GrimrailDepotScript>();
}

} // namespace Playerbot
