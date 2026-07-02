#include "DungeonScript.h"
#include "../BotSnapshotView.h"
#include "../BotSnapshot.h"
#include "DatabaseEnv.h"
#include "Config.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot {

namespace {
inline uint64_t MakeKey(uint32_t map_id, uint32_t difficulty)
{
    return (static_cast<uint64_t>(map_id) << 8) | (difficulty & 0xFFu);
}

// Dungeon route waypoints are STATIC, map-derived nav data (chain-pathfound
// from the navmesh) — identical across every realm running the same maps, just
// like the shared handcrafted_road table. They live in the shared playerbot
// schema (Playerbot.SharedDatabase, default "playerbot") so all servers share
// ONE copy instead of duplicating them in every realm's world DB, and queried
// via CharacterDatabase with a schema qualifier (mirrors BotNamePool /
// WorldMetadata — same MySQL server, cross-schema reference).
std::string const& SharedDb()
{
    static std::string const db =
        sConfigMgr->GetStringDefault("Playerbot.SharedDatabase", "playerbot");
    return db;
}
}

void DungeonScriptMgr::Register(std::unique_ptr<DungeonScript> script)
{
    if (!script) return;
    const uint64_t key = MakeKey(script->map_id(), script->difficulty_id());
    // Duplicate-map_id detection. The audit (2026-05-14) caught 3 scripts
    // registered against the wrong map (LostCityOfTolvir 754→755,
    // TempleOfSethraliss 1862→1877, Arcway/Vault sharing 1493). The first
    // registration wins emplace; the second silently no-ops with no error
    // unless we surface it. Log a WARN so the next time someone adds a
    // dungeon they catch the collision immediately at module init.
    if (auto it = scripts_.find(key); it != scripts_.end())
    {
        TC_LOG_WARN("playerbot.v2",
            "[DungeonScriptMgr] duplicate registration: '{}' (map={}, diff={}) "
            "conflicts with already-registered '{}'; new script discarded. "
            "Verify map_id is correct.",
            script->name(), script->map_id(), script->difficulty_id(),
            it->second ? it->second->name() : "<null>");
        return;
    }
    scripts_.emplace(key, std::move(script));
}

void DungeonScriptMgr::RegisterGlobal(std::unique_ptr<DungeonScript> script)
{
    if (!script) return;
    global_scripts_.push_back(std::move(script));
}

DungeonScript const* DungeonScriptMgr::GetScriptFor(uint32_t map_id, uint32_t difficulty_id) const
{
    if (auto it = scripts_.find(MakeKey(map_id, difficulty_id)); it != scripts_.end())
        return it->second.get();
    if (difficulty_id != 0)
    {
        if (auto it = scripts_.find(MakeKey(map_id, 0)); it != scripts_.end())
            return it->second.get();
    }
    return nullptr;
}

namespace {
inline void MergeAdvice(DungeonAdvice& dst, DungeonAdvice const& src)
{
    auto cat = [](std::vector<uint32_t>& d, std::vector<uint32_t> const& s)
    { d.insert(d.end(), s.begin(), s.end()); };
    cat(dst.high_priority_kill_entries, src.high_priority_kill_entries);
    cat(dst.mandatory_interrupt_spells, src.mandatory_interrupt_spells);
    cat(dst.cc_priority_entries,        src.cc_priority_entries);
    cat(dst.dangerous_auras,            src.dangerous_auras);
    cat(dst.kite_creature_entries,      src.kite_creature_entries);
    cat(dst.spread_on_self_auras,       src.spread_on_self_auras);
    cat(dst.stack_on_cast_spells,       src.stack_on_cast_spells);
    cat(dst.dispel_priority_spells,     src.dispel_priority_spells);
    cat(dst.pull_separately_entries,        src.pull_separately_entries);
    cat(dst.pull_separately_auras,          src.pull_separately_auras);
    cat(dst.dispel_enemy_priority_spells,   src.dispel_enemy_priority_spells);
    cat(dst.tank_swap_on_spells,        src.tank_swap_on_spells);
    cat(dst.soak_spells,                src.soak_spells);
    cat(dst.bosses,                     src.bosses);
    dst.progression_waypoints.insert(dst.progression_waypoints.end(),
        src.progression_waypoints.begin(), src.progression_waypoints.end());
}

// ORDER-PRESERVING dedup (audit B29/B35): the old sort+unique re-ordered
// every vector NUMERICALLY each tick, destroying the authored encounter
// progression that bosses[] (and high_priority_kill_entries) explicitly
// encode — tank-advance walks bosses[] in order, so any dungeon whose later
// boss had a lower creature entry got its progression scrambled. Keep first
// occurrence, preserve authored order; vectors are <=32 entries so the
// linear-scan dedup costs no more than the sort did.
inline void DedupAdvice(DungeonAdvice& a)
{
    auto dedup = [](std::vector<uint32_t>& v)
    {
        if (v.size() < 2) return;
        std::vector<uint32_t> out;
        out.reserve(v.size());
        for (uint32_t x : v)
            if (std::find(out.begin(), out.end(), x) == out.end())
                out.push_back(x);
        v.swap(out);
    };
    dedup(a.high_priority_kill_entries);
    dedup(a.mandatory_interrupt_spells);
    dedup(a.cc_priority_entries);
    dedup(a.dangerous_auras);
    dedup(a.kite_creature_entries);
    dedup(a.spread_on_self_auras);
    dedup(a.stack_on_cast_spells);
    dedup(a.dispel_priority_spells);
    dedup(a.pull_separately_entries);
    dedup(a.pull_separately_auras);
    dedup(a.dispel_enemy_priority_spells);
    dedup(a.tank_swap_on_spells);
    dedup(a.soak_spells);
    dedup(a.bosses);
    // Waypoints are positional — duplicates are intentional in scripts
    // that re-walk a corridor (path-and-return patterns). Don't dedup.
}
}

DungeonAdvice DungeonScriptMgr::GetAdvice(BotSnapshotView const& s) const
{
    DungeonAdvice merged;
    if (DungeonScript const* per_dungeon = GetScriptFor(s.map_id(), s.raw().instance_ctx.map_difficulty))
        merged = per_dungeon->get_advice(s);
    for (auto const& gs : global_scripts_)
        MergeAdvice(merged, gs->get_advice(s));
    // Per-tick dedup: cleans up 16+ scripts with copy-paste duplicate
    // spell IDs in mandatory_interrupt_spells / 9 with duplicates in
    // dangerous_auras / 3 in high_priority_kill_entries. The consumer
    // rules scan these vectors linearly; the script-side dups never
    // changed correctness (matching the same ID twice is a no-op) but
    // they wasted compares and made the audit log noisier.
    DedupAdvice(merged);
    // Inject auto-generated route_waypoints when the per-dungeon script authored
    // NONE. These are the navmesh-chained corridor points (entrance->bosses) that
    // let the far-boss advance route winding corridors past the 74-poly cap on
    // dungeons that never got a hand-authored chain. A script that DID author its
    // own (Deadmines' harbor descent) keeps it — the empty() guard makes authored
    // waypoints authoritative. Difficulty-0 rows are the "any-difficulty" default.
    if (merged.route_waypoints.empty() && !generated_routes_.empty())
    {
        const uint32_t diff = s.raw().instance_ctx.map_difficulty;
        auto it = generated_routes_.find(MakeKey(s.map_id(), diff));
        if (it == generated_routes_.end() && diff != 0)
            it = generated_routes_.find(MakeKey(s.map_id(), 0));
        if (it != generated_routes_.end())
            merged.route_waypoints = it->second;
    }
    return merged;
}

size_t DungeonScriptMgr::LoadGeneratedRoutes()
{
    generated_routes_.clear();
    QueryResult result = CharacterDatabase.Query(fmt::format(
        "SELECT map_id, difficulty, position_x, position_y, position_z "
        "FROM {}.playerbot_dungeon_routes ORDER BY map_id, difficulty, seq",
        SharedDb()).c_str());
    if (!result)
    {
        TC_LOG_INFO("playerbot.v2",
            "[DungeonRoutes] {}.playerbot_dungeon_routes empty/absent — no generated waypoints.",
            SharedDb());
        return 0;
    }
    size_t count = 0;
    do
    {
        Field* f = result->Fetch();
        const uint32_t map  = f[0].GetUInt16();
        const uint32_t diff = f[1].GetUInt8();
        generated_routes_[MakeKey(map, diff)].push_back(
            DungeonAdvice::ProgressionPoint{ f[2].GetFloat(), f[3].GetFloat(), f[4].GetFloat() });
        ++count;
    } while (result->NextRow());
    TC_LOG_INFO("playerbot.v2",
        "[DungeonRoutes] loaded {} generated route waypoint(s) across {} dungeon(s).",
        count, generated_routes_.size());
    return count;
}

} // namespace Playerbot
