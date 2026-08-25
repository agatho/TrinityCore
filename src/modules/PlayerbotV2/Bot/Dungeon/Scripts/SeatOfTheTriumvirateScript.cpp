// SeatOfTheTriumvirateScript — Seat of the Triumvirate (map 1753,
// Legion 7.3 100-110). Argus void-themed dungeon.
//   * Zuraal the Ascended — Void Rupture (zone) + Engulfing Void.
//   * Saprish — Bonds of the Triumvirate + adds.
//   * Viceroy Nezhar — Demoralizing Shout + Drain Soul.
//   * L'ura (final) — Pulsing Shadows + Dark Fathoms (drift mechanic).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SeatOfTheTriumvirateScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1753; }
    char const* name() const override { return "seat_of_the_triumvirate"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            123054,  // Remnant of Anguish (L'ura)
            122319,  // Darkfang (Saprish hunting beast)
            122423,  // Grand Shadow-Weaver (spawned trash caster)
        };
        a.mandatory_interrupt_spells = {
            246208,  // Void Rupture (Zuraal)
            247816,  // Drain Soul (Nezhar)
            247470,  // Engulfing Void (Zuraal)
            246183,  // Pulsing Shadows (L'ura)
        };
        a.cc_priority_entries = {
            122423,  // Grand Shadow-Weaver
            122421,  // Umbral War-Adept
        };
        a.dangerous_auras = {
            246208,  // Void Rupture zone
            247816,  // Drain Soul beam
            246448,  // Dark Fathoms drift
            247470,  // Engulfing Void
        };
        // Boss progression — Seat of the Triumvirate has 4 encounters.
        // Entries DB-verified (creature_template names + map 1753 spawns;
        // Saprish 122316 is event-spawned).
        a.bosses = {
            122313,  // Zuraal the Ascended
            122316,  // Saprish
            122056,  // Viceroy Nezhar
            124729,  // L'ura (final)
        };
        // Progression waypoints — DB-truth boss spawn positions on map 1753
        // (world.creature), in encounter order. Saprish (122316) is
        // event-spawned with no creature row, so it has no waypoint.
        a.progression_waypoints = {
            { 5510.3f, 10800.2f, 21.6f },   // Zuraal the Ascended
            { 6140.2f, 10391.4f, 19.9f },   // Viceroy Nezhar
            { 5979.3f, 10220.4f, 14.3f },   // L'ura (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSeatOfTheTriumvirateScript()
{
    return std::make_unique<SeatOfTheTriumvirateScript>();
}

} // namespace Playerbot
