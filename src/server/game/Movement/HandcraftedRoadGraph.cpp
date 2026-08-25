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

#include "HandcraftedRoadGraph.h"
#include "HandcraftedRoadStorage.h"
#include "Config.h"
#include "GridDefines.h"
#include "Log.h"
#include "Map.h"
#include "PhaseShift.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace
{
    inline float Dist2D(float ax, float ay, float bx, float by)
    {
        float dx = ax - bx;
        float dy = ay - by;
        return std::sqrt(dx * dx + dy * dy);
    }

    inline uint64 CellKey(int32 cx, int32 cy)
    {
        return (static_cast<uint64>(static_cast<uint32>(cx)) << 32)
             |  static_cast<uint64>(static_cast<uint32>(cy));
    }

    // Resolve a ground height for an authored road node (the handcrafted_road
    // table stores only X/Y). Seeded high so the terrain/vmap search descends to
    // the road surface; falls back to the seed Z if the map can't resolve one.
    float ResolveGroundZ(Map* map, PhaseShift const& phase, float x, float y, float seedZ)
    {
        float z = map->GetHeight(phase, x, y, MAX_HEIGHT, true);
        if (z <= INVALID_HEIGHT + 1.0f)
            z = map->GetHeight(phase, x, y, seedZ + 20.0f, true);
        if (z <= INVALID_HEIGHT + 1.0f)
            z = seedZ;
        return z;
    }
}

HandcraftedRoadGraph* HandcraftedRoadGraph::Instance()
{
    static HandcraftedRoadGraph instance;
    return &instance;
}

void HandcraftedRoadGraph::EnsureConfig()
{
    std::call_once(_configOnce, [this]()
    {
        _enabled         = sConfigMgr->GetBoolDefault("RoadGraph.Enable", true);
        _minDistance     = sConfigMgr->GetFloatDefault("RoadGraph.MinDistance", 150.0f);
        _maxDetourRatio  = sConfigMgr->GetFloatDefault("RoadGraph.MaxDetourRatio", 1.6f);
        _maxEntryDistance = sConfigMgr->GetFloatDefault("RoadGraph.MaxEntryDistance", 120.0f);
        _nodeMergeEpsilon = sConfigMgr->GetFloatDefault("RoadGraph.NodeMergeEpsilon", 3.0f);
        if (_nodeMergeEpsilon < 0.1f)
            _nodeMergeEpsilon = 3.0f;

        TC_LOG_INFO("server.loading",
            "HandcraftedRoadGraph: {} (minDist={}, maxDetour={}, maxEntry={}, mergeEps={})",
            _enabled ? "enabled" : "disabled", _minDistance, _maxDetourRatio,
            _maxEntryDistance, _nodeMergeEpsilon);
    });
}

void HandcraftedRoadGraph::InvalidateAll()
{
    std::unique_lock<std::shared_mutex> wl(_mutex);
    _maps.clear();
}

std::shared_ptr<HandcraftedRoadGraph::MapGraph const>
HandcraftedRoadGraph::BuildFromStorage(uint32 mapId, float mergeEpsilon)
{
    std::vector<HandcraftedRoadSegment> const& segs = HandcraftedRoadStorage::GetForMap(mapId);
    if (segs.empty())
        return nullptr;

    auto graph = std::make_shared<MapGraph>();
    MapGraph& g = *graph;

    float const eps2 = mergeEpsilon * mergeEpsilon;
    float const cell = std::max(mergeEpsilon, 1.0f);
    std::unordered_map<uint64, std::vector<uint32>> buckets;

    auto getOrAddNode = [&](float x, float y) -> uint32
    {
        int32 cx = static_cast<int32>(std::floor(x / cell));
        int32 cy = static_cast<int32>(std::floor(y / cell));
        for (int32 dx = -1; dx <= 1; ++dx)
        {
            for (int32 dy = -1; dy <= 1; ++dy)
            {
                auto it = buckets.find(CellKey(cx + dx, cy + dy));
                if (it == buckets.end())
                    continue;
                for (uint32 ni : it->second)
                {
                    float ex = g.nodes[ni].x - x;
                    float ey = g.nodes[ni].y - y;
                    if (ex * ex + ey * ey <= eps2)
                        return ni;
                }
            }
        }
        uint32 idx = static_cast<uint32>(g.nodes.size());
        Node n;
        n.x = x;
        n.y = y;
        g.nodes.push_back(std::move(n));
        buckets[CellKey(cx, cy)].push_back(idx);
        return idx;
    };

    for (HandcraftedRoadSegment const& s : segs)
    {
        uint32 a = getOrAddNode(s.fromX, s.fromY);
        uint32 b = getOrAddNode(s.toX, s.toY);
        if (a == b)
            continue; // degenerate / sub-epsilon

        uint32 edgeIdx = static_cast<uint32>(g.edges.size());
        Edge e;
        e.a = a;
        e.b = b;
        e.cost = Dist2D(s.fromX, s.fromY, s.toX, s.toY);
        g.edges.push_back(e);
        g.nodes[a].edges.push_back(edgeIdx);
        g.nodes[b].edges.push_back(edgeIdx);
    }

    if (g.nodes.empty())
        return nullptr;

    TC_LOG_INFO("server.loading",
        "HandcraftedRoadGraph: built map {} graph — {} segments -> {} nodes, {} edges",
        mapId, uint32(segs.size()), uint32(g.nodes.size()), uint32(g.edges.size()));

    return graph;
}

std::shared_ptr<HandcraftedRoadGraph::MapGraph const> HandcraftedRoadGraph::GetOrBuild(uint32 mapId)
{
    // Detect a `.reload handcrafted_road` (or any storage reload) and drop the
    // stale cache so the next build reflects the new segments.
    uint32 const gen = HandcraftedRoadStorage::Generation();
    if (gen != _builtGeneration.load(std::memory_order_acquire))
    {
        std::unique_lock<std::shared_mutex> wl(_mutex);
        if (gen != _builtGeneration.load(std::memory_order_relaxed))
        {
            _maps.clear();
            _builtGeneration.store(gen, std::memory_order_release);
        }
    }

    {
        std::shared_lock<std::shared_mutex> rl(_mutex);
        auto it = _maps.find(mapId);
        if (it != _maps.end())
            return it->second; // may be null: "built, no usable graph"
    }

    std::unique_lock<std::shared_mutex> wl(_mutex);
    auto it = _maps.find(mapId);
    if (it != _maps.end())
        return it->second;

    std::shared_ptr<MapGraph const> built = BuildFromStorage(mapId, _nodeMergeEpsilon);
    _maps[mapId] = built;
    return built;
}

uint32 HandcraftedRoadGraph::NearestNode(MapGraph const& g, float x, float y, float maxRange)
{
    float bestSq = maxRange * maxRange;
    uint32 best = std::numeric_limits<uint32>::max();
    for (uint32 i = 0; i < g.nodes.size(); ++i)
    {
        float dx = g.nodes[i].x - x;
        float dy = g.nodes[i].y - y;
        float d = dx * dx + dy * dy;
        if (d <= bestSq)
        {
            bestSq = d;
            best = i;
        }
    }
    return best;
}

float HandcraftedRoadGraph::FindPath(MapGraph const& g, uint32 start, uint32 end,
                                     std::vector<uint32>& outNodes)
{
    outNodes.clear();
    if (start >= g.nodes.size() || end >= g.nodes.size())
        return -1.0f;
    if (start == end)
    {
        outNodes.push_back(start);
        return 0.0f;
    }

    auto heuristic = [&](uint32 n)
    {
        return Dist2D(g.nodes[n].x, g.nodes[n].y, g.nodes[end].x, g.nodes[end].y);
    };

    using PQ = std::pair<float, uint32>; // (f, node)
    std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> open;
    std::unordered_map<uint32, float> gScore;
    std::unordered_map<uint32, uint32> cameFrom;

    gScore[start] = 0.0f;
    open.push({ heuristic(start), start });

    while (!open.empty())
    {
        auto [f, cur] = open.top();
        open.pop();

        if (cur == end)
        {
            float total = gScore[end];
            uint32 n = end;
            while (n != start)
            {
                outNodes.push_back(n);
                n = cameFrom[n];
            }
            outNodes.push_back(start);
            std::reverse(outNodes.begin(), outNodes.end());
            return total;
        }

        float curG = gScore[cur];
        if (f - heuristic(cur) > curG + 0.001f)
            continue; // stale queue entry

        for (uint32 edgeIdx : g.nodes[cur].edges)
        {
            Edge const& e = g.edges[edgeIdx];
            uint32 nb = e.Other(cur);
            float tentative = curG + e.cost;
            auto it = gScore.find(nb);
            if (it != gScore.end() && tentative >= it->second)
                continue;
            gScore[nb] = tentative;
            cameFrom[nb] = cur;
            open.push({ tentative + heuristic(nb), nb });
        }
    }

    return -1.0f; // no path
}

bool HandcraftedRoadGraph::HasRoadNetwork(uint32 mapId)
{
    EnsureConfig();
    if (!_enabled)
        return false;
    return GetOrBuild(mapId) != nullptr;
}

bool HandcraftedRoadGraph::GetMapInfo(uint32 mapId, uint32& outNodes, uint32& outEdges)
{
    std::shared_ptr<MapGraph const> g = GetOrBuild(mapId);
    if (!g)
    {
        outNodes = outEdges = 0;
        return false;
    }
    outNodes = static_cast<uint32>(g->nodes.size());
    outEdges = static_cast<uint32>(g->edges.size());
    return true;
}

bool HandcraftedRoadGraph::ComputeRoute(Map* map, PhaseShift const& phase, Position const& start,
                                        Position const& end, std::vector<Position>& outWaypoints)
{
    outWaypoints.clear();
    EnsureConfig();

    if (!map || !_enabled)
        return false;

    uint64 const reqN = _stats.routesRequested.fetch_add(1, std::memory_order_relaxed) + 1;
    // Periodic aggregate so the logs show real road-route USAGE (used vs fell
    // back) over time, self-throttled by request volume (no timer needed).
    if ((reqN % 500) == 0)
        TC_LOG_INFO("server.loading",
            "HandcraftedRoadGraph usage: requested={} used={} ({}% used) | fallbacks: "
            "short={} noGraph={} notNearRoad={} rejected={}",
            reqN, _stats.routesUsed.load(std::memory_order_relaxed),
            uint32(_stats.routesUsed.load(std::memory_order_relaxed) * 100 / std::max<uint64>(reqN, 1)),
            _stats.fbShort.load(std::memory_order_relaxed),
            _stats.fbNoGraph.load(std::memory_order_relaxed),
            _stats.fbNoSnap.load(std::memory_order_relaxed),
            _stats.fbReject.load(std::memory_order_relaxed));

    float const sx = start.GetPositionX(), sy = start.GetPositionY();
    float const ex = end.GetPositionX(),   ey = end.GetPositionY();
    float const directDist = Dist2D(sx, sy, ex, ey);

    if (directDist < _minDistance)
    {
        _stats.fbShort.fetch_add(1, std::memory_order_relaxed);
        _stats.fallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::shared_ptr<MapGraph const> graphPtr = GetOrBuild(map->GetId());
    if (!graphPtr)
    {
        _stats.fbNoGraph.fetch_add(1, std::memory_order_relaxed);
        _stats.fallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    MapGraph const& g = *graphPtr;

    // Entry/exit reach scales with trip length: a long journey is worth walking
    // further to reach the road network (a fixed 120y cap meant any trip with an
    // off-road destination — i.e. most of them — never qualified). The
    // entry-dominates (0.8x) and detour-ratio gates below keep this honest: a
    // road that's too far out of the way is still rejected.
    float const entryCap = std::max(_maxEntryDistance, directDist * 0.5f);
    uint32 startNode = NearestNode(g, sx, sy, entryCap);
    uint32 endNode   = NearestNode(g, ex, ey, entryCap);
    if (startNode == std::numeric_limits<uint32>::max() ||
        endNode   == std::numeric_limits<uint32>::max() ||
        startNode == endNode)
    {
        _stats.fbNoSnap.fetch_add(1, std::memory_order_relaxed);
        _stats.fallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    float const startEntry = Dist2D(sx, sy, g.nodes[startNode].x, g.nodes[startNode].y);
    float const endEntry   = Dist2D(ex, ey, g.nodes[endNode].x, g.nodes[endNode].y);

    // If just reaching/leaving the road eats most of the trip, it won't help.
    if (startEntry + endEntry > directDist * 0.8f)
    {
        _stats.fbReject.fetch_add(1, std::memory_order_relaxed);
        _stats.fallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::vector<uint32> nodePath;
    float roadCost = FindPath(g, startNode, endNode, nodePath);
    if (roadCost < 0.0f || nodePath.size() < 2)
    {
        _stats.fbReject.fetch_add(1, std::memory_order_relaxed);
        _stats.fallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    float const totalRoadDist = roadCost + startEntry + endEntry;
    if (totalRoadDist > directDist * _maxDetourRatio)
    {
        _stats.fbReject.fetch_add(1, std::memory_order_relaxed);
        _stats.fallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    outWaypoints.reserve(nodePath.size() + 1);
    float const seedZ = start.GetPositionZ();
    for (uint32 ni : nodePath)
    {
        float nx = g.nodes[ni].x;
        float ny = g.nodes[ni].y;
        float nz = ResolveGroundZ(map, phase, nx, ny, seedZ);
        outWaypoints.emplace_back(nx, ny, nz, 0.0f);
    }
    // Finish at the real destination, not the last on-road node.
    outWaypoints.push_back(end);

    _stats.routesUsed.fetch_add(1, std::memory_order_relaxed);
    return true;
}
