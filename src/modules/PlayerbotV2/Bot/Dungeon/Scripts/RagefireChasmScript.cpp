// RagefireChasmScript — Ragefire Chasm (map 389, modern post-Cata
// remake L13-21). Tiny Horde starter dungeon under Orgrimmar. TC's
// instance script defines four encounters in canonical order:
//   1) Adarogg                  (61408)
//   2) Dark Shaman Koranthal    (61412)
//   3) Slagmaw                  (61463)
//   4) Lava Guard Gordoth       (61528) — final
// The layout is a clockwise spiral down/around a central lava pool.
// Tank-advance walks the progression_waypoints[] in order; each point
// is meant to land the tank inside the next mob pack's aggro radius.
// Detour validates each move so an out-of-date waypoint just gets
// rejected — bot stays put until trash/boss is in nearby_enemies.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RagefireChasmScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 389; }
    char const* name() const override { return "ragefire_chasm"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Boss order MUST match the DB crumb-route order (wowc_playerbot.
        // playerbot_dungeon_routes map389, 54 crumbs): the route descends into
        // Adarogg's pit FIRST (crumbs 1-13, entrance→-285,-54,-61) then rises to
        // Koranthal (24) → Slagmaw (32) → Gordoth (53). That IS the TC encounter
        // index order. bosses_done_count indexes both bosses[] and the escape's
        // progression_waypoints[], and the tank-advance pushes the earliest LIVE
        // bosses[i] — so a Koranthal-first reorder (tried 07-22) fights the route:
        // the tank beelined 104y NW toward Koranthal OFF the route and stalled,
        // because the route cursor still wanted the Adarogg descent first. Keep
        // encounter/route order.
        a.bosses = {
            61408,  // Adarogg               (SW pit, z-60 — route crumb 13)
            61412,  // Dark Shaman Koranthal (NE, z-20   — route crumb 24)
            61463,  // Slagmaw               (N,  z-19   — route crumb 32)
            61528,  // Lava Guard Gordoth    (NW, z-18   — route crumb 53)
        };
        // Progression waypoints — the ACTUAL boss spawn positions, aligned
        // 1:1 with bosses[] above (waypoints[i] == bosses[i]). This alignment
        // is REQUIRED: the false-combat escape (State_Idle) relocates a wedged
        // bot to waypoints[bosses_done_count] and its logic asserts
        // waypoints[i]==bosses[i] "is exactly the encounter we still owe".
        //
        // ROOT-FIX 2026-07-22: the previous coords were mis-surveyed by
        // 200-400y (e.g. Koranthal waypoint (38.7,-90) vs the real (-117,71))
        // AND off-by-one (a leading "ramp" entry shifted every boss). The tank-
        // advance's Detour check therefore rejected every waypoint as
        // unreachable geometry, so the group never routed to a pack — it drifted
        // via incidental aggro, stalling 60-80y short of each boss while a bot
        // sat in false-combat, and the escape teleported to the wrong point.
        // Coords are the live wc_world.creature spawn positions on map 389
        // (verified reachable — the squad kills these bosses when it arrives).
        a.progression_waypoints = {
            { -284.6f,  -53.8f, -60.7f },   // 61408 Adarogg (pit — route crumb 13)
            { -117.0f,   71.1f, -20.7f },   // 61412 Dark Shaman Koranthal
            { -257.2f,  172.4f, -19.6f },   // 61463 Slagmaw
            { -369.5f,  166.2f, -18.5f },   // 61528 Lava Guard Gordoth
        };
        // Boss-specific interrupts / dangerous casts. Spell IDs from TC
        // boss scripts (or WoWHead) — populate as we identify them
        // empirically. Empty list = no override, generic interrupt rule
        // still kicks any interruptible cast.
        a.mandatory_interrupt_spells = {};
        a.dangerous_auras = {};
        a.cc_priority_entries = {};
        a.high_priority_kill_entries = {};
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRagefireChasmScript()
{
    return std::make_unique<RagefireChasmScript>();
}

} // namespace Playerbot
