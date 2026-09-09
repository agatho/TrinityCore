// PortalIndex — global cache of every cross-map travel anchor in the world:
// static portal GOs (GAMEOBJECT_TYPE_SPELLCASTER with a teleport spell),
// continental ships / zeppelins (GAMEOBJECT_TYPE_TRANSPORT and
// GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT). Built once at module init by walking
// `sObjectMgr->GetAllGameObjectData()`, resolving each spawn's destination
// via `spell_target_position` (for portals) or `TransportTemplate::MapIds`
// (for transports). Read-only afterwards.
//
// Drives the cross-map cascade's "walk to a portal I can't see yet" leg.
// Without this, a Stormwind bot with a Northrend goal can't pathfind to
// the Stormwind portal room (the goal POI is across the world, the portal
// itself is invisible past 30 y) — the bot wanders aimlessly until it
// stumbles into the portal-room interior. With this, the bot computes
// "nearest known portal on my map that reaches the goal map" each
// snapshot and walks there.

#pragma once

#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class Player;

namespace Playerbot::V2::Travel {

// Resolve a SPELLCASTER portal spell's teleport destination, following the
// FORCE_CAST / TRIGGER_SPELL indirection modern portals use: the GO casts a
// "portal effect" spell whose own effect is a force-cast/trigger (NOT a
// teleport), which in turn casts the real TELEPORT_UNITS spell whose
// spell_target_position holds the landing (e.g. Orgrimmar's Undercity portal
// GO 293684 -> spell 17611 FORCE_CAST -> 121862 TELEPORT_UNITS -> map 0).
// Returns true and fills dest_map/x/y/z when a teleport destination resolves.
// NOTE: dest_map 0 (Eastern Kingdoms) is VALID — callers must branch on the
// bool return, never on `!dest_map`. Bounded recursion guards trigger cycles.
// Shared by PortalIndex (anchor build) and the snapshot builder (per-GO
// teleport_dest_map) so the two stay in lockstep.
bool ResolvePortalSpellDest(uint32 spell_id, uint32& dest_map,
                            float& dest_x, float& dest_y, float& dest_z,
                            int depth = 0);

struct PortalAnchor
{
    enum class Kind : uint8 { Portal = 1, Transport = 2 };

    uint32 source_map = 0;       // map this anchor stands on
    uint32 dest_map   = 0;       // map a user reaches by using it
    float  x = 0.f, y = 0.f, z = 0.f;
    // T-P2b: arrival position on dest_map (portal SpellTargetPosition, or
    // the destination dock's stop coords for transports). Used by
    // UnifiedTravelGraph::BuildPortalEdges to break landing-node rank ties
    // by 2D distance to where the bot actually arrives, instead of picking
    // an arbitrary first best-rank node that may be across the map.
    // 0,0 means "unknown" (no tiebreak applied) — never observed for a
    // resolved anchor but kept defensive.
    float  dest_x = 0.f, dest_y = 0.f;
    // Region ids (RegionMapper) for the anchor's own position and its landing
    // position. Maps like 530 (Outland/Azuremyst/Eversong) and 1 (Kalimdor/
    // Teldrassil) are one mapId but physically disconnected; routing must match
    // the goal's REGION, not just its map. dest_region lets FindNearest pick the
    // portal/dock that lands in the goal's landmass (e.g. the Exodar portal for
    // a Bloodmyst goal, not the Shattrath portal that lands in Outland).
    // source_region = RegionForPosition(source_map, x, y);
    // dest_region   = RegionForPosition(dest_map, dest_x, dest_y) (0 if dest
    // coords unknown). 0 = main/global region.
    uint32 source_region = 0;
    uint32 dest_region   = 0;
    uint32 go_entry   = 0;       // gameobject_template entry (diagnostics)
    Kind   kind       = Kind::Portal;
    // PlayerCondition that gates use. 0 means "no condition / anyone".
    // The most common case for portals is a faction filter (Alliance-only
    // Stormwind portal room, Horde-only Orgrimmar etc). Evaluated per-bot
    // at FindNearest time via ConditionMgr::IsPlayerMeetingCondition,
    // so the same anchor is offered to one bot and skipped for another
    // depending on faction/level/rep/whatever the spawn requires.
    uint32 condition_id = 0;
};

class PortalIndex
{
public:
    PortalIndex() = default;

    // Idempotent. Walks every gameobject spawn, filters to types that move
    // players between maps, and resolves the destination via Spell or
    // Transport metadata. Logs an error and skips entries that don't
    // resolve cleanly. Call from the module-init thread (queries
    // sObjectMgr/sSpellMgr/sTransportMgr which are read-only by then).
    bool Initialize();

    bool IsInitialized() const { return _initialized; }
    size_t GetAnchorCount() const { return _anchors.size(); }

    // Read-only view of every loaded anchor. Used by UnifiedTravelGraph
    // to seed Portal/Dock nodes — we want every anchor in the unified
    // graph so cross-map routing can compose ship→portal→walk hops.
    // Safe to iterate from any thread post-Initialize; the vector is
    // never mutated after Initialize returns.
    std::vector<PortalAnchor> const& Anchors() const { return _anchors; }

    // Closest anchor on `source_map` whose `dest_map` matches the goal.
    // Returns nullptr if no anchor exists for that pair. O(N) over the
    // index — N is bounded by the number of physical portal/transport
    // spawns in the world (low hundreds), so this is microseconds and
    // safe to call from the snapshot builder per-tick.
    // `bot` is the requesting player — used to evaluate per-anchor
    // PlayerCondition (faction, level, rep). Pass nullptr to skip the
    // condition filter (diagnostics / unit tests).
    // `dest_region` (RegionMapper id of the GOAL position on dest_map) makes
    // selection region-aware on split maps: only anchors whose LANDING is in
    // the goal's region are eligible (e.g. the Exodar portal for a Bloodmyst
    // goal, not the Shattrath portal that lands in Outland — both "→530").
    // Pass kRegionAny to disable the region filter (default, back-compat).
    static constexpr uint32 kRegionAny = 0xFFFFFFFFu;
    // "No next hop" sentinel for NextHopMap. Map id 0 (Eastern Kingdoms) is a
    // VALID hop, so 0 must not mean "none" — see Playerbot::kInvalidMapId.
    static constexpr uint32 kNoMap = 0xFFFFFFFFu;
    PortalAnchor const* FindNearest(
        Player const* bot,
        uint32 source_map, uint32 dest_map,
        float  bot_x, float bot_y,
        uint32 dest_region = kRegionAny,
        // Goal position on dest_map. When set (non-zero) and several docks reach
        // dest_map with KNOWN arrival points, prefer the one that LANDS nearest
        // the goal instead of the one nearest to BOARD. Critical when one map has
        // two docks to far-apart spots (the Org tower's Undercity vs Grom'gol
        // zeppelins both land on map 0, but in Tirisfal vs Stranglethorn — a
        // continent apart): a Tirisfal-bound bot must take the Undercity zeppelin
        // even if the Grom'gol one is slightly closer to board. No effect on
        // single-dock destinations (the same anchor wins either way).
        float  goal_x = 0.f, float goal_y = 0.f) const;

    // ---- TravelPlanner Phase E: multi-hop routing -----------------
    // Given (from_map → to_map), return the IMMEDIATE next-hop map id
    // along the shortest path through the portal graph. If a direct
    // anchor exists (from→to), returns to_map. If no path exists, or
    // the bot is already on to_map, returns 0 (caller's pre-checks
    // should already cover the trivial cases). The graph is built
    // once at Initialize() time as an unweighted-BFS adjacency table
    // — N anchors → N adjacency edges, ~50-100 maps total, lookup is
    // a hash + small queue per call (microseconds). Faction/rep/quest
    // gating is NOT applied here (we don't know which bot is asking);
    // FindNearest's PlayerCondition filter handles per-bot eligibility
    // at the leg-walk step. A planner result of `mapX` means "any
    // bot can theoretically traverse this map next" — if the per-bot
    // anchor for that hop is gated out, FindNearest returns null and
    // the bot falls back to greedy (current behaviour).
    uint32 NextHopMap(uint32 from_map, uint32 to_map) const;

private:
    void BuildGraph();
    // Pure BFS over the (immutable post-Initialize) adjacency graph. No cache
    // access, no locking — safe to call from any thread concurrently because it
    // only READS _adjacency. NextHopMap wraps this with the memo cache + lock.
    uint32 ComputeNextHopUncached(uint32 from_map, uint32 to_map) const;
    std::vector<PortalAnchor> _anchors;
    // map_id → list of map_ids reachable from it (one anchor away).
    // Built from _anchors at Initialize time.
    std::unordered_map<uint32, std::vector<uint32>> _adjacency;
    // (from << 32 | to) → next-hop map id memoization. The adjacency
    // graph is static post-Initialize, so the cache is sound for the
    // whole session. Mutable because NextHopMap is logically `const`
    // (idempotent lookup) but writes the cache on miss. Bounded by
    // the (small) cross-product of map ids the world references —
    // typically tens of entries, not thousands.
    mutable std::unordered_map<uint64, uint32> _next_hop_cache;
    // Guards _next_hop_cache ONLY. NextHopMap is called concurrently from the
    // Phase 4 parallel snapshot-build workers (BotSnapshotBuilder::Build runs on
    // up to 24 threads); an unsynchronised emplace into _next_hop_cache from two
    // workers corrupts the bucket list and spins emplace forever (observed as a
    // FreezeDetector world-thread hang, 2026-06-15). shared_lock for the cache
    // hit fast path (overwhelmingly common after warmup → full read parallelism),
    // unique_lock only to publish a freshly computed miss. The BFS itself runs
    // OUTSIDE the lock (reads only the immutable _adjacency).
    mutable std::shared_mutex _next_hop_mtx;
    bool _initialized = false;
};

} // namespace Playerbot::V2::Travel
