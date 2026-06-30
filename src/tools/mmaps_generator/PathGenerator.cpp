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

#include "Banner.h"
#include "DB2FileLoader.h"
#include "DB2FileSystemSource.h"
#include "ExtractorDB2LoadInfo.h"
#include "IoContext.h"
#include "Locales.h"
#include "Log.h"
#include "MapBuilder.h"
#include "MMapDefines.h"
#include "RoadOverrides.h"
#include "Memory.h"
#include "PathCommon.h"
#include "Timer.h"
#include "Util.h"
#include "VMapManager.h"
#include <DetourCommon.h>
#include <DetourNavMeshQuery.h>
#include <boost/filesystem/operations.hpp>
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr char Readme[] =
{
#include "Info/readme.txt"
};

namespace
{
    std::unordered_map<uint32, uint8> _liquidTypes;
}

namespace MMAP
{
    std::unordered_map<uint32, MapEntry> sMapStore;

    namespace VMapFactory
    {
        std::unique_ptr<VMAP::VMapManager> CreateVMapManager(uint32 mapId)
        {
            std::unique_ptr<VMAP::VMapManager> vmgr = std::make_unique<VMAP::VMapManager>();

            do
            {
                int32 parentMapId = sMapStore[mapId].ParentMapID;

                vmgr->InitializeThreadUnsafe(mapId, parentMapId);
                if (parentMapId < 0)
                    break;

                mapId = parentMapId;
            } while (true);

            vmgr->GetLiquidFlagsPtr = [](uint32 liquidId) -> uint32
            {
                auto itr = _liquidTypes.find(liquidId);
                return itr != _liquidTypes.end() ? (1 << itr->second) : 0;
            };
            vmgr->LoadPathOnlyModels = true;
            return vmgr;
        }
    }
}

void SetupLogging(Trinity::Asio::IoContext* ioContext)
{
    Log* log = sLog;

    log->SetAsynchronous(ioContext);

    log->CreateAppenderFromConfigLine("Appender.Console", "1,2,0");         // APPENDER_CONSOLE | LOG_LEVEL_DEBUG | APPENDER_FLAGS_NONE
    log->CreateLoggerFromConfigLine("Logger.root", "2,Console");            // LOG_LEVEL_DEBUG | Console appender
    log->CreateLoggerFromConfigLine("Logger.tool.mmapgen", "2,Console");    // LOG_LEVEL_DEBUG | Console appender
    log->CreateLoggerFromConfigLine("Logger.maps", "3,Console");            // LOG_LEVEL_DEBUG | Console appender
    log->CreateLoggerFromConfigLine("Logger.maps.mmapgen", "2,Console");    // LOG_LEVEL_DEBUG | Console appender
}

bool checkDirectories(boost::filesystem::path const& inputDirectory, boost::filesystem::path const& outputDirectory,
    bool debugOutput, std::vector<std::string>& dbcLocales)
{
    if (MMAP::getDirContents(dbcLocales, inputDirectory / "dbc", boost::filesystem::directory_file) == MMAP::LISTFILE_DIRECTORY_NOT_FOUND || dbcLocales.empty())
    {
        TC_LOG_ERROR("tool.mmapgen", "'dbc' directory is empty or does not exist");
        return false;
    }

    std::vector<std::string> dirFiles;

    if (MMAP::getDirContents(dirFiles, inputDirectory / "maps") == MMAP::LISTFILE_DIRECTORY_NOT_FOUND || dirFiles.empty())
    {
        TC_LOG_ERROR("tool.mmapgen", "'maps' directory is empty or does not exist");
        return false;
    }

    dirFiles.clear();
    if (MMAP::getDirContents(dirFiles, inputDirectory / "vmaps" / "0000", boost::filesystem::regular_file, "*.vmtree") == MMAP::LISTFILE_DIRECTORY_NOT_FOUND || dirFiles.empty())
    {
        TC_LOG_ERROR("tool.mmapgen", "'vmaps' directory is empty or does not exist");
        return false;
    }

    boost::system::error_code ec;
    if (!boost::filesystem::create_directories(outputDirectory / "mmaps", ec) && ec)
    {
        TC_LOG_ERROR("tool.mmapgen", "'mmaps' directory does not exist and failed to create it");
        return false;
    }

    if (debugOutput)
    {
        if (!boost::filesystem::create_directories(outputDirectory / "meshes", ec) && ec)
        {
            TC_LOG_ERROR("tool.mmapgen", "'meshes' directory does not exist and failed to create it (no place to put debugOutput files)");
            return false;
        }
    }

    return true;
}

int finish(char const* message, int returnValue)
{
    TC_LOG_FATAL("tool.mmapgen.commandline", "{}", message);
    getchar(); // Wait for user input
    return returnValue;
}

// Audit-only budget knobs (set from CLI in handleArgs, read in RunNavmeshAudit).
// Defaults mirror the LIVE in-game pathfinder so a bare --audit reports what the
// bot actually experiences: 1024-node A* pool (MMapManager) + 74-poly path cap
// (MAX_PATH_LENGTH). Sweeping them quantifies how many WEDGE objectives a bigger
// budget would convert to OK — see docs/MMAP_SWEEP_FINDINGS.md.
static int s_auditGameNodes = 1024;   // dtNavMeshQuery node pool for the "game" query
static int s_auditPathCap   = 74;     // per-hop accepted path length (MAX_PATH_LENGTH)

bool handleArgs(int argc, char** argv,
               int& mapnum,
               int& tileX,
               int& tileY,
               Optional<float>& maxAngle,
               Optional<float>& maxAngleNotSteep,
               bool& skipLiquid,
               bool& skipContinents,
               bool& skipJunkMaps,
               bool& skipBattlegrounds,
               bool& debugOutput,
               bool& silent,
               bool& bigBaseUnit,
               char const*& offMeshInputPath,
               char const*& file,
               unsigned int& threads,
               boost::filesystem::path& inputDirectory,
               boost::filesystem::path& outputDirectory)
{
    char* param = nullptr;
    [[maybe_unused]] bool allowDebug = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--maxAngle") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            float maxangle = atof(param);
            if (maxangle <= 90.f && maxangle >= 0.f)
                maxAngle = maxangle;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--maxAngle', using default");
        }
        else if (strcmp(argv[i], "--maxAngleNotSteep") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            float maxangle = atof(param);
            if (maxangle <= 90.f && maxangle >= 0.f)
                maxAngleNotSteep = maxangle;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--maxAngleNotSteep', using default");
        }
        // ---- map-1 optimization sweep knobs (set g_mmapGenTuning directly) ----
        else if (strcmp(argv[i], "--partition") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;
            if (strcmp(param, "watershed") == 0)
                g_mmapGenTuning.partition = MmapPartitionMethod::Watershed;
            else if (strcmp(param, "monotone") == 0)
                g_mmapGenTuning.partition = MmapPartitionMethod::Monotone;
            else if (strcmp(param, "layers") == 0 || strcmp(param, "layered") == 0)
                g_mmapGenTuning.partition = MmapPartitionMethod::Layers;
            else if (strcmp(param, "auto") == 0)
                g_mmapGenTuning.partition = MmapPartitionMethod::Auto;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid '--partition' (want watershed|monotone|layers|auto), using layers");
        }
        else if (strcmp(argv[i], "--minRegionArea") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.minRegionArea = atoi(param);
        }
        else if (strcmp(argv[i], "--mergeRegionArea") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.mergeRegionArea = atoi(param);
        }
        else if (strcmp(argv[i], "--maxSimplError") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.maxSimplificationError = float(atof(param));
        }
        else if (strcmp(argv[i], "--maxEdgeLen") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.maxEdgeLen = atoi(param);
        }
        else if (strcmp(argv[i], "--walkableClimb") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.walkableClimb = atoi(param);
        }
        else if (strcmp(argv[i], "--walkableRadius") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.walkableRadius = atoi(param);
        }
        else if (strcmp(argv[i], "--cellSize") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.cellSize = float(atof(param));
        }
        else if (strcmp(argv[i], "--detailSampleDist") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.detailSampleDist = float(atof(param));
        }
        else if (strcmp(argv[i], "--detailSampleMaxError") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            g_mmapGenTuning.detailSampleMaxError = float(atof(param));
        }
        else if (strcmp(argv[i], "--fineCells") == 0)
        {
            g_mmapGenTuning.fineCells = true;
        }
        else if (strcmp(argv[i], "--gameNodes") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            int v = atoi(param);
            if (v >= 16 && v <= 1048576) s_auditGameNodes = v;
        }
        else if (strcmp(argv[i], "--pathCap") == 0)
        {
            param = argv[++i];
            if (!param) return false;
            int v = atoi(param);
            if (v >= 4 && v <= 1000000) s_auditPathCap = v;
        }
        else if (strcmp(argv[i], "--threads") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;
            threads = static_cast<unsigned int>(std::max(0, atoi(param)));
        }
        else if (strcmp(argv[i], "--file") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;
            file = param;
        }
        else if (strcmp(argv[i], "--tile") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            char* stileX = strtok(param, ",");
            char* stileY = strtok(nullptr, ",");
            int tilex = atoi(stileX);
            int tiley = atoi(stileY);

            if ((tilex > 0 && tilex < 64) || (tilex == 0 && strcmp(stileX, "0") == 0))
                tileX = tilex;
            if ((tiley > 0 && tiley < 64) || (tiley == 0 && strcmp(stileY, "0") == 0))
                tileY = tiley;

            if (tileX < 0 || tileY < 0)
            {
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid tile coords.");
                return false;
            }
        }
        else if (strcmp(argv[i], "--skipLiquid") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            if (strcmp(param, "true") == 0)
                skipLiquid = true;
            else if (strcmp(param, "false") == 0)
                skipLiquid = false;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--skipLiquid', using default");
        }
        else if (strcmp(argv[i], "--skipContinents") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            if (strcmp(param, "true") == 0)
                skipContinents = true;
            else if (strcmp(param, "false") == 0)
                skipContinents = false;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--skipContinents', using default");
        }
        else if (strcmp(argv[i], "--skipJunkMaps") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            if (strcmp(param, "true") == 0)
                skipJunkMaps = true;
            else if (strcmp(param, "false") == 0)
                skipJunkMaps = false;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--skipJunkMaps', using default");
        }
        else if (strcmp(argv[i], "--skipBattlegrounds") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            if (strcmp(param, "true") == 0)
                skipBattlegrounds = true;
            else if (strcmp(param, "false") == 0)
                skipBattlegrounds = false;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--skipBattlegrounds', using default");
        }
        else if (strcmp(argv[i], "--debugOutput") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            if (strcmp(param, "true") == 0)
                debugOutput = true;
            else if (strcmp(param, "false") == 0)
                debugOutput = false;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--debugOutput', using default true");
        }
        else if (strcmp(argv[i], "--silent") == 0)
        {
            silent = true;
        }
        else if (strcmp(argv[i], "--bigBaseUnit") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            if (strcmp(param, "true") == 0)
                bigBaseUnit = true;
            else if (strcmp(param, "false") == 0)
                bigBaseUnit = false;
            else
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid option for '--bigBaseUnit', using default false");
        }
        else if (strcmp(argv[i], "--offMeshInput") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            offMeshInputPath = param;
        }
        else if (strcmp(argv[i], "--input") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            inputDirectory = param;
        }
        else if (strcmp(argv[i], "--output") == 0)
        {
            param = argv[++i];
            if (!param)
                return false;

            outputDirectory = param;
        }
        else if (strcmp(argv[i], "--allowDebug") == 0)
        {
            allowDebug = true;
        }
        else if (strcmp(argv[i], "--audit") == 0)
        {
            // Handled in main() before handleArgs; skip here.
        }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-?"))
        {
            TC_LOG_INFO("tool.mmapgen", "{}", Readme);
            silent = true;
            return false;
        }
        else
        {
            int map = atoi(argv[i]);
            if (map > 0 || (map == 0 && (strcmp(argv[i], "0") == 0)))
                mapnum = map;
            else
            {
                TC_LOG_ERROR("tool.mmapgen.commandline", "invalid map id {}", map);
                return false;
            }
        }
    }

#if !defined(NDEBUG)
    if (!allowDebug)
    {
        finish("Build mmaps_generator in RelWithDebInfo or Release mode or it will take hours to complete!!!\nUse '--allowDebug' argument if you really want to run this tool in Debug.", -2);
        silent = true;
        return false;
    }
#endif

    return true;
}

std::unordered_map<uint32, uint8> LoadLiquid(boost::filesystem::path const& inputDirectory, std::string const& locale, bool silent, int32 errorExitCode)
{
    DB2FileLoader liquidDb2;
    std::unordered_map<uint32, uint8> liquidData;
    DB2FileSystemSource liquidTypeSource((inputDirectory / "dbc" / locale / "LiquidType.db2").string());
    try
    {
        liquidDb2.Load(&liquidTypeSource, &LiquidTypeLoadInfo::Instance);
        for (uint32 x = 0; x < liquidDb2.GetRecordCount(); ++x)
        {
            DB2Record record = liquidDb2.GetRecord(x);
            if (!record)
                continue;

            liquidData[record.GetId()] = record.GetUInt8("SoundBank");
        }
    }
    catch (std::exception const& e)
    {
        if (silent)
            exit(errorExitCode);

        exit(finish(e.what(), errorExitCode));
    }

    return liquidData;
}

void LoadMap(boost::filesystem::path const& inputDirectory, std::string const& locale, bool silent, int32 errorExitCode)
{
    DB2FileLoader mapDb2;
    DB2FileSystemSource mapSource((inputDirectory / "dbc" / locale / "Map.db2").string());
    try
    {
        mapDb2.Load(&mapSource, &MapLoadInfo::Instance);
        for (uint32 x = 0; x < mapDb2.GetRecordCount(); ++x)
        {
            DB2Record record = mapDb2.GetRecord(x);
            if (!record)
                continue;

            int16 parentMapId = int16(record.GetUInt16("ParentMapID"));
            if (parentMapId < 0)
                parentMapId = int16(record.GetUInt16("CosmeticParentMapID"));

            MMAP::MapEntry& map = MMAP::sMapStore[record.GetId()];
            map.MapType = record.GetUInt8("MapType");
            map.InstanceType = record.GetUInt8("InstanceType");
            map.ParentMapID = parentMapId;
            map.Flags = record.GetInt32("Flags1");
        }
    }
    catch (std::exception const& e)
    {
        if (silent)
            exit(errorExitCode);

        exit(finish(e.what(), errorExitCode));
    }
}

void RunNavmeshAudit(boost::filesystem::path const& mmapsDir, int mapId)
{
    // Load the .mmap header to get dtNavMeshParams.
    char headerFile[256];
    snprintf(headerFile, sizeof(headerFile), "%s/%04d.mmap", mmapsDir.string().c_str(), mapId);

    FILE* fp = fopen(headerFile, "rb");
    if (!fp)
    {
        printf("[audit] Cannot open %s\n", headerFile);
        return;
    }
    MmapNavMeshHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1 || header.mmapMagic != MMAP_MAGIC)
    {
        printf("[audit] Invalid mmap header for map %d\n", mapId);
        fclose(fp);
        return;
    }
    fclose(fp);

    dtNavMesh* navmesh = dtAllocNavMesh();
    if (!navmesh || dtStatusFailed(navmesh->init(&header.params)))
    {
        printf("[audit] Failed to init dtNavMesh for map %d\n", mapId);
        if (navmesh) dtFreeNavMesh(navmesh);
        return;
    }

    // Load all .mmtile files for this map.
    int tilesLoaded = 0;
    for (int tx = 0; tx < 64; ++tx)
    {
        for (int ty = 0; ty < 64; ++ty)
        {
            char tilePath[256];
            snprintf(tilePath, sizeof(tilePath), "%s/%04d_%02d_%02d.mmtile",
                     mmapsDir.string().c_str(), mapId, ty, tx);
            FILE* tf = fopen(tilePath, "rb");
            if (!tf) continue;
            MmapTileHeader tileHeader;
            if (fread(&tileHeader, sizeof(tileHeader), 1, tf) != 1 ||
                tileHeader.mmapMagic != MMAP_MAGIC ||
                tileHeader.mmapVersion != MMAP_VERSION)
            {
                fclose(tf);
                continue;
            }
            unsigned char* data = static_cast<unsigned char*>(dtAlloc(tileHeader.size, DT_ALLOC_PERM));
            if (!data) { fclose(tf); continue; }
            if (fread(data, tileHeader.size, 1, tf) != 1) { dtFree(data); fclose(tf); continue; }
            fclose(tf);
            dtTileRef tileRef = 0;
            if (dtStatusFailed(navmesh->addTile(data, tileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef)))
            {
                dtFree(data);
                continue;
            }
            ++tilesLoaded;
        }
    }
    printf("[audit] Map %d: loaded %d tiles\n", mapId, tilesLoaded);
    if (tilesLoaded == 0) { dtFreeNavMesh(navmesh); return; }

    dtNavMesh const* mesh = navmesh;

    // Pass 1: poly counts by area type + area stats.
    uint32 totalPolys = 0, surfacePolys = 0;
    uint32 countGround = 0, countSteep = 0, countWater = 0, countMagma = 0, countRoad = 0, countOther = 0;
    float minArea = 1e30f, maxArea = 0.f;
    double sumArea = 0.0;
    uint32 tinyCount = 0;

    int const maxTiles = mesh->getMaxTiles();
    for (int i = 0; i < maxTiles; ++i)
    {
        dtMeshTile const* tile = mesh->getTile(i);
        if (!tile || !tile->header) continue;
        for (int p = 0; p < tile->header->polyCount; ++p)
        {
            dtPoly const* poly = &tile->polys[p];
            ++totalPolys;
            if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
            if (poly->getArea() == NAV_AREA_EMPTY) { ++countOther; continue; }
            ++surfacePolys;
            switch (poly->getArea())
            {
                case NAV_AREA_GROUND:      ++countGround; break;
                case NAV_AREA_GROUND_STEEP: ++countSteep;  break;
                case NAV_AREA_WATER:        ++countWater;  break;
                case NAV_AREA_MAGMA_SLIME:  ++countMagma;  break;
                case NAV_AREA_ROAD:         ++countRoad;   break;
                default:                    ++countOther;  break;
            }
            // Compute poly area from detail mesh.
            dtPolyDetail const* pd = &tile->detailMeshes[p];
            float area = 0.f;
            for (int j = 0; j < pd->triCount; ++j)
            {
                unsigned char const* t = &tile->detailTris[(pd->triBase + j) * 4];
                float v[3][3];
                for (int k = 0; k < 3; ++k)
                {
                    if (t[k] < poly->vertCount)
                        dtVcopy(v[k], &tile->verts[poly->verts[t[k]] * 3]);
                    else
                        dtVcopy(v[k], &tile->detailVerts[(pd->vertBase + t[k] - poly->vertCount) * 3]);
                }
                float e1[3], e2[3], cross[3];
                dtVsub(e1, v[1], v[0]); dtVsub(e2, v[2], v[0]);
                dtVcross(cross, e1, e2);
                area += 0.5f * std::sqrt(cross[0]*cross[0] + cross[1]*cross[1] + cross[2]*cross[2]);
            }
            if (area < minArea) minArea = area;
            if (area > maxArea) maxArea = area;
            sumArea += area;
            if (area < 1.0f) ++tinyCount;
        }
    }
    if (surfacePolys == 0) minArea = 0.f;
    float meanArea = surfacePolys > 0 ? static_cast<float>(sumArea / surfacePolys) : 0.f;

    // Pass 2: BFS connectivity analysis.
    std::unordered_set<dtPolyRef> visited;
    visited.reserve(totalPolys);
    uint32 componentCount = 0, largestComp = 0, roadComps = 0, isolatedComps = 0, isolatedPolys = 0;
    uint32 isoGround = 0, isoWater = 0, isoRoad = 0, isoOtherArea = 0;
    std::queue<dtPolyRef> bfs;

    // Per-component geometry, so we can name and locate the SIGNIFICANT
    // disconnected islands (cave / barrow / den interiors) — the ones too
    // big to be dismissed as the <10-poly noise tracked above, yet not the
    // mainland. These are the off-mesh-connection candidates: a meshed but
    // unreachable room. WoW coords from a tile vert: x=vert[2] y=vert[0]
    // z=vert[1] (inverse of the probe's TC=(detour.z,detour.x,detour.y)).
    struct IslandInfo
    {
        uint32 size;
        double cx, cy, cz;                 // centroid accumulator (then mean)
        float minx, miny, minz, maxx, maxy, maxz;
        uint32 ground, water, road, other;
    };
    std::vector<IslandInfo> islands;       // one per component (any size)
    constexpr uint32 kIslandMinPolys = 15; // a real room, not a sliver

    // Component labeling, so a checkpoint (e.g. a quest objective coord) can
    // be classified: which component is it in, and is that the reachable
    // mainland? polyComp maps a surface poly -> its component index;
    // compSizes[idx] is that component's poly count. ~60-100MB for a
    // continent — fine for an offline tool.
    std::unordered_map<dtPolyRef, uint32> polyComp;
    polyComp.reserve(totalPolys);
    std::vector<uint32> compSizes;

    for (int i = 0; i < maxTiles; ++i)
    {
        dtMeshTile const* tile = mesh->getTile(i);
        if (!tile || !tile->header) continue;
        dtPolyRef base = mesh->getPolyRefBase(tile);
        for (int p = 0; p < tile->header->polyCount; ++p)
        {
            if (tile->polys[p].getArea() == NAV_AREA_EMPTY) continue;
            dtPolyRef ref = base | static_cast<dtPolyRef>(p);
            if (visited.count(ref)) continue;
            ++componentCount;
            uint32 sz = 0;
            bool hasRoad = false;
            uint32 cGround = 0, cWater = 0, cRoad = 0, cOtherA = 0;
            IslandInfo isl{};
            isl.minx = isl.miny = isl.minz = 1e30f;
            isl.maxx = isl.maxy = isl.maxz = -1e30f;
            uint32 compId = static_cast<uint32>(compSizes.size());
            bfs.push(ref); visited.insert(ref); polyComp[ref] = compId;
            while (!bfs.empty())
            {
                dtPolyRef cur = bfs.front(); bfs.pop(); ++sz;
                dtMeshTile const* ct = nullptr; dtPoly const* cp = nullptr;
                if (dtStatusFailed(mesh->getTileAndPolyByRef(cur, &ct, &cp))) continue;
                unsigned char ca = cp->getArea();
                if (ca == NAV_AREA_ROAD) { hasRoad = true; ++cRoad; }
                else if (ca == NAV_AREA_GROUND || ca == NAV_AREA_GROUND_STEEP) ++cGround;
                else if (ca == NAV_AREA_WATER) ++cWater;
                else ++cOtherA;
                // Accumulate this poly's centroid + bbox in WoW coords.
                if (cp->getType() != DT_POLYTYPE_OFFMESH_CONNECTION && cp->vertCount > 0)
                {
                    double px = 0, py = 0, pz = 0;
                    for (int vi = 0; vi < cp->vertCount; ++vi)
                    {
                        float const* v = &ct->verts[cp->verts[vi] * 3];
                        float wx = v[2], wy = v[0], wz = v[1];
                        px += wx; py += wy; pz += wz;
                        if (wx < isl.minx) isl.minx = wx; if (wx > isl.maxx) isl.maxx = wx;
                        if (wy < isl.miny) isl.miny = wy; if (wy > isl.maxy) isl.maxy = wy;
                        if (wz < isl.minz) isl.minz = wz; if (wz > isl.maxz) isl.maxz = wz;
                    }
                    px /= cp->vertCount; py /= cp->vertCount; pz /= cp->vertCount;
                    isl.cx += px; isl.cy += py; isl.cz += pz;
                }
                for (unsigned int li = cp->firstLink; li != DT_NULL_LINK; li = ct->links[li].next)
                {
                    dtPolyRef nb = ct->links[li].ref;
                    if (!nb || visited.count(nb)) continue;
                    dtMeshTile const* nt = nullptr; dtPoly const* np = nullptr;
                    if (dtStatusFailed(mesh->getTileAndPolyByRef(nb, &nt, &np))) continue;
                    if (np->getArea() == NAV_AREA_EMPTY) continue;
                    visited.insert(nb); bfs.push(nb); polyComp[nb] = compId;
                }
            }
            compSizes.push_back(sz);
            if (sz > largestComp) largestComp = sz;
            if (hasRoad) ++roadComps;
            if (sz < 10)
            {
                ++isolatedComps; isolatedPolys += sz;
                isoGround += cGround; isoWater += cWater;
                isoRoad += cRoad; isoOtherArea += cOtherA;
            }
            if (sz >= kIslandMinPolys)
            {
                isl.size = sz;
                isl.cx /= sz; isl.cy /= sz; isl.cz /= sz;
                isl.ground = cGround; isl.water = cWater; isl.road = cRoad; isl.other = cOtherA;
                islands.push_back(isl);
            }
        }
    }

    float isoPct = surfacePolys > 0 ? (float(isolatedPolys) / float(surfacePolys)) * 100.f : 0.f;

    printf("\n=== NavMesh Audit: Map %d ===\n", mapId);
    printf("Tiles: %d | Total polys: %u (surface: %u)\n", tilesLoaded, totalPolys, surfacePolys);
    printf("Ground: %u | Steep: %u | Water: %u | Road: %u | Magma: %u | Other: %u\n",
           countGround, countSteep, countWater, countRoad, countMagma, countOther);
    printf("Poly area: min=%.2f max=%.2f mean=%.2f tiny(<1yd2)=%u\n",
           minArea, maxArea, meanArea, tinyCount);
    printf("Components: %u total | largest=%u | road-connected=%u | isolated(<10)=%u\n",
           componentCount, largestComp, roadComps, isolatedComps);
    printf("Isolated island polys: %u (%.1f%% of total)\n", isolatedPolys, isoPct);
    printf("  Breakdown: ground=%u water=%u road=%u other=%u\n",
           isoGround, isoWater, isoRoad, isoOtherArea);

    // Significant disconnected islands (off-mesh-connection candidates).
    // These are meshed-but-unreachable rooms — cave / barrow / den interiors
    // and the like. Sorted largest-first. Cross-reference each centroid/bbox
    // against quest_poi_points + creature/gameobject spawns: an island that
    // contains a quest objective needs an offmesh.txt bridge (see the
    // navmesh-offmesh-bridge workflow). The single largest component is the
    // mainland and is suppressed here.
    {
        size_t mainIdx = islands.size();
        uint32 mainSz = 0;
        for (size_t i = 0; i < islands.size(); ++i)
            if (islands[i].size > mainSz) { mainSz = islands[i].size; mainIdx = i; }

        std::vector<size_t> order;
        for (size_t i = 0; i < islands.size(); ++i)
            if (i != mainIdx) order.push_back(i);
        std::sort(order.begin(), order.end(),
                  [&](size_t a, size_t b) { return islands[a].size > islands[b].size; });

        printf("Disconnected islands >= %u polys (excl. mainland of %u polys): %zu\n",
               kIslandMinPolys, mainSz, order.size());
        printf("  (TSV: polys centroidX centroidY centroidZ  bbox[minX minY minZ maxX maxY maxZ]  g/w/r/o)\n");
        for (size_t k = 0; k < order.size(); ++k)
        {
            IslandInfo const& I = islands[order[k]];
            printf("  ISLAND\t%u\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%u/%u/%u/%u\n",
                   I.size, I.cx, I.cy, I.cz,
                   I.minx, I.miny, I.minz, I.maxx, I.maxy, I.maxz,
                   I.ground, I.water, I.road, I.other);
        }
    }

    // Checkpoint classification. Auto-loads <output>/audit_checkpoints.tsv when
    // present. Lines (WoW coords; '#' ignored):
    //   REF <mapId> <x> <y> <z>            -- a known-REACHABLE reference per map
    //   <mapId> <x> <y> <z> [label]        -- an objective to classify
    // For each objective we snap to the nearest poly, then run an UNCAPPED
    // Detour findPath from the map's REF poly (the tool is free of the game's
    // 74-poly limit). The verdict targets the actual failure mode — a route
    // that EXISTS but is longer than the in-game MAX_PATH_LENGTH (74), which
    // makes the live pathfinder return a partial that dead-ends and the bot
    // oscillate (the Ban'ethil den case). Just being in a different component
    // from the mainland is NOT a problem (Teldrassil is a legit boat-only
    // landmass), so component size alone is reported, not used as the verdict.
    //   NO_MESH    -> objective not on any poly (true geometry hole)
    //   UNREACHABLE-> no path from REF (different component / sealed)
    //   WEDGE>74   -> reachable but route > 74 polys: bot will partial-wedge;
    //                 add an offmesh.txt bridge to bring it under the cap
    //   OK<=74     -> reachable within the game cap
    // Without a REF for the map, falls back to reporting component size only.
    {
        boost::filesystem::path cpPath = mmapsDir.parent_path() / "audit_checkpoints.tsv";
        boost::system::error_code ec;
        if (boost::filesystem::is_regular_file(cpPath, ec))
        {
            // Two queries: queryGame mirrors the live server's A* node pool
            // (MMapManager inits dtNavMeshQuery with 1024 nodes) so its result
            // is what the BOT actually experiences; queryFull (65535 nodes,
            // Detour's practical max) tells us whether a route physically
            // exists at all. The pair separates the two failure modes:
            //   game fails + full succeeds  -> WEDGE  (connected but the route
            //       needs >1024 A* expansions / >74 path polys: the bot gets a
            //       partial that dead-ends and oscillates -> offmesh.txt bridge)
            //   both fail                   -> ISOLATED (truly disconnected /
            //       sealed: bridge to the nearest reachable mesh, or data fix)
            //   game succeeds               -> OK
            dtNavMeshQuery* queryGame = dtAllocNavMeshQuery();
            dtNavMeshQuery* queryFull = dtAllocNavMeshQuery();
            bool okG = queryGame && dtStatusSucceed(queryGame->init(navmesh, s_auditGameNodes));
            bool okF = queryFull && dtStatusSucceed(queryFull->init(navmesh, 65535));
            if (okG && okF)
            {
                dtQueryFilter filter;
                filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_WATER | NAV_ROAD);
                filter.setExcludeFlags(0);
                float const ext[3] = { 5.0f, 60.0f, 5.0f };

                // Pass A: find this map's REF poly (a known-reachable anchor).
                dtPolyRef refRef = 0; float refSnap[3] = { 0, 0, 0 };
                {
                    FILE* cf = fopen(cpPath.string().c_str(), "rb");
                    char line[512];
                    while (cf && fgets(line, sizeof(line), cf))
                    {
                        int rm = -1; float rx = 0, ry = 0, rz = 0;
                        if (sscanf(line, "REF %d %f %f %f", &rm, &rx, &ry, &rz) == 4 && rm == mapId)
                        {
                            float p[3] = { ry, rz, rx };
                            queryFull->findNearestPoly(p, ext, &filter, &refRef, refSnap);
                            break;
                        }
                    }
                    if (cf) fclose(cf);
                }

                FILE* cf = fopen(cpPath.string().c_str(), "rb");
                printf("Checkpoint classification (from %s), REF poly=%llu (game=%d nodes/pathCap=%d, full=65535):\n",
                       cpPath.string().c_str(), static_cast<unsigned long long>(refRef), s_auditGameNodes, s_auditPathCap);
                printf("  (TSV: VERDICT label  reqX reqY reqZ  snapX snapY snapZ  gamePolys compPolys)\n");
                char line[512];
                uint32 nOk = 0, nWedge = 0, nIso = 0, nNoMesh = 0, nNoRef = 0;
                constexpr int kBuf = 16384;
                static dtPolyRef pathBuf[kBuf];   // static: keep off the stack
                while (cf && fgets(line, sizeof(line), cf))
                {
                    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
                    if (strncmp(line, "REF", 3) == 0) continue;
                    int qm = -1; float qx = 0, qy = 0, qz = 0; char label[128] = "";
                    float rfx = 0, rfy = 0, rfz = 0;
                    // Optional per-line REF after the label (e.g. the quest
                    // giver's spawn): "<map> <x> <y> <z> <label> <refX> <refY> <refZ>".
                    // A per-line REF wins over the map REF, so one audit run can
                    // sweep thousands of objectives each anchored to its own
                    // reachable source.
                    int got = sscanf(line, "%d %f %f %f %127s %f %f %f",
                                     &qm, &qx, &qy, &qz, label, &rfx, &rfy, &rfz);
                    if (got < 4 || qm != mapId) continue;

                    dtPolyRef srcRef = refRef; float srcSnap[3] = { refSnap[0], refSnap[1], refSnap[2] };
                    if (got >= 8)
                    {
                        float rp[3] = { rfy, rfz, rfx };
                        srcRef = 0;
                        queryFull->findNearestPoly(rp, ext, &filter, &srcRef, srcSnap);
                    }

                    float pt[3] = { qy, qz, qx };          // WoW -> Detour order
                    dtPolyRef ref = 0; float snap[3] = { 0, 0, 0 };
                    queryFull->findNearestPoly(pt, ext, &filter, &ref, snap);

                    // Component identity (BFS-exact) decides WEDGE vs ISOLATED —
                    // NOT a full findPath, whose success is A*-node-budget- and
                    // start-position-dependent (the same reachable poly reads
                    // "found" from one REF and "fail" from another at 65535
                    // nodes). polyComp is the ground truth for connectivity.
                    auto compOf = [&](dtPolyRef r) -> uint32 {
                        auto it = polyComp.find(r);
                        return (it != polyComp.end()) ? it->second : 0xFFFFFFFFu;
                    };
                    uint32 objComp = ref ? compOf(ref) : 0xFFFFFFFFu;
                    uint32 refComp = srcRef ? compOf(srcRef) : 0xFFFFFFFFu;
                    uint32 compPolys = (objComp != 0xFFFFFFFFu && objComp < compSizes.size())
                                     ? compSizes[objComp] : 0;

                    char const* verdict;
                    int gamePolys = -1, fullPolys = -1;
                    float wedge[3] = { 0, 0, 0 }; bool haveWedge = false;
                    if (!ref) { verdict = "NO_MESH"; ++nNoMesh; }
                    else if (!srcRef) { verdict = "NO_REF"; ++nNoRef; }
                    else if (objComp == 0xFFFFFFFFu || objComp != refComp)
                    {
                        // Different navmesh component from the reachable REF:
                        // no walkable route at all (boat-only landmass, sealed
                        // interior, or off-the-mesh REF).
                        verdict = "ISOLATED"; ++nIso;
                    }
                    else
                    {
                        // Same component => a route exists; but the LIVE bot
                        // pathfinds in 74-poly / 1024-node hops and walks to each
                        // partial endpoint, then re-paths. Simulate that chain:
                        // each hop advances along the A* frontier toward the goal
                        // (A* itself rounds obstacles within budget, so distance
                        // may transiently grow on a detour — we must NOT treat
                        // that as a stall). A TRUE wedge is a topological no-
                        // advance: the partial can't leave the current poly, or
                        // it revisits a poly already walked (cycle).
                        dtPolyRef cur = srcRef;
                        float curPos[3] = { srcSnap[0], srcSnap[1], srcSnap[2] };
                        std::unordered_set<dtPolyRef> walked;
                        walked.insert(cur);
                        bool reached = false; int hops = 0;
                        for (; hops < 200; ++hops)
                        {
                            int ng = 0;
                            queryGame->findPath(cur, ref, curPos, snap, &filter, pathBuf, &ng, kBuf);
                            if (ng <= 0) break;
                            // Live bot truncates each path to MAX_PATH_LENGTH and
                            // walks to the partial endpoint, then re-paths. Model
                            // that cap so the wedge verdict matches what the bot
                            // experiences (and so --pathCap can sweep it).
                            int adv = (ng > s_auditPathCap) ? s_auditPathCap : ng;
                            dtPolyRef last = pathBuf[adv - 1];
                            if (last == ref) { reached = true; break; }
                            if (last == cur || walked.count(last)) break; // no-advance / cycle => stall
                            float endPos[3];
                            if (dtStatusFailed(queryGame->closestPointOnPoly(last, snap, endPos, nullptr)))
                                break;
                            walked.insert(last); cur = last; dtVcopy(curPos, endPos);
                        }
                        if (!reached) { dtVcopy(wedge, curPos); haveWedge = true; }
                        float stallD2 = dtVdistSqr(curPos, snap);
                        (void)stallD2;
                        if (reached) { verdict = "OK"; gamePolys = hops; ++nOk; }
                        else
                        {
                            verdict = "WEDGE"; ++nWedge;
                            if (!haveWedge) { dtVcopy(wedge, curPos); haveWedge = true; }
                            gamePolys = hops;
                        }
                        if (!reached && haveWedge)
                            printf("    wedge-at\t%.1f\t%.1f\t%.1f\t(incremental pathing stalled %.0fy from objective, %d hops)\n",
                                   wedge[2], wedge[0], wedge[1], std::sqrt(stallD2), hops);
                    }
                    printf("  %s\t%s\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\t%u\n",
                           verdict, label[0] ? label : "-", qx, qy, qz,
                           snap[2], snap[0], snap[1], gamePolys, compPolys);
                    (void)fullPolys;
                }
                if (cf) fclose(cf);
                printf("Checkpoint summary: OK=%u WEDGE=%u ISOLATED=%u NO_MESH=%u NO_REF=%u\n",
                       nOk, nWedge, nIso, nNoMesh, nNoRef);
            }
            if (queryGame) dtFreeNavMeshQuery(queryGame);
            if (queryFull) dtFreeNavMeshQuery(queryFull);
        }
    }

    dtFreeNavMesh(navmesh);
}

int main(int argc, char** argv)
{
    Trinity::VerifyOsVersion();

    Trinity::Locale::Init();

    Trinity::Asio::IoContext ioContext(1);

    SetupLogging(&ioContext);

    std::thread loggingThread;

    auto workGuard = std::pair(
        Trinity::make_unique_ptr_with_deleter(&loggingThread, [](std::thread* thread) { thread->join(); }),
        boost::asio::make_work_guard(ioContext.get_executor())
    );

    loggingThread = std::thread([](Trinity::Asio::IoContext* context) { context->run(); }, &ioContext);

    Trinity::Banner::Show("MMAP generator", [](char const* text) { TC_LOG_INFO("tool.mmapgen", "{}", text); }, nullptr);

    unsigned int threads = std::thread::hardware_concurrency();
    int mapnum = -1;
    int tileX = -1, tileY = -1;
    Optional<float> maxAngle, maxAngleNotSteep;
    bool skipLiquid = false,
         skipContinents = false,
         skipJunkMaps = true,
         skipBattlegrounds = false,
         debugOutput = false,
         silent = false,
         bigBaseUnit = false;
    char const* offMeshInputPath = nullptr;
    char const* file = nullptr;
    boost::filesystem::path inputDirectory = boost::filesystem::current_path();
    boost::filesystem::path outputDirectory = boost::filesystem::current_path();

    // Check for --audit before full arg parsing.
    bool auditMode = false;
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--audit") == 0) auditMode = true;

    bool validParam = handleArgs(argc, argv, mapnum,
                                 tileX, tileY, maxAngle, maxAngleNotSteep,
                                 skipLiquid, skipContinents, skipJunkMaps, skipBattlegrounds,
                                 debugOutput, silent, bigBaseUnit, offMeshInputPath, file, threads,
                                 inputDirectory, outputDirectory);

    if (!validParam && !auditMode)
        return silent ? -1 : finish("You have specified invalid parameters", -1);

    if (auditMode)
    {
        if (mapnum < 0)
        {
            printf("[audit] Running audit on maps 0 (EK) and 1 (Kalimdor)...\n");
            RunNavmeshAudit(outputDirectory / "mmaps", 0);
            RunNavmeshAudit(outputDirectory / "mmaps", 1);
        }
        else
        {
            RunNavmeshAudit(outputDirectory / "mmaps", mapnum);
        }
        return 0;
    }

    if (mapnum == -1 && debugOutput)
    {
        if (silent)
            return -2;

        TC_LOG_INFO("tool.mmapgen", "You have specifed debug output, but didn't specify a map to generate.");
        TC_LOG_INFO("tool.mmapgen", "This will generate debug output for ALL maps.");
        TC_LOG_INFO("tool.mmapgen", "Are you sure you want to continue? (y/n)");
        if (getchar() != 'y')
            return 0;
    }

    std::vector<std::string> dbcLocales;
    if (!checkDirectories(inputDirectory, outputDirectory, debugOutput, dbcLocales))
        return silent ? -3 : finish("Press ENTER to close...", -3);

    _liquidTypes = LoadLiquid(inputDirectory, dbcLocales[0], silent, -5);

    LoadMap(inputDirectory, dbcLocales[0], silent, -4);

    MMAP::CreateVMapManager = &MMAP::VMapFactory::CreateVMapManager;

    // Load operator-curated road overrides (see RoadOverrides.h). The
    // export written by `.playerbot meta export` lives at
    // <input>/world_metadata.csv by default. Absent file is fine — the
    // override system is opt-in. Loaded ONCE here so all per-tile
    // calls in TileBuilder hit a frozen, lock-free read-only structure.
    {
        boost::filesystem::path overridesPath = inputDirectory / "world_metadata.csv";
        int loaded = MMAP::RoadOverrides::Instance().LoadFromFile(overridesPath.generic_string());
        if (loaded > 0)
            TC_LOG_INFO("tool.mmapgen",
                "Loaded {} road-override waypoint(s) from {}",
                loaded, overridesPath.string());
    }

    // Auto-discover off-mesh connections. The generator never creates
    // walkable mesh for some legitimately-traversable transitions —
    // most notably cave / barrow-den mouths, where the descending
    // entrance tunnel has no extracted collision surface, leaving the
    // interior a fully-meshed but DISCONNECTED navmesh island (proven
    // for Ban'ethil Barrow Den, map 1: interior reachable internally,
    // but no path from the surface — a source-data hole that NO tile
    // regen at any slope angle can close). The TC-native cure is an
    // off-mesh connection bridging the surface-approach poly to the
    // island poly. Rather than require every operator to remember a
    // --offMeshInput flag (a forgotten flag silently drops the bridge
    // on the next regen and re-breaks the quest), we auto-load a
    // canonical <input>/offmesh.txt when no explicit path was given —
    // the same opt-in, absent-is-fine pattern used for the road
    // overrides above. An explicit --offMeshInput still wins.
    // File format (one connection per line, '#' lines ignored):
    //   <mapId> <tileX>,<tileY> (<fromX> <fromY> <fromZ>) (<toX> <toY> <toZ>) <radius> [areaId] [flags]
    std::string offMeshDefaultStorage;
    if (offMeshInputPath == nullptr)
    {
        boost::filesystem::path defaultOffMesh = inputDirectory / "offmesh.txt";
        boost::system::error_code ec;
        if (boost::filesystem::is_regular_file(defaultOffMesh, ec))
        {
            offMeshDefaultStorage = defaultOffMesh.string();
            offMeshInputPath = offMeshDefaultStorage.c_str();
            TC_LOG_INFO("tool.mmapgen", "Auto-loading off-mesh connections from {}", offMeshDefaultStorage);
        }
    }

    MMAP::MapBuilder builder(inputDirectory, outputDirectory, maxAngle, maxAngleNotSteep, skipLiquid, skipContinents, skipJunkMaps,
                       skipBattlegrounds, debugOutput, bigBaseUnit, mapnum, offMeshInputPath, threads);

    uint32 start = getMSTime();
    if (file)
        builder.buildMeshFromFile(file);
    else if (tileX > -1 && tileY > -1 && mapnum >= 0)
        builder.buildSingleTile(mapnum, tileX, tileY);
    else if (mapnum >= 0)
        builder.buildMaps(uint32(mapnum));
    else
        builder.buildMaps({});

    if (!silent)
        TC_LOG_INFO("tool.mmapgen", "Finished. MMAPS were built in {}", secsToTimeString(GetMSTimeDiffToNow(start) / 1000));

    return 0;
}

#if TRINITY_COMPILER_IS_MICROSOFT
#include "WheatyExceptionReport.h"
// must be at end of file because of init_seg pragma
INIT_CRASH_HANDLER();
#endif
