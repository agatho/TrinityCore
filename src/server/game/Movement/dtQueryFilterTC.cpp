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

#include "dtQueryFilterTC.h"
#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "MMapDefines.h"

#include <atomic>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace
{
    // Process-wide telemetry counters. lock-free, monotonic, reset by
    // dtQueryFilterTC::ResetStats(). All increments happen from
    // PathGenerator threads at the very end of CalculatePath, after the
    // pathfind has stabilised — so contention is bounded by path/sec
    // throughput, not getCost() edge-eval rate.
    std::atomic<uint64_t> g_pathsRun{0};
    std::atomic<uint64_t> g_pathsWithRoadPoly{0};
    std::atomic<uint64_t> g_pathsRoadBonusDisabled{0};
    std::atomic<uint64_t> g_pathsWithSlopePenalty{0};
    std::atomic<uint64_t> g_roadPolysVisited{0};
    std::atomic<uint64_t> g_totalPolysVisited{0};
    std::atomic<uint64_t> g_pathsInInstance{0};

    // Per-map counters: same fields but fragmented by source-unit map id.
    // Protected by g_perMapMutex because std::unordered_map isn't a
    // concurrent container; the table itself is rarely resized (one
    // entry per map id) so contention is bounded by /roadstats reads.
    // Path tally writes are short critical sections (~7 increments).
    std::mutex g_perMapMutex;
    std::unordered_map<uint32, dtQueryFilterTC::RoadStats> g_perMapStats;
}

dtQueryFilterTC::dtQueryFilterTC() = default;

void dtQueryFilterTC::BeginPathStats() const
{
    _maxSlopeFactorThisPath = 1.0f;
}

dtQueryFilterTC::RoadStats dtQueryFilterTC::SampleStats()
{
    RoadStats s;
    s.pathsRun                = g_pathsRun.load(std::memory_order_relaxed);
    s.pathsWithRoadPoly       = g_pathsWithRoadPoly.load(std::memory_order_relaxed);
    s.pathsRoadBonusDisabled  = g_pathsRoadBonusDisabled.load(std::memory_order_relaxed);
    s.pathsWithSlopePenalty   = g_pathsWithSlopePenalty.load(std::memory_order_relaxed);
    s.roadPolysVisited        = g_roadPolysVisited.load(std::memory_order_relaxed);
    s.totalPolysVisited       = g_totalPolysVisited.load(std::memory_order_relaxed);
    s.pathsInInstance         = g_pathsInInstance.load(std::memory_order_relaxed);
    return s;
}

void dtQueryFilterTC::ResetStats()
{
    g_pathsRun.store(0, std::memory_order_relaxed);
    g_pathsWithRoadPoly.store(0, std::memory_order_relaxed);
    g_pathsRoadBonusDisabled.store(0, std::memory_order_relaxed);
    g_pathsWithSlopePenalty.store(0, std::memory_order_relaxed);
    g_roadPolysVisited.store(0, std::memory_order_relaxed);
    g_totalPolysVisited.store(0, std::memory_order_relaxed);
    g_pathsInInstance.store(0, std::memory_order_relaxed);

    std::scoped_lock lock(g_perMapMutex);
    g_perMapStats.clear();
}

dtQueryFilterTC::RoadStats dtQueryFilterTC::SampleStatsForMap(uint32 mapId)
{
    std::scoped_lock lock(g_perMapMutex);
    auto it = g_perMapStats.find(mapId);
    if (it == g_perMapStats.end())
        return RoadStats{};   // zero-initialised
    return it->second;
}

std::vector<uint32> dtQueryFilterTC::ListMapsWithStats()
{
    std::scoped_lock lock(g_perMapMutex);
    std::vector<uint32> out;
    out.reserve(g_perMapStats.size());
    for (auto const& [mapId, _] : g_perMapStats)
        out.push_back(mapId);
    return out;
}

void dtQueryFilterTC::TallyPath(dtNavMesh const* mesh,
                                dtPolyRef const* polys, uint32 polyCount,
                                bool roadBonusDisabled,
                                float maxSlopeFactor,
                                bool inInstance,
                                uint32 mapId)
{
    g_pathsRun.fetch_add(1, std::memory_order_relaxed);
    if (roadBonusDisabled)
        g_pathsRoadBonusDisabled.fetch_add(1, std::memory_order_relaxed);
    if (maxSlopeFactor > 1.10f)
        g_pathsWithSlopePenalty.fetch_add(1, std::memory_order_relaxed);
    if (inInstance)
        g_pathsInInstance.fetch_add(1, std::memory_order_relaxed);

    uint32 roadCount = 0;
    if (mesh && polys && polyCount > 0)
    {
        for (uint32 i = 0; i < polyCount; ++i)
        {
            dtMeshTile const* tile = nullptr;
            dtPoly const* poly = nullptr;
            if (dtStatusFailed(mesh->getTileAndPolyByRef(polys[i], &tile, &poly)))
                continue;
            if (poly && poly->getArea() == NAV_AREA_ROAD)
                ++roadCount;
        }

        g_totalPolysVisited.fetch_add(polyCount, std::memory_order_relaxed);
        g_roadPolysVisited.fetch_add(roadCount, std::memory_order_relaxed);
        if (roadCount > 0)
            g_pathsWithRoadPoly.fetch_add(1, std::memory_order_relaxed);
    }

    // Per-map record. kNoMapId means "no map id resolved" — skip
    // per-map tracking entirely. Map 0 IS Eastern Kingdoms and MUST be
    // tracked normally (the previous "mapId != 0" gate accidentally
    // hid all EK road biasing from the per-map view).
    if (mapId != kNoMapId)
    {
        std::scoped_lock lock(g_perMapMutex);
        RoadStats& m = g_perMapStats[mapId];
        ++m.pathsRun;
        if (roadBonusDisabled)
            ++m.pathsRoadBonusDisabled;
        if (maxSlopeFactor > 1.10f)
            ++m.pathsWithSlopePenalty;
        if (inInstance)
            ++m.pathsInInstance;
        m.totalPolysVisited += polyCount;
        m.roadPolysVisited += roadCount;
        if (roadCount > 0)
            ++m.pathsWithRoadPoly;
    }
}

float dtQueryFilterTC::ComputeSlopeMultiplier(float const* pa, float const* pb,
                                              float slopeCoefficient)
{
    // Detour coord system: Y is up. Slope = angle between (pb - pa) and the
    // horizontal plane. Computed from the Y-delta over the horizontal
    // distance: tan(angle) = |dy| / hypot(dx, dz).
    float dx = pb[0] - pa[0];
    float dy = pb[1] - pa[1];
    float dz = pb[2] - pa[2];

    float horiz = std::sqrt(dx * dx + dz * dz);
    if (horiz < 1e-6f)
        return 1.0f;   // pure vertical segment — slope undefined, no penalty

    float slopeRad = std::atan(std::fabs(dy) / horiz);
    float slopeDeg = slopeRad * (180.0f / 3.14159265358979323846f);

    // Cost = 1.0 + slopeDeg * coefficient / 100.0
    // - At 0° (flat): 1.0
    // - At 30°:       1.30 (with coefficient 1.0)
    // - At 60°:       1.60
    // - At 80°:       1.80
    // Capped at 2.0 to prevent runaway costs for near-vertical segments
    // that snuck past the walkable-slope filter.
    float factor = 1.0f + slopeDeg * slopeCoefficient / 100.0f;
    if (factor > 2.0f) factor = 2.0f;
    if (factor < 1.0f) factor = 1.0f;
    return factor;
}

float dtQueryFilterTC::getCost(float const* pa, float const* pb,
    dtPolyRef /*prevRef*/, dtMeshTile const* /*prevTile*/, dtPoly const* /*prevPoly*/,
    dtPolyRef /*curRef*/,  dtMeshTile const* /*curTile*/,  dtPoly const* curPoly,
    dtPolyRef /*nextRef*/, dtMeshTile const* /*nextTile*/, dtPoly const* /*nextPoly*/) const
{
    float dist = dtVdist(pa, pb);

    // Area cost lookup. If road bonus is disabled for this filter
    // instance, override the road area's cost back to 1.0 (no preference).
    unsigned char area = curPoly->getArea();
    float areaCost = getAreaCost(area);
    if (_disableRoadBonus && area == NAV_AREA_ROAD)
        areaCost = 1.0f;

    float slopeMul = ComputeSlopeMultiplier(pa, pb, _slopeCoefficient);

    // Telemetry: track the largest slope factor observed on any segment
    // of this pathfind. PathGenerator reads after CalculatePath.
    if (slopeMul > _maxSlopeFactorThisPath)
        _maxSlopeFactorThisPath = slopeMul;

    return dist * areaCost * slopeMul;
}
