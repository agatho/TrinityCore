// SunkenTempleScript — Temple of Atal'Hakkar / Sunken Temple
// (map 109, vanilla 50-60). Two-tier troll temple.
//   * Avatar of Hakkar event — 6 Atal'ai Defenders summon Avatar.
//     Adds priority kill.
//   * Jammal'an the Prophet + Ogom the Wretched — Jammal'an heals
//     Ogom; interrupt the heal.
//   * Hakkari Bloodkeepers / Priests — caster trash, CC priority.
//   * Shade of Eranikus (final tier) — Sleep aura debuff.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SunkenTempleScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 109; }
    char const* name() const override { return "sunken_temple"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            8580,   // Atal'alarion (statue-puzzle summon)
            8336,   // Hakkari Sapper
        };
        a.mandatory_interrupt_spells = {
            12039,  // Holy Light (Jammal'an heals Ogom)
            12559,  // Mind Blast (Hakkari Priest)
            12544,  // Frost Armor (Hakkari)
        };
        a.cc_priority_entries = {
            5273,   // Atal'ai High Priest
            5291,   // Hakkari Frostwing (frost caster)
        };
        a.dangerous_auras = {
            12480,  // Sleep (Shade of Eranikus) — interrupted by damage
        };
        // Boss progression — NPC entries from TC's sunken_temple.h.
        // ST is a 2-tier troll dungeon; the dragonkin tier (5719-5722)
        // is fought sequentially before Jammal'an at the bottom and
        // Hakkar/Eranikus at the apex.
        a.bosses = {
            5719,   // Morphaz (Green Dragonkin)
            5720,   // Weaver
            5721,   // Dreamscythe
            5722,   // Hazzas
            5710,   // Jammal'an the Prophet
            8443,   // Avatar of Hakkar
            5709,   // Shade of Eranikus (final)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 109), in encounter order. Weaver (5720), Dreamscythe (5721)
        // and the Avatar of Hakkar (8443) are script-spawned with no
        // creature rows, so they contribute no waypoint; the pathfinder
        // routes the corridors between the remaining anchors.
        a.progression_waypoints = {
            { -644.1f,  103.4f,  -90.8f },   // Morphaz spawn
            { -646.8f,  123.2f,  -90.8f },   // Hazzas spawn
            { -426.7f,  -85.7f,  -88.1f },   // Jammal'an the Prophet spawn
            { -660.0f,  -34.0f,  -90.8f },   // Shade of Eranikus spawn
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSunkenTempleScript()
{
    return std::make_unique<SunkenTempleScript>();
}

} // namespace Playerbot
