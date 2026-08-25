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
        // Modern RFC bosses (TC instance_ragefire_chasm.cpp encounter order).
        a.bosses = {
            61408,  // Adarogg
            61412,  // Dark Shaman Koranthal
            61463,  // Slagmaw
            61528,  // Lava Guard Gordoth
        };
        // Progression waypoints. Approximate spiral coords through the
        // cavern — tank walks them in order, mobs aggro along the way.
        // Tuning note: if a waypoint sits in unreachable geometry on a
        // particular server's map data, move_to's Detour check rejects
        // and the tank stalls. Re-survey via `.gps` in-game and update.
        a.progression_waypoints = {
            { -317.9f,  -10.9f, -19.4f },   // ramp bottom past entrance
            {   -3.2f,  -25.1f, -19.4f },   // Adarogg's chamber
            {   38.7f,  -90.0f, -19.5f },   // approach to Koranthal
            {  -29.6f, -101.0f, -19.7f },   // Slagmaw's pit
            {   -1.3f,  -39.9f, -19.5f },   // Lava Guard Gordoth's room
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
