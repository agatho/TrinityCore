/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "ThreadSafePathfinder.h"
#include "MMapManager.h"
#include "Log.h"

namespace Playerbot
{

// Thread-local query storage — each worker thread gets its own instances
thread_local std::unordered_map<uint32, dtNavMeshQuery*> ThreadSafePathfinder::s_threadQueries;

dtNavMeshQuery* ThreadSafePathfinder::GetThreadLocalQuery(uint32 mapId)
{
    // Check if we already have a query for this map on this thread
    auto it = s_threadQueries.find(mapId);
    if (it != s_threadQueries.end())
        return it->second;

    // Get the shared dtNavMesh (read-only, thread-safe for reads)
    MMAP::MMapManager* mmap = MMAP::MMapManager::instance();
    if (!mmap)
        return nullptr;

    dtNavMesh* navMesh = mmap->GetNavMesh(mapId, 0);
    if (!navMesh)
        return nullptr;

    // Create a NEW dtNavMeshQuery for this thread
    // Each query has its own node pool and open list — fully independent
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query)
    {
        TC_LOG_ERROR("module.playerbot.pathfinding",
            "ThreadSafePathfinder: Failed to allocate dtNavMeshQuery for map {}", mapId);
        return nullptr;
    }

    // Initialize with shared navmesh, 1024 nodes (same as TrinityCore's MMapManager)
    dtStatus status = query->init(navMesh, 1024);
    if (dtStatusFailed(status))
    {
        TC_LOG_ERROR("module.playerbot.pathfinding",
            "ThreadSafePathfinder: Failed to init dtNavMeshQuery for map {} (status: {})",
            mapId, status);
        dtFreeNavMeshQuery(query);
        return nullptr;
    }

    s_threadQueries[mapId] = query;
    return query;
}

bool ThreadSafePathfinder::IsReachable(uint32 mapId, Position const& start, Position const& dest)
{
    dtNavMeshQuery* query = GetThreadLocalQuery(mapId);
    if (!query)
        return true; // No navmesh = assume reachable (don't block)

    // Set up query filter (same as TrinityCore's PathGenerator::CreateFilter)
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF); // Include all area types
    filter.setExcludeFlags(0);      // Exclude nothing

    // Find nearest poly to start position
    float startPos[3] = { start.GetPositionY(), start.GetPositionZ(), start.GetPositionX() };
    float destPos[3] = { dest.GetPositionY(), dest.GetPositionZ(), dest.GetPositionX() };
    float extents[3] = { 3.0f, 5.0f, 3.0f };

    dtPolyRef startRef = 0;
    dtPolyRef endRef = 0;
    float nearestStart[3], nearestEnd[3];

    dtStatus status = query->findNearestPoly(startPos, extents, &filter, &startRef, nearestStart);
    if (dtStatusFailed(status) || startRef == 0)
        return true; // Can't find start poly — assume reachable

    status = query->findNearestPoly(destPos, extents, &filter, &endRef, nearestEnd);
    if (dtStatusFailed(status) || endRef == 0)
        return false; // Destination has no navmesh poly — unreachable

    // Try to find a path
    dtPolyRef pathPolys[64]; // Short path check — we don't need the full path
    int pathCount = 0;

    status = query->findPath(startRef, endRef, nearestStart, nearestEnd,
                             &filter, pathPolys, &pathCount, 64);

    if (dtStatusFailed(status) || pathCount == 0)
        return false; // No path found

    // Check if the path actually reaches the destination
    // If the last poly in the path is not the end poly, the path is partial
    if (pathPolys[pathCount - 1] != endRef)
        return false; // Partial path — destination unreachable

    return true;
}

void ThreadSafePathfinder::Cleanup()
{
    for (auto& [mapId, query] : s_threadQueries)
    {
        if (query)
            dtFreeNavMeshQuery(query);
    }
    s_threadQueries.clear();
}

} // namespace Playerbot
