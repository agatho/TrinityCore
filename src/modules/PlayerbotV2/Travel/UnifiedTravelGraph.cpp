#include "UnifiedTravelGraph.h"

#include "PlayerbotMovement.h"   // SehSafeCalculatePath (tile-race guard)
#include "PortalIndex.h"
#include "QuestHubDatabase.h"
#include "ElevatorIndex.h"
#include "../Services.h"
#include "../Bot/World/CapitalsTable.h"

#include "ConditionMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>

namespace Playerbot::V2::Travel {

namespace {

// Cost model. Units are "travel seconds" so kinds can be mixed in one A*.
// Calibration assumes running ~7 yd/s on foot, ~30 yd/s on a flight path.
// We're not chasing wall-clock precision — only relative ordering matters
// so the router picks ship-then-walk over taxi-around-the-continent when
// it actually is faster.
constexpr float kWalkSpeedYds   = 7.0f;
constexpr float kFlightSpeedYds = 30.0f;
// Fixed costs (cast time + loading screen).
constexpr float kPortalCost     = 5.0f;     // cast + zone
constexpr float kShipBoardCost  = 60.0f;    // half a typical 2-min period
constexpr float kHearthCost     = 12.0f;    // 10s cast + small zone fee
constexpr float kTeleportCost   = 3.0f;     // dungeon trigger
// Elevator ride: half the average platform period (~15s) + a couple
// seconds for stepping on/off. Calibrated below ship cost so the
// router prefers a local elevator over a continental dock when both
// reach the same Z; well above walk so a 50y direct path beats riding
// when the navmesh already connects the floors.
constexpr float kElevatorCost   = 15.0f;
// Third-party transit penalty: a one-time toll for ENTERING a map that is
// neither the start nor the goal map. Without it the flat portal cost
// (kPortalCost=5, distance-independent) turns the global portal network into a
// free cross-continent teleport mesh, and the A* below produces geographically
// absurd routes — observed live (2026-06-19, Tindle): a L5 Stormwind->Dun
// Morogh trip (both map 0, ~3,400y) routed through the DRAGON ISLES (map 2444)
// via 2 flat-cost portals + 4 short taxis because that undercut the one long
// same-continent flight (~125s) in cost units. The bot can neither survive nor
// execute it, mills at the first portal anchor, and Tier-3 stuck-recovery then
// blacklists the quest. A point-to-point trip legitimately touches AT MOST the
// start and goal maps (e.g. EK->Kalimdor by a single boat); a third map is
// never a required waypoint when a direct route exists. 600s >> any real
// same-map flight, so a direct route always wins, yet finite so a genuinely
// required transit (the only path to some island/hub) stays routable.
constexpr float kTransitMapPenalty = 600.0f;
// Soft cap on walk edges: don't connect two same-map nodes more than
// this far apart at the walk-edge build pass — the result is a path
// the navmesh can't realize. ConnectIslands() patches the longer hops
// via Bridge nodes.
constexpr float kMaxWalkEdgeYds = 600.0f;
// Max vertical gap a same-map walk edge may span. Above this the only routable
// path between the two floors is an EdgeKind::Elevator edge (see BuildWalkEdges).
// 18y: an elevated FM (z~103) still links to the lift's TOP stop (z~90, dz 13)
// but never to the ground / bottom stop (dz 90+); comfortably above any real
// ramp's rise across a 600y edge.
constexpr float kMaxWalkEdgeDz = 18.0f;

inline float Dist2D(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx*dx + dy*dy);
}

// Faction mask helper for taxi nodes.
uint32 TaxiFactionFromFlags(int32 flags)
{
    // TaxiNodeFlags::ShowOnAllianceMap = 0x1, ShowOnHordeMap = 0x2.
    const bool alli  = (flags & 0x1) != 0;
    const bool horde = (flags & 0x2) != 0;
    if (alli && !horde) return ALLIANCE;
    if (horde && !alli) return HORDE;
    return 0;   // both or neither → neutral
}

} // anonymous

void UnifiedTravelGraph::Initialize()
{
    std::unique_lock lk(_mtx);
    _nodes.clear();
    _adj.clear();
    _nodes_by_map.clear();

    LoadFlightMasters();
    LoadPortalsAndDocks();
    LoadDungeonEntries();
    LoadAreaTriggerTeleports();
    LoadCapitals();
    LoadQuestHubs();
    LoadElevators();

    BuildTaxiEdges();
    BuildPortalEdges();
    BuildElevatorEdges();
    BuildWalkEdges();
    ConnectIslands();

    _initialized = true;

    KindCounts counts = {};
    for (auto const& n : _nodes)
    {
        switch (n.kind)
        {
            case NodeKind::FlightMaster: ++counts.flight_masters; break;
            case NodeKind::Portal:       ++counts.portals;        break;
            case NodeKind::Dock:         ++counts.docks;          break;
            case NodeKind::DungeonEntry: ++counts.dungeon_entries; break;
            case NodeKind::Capital:      ++counts.capitals;       break;
            case NodeKind::QuestHub:     ++counts.quest_hubs;     break;
            case NodeKind::Bridge:       ++counts.bridges;        break;
            case NodeKind::ElevatorStop: ++counts.elevator_stops; break;
            default: break;
        }
    }
    size_t total_edges = 0;
    for (auto const& a : _adj) total_edges += a.size();

    TC_LOG_INFO("playerbot.v2",
        "[UnifiedTravelGraph] Initialized: {} nodes ({} FM / {} Portal / {} Dock / "
        "{} DungeonEntry / {} Capital / {} QuestHub / {} Bridge / {} ElevatorStop), "
        "{} edges, {} maps.",
        _nodes.size(),
        counts.flight_masters, counts.portals, counts.docks,
        counts.dungeon_entries, counts.capitals, counts.quest_hubs,
        counts.bridges, counts.elevator_stops,
        total_edges, _nodes_by_map.size());
}

NodeId UnifiedTravelGraph::AppendNode(GraphNode node)
{
    const NodeId id = NodeId(_nodes.size());
    node.id = id;
    _nodes_by_map[node.map_id].push_back(id);
    _nodes.push_back(std::move(node));
    _adj.emplace_back();
    return id;
}

void UnifiedTravelGraph::AddEdge(NodeId from, GraphEdge edge)
{
    if (from >= _adj.size()) return;
    _adj[from].push_back(edge);
}

// ---------------------------------------------------------------- Loaders

void UnifiedTravelGraph::LoadFlightMasters()
{
    // sTaxiNodesStore: every flight master in the world. We skip nodes
    // not part of the visible network (`IsPartOfTaxiNetwork()` filters
    // out the "hidden" scenario hubs that real players never touch).
    uint32 added = 0;
    for (TaxiNodesEntry const* tn : sTaxiNodesStore)
    {
        if (!tn) continue;
        if (!tn->IsPartOfTaxiNetwork()) continue;
        GraphNode n;
        n.kind         = NodeKind::FlightMaster;
        n.map_id       = tn->ContinentID;
        n.x            = tn->Pos.X;
        n.y            = tn->Pos.Y;
        n.z            = tn->Pos.Z;
        n.condition_id = uint32(tn->ConditionID > 0 ? tn->ConditionID : 0);
        n.faction      = TaxiFactionFromFlags(tn->Flags);
        n.name         = tn->Name[LOCALE_enUS] ? tn->Name[LOCALE_enUS] : "TaxiNode";
        n.payload_id   = tn->ID;
        AppendNode(std::move(n));
        ++added;
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] LoadFlightMasters: {} nodes", added);
}

void UnifiedTravelGraph::LoadPortalsAndDocks()
{
    // Snap each PortalAnchor (already loaded by PortalIndex) into a
    // graph node. The anchor's source_map → dest_map mapping becomes
    // the cross-map edge built later in BuildPortalEdges. Two anchor
    // kinds: SPELLCASTER GO (Portal) and TRANSPORT/MO_TRANSPORT (Dock).
    auto& portal_idx = Services::Portals();
    uint32 added_portals = 0, added_docks = 0;
    for (auto const& a : portal_idx.Anchors())
    {
        GraphNode n;
        n.kind         = (a.kind == PortalAnchor::Kind::Portal)
                             ? NodeKind::Portal : NodeKind::Dock;
        n.map_id       = a.source_map;
        n.x            = a.x;
        n.y            = a.y;
        n.z            = a.z;
        n.condition_id = a.condition_id;
        // PortalAnchor doesn't carry name; synthesize one for diagnostics.
        n.name = (a.kind == PortalAnchor::Kind::Portal)
                     ? ("Portal to map " + std::to_string(a.dest_map))
                     : ("Dock to map "   + std::to_string(a.dest_map));
        n.payload_id   = a.go_entry;
        AppendNode(std::move(n));
        if (a.kind == PortalAnchor::Kind::Portal) ++added_portals;
        else                                      ++added_docks;
    }
    TC_LOG_INFO("playerbot.v2",
        "[UnifiedTravelGraph] LoadPortalsAndDocks: {} portals, {} docks",
        added_portals, added_docks);
}

void UnifiedTravelGraph::LoadDungeonEntries()
{
    // Walk sMapStore directly (it's a sparse DB2 — iterate via the
    // store's range-for, NOT by contiguous index). MapEntry exposes
    // GetEntrancePos(int32& mapid, float& x, float& y) returning
    // false when no entrance is defined (GM-only maps). We register
    // the entry on the PARENT map so ground travel + flights can
    // route to it; the area-trigger handles the actual teleport.
    uint32 added = 0;
    for (MapEntry const* me : sMapStore)
    {
        if (!me) continue;
        if (!me->IsDungeon() && !me->IsRaid()) continue;
        int32 parent_map = 0;
        float ex = 0.f, ey = 0.f;
        if (!me->GetEntrancePos(parent_map, ex, ey))
            continue;
        if (parent_map < 0) continue;

        GraphNode n;
        n.kind        = NodeKind::DungeonEntry;
        n.map_id      = uint32(parent_map);
        n.x           = ex;
        n.y           = ey;
        n.z           = 0.f;
        char const* map_name = me->MapName[LOCALE_enUS];
        n.name        = std::string("Entrance to ") + (map_name ? map_name : "?");
        n.payload_id  = me->ID;
        AppendNode(std::move(n));
        ++added;
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] LoadDungeonEntries: {} nodes", added);
}

void UnifiedTravelGraph::LoadAreaTriggerTeleports()
{
    // The classic `areatrigger_teleport` table drives the portals a real
    // player crosses by WALKING into the trigger volume — the client sends
    // CMSG_AREATRIGGER and the server runs Player::TeleportTo. A clientless
    // bot never sends that packet, so these teleports were previously
    // invisible to travel: a Night Elf that out-levels Teldrassil could not
    // reach Rut'theran Village (the Darnassus<->Rut'theran link IS such a
    // teleport, both on map 1) and stayed islanded.
    //
    // We model each SAME-MAP teleport as a one-way Teleport edge between a
    // source node (the AreaTrigger.db2 volume centre) and a landing node (the
    // resolved world_safe_locs destination). BuildWalkEdges then stitches the
    // landing node into the local cluster (e.g. the Rut'theran flight master +
    // hub), so A* can compose walk -> teleport -> walk/flight across the gap.
    // The State_Idle leg executor (idle:use_areatrigger_teleport) fires the
    // teleport server-side via near_teleport_to when the bot reaches the source.
    //
    // SCOPE: load BOTH same-map and CROSS-MAP continent teleports. We skip only
    // those whose DESTINATION is an instance (dungeon/raid/BG/arena) —
    // LoadDungeonEntries already anchors instance entrances and we must never
    // route a leveling bot INTO an instance via a stray teleport. Cross-map
    // CONTINENT teleports are exactly what islanded starter zones need (A2:
    // Gilneas 654, Exile's Reach, allied-race / Zandalar starter exits had zero
    // outbound graph edges, so out-levelled bots there could never leave). The
    // leg executor fires same-map via near_teleport_to and cross-map via
    // teleport_to. PlayerCondition gating on the AT is not modelled — for the
    // leveling use case A* only routes through an AT that lies on the cheapest
    // path to a LEVEL-APPROPRIATE target hub, so a bot won't detour through a
    // gated max-level portal; refine with the conditions table if needed.
    uint32 added = 0, skipped_instance = 0, added_xmap = 0;
    QueryResult res = WorldDatabase.Query("SELECT ID, PortLocID FROM areatrigger_teleport");
    if (res)
    {
        do
        {
            Field* f = res->Fetch();
            const uint32 trigger_id = f[0].GetUInt32();
            AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(trigger_id);
            if (!at) continue;
            WorldSafeLocsEntry const* dest = sObjectMgr->GetAreaTrigger(trigger_id);
            if (!dest) continue;     // some rows are quest/rest triggers, no port loc
            const uint32 dest_map_id = dest->Loc.GetMapId();
            if (MapEntry const* dme = sMapStore.LookupEntry(dest_map_id))
                if (dme->Instanceable()) { ++skipped_instance; continue; }
            // Also skip SOURCE-instanceable triggers: most cross-map areatrigger_
            // teleports are dungeon/raid/arena EXIT teleports whose source volume
            // sits INSIDE the instance (e.g. "Maraudon Exit Target", "CoT Dragon
            // Soul Exit"). Their src node lands on an instance map a continent-
            // travelling bot never stands on, so they only bloat the graph — keep
            // only continent-rooted teleports (Darnassus, starter-zone exits, the
            // Dark Portal, city portal rooms).
            if (MapEntry const* sme = sMapStore.LookupEntry(at->ContinentID))
                if (sme->Instanceable()) { ++skipped_instance; continue; }

            GraphNode src;
            src.kind       = NodeKind::AreaTriggerTel;
            src.map_id     = at->ContinentID;
            src.x          = at->Pos.X;
            src.y          = at->Pos.Y;
            src.z          = at->Pos.Z;
            src.name       = "AreaTrigger " + std::to_string(trigger_id) + " (src)";
            src.payload_id = trigger_id;
            const NodeId src_id = AppendNode(std::move(src));

            GraphNode dst;
            dst.kind       = NodeKind::AreaTriggerTel;
            dst.map_id     = dest_map_id;
            dst.x          = dest->Loc.GetPositionX();
            dst.y          = dest->Loc.GetPositionY();
            dst.z          = dest->Loc.GetPositionZ();
            dst.name       = "AreaTrigger " + std::to_string(trigger_id) + " (dst)";
            dst.payload_id = trigger_id;
            const NodeId dst_id = AppendNode(std::move(dst));

            GraphEdge e;
            e.to         = dst_id;
            e.kind       = EdgeKind::Teleport;
            e.cost       = kTeleportCost;
            e.payload_id = trigger_id;     // leg executor looks the AT back up for radius/diag
            AddEdge(src_id, e);
            ++added;
            if (uint32(at->ContinentID) != dest_map_id) ++added_xmap;
        } while (res->NextRow());
    }
    TC_LOG_INFO("playerbot.v2",
        "[UnifiedTravelGraph] LoadAreaTriggerTeleports: {} teleport edges ({} cross-map; {} instance-dest skipped)",
        added, added_xmap, skipped_instance);
}

void UnifiedTravelGraph::LoadCapitals()
{
    uint32 added = 0;
    for (auto const& cap : V2::World::AllCapitals())
    {
        GraphNode n;
        n.kind       = NodeKind::Capital;
        n.map_id     = cap.map_id;
        n.x          = cap.x;
        n.y          = cap.y;
        n.z          = cap.z;
        n.name       = cap.name ? cap.name : "?";
        n.payload_id = added;     // index into kCapitals
        AppendNode(std::move(n));
        ++added;
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] LoadCapitals: {} nodes", added);
}

void UnifiedTravelGraph::LoadQuestHubs()
{
    // QuestHubs give us "I want to quest somewhere in my level range" as
    // a routable destination. Each hub becomes a node so A* can compose
    // (current pos → flight master → quest hub center). Walk-edges built
    // later by BuildWalkEdges() connect each hub to its same-map
    // flight masters / portals / docks within kMaxWalkEdgeYds, so the
    // router can pick (taxi to nearest FM → walk hub) without baking
    // hub-specific code.
    //
    // Pre-filter:
    //   * Skip hubs with no quest IDs (validation drop).
    //   * Hubs at Z=0 or with empty mapId are DBSCAN noise — exclude.
    //
    // Faction handling: hubs carry a factionMask but graph nodes don't
    // (yet) — A* picks the cheapest path regardless of faction. The
    // route consumer (idle:travel_plan) re-validates faction on use.
    uint32 added = 0;
    Services::Hubs().ForEach([&](QuestHub const& h)
    {
        // Map 0 is EASTERN KINGDOMS — a perfectly valid hub map. The old
        // `mapId == 0` "DBSCAN noise" check silently dropped EVERY quest hub
        // on the EK continent from the graph, so no route could ever attach
        // near an EK goal (verified live: [bridge_probe] to_attach=0 for
        // Deathknell/Tirisfal goals while from_attach>0 — bots wedged in the
        // UC interior could never bridge out, and every EK hub relocation
        // was unroutable). Real DBSCAN noise is a hub with no quests or a
        // null-island position — test THOSE, never mapId==0.
        if (h.questIds.empty()) return;
        if (h.location.GetPositionX() == 0.f && h.location.GetPositionY() == 0.f) return;

        GraphNode n;
        n.kind   = NodeKind::QuestHub;
        n.map_id = h.mapId;
        n.x      = h.location.GetPositionX();
        n.y      = h.location.GetPositionY();
        n.z      = h.location.GetPositionZ();
        // CRITICAL: must use AppendNode (not inline push_back) so the
        // _adj vector grows in lock-step with _nodes. The old inline
        // path made QuestHub nodes structurally dead — BuildWalkEdges,
        // ConnectIslands, and the A* expansion all gate on
        // `from >= _adj.size()` and silently drop hubs entirely.
        // idle:travel_plan therefore never routed TO any QuestHub
        // (audit-finding 2026-05-21).
        AppendNode(std::move(n));
        ++added;
    });
    TC_LOG_INFO("playerbot.v2",
        "[UnifiedTravelGraph] LoadQuestHubs: {} nodes", added);
}

void UnifiedTravelGraph::LoadElevators()
{
    // One ElevatorStop node per ElevatorIndex stop. The stop's spawn_id
    // is preserved in payload_id so BuildElevatorEdges can group stops
    // belonging to the same physical platform into a clique. Multi-stop
    // platforms (TB spoke lifts pass three floors) get one node per
    // floor, fully connected with Elevator edges.
    //
    // Why this is the right shape: the router naturally composes
    // (walk_to_local_stop) → (elevator_to_other_floor) →
    // (walk_from_destination_stop). BuildWalkEdges then connects stops
    // to nearby same-map FM / portal / hub nodes within the standard
    // kMaxWalkEdgeYds budget — no special-cased plumbing required.
    uint32 added = 0;
    for (auto const& s : ElevatorIndex::Instance().Stops())
    {
        GraphNode n;
        n.kind       = NodeKind::ElevatorStop;
        n.map_id     = s.map_id;
        n.x          = s.x;
        n.y          = s.y;
        n.z          = s.z;
        n.name       = "elevator_stop";
        // payload_id = low 32 bits of spawn_id. spawn_id (gameobject.guid)
        // is a 64-bit value but in practice fits in 32 bits for vanilla
        // through retail; we use this for cheap O(1) grouping in
        // BuildElevatorEdges. If a server ever exceeds 2^32 spawn rows
        // this will collide — unlikely, and BuildElevatorEdges has a
        // distance guard to reject false matches.
        n.payload_id = uint32(s.spawn_id & 0xFFFFFFFFu);
        AppendNode(std::move(n));
        ++added;
    }
    TC_LOG_INFO("playerbot.v2",
        "[UnifiedTravelGraph] LoadElevators: {} nodes", added);
    // One-shot diag (UC pocket-escape verification): list the elevator
    // stops registered inside the Undercity bounding box so we can see
    // whether the Undervators contributed BOTTOM stops (z<0) or only
    // tops — a missing bottom stop leaves the interior with no level
    // attach and the router detours to absurd surface nodes.
    for (auto const& s2 : ElevatorIndex::Instance().Stops())
        if (s2.map_id == 0 &&
            s2.x > 1400.f && s2.x < 1700.f && s2.y > 50.f && s2.y < 350.f)
            TC_LOG_INFO("playerbot.v2",
                "[UnifiedTravelGraph] UC elevator stop: spawn={} ({:.1f},{:.1f},{:.1f})",
                s2.spawn_id, s2.x, s2.y, s2.z);
}

// ---------------------------------------------------------------- Edge builders

void UnifiedTravelGraph::BuildTaxiEdges()
{
    // Index flight-master nodes by their TaxiNode ID for O(1) lookup
    // from the TaxiPath endpoints.
    std::unordered_map<uint32, NodeId> by_taxi_id;
    by_taxi_id.reserve(_nodes.size());
    for (NodeId i = 0; i < _nodes.size(); ++i)
    {
        if (_nodes[i].kind == NodeKind::FlightMaster)
            by_taxi_id[_nodes[i].payload_id] = i;
    }

    uint32 added = 0;
    for (TaxiPathEntry const* tp : sTaxiPathStore)
    {
        if (!tp) continue;
        auto it_from = by_taxi_id.find(tp->FromTaxiNode);
        auto it_to   = by_taxi_id.find(tp->ToTaxiNode);
        if (it_from == by_taxi_id.end() || it_to == by_taxi_id.end())
            continue;
        // Path length: walk the precomputed path nodes (already loaded
        // into sTaxiPathNodesByPath at boot). Sum euclidean segment
        // distances; tile-crossing path-node arrays start at 1 entry
        // per node so a 2-node path has length ~node distance.
        float path_yd = 0.f;
        if (tp->ID < sTaxiPathNodesByPath.size())
        {
            auto const& nodes = sTaxiPathNodesByPath[tp->ID];
            for (size_t i = 1; i < nodes.size(); ++i)
            {
                if (!nodes[i] || !nodes[i-1]) continue;
                if (nodes[i]->ContinentID != nodes[i-1]->ContinentID)
                    continue;     // skip continent-bridge artifacts in flight path
                path_yd += Dist2D(nodes[i]->Loc.X, nodes[i]->Loc.Y,
                                  nodes[i-1]->Loc.X, nodes[i-1]->Loc.Y);
            }
        }
        if (path_yd <= 0.f)
        {
            // Fallback: straight-line distance between endpoints.
            GraphNode const& a = _nodes[it_from->second];
            GraphNode const& b = _nodes[it_to->second];
            path_yd = (a.map_id == b.map_id)
                          ? Dist2D(a.x, a.y, b.x, b.y)
                          : 5000.f;   // unknown intercontinental — high but not infinite
        }
        GraphEdge e;
        e.to         = it_to->second;
        e.kind       = EdgeKind::Taxi;
        e.cost       = path_yd / kFlightSpeedYds;
        // Per-bot gating: the From node's faction restriction effectively
        // gates whether this edge is usable. We don't repeat the
        // condition here; FilterEdgeUsable() consults the source node.
        e.condition_id = 0;
        e.payload_id   = tp->ID;
        AddEdge(it_from->second, e);
        ++added;
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] BuildTaxiEdges: {} edges", added);
}

void UnifiedTravelGraph::BuildPortalEdges()
{
    // PortalIndex anchors are 1:1 with Portal/Dock graph nodes — we
    // appended them in the same order. For each anchor we know the
    // destination map; we need to find a *target* node on that map to
    // attach the cross-map edge to. Heuristic: target the closest
    // Capital, Portal, or FlightMaster on the destination map. The
    // walk-edge build pass will then knit destinations to local POIs.
    auto& portal_idx = Services::Portals();
    auto const& anchors = portal_idx.Anchors();
    uint32 added = 0;
    // The graph nodes for portals/docks start at the offset where we
    // appended them (right after the flight masters). Walk the same
    // anchor list with parallel iteration.
    NodeId anchor_node_base = INVALID_NODE_ID;
    for (NodeId i = 0; i < _nodes.size(); ++i)
    {
        if (_nodes[i].kind == NodeKind::Portal || _nodes[i].kind == NodeKind::Dock)
        {
            anchor_node_base = i;
            break;
        }
    }
    if (anchor_node_base == INVALID_NODE_ID) return;

    for (size_t i = 0; i < anchors.size(); ++i)
    {
        const NodeId src = NodeId(anchor_node_base + i);
        if (src >= _nodes.size()) break;
        PortalAnchor const& a = anchors[i];

        // Find a landing node on the destination map. Prefer Capital
        // (well-known coords near banker/inn cluster), then
        // FlightMaster (so onward travel composes), then any node.
        //
        // T-P2b: among nodes of the SAME (best) rank, pick the one
        // physically nearest to the anchor's arrival position
        // (dest_x/dest_y). Previously the first best-rank node in
        // iteration order won, which could attach a Northrend portal to a
        // capital on the far side of the continent — inflating the walk
        // leg the bot has to do after the loading screen. When arrival
        // coords are unknown (0,0) the distance term is 0 for every
        // candidate, so behaviour degrades gracefully to first-best-rank.
        const bool have_dest_pos = (a.dest_x != 0.f || a.dest_y != 0.f);
        NodeId best = INVALID_NODE_ID;
        int    best_rank = 99;
        float  best_dsq  = std::numeric_limits<float>::max();
        auto it = _nodes_by_map.find(a.dest_map);
        if (it == _nodes_by_map.end()) continue;
        for (NodeId cand : it->second)
        {
            int rank = 99;
            switch (_nodes[cand].kind)
            {
                case NodeKind::Capital:      rank = 0; break;
                case NodeKind::FlightMaster: rank = 1; break;
                case NodeKind::Portal:       rank = 2; break;
                case NodeKind::Dock:         rank = 2; break;
                default:                     rank = 3; break;
            }
            float dsq = 0.f;
            if (have_dest_pos)
            {
                const float dx = _nodes[cand].x - a.dest_x;
                const float dy = _nodes[cand].y - a.dest_y;
                dsq = dx * dx + dy * dy;
            }
            if (rank < best_rank || (rank == best_rank && dsq < best_dsq))
            {
                best_rank = rank;
                best_dsq  = dsq;
                best      = cand;
            }
        }
        if (best == INVALID_NODE_ID) continue;

        GraphEdge e;
        e.to           = best;
        e.kind         = (_nodes[src].kind == NodeKind::Portal)
                             ? EdgeKind::Portal : EdgeKind::Ship;
        e.cost         = (e.kind == EdgeKind::Portal) ? kPortalCost : kShipBoardCost;
        e.condition_id = a.condition_id;
        e.payload_id   = a.go_entry;
        AddEdge(src, e);
        ++added;
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] BuildPortalEdges: {} cross-map edges", added);
}

void UnifiedTravelGraph::BuildElevatorEdges()
{
    // Group ElevatorStop nodes by payload_id (spawn_id-low32). Stops on
    // the same physical platform form a clique connected by Elevator
    // edges in both directions. A platform with N stops produces N×(N-1)
    // directed edges — typically N=2 (start + one floor), so 2 edges
    // each.
    //
    // Safety: BuildPortalIndex emits stop 0 = spawn position even for
    // single-floor platforms (no Timeto*floor > 0), so those nodes
    // exist but have NO siblings → no Elevator edges added. They still
    // get walk-edges through BuildWalkEdges and act as travel anchors.
    std::unordered_map<uint32, std::vector<NodeId>> by_spawn;
    by_spawn.reserve(_nodes.size() / 4);
    for (NodeId i = 0; i < _nodes.size(); ++i)
    {
        if (_nodes[i].kind != NodeKind::ElevatorStop) continue;
        by_spawn[_nodes[i].payload_id].push_back(i);
    }

    uint32 added = 0;
    for (auto const& [spawn_id, ids] : by_spawn)
    {
        if (ids.size() < 2) continue;
        // Distance guard: stops on the same spawn should never be more
        // than ~120y apart vertically and ~60y horizontally (the tallest
        // TB lift is ~95y rise). If two nodes share a payload_id but are
        // farther than 200y total Manhattan they're a uint32 collision
        // (extremely rare 64→32 truncation) — skip the pair.
        for (size_t i = 0; i < ids.size(); ++i)
        {
            for (size_t j = i + 1; j < ids.size(); ++j)
            {
                GraphNode const& a = _nodes[ids[i]];
                GraphNode const& b = _nodes[ids[j]];
                const float dxy = Dist2D(a.x, a.y, b.x, b.y);
                const float dz  = std::fabs(a.z - b.z);
                if (dxy > 60.f || dz > 120.f) continue;
                GraphEdge eab{};
                eab.to = ids[j]; eab.kind = EdgeKind::Elevator;
                eab.cost = kElevatorCost; eab.payload_id = spawn_id;
                AddEdge(ids[i], eab);
                GraphEdge eba{};
                eba.to = ids[i]; eba.kind = EdgeKind::Elevator;
                eba.cost = kElevatorCost; eba.payload_id = spawn_id;
                AddEdge(ids[j], eba);
                added += 2;
            }
        }
    }
    TC_LOG_INFO("playerbot.v2",
        "[UnifiedTravelGraph] BuildElevatorEdges: {} edges across {} platforms",
        added, uint32(by_spawn.size()));
}

void UnifiedTravelGraph::BuildWalkEdges()
{
    // Same-map walk edges between nearby nodes. Bounded by kMaxWalkEdgeYds
    // so we don't connect a node in Stranglethorn to one in Westfall —
    // the navmesh would reject that path. Walk edges are bidirectional
    // (added once per direction).
    //
    // Algorithm: for each map, walk every pair of nodes. The candidate
    // count per map is bounded (typically 20-150 nodes per continent),
    // so O(N^2) per map runs in milliseconds at boot.
    uint32 added = 0;
    for (auto const& [map_id, ids] : _nodes_by_map)
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            for (size_t j = i + 1; j < ids.size(); ++j)
            {
                GraphNode const& a = _nodes[ids[i]];
                GraphNode const& b = _nodes[ids[j]];
                const float d = Dist2D(a.x, a.y, b.x, b.y);
                if (d > kMaxWalkEdgeYds) continue;
                // Walk edges are 2D only — but a large VERTICAL gap is not
                // walkable. Without this guard the planner fabricated a "walk
                // straight up" edge from a ground node to an elevated flight
                // master / platform vendor (same X/Y, +90y Z), so A* skipped the
                // elevator entirely and the executor NoPathed on the doomed climb.
                // Capping the Z delta forces vertical traversal through the
                // EdgeKind::Elevator edges (an FM at z103 still connects to the
                // lift's TOP stop at z~90 — dz 13 — but never to the ground/bottom
                // stop). kMaxWalkEdgeDz > the tallest real ramp's rise-per-600y,
                // < the shortest city lift's travel; the executor's walkTo still
                // navmesh-pathfinds the actual (sloped) route within an edge.
                if (std::fabs(a.z - b.z) > kMaxWalkEdgeDz) continue;
                const float cost = d / kWalkSpeedYds;
                GraphEdge eab{};
                eab.to = ids[j]; eab.kind = EdgeKind::Walk; eab.cost = cost;
                AddEdge(ids[i], eab);
                GraphEdge eba{};
                eba.to = ids[i]; eba.kind = EdgeKind::Walk; eba.cost = cost;
                AddEdge(ids[j], eba);
                added += 2;
            }
        }
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] BuildWalkEdges: {} edges (cap={}y)",
                added, uint32(kMaxWalkEdgeYds));
}

void UnifiedTravelGraph::ConnectIslands()
{
    // Identify nodes that have NO walk-edge neighbors (in-island
    // singletons) and connect them to the closest reachable node on
    // their map via a Bridge node placed at the midpoint. This is a
    // fallback for sparsely-populated maps (BG sub-maps, scenario
    // staging zones) where the kMaxWalkEdgeYds threshold leaves real
    // nodes unreachable.
    //
    // Snapshot the node count BEFORE we start appending Bridge nodes.
    // AppendNode() below grows _nodes (and _adj), so iterating against the
    // live _nodes.size() would walk i into the freshly-added Bridge indices,
    // and has_walk[i] — sized to the ORIGINAL count — would read out of bounds
    // (a vector<bool> proxy deref past the allocation = ACCESS_VIOLATION, seen
    // only when ≥1 bridge was added and the OOB landed on unmapped memory, so
    // it crashed startup non-deterministically). Bridge nodes are created WITH
    // walk edges, so they never need processing here regardless.
    const size_t original_n = _nodes.size();
    // Build a quick "has any walk edge" set first.
    std::vector<bool> has_walk(original_n, false);
    for (NodeId i = 0; i < original_n && i < _adj.size(); ++i)
        for (auto const& e : _adj[i])
            if (e.kind == EdgeKind::Walk) { has_walk[i] = true; break; }

    uint32 bridges_added = 0;
    for (NodeId i = 0; i < original_n; ++i)
    {
        if (has_walk[i]) continue;
        // Find the closest other node on the same map.
        GraphNode const& src = _nodes[i];
        auto it = _nodes_by_map.find(src.map_id);
        if (it == _nodes_by_map.end()) continue;
        NodeId best = INVALID_NODE_ID;
        float  best_d = std::numeric_limits<float>::max();
        for (NodeId cand : it->second)
        {
            if (cand == i) continue;
            const float d = Dist2D(src.x, src.y, _nodes[cand].x, _nodes[cand].y);
            if (d < best_d) { best_d = d; best = cand; }
        }
        if (best == INVALID_NODE_ID) continue;
        // Insert a Bridge node at the midpoint with walk edges to both.
        GraphNode br;
        br.kind   = NodeKind::Bridge;
        br.map_id = src.map_id;
        br.x      = (src.x + _nodes[best].x) * 0.5f;
        br.y      = (src.y + _nodes[best].y) * 0.5f;
        br.z      = (src.z + _nodes[best].z) * 0.5f;
        br.name   = "Bridge";
        const NodeId bid = AppendNode(std::move(br));
        const float half = best_d * 0.5f / kWalkSpeedYds;
        AddEdge(i,    GraphEdge{bid,  EdgeKind::Walk, half, 0, 0});
        AddEdge(bid,  GraphEdge{i,    EdgeKind::Walk, half, 0, 0});
        AddEdge(bid,  GraphEdge{best, EdgeKind::Walk, half, 0, 0});
        AddEdge(best, GraphEdge{bid,  EdgeKind::Walk, half, 0, 0});
        ++bridges_added;
    }
    TC_LOG_INFO("playerbot.v2", "[UnifiedTravelGraph] ConnectIslands: {} bridge nodes added",
                bridges_added);
}

// ---------------------------------------------------------------- Read API

size_t UnifiedTravelGraph::GetNodeCount() const
{
    std::shared_lock lk(_mtx);
    return _nodes.size();
}

size_t UnifiedTravelGraph::GetEdgeCount() const
{
    std::shared_lock lk(_mtx);
    size_t total = 0;
    for (auto const& a : _adj) total += a.size();
    return total;
}

UnifiedTravelGraph::KindCounts UnifiedTravelGraph::GetKindCounts() const
{
    std::shared_lock lk(_mtx);
    KindCounts c{};
    for (auto const& n : _nodes)
    {
        switch (n.kind)
        {
            case NodeKind::FlightMaster: ++c.flight_masters; break;
            case NodeKind::Portal:       ++c.portals;        break;
            case NodeKind::Dock:         ++c.docks;          break;
            case NodeKind::DungeonEntry: ++c.dungeon_entries; break;
            case NodeKind::Capital:      ++c.capitals;       break;
            case NodeKind::QuestHub:     ++c.quest_hubs;     break;
            case NodeKind::Bridge:       ++c.bridges;        break;
            default: break;
        }
    }
    return c;
}

GraphNode const* UnifiedTravelGraph::GetNode(NodeId id) const
{
    std::shared_lock lk(_mtx);
    if (id >= _nodes.size()) return nullptr;
    return &_nodes[id];
}

GraphNode const* UnifiedTravelGraph::FindNearestNodeOnMap(
    uint32 map_id, float x, float y, float max_range) const
{
    std::shared_lock lk(_mtx);
    auto it = _nodes_by_map.find(map_id);
    if (it == _nodes_by_map.end()) return nullptr;
    float best_dsq = max_range * max_range;
    GraphNode const* best = nullptr;
    for (NodeId nid : it->second)
    {
        if (nid >= _nodes.size()) continue;
        auto const& n = _nodes[nid];
        const float dx = n.x - x, dy = n.y - y;
        const float dsq = dx * dx + dy * dy;
        if (dsq < best_dsq)
        {
            best_dsq = dsq;
            best = &n;
        }
    }
    return best;
}

// ---------------------------------------------------------------- A* router

namespace {

// Per-edge gating per requesting bot. Centralized so loaders don't
// need to think about it. Returns false → edge skipped during search.
bool EdgeUsable(Player const* bot,
                GraphNode const& src_node,
                GraphNode const& dst_node,
                GraphEdge const& edge,
                bool allow_hearth)
{
    if (!allow_hearth && edge.kind == EdgeKind::Hearth) return false;
    // Source-node faction gate: flight masters / portals tagged Alliance
    // or Horde aren't usable by the opposing team. Neutral (faction=0)
    // is fine for anyone.
    if (bot && src_node.faction != 0 && src_node.faction != bot->GetTeam())
        return false;
    // T-P1b: Taxi edges may only be taken to a flight master the bot has
    // already DISCOVERED. The server refuses CMSG_ACTIVATE_TAXI for an
    // unknown destination node (TaxiHandler.cpp checks
    // IsTaximaskNodeKnown for both endpoints), so routing through an
    // undiscovered node strands the bot. The destination FlightMaster
    // node's payload_id is its sTaxiNodesStore id (set in
    // LoadFlightMasters); that is exactly the index
    // PlayerTaxi::IsTaximaskNodeKnown expects. We gate only the arrival
    // node here — the source node is where the bot physically stands to
    // board, and is handled by walk-attach. We deliberately do NOT
    // auto-learn paths; an unknown destination simply prunes the edge so
    // A* finds a route the bot can actually execute.
    if (bot && edge.kind == EdgeKind::Taxi &&
        dst_node.kind == NodeKind::FlightMaster && dst_node.payload_id != 0)
    {
        if (!bot->m_taxi.IsTaximaskNodeKnown(dst_node.payload_id))
            return false;
    }
    // Source-node ConditionMgr gate (rep, level, quest unlock).
    if (bot && src_node.condition_id != 0)
    {
        auto const* cond_mgr = sConditionMgr;
        if (cond_mgr && !cond_mgr->IsPlayerMeetingCondition(
                const_cast<Player*>(bot), src_node.condition_id))
            return false;
    }
    if (bot && edge.condition_id != 0)
    {
        auto const* cond_mgr = sConditionMgr;
        if (cond_mgr && !cond_mgr->IsPlayerMeetingCondition(
                const_cast<Player*>(bot), edge.condition_id))
            return false;
    }
    return true;
}

} // anonymous

Route UnifiedTravelGraph::FindRoute(RouteRequest const& req) const
{
    Route out;
    if (!_initialized) return out;

    // Bad-goal guard: a null-island destination (0,0,0) is a dead goal — an
    // unpopulated POI / a hub with no resolved location. Reject as unroutable so
    // we never emit a trivial "walk to world origin" leg. Do NOT key this on
    // map==0: map 0 (Eastern Kingdoms) is a real, heavily-travelled continent.
    if (req.to_x == 0.0f && req.to_y == 0.0f && req.to_z == 0.0f)
        return out;   // out.ok stays false

    // Live-navmesh walkability check for source attaches (see RouteRequest::
    // validate_source_walk). Since #5 Phase 4 this runs on the snapshot-build
    // WORKER threads (GraphHasBridgeRoute ← BotSnapshotBuilder::Build). Safe
    // there: each call uses its own stack-local PathGenerator/dtNavMeshQuery
    // (no shared query state), and a worker builds one Map* partition's bots
    // SEQUENTIALLY, so a given navmesh is read one-bot-at-a-time per worker;
    // queries on a shared dtNavMesh (instances of one mapId) are concurrent
    // READS, which Detour permits. The only hazard is a tile load/unload
    // concurrent with a query — tile loads finish in sMapMgr->Update before the
    // build phase, and the per-mapId MMap load-lock + SehSafeCalculatePath below
    // backstop any residual. Accept only a complete path (NORMAL); INCOMPLETE
    // means the path dead-ends short of the target (a disconnected pocket
    // wall) or exceeded the budget — for a probe rescuing a WEDGED bot,
    // treating ambiguous as unreachable is correct: the surviving attaches
    // (typically nearby, same-pocket nodes like an interior elevator stop or
    // flight master) are the ones the bot can actually act on.
    auto const sourceWalkable = [&](float tx, float ty, float tz) -> bool
    {
        if (!req.validate_source_walk) return true;
        if (!req.bot) return true;
        PathGenerator path(req.bot);
        // SehSafeCalculatePath, NOT raw CalculatePath: this probe runs from
        // the snapshot builder (GraphHasBridgeRoute) on the build worker threads
        // and crashed inside Detour reading a tile mid-load (2026-06-12 11:59
        // dump: dtVlerp ← findNearestPoly ← GetPolyByLocation ← BuildPolyPath
        // ← THIS lambda). Same tile-race hazard as every other raw
        // dtNavMeshQuery/CalculatePath call from playerbot code.
        if (!BotMovement::SehSafeCalculatePath(path, tx, ty, tz)) return false;
        // PathType is a bitmask.
        const uint32 t = uint32(path.GetPathType());
        // (1) COMPLETE ground path → definitively walkable. NORMAL set, no
        // failure/partial bits. FARFROMPOLY_START tolerated (the bot may stand
        // slightly off-poly); FARFROMPOLY_END is not (node off reachable mesh).
        if ((t & PATHFIND_NORMAL) &&
            !(t & (PATHFIND_INCOMPLETE | PATHFIND_NOPATH | PATHFIND_SHORT |
                   PATHFIND_SHORTCUT | PATHFIND_FARFROMPOLY_END)))
            return true;
        // (2) PROGRESSING-TRUNCATED path → accept. A flight master at 300-600y
        // exceeds the 74-poly/point smooth cap (296y) and returns INCOMPLETE even
        // though it is genuinely reachable — this was the from_attach=0 cause that
        // blocked cross-map travel. Raising the global cap to fix it FROZE the
        // build (256-poly A* per probe; 2026-06-16). Instead accept the INCOMPLETE
        // path WHEN it is a VALID navmesh path heading toward the node (actual
        // endpoint meaningfully closer than the start): the bot attaches and walks
        // the validated 296y prefix, then re-probes closer and converges in 1-2
        // hops. SAFE: the executed prefix is always real navmesh (no through-wall);
        // a path that dead-ends near the bot (barely progressed) or is genuinely
        // blocked (NOPATH) or off-mesh (FARFROMPOLY_END) is still rejected. CHEAP:
        // same cap-74 CalculatePath cost as before plus a distance compare — no
        // extra navmesh work, so no build-freeze risk.
        if ((t & PATHFIND_INCOMPLETE) &&
            !(t & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY_END)))
        {
            G3D::Vector3 const& s = path.GetStartPosition();
            G3D::Vector3 const& e = path.GetActualEndPosition();
            const float d_start = (tx - s.x) * (tx - s.x) + (ty - s.y) * (ty - s.y);
            const float d_end   = (tx - e.x) * (tx - e.x) + (ty - e.y) * (ty - e.y);
            // Progressed to within HALF the start-distance (≈≥29% of the way) AND
            // at least ~100y closer in absolute terms — real travel toward the node,
            // not a stall at a wall by the bot.
            if (d_end + 10000.0f < d_start && d_end < d_start * 0.5f)
                return true;
        }
        return false;
    };

    std::shared_lock lk(_mtx);

    // Trivial: same map and within walking distance — no routing needed.
    // Route shape contract:
    //   - Multi-hop: at least 2 legs. First leg's from_node = INVALID
    //     (off-graph source attach), middle legs have both endpoints in
    //     the graph, final leg's to_node = INVALID (off-graph sink).
    //   - Direct walk (this branch): 1 leg with from_node = to_node =
    //     INVALID and kind = Walk. Consumers iterating legs should
    //     NEVER blindly index _nodes[leg.from/to_node] — always check
    //     `!= INVALID_NODE_ID` first. Both shapes share this rule, so
    //     single-leg consumers don't need special-case handling.
    if (req.from_map == req.to_map && !req.skip_trivial)
    {
        const float d = Dist2D(req.from_x, req.from_y, req.to_x, req.to_y);
        if (d <= kMaxWalkEdgeYds && sourceWalkable(req.to_x, req.to_y, req.to_z))
        {
            RouteLeg leg{};
            leg.from_node = INVALID_NODE_ID;
            leg.to_node   = INVALID_NODE_ID;
            leg.kind      = EdgeKind::Walk;
            leg.to_map    = req.to_map;
            leg.to_x      = req.to_x;
            leg.to_y      = req.to_y;
            leg.to_z      = req.to_z;
            out.legs.push_back(leg);
            out.total_cost = d / kWalkSpeedYds;
            out.ok = true;
            return out;
        }
    }

    // Build source-attach edges: walk to every node on from_map within
    // kMaxWalkEdgeYds. Same for destination-attach (incoming walk to
    // dest from same-map nodes).
    struct Attach { NodeId node; float cost; };
    std::vector<Attach> from_attach;
    std::vector<Attach> to_attach;
    // Attach cost = 2D distance + 4x the VERTICAL separation. Raw xy
    // distance made a surface node 125y directly ABOVE an interior bot
    // the cheapest attach (Somi in the UC inner ring attached to a
    // surface node at z+125 44y away instead of the Undervator bottom
    // stop 200y away on HER level) — the resulting walk leg NoPathed
    // forever. Vertical separation is almost never bridged by a plain
    // walk, so weight it heavily; the elevator-stop nodes on the bot's
    // own level then win the attach and the lift edge enters the route.
    // (validate_source_walk does this properly with live navmesh checks
    // but is world-thread-only; this heuristic covers AI-thread callers.)
    auto attach_cost = [](float d2, float dz) -> float
    { return (d2 + 4.0f * std::fabs(dz)) / kWalkSpeedYds; };
    if (auto it = _nodes_by_map.find(req.from_map); it != _nodes_by_map.end())
    {
        // Collect in-range, slope-OK candidates and probe CHEAPEST-FIRST, capping
        // the number of expensive navmesh probes. validate_source_walk does a full
        // Detour path build per node; at the raised 256-point path cap, probing
        // EVERY in-range node spiked the world tick (135ms @ 560 bots). A* only
        // needs a couple of source seeds, and the cheapest (nearest, flattest)
        // attaches are both the most likely walkable AND the best seeds — so sort
        // by attach_cost and stop after a few hits / a hard probe budget.
        // Slope-gated vertical limit (an attach IS a walk; ramps climb ~1:4):
        // allow dz up to max(18y, d/4). Without it a far floor directly above
        // (Somi → Undervator top stop, 117y up / 229y away, a 1:2 "slope") won
        // the attach and the walk leg NoPathed forever instead of riding the lift.
        std::vector<std::pair<float, NodeId>> cand;   // (attach_cost, node)
        for (NodeId nid : it->second)
        {
            const float d = Dist2D(req.from_x, req.from_y, _nodes[nid].x, _nodes[nid].y);
            if (d > kMaxWalkEdgeYds) continue;
            if (std::fabs(_nodes[nid].z - req.from_z) >
                std::max(kMaxWalkEdgeDz, d * 0.25f)) continue;
            cand.emplace_back(attach_cost(d, _nodes[nid].z - req.from_z), nid);
        }
        std::sort(cand.begin(), cand.end(),
                  [](auto const& a, auto const& b) { return a.first < b.first; });
        constexpr size_t kMaxAttaches     = 3;   // enough A* source seeds
        constexpr size_t kMaxAttachProbes = 8;   // hard cap on expensive navmesh probes
        size_t probed = 0;
        for (auto const& c : cand)
        {
            if (from_attach.size() >= kMaxAttaches) break;
            if (req.validate_source_walk && ++probed > kMaxAttachProbes) break;
            if (sourceWalkable(_nodes[c.second].x, _nodes[c.second].y, _nodes[c.second].z))
                from_attach.push_back({c.second, c.first});
        }
    }
    if (auto it = _nodes_by_map.find(req.to_map); it != _nodes_by_map.end())
    {
        for (NodeId nid : it->second)
        {
            const float d = Dist2D(req.to_x, req.to_y, _nodes[nid].x, _nodes[nid].y);
            if (std::fabs(_nodes[nid].z - req.to_z) >
                std::max(kMaxWalkEdgeDz, d * 0.25f)) continue;
            if (d <= kMaxWalkEdgeYds)
                to_attach.push_back({nid,
                    attach_cost(d, _nodes[nid].z - req.to_z)});
        }
    }
    out.from_attach_count = uint32(from_attach.size());
    out.to_attach_count   = uint32(to_attach.size());
    if (from_attach.empty() || to_attach.empty())
    {
        // No graph-reachable seed on at least one side. Caller falls
        // back to existing greedy walk rules.
        return out;
    }
    // Reverse-lookup the cheapest exit edge for each "to" node.
    std::unordered_map<NodeId, float> exit_cost;
    exit_cost.reserve(to_attach.size());
    for (auto const& a : to_attach) exit_cost[a.node] = a.cost;

    // A* over real graph nodes. The "from" virtual is collapsed into
    // initial open-set entries pre-seeded with each from_attach's cost.
    struct Open {
        float  f;
        float  g;
        NodeId node;
        NodeId parent;
        EdgeKind edge_in;
        uint32 edge_payload;
        bool operator>(Open const& o) const { return f > o.f; }
    };
    std::priority_queue<Open, std::vector<Open>, std::greater<Open>> pq;
    // Heuristic: same-map = euclidean distance / walk speed. Cross-map
    // = 0 (admissible because crossing maps costs portals/taxis we
    // can't predict; relying on Dijkstra-like search for cross-map).
    auto Heuristic = [&](NodeId n)->float {
        GraphNode const& nd = _nodes[n];
        if (nd.map_id != req.to_map) return 0.f;
        const float d = Dist2D(nd.x, nd.y, req.to_x, req.to_y);
        return d / kWalkSpeedYds;
    };
    std::unordered_map<NodeId, float> best_g;
    struct Parent { NodeId node; EdgeKind kind; uint32 payload; };
    std::unordered_map<NodeId, Parent> parents;
    for (auto const& a : from_attach)
    {
        const float g = a.cost;
        const float h = Heuristic(a.node);
        pq.push(Open{g + h, g, a.node, INVALID_NODE_ID, EdgeKind::Walk, 0});
        best_g[a.node] = g;
        parents[a.node] = Parent{INVALID_NODE_ID, EdgeKind::Walk, 0};
    }

    NodeId goal_node = INVALID_NODE_ID;
    float  goal_cost = std::numeric_limits<float>::max();
    uint32 visited = 0;

    while (!pq.empty())
    {
        Open cur = pq.top(); pq.pop();
        ++visited;
        if (visited > req.max_visited) break;
        // Stale entry?
        auto bg = best_g.find(cur.node);
        if (bg == best_g.end() || cur.g > bg->second) continue;
        // Goal test: reaching any node that has an "exit cost" to dest
        // finishes the search; the leg from cur.node → dest is a
        // walk equal to exit_cost.
        if (auto ec = exit_cost.find(cur.node); ec != exit_cost.end())
        {
            const float total = cur.g + ec->second;
            if (total < goal_cost)
            {
                goal_cost = total;
                goal_node = cur.node;
                // Optimal-first heap: first pop that meets goal is
                // optimal under admissible heuristic. Break early.
                break;
            }
        }
        // Expand.
        if (cur.node >= _adj.size()) continue;
        for (auto const& e : _adj[cur.node])
        {
            if (e.to >= _nodes.size())
                continue;
            if (!EdgeUsable(req.bot, _nodes[cur.node], _nodes[e.to], e, req.allow_hearth))
                continue;
            float nxt_g = cur.g + e.cost;
            // Third-party transit penalty (cross-zone routing fix): toll each
            // ENTRY into a map that is neither the start nor the goal map so the
            // flat-cost portal network can't be abused as a free cross-continent
            // teleport mesh (see kTransitMapPenalty). One-time per map-entry, not
            // per intra-map edge, so the toll reflects the detour, not its length.
            {
                const uint32 cur_map = _nodes[cur.node].map_id;
                const uint32 dst_map = _nodes[e.to].map_id;
                if (dst_map != cur_map && dst_map != req.from_map && dst_map != req.to_map)
                    nxt_g += kTransitMapPenalty;
            }
            auto exist = best_g.find(e.to);
            if (exist != best_g.end() && exist->second <= nxt_g) continue;
            best_g[e.to] = nxt_g;
            parents[e.to] = Parent{cur.node, e.kind, e.payload_id};
            const float nxt_f = nxt_g + Heuristic(e.to);
            pq.push(Open{nxt_f, nxt_g, e.to, cur.node, e.kind, e.payload_id});
        }
    }

    out.visited_nodes = visited;
    if (goal_node == INVALID_NODE_ID) return out;

    // Walk parents back to source to materialize the leg list.
    std::vector<NodeId> chain;
    for (NodeId cur = goal_node;
         cur != INVALID_NODE_ID;
         cur = parents.count(cur) ? parents[cur].node : INVALID_NODE_ID)
    {
        chain.push_back(cur);
        if (parents.count(cur) == 0 || parents[cur].node == INVALID_NODE_ID) break;
    }
    std::reverse(chain.begin(), chain.end());
    // First leg: virtual source → chain[0] (walk).
    {
        // Look up the cost we recorded in from_attach so the leg's
        // metadata reflects the actual attach hop.
        RouteLeg leg{};
        leg.from_node = INVALID_NODE_ID;
        leg.to_node   = chain.front();
        leg.kind      = EdgeKind::Walk;
        GraphNode const& dst = _nodes[chain.front()];
        leg.to_map    = dst.map_id;
        leg.to_x      = dst.x;
        leg.to_y      = dst.y;
        leg.to_z      = dst.z;
        out.legs.push_back(leg);
    }
    for (size_t i = 1; i < chain.size(); ++i)
    {
        Parent p = parents[chain[i]];
        RouteLeg leg{};
        leg.from_node  = p.node;
        leg.to_node    = chain[i];
        leg.kind       = p.kind;
        leg.payload_id = p.payload;
        GraphNode const& dst = _nodes[chain[i]];
        leg.to_map = dst.map_id;
        leg.to_x   = dst.x;
        leg.to_y   = dst.y;
        leg.to_z   = dst.z;
        out.legs.push_back(leg);
    }
    // Final leg: chain.back() → virtual sink (walk).
    {
        RouteLeg leg{};
        leg.from_node = chain.back();
        leg.to_node   = INVALID_NODE_ID;
        leg.kind      = EdgeKind::Walk;
        leg.to_map    = req.to_map;
        leg.to_x      = req.to_x;
        leg.to_y      = req.to_y;
        leg.to_z      = req.to_z;
        out.legs.push_back(leg);
    }
    out.total_cost = goal_cost;
    out.ok = true;
    return out;
}

} // namespace Playerbot::V2::Travel
