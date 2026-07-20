// UnifiedTravelGraph — single A*-routable graph that unifies every form
// of cross-map travel in WoW: walking, flight paths, static portals,
// continental ships/zeppelins, dungeon entrances, hearthstone, and
// quest-hub anchors. Replaces the disconnected per-system planners
// (PortalIndex BFS, QuestHub nearest-on-map, ad-hoc walk_to_taxi
// rules) with one router that can answer "from (map X, pos P) reach
// (map Y, pos Q)" as a typed sequence of legs.
//
// Why: TC's PathGenerator caps individual paths at ~296y, so any
// long-haul trip needs multi-hop composition through anchors. Our
// previous TravelPlanner (Phase E, BFS over portals only) couldn't
// compose ground+ship+flight+teleport into one route — bots stuck at
// "I'm in Stormwind, goal is Northrend, no direct portal exists"
// would just wander. The graph closes that gap.
//
// Architecture:
//   * GraphNode = a fixed travel anchor (flight master, portal, dock,
//     dungeon entrance, capital hub, synthetic bridge).
//   * GraphEdge = how to get from one node to another, with a kind
//     and a base cost (in arbitrary "travel-seconds" units, scaled
//     per kind so walk-300y costs about the same as a 1-hop taxi).
//   * Routing = A* with a same-map euclidean heuristic (0 for cross-
//     map since cross-map costs are pre-baked into edges).
//
// Threading: built once at boot from `Initialize()` on the world
// thread (reads sObjectMgr / sTaxiPathStore / sTransportMgr — all
// read-only after world load). Subsequent queries use a shared_mutex
// for read concurrency; the per-bot route memo is owned by BotAI and
// stays single-thread.

#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Player;

namespace Playerbot::V2::Travel {

enum class NodeKind : uint8
{
    FlightMaster   = 1,   // sTaxiNodesStore entry (per-faction reachable)
    Portal         = 2,   // PortalAnchor::Portal (SPELLCASTER GO + spell_target_position)
    Dock           = 3,   // PortalAnchor::Transport (ship/zeppelin)
    DungeonEntry   = 4,   // MapEntry::GetEntrancePos(map) projected onto parent map
    Capital        = 5,   // CapitalsTable (Stormwind/Org/Dalaran/Shrines)
    QuestHub       = 6,   // QuestHubDatabase cluster center
    Bridge         = 7,   // Synthetic; added by ConnectIslands() to close gaps
    Hearth         = 8,   // Virtual node anchored on bot's homebind; per-bot
    ElevatorStop   = 9,   // One per ElevatorIndex::ElevatorStop; same-map pairs joined by Elevator edges
    AreaTriggerTel = 10,  // areatrigger_teleport endpoint (source volume or landing point); same-map island bridges
};

enum class EdgeKind : uint8
{
    Walk     = 1,   // Same-map ground move. Cost = euclidean distance / run_speed.
    Taxi     = 2,   // Flight path. Cost = TaxiPath length / flight_speed.
    Portal   = 3,   // Spell teleport. Cost = ~3s cast + ~2s loading screen.
    Ship     = 4,   // Continental transport. Cost = half the period (avg wait).
    Hearth   = 5,   // Bind teleport. Cost = ~10s cast + 30min CD modelled as priority demotion.
    Teleport = 6,   // Misc instant teleport (dungeon entrance area-trigger, etc).
    Elevator = 7,   // Ride an in-world elevator platform (TB lifts, Org Cleft, UC main, etc).
};

// Stable per-session integer id; safe to memoize across ticks.
using NodeId = uint32_t;
static constexpr NodeId INVALID_NODE_ID = 0xFFFFFFFFu;

struct GraphNode
{
    NodeId   id          = INVALID_NODE_ID;
    NodeKind kind        = NodeKind::Bridge;
    uint32   map_id      = 0;
    float    x           = 0.f;
    float    y           = 0.f;
    float    z           = 0.f;
    // 0 = no condition (anyone). Non-zero = ConditionMgr::IsPlayerMeetingCondition
    // (faction / level / rep / quest gate). Used to filter per-bot routes.
    uint32   condition_id = 0;
    // For diagnostics + /traveldebug whisper.
    std::string name;
    // For flight masters and faction-gated nodes: 0 neutral, ALLIANCE, HORDE.
    uint32   faction     = 0;
    // Backing entity id (depends on kind):
    //   FlightMaster   -> TaxiNodesEntry::ID
    //   Portal/Dock    -> gameobject_template::entry
    //   DungeonEntry   -> destination map id
    //   Capital        -> CapitalsTable entry index
    //   QuestHub       -> QuestHub::hubId
    //   Bridge         -> 0
    //   Hearth         -> bot guid_low (per-bot virtual nodes don't enter the
    //                     adjacency table; resolved at routing time)
    uint32   payload_id  = 0;
};

struct GraphEdge
{
    NodeId   to          = INVALID_NODE_ID;
    EdgeKind kind        = EdgeKind::Walk;
    float    cost        = 0.f;
    uint32   condition_id = 0;
    // Edge-kind-specific payload. Single uint32 covers every kind today:
    //   Taxi   -> TaxiPathEntry::ID (we walk sTaxiPathNodesByPath[id] at exec time)
    //   Portal -> source GO entry (already encoded in source node, kept for diag)
    //   Ship   -> TransportTemplate ID (lets executor synchronise with arrival)
    //   Other  -> 0
    uint32   payload_id  = 0;
};

// A successfully-found route. Legs are ordered source→destination.
// Cost is the A* g-value of the destination (sum of edge costs).
struct RouteLeg
{
    NodeId   from_node = INVALID_NODE_ID;
    NodeId   to_node   = INVALID_NODE_ID;
    EdgeKind kind      = EdgeKind::Walk;
    // World coordinates of the leg's destination (denormalized from the
    // node for executor convenience — caller doesn't need to look up
    // the node again to move toward it).
    uint32   to_map    = 0;
    float    to_x      = 0.f;
    float    to_y      = 0.f;
    float    to_z      = 0.f;
    uint32   payload_id = 0;
};

struct Route
{
    bool                  ok            = false;
    float                 total_cost    = 0.f;
    std::vector<RouteLeg> legs;
    // Diagnostic: total nodes the A* search visited. Bounded by the
    // node count (low thousands); helps tune costs without re-running.
    uint32                visited_nodes = 0;
    // Diagnostics for ok=false triage: how many same-map graph nodes were
    // attachable from the source (post navmesh validation when
    // validate_source_walk is set) and to the destination. 0 on either side
    // means the graph has NO seed near that endpoint — a coverage gap
    // (missing hub/FM/elevator node or >kMaxWalkEdgeYds desert), as opposed
    // to "seeds exist but A* found no connecting path".
    uint32                from_attach_count = 0;
    uint32                to_attach_count   = 0;
};

// Query parameters. Source is the bot's actual position; destination is
// either a real graph node (when planning to a known POI like a dungeon
// entrance) or a free position the bot wants to reach (we attach a
// virtual sink node connected by walk-edges to nearby same-map nodes).
struct RouteRequest
{
    Player const* bot          = nullptr;   // for PlayerCondition gating
    uint32        from_map     = 0;
    float         from_x       = 0.f;
    float         from_y       = 0.f;
    float         from_z       = 0.f;
    uint32        to_map       = 0;
    float         to_x         = 0.f;
    float         to_y         = 0.f;
    float         to_z         = 0.f;
    // Search bounds. Default = generous; raise for huge cross-continent
    // multi-hop, lower for tighter cost on hot paths.
    uint32        max_visited  = 4000;
    // When true, allow Hearth-edge usage (default true). Disabled inside
    // BGs / dungeons where hearth is illegal.
    bool          allow_hearth = true;
    // When true, source-attach walk edges are validated against the LIVE
    // navmesh: an attach the bot cannot actually path to is dropped, so the
    // route is forced through genuinely reachable seed nodes. Without this
    // the attach is straight-line euclidean, which lets a bot wedged in a
    // navmesh-disconnected pocket (the Undercity interior, the SW portal
    // room) "attach" through a wall to a surface node — producing a walk-
    // only route that can never be executed and masking the real bridge
    // route (elevator / teleport) out of the pocket. Used by the snapshot
    // builder's wedged-objective probe. WORLD THREAD ONLY (PathGenerator
    // queries the live navmesh); requires `bot`. Default off: AI-worker
    // callers must not set this.
    bool          validate_source_walk = false;
    // Skip the trivial same-map direct-walk branch and force graph A*.
    // Set by callers whose previously-planted direct walk WEDGED (the
    // AI thread can't navmesh-validate, and geometry alone can't tell a
    // 540y/95z ramp from an elevator-only interior — Somi's UC→Brill
    // "trivial" walk was mesh-impossible).
    bool          skip_trivial = false;
};

class UnifiedTravelGraph
{
public:
    UnifiedTravelGraph()  = default;
    ~UnifiedTravelGraph() = default;

    // Build the entire graph. Idempotent — second call rebuilds from
    // scratch. Must be called AFTER PortalIndex and QuestHubDatabase
    // have completed their own Initialize.
    void Initialize();

    bool IsInitialized() const { return _initialized; }

    // --- Read API (thread-safe) -----------------------------------
    Route FindRoute(RouteRequest const& req) const;

    // Diagnostic accessors. Total counts are atomic; the per-node
    // walks lock the shared_mutex briefly.
    size_t GetNodeCount() const;
    size_t GetEdgeCount() const;
    // Aggregated per-NodeKind counts for `/traveldebug stats`.
    struct KindCounts {
        uint32 flight_masters = 0;
        uint32 portals        = 0;
        uint32 docks          = 0;
        uint32 dungeon_entries = 0;
        uint32 capitals       = 0;
        uint32 quest_hubs     = 0;
        uint32 bridges        = 0;
        uint32 elevator_stops = 0;
    };
    KindCounts GetKindCounts() const;

    // Direct node lookup (used by callers that know an anchor exists,
    // e.g. flight master at TaxiNodeID = X).
    GraphNode const* GetNode(NodeId id) const;

    // Spatial nearest-node query: returns the closest graph node on `map_id`
    // within `max_range` of (x, y). O(N) where N = nodes on that map
    // (~50-200 for continents). Returns nullptr if no node within range.
    // Thread-safe (shared lock).
    GraphNode const* FindNearestNodeOnMap(uint32 map_id, float x, float y,
                                          float max_range = 400.f) const;

private:
    // Build sub-passes — split so per-source bugs are isolatable and
    // the boot log shows what loaded vs. what didn't.
    void LoadFlightMasters();   // sTaxiNodesStore → FlightMaster nodes
    void LoadPortalsAndDocks(); // Services::Portals() → Portal/Dock nodes
    void LoadDungeonEntries();  // MapEntry::GetEntrancePos
    void LoadAreaTriggerTeleports(); // areatrigger_teleport -> same-map Teleport edges
    void LoadCapitals();        // CapitalsTable
    void LoadQuestHubs();       // Services::Hubs()
    void LoadElevators();       // ElevatorIndex::Stops() → ElevatorStop nodes

    void BuildTaxiEdges();      // sTaxiPathStore → directed Taxi edges
    void BuildPortalEdges();    // Portal/Dock cross-map edges (cost=Portal/Ship)
    void BuildWalkEdges();      // Same-map intra-cluster walk edges (bounded)
    void BuildElevatorEdges();  // Bidirectional Elevator edges between stops of the same platform

    // Synthetic-bridge pass: detects nodes that are connected only via
    // expensive cross-map kinds (no walk neighbors); adds Bridge nodes
    // between nearby clusters to keep the graph traversable when the
    // walk-edge pass missed a connection.
    void ConnectIslands();

    // Allocate a new node id (monotonic). Caller fills the struct
    // and returns the assigned id.
    NodeId AppendNode(GraphNode node);
    // Add a directed edge. For bidirectional connections call twice.
    void   AddEdge(NodeId from, GraphEdge edge);

    // Attaches a virtual "source" node connected to all reachable
    // graph nodes within `walk_radius_yd` on the request's source map.
    // Returns the virtual node id (valid only for the lifetime of the
    // returned `scratch` vector — the routing call owns it).
    struct ScratchNode {
        NodeId               id;
        GraphNode            node;
        std::vector<GraphEdge> edges_out;
        // In-edges are routed by patching the real-node side via
        // ScratchInEdge below.
    };
    struct ScratchInEdge {
        NodeId   from_real_node;
        GraphEdge edge_into_scratch;
    };

    // Members ------------------------------------------------------
    mutable std::shared_mutex _mtx;
    bool                      _initialized = false;
    std::vector<GraphNode>    _nodes;
    // Adjacency in the same order as _nodes (parallel arrays).
    std::vector<std::vector<GraphEdge>> _adj;
    // Quick index: (map_id) -> list of node ids on that map.
    std::unordered_map<uint32, std::vector<NodeId>> _nodes_by_map;
};

} // namespace Playerbot::V2::Travel
