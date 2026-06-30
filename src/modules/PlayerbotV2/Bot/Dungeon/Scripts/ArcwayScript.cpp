// ArcwayScript — The Arcway (map 1516, Legion 100-110).
// Suramar arcane sewer dungeon — undead+demon mixed pulls.
//   * Ivanyr — Power Overwhelming buff stacks (interrupt grants stacks).
//   * Corstilax — Quarantine bubbles (containment mechanic).
//   * General Xakal — Felbound Slash (cone) + Pursuing Spikes.
//   * Nal'tira — Nether Venom + Tangled Web (spider webs).
//   * Advisor Vandros (final) — Time Lock (interrupt) + Mana Bombs.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ArcwayScript final : public DungeonScript
{
public:
    // The Arcway (Legion/Suramar) instance map is 1516. The old 1456 was Eye of
    // Azshara's map — the collision made DungeonScriptMgr discard whichever
    // registered second (first-registration-wins), so Arcway bots got no boss
    // callouts/pull-pacing/interrupts. (1492=Maw of Souls, 1493=Vault — both
    // already taken; 1516 is unused by any other DungeonScript.)
    uint32_t  map_id() const override { return 1516; }
    char const* name() const override { return "arcway"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            105617,  // Eredar Chaosbringer (verified spawn on map 1516)
        };
        a.mandatory_interrupt_spells = {
            195119,  // Time Lock (Vandros)
            195273,  // Mana Bombs (Vandros)
            195249,  // Charged Bolt (Ivanyr)
            195293,  // Quarantine cast (Corstilax)
            195275,  // Volatile Magic (Ivanyr)
        };
        a.cc_priority_entries = {
            105617,  // Eredar Chaosbringer
        };
        a.dangerous_auras = {
            195293,  // Quarantine field
            195119,  // Time Lock zone
            195275,  // Volatile Magic ground
            196158,  // Belch (Naraxas)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 1516), in encounter order. Vandros has no static spawn row
        // (instance-script spawned), so his waypoint is omitted; the bots
        // reach him via the boss-Cell-scan fallback after Nal'tira.
        a.progression_waypoints = {
            { 3146.7f, 5118.5f, 623.3f },   // Ivanyr
            { 3124.2f, 4897.8f, 617.7f },   // Corstilax
            { 3318.1f, 4500.9f, 570.9f },   // General Xakal
            { 3143.9f, 4659.2f, 581.1f },   // Nal'tira
        };
        // Boss progression — The Arcway has 5 encounters. Entries verified
        // against world.creature spawns on map 1516 (Vandros is spawned by
        // the instance script, no static spawn row). The previous 909xx/910xx
        // ids were Neltharion's Lair creatures (Rokmora/Ularogg/Naraxas).
        a.bosses = {
            98203,   // Ivanyr
            98205,   // Corstilax
            98206,   // General Xakal
            98207,   // Nal'tira
            98208,   // Advisor Vandros (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeArcwayScript()
{
    return std::make_unique<ArcwayScript>();
}

} // namespace Playerbot
