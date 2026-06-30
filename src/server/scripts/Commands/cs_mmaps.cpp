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

/**
* @file cs_mmaps.cpp
* @brief .mmap related commands
*
* This file contains the CommandScripts for all
* mmap sub-commands
*/

#include "ScriptMgr.h"
#include "CellImpl.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "GridNotifiersImpl.h"
#include "MMapManager.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "PointMovementGenerator.h"
#include "RBAC.h"
#include "WorldSession.h"

#include <DetourCommon.h>

#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <queue>
#include <unordered_set>

#if TRINITY_COMPILER_IS_GCC
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace Trinity::ChatCommands;

class mmaps_commandscript : public CommandScript
{
public:
    mmaps_commandscript() : CommandScript("mmaps_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable mmapCommandTable =
        {
            { "loadedtiles", rbac::RBAC_PERM_COMMAND_MMAP_LOADEDTILES, false, &HandleMmapLoadedTilesCommand, "" },
            { "loc",         rbac::RBAC_PERM_COMMAND_MMAP_LOC,         false, &HandleMmapLocCommand,         "" },
            { "path",        rbac::RBAC_PERM_COMMAND_MMAP_PATH,        false, &HandleMmapPathCommand,        "" },
            { "stats",       rbac::RBAC_PERM_COMMAND_MMAP_STATS,       false, &HandleMmapStatsCommand,       "" },
            { "testarea",    rbac::RBAC_PERM_COMMAND_MMAP_TESTAREA,    false, &HandleMmapTestArea,           "" },
            // Auto-derives off-mesh connections from areatrigger_teleport
            // joined with AreaTrigger.db2 + WorldSafeLocs DB2. Writes a
            // text file in the format mmaps_generator --offMeshInput
            // expects. Same-map only (cross-map portals are out of scope
            // for off-mesh; UnifiedTravelGraph handles those instead).
            { "emitoffmesh", rbac::RBAC_PERM_COMMAND_MMAP_STATS,       true,  &HandleMmapEmitOffmesh,         "" },
            { "audit",       rbac::RBAC_PERM_COMMAND_MMAP_STATS,       false, &HandleMmapAuditCommand,        "" },
        };

        static ChatCommandTable commandTable =
        {
            { "mmap", rbac::RBAC_PERM_COMMAND_MMAP, true, nullptr, "", mmapCommandTable },
        };
        return commandTable;
    }

    static bool HandleMmapPathCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!MMAP::MMapManager::instance()->GetNavMesh(player->GetMapId(), player->GetInstanceId()))
        {
            handler->PSendSysMessage("NavMesh not loaded for current map.");
            return true;
        }

        handler->PSendSysMessage("mmap path:");

        // units
        Unit* target = handler->getSelectedUnit();
        if (!player || !target)
        {
            handler->PSendSysMessage("Invalid target/source selection.");
            return true;
        }

        char* para = strtok((char*)args, " ");

        bool useStraightPath = false;
        if (para && strcmp(para, "true") == 0)
            useStraightPath = true;

        bool useRaycast = false;
        if (para && (strcmp(para, "line") == 0 || strcmp(para, "ray") == 0 || strcmp(para, "raycast") == 0))
            useRaycast = true;

        // unit locations
        float x, y, z;
        player->GetPosition(x, y, z);

        // path
        PathGenerator path(target);
        path.SetUseStraightPath(useStraightPath);
        path.SetUseRaycast(useRaycast);
        bool result = path.CalculatePath(x, y, z, false);

        Movement::PointsArray const& pointPath = path.GetPath();
        handler->PSendSysMessage("%s's path to %s:", target->GetName().c_str(), player->GetName().c_str());
        handler->PSendSysMessage("Building: %s", useStraightPath ? "StraightPath" : useRaycast ? "Raycast" : "SmoothPath");
        handler->PSendSysMessage("Result: %s - Length: %zu - Type: %u", (result ? "true" : "false"), pointPath.size(), path.GetPathType());

        G3D::Vector3 const& start = path.GetStartPosition();
        G3D::Vector3 const& end = path.GetEndPosition();
        G3D::Vector3 const& actualEnd = path.GetActualEndPosition();

        handler->PSendSysMessage("StartPosition     (%.3f, %.3f, %.3f)", start.x, start.y, start.z);
        handler->PSendSysMessage("EndPosition       (%.3f, %.3f, %.3f)", end.x, end.y, end.z);
        handler->PSendSysMessage("ActualEndPosition (%.3f, %.3f, %.3f)", actualEnd.x, actualEnd.y, actualEnd.z);

        if (!player->IsGameMaster())
            handler->PSendSysMessage("Enable GM mode to see the path points.");

        for (uint32 i = 0; i < pointPath.size(); ++i)
            player->SummonCreature(VISUAL_WAYPOINT, pointPath[i].x, pointPath[i].y, pointPath[i].z, 0, TEMPSUMMON_TIMED_DESPAWN, 9s);

        return true;
    }

    static bool HandleMmapLocCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->PSendSysMessage("mmap tileloc:");

        // grid tile location
        Player* player = handler->GetSession()->GetPlayer();

        int32 gx = 32 - player->GetPositionX() / SIZE_OF_GRIDS;
        int32 gy = 32 - player->GetPositionY() / SIZE_OF_GRIDS;

        float x, y, z;
        player->GetPosition(x, y, z);

        // calculate navmesh tile location
        uint32 terrainMapId = PhasingHandler::GetTerrainMapId(player->GetPhaseShift(), player->GetMapId(), player->GetMap()->GetTerrain(), x, y);

        handler->PSendSysMessage("%04u_%02i_%02i.mmtile", terrainMapId, gx, gy);
        handler->PSendSysMessage("tileloc [%i, %i]", gy, gx);

        dtNavMesh const* navmesh = MMAP::MMapManager::instance()->GetNavMesh(terrainMapId, player->GetInstanceId());
        dtNavMeshQuery const* navmeshquery = MMAP::MMapManager::instance()->GetNavMeshQuery(terrainMapId, player->GetMapId(), player->GetInstanceId());
        if (!navmesh || !navmeshquery)
        {
            handler->PSendSysMessage("NavMesh not loaded for current map.");
            return true;
        }

        float const* min = navmesh->getParams()->orig;
        float location[VERTEX_SIZE] = { y, z, x };
        float extents[VERTEX_SIZE] = { 3.0f, 5.0f, 3.0f };

        int32 tilex = int32((y - min[0]) / SIZE_OF_GRIDS);
        int32 tiley = int32((x - min[2]) / SIZE_OF_GRIDS);

        handler->PSendSysMessage("Calc   [%02i, %02i]", tilex, tiley);

        // navmesh poly -> navmesh tile location
        dtQueryFilter filter = dtQueryFilter();
        dtPolyRef polyRef = INVALID_POLYREF;
        if (dtStatusFailed(navmeshquery->findNearestPoly(location, extents, &filter, &polyRef, nullptr)))
        {
            handler->PSendSysMessage("Dt     [??,??] (invalid poly, probably no tile loaded)");
            return true;
        }

        if (polyRef == INVALID_POLYREF)
            handler->PSendSysMessage("Dt     [??, ??] (invalid poly, probably no tile loaded)");
        else
        {
            dtMeshTile const* tile;
            dtPoly const* poly;
            if (dtStatusSucceed(navmesh->getTileAndPolyByRef(polyRef, &tile, &poly)))
            {
                if (tile)
                {
                    handler->PSendSysMessage("Dt     [%02i,%02i]", tile->header->x, tile->header->y);
                    return true;
                }
            }

            handler->PSendSysMessage("Dt     [??,??] (no tile loaded)");
        }

        return true;
    }

    static bool HandleMmapLoadedTilesCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 terrainMapId = PhasingHandler::GetTerrainMapId(player->GetPhaseShift(), player->GetMapId(), player->GetMap()->GetTerrain(), player->GetPositionX(), player->GetPositionY());
        dtNavMesh const* navmesh = MMAP::MMapManager::instance()->GetNavMesh(terrainMapId, player->GetInstanceId());
        dtNavMeshQuery const* navmeshquery = MMAP::MMapManager::instance()->GetNavMeshQuery(terrainMapId, player->GetMapId(), player->GetInstanceId());
        if (!navmesh || !navmeshquery)
        {
            handler->PSendSysMessage("NavMesh not loaded for current map.");
            return true;
        }

        handler->PSendSysMessage("mmap loadedtiles:");

        for (int32 i = 0; i < navmesh->getMaxTiles(); ++i)
        {
            dtMeshTile const* tile = navmesh->getTile(i);
            if (!tile || !tile->header)
                continue;

            handler->PSendSysMessage("[%02i, %02i]", tile->header->x, tile->header->y);
        }

        return true;
    }

    static bool HandleMmapStatsCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 terrainMapId = PhasingHandler::GetTerrainMapId(player->GetPhaseShift(), player->GetMapId(), player->GetMap()->GetTerrain(), player->GetPositionX(), player->GetPositionY());
        handler->PSendSysMessage("mmap stats:");
        handler->PSendSysMessage("  global mmap pathfinding is %sabled", DisableMgr::IsPathfindingEnabled(player->GetMapId()) ? "en" : "dis");

        MMAP::MMapManager* manager = MMAP::MMapManager::instance();
        handler->PSendSysMessage(" %u maps loaded with %u tiles overall", manager->getLoadedMapsCount(), manager->getLoadedTilesCount());

        dtNavMesh const* navmesh = manager->GetNavMesh(terrainMapId, player->GetInstanceId());
        if (!navmesh)
        {
            handler->PSendSysMessage("NavMesh not loaded for current map.");
            return true;
        }

        uint32 tileCount = 0;
        uint32 nodeCount = 0;
        uint32 polyCount = 0;
        uint32 vertCount = 0;
        uint32 triCount = 0;
        uint32 triVertCount = 0;
        uint32 dataSize = 0;
        for (int32 i = 0; i < navmesh->getMaxTiles(); ++i)
        {
            dtMeshTile const* tile = navmesh->getTile(i);
            if (!tile || !tile->header)
                continue;

            tileCount++;
            nodeCount += tile->header->bvNodeCount;
            polyCount += tile->header->polyCount;
            vertCount += tile->header->vertCount;
            triCount += tile->header->detailTriCount;
            triVertCount += tile->header->detailVertCount;
            dataSize += tile->dataSize;
        }

        handler->PSendSysMessage("Navmesh stats:");
        handler->PSendSysMessage(" %u tiles loaded", tileCount);
        handler->PSendSysMessage(" %u BVTree nodes", nodeCount);
        handler->PSendSysMessage(" %u polygons (%u vertices)", polyCount, vertCount);
        handler->PSendSysMessage(" %u triangles (%u vertices)", triCount, triVertCount);
        handler->PSendSysMessage(" %.2f MB of data (not including pointers)", ((float)dataSize / sizeof(unsigned char)) / 1048576);

        return true;
    }

    static bool HandleMmapTestArea(ChatHandler* handler, char const* /*args*/)
    {
        float radius = 40.0f;
        WorldObject* object = handler->GetSession()->GetPlayer();

        // Get Creatures
        std::list<Creature*> creatureList;
        Trinity::AnyUnitInObjectRangeCheck go_check(object, radius);
        Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> go_search(object, creatureList, go_check);
        Cell::VisitGridObjects(object, go_search, radius);

        if (!creatureList.empty())
        {
            handler->PSendSysMessage("Found %zu Creatures.", creatureList.size());

            uint32 paths = 0;
            uint32 uStartTime = getMSTime();

            float gx, gy, gz;
            object->GetPosition(gx, gy, gz);
            for (std::list<Creature*>::iterator itr = creatureList.begin(); itr != creatureList.end(); ++itr)
            {
                PathGenerator path(*itr);
                path.CalculatePath(gx, gy, gz);
                ++paths;
            }

            uint32 uPathLoadTime = getMSTimeDiff(uStartTime, getMSTime());
            handler->PSendSysMessage("Generated %i paths in %i ms", paths, uPathLoadTime);
        }
        else
            handler->PSendSysMessage("No creatures in %f yard range.", radius);

        return true;
    }

    // .mmap emitoffmesh [outputPath]
    //
    // Walks every entry in AreaTrigger.db2, looks up the
    // areatrigger_teleport row via ObjectMgr::GetAreaTrigger, and writes
    // a same-map (source.ContinentID == dest.MapID) off-mesh-connection
    // record per pair. The file is in the format consumed by
    // mmaps_generator --offMeshInput:
    //
    //   "<mapId> <tileX>,<tileY> (sx sy sz) (ex ey ez) <radius> <area> <flag>   // <note>"
    //
    // Tile math: SIZE_OF_GRIDS = 533.33333f; tileX = 32 - floor(Y / SIZE);
    // tileY = 32 - floor(X / SIZE) — matches MAP_RESOLUTION elsewhere in
    // the codebase. Output: one record per qualifying teleport row.
    //
    // Skip rules:
    //   - cross-map portals (source mapId != dest mapId)
    //   - distance < 5y (not worth an off-mesh edge)
    //   - distance > 1500y (likely mis-data; Recast handles long links
    //     poorly)
    //   - WorldSafeLoc with non-zero TransportSpawnId (moves with a
    //     transport — off-mesh assumes static geometry)
    //
    // The default output path is ./offmesh_areatrigger.txt in the
    // worldserver CWD; an optional argument overrides. RBAC re-uses the
    // STATS permission since this is a diagnostic-class command.
    static bool HandleMmapEmitOffmesh(ChatHandler* handler, char const* args)
    {
        std::string outPath = "offmesh_areatrigger.txt";
        if (args && *args)
        {
            // Trim leading whitespace and quotes.
            char const* p = args;
            while (*p == ' ' || *p == '\t' || *p == '"') ++p;
            outPath.assign(p);
            // Trim trailing quotes / whitespace.
            while (!outPath.empty()
                   && (outPath.back() == ' ' || outPath.back() == '\t'
                       || outPath.back() == '"'))
                outPath.pop_back();
        }

        FILE* f = fopen(outPath.c_str(), "wb");
        if (!f)
        {
            handler->PSendSysMessage("emitoffmesh: cannot open '%s' for write", outPath.c_str());
            return true;
        }

        // Header. Comment lines are accepted by the mmaps_generator
        // parser (it strips `//` to end-of-line via the same C++ idiom).
        std::time_t now_t = std::time(nullptr);
        char ts[64] = {0};
        std::strftime(ts, sizeof(ts) - 1, "%Y-%m-%d %H:%M:%S", std::gmtime(&now_t));
        std::fprintf(f, "// Auto-derived off-mesh connections from areatrigger_teleport.\n");
        std::fprintf(f, "// Emitted by `.mmap emitoffmesh` at %s UTC.\n", ts);
        std::fprintf(f, "// Format: <mapId> <tileX>,<tileY> (sx sy sz) (ex ey ez) <radius> <area> <flag>\n");

        constexpr float kSizeOfGrids = 533.33333f;
        constexpr float kMinDistance = 5.0f;
        constexpr float kMaxDistance = 1500.0f;
        constexpr float kAgentRadius = 2.5f;

        uint32 kept = 0;
        uint32 skipped_no_teleport = 0;
        uint32 skipped_cross_map = 0;
        uint32 skipped_distance = 0;
        uint32 skipped_transport = 0;

        for (AreaTriggerEntry const* at : sAreaTriggerStore)
        {
            if (!at) continue;

            WorldSafeLocsEntry const* dst = sObjectMgr->GetAreaTrigger(at->ID);
            if (!dst)
            {
                ++skipped_no_teleport;
                continue;
            }

            // Moving anchors — out of scope for static off-mesh.
            if (dst->TransportSpawnId != 0)
            {
                ++skipped_transport;
                continue;
            }

            if (uint32(at->ContinentID) != dst->Loc.GetMapId())
            {
                ++skipped_cross_map;
                continue;
            }

            const float sx = at->Pos.X, sy = at->Pos.Y, sz = at->Pos.Z;
            const float ex = dst->Loc.GetPositionX();
            const float ey = dst->Loc.GetPositionY();
            const float ez = dst->Loc.GetPositionZ();

            const float dx = ex - sx, dy = ey - sy, dz = ez - sz;
            const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist < kMinDistance || dist > kMaxDistance)
            {
                ++skipped_distance;
                continue;
            }

            // TC's tile math (matches `mmaps_generator` MapBuilder
            // tile_for_xy + `MMapDefines.h` MAP_RESOLUTION constants).
            const int32 tileX = int32(32 - std::floor(sy / kSizeOfGrids));
            const int32 tileY = int32(32 - std::floor(sx / kSizeOfGrids));

            std::fprintf(f,
                "%u %d,%d (%.3f %.3f %.3f) (%.3f %.3f %.3f) %.2f 1 1   // areatrigger_teleport at=%u\n",
                uint32(at->ContinentID), tileX, tileY,
                sx, sy, sz, ex, ey, ez, kAgentRadius,
                at->ID);
            ++kept;
        }

        std::fclose(f);

        handler->PSendSysMessage("emitoffmesh: wrote %u records to '%s'", kept, outPath.c_str());
        handler->PSendSysMessage("emitoffmesh: skipped: %u no-teleport, %u cross-map, %u out-of-range, %u transport-anchor",
            skipped_no_teleport, skipped_cross_map, skipped_distance, skipped_transport);
        return true;
    }

    // Compute the surface area of a single navmesh polygon from its detail
    // mesh triangles. Returns area in Detour coordinate-space square units
    // (equivalent to square yards in WoW's world scale).
    static float ComputePolyArea(dtMeshTile const* tile, dtPoly const* poly, int polyIdx)
    {
        dtPolyDetail const* pd = &tile->detailMeshes[polyIdx];
        float area = 0.f;
        for (int j = 0; j < pd->triCount; ++j)
        {
            unsigned char const* t = &tile->detailTris[(pd->triBase + j) * 4];
            float v0[3], v1[3], v2[3];
            for (int k = 0; k < 3; ++k)
            {
                float* dst = (k == 0) ? v0 : (k == 1) ? v1 : v2;
                if (t[k] < poly->vertCount)
                    dtVcopy(dst, &tile->verts[poly->verts[t[k]] * 3]);
                else
                    dtVcopy(dst, &tile->detailVerts[(pd->vertBase + t[k] - poly->vertCount) * 3]);
            }
            float e1[3], e2[3], cross[3];
            dtVsub(e1, v1, v0);
            dtVsub(e2, v2, v0);
            dtVcross(cross, e1, e2);
            area += 0.5f * std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
        }
        return area;
    }

    // .mmap audit
    //
    // Full navmesh quality audit for the player's current map. Reports:
    //   - Polygon counts by area type (ground/steep/water/magma/road/other)
    //   - Polygon area statistics (min/max/mean/tiny count)
    //   - Connected component analysis via BFS flood-fill (total components,
    //     largest component, road-connected components, isolated islands)
    //
    // Designed to detect regressions from navmesh generation parameter
    // changes (e.g. minRegionArea reduction that may introduce tiny
    // isolated polygons bots get stuck on).
    static bool HandleMmapAuditCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 mapId = PhasingHandler::GetTerrainMapId(
            player->GetPhaseShift(), player->GetMapId(),
            player->GetMap()->GetTerrain(),
            player->GetPositionX(), player->GetPositionY());

        dtNavMesh const* navmesh = MMAP::MMapManager::instance()->GetNavMesh(mapId, player->GetInstanceId());
        if (!navmesh)
        {
            handler->PSendSysMessage("NavMesh not loaded for current map.");
            return true;
        }

        uint32 const startMs = getMSTime();

        // ---- Pass 1: per-tile poly counts, area types, polygon area stats ----

        uint32 tileCount = 0;
        uint32 totalPolys = 0;

        // Area-type counters
        uint32 countGround = 0;
        uint32 countSteep = 0;
        uint32 countWater = 0;
        uint32 countMagma = 0;
        uint32 countRoad = 0;
        uint32 countOther = 0;

        // Polygon area statistics
        float minArea = std::numeric_limits<float>::max();
        float maxArea = 0.f;
        double sumArea = 0.0;
        uint32 tinyCount = 0; // polys < 1.0 sq yd

        // Pre-scan to count total polys for visited-set reservation.
        uint32 totalPolyEstimate = 0;
        int const maxTiles = navmesh->getMaxTiles();
        for (int i = 0; i < maxTiles; ++i)
        {
            dtMeshTile const* tile = navmesh->getTile(i);
            if (!tile || !tile->header)
                continue;
            totalPolyEstimate += static_cast<uint32>(tile->header->polyCount);
        }

        // Main per-tile scan.
        for (int i = 0; i < maxTiles; ++i)
        {
            dtMeshTile const* tile = navmesh->getTile(i);
            if (!tile || !tile->header)
                continue;

            ++tileCount;
            int const polyCount = tile->header->polyCount;
            totalPolys += static_cast<uint32>(polyCount);

            for (int p = 0; p < polyCount; ++p)
            {
                dtPoly const* poly = &tile->polys[p];

                // Skip off-mesh connection polys — they are virtual links,
                // not surface geometry, and have no detail mesh.
                if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                    continue;

                unsigned char area = poly->getArea();
                switch (area)
                {
                    case NAV_AREA_GROUND:       ++countGround; break;
                    case NAV_AREA_GROUND_STEEP:  ++countSteep;  break;
                    case NAV_AREA_WATER:         ++countWater;  break;
                    case NAV_AREA_MAGMA_SLIME:   ++countMagma;  break;
                    case NAV_AREA_ROAD:          ++countRoad;   break;
                    default:                           ++countOther;  break;
                }

                float polyArea = ComputePolyArea(tile, poly, p);
                if (polyArea < minArea) minArea = polyArea;
                if (polyArea > maxArea) maxArea = polyArea;
                sumArea += polyArea;
                if (polyArea < 1.0f) ++tinyCount;
            }
        }

        float meanArea = (totalPolys > 0) ? static_cast<float>(sumArea / totalPolys) : 0.f;
        if (totalPolys == 0)
            minArea = 0.f;

        // ---- Pass 2: connected component analysis via BFS flood-fill ----

        // We use an unordered_set<dtPolyRef> for the visited set since
        // dtPolyRef is 64-bit and the ref-space is sparse (salt+tile+poly
        // encoding). Reserve to avoid repeated rehashing.
        std::unordered_set<dtPolyRef> visited;
        visited.reserve(totalPolyEstimate);

        uint32 componentCount = 0;
        uint32 largestComponent = 0;
        uint32 roadConnectedComponents = 0;
        uint32 isolatedComponentCount = 0; // components with < 10 polys
        uint32 isolatedPolyCount = 0;      // total polys in isolated components

        std::queue<dtPolyRef> bfsQueue;

        for (int i = 0; i < maxTiles; ++i)
        {
            dtMeshTile const* tile = navmesh->getTile(i);
            if (!tile || !tile->header)
                continue;

            dtPolyRef const base = navmesh->getPolyRefBase(tile);
            int const polyCount = tile->header->polyCount;

            for (int p = 0; p < polyCount; ++p)
            {
                dtPolyRef const ref = base | static_cast<dtPolyRef>(p);

                if (visited.count(ref))
                    continue;

                // New connected component — BFS from this poly.
                ++componentCount;
                uint32 compSize = 0;
                bool compHasRoad = false;

                bfsQueue.push(ref);
                visited.insert(ref);

                while (!bfsQueue.empty())
                {
                    dtPolyRef const curRef = bfsQueue.front();
                    bfsQueue.pop();
                    ++compSize;

                    dtMeshTile const* curTile = nullptr;
                    dtPoly const* curPoly = nullptr;
                    if (dtStatusFailed(navmesh->getTileAndPolyByRef(curRef, &curTile, &curPoly)))
                        continue;

                    if (curPoly->getArea() == NAV_AREA_ROAD)
                        compHasRoad = true;

                    // Walk the link chain for this poly.
                    for (unsigned int linkIdx = curPoly->firstLink;
                         linkIdx != DT_NULL_LINK;
                         linkIdx = curTile->links[linkIdx].next)
                    {
                        dtPolyRef const neighborRef = curTile->links[linkIdx].ref;
                        if (neighborRef == 0)
                            continue;
                        if (visited.count(neighborRef))
                            continue;
                        visited.insert(neighborRef);
                        bfsQueue.push(neighborRef);
                    }
                }

                if (compSize > largestComponent)
                    largestComponent = compSize;
                if (compHasRoad)
                    ++roadConnectedComponents;
                if (compSize < 10)
                {
                    ++isolatedComponentCount;
                    isolatedPolyCount += compSize;
                }
            }
        }

        uint32 const elapsedMs = getMSTimeDiff(startMs, getMSTime());

        float isolatedPct = (totalPolys > 0)
            ? (static_cast<float>(isolatedPolyCount) / static_cast<float>(totalPolys)) * 100.f
            : 0.f;

        // ---- Output ----

        handler->PSendSysMessage("[NavAudit] Map %u: %u tiles, %u total polys (%u ms)",
            mapId, tileCount, totalPolys, elapsedMs);
        handler->PSendSysMessage("[NavAudit] Ground: %u | Steep: %u | Water: %u | Road: %u | Magma: %u | Other: %u",
            countGround, countSteep, countWater, countRoad, countMagma, countOther);
        handler->PSendSysMessage("[NavAudit] Poly area: min=%.2f max=%.2f mean=%.2f tiny(<1yd2)=%u",
            minArea, maxArea, meanArea, tinyCount);
        handler->PSendSysMessage("[NavAudit] Components: %u total | largest=%u | road-connected=%u | isolated(<10)=%u",
            componentCount, largestComponent, roadConnectedComponents, isolatedComponentCount);
        handler->PSendSysMessage("[NavAudit] Isolated island polys: %u (%.1f%% of total)",
            isolatedPolyCount, isolatedPct);

        return true;
    }
};

void AddSC_mmaps_commandscript()
{
    new mmaps_commandscript();
}
