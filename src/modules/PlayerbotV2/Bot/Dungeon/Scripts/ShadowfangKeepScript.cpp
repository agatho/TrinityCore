// ShadowfangKeepScript — Shadowfang Keep (map 33, vanilla 16-26).
// Worgen / Forsaken keep in Silverpine. Notable mechanics:
//   * Apothecary Hummel + Frye + Baxter — three-pack final boss in
//     classic SFK. Each Apothecary uses different toxin auras —
//     Hummel applies Chromatic Mutation (interruptible), Frye applies
//     Anesthetic (sleep), Baxter applies Acid Vials (poison DoT).
//     CC priority: drop Frye first (sleep on healer = wipe), then
//     Baxter (DoT removable), then Hummel (single-target).
//   * Hummel periodically summons Crown Apothecary adds — kill order:
//     adds before boss to avoid 5+ targets on tank.
//   * Lord Godfrey casts Cursed Bullet (interruptible) — wastes party
//     resources if missed.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ShadowfangKeepScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 33; }
    char const* name() const override { return "shadowfang_keep"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Apothecary entries verified vs TC boss_apothecary_hummel.cpp:84-85
        // (NPC_APOTHECARY_FRYE = 36272, NPC_APOTHECARY_BAXTER = 36565;
        // Hummel itself is the Cata heroic encounter, entry not exposed
        // in the .h — DB lookup confirms ~36296).
        a.high_priority_kill_entries = {
            36885,  // Crown Apothecary (Hummel encounter adds, script-spawned
                    // per TC boss_apothecary_hummel.cpp:87)
        };
        // Spells from TC boss_apothecary_hummel.cpp:34-48. Old IDs
        // 68595/68594/7964 had no TC backing (68595/68594 appear nowhere
        // in TC source); the real interruptibles are:
        a.mandatory_interrupt_spells = {
            68607,  // Perfume Spray (Hummel)
            68948,  // Cologne Spray (Hummel rework)
            68821,  // Chain Reaction
        };
        a.cc_priority_entries = {
            36272,  // Apothecary Frye (real entry per TC :84)
            36565,  // Apothecary Baxter
        };
        // Dangerous-ground spills (perfume/cologne pools).
        a.dangerous_auras = {
            68798,  // Perfume Spill
            68614,  // Cologne Spill
            68927,  // Perfume Spill Damage
            68934,  // Cologne Spill Damage
        };
        // Boss progression — NPC entries from TC's shadowfang_keep.h
        // (the Cata remake is the ONLY live version on map 33; classic
        // Rethilgore/Razorclaw/Nandos/Arugal no longer spawn there).
        // TC encounter order: Ashbury(0) Silverlaine(1) Springvale(2)
        // Walden(3) Godfrey(4); Apothecary Hummel(5) is the seasonal
        // Crown Chemical Co. event, script-spawned.
        a.bosses = {
            46962,  // Baron Ashbury
            3887,   // Baron Silverlaine
            4278,   // Commander Springvale
            46963,  // Lord Walden
            46964,  // Lord Godfrey (final)
        };
        // Progression waypoints — Shadowfang Keep is a vertical
        // Silverpine fortress with stair levels.
        a.progression_waypoints = {
            { -239.0f,  2113.0f,  79.0f },   // entry
            { -154.0f,  2178.0f,  93.0f },   // courtyard (Rethilgore/Nandos)
            { -245.0f,  2107.0f, 124.0f },   // upper hall (Silverlaine)
            { -157.0f,  2174.0f, 128.0f },   // Razorclaw kennel
            { -226.0f,  2280.0f, 130.0f },   // Springvale chapel
            { -149.0f,  2178.0f, 130.0f },   // Arugal/Godfrey tower top
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeShadowfangKeepScript()
{
    return std::make_unique<ShadowfangKeepScript>();
}

} // namespace Playerbot
