// DawnbreakerScript — The Dawnbreaker (map 2662, TWW 70-80).
// Hallowfall airship dungeon — bosses on flying ship.
//   * Speaker Shadowcrown — Shadow Bolt Volley + Web Trap.
//   * Anub'ikkaj — Terrifying Slam + adds.
//   * Rasha'nan (final) — Encasing Spit (immobilize) + Acidic Eruption.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DawnbreakerScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2662; }
    char const* name() const override { return "dawnbreaker"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            223994,  // Nightfall Shadowmage (Shadowcrown gauntlet, interrupt/kill priority)
            216625,  // Nightfall Bomber (Rasha'nan encounter — hurls Arathi Bombs)
            217076,  // UNVERIFIED: 217076 = Doria Ravenblight; authored comment claimed a
                     // "Tarantula (Anub'ikkaj)" add that has no counterpart in this dungeon.
        };
        a.mandatory_interrupt_spells = {
            449734,  // Shadow Bolt Volley (Shadowcrown)
            451222,  // Encasing Spit (Rasha'nan)
            449734,
            450756,  // Terrifying Slam (Anub'ikkaj)
            449734,
        };
        a.cc_priority_entries = {
            223994,  // Nightfall Shadowmage
            216625,  // Nightfall Bomber
        };
        a.dangerous_auras = {
            449734,  // Shadow Bolt zone
            451222,  // Encasing Spit pool
            450756,  // Terrifying Slam ground
            449734,
        };
        // Boss progression — TWW S1 dungeon, no TC instance script.
        // Entries verified against world.creature_template names + Wowhead
        // (Rasha'nan dungeon version is 224552; 214504 is the Nerub-ar Palace
        // raid version).
        // Correct template IDs; bosses are event-summoned during the
        // airship assault (0 spawn rows on map 2662) — navigator falls
        // back to waypoints.
        a.bosses = {
            211087,  // Speaker Shadowcrown
            211089,  // Anub'ikkaj
            224552,  // Rasha'nan (final)
        };
        // Progression waypoints — Dawnbreaker is an aerial-vehicle
        // dungeon over Hallowfall; bots land at each platform via
        // the airship gunship phase.
        a.progression_waypoints = {
            { -550.0f, -575.0f,  150.0f },   // entry deck
            { -660.0f, -680.0f,  160.0f },   // Shadowcrown island
            { -715.0f, -789.0f,  165.0f },   // Anub'ikkaj platform
            { -788.0f, -893.0f,  180.0f },   // Rasha'nan tower
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDawnbreakerScript()
{
    return std::make_unique<DawnbreakerScript>();
}

} // namespace Playerbot
