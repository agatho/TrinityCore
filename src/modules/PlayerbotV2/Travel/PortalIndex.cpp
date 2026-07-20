#include "PortalIndex.h"
#include "RegionMapper.h"
#include "ConditionMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "GameObjectData.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SharedDefines.h"
#include "TransportMgr.h"
#include <limits>
#include <queue>
#include <unordered_set>

namespace Playerbot::V2::Travel {

// Resolve a SPELLCASTER portal's teleport destination, following the
// FORCE_CAST / TRIGGER_SPELL indirection that modern portals use. The GO
// casts a "portal effect" spell whose own effect is a force-cast or trigger
// (not a teleport); that spell in turn casts the real TELEPORT_UNITS spell
// whose spell_target_position holds the landing. Example: Orgrimmar's
// "Portal to Undercity" GO (293684) casts 17611 (FORCE_CAST) -> 121862
// (TELEPORT_UNITS -> map 0, Undercity). The older direct-spell-only lookup
// missed every such indirect portal. Bounded recursion guards trigger cycles.
// Declared in PortalIndex.h; shared with the snapshot builder.
bool ResolvePortalSpellDest(uint32 spell_id, uint32& dest_map,
                            float& dest_x, float& dest_y, float& dest_z, int depth)
{
    if (depth > 4) return false;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!si) return false;

    // Direct teleport effect on this spell.
    for (SpellEffectInfo const& eff : si->GetEffects())
    {
        if (eff.Effect != SPELL_EFFECT_TELEPORT_UNITS &&
            eff.Effect != SPELL_EFFECT_TELEPORT_WITH_SPELL_VISUAL_KIT_LOADING_SCREEN)
            continue;
        if (SpellTargetPosition const* tp =
                sSpellMgr->GetSpellTargetPosition(spell_id, eff.EffectIndex))
        {
            dest_map = tp->GetMapId();
            dest_x   = tp->GetPositionX();
            dest_y   = tp->GetPositionY();
            dest_z   = tp->GetPositionZ();
            return true;
        }
    }

    // Indirect: the spell force-casts / triggers another spell that teleports.
    for (SpellEffectInfo const& eff : si->GetEffects())
    {
        switch (eff.Effect)
        {
            case SPELL_EFFECT_TRIGGER_SPELL:
            case SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE:
            case SPELL_EFFECT_FORCE_CAST:
            case SPELL_EFFECT_FORCE_CAST_WITH_VALUE:
            case SPELL_EFFECT_FORCE_CAST_2:
                if (uint32 trig = eff.TriggerSpell)
                    if (ResolvePortalSpellDest(trig, dest_map, dest_x, dest_y, dest_z, depth + 1))
                        return true;
                break;
            default:
                break;
        }
    }
    return false;
}

bool PortalIndex::Initialize()
{
    _anchors.clear();
    _anchors.reserve(256);

    auto const& spawns = sObjectMgr->GetAllGameObjectData();
    for (auto const& [spawn_id, data] : spawns)
    {
        GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(data.id);
        if (!tmpl) continue;

        if (tmpl->type == GAMEOBJECT_TYPE_SPELLCASTER)
        {
            // Portal: GO triggers a spell whose effect[0] is a teleport.
            // The destination is in spell_target_position. Most static
            // SPELLCASTER GOs are altar / event triggers without a
            // teleport effect — they resolve to nullptr here and we skip.
            const uint32 spell_id = tmpl->spellCaster.spell;
            if (!spell_id) continue;
            uint32 dest_map = 0;
            float  dest_x = 0.f, dest_y = 0.f, dest_z = 0.f;
            // Resolve the teleport destination, following FORCE_CAST /
            // TRIGGER_SPELL indirection (modern portals cast a force-cast
            // spell that in turn casts the real teleport). dest_x/dest_y are
            // the arrival point so BuildPortalEdges can pick the landing node
            // nearest the goal rather than the first best-rank.
            if (!ResolvePortalSpellDest(spell_id, dest_map, dest_x, dest_y, dest_z))
                continue;
            // Do NOT test `!dest_map`: map 0 (Eastern Kingdoms) is a valid
            // destination, and treating 0 as "unset" silently dropped every
            // portal that lands in EK (Undercity, Twilight Highlands, ...).
            // Only same-map "portals" (altars / effect casters) are skipped.
            if (dest_map == data.mapId) continue;
            PortalAnchor a;
            a.source_map = data.mapId;
            a.dest_map   = dest_map;
            a.x = data.spawnPoint.GetPositionX();
            a.y = data.spawnPoint.GetPositionY();
            a.z = data.spawnPoint.GetPositionZ();
            a.dest_x = dest_x;
            a.dest_y = dest_y;
            a.go_entry     = data.id;
            a.kind         = PortalAnchor::Kind::Portal;
            a.condition_id = tmpl->spellCaster.conditionID1;
            _anchors.push_back(a);
        }
        else if (tmpl->type == GAMEOBJECT_TYPE_TRANSPORT)
        {
            // Type 11 — local transports (elevators, lifts, instance
            // shuttles). These ARE in `gameobject` so their spawnPoint
            // is meaningful. We don't route bots through these for
            // cross-map travel: they stay on one map (no MapIds set).
            // Skip — handled implicitly by walking onto them when the
            // bot is already at their location.
            continue;
        }
    }

    // ---- Continental transports (ships / zeppelins, GO type 15) ----
    // These are NOT in `gameobject`; their spawns live in the separate
    // `transports` SQL table and they only have positions while running
    // their path animation. Static dock waypoints come from the path's
    // TaxiPathNode entries with TAXI_PATH_NODE_FLAG_STOP — those are
    // the rendezvous coordinates the bot needs to walk to in order to
    // board.
    //
    // We iterate every gameobject_template with type=15, look up its
    // taxi path, walk the path nodes, and for every (STOP_node A, STOP_
    // node B) pair where A and B are on different continents, add a
    // (source=A.continent, dest=B.continent, x/y/z=A.Loc) anchor.
    {
        size_t pre_continental = _anchors.size();
        // Only ACTUALLY-SPAWNED transports (rows in the `transports` table) run
        // in the world. The gameobject_template list still carries decommissioned
        // routes — e.g. the Undercity zeppelin 164871 "The Thundercaller", whose
        // Org→Tirisfal route was removed when Undercity became a post-BfA ruin.
        // Building an anchor for it sent a Tirisfal-bound bot (Somi) to the dead
        // tower to wait forever for a zeppelin that never spawns (verified live:
        // 164871 absent from `transports`; 0 sightings over minutes while the
        // running 175080/186238 cycled normally). Gate on the spawn table so the
        // travel graph only routes through transports that physically exist.
        std::unordered_set<uint32> spawned_transport_entries;
        if (QueryResult res = WorldDatabase.Query("SELECT DISTINCT entry FROM transports"))
        {
            do { spawned_transport_entries.insert((*res)[0].GetUInt32()); } while (res->NextRow());
        }
        TC_LOG_INFO("server.loading",
            "[PlayerbotV2 PortalIndex] {} spawned transports gate continental anchors.",
            spawned_transport_entries.size());
        auto const& templates = sObjectMgr->GetGameObjectTemplates();
        for (auto const& [entry, tmpl] : templates)
        {
            if (tmpl.type != GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT) continue;
            if (!spawned_transport_entries.count(entry)) continue;  // decommissioned / never spawned
            const uint32 path_id = tmpl.moTransport.taxiPathID;
            if (path_id == 0 || path_id >= sTaxiPathNodesByPath.size()) continue;
            auto const& nodes = sTaxiPathNodesByPath[path_id];
            // Collect STOP-flagged nodes (the docks where the transport
            // pauses for boarding/disembark). Continental ships have
            // 2-4 stops; a single per-map STOP is the bot's rendezvous.
            std::vector<TaxiPathNodeEntry const*> stops;
            stops.reserve(8);
            for (TaxiPathNodeEntry const* node : nodes)
            {
                if (!node) continue;
                if ((node->Flags & TAXI_PATH_NODE_FLAG_STOP) == 0) continue;
                stops.push_back(node);
            }
            if (stops.size() < 2) continue;
            // DIAG (2026-06-02): dump the REAL stop-waypoint positions per live
            // 12.0 transport. These are the coords bots are sent to in order to
            // board — we need to see which sit in water (z near sea level ~0) vs
            // on a pier/platform, to design a general dock-approach that doesn't
            // strand bots swimming. One line per stop; reads from Server.log.
            for (TaxiPathNodeEntry const* sn : stops)
                TC_LOG_INFO("server.loading",
                    "[dock_dump] entry={} '{}' path={} stop map={} pos=({:.1f},{:.1f},{:.1f})",
                    entry, tmpl.name, path_id, sn->ContinentID,
                    sn->Loc.X, sn->Loc.Y, sn->Loc.Z);
            for (TaxiPathNodeEntry const* src_node : stops)
            {
                for (TaxiPathNodeEntry const* dst_node : stops)
                {
                    if (src_node == dst_node) continue;
                    if (src_node->ContinentID == dst_node->ContinentID) continue;
                    PortalAnchor a;
                    a.source_map = src_node->ContinentID;
                    a.dest_map   = dst_node->ContinentID;
                    a.x = src_node->Loc.X;
                    a.y = src_node->Loc.Y;
                    a.z = src_node->Loc.Z;
                    // Curated pier WAIT override (operator in-client ground
                    // truth). Boat STOP waypoints sit on the open water surface
                    // (z~0), so a bot walking to the raw anchor ends up standing
                    // ON the water instead of on the pier where players board.
                    // Snap the WAIT position to the real pier; dest_x/y (arrival
                    // on the far map) stays the STOP, and the docked ship's
                    // board-scan still reaches the bot on the pier (State_Idle
                    // boards within a generous transport range). Matched to the
                    // STOP by 2D proximity on the same map.
                    {
                        // Operator-provided pier WAIT spots, matched to a boat STOP
                        // by 2D proximity (60y) on the same map.
                        struct PierOverride { uint32 map; float near_x, near_y, wx, wy, wz; };
                        static constexpr PierOverride kPiers[] = {
                            // Stormwind Harbor — The Bravery (Boralus / Dragon Isles).
                            { 0,   -8650.0f, 1346.0f, -8644.78f, 1328.72f, 5.54f },
                            // Stormwind Harbor — Stormwind's Pride (Valiance Keep / NR).
                            { 0,   -8288.0f, 1424.0f, -8291.01f, 1408.10f, 4.70f },
                            // Valiance Keep (Northrend) — Stormwind boat dock.
                            { 571,  2218.0f, 5119.0f,  2229.76f, 5130.87f, 5.34f },
                            // Ratchet (Kalimdor) — Booty Bay boat dock.
                            { 1,    -994.0f, -3827.0f,  -995.53f, -3829.46f, 5.61f },
                            // Booty Bay (Eastern Kingdoms) — Ratchet boat dock.
                            { 0,  -14281.0f,  556.0f, -14282.50f,  563.47f, 7.84f },
                            // Waking Shores (Dragon Isles) — Stormwind boat dock.
                            { 2444, 3736.0f, -1901.0f,  3736.29f, -1901.36f, 5.88f },
                            // Boralus Harbor (Kul Tiras) — Stormwind boat dock.
                            { 1643, 1022.0f,  -657.0f,  1022.74f,  -657.06f, 6.46f },
                            // Orgrimmar has TWO zeppelin towers (~64y apart); raw
                            // STOPs are the unreachable z152 float points, wait on
                            // the walkable upper platforms at z135.
                            // Tower 1 -> Tirisfal/Undercity.
                            { 1,    1833.0f, -4391.0f,  1841.36f, -4392.05f, 135.23f },
                            // Tower 2 -> Grom'gol/Stranglethorn.
                            { 1,    1880.0f, -4435.0f,  1868.68f, -4418.04f, 135.23f },
                        };
                        // Tight match radius (30y): boat STOPs are far apart, but the
                        // two Org zeppelin STOPs are only ~64y apart, so a wide radius
                        // would route both towers' bots to one platform.
                        for (auto const& po : kPiers)
                        {
                            if (a.source_map != po.map) continue;
                            const float pdx = a.x - po.near_x, pdy = a.y - po.near_y;
                            if (pdx * pdx + pdy * pdy <= 30.0f * 30.0f)
                            { a.x = po.wx; a.y = po.wy; a.z = po.wz; break; }
                        }
                    }
                    // T-P2b: arrival dock coords on the destination map.
                    a.dest_x = dst_node->Loc.X;
                    a.dest_y = dst_node->Loc.Y;
                    a.go_entry     = entry;
                    a.kind         = PortalAnchor::Kind::Transport;
                    a.condition_id = 0;
                    _anchors.push_back(a);
                }
            }
        }
        TC_LOG_INFO("server.loading",
            "[PlayerbotV2 PortalIndex] Added {} continental-transport dock anchors.",
            _anchors.size() - pre_continental);
    }

    // Tag every anchor with its source/destination REGION (RegionMapper), so
    // FindNearest can route to the correct landmass on physically-split maps
    // (530 Outland/Azuremyst/Eversong; 1 Kalimdor/Teldrassil). dest_region is
    // left 0 when the landing coords are unknown (dest_x==0&&dest_y==0) — the
    // FindNearest fallback treats those as "any region" so a route is never lost.
    {
        uint32 region_tagged = 0;
        for (PortalAnchor& a : _anchors)
        {
            a.source_region = RegionForPosition(a.source_map, a.x, a.y);
            a.dest_region   = (a.dest_x != 0.f || a.dest_y != 0.f)
                ? RegionForPosition(a.dest_map, a.dest_x, a.dest_y)
                : 0u;
            if (a.source_region != 0 || a.dest_region != 0) ++region_tagged;
        }
        TC_LOG_INFO("server.loading",
            "[PlayerbotV2 PortalIndex] Region-tagged anchors: {} in a non-default region.",
            region_tagged);
    }

    _anchors.shrink_to_fit();
    BuildGraph();
    _initialized = true;
    TC_LOG_INFO("server.loading",
        "[PlayerbotV2 PortalIndex] Built {} anchors from {} gameobject spawns; "
        "graph has {} source maps.",
        _anchors.size(), spawns.size(), _adjacency.size());
    return true;
}

void PortalIndex::BuildGraph()
{
    _adjacency.clear();
    // Memoization is keyed off the adjacency table; rebuilding the
    // graph invalidates every cached lookup.
    _next_hop_cache.clear();
    // Each anchor contributes one directed edge source_map → dest_map.
    // De-dup destinations per source so BFS visits each adjacent map
    // once. Static portals tend to be one-way (Stormwind → Dalaran is a
    // separate portal from Dalaran → Stormwind), so we don't synthesize
    // a reverse edge — if the world has both directions wired the
    // adjacency captures both naturally.
    for (PortalAnchor const& a : _anchors)
    {
        if (a.source_map == a.dest_map) continue;
        auto& adj = _adjacency[a.source_map];
        bool seen = false;
        for (uint32 m : adj) if (m == a.dest_map) { seen = true; break; }
        if (!seen) adj.push_back(a.dest_map);
    }
}

uint32 PortalIndex::NextHopMap(uint32 from_map, uint32 to_map) const
{
    if (from_map == to_map) return kNoMap;
    if (_adjacency.empty()) return kNoMap;

    // Memoization cache. The adjacency graph is static after Initialize() so any
    // (from, to) → next_hop pair is permanent. Without this, the BFS allocated
    // three containers per call — at 2000 bots with off-map quest goals common,
    // hundreds of heap allocs per snapshot tick. Cache hit avoids all of that.
    //
    // CONCURRENCY: NextHopMap is called from the Phase 4 parallel snapshot-build
    // workers (BotSnapshotBuilder::Build on up to 24 threads). The cache must be
    // synchronised — an unsynchronised emplace from two workers corrupts the
    // bucket list and spins emplace forever. shared_lock for the hit fast path
    // (full read parallelism); the BFS for a miss runs OUTSIDE the lock (reads
    // only the immutable _adjacency); a unique_lock publishes the result.
    const uint64 cache_key = (uint64(from_map) << 32) | uint64(to_map);
    {
        std::shared_lock<std::shared_mutex> rlk(_next_hop_mtx);
        if (auto cit = _next_hop_cache.find(cache_key); cit != _next_hop_cache.end())
            return cit->second;
    }

    const uint32 result = ComputeNextHopUncached(from_map, to_map);

    // Publish. emplace is a no-op if another worker computed the same key in the
    // race window between our shared-lock miss and this unique-lock — the value
    // is deterministic for a fixed graph, so either insertion is correct.
    {
        std::unique_lock<std::shared_mutex> wlk(_next_hop_mtx);
        _next_hop_cache.emplace(cache_key, result);
    }
    return result;
}

uint32 PortalIndex::ComputeNextHopUncached(uint32 from_map, uint32 to_map) const
{
    // Direct edge fast-path: if there is any anchor from→to, the greedy rule
    // already finds it; report to_map so the caller's logic stays consistent
    // ("walk toward an anchor whose dest_map == NextHopMap result"). Avoids a
    // BFS sweep when not needed.
    auto it = _adjacency.find(from_map);
    if (it == _adjacency.end())
        return kNoMap;
    for (uint32 m : it->second)
        if (m == to_map)
            return to_map;

    // BFS from from_map; track the FIRST hop taken on each path so we can return
    // it when the goal is reached. Cap traversal as a safety bound; real WoW
    // topology is at most ~3-4 hops between any two maps.
    std::unordered_map<uint32, uint32> first_hop;  // map_visited → first_hop_taken
    std::unordered_set<uint32> visited;
    std::queue<uint32> queue;
    visited.insert(from_map);
    for (uint32 hop : it->second)
    {
        if (visited.count(hop)) continue;
        visited.insert(hop);
        first_hop[hop] = hop;
        queue.push(hop);
    }
    constexpr size_t kMaxIterations = 1024;
    size_t iter = 0;
    while (!queue.empty() && iter < kMaxIterations)
    {
        ++iter;
        uint32 cur = queue.front();
        queue.pop();
        if (cur == to_map)
            return first_hop[cur];
        auto it2 = _adjacency.find(cur);
        if (it2 == _adjacency.end()) continue;
        for (uint32 nxt : it2->second)
        {
            if (visited.count(nxt)) continue;
            visited.insert(nxt);
            first_hop[nxt] = first_hop[cur];   // inherit the first-hop tag
            queue.push(nxt);
        }
    }
    return kNoMap;  // no route
}

PortalAnchor const* PortalIndex::FindNearest(
    Player const* bot,
    uint32 source_map, uint32 dest_map,
    float bot_x, float bot_y,
    uint32 dest_region,
    float goal_x, float goal_y) const
{
    // Region filter only matters on a map that is physically split into
    // multiple unreachable landmasses (530, 1, ...). On normal maps every
    // anchor's dest_region is 0 and the filter is a no-op.
    const bool region_filter =
        dest_region != kRegionAny && MapHasMultipleRegions(dest_map);
    // When a goal position is known, ALSO track the dock whose ARRIVAL is
    // nearest the goal. Eastern Kingdoms (map 0) is one connected continent so
    // the region filter above is a no-op there, yet the Org tower's two map-0
    // zeppelins land a continent apart (Tirisfal vs Stranglethorn). Picking by
    // board-distance alone sent a Tirisfal bot to whichever was nearest to board
    // (Grom'gol). Preferring the nearest-arrival dock fixes that without
    // affecting single-dock destinations.
    const bool use_goal = (goal_x != 0.f || goal_y != 0.f);
    PortalAnchor const* best = nullptr;
    float best_distSq = std::numeric_limits<float>::max();
    PortalAnchor const* best_by_goal = nullptr;
    float best_goalSq = std::numeric_limits<float>::max();
    for (PortalAnchor const& a : _anchors)
    {
        if (a.source_map != source_map) continue;
        if (a.dest_map   != dest_map)   continue;
        // Region gate: on a split dest map, the anchor must LAND in the goal's
        // region. Anchors with unknown landing coords (dest_x==0&&dest_y==0,
        // so dest_region couldn't be resolved) are accepted as a fallback so we
        // never drop the only available route into a region.
        if (region_filter && (a.dest_x != 0.f || a.dest_y != 0.f) &&
            a.dest_region != dest_region)
            continue;
        // Faction / level / rep gate. condition_id == 0 means anyone
        // can use; otherwise the bot must satisfy the PlayerCondition.
        // ConditionMgr::IsPlayerMeetingCondition looks up the entry,
        // returns true on missing IDs (logs a warning), so unknown
        // condition rows are treated as "no filter" — matches server
        // behaviour at GO use time. Skipping this check would route a
        // Horde bot toward an Alliance-only Stormwind portal; the bot
        // would arrive but the GO would be invisible (or use would
        // bounce server-side).
        if (a.condition_id != 0 && bot != nullptr &&
            !ConditionMgr::IsPlayerMeetingCondition(bot, a.condition_id))
            continue;
        const float dx = a.x - bot_x;
        const float dy = a.y - bot_y;
        const float dsq = dx * dx + dy * dy;
        if (dsq < best_distSq)
        {
            best_distSq = dsq;
            best = &a;
        }
        if (use_goal && (a.dest_x != 0.f || a.dest_y != 0.f))
        {
            const float gdx = a.dest_x - goal_x;
            const float gdy = a.dest_y - goal_y;
            const float gdsq = gdx * gdx + gdy * gdy;
            if (gdsq < best_goalSq)
            {
                best_goalSq = gdsq;
                best_by_goal = &a;
            }
        }
    }
    // Prefer the dock that LANDS nearest the goal (when arrivals are known);
    // otherwise the dock nearest to BOARD. For a single-dock destination these
    // resolve to the same anchor, so existing routes are unchanged.
    return best_by_goal ? best_by_goal : best;
}

} // namespace Playerbot::V2::Travel
