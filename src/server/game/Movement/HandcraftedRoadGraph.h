/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_HANDCRAFTED_ROAD_GRAPH_H
#define TRINITYCORE_HANDCRAFTED_ROAD_GRAPH_H

#include "Define.h"
#include "Position.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class Map;
class PhaseShift;

/**
 * HandcraftedRoadGraph
 * --------------------
 * A routable centerline graph built from the `handcrafted_road` segments that
 * the world_editor authors and HandcraftedRoadStorage loads. Each segment
 * (fromX/fromY -> toX/toY) becomes a graph edge; coincident endpoints collapse
 * into a shared junction node, so chains of authored segments form connected
 * polylines a bot can follow.
 *
 * Where the NAV_AREA_ROAD area-cost bias merely nudges Detour's polygon choice,
 * this graph lets a bot route ALONG the authored centerline: snap to the nearest
 * road node, A* across the graph, and walk the node sequence. The area-cost bias
 * remains the fallback for trips that can't beneficially use a road.
 *
 * The `handcrafted_road` table carries no Z; ground height is resolved from the
 * live Map at route time. Graphs are built lazily per map and cached until the
 * underlying storage is reloaded (detected via HandcraftedRoadStorage::Generation()).
 *
 * Thread-safety: ComputeRoute / HasRoadNetwork may be called from any thread.
 * Built graphs are immutable; the cache is guarded by a shared_mutex and built
 * graphs are handed back as shared_ptr so a reload can't free one in use.
 */
class TC_GAME_API HandcraftedRoadGraph
{
public:
    static HandcraftedRoadGraph* Instance();

    // True if the given map has at least one routable road node (builds lazily).
    bool HasRoadNetwork(uint32 mapId);

    /**
     * Compute a road-following route between start and end on the given map.
     * On success, outWaypoints holds the ordered positions to visit:
     *   [ entry road node, intermediate road nodes..., exit road node, end ]
     * with Z resolved from the map. The caller's current position is the implicit
     * first leg and is NOT included. Returns false (and clears the vector) when no
     * beneficial road route exists.
     */
    bool ComputeRoute(Map* map, PhaseShift const& phase, Position const& start, Position const& end,
                      std::vector<Position>& outWaypoints);

    // Drop every cached graph (next request rebuilds). Also happens automatically
    // when HandcraftedRoadStorage::Generation() changes.
    void InvalidateAll();

    // Per-map node/edge counts for diagnostics (builds lazily). Returns false if
    // the map has no usable segments.
    bool GetMapInfo(uint32 mapId, uint32& outNodes, uint32& outEdges);

    struct Stats
    {
        std::atomic<uint64> routesRequested{0};
        std::atomic<uint64> routesUsed{0};
        std::atomic<uint64> fallbacks{0};
        // Fallback breakdown so 0%-used can be diagnosed: is it expected
        // sparsity (short trips / roadless maps / bot far from any road) or
        // gates rejecting otherwise-valid routes?
        std::atomic<uint64> fbShort{0};   // trip shorter than MinDistance
        std::atomic<uint64> fbNoGraph{0}; // map has no handcrafted road graph
        std::atomic<uint64> fbNoSnap{0};  // start/end not within MaxEntryDistance of a road
        std::atomic<uint64> fbReject{0};  // near roads but route rejected (entry/detour/no-path)
    };
    Stats const& GetStats() const { return _stats; }

private:
    HandcraftedRoadGraph() = default;
    ~HandcraftedRoadGraph() = default;
    HandcraftedRoadGraph(HandcraftedRoadGraph const&) = delete;
    HandcraftedRoadGraph& operator=(HandcraftedRoadGraph const&) = delete;

    struct Node
    {
        float x = 0.0f;
        float y = 0.0f;
        std::vector<uint32> edges; // indices into MapGraph::edges
    };

    struct Edge
    {
        uint32 a = 0;
        uint32 b = 0;
        float cost = 0.0f;
        uint32 Other(uint32 n) const { return n == a ? b : a; }
    };

    struct MapGraph
    {
        std::vector<Node> nodes;
        std::vector<Edge> edges;
    };

    std::shared_ptr<MapGraph const> GetOrBuild(uint32 mapId);
    static std::shared_ptr<MapGraph const> BuildFromStorage(uint32 mapId, float mergeEpsilon);

    // Lazily read config (RoadGraph.* in worldserver.conf) exactly once.
    void EnsureConfig();

    // Nearest node within maxRange (linear scan — graphs are small). UINT32_MAX if none.
    static uint32 NearestNode(MapGraph const& g, float x, float y, float maxRange);
    // A* over the node graph. Fills outNodes with the node index path, returns cost or -1.
    static float FindPath(MapGraph const& g, uint32 start, uint32 end, std::vector<uint32>& outNodes);

    mutable std::shared_mutex _mutex;
    // A present-but-null mapped value records "built, no usable graph".
    std::unordered_map<uint32, std::shared_ptr<MapGraph const>> _maps;
    std::atomic<uint32> _builtGeneration{0xFFFFFFFF};

    std::once_flag _configOnce;
    bool  _enabled = true;
    float _minDistance = 150.0f;
    float _maxDetourRatio = 1.6f;
    float _maxEntryDistance = 120.0f;
    float _nodeMergeEpsilon = 3.0f;

    mutable Stats _stats;
};

#define sHandcraftedRoadGraph HandcraftedRoadGraph::Instance()

#endif // TRINITYCORE_HANDCRAFTED_ROAD_GRAPH_H
