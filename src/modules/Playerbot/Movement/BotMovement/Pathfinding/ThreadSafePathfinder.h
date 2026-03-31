/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Thread-safe pathfinding for bot worker threads.
 *
 * TrinityCore's PathGenerator uses a shared dtNavMeshQuery which is NOT
 * thread-safe. This class creates per-thread dtNavMeshQuery instances that
 * share the read-only dtNavMesh, allowing safe concurrent pathfinding from
 * bot worker threads.
 */

#pragma once

#include "Define.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "Position.h"
#include <unordered_map>
#include <mutex>

namespace Playerbot
{

/**
 * @brief Thread-safe pathfinding using per-thread dtNavMeshQuery instances
 *
 * Each worker thread gets its own dtNavMeshQuery (with independent node pool
 * and open list) that references the shared dtNavMesh. This eliminates the
 * data race that crashes when multiple threads call findPath() on the same
 * query object.
 *
 * Usage:
 *   bool reachable = ThreadSafePathfinder::IsReachable(mapId, startPos, destPos);
 */
class ThreadSafePathfinder
{
public:
    /**
     * @brief Check if a destination is reachable via navmesh from a start position
     *
     * @param mapId     Map ID to query navmesh for
     * @param start     Start position (bot's position)
     * @param dest      Destination position (NPC, objective, etc.)
     * @return true if navmesh path exists, false if NOPATH or navmesh unavailable
     *
     * Thread-safe — uses per-thread query objects.
     */
    static bool IsReachable(uint32 mapId, Position const& start, Position const& dest);

    /**
     * @brief Clean up thread-local query objects (call on shutdown)
     */
    static void Cleanup();

private:
    // Get or create a dtNavMeshQuery for the current thread + map
    static dtNavMeshQuery* GetThreadLocalQuery(uint32 mapId);

    // Thread-local storage: mapId -> query object
    static thread_local std::unordered_map<uint32, dtNavMeshQuery*> s_threadQueries;
};

} // namespace Playerbot
