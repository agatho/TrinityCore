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

#include "tc_catch2.h"

#include "RoadCorridor.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourAlloc.h"
#include "MMapDefines.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace Road;

// =============================================================================
// Invariant: NAV_AREA_ROAD must remain 7. RoadCorridor.cpp hard-codes the value
// locally to avoid pulling MMapDefines.h into extractor_common; this test fires
// if anyone ever rebumps the constant.
// =============================================================================

TEST_CASE("RoadCorridor - NAV_AREA_ROAD invariant is 7", "[RoadCorridor]")
{
    REQUIRE(static_cast<unsigned char>(NAV_AREA_ROAD) == 7);
}

// =============================================================================
// Detail::PointSegmentDistSq2D — point-to-segment distance math.
// All segment / point coordinates here are in the Detour XZ plane (the
// helper is frame-agnostic; same math is used to test the TC->Detour
// rotation at the API layer below).
// =============================================================================

TEST_CASE("RoadCorridor - point on segment yields distance 0", "[RoadCorridor]")
{
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 0, 50, 0);
    REQUIRE(d2 == Approx(0.0f));
}

TEST_CASE("RoadCorridor - point at perpendicular distance 4 from horizontal segment",
          "[RoadCorridor]")
{
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 0, 50, 4);
    REQUIRE(std::sqrt(d2) == Approx(4.0f));
}

TEST_CASE("RoadCorridor - point at perpendicular distance 6 is OUTSIDE width=10",
          "[RoadCorridor]")
{
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 0, 50, 6);
    // halfWidth = 5; 6 > 5 → outside.
    REQUIRE(std::sqrt(d2) == Approx(6.0f));
    REQUIRE(d2 > 5.0f * 5.0f);
}

TEST_CASE("RoadCorridor - endpoint clamp (point before start)", "[RoadCorridor]")
{
    // (-5, 0) is before start (0,0). Closest point is start; distance = 5.
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 0, -5, 0);
    REQUIRE(std::sqrt(d2) == Approx(5.0f));
    REQUIRE(d2 > 0.0f);   // explicitly NOT zero — endpoint clamp prevents
                          // an off-corridor point being projected onto the
                          // segment line and counting as in-corridor.
}

TEST_CASE("RoadCorridor - endpoint clamp (point past end)", "[RoadCorridor]")
{
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 0, 105, 0);
    REQUIRE(std::sqrt(d2) == Approx(5.0f));
}

TEST_CASE("RoadCorridor - inclined segment, point on line is in corridor",
          "[RoadCorridor]")
{
    // 45-degree line through origin. (50, 50) is exactly on it.
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 100, 50, 50);
    REQUIRE(d2 == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("RoadCorridor - inclined segment, perpendicular offset distance",
          "[RoadCorridor]")
{
    // 45-degree line y=x. Point (50, 60) projects onto (55, 55) on the
    // line; perpendicular distance = sqrt((50-55)^2 + (60-55)^2)
    //                              = sqrt(25+25) = sqrt(50) ≈ 7.071.
    float const d2 = Detail::PointSegmentDistSq2D(0, 0, 100, 100, 50, 60);
    REQUIRE(std::sqrt(d2) == Approx(std::sqrt(50.0f)).margin(1e-4f));
    // With width=10 (halfWidth=5), 7.07 > 5 → NOT in corridor.
    REQUIRE(d2 > 5.0f * 5.0f);
}

TEST_CASE("RoadCorridor - degenerate zero-length segment", "[RoadCorridor]")
{
    // Both endpoints coincide. Distance reduces to point-to-point.
    float const d2 = Detail::PointSegmentDistSq2D(10, 10, 10, 10, 13, 14);
    REQUIRE(std::sqrt(d2) == Approx(5.0f));
}

TEST_CASE("RoadCorridor - TC->Detour coord swap", "[RoadCorridor]")
{
    // TC (X=north-south, Y=west-east); Detour stores (Y, _, X) so Detour[0]
    // gets TC Y and Detour[2] gets TC X. Cf PathGenerator.cpp:189.
    Detail::DetourXZ const d = Detail::TcToDetour(/*tcX=*/123.0f, /*tcY=*/456.0f);
    REQUIRE(d.x == Approx(456.0f));  // Detour[0] = TC Y
    REQUIRE(d.z == Approx(123.0f));  // Detour[2] = TC X
}

// =============================================================================
// ScanCorridor / Apply* — exercise the public API against a fabricated
// dtNavMesh. We build a single tile with a 4x4 grid of square polygons
// (each 10x10 in the XZ plane) using dtCreateNavMeshData and add it.
// =============================================================================

namespace
{
    // Build a tile spanning Detour XZ rect [0, gridSize*cells] x [0, gridSize*cells]
    // with `cells` x `cells` axis-aligned square polygons of edge `gridSize`.
    // Polygons are NAV_AREA_GROUND initially.
    //
    // Vertices are placed on a (cells+1) x (cells+1) grid. Each polygon is a
    // quad (4 verts), nvp=6 for headroom.
    struct BuiltMesh
    {
        dtNavMesh*     mesh        = nullptr;
        unsigned char* tileData    = nullptr;  // owned by dtNavMesh after addTile
        int            tileDataSize = 0;
        int            cells       = 0;
        float          gridSize    = 0.0f;
    };

    BuiltMesh BuildGridMesh(int cells = 4, float gridSize = 10.0f)
    {
        BuiltMesh out;
        out.cells = cells;
        out.gridSize = gridSize;

        int const nvp = 6;
        int const vertsPerSide = cells + 1;
        int const vertCount = vertsPerSide * vertsPerSide;
        int const polyCount = cells * cells;

        // Quantize verts to unsigned short in tile-local coords. Detour
        // treats verts[0..2] as ushort grid coords relative to bmin; the
        // header has cs=gridSize so each ushort unit = gridSize world units.
        std::vector<unsigned short> verts(vertCount * 3);
        for (int z = 0; z < vertsPerSide; ++z)
        {
            for (int x = 0; x < vertsPerSide; ++x)
            {
                int const i = (z * vertsPerSide + x) * 3;
                verts[i + 0] = static_cast<unsigned short>(x);
                verts[i + 1] = 0;  // y (height) — flat plane.
                verts[i + 2] = static_cast<unsigned short>(z);
            }
        }

        // Polys: 4 verts each (the rest of nvp slots set to RC_MESH_NULL_IDX).
        // Each poly's vertex indices wind CCW in XZ (looking down +Y).
        std::vector<unsigned short> polys(polyCount * 2 * nvp,
                                          static_cast<unsigned short>(0xffff));  // RC_MESH_NULL_IDX
        for (int z = 0; z < cells; ++z)
        {
            for (int x = 0; x < cells; ++x)
            {
                int const pi = (z * cells + x) * 2 * nvp;
                unsigned short v00 = static_cast<unsigned short>( z      * vertsPerSide + x      );
                unsigned short v10 = static_cast<unsigned short>( z      * vertsPerSide + x + 1  );
                unsigned short v11 = static_cast<unsigned short>((z + 1) * vertsPerSide + x + 1  );
                unsigned short v01 = static_cast<unsigned short>((z + 1) * vertsPerSide + x      );
                polys[pi + 0] = v00;
                polys[pi + 1] = v10;
                polys[pi + 2] = v11;
                polys[pi + 3] = v01;
                // neighbour slots [nvp..2*nvp) left at 0xffff — no portals.
            }
        }

        std::vector<unsigned short> polyFlags(polyCount, 1);
        std::vector<unsigned char>  polyAreas(polyCount, 1);  // NAV_AREA_GROUND

        // Skip explicit detail mesh — leave detailMeshes null so
        // dtCreateNavMeshData auto-fans each polygon into triangles from
        // its own verts (no extra detail vertices). This matches the
        // "no input detail mesh" branch in DetourNavMeshBuilder.cpp.

        dtNavMeshCreateParams params{};
        params.verts = verts.data();
        params.vertCount = vertCount;
        params.polys = polys.data();
        params.polyFlags = polyFlags.data();
        params.polyAreas = polyAreas.data();
        params.polyCount = polyCount;
        params.nvp = nvp;
        params.detailMeshes = nullptr;
        params.detailVerts = nullptr;
        params.detailVertsCount = 0;
        params.detailTris = nullptr;
        params.detailTriCount = 0;
        params.walkableHeight = 2.0f;
        params.walkableRadius = 0.6f;
        params.walkableClimb = 1.0f;
        params.cs = gridSize;
        params.ch = 1.0f;
        params.bmin[0] = 0.0f;
        params.bmin[1] = 0.0f;
        params.bmin[2] = 0.0f;
        params.bmax[0] = gridSize * cells;
        params.bmax[1] = 1.0f;
        params.bmax[2] = gridSize * cells;
        params.buildBvTree = false;
        params.tileX = 0;
        params.tileY = 0;
        params.tileLayer = 0;

        if (!dtCreateNavMeshData(&params, &out.tileData, &out.tileDataSize))
            return out;

        out.mesh = dtAllocNavMesh();
        dtNavMeshParams nmParams{};
        nmParams.orig[0] = 0.0f;
        nmParams.orig[1] = 0.0f;
        nmParams.orig[2] = 0.0f;
        nmParams.tileWidth  = gridSize * cells;
        nmParams.tileHeight = gridSize * cells;
        nmParams.maxTiles = 1;
        nmParams.maxPolys = polyCount;
        if (dtStatusFailed(out.mesh->init(&nmParams)))
        {
            dtFreeNavMesh(out.mesh);
            out.mesh = nullptr;
            dtFree(out.tileData);
            out.tileData = nullptr;
            return out;
        }
        if (dtStatusFailed(out.mesh->addTile(out.tileData, out.tileDataSize,
                                             DT_TILE_FREE_DATA, 0, nullptr)))
        {
            dtFreeNavMesh(out.mesh);
            out.mesh = nullptr;
            out.tileData = nullptr;  // ownership reverts only on success; addTile failed.
            return out;
        }
        // After successful addTile with DT_TILE_FREE_DATA, the navmesh owns
        // the data. Null our pointer so we don't double-free.
        out.tileData = nullptr;
        return out;
    }

    void DestroyMesh(BuiltMesh& m)
    {
        if (m.mesh) dtFreeNavMesh(m.mesh);
        m.mesh = nullptr;
    }

    // Helper: count how many polygons in the mesh currently have area==target.
    int CountPolysWithArea(dtNavMesh const& nm, unsigned char target)
    {
        int n = 0;
        for (int t = 0; t < nm.getMaxTiles(); ++t)
        {
            dtMeshTile const* tile = nm.getTile(t);
            if (!tile || !tile->header) continue;
            for (int p = 0; p < tile->header->polyCount; ++p)
                if (tile->polys[p].getArea() == target)
                    ++n;
        }
        return n;
    }
}

TEST_CASE("RoadCorridor - ScanCorridor on empty navmesh returns 0 polys",
          "[RoadCorridor]")
{
    dtNavMesh* nm = dtAllocNavMesh();
    REQUIRE(nm != nullptr);
    dtNavMeshParams params{};
    params.tileWidth = 100.0f;
    params.tileHeight = 100.0f;
    params.maxTiles = 1;
    params.maxPolys = 1;
    REQUIRE_FALSE(dtStatusFailed(nm->init(&params)));

    Segment seg{ 0, 0, 100, 0, 10 };
    CorridorResult r = ScanCorridor(*nm, seg);
    REQUIRE(r.polyRefs.empty());
    REQUIRE(r.tilesScanned == 0);
    REQUIRE(r.polysExamined == 0);

    dtFreeNavMesh(nm);
}

TEST_CASE("RoadCorridor - ScanCorridor on completely-uninitialized navmesh is safe",
          "[RoadCorridor]")
{
    dtNavMesh* nm = dtAllocNavMesh();
    REQUIRE(nm != nullptr);
    // Note: deliberately NOT calling init(). getMaxTiles() should return 0.
    Segment seg{ 0, 0, 100, 0, 10 };
    CorridorResult r = ScanCorridor(*nm, seg);
    REQUIRE(r.polyRefs.empty());
    dtFreeNavMesh(nm);
}

TEST_CASE("RoadCorridor - ScanCorridor finds polys under a horizontal segment",
          "[RoadCorridor]")
{
    // Grid: 4x4 cells of 10x10. Cell centers are at (5, 5), (15, 5), (25, 5),
    // (35, 5) for the bottom row in Detour XZ. NAV_AREA stored on each.
    BuiltMesh m = BuildGridMesh(4, 10.0f);
    REQUIRE(m.mesh != nullptr);

    // TC frame: TC X = Detour Z, TC Y = Detour X. We want a segment along
    // Detour X (covering all four polys in the bottom row, z in [0,10]).
    // In TC: fromY=0, toY=40 (Detour X 0→40); fromX=5, toX=5 (Detour Z=5).
    Segment seg{ /*fromX*/ 5.0f, /*fromY*/ 0.0f,
                 /*toX*/   5.0f, /*toY*/   40.0f,
                 /*width*/ 6.0f };  // half=3 → catches Z in [2,8] which covers
                                    // centroid z=5 of bottom row only.

    CorridorResult r = ScanCorridor(*m.mesh, seg);
    REQUIRE(r.tilesScanned == 1);
    REQUIRE(r.polyRefs.size() == 4u);  // exactly the bottom row.

    DestroyMesh(m);
}

TEST_CASE("RoadCorridor - ScanCorridor maxResults caps result count",
          "[RoadCorridor]")
{
    BuiltMesh m = BuildGridMesh(4, 10.0f);
    REQUIRE(m.mesh != nullptr);

    // Wide corridor covering the whole grid (width 100 >> grid extent 40).
    Segment seg{ 20.0f, 0.0f, 20.0f, 40.0f, 100.0f };

    CorridorResult full = ScanCorridor(*m.mesh, seg);
    REQUIRE(full.polyRefs.size() == 16u);

    CorridorResult capped = ScanCorridor(*m.mesh, seg, /*maxResults*/ 5);
    REQUIRE(capped.polyRefs.size() == 5u);

    DestroyMesh(m);
}

TEST_CASE("RoadCorridor - ScanCorridorsBatch dedupes overlapping segments",
          "[RoadCorridor]")
{
    BuiltMesh m = BuildGridMesh(4, 10.0f);
    REQUIRE(m.mesh != nullptr);

    // Two segments covering the bottom row. They are spatially identical
    // and should yield the SAME 4 polys; batch must dedupe to 4 entries
    // (not 8) and return them sorted.
    Segment a{ 5.0f, 0.0f, 5.0f, 40.0f, 6.0f };
    Segment b{ 5.0f, 0.0f, 5.0f, 40.0f, 6.0f };
    std::vector<Segment> segs{ a, b };
    auto merged = ScanCorridorsBatch(*m.mesh, segs);
    REQUIRE(merged.size() == 4u);
    REQUIRE(std::is_sorted(merged.begin(), merged.end()));

    DestroyMesh(m);
}

TEST_CASE("RoadCorridor - ApplyCorridorToNavmesh flips area to NAV_AREA_ROAD",
          "[RoadCorridor]")
{
    BuiltMesh m = BuildGridMesh(4, 10.0f);
    REQUIRE(m.mesh != nullptr);

    REQUIRE(CountPolysWithArea(*m.mesh, NAV_AREA_ROAD) == 0);
    REQUIRE(CountPolysWithArea(*m.mesh, 1 /*GROUND*/) == 16);

    Segment seg{ 5.0f, 0.0f, 5.0f, 40.0f, 6.0f };  // bottom row only
    std::size_t tagged = ApplyCorridorToNavmesh(*m.mesh, seg);
    REQUIRE(tagged == 4u);
    REQUIRE(CountPolysWithArea(*m.mesh, NAV_AREA_ROAD) == 4);

    // Reapplying does not retag (already road) → 0 new flips.
    std::size_t reapplied = ApplyCorridorToNavmesh(*m.mesh, seg);
    REQUIRE(reapplied == 0u);
    REQUIRE(CountPolysWithArea(*m.mesh, NAV_AREA_ROAD) == 4);

    DestroyMesh(m);
}

TEST_CASE("RoadCorridor - ApplyCorridorsToNavmesh handles multiple segments",
          "[RoadCorridor]")
{
    BuiltMesh m = BuildGridMesh(4, 10.0f);
    REQUIRE(m.mesh != nullptr);

    // Two perpendicular segments forming a cross through the grid center.
    // Horizontal: covers row at Detour Z≈15 (TC X=15) across all cols.
    // Vertical: covers col at Detour X≈15 (TC Y=15) across all rows.
    Segment horiz{ /*fromX*/ 15.0f, /*fromY*/ 0.0f,
                   /*toX*/   15.0f, /*toY*/   40.0f, /*w*/ 6.0f };
    Segment vert { /*fromX*/ 0.0f,  /*fromY*/ 15.0f,
                   /*toX*/   40.0f, /*toY*/   15.0f, /*w*/ 6.0f };

    std::vector<Segment> segs{ horiz, vert };
    std::size_t tagged = ApplyCorridorsToNavmesh(*m.mesh, segs);
    // Horizontal tags 4 polys (one row); vertical tags 4 polys (one col).
    // They cross at exactly 1 poly. So new-flip count = 4 + (4 - 1) = 7.
    REQUIRE(tagged == 7u);
    REQUIRE(CountPolysWithArea(*m.mesh, NAV_AREA_ROAD) == 7);

    DestroyMesh(m);
}

TEST_CASE("RoadCorridor - Segment outside navmesh bounds tags nothing",
          "[RoadCorridor]")
{
    BuiltMesh m = BuildGridMesh(4, 10.0f);
    REQUIRE(m.mesh != nullptr);

    // Segment far away from the (0..40) tile.
    Segment seg{ 1000.0f, 1000.0f, 1100.0f, 1000.0f, 10.0f };
    CorridorResult r = ScanCorridor(*m.mesh, seg);
    REQUIRE(r.polyRefs.empty());
    REQUIRE(r.tilesScanned == 0);  // AABB rejected without polygon scan.

    DestroyMesh(m);
}
