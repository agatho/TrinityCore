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

#include "TileBuilder.h"
#include "IntermediateValues.h"
#include "Log.h"
#include "MMapDefines.h"
#include "Memory.h"
#include "RoadOverrides.h"
#include "StringFormat.h"
#include "VMapManager.h"
#include <DetourNavMeshBuilder.h>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

// Generator tuning overrides (see MMapDefines.h). Default-constructed =
// partition Layers + all sentinels, i.e. the existing fork behaviour. The
// mmaps_generator CLI mutates this once before tile building begins.
MmapGenTuning g_mmapGenTuning;

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX            // windows.h min/max macros break std::min/max below
#endif
#include <windows.h>
namespace
{
    // Plain C++ relay so the SEH frame below stays destructor-free
    // (MSVC C2712: __try cannot coexist with object unwinding).
    void DoBuildMoveMapTile(MMAP::TileBuilder* b, uint32 mapID, uint32 tileX, uint32 tileY,
                            MMAP::MeshData* meshData, float* bmin, float* bmax,
                            dtNavMeshParams const* params,
                            MMAP::TileBuilder::TileResult* outResult)
    {
        *outResult = b->buildMoveMapTile(mapID, tileX, tileY, *meshData,
                                         *reinterpret_cast<float(*)[3]>(bmin),
                                         *reinterpret_cast<float(*)[3]>(bmax),
                                         params);
    }
}
// SEH guard for one tile build. Recast can ACCESS_VIOLATION on degenerate
// input geometry (observed 2026-06-12: rcBuildPolyMesh removeVertex AV on
// Eastern Kingdoms tiles fed by the fixed vmap extractor's much richer WMO
// collision — killed the whole world build at tile 853). One poisoned tile
// must not abort a multi-hour batch: skip it, report it, keep going.
extern "C" unsigned int SehSafeBuildMoveMapTile(
    MMAP::TileBuilder* b, unsigned int mapID, unsigned int tileX, unsigned int tileY,
    MMAP::MeshData* meshData, float* bmin, float* bmax,
    dtNavMeshParams const* params, MMAP::TileBuilder::TileResult* outResult) noexcept
{
    __try
    {
        DoBuildMoveMapTile(b, mapID, tileX, tileY, meshData, bmin, bmax, params, outResult);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}
#endif

namespace
{
    struct Tile
    {
        Tile() : chf(nullptr), solid(nullptr), cset(nullptr), pmesh(nullptr), dmesh(nullptr) {}
        ~Tile()
        {
            rcFreeCompactHeightfield(chf);
            rcFreeContourSet(cset);
            rcFreeHeightField(solid);
            rcFreePolyMesh(pmesh);
            rcFreePolyMeshDetail(dmesh);
        }
        rcCompactHeightfield* chf;
        rcHeightfield* solid;
        rcContourSet* cset;
        rcPolyMesh* pmesh;
        rcPolyMeshDetail* dmesh;
    };
}

namespace MMAP
{
    struct TileConfig
    {
        TileConfig(bool bigBaseUnit)
        {
            // these are WORLD UNIT based metrics
            // this are basic unit dimentions
            // value have to divide GRID_SIZE(533.3333f) ( aka: 0.5333, 0.2666, 0.3333, 0.1333, etc )
            BASE_UNIT_DIM = bigBaseUnit ? 0.5333333f : 0.2666666f;

            // All are in UNIT metrics!
            VERTEX_PER_MAP = int(GRID_SIZE / BASE_UNIT_DIM + 0.5f);
            VERTEX_PER_TILE = bigBaseUnit ? 40 : 80; // must divide VERTEX_PER_MAP
            TILES_PER_MAP = VERTEX_PER_MAP / VERTEX_PER_TILE;
        }

        float BASE_UNIT_DIM;
        int VERTEX_PER_MAP;
        int VERTEX_PER_TILE;
        int TILES_PER_MAP;
    };

    TileBuilder::TileBuilder(boost::filesystem::path const& inputDirectory, boost::filesystem::path const& outputDirectory,
        Optional<float> maxWalkableAngle, Optional<float> maxWalkableAngleNotSteep,
        bool skipLiquid, bool bigBaseUnit, bool debugOutput, std::vector<OffMeshData> const* offMeshConnections) :
        m_outputDirectory(outputDirectory),
        m_maxWalkableAngle(maxWalkableAngle),
        m_maxWalkableAngleNotSteep(maxWalkableAngleNotSteep),
        m_bigBaseUnit(bigBaseUnit),
        m_debugOutput(debugOutput),
        m_terrainBuilder(inputDirectory, skipLiquid),
        m_rcContext(false),
        m_offMeshConnections(offMeshConnections)
    {
    }

    TileBuilder::~TileBuilder() = default;

    /**************************************************************************/
    void TileBuilder::buildTile(uint32 mapID, uint32 tileX, uint32 tileY, dtNavMesh* navMesh)
    {
        if (shouldSkipTile(mapID, tileX, tileY))
        {
            OnTileDone();
            return;
        }

        TC_LOG_INFO("maps.mmapgen", "{} [Map {:04}] Building tile [{:02},{:02}]", GetProgressText(), mapID, tileX, tileY);
        // Unbuffered breadcrumb: TC_LOG console output is block-buffered
        // through redirected pipes and is LOST on a hard crash (observed
        // 2026-06-12: three 0-byte logs from heap-corruption deaths). This
        // stderr line + flush survives, so the crash log names the in-
        // flight tiles. Cheap (one line per tile) — keep.
        std::fprintf(stderr, "[tile-begin] map=%04u tile=%02u,%02u\n", mapID, tileX, tileY);
        std::fflush(stderr);

        MeshData meshData;

        std::unique_ptr<VMAP::VMapManager> vmapManager = CreateVMapManager(mapID);

        // get heightmap data
        m_terrainBuilder.loadMap(mapID, tileX, tileY, meshData, vmapManager.get());

        // get model data
        m_terrainBuilder.loadVMap(mapID, tileX, tileY, meshData, vmapManager.get());

        // if there is no data, give up now
        if (meshData.solidVerts.empty() && meshData.liquidVerts.empty())
        {
            OnTileDone();
            return;
        }

        // remove unused vertices
        TerrainBuilder::cleanVertices(meshData.solidVerts, meshData.solidTris);
        TerrainBuilder::cleanVertices(meshData.liquidVerts, meshData.liquidTris);

        if (meshData.liquidVerts.empty() && meshData.solidVerts.empty())
        {
            OnTileDone();
            return;
        }

        // gather all mesh data for final data check, and bounds calculation
        std::vector<float> allVerts(meshData.liquidVerts.size() + meshData.solidVerts.size());
        std::ranges::copy(meshData.liquidVerts, allVerts.begin());
        std::ranges::copy(meshData.solidVerts, allVerts.begin() + std::ssize(meshData.liquidVerts));

        // get bounds of current tile
        float bmin[3], bmax[3];
        getTileBounds(tileX, tileY, allVerts.data(), allVerts.size() / 3, bmin, bmax);

        if (m_offMeshConnections)
            m_terrainBuilder.loadOffMeshConnections(mapID, tileX, tileY, meshData, *m_offMeshConnections);

        // build navmesh tile
        TileResult tileResult;
#ifdef _WIN32
        // SEH-guarded: a Recast AV on degenerate geometry skips THIS tile
        // instead of killing the whole batch (see SehSafeBuildMoveMapTile).
        if (unsigned int seh = SehSafeBuildMoveMapTile(this, mapID, tileX, tileY,
                &meshData, bmin, bmax, navMesh->getParams(), &tileResult))
        {
            TC_LOG_ERROR("maps.mmapgen",
                "[Map {:04}] SEH 0x{:08X} building tile [{:02},{:02}] — SKIPPED "
                "(degenerate input geometry?)",
                mapID, seh, tileX, tileY);
            OnTileDone();
            return;
        }
#else
        tileResult = buildMoveMapTile(mapID, tileX, tileY, meshData, bmin, bmax, navMesh->getParams());
#endif
        if (tileResult.data)
            saveMoveMapTileToFile(mapID, tileX, tileY, navMesh, tileResult);

        OnTileDone();
    }

    /**************************************************************************/
    TileBuilder::TileResult TileBuilder::buildMoveMapTile(uint32 mapID, uint32 tileX, uint32 tileY,
        MeshData& meshData, float (&bmin)[3], float (&bmax)[3],
        dtNavMeshParams const* navMeshParams, std::string_view fileNameSuffix)
    {
        // console output
        std::string tileString = Trinity::StringFormat("[Map {:04}] [{:02},{:02}]:", mapID, tileX, tileY);
        TC_LOG_INFO("maps.mmapgen", "{} Building movemap tile...", tileString);

        TileResult tileResult;

        IntermediateValues iv;

        float* tVerts = meshData.solidVerts.data();
        int tVertCount = meshData.solidVerts.size() / 3;
        int* tTris = meshData.solidTris.data();
        int tTriCount = meshData.solidTris.size() / 3;

        float* lVerts = meshData.liquidVerts.data();
        int lVertCount = meshData.liquidVerts.size() / 3;
        int* lTris = meshData.liquidTris.data();
        int lTriCount = meshData.liquidTris.size() / 3;
        uint8* lTriFlags = meshData.liquidType.data();

        const TileConfig tileConfig = TileConfig(m_bigBaseUnit);
        int TILES_PER_MAP = tileConfig.TILES_PER_MAP;
        float BASE_UNIT_DIM = tileConfig.BASE_UNIT_DIM;
        rcConfig config = GetMapSpecificConfig(mapID, bmin, bmax, tileConfig);

        // this sets the dimensions of the heightfield - should maybe happen before border padding
        rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

        // allocate subregions : tiles
        std::unique_ptr<Tile[]> tiles = std::make_unique<Tile[]>(TILES_PER_MAP * TILES_PER_MAP);

        // Initialize per tile config.
        rcConfig tileCfg = config;
        tileCfg.width = config.tileSize + config.borderSize * 2;
        tileCfg.height = config.tileSize + config.borderSize * 2;

        // merge per tile poly and detail meshes
        std::unique_ptr<rcPolyMesh*[]> pmmerge = std::make_unique<rcPolyMesh*[]>(TILES_PER_MAP * TILES_PER_MAP);
        std::unique_ptr<rcPolyMeshDetail*[]> dmmerge = std::make_unique<rcPolyMeshDetail*[]>(TILES_PER_MAP * TILES_PER_MAP);
        int nmerge = 0;
        // build all tiles
        for (int y = 0; y < TILES_PER_MAP; ++y)
        {
            for (int x = 0; x < TILES_PER_MAP; ++x)
            {
                Tile& tile = tiles[x + y * TILES_PER_MAP];

                // Calculate the per tile bounding box.
                tileCfg.bmin[0] = config.bmin[0] + x * float(config.tileSize * config.cs);
                tileCfg.bmin[2] = config.bmin[2] + y * float(config.tileSize * config.cs);
                tileCfg.bmax[0] = config.bmin[0] + (x + 1) * float(config.tileSize * config.cs);
                tileCfg.bmax[2] = config.bmin[2] + (y + 1) * float(config.tileSize * config.cs);

                tileCfg.bmin[0] -= tileCfg.borderSize * tileCfg.cs;
                tileCfg.bmin[2] -= tileCfg.borderSize * tileCfg.cs;
                tileCfg.bmax[0] += tileCfg.borderSize * tileCfg.cs;
                tileCfg.bmax[2] += tileCfg.borderSize * tileCfg.cs;

                // build heightfield
                tile.solid = rcAllocHeightfield();
                if (!tile.solid || !rcCreateHeightfield(&m_rcContext, *tile.solid, tileCfg.width, tileCfg.height, tileCfg.bmin, tileCfg.bmax, tileCfg.cs, tileCfg.ch))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed building heightfield!", tileString);
                    continue;
                }

                // mark all walkable tiles, both liquids and solids

                /* we want to have triangles with slope less than walkableSlopeAngleNotSteep (<= 55) to have NAV_AREA_GROUND
                 * and with slope between walkableSlopeAngleNotSteep and walkableSlopeAngle (55 < .. <= 70) to have NAV_AREA_GROUND_STEEP.
                 * we achieve this using recast API: memset everything to NAV_AREA_GROUND_STEEP, call rcClearUnwalkableTriangles with 70 so
                 * any area above that will get RC_NULL_AREA (unwalkable), then call rcMarkWalkableTriangles with 55 to set NAV_AREA_GROUND
                 * on anything below 55 . Players and idle Creatures can use NAV_AREA_GROUND, while Creatures in combat can use NAV_AREA_GROUND_STEEP.
                 */
                std::unique_ptr<unsigned char[]> triFlags = std::make_unique<unsigned char[]>(tTriCount);
                memset(triFlags.get(), NAV_AREA_GROUND_STEEP, tTriCount * sizeof(unsigned char));
                rcClearUnwalkableTriangles(&m_rcContext, tileCfg.walkableSlopeAngle, tVerts, tVertCount, tTris, tTriCount, triFlags.get());
                rcMarkWalkableTriangles(&m_rcContext, tileCfg.walkableSlopeAngleNotSteep, tVerts, tVertCount, tTris, tTriCount, triFlags.get(), NAV_AREA_GROUND);

                // Road-aware mmaps Phase 2 (WMO): promote triangles whose
                // GroupModel material is a road texture (sidecar-flagged)
                // to NAV_AREA_ROAD. Run AFTER rcMarkWalkableTriangles so we
                // override its NAV_AREA_GROUND for the road triangles.
                // Triangles without a road flag keep whatever
                // ClearUnwalkable / MarkWalkable assigned (GROUND, STEEP,
                // or RC_NULL_AREA for too-steep).
                //
                // Texture detector tags stone-textured mountains as roads —
                // curated handcrafted_road/RoadOverrides are the road
                // sources; audit 2026-07-03. This WMO-sidecar signal is
                // texture-classifier derived, so only consume it under
                // --textureRoads (default off).
                if (g_mmapGenTuning.textureRoads && !meshData.solidTriRoadFlags.empty())
                {
                    int promoted = 0;
                    int limit = std::min<int>(tTriCount, static_cast<int>(meshData.solidTriRoadFlags.size()));
                    for (int i = 0; i < limit; ++i)
                    {
                        if (meshData.solidTriRoadFlags[i] != 0 &&
                            triFlags[i] != RC_NULL_AREA)
                        {
                            triFlags[i] = NAV_AREA_ROAD;
                            ++promoted;
                        }
                    }
                    if (promoted > 0)
                        meshData.hasRoadMask = true; // signal post-pass to run
                }
                else if (!meshData.solidTriRoadFlags.empty())
                {
                    // Present on disk but ignored by policy — flagged for the
                    // one-line-per-run summary in PathGenerator.cpp.
                    g_mmapGenTuning.textureRoadsIgnoredSeen.store(true, std::memory_order_relaxed);
                }

                rcRasterizeTriangles(&m_rcContext, tVerts, tVertCount, tTris, triFlags.get(), tTriCount, *tile.solid, config.walkableClimb);

                rcFilterLowHangingWalkableObstacles(&m_rcContext, config.walkableClimb, *tile.solid);
                rcFilterLedgeSpans(&m_rcContext, tileCfg.walkableHeight, tileCfg.walkableClimb, *tile.solid);
                rcFilterWalkableLowHeightSpans(&m_rcContext, tileCfg.walkableHeight, *tile.solid);

                // add liquid triangles
                rcRasterizeTriangles(&m_rcContext, lVerts, lVertCount, lTris, lTriFlags, lTriCount, *tile.solid, config.walkableClimb);

                // compact heightfield spans
                tile.chf = rcAllocCompactHeightfield();
                if (!tile.chf || !rcBuildCompactHeightfield(&m_rcContext, tileCfg.walkableHeight, tileCfg.walkableClimb, *tile.solid, *tile.chf))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed compacting heightfield!", tileString);
                    continue;
                }

                // build polymesh intermediates
                if (!rcErodeWalkableArea(&m_rcContext, config.walkableRadius, *tile.chf))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed eroding area!", tileString);
                    continue;
                }

                if (!rcMedianFilterWalkableArea(&m_rcContext, *tile.chf))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed filtering area!", tileString);
                    continue;
                }

                // Region partitioning — selectable via g_mmapGenTuning.partition
                // (mmaps_generator --partition). The fork default is Layers
                // (rcBuildLayerRegions), chosen 2026-05-21 after probe-based
                // validation: watershed produced disconnected polygon islands
                // for spiral-stair / multi-floor WMOs (Aldrassil's Tenaron
                // Stormgrip room was unreachable — ground polys and upper-floor
                // polys landed in separate regions with no edges between them).
                //
                // Recast docs (Mikko Mononen): watershed yields the nicest,
                // most regular regions on open terrain but overlaps/holes in
                // "narrow spiral corridors e.g. spiral stairs"; monotone never
                // produces holes (long thin polys instead); layered is the
                // middle ground and links multi-floor geometry. RecastDemo's
                // "indoor" sample uses MONOTONE for exactly this reason.
                //
                // Watershed is the only method that needs the distance field,
                // so it is built only on that branch (saves work on the others).
                // Source ref: src/modules/PlayerbotV2/docs/MMAP_PARAMETER_TUNING.md
                // Resolve the effective method. Auto picks PER SUBTILE from the
                // compact-heightfield's vertical-layering fraction — the fraction
                // of occupied cells carrying >=2 walkable spans (i.e. a floor with
                // something walkable above it: bridges, multi-floor WMOs, caves).
                // That 2.5D signal is the principled discriminator: open terrain
                // is ~0 layered, a barrow-den/tower tile is materially layered.
                //   >0.06  -> Layers     (link the floors; the connectivity win)
                //   <0.005 -> Watershed  (flat/open: smoothest, most regular)
                //   else   -> Monotone   (mixed: never holes, predictable)
                MmapPartitionMethod method = g_mmapGenTuning.partition;
                // Instanced dungeon/raid maps: the Layers default islands
                // winding WMO mine tunnels into disconnected navmesh regions
                // (Deadmines foundry — root-caused + live-validated 2026-07-01;
                // Monotone meshes the tunnels and does NOT regress multi-floor
                // linkage, Aldrassil verified). Switch such maps Layers->Monotone
                // unless the operator gave an explicit --partition.
                if (!g_mmapGenTuning.partitionExplicit
                    && method == MmapPartitionMethod::Layers
                    && g_mmapGenTuning.instanceMaps.count(mapID))
                    method = MmapPartitionMethod::Monotone;
                if (method == MmapPartitionMethod::Auto)
                {
                    uint32 occupied = 0, layered = 0;
                    const int cellCount = tile.chf->width * tile.chf->height;
                    for (int ci = 0; ci < cellCount; ++ci)
                    {
                        const rcCompactCell& cc = tile.chf->cells[ci];
                        if (cc.count == 0) continue;
                        ++occupied;
                        if (cc.count >= 2) ++layered;
                    }
                    const float layeredFrac = occupied ? float(layered) / float(occupied) : 0.f;
                    if (layeredFrac > 0.06f)        method = MmapPartitionMethod::Layers;
                    else if (layeredFrac < 0.005f)  method = MmapPartitionMethod::Watershed;
                    else                            method = MmapPartitionMethod::Monotone;
                }

                bool regionOk = false;
                switch (method)
                {
                    case MmapPartitionMethod::Watershed:
                        if (!rcBuildDistanceField(&m_rcContext, *tile.chf))
                        {
                            TC_LOG_ERROR("maps.mmapgen", "{} Failed building distance field!", tileString);
                            break;
                        }
                        regionOk = rcBuildRegions(&m_rcContext, *tile.chf,
                            tileCfg.borderSize, tileCfg.minRegionArea, tileCfg.mergeRegionArea);
                        break;
                    case MmapPartitionMethod::Monotone:
                        regionOk = rcBuildRegionsMonotone(&m_rcContext, *tile.chf,
                            tileCfg.borderSize, tileCfg.minRegionArea, tileCfg.mergeRegionArea);
                        break;
                    case MmapPartitionMethod::Layers:
                    default:
                        regionOk = rcBuildLayerRegions(&m_rcContext, *tile.chf,
                            tileCfg.borderSize, tileCfg.minRegionArea);
                        break;
                }
                // Safety net: if watershed/monotone failed on this subtile, fall
                // back to layered (the most robust for our multi-floor geometry)
                // so a partitioner hiccup never silently drops the subtile.
                if (!regionOk && method != MmapPartitionMethod::Layers)
                    regionOk = rcBuildLayerRegions(&m_rcContext, *tile.chf,
                        tileCfg.borderSize, tileCfg.minRegionArea);
                if (!regionOk)
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed building regions!", tileString);
                    continue;
                }

                tile.cset = rcAllocContourSet();
                if (!tile.cset || !rcBuildContours(&m_rcContext, *tile.chf, tileCfg.maxSimplificationError, tileCfg.maxEdgeLen, *tile.cset))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed building contours!", tileString);
                    continue;
                }

                // build polymesh
                tile.pmesh = rcAllocPolyMesh();
                if (!tile.pmesh || !rcBuildPolyMesh(&m_rcContext, *tile.cset, tileCfg.maxVertsPerPoly, *tile.pmesh))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed building polymesh!", tileString);
                    continue;
                }

                tile.dmesh = rcAllocPolyMeshDetail();
                if (!tile.dmesh || !rcBuildPolyMeshDetail(&m_rcContext, *tile.pmesh, *tile.chf, tileCfg.detailSampleDist, tileCfg.detailSampleMaxError, *tile.dmesh))
                {
                    TC_LOG_ERROR("maps.mmapgen", "{} Failed building polymesh detail!", tileString);
                    continue;
                }

                // free those up
                // we may want to keep them in the future for debug
                // but right now, we don't have the code to merge them
                rcFreeHeightField(tile.solid);
                tile.solid = nullptr;
                rcFreeCompactHeightfield(tile.chf);
                tile.chf = nullptr;
                rcFreeContourSet(tile.cset);
                tile.cset = nullptr;

                pmmerge[nmerge] = tile.pmesh;
                dmmerge[nmerge] = tile.dmesh;
                nmerge++;
            }
        }

        iv.polyMesh = rcAllocPolyMesh();
        if (!iv.polyMesh)
        {
            TC_LOG_ERROR("maps.mmapgen", "{} alloc iv.polyMesh FAILED!", tileString);
            return tileResult;
        }
        rcMergePolyMeshes(&m_rcContext, pmmerge.get(), nmerge, *iv.polyMesh);

        iv.polyMeshDetail = rcAllocPolyMeshDetail();
        if (!iv.polyMeshDetail)
        {
            TC_LOG_ERROR("maps.mmapgen", "{} alloc m_dmesh FAILED!", tileString);
            return tileResult;
        }
        rcMergePolyMeshDetails(&m_rcContext, dmmerge.get(), nmerge, *iv.polyMeshDetail);

        // free things up
        pmmerge = nullptr;
        dmmerge = nullptr;
        tiles = nullptr;

        // Road-aware mmaps: pre-pass that promotes a polygon's area to
        // NAV_AREA_ROAD when its centroid falls in an MCNK flagged as road
        // by the extractor (parallel .road file consumed by TerrainBuilder).
        //
        // The promotion happens BEFORE the flag-computation loop below, so
        // road polygons get NAV_ROAD in `flags[i]` via the standard formula
        // (1 << (NAV_AREA_MAX_VALUE - NAV_AREA_ROAD) == 0x10). Non-road
        // polygons are untouched and retain NAV_AREA_GROUND / *_STEEP /
        // *_WATER / *_MAGMA_SLIME exactly as before.
        // Override candidates for this tile from the operator-curated
        // road_points (see RoadOverrides.h). Computed once outside the
        // poly loop because all polys in the tile share the same bbox.
        // Done outside the `hasRoadMask` branch so override tagging
        // works even on tiles where the texture classifier produced an
        // empty road mask (the entire point of overrides).
        std::vector<MMAP::RoadOverridePoint> tile_overrides;
        {
            float const tileMinX = bmin[0];
            float const tileMinZ = bmin[2];
            float const tileMaxX = bmin[0] + (bmax[0] - bmin[0]);
            float const tileMaxZ = bmin[2] + (bmax[2] - bmin[2]);
            tile_overrides = MMAP::RoadOverrides::Instance()
                .PointsOverlappingTile(mapID, tileMinX, tileMinZ,
                                              tileMaxX, tileMaxZ);
        }

        if (meshData.hasRoadMask || !tile_overrides.empty())
        {
            // Texture-derived ADT road mask present but policy says ignore it
            // (see g_mmapGenTuning.textureRoads comment in MMapDefines.h) —
            // flag once per tile for the one-line-per-run summary rather than
            // spamming per-poly below.
            if (meshData.hasRoadMask && !g_mmapGenTuning.textureRoads)
                g_mmapGenTuning.textureRoadsIgnoredSeen.store(true, std::memory_order_relaxed);

            int const nvp = iv.polyMesh->nvp;
            float const cs = config.cs;
            // World-space origin of this tile (matches TerrainBuilder's
            // (tileX - 32) * GRID_SIZE convention via bmin[0]/bmin[2]).
            float const tileMinX = bmin[0];
            float const tileMinZ = bmin[2];
            for (int i = 0; i < iv.polyMesh->npolys; ++i)
            {
                uint8 area = iv.polyMesh->areas[i] & NAV_AREA_ALL_MASK;
                // Only promote GROUND/GROUND_STEEP polys — water/lava don't
                // become roads.
                if (area != NAV_AREA_GROUND && area != NAV_AREA_GROUND_STEEP)
                    continue;

                // Polygon centroid in tile-local coords (then world coords).
                unsigned short const* poly = &iv.polyMesh->polys[i * 2 * nvp];
                int sx = 0, sz = 0, n = 0;
                for (int j = 0; j < nvp; ++j)
                {
                    if (poly[j] == RC_MESH_NULL_IDX) break;
                    unsigned short const* v = &iv.polyMesh->verts[poly[j] * 3];
                    sx += v[0];
                    sz += v[2];
                    ++n;
                }
                if (n == 0) continue;

                float worldX = tileMinX + (static_cast<float>(sx) / n) * cs;
                float worldZ = tileMinZ + (static_cast<float>(sz) / n) * cs;

                // Texture-classifier road mask (per-MCNK 16x16 grid).
                // Texture detector tags stone-textured mountains as roads —
                // curated handcrafted_road/RoadOverrides are the road
                // sources; audit 2026-07-03. Ignored unless --textureRoads
                // opts back into the (mistagging) legacy behavior.
                if (g_mmapGenTuning.textureRoads && meshData.hasRoadMask)
                {
                    static constexpr float kChunkSize = GRID_SIZE / 16.0f;
                    int mcnkCol = static_cast<int>((worldX - tileMinX) / kChunkSize);
                    int mcnkRow = static_cast<int>((worldZ - tileMinZ) / kChunkSize);
                    if (mcnkRow < 0) mcnkRow = 0;
                    if (mcnkRow > 15) mcnkRow = 15;
                    if (mcnkCol < 0) mcnkCol = 0;
                    if (mcnkCol > 15) mcnkCol = 15;
                    std::size_t maskIdx = static_cast<std::size_t>(mcnkRow * 16 + mcnkCol);
                    if (meshData.roadMask[maskIdx] != 0)
                    {
                        iv.polyMesh->areas[i] = NAV_AREA_ROAD;
                        continue;
                    }
                }

                // Operator overrides — exact world-space distance check.
                // Polys whose centroid is within ANY override's radius
                // get tagged. Continues looping over polys (don't break
                // out of the for(i) loop because other polys may match
                // other override points).
                for (auto const& ov : tile_overrides)
                {
                    const float dx = worldX - ov.x;
                    // The Y axis in TC is the Z axis in Detour internal
                    // coords — bmin/bmax are in Detour layout (XZY).
                    // Override's `y` is TC world-Y == Detour Z.
                    const float dz = worldZ - ov.y;
                    if (dx*dx + dz*dz <= ov.radius * ov.radius)
                    {
                        iv.polyMesh->areas[i] = NAV_AREA_ROAD;
                        break;
                    }
                }
            }
        }

        // set polygons as walkable
        // TODO: special flags for DYNAMIC polygons, ie surfaces that can be turned on and off
        for (int i = 0; i < iv.polyMesh->npolys; ++i)
        {
            if (uint8 area = iv.polyMesh->areas[i] & NAV_AREA_ALL_MASK)
            {
                if (area >= NAV_AREA_MIN_VALUE)
                    iv.polyMesh->flags[i] = 1 << (NAV_AREA_MAX_VALUE - area);
                else
                    iv.polyMesh->flags[i] = NAV_GROUND; // TODO: these will be dynamic in future
            }
        }

        // Post-build island pruning: flood-fill from the largest connected
        // component and mark isolated tiny islands (< 5 polys) as empty so
        // bots never land on unreachable navmesh fragments. These arise from
        // the minRegionArea=rcSqr(12) reduction: rooftops, awning ledges,
        // and small geometry artifacts survive area filtering but produce
        // disconnected poly groups that Detour can findNearestPoly onto
        // but can't route out of.
        {
            int const npolys = iv.polyMesh->npolys;
            int const nvp = iv.polyMesh->nvp;
            if (npolys > 0)
            {
                // Build adjacency from shared edges (polys sharing 2 verts).
                std::vector<std::vector<int>> adj(npolys);
                for (int i = 0; i < npolys; ++i)
                {
                    for (int j = 0; j < nvp; ++j)
                    {
                        unsigned short nei = iv.polyMesh->polys[i * 2 * nvp + nvp + j];
                        if (nei == RC_MESH_NULL_IDX) continue;
                        int neiIdx = static_cast<int>(nei);
                        if (neiIdx >= 0 && neiIdx < npolys)
                            adj[i].push_back(neiIdx);
                    }
                }
                // Flood-fill connected components.
                std::vector<int> comp_id(npolys, -1);
                std::vector<int> comp_sizes;
                int num_comps = 0;
                for (int i = 0; i < npolys; ++i)
                {
                    if (comp_id[i] >= 0) continue;
                    if (iv.polyMesh->areas[i] == 0) continue;
                    int cid = num_comps++;
                    int sz = 0;
                    std::queue<int> q;
                    q.push(i);
                    comp_id[i] = cid;
                    while (!q.empty())
                    {
                        int cur = q.front(); q.pop();
                        ++sz;
                        for (int nb : adj[cur])
                        {
                            if (comp_id[nb] < 0 && iv.polyMesh->areas[nb] != 0)
                            {
                                comp_id[nb] = cid;
                                q.push(nb);
                            }
                        }
                    }
                    comp_sizes.push_back(sz);
                }
                // Find the largest component.
                int largest_id = -1;
                int largest_sz = 0;
                for (int c = 0; c < num_comps; ++c)
                {
                    if (comp_sizes[c] > largest_sz)
                    {
                        largest_sz = comp_sizes[c];
                        largest_id = c;
                    }
                }

                // Per-poly centroid in Recast space (same space as the navmesh
                // verts and off-mesh connection endpoints). Recast axes:
                // x/z = horizontal plane, y = up.
                std::vector<float> cx(npolys, 0.f), cy(npolys, 0.f), cz(npolys, 0.f);
                {
                    float const* bmin = iv.polyMesh->bmin;
                    float const cs = iv.polyMesh->cs, chh = iv.polyMesh->ch;
                    for (int i = 0; i < npolys; ++i)
                    {
                        unsigned short const* p = &iv.polyMesh->polys[i * 2 * nvp];
                        int n = 0;
                        for (int j = 0; j < nvp; ++j)
                        {
                            if (p[j] == RC_MESH_NULL_IDX) break;
                            unsigned short const* v = &iv.polyMesh->verts[p[j] * 3];
                            cx[i] += bmin[0] + v[0] * cs;
                            cy[i] += bmin[1] + v[1] * chh;
                            cz[i] += bmin[2] + v[2] * cs;
                            ++n;
                        }
                        if (n) { cx[i] /= n; cy[i] /= n; cz[i] /= n; }
                    }
                }

                // Bridge-or-prune each non-largest component:
                //   * If a substantial component sits a SHORT, ~LEVEL gap from
                //     the main mesh, emit a bidirectional off-mesh connection
                //     instead of pruning it. Fixes piers/docks, detached
                //     platforms, and narrow water/gangplank gaps the navmesh
                //     can't auto-link (a horizontal gap that walkableClimb,
                //     which only bridges vertical steps, never closes). The
                //     near-level gate (small vertical delta) is the safety
                //     valve: rooftops/awnings/cliff ledges — which make up most
                //     isolated fragments — have a LARGE vertical delta and are
                //     therefore never bridged (so bots don't "jump" onto roofs).
                //   * Otherwise, prune tiny unreachable fragments (< 10 polys)
                //     as before so bots never findNearestPoly onto a dead-end.
                // 30y horizontal reach: tree-canopy / overhang holes (Teldrassil,
                // forested zones) leave isolated near-level navmesh fragments
                // ~25-30y from the main mesh where the walkable ground is real
                // but the gen raycast was occluded. 20y missed them (verified:
                // a Teldrassil fragment at (9919,777) sits ~27y from reachable
                // mesh, unreachable from every direction → bot Uraimus trapped).
                // The ≤3y vertical gate still blocks rooftops/cliffs, so a 30y
                // NEAR-LEVEL gap is almost always genuinely walkable ground.
                constexpr float kBridgeMaxHoriz   = 30.0f;          // yards
                constexpr float kBridgeMaxHorizSq = kBridgeMaxHoriz * kBridgeMaxHoriz;
                constexpr float kBridgeMaxVert    = 3.0f;           // near-level only
                constexpr int   kMinBridgeComp    = 4;              // skip noise slivers
                // Each off-mesh connection costs 2 verts + 1 poly in the tile
                // data. Detour stores vertex indices as uint16, so the total
                // (base verts + cons*2) must stay < 0xffff or dtCreateNavMeshData
                // fails and the tile is dropped. WMO-dense expansion tiles
                // (Outland/Shadowlands) have many isolated fragments; left
                // unbounded the bridges overflowed the budget and silently
                // killed ~250 tiles per such map. Cap per-tile bridge count and
                // stop before the vertex budget; the dtCreateNavMeshData retry
                // below is the final safety net.
                constexpr int   kMaxBridgesPerTile = 48;
                constexpr int   kBridgeSkipPolys   = 2000;          // also bounds the O(n^2) search
                int pruned = 0, bridged = 0;
                int const baseCons = static_cast<int>(meshData.offMeshConnections.size() / 6);
                if (largest_id >= 0 && npolys <= kBridgeSkipPolys)
                {
                    // Nearest main-mesh poly per non-largest component
                    // (min horizontal gap subject to the near-level gate).
                    std::vector<float> bestSq(num_comps, std::numeric_limits<float>::max());
                    std::vector<int>   bestIso(num_comps, -1), bestMain(num_comps, -1);
                    for (int i = 0; i < npolys; ++i)
                    {
                        int const c = comp_id[i];
                        if (c < 0 || c == largest_id) continue;
                        if (comp_sizes[c] < kMinBridgeComp) continue;
                        for (int m = 0; m < npolys; ++m)
                        {
                            if (comp_id[m] != largest_id) continue;
                            // Relaxed centroid vertical gate for SELECTION only —
                            // the strict per-vertex ≤3y check happens in the
                            // refine pass below. Using kBridgeMaxVert here would
                            // skip a valid near-level main poly whose CENTROID Y
                            // differs by >3y (large/sloped poly) even though its
                            // nearest edge vertex is level with the fragment.
                            if (std::fabs(cy[i] - cy[m]) > 8.0f) continue;
                            float const dx = cx[i] - cx[m], dz = cz[i] - cz[m];
                            float const d = dx * dx + dz * dz;
                            if (d < bestSq[c]) { bestSq[c] = d; bestIso[c] = i; bestMain[c] = m; }
                        }
                    }
                    float const* bmin = iv.polyMesh->bmin;
                    float const csz = iv.polyMesh->cs, chh = iv.polyMesh->ch;
                    for (int c = 0; c < num_comps; ++c)
                    {
                        if (c == largest_id) continue;
                        bool bridged_this = false;
                        if (bestIso[c] >= 0)
                        {
                            // Refine the gap to the true EDGE distance: centroid-
                            // to-centroid OVERESTIMATES for large polys (a real 27y
                            // edge gap reads as ~37y between centroids), which wrongly
                            // rejected genuine near-level holes — e.g. the Teldrassil
                            // canopy hole that traps every Night Elf bot. Walk the
                            // nearest near-level VERTEX pair between the two polys; use
                            // it for BOTH the threshold test and the off-mesh endpoints
                            // (anchored on the actual walkable edges, nudged ~1.5y inward
                            // so Detour's findNearestPoly resolves each end to its poly).
                            float ea[3] = {0,0,0}, eb[3] = {0,0,0};
                            float bestv = std::numeric_limits<float>::max();
                            unsigned short const* pa = &iv.polyMesh->polys[bestIso[c] * 2 * nvp];
                            unsigned short const* pb = &iv.polyMesh->polys[bestMain[c] * 2 * nvp];
                            for (int ja = 0; ja < nvp && pa[ja] != RC_MESH_NULL_IDX; ++ja)
                            {
                                unsigned short const* va = &iv.polyMesh->verts[pa[ja] * 3];
                                float const vax = bmin[0] + va[0]*csz, vay = bmin[1] + va[1]*chh, vaz = bmin[2] + va[2]*csz;
                                for (int jb = 0; jb < nvp && pb[jb] != RC_MESH_NULL_IDX; ++jb)
                                {
                                    unsigned short const* vb = &iv.polyMesh->verts[pb[jb] * 3];
                                    float const vbx = bmin[0] + vb[0]*csz, vby = bmin[1] + vb[1]*chh, vbz = bmin[2] + vb[2]*csz;
                                    if (std::fabs(vay - vby) > kBridgeMaxVert) continue;
                                    float const dx = vax - vbx, dz = vaz - vbz, d = dx*dx + dz*dz;
                                    if (d < bestv) { bestv = d; ea[0]=vax;ea[1]=vay;ea[2]=vaz; eb[0]=vbx;eb[1]=vby;eb[2]=vbz; }
                                }
                            }
                            bool const budget_ok =
                                bridged < kMaxBridgesPerTile &&
                                (iv.polyMesh->nverts + (baseCons + bridged + 1) * 2) < 0xff00;
                            if (budget_ok && bestv <= kBridgeMaxHorizSq)
                            {
                                // Nudge each endpoint ~1.5y toward its poly centroid.
                                auto nudge = [](float* e, float gx, float gy, float gz) {
                                    float const dx = gx - e[0], dy = gy - e[1], dz = gz - e[2];
                                    float const len = std::sqrt(dx*dx + dy*dy + dz*dz);
                                    if (len > 0.01f) { float const t = (len < 1.5f ? len : 1.5f) / len;
                                        e[0] += dx*t; e[1] += dy*t; e[2] += dz*t; }
                                };
                                nudge(ea, cx[bestIso[c]],  cy[bestIso[c]],  cz[bestIso[c]]);
                                nudge(eb, cx[bestMain[c]], cy[bestMain[c]], cz[bestMain[c]]);
                                meshData.offMeshConnections.push_back(ea[0]);
                                meshData.offMeshConnections.push_back(ea[1]);
                                meshData.offMeshConnections.push_back(ea[2]);
                                meshData.offMeshConnections.push_back(eb[0]);
                                meshData.offMeshConnections.push_back(eb[1]);
                                meshData.offMeshConnections.push_back(eb[2]);
                                meshData.offMeshConnectionRads.push_back(2.0f);
                                meshData.offMeshConnectionDirs.push_back(1);   // bidirectional
                                meshData.offMeshConnectionsAreas.push_back(NAV_AREA_GROUND);
                                meshData.offMeshConnectionsFlags.push_back(NAV_GROUND);
                                ++bridged;
                                bridged_this = true;
                            }
                        }
                        if (!bridged_this && comp_sizes[c] < 10)
                        {
                            for (int i = 0; i < npolys; ++i)
                                if (comp_id[i] == c)
                                {
                                    iv.polyMesh->areas[i] = 0;
                                    iv.polyMesh->flags[i] = 0;
                                    ++pruned;
                                }
                        }
                        // else: large + unbridgeable component — leave intact.
                    }
                }
                if (pruned > 0 || bridged > 0)
                {
                    TC_LOG_DEBUG("maps.mmapgen", "{} Island pass: bridged {}, pruned {} ({} components, largest={})",
                        tileString, bridged, pruned, num_comps, largest_sz);
                }
            }
        }

        // setup mesh parameters
        dtNavMeshCreateParams params = {};
        params.verts = iv.polyMesh->verts;
        params.vertCount = iv.polyMesh->nverts;
        params.polys = iv.polyMesh->polys;
        params.polyAreas = iv.polyMesh->areas;
        params.polyFlags = iv.polyMesh->flags;
        params.polyCount = iv.polyMesh->npolys;
        params.nvp = iv.polyMesh->nvp;
        params.detailMeshes = iv.polyMeshDetail->meshes;
        params.detailVerts = iv.polyMeshDetail->verts;
        params.detailVertsCount = iv.polyMeshDetail->nverts;
        params.detailTris = iv.polyMeshDetail->tris;
        params.detailTriCount = iv.polyMeshDetail->ntris;

        params.offMeshConVerts = meshData.offMeshConnections.data();
        params.offMeshConCount = meshData.offMeshConnections.size() / 6;
        params.offMeshConRad = meshData.offMeshConnectionRads.data();
        params.offMeshConDir = meshData.offMeshConnectionDirs.data();
        params.offMeshConAreas = meshData.offMeshConnectionsAreas.data();
        params.offMeshConFlags = meshData.offMeshConnectionsFlags.data();

        params.walkableHeight = BASE_UNIT_DIM * config.walkableHeight;    // agent height
        params.walkableRadius = BASE_UNIT_DIM * config.walkableRadius;    // agent radius
        params.walkableClimb = BASE_UNIT_DIM * config.walkableClimb;      // keep less that walkableHeight (aka agent height)!
        params.tileX = (((bmin[0] + bmax[0]) / 2) - navMeshParams->orig[0]) / GRID_SIZE;
        params.tileY = (((bmin[2] + bmax[2]) / 2) - navMeshParams->orig[2]) / GRID_SIZE;
        rcVcopy(params.bmin, bmin);
        rcVcopy(params.bmax, bmax);
        params.cs = config.cs;
        params.ch = config.ch;
        params.tileLayer = 0;
        params.buildBvTree = true;

        // will hold final navmesh
        unsigned char* navData = nullptr;

        auto debugOutputWriter = Trinity::make_unique_ptr_with_deleter(m_debugOutput ? &iv : nullptr,
            [borderSize = static_cast<unsigned short>(config.borderSize),
            outputDir = &m_outputDirectory, fileNameSuffix,
            mapID, tileX, tileY, &meshData](IntermediateValues* intermediate)
        {
            // restore padding so that the debug visualization is correct
            for (std::ptrdiff_t i = 0; i < intermediate->polyMesh->nverts; ++i)
            {
                unsigned short* v = &intermediate->polyMesh->verts[i * 3];
                v[0] += borderSize;
                v[2] += borderSize;
            }

            intermediate->generateObjFile(*outputDir, fileNameSuffix, mapID, tileX, tileY, meshData);
            intermediate->writeIV(*outputDir, fileNameSuffix, mapID, tileX, tileY);
        });

        // these values are checked within dtCreateNavMeshData - handle them here
        // so we have a clear error message
        if (params.nvp > DT_VERTS_PER_POLYGON)
        {
            TC_LOG_ERROR("maps.mmapgen", "{} Invalid verts-per-polygon value!", tileString);
            return tileResult;
        }

        if (params.vertCount >= 0xffff)
        {
            TC_LOG_ERROR("maps.mmapgen", "{} Too many vertices!", tileString);
            return tileResult;
        }

        if (!params.vertCount || !params.verts)
        {
            // occurs mostly when adjacent tiles have models
            // loaded but those models don't span into this tile

            // message is an annoyance
            //TC_LOG_ERROR("maps.mmapgen", "{} No vertices to build tile!", tileString);
            return tileResult;
        }

        if (!params.polyCount || !params.polys)
        {
            // we have flat tiles with no actual geometry - don't build those, its useless
            // keep in mind that we do output those into debug info
            TC_LOG_ERROR("maps.mmapgen", "{} No polygons to build on tile!", tileString);
            return tileResult;
        }

        if (!params.detailMeshes || !params.detailVerts || !params.detailTris)
        {
            TC_LOG_ERROR("maps.mmapgen", "{} No detail mesh to build tile!", tileString);
            return tileResult;
        }

        TC_LOG_DEBUG("maps.mmapgen", "{} Building navmesh tile...", tileString);
        if (!dtCreateNavMeshData(&params, &navData, &tileResult.size))
        {
            // SAFETY NET: the auto-bridge off-mesh connections can push a dense
            // tile past Detour's per-tile budget, failing creation. Never drop
            // the tile over a best-effort bridge — retry without off-mesh
            // connections so navmesh coverage is preserved exactly as before
            // the bridge feature (worst case: this one tile has no bridges).
            if (params.offMeshConCount > 0)
            {
                params.offMeshConVerts = nullptr;
                params.offMeshConRad   = nullptr;
                params.offMeshConDir   = nullptr;
                params.offMeshConAreas = nullptr;
                params.offMeshConFlags = nullptr;
                params.offMeshConCount = 0;
                if (dtCreateNavMeshData(&params, &navData, &tileResult.size))
                {
                    TC_LOG_DEBUG("maps.mmapgen", "{} Built without auto-bridges (off-mesh budget overflow).", tileString);
                    tileResult.data.reset(navData);
                    return tileResult;
                }
            }
            TC_LOG_ERROR("maps.mmapgen", "{} Failed building navmesh tile!", tileString);
            return tileResult;
        }

        tileResult.data.reset(navData);
        return tileResult;
    }

    void TileBuilder::saveMoveMapTileToFile(uint32 mapID, uint32 tileX, uint32 tileY, dtNavMesh* navMesh,
        TileResult const& tileResult, std::string_view fileNameSuffix)
    {
        dtTileRef tileRef = 0;
        auto navMeshTile = Trinity::make_unique_ptr_with_deleter<dtTileRef*>(nullptr, [navMesh](dtTileRef const* ref)
        {
            navMesh->removeTile(*ref, nullptr, nullptr);
        });

        if (navMesh)
        {
            TC_LOG_DEBUG("maps.mmapgen", "[Map {:04}] [{:02},{:02}]: Adding tile to navmesh...", mapID, tileX, tileY);
            // DT_TILE_FREE_DATA tells detour to unallocate memory when the tile
            // is removed via removeTile()
            dtStatus dtResult = navMesh->addTile(tileResult.data.get(), tileResult.size, 0, 0, &tileRef);
            if (!tileRef || !dtStatusSucceed(dtResult))
            {
                TC_LOG_ERROR("maps.mmapgen", "[Map {:04}] [{:02},{:02}]: Failed adding tile to navmesh!", mapID, tileX, tileY);
                return;
            }

            navMeshTile.reset(&tileRef);
        }

        // file output
        std::string fileName = Trinity::StringFormat("{}/mmaps/{:04}_{:02}_{:02}{}.mmtile", m_outputDirectory.generic_string(), mapID, tileX, tileY, fileNameSuffix);
        auto file = Trinity::make_unique_ptr_with_deleter<&::fclose>(fopen(fileName.c_str(), "wb"));
        if (!file)
        {
            TC_LOG_ERROR("maps.mmapgen", "[Map {:04}] [{:02},{:02}]: {}: Failed to open {} for writing!", mapID, tileX, tileY, strerror(errno), fileName);
            return;
        }

        TC_LOG_DEBUG("maps.mmapgen", "[Map {:04}] [{:02},{:02}]: Writing to file...", mapID, tileX, tileY);

        // write header
        MmapTileHeader header;
        header.usesLiquids = m_terrainBuilder.usesLiquids();
        header.size = uint32(tileResult.size);
        fwrite(&header, sizeof(MmapTileHeader), 1, file.get());

        // write data
        fwrite(tileResult.data.get(), sizeof(unsigned char), tileResult.size, file.get());
    }

    /**************************************************************************/
    void TileBuilder::getTileBounds(uint32 tileX, uint32 tileY, float const* verts, std::size_t vertCount, float* bmin, float* bmax)
    {
        // this is for elevation
        if (verts && vertCount)
            rcCalcBounds(verts, int(vertCount), bmin, bmax);
        else
        {
            bmin[1] = FLT_MIN;
            bmax[1] = FLT_MAX;
        }

        // this is for width and depth
        bmax[0] = (32 - int(tileY)) * GRID_SIZE;
        bmax[2] = (32 - int(tileX)) * GRID_SIZE;
        bmin[0] = bmax[0] - GRID_SIZE;
        bmin[2] = bmax[2] - GRID_SIZE;
    }

    /**************************************************************************/
    bool TileBuilder::shouldSkipTile(uint32 /*mapID*/, uint32 /*tileX*/, uint32 /*tileY*/) const
    {
        if (m_debugOutput)
            return false;

        return true;
    }

    rcConfig TileBuilder::GetMapSpecificConfig(uint32 mapID, float const (&bmin)[3], float const (&bmax)[3], TileConfig const& tileConfig) const
    {
        rcConfig config { };

        rcVcopy(config.bmin, bmin);
        rcVcopy(config.bmax, bmax);

        config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
        // cs/ch coupled to BASE_UNIT_DIM by default. --cellSize sets an explicit
        // value; --fineCells halves it (4x voxel density) for the resolution probe.
        {
            float cs = tileConfig.BASE_UNIT_DIM;
            if (g_mmapGenTuning.cellSize > 0.f)
                cs = g_mmapGenTuning.cellSize;
            else if (g_mmapGenTuning.fineCells)
                cs *= 0.5f;
            config.cs = cs;
            config.ch = cs;
        }
        // Keeping these 2 slope angles the same reduces a lot the number of polys.
        // 55 should be the minimum, maybe 70 is ok (keep in mind blink uses mmaps), 85 is too much for players
        config.walkableSlopeAngle = m_maxWalkableAngle.value_or(55.0f);
        config.walkableSlopeAngleNotSteep = m_maxWalkableAngleNotSteep.value_or(55.0f);
        config.tileSize = tileConfig.VERTEX_PER_TILE;
        config.walkableRadius = m_bigBaseUnit ? 1 : 2;
        config.borderSize = config.walkableRadius + 3;
        config.maxEdgeLen = tileConfig.VERTEX_PER_TILE + 1;        // anything bigger than tileSize
        config.walkableHeight = m_bigBaseUnit ? 3 : 6;
        // Walkable climb — vertical step a navmesh agent can ascend in
        // one voxel column. Recast.h: "value >= 4|8 allows npcs to walk
        // over all fences". Bumped 2026-05-21 from 6 → 10 (1.6y → 2.67y)
        // because spiral-stair WMOs (Aldrassil) have ramp-entry step
        // rises ~2y that the 1.6y climb couldn't bridge — ground polys
        // never linked to the first ramp poly, so even with the right
        // partition method the navmesh was a disconnected graph.
        // Trade-off: bots may climb low fences/walls (≤2.67y) they
        // previously couldn't. This is a known acceptable behavior in
        // most Blizzard content where short walls are intentional
        // hop-overs (1-yard walls in BG bases, garden edges, etc).
        config.walkableClimb = m_bigBaseUnit ? 5 : 10;
        // Region-area thresholds — tuned 2026-05-21 after the Aldrassil
        // navmesh investigation (see WMO_COLLIDE_HIT_FIX.md).
        //
        // Previous values rcSqr(60)=3600 vx² / rcSqr(50)=2500 vx² are 50×
        // larger than the upstream Recast demo defaults (rcSqr(8)=64).
        // At cs=0.266 yards, 3600 vx² ≈ 256 yards² — anything smaller got
        // discarded as "noise". That filtered out:
        //   * Aldrassil's top floor (Tenaron Stormgrip's room, ~36 yards²)
        //   * Every elevated single-room poly in towers and dungeons
        //   * Most small ledges and platforms
        // The discarded regions appeared in the pmesh as singleton 1-poly
        // regions disconnected from the surrounding navmesh, so Detour
        // could find them via findNearestPoly but couldn't path TO them
        // (no graph edges into the region).
        //
        // Lowered to rcSqr(12)² = 144 / rcSqr(20)² = 400 — matches the
        // RecastDemo "indoor" sample profile while staying conservative
        // vs. the 64/400 demo defaults. Trade-off: marginally noisier
        // navmesh on rooftops / awnings / small unwalkable platforms,
        // but those filter out via the higher-level walkability gate
        // anyway. The 30× reduction in minRegionArea unlocks every
        // small-room interior navmesh that was previously trimmed.
        config.minRegionArea = rcSqr(12);
        config.mergeRegionArea = rcSqr(20);
        config.maxSimplificationError = 1.8f;           // eliminates most jagged edges (tiny polygons)
        config.detailSampleDist = config.cs * 16;
        config.detailSampleMaxError = config.ch * 1;

        // --------------------------------------------------------------------
        // CLI sweep overrides (g_mmapGenTuning). Applied AFTER the baseline
        // literals but BEFORE the per-map switch(mapID) below, so intentional
        // map-specific special cases (map 48 ch*2, map 562 radius=0, etc.) still
        // win over a global sweep flag. Sentinels (<0 / false) leave the default.
        // Values are raw voxel counts / area-in-vx² (e.g. --minRegionArea 144).
        // --------------------------------------------------------------------
        if (g_mmapGenTuning.minRegionArea >= 0)
            config.minRegionArea = g_mmapGenTuning.minRegionArea;
        if (g_mmapGenTuning.mergeRegionArea >= 0)
            config.mergeRegionArea = g_mmapGenTuning.mergeRegionArea;   // inert under Layers
        if (g_mmapGenTuning.maxSimplificationError >= 0.f)
            config.maxSimplificationError = g_mmapGenTuning.maxSimplificationError;
        if (g_mmapGenTuning.maxEdgeLen >= 0)
            config.maxEdgeLen = g_mmapGenTuning.maxEdgeLen;
        if (g_mmapGenTuning.walkableClimb >= 0)
            config.walkableClimb = g_mmapGenTuning.walkableClimb;
        if (g_mmapGenTuning.walkableRadius >= 0)
        {
            config.walkableRadius = g_mmapGenTuning.walkableRadius;
            config.borderSize = config.walkableRadius + 3;   // borderSize tracks radius
        }
        if (g_mmapGenTuning.detailSampleDist >= 0.f)
            config.detailSampleDist = g_mmapGenTuning.detailSampleDist;
        if (g_mmapGenTuning.detailSampleMaxError >= 0.f)
            config.detailSampleMaxError = g_mmapGenTuning.detailSampleMaxError;

        switch (mapID)
        {
            // Blade's Edge Arena
            case 562:
                // This allows to walk on the ropes to the pillars
                config.walkableRadius = 0;
                break;
            // Blackfathom Deeps
            case 48:
                // Reduce the chance to have underground levels
                config.ch *= 2;
                break;
            default:
                break;
        }

        return config;
    }

    std::string TileBuilder::GetProgressText() const
    {
        return "";
    }
}
