/*
 * RoadOverlayBuilder - see header for design.
 *
 * Algorithm (one pass over every tile, one inner pass over every poly):
 *
 *   1. Build a per-tile centroid table for every poly whose area is
 *      NAV_AREA_ROAD.  Centroid is the unweighted XY-mean of the
 *      polygon's vertices, in TC world coords (X = north, Y = west).
 *
 *   2. For each road polygon, walk its edges.  For each edge whose
 *      neighbour is *also* a road polygon (intra-tile via neis[e] or
 *      cross-tile via the firstLink chain), emit a (centroid_self,
 *      centroid_other) pair as two consecutive vertices in the GL_LINES
 *      buffer.
 *
 *   3. Edges are visited from both sides so every (a -> b) pair is also
 *      walked as (b -> a); de-duplication is not attempted because the
 *      vertex count is small (a continent typically holds <50k road
 *      polys, so worst-case <300k vertices = ~2.4MB on the GPU) and
 *      double-drawing two-pixel-thick lines is visually identical to
 *      drawing them once at the same width.
 */

#include "RoadOverlayBuilder.h"

#include <DetourNavMesh.h>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace world_editor::render
{

namespace
{

// Must match src/common/mmaps_common/MMapDefines.h NAV_AREA_ROAD and the
// constant used in NavMeshView.cpp / SceneView3D.cpp / mmap_world_dump.cpp.
constexpr uint8_t kNavAreaRoad = 7;

// Compute the TC-world XY centroid for a poly inside `tile`.  Detour
// frame: verts[0] = TC Y (west), verts[2] = TC X (north).
inline QVector2D polyCentroidTcXY(dtMeshTile const* tile, dtPoly const& poly)
{
    float sumX = 0.0f;
    float sumY = 0.0f;
    int const nv = poly.vertCount;
    if (nv <= 0)
        return QVector2D(0.0f, 0.0f);
    for (int i = 0; i < nv; ++i)
    {
        float const* v = &tile->verts[poly.verts[i] * 3];
        // dt[0] = TC Y, dt[2] = TC X.
        sumX += v[2];
        sumY += v[0];
    }
    float const inv = 1.0f / float(nv);
    return QVector2D(sumX * inv, sumY * inv);
}

} // namespace

void RoadOverlayBuilder::buildFromNavmesh(dtNavMesh const* nm)
{
    m_vertices.clear();
    if (!nm)
        return;

    int const maxTiles = nm->getMaxTiles();

    // Per-tile cache: poly-index -> centroid.  Only road polys land in the
    // map; the rest are skipped.  Keyed by tile index to avoid recomputing
    // centroids when chasing cross-tile neighbours via decodePolyId.
    //
    // unordered_map<int, vector<...>> would over-allocate; we store one
    // flat vector per tile lazily.
    std::vector<std::vector<std::pair<int, QVector2D>>> tileRoadCentroids;
    tileRoadCentroids.resize(size_t(maxTiles));

    auto centroidFor = [&](int tileIdx, dtMeshTile const* tile, int polyIdx) -> QVector2D
    {
        auto& v = tileRoadCentroids[size_t(tileIdx)];
        for (auto const& kv : v)
            if (kv.first == polyIdx)
                return kv.second;
        QVector2D const c = polyCentroidTcXY(tile, tile->polys[polyIdx]);
        v.emplace_back(polyIdx, c);
        return c;
    };

    // Reserve a generous upper bound to keep the line buffer from
    // reallocating mid-build on big continents.  Final size is typically
    // a small multiple of the road-poly count.
    m_vertices.reserve(size_t(maxTiles) * 16);

    for (int ti = 0; ti < maxTiles; ++ti)
    {
        dtMeshTile const* tile = nm->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount <= 0)
            continue;

        int const polyCount = tile->header->polyCount;
        for (int p = 0; p < polyCount; ++p)
        {
            dtPoly const& poly = tile->polys[p];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;
            if (poly.getArea() != kNavAreaRoad)
                continue;
            int const nv = poly.vertCount;
            if (nv < 3)
                continue;

            QVector2D const cSelf = centroidFor(ti, tile, p);

            for (int e = 0; e < nv; ++e)
            {
                unsigned short const nei = poly.neis[e];
                if (nei == 0)
                    continue;  // open edge (no neighbour at all)

                if (nei & DT_EXT_LINK)
                {
                    // Cross-tile neighbour.  Walk the firstLink chain;
                    // every link whose `edge` matches `e` is a connection
                    // through this poly's edge into another tile.
                    unsigned int linkIdx = poly.firstLink;
                    while (linkIdx != DT_NULL_LINK)
                    {
                        dtLink const& link = tile->links[linkIdx];
                        if (link.edge == e)
                        {
                            dtMeshTile const* otherTile = nullptr;
                            dtPoly const*     otherPoly = nullptr;
                            nm->getTileAndPolyByRefUnsafe(link.ref, &otherTile, &otherPoly);
                            if (otherTile && otherPoly
                                && otherPoly->getType() != DT_POLYTYPE_OFFMESH_CONNECTION
                                && otherPoly->getArea() == kNavAreaRoad)
                            {
                                // Decode neighbour tile index so the centroid
                                // cache slot is reused on subsequent visits.
                                unsigned int salt = 0, otherTileIdx = 0, otherPolyIdx = 0;
                                nm->decodePolyId(link.ref, salt, otherTileIdx, otherPolyIdx);
                                if (int(otherTileIdx) >= 0 && int(otherTileIdx) < maxTiles)
                                {
                                    QVector2D const cOther = centroidFor(int(otherTileIdx),
                                                                         otherTile,
                                                                         int(otherPolyIdx));
                                    m_vertices.push_back(cSelf);
                                    m_vertices.push_back(cOther);
                                }
                            }
                        }
                        linkIdx = link.next;
                    }
                }
                else
                {
                    // Intra-tile neighbour: nei is (1-based) poly index + 1.
                    int const neighbourIdx = int(nei) - 1;
                    if (neighbourIdx < 0 || neighbourIdx >= polyCount)
                        continue;
                    dtPoly const& neiPoly = tile->polys[neighbourIdx];
                    if (neiPoly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                        continue;
                    if (neiPoly.getArea() != kNavAreaRoad)
                        continue;
                    QVector2D const cOther = centroidFor(ti, tile, neighbourIdx);
                    m_vertices.push_back(cSelf);
                    m_vertices.push_back(cOther);
                }
            }
        }
    }
}

} // namespace world_editor::render
