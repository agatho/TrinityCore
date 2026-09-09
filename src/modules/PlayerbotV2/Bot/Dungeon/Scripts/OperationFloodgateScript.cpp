// OperationFloodgateScript — Operation: Floodgate (map 2773, TWW 70-80).
// Undermine subzone — Bilgewater Cartel pumping station.
//   * Big M.O.M.M.A. — Drain (channel) + Sonic Boom.
//   * Demolition Duo — twin tank fight (Keeza Quickfuse + Bront).
//   * Swampface — Mudslide (zone) + Razorchoke Vines.
//   * Geezle Gigazap (final) — Lightning Storm + Turbo Charged.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class OperationFloodgateScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2773; }
    char const* name() const override { return "operation_floodgate"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            226660,  // Loaderbot (M.O.M.M.A. add)
            226805,  // Sappers' Cache (Demolition Duo)
            226668,  // Sliponius Frog (Swampface)
        };
        a.mandatory_interrupt_spells = {
            465124,  // Sonic Boom (M.O.M.M.A.)
            469025,  // Lightning Storm (Geezle)
            465253,  // Razorchoke Vines (Swampface)
            465124,
            465124,
        };
        a.cc_priority_entries = {
            226660,
            226668,
        };
        a.dangerous_auras = {
            465124,  // Sonic Boom zone
            465253,  // Razorchoke Vines pool
            469025,  // Lightning Storm ground
            465249,  // Mudslide
        };
        // Boss progression — Operation: Floodgate has 4 encounters.
        // Demolition Duo uses Keeza Quickfuse as fight-lead.
        // Swampface (226396) has no spawn row on map 2773 (event-
        // summoned) — navigator falls back to waypoints for him.
        a.bosses = {
            226398,  // Big M.O.M.M.A.
            226402,  // Demolition Duo (Keeza Quickfuse lead)
            226396,  // Swampface
            226404,  // Geezle Gigazap (final)
        };
        // Progression waypoints — Operation Floodgate is an Undermine
        // pumping station: outdoor industrial floor → cave → tank room.
        a.progression_waypoints = {
            { -825.0f, 4030.0f,  100.0f },   // entry
            { -748.0f, 3942.0f,  100.0f },   // M.O.M.M.A. floor
            { -641.0f, 3854.0f,   95.0f },   // Demolition Duo bridge
            { -558.0f, 3756.0f,   85.0f },   // Swampface mire
            { -470.0f, 3645.0f,   75.0f },   // Geezle pumping station
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeOperationFloodgateScript()
{
    return std::make_unique<OperationFloodgateScript>();
}

} // namespace Playerbot
