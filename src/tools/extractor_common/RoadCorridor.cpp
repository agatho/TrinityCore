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

#include "RoadCorridor.h"

#include "DetourNavMesh.h"

#include <algorithm>
#include <cmath>

namespace Road
{
    namespace
    {
        // NAV_AREA_ROAD is also defined in src/common/mmaps_common/MMapDefines.h
        // but pulling that header into extractor_common would add a heavy
        // dependency. Keep a local constant in sync with the canonical value;
        // a build-time invariant test in tools_tests prevents drift.
        constexpr unsigned char kNavAreaRoad = 7;

        struct Point2 { float x, z; };  // Detour frame: x = Detour[0], z = Detour[2].

        // Convert a TC (X, Y) world point to a Detour-frame (X, Z) point.
        // TC (x, y, z) maps to Detour (y, z, x), so TC X -> Detour[2] and
        // TC Y -> Detour[0]. We only need the horizontal plane here.
        inline Point2 TcToDetour(float tcX, float tcY) noexcept
        {
            return { tcY, tcX };
        }

        // Squared distance from point p to the line segment [a, b] in the
        // Detour XZ plane.
        inline float PointSegmentDistSq2D(Point2 a, Point2 b, Point2 p) noexcept
        {
            float const dx = b.x - a.x;
            float const dz = b.z - a.z;
            float const lenSq = dx * dx + dz * dz;
            if (lenSq <= 0.0f)
            {
                float const ex = p.x - a.x;
                float const ez = p.z - a.z;
                return ex * ex + ez * ez;
            }
            float t = ((p.x - a.x) * dx + (p.z - a.z) * dz) / lenSq;
            if (t < 0.0f) t = 0.0f;
            else if (t > 1.0f) t = 1.0f;
            float const projX = a.x + t * dx;
            float const projZ = a.z + t * dz;
            float const ex = p.x - projX;
            float const ez = p.z - projZ;
            return ex * ex + ez * ez;
        }

        struct CorridorBox
        {
            float minX, maxX;
            float minZ, maxZ;
        };

        // Axis-aligned bounding box of the corridor (segment endpoints
        // inflated by half-width on each side) in the Detour XZ plane.
        inline CorridorBox MakeCorridorBox(Point2 a, Point2 b, float halfWidth) noexcept
        {
            CorridorBox box;
            box.minX = std::min(a.x, b.x) - halfWidth;
            box.maxX = std::max(a.x, b.x) + halfWidth;
            box.minZ = std::min(a.z, b.z) - halfWidth;
            box.maxZ = std::max(a.z, b.z) + halfWidth;
            return box;
        }

        // Test if the tile's XZ AABB overlaps the corridor box. y is ignored;
        // the navmesh is effectively 2D for our purposes.
        inline bool TileOverlapsCorridor(dtMeshTile const& tile, CorridorBox const& box) noexcept
        {
            dtMeshHeader const* hdr = tile.header;
            if (!hdr) return false;
            if (hdr->bmax[0] < box.minX) return false;
            if (hdr->bmin[0] > box.maxX) return false;
            if (hdr->bmax[2] < box.minZ) return false;
            if (hdr->bmin[2] > box.maxZ) return false;
            return true;
        }

        // Compute polygon centroid in Detour XZ plane, given an unsigned-short
        // vertex-index array of length `vertCount` referencing tile.verts.
        inline Point2 PolyCentroidXZ(dtMeshTile const& tile, dtPoly const& poly) noexcept
        {
            float sumX = 0.0f, sumZ = 0.0f;
            int const n = poly.vertCount;
            for (int j = 0; j < n; ++j)
            {
                float const* v = &tile.verts[poly.verts[j] * 3];
                sumX += v[0];
                sumZ += v[2];
            }
            float const inv = (n > 0) ? (1.0f / static_cast<float>(n)) : 0.0f;
            return { sumX * inv, sumZ * inv };
        }

        // Core scan: walks all tiles and polygons, invoking `visit(tile, polyIdx)`
        // for every polygon that lies in the corridor. visit() returns true to
        // continue, false to stop early.
        template <typename Visitor>
        void Walk(dtNavMesh const& nm, Segment const& seg,
                  uint32_t& tilesScanned, uint32_t& polysExamined,
                  Visitor&& visit)
        {
            if (seg.width <= 0.0f) return;

            Point2 const a = TcToDetour(seg.fromX, seg.fromY);
            Point2 const b = TcToDetour(seg.toX,   seg.toY);
            float const halfWidth   = seg.width * 0.5f;
            float const halfWidthSq = halfWidth * halfWidth;
            CorridorBox const box = MakeCorridorBox(a, b, halfWidth);

            int const maxTiles = nm.getMaxTiles();
            for (int t = 0; t < maxTiles; ++t)
            {
                dtMeshTile const* tile = nm.getTile(t);
                if (!tile || !tile->header) continue;
                if (!TileOverlapsCorridor(*tile, box)) continue;
                ++tilesScanned;

                int const polyCount = tile->header->polyCount;
                for (int p = 0; p < polyCount; ++p)
                {
                    dtPoly const& poly = tile->polys[p];
                    if (poly.vertCount == 0) continue;
                    // Off-mesh connections are virtual; skip them.
                    if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
                    ++polysExamined;

                    Point2 const c = PolyCentroidXZ(*tile, poly);
                    // Coarse AABB pre-filter on the centroid itself.
                    if (c.x < box.minX || c.x > box.maxX ||
                        c.z < box.minZ || c.z > box.maxZ)
                        continue;

                    float const dSq = PointSegmentDistSq2D(a, b, c);
                    if (dSq > halfWidthSq) continue;

                    if (!visit(*tile, t, p)) return;
                }
            }
        }
    } // namespace

    CorridorResult ScanCorridor(dtNavMesh const& nm, Segment const& seg, std::size_t maxResults)
    {
        CorridorResult out;
        if (maxResults == 0) return out;

        Walk(nm, seg, out.tilesScanned, out.polysExamined,
             [&](dtMeshTile const& tile, int tileIdx, int polyIdx) -> bool
             {
                 dtPolyRef const ref = nm.encodePolyId(
                     tile.salt,
                     static_cast<unsigned int>(tileIdx),
                     static_cast<unsigned int>(polyIdx));
                 out.polyRefs.push_back(static_cast<uint64_t>(ref));
                 return out.polyRefs.size() < maxResults;
             });
        return out;
    }

    std::vector<uint64_t> ScanCorridorsBatch(dtNavMesh const& nm,
                                             std::vector<Segment> const& segments)
    {
        std::vector<uint64_t> merged;
        for (Segment const& s : segments)
        {
            CorridorResult r = ScanCorridor(nm, s);
            merged.insert(merged.end(), r.polyRefs.begin(), r.polyRefs.end());
        }
        std::sort(merged.begin(), merged.end());
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
        return merged;
    }

    std::size_t ApplyCorridorToNavmesh(dtNavMesh& nm, Segment const& seg)
    {
        uint32_t tilesScanned = 0;
        uint32_t polysExamined = 0;
        std::size_t tagged = 0;

        Walk(nm, seg, tilesScanned, polysExamined,
             [&](dtMeshTile const& tile, int /*tileIdx*/, int polyIdx) -> bool
             {
                 // Walk() yields a const tile; we need a mutable handle to
                 // flip the area. The non-const overload of getTile() does
                 // exactly that without re-scanning. The tile pointer is
                 // stable for the lifetime of the navmesh.
                 (void)tile;
                 dtMeshTile* mtile = const_cast<dtMeshTile*>(&tile);
                 dtPoly& mpoly = mtile->polys[polyIdx];
                 if (mpoly.getArea() != kNavAreaRoad)
                 {
                     mpoly.setArea(kNavAreaRoad);
                     ++tagged;
                 }
                 return true;
             });
        return tagged;
    }

    std::size_t ApplyCorridorsToNavmesh(dtNavMesh& nm,
                                        std::vector<Segment> const& segments)
    {
        std::size_t total = 0;
        for (Segment const& s : segments)
            total += ApplyCorridorToNavmesh(nm, s);
        return total;
    }

    namespace Detail
    {
        float PointSegmentDistSq2D(float ax, float az, float bx, float bz,
                                   float px, float pz) noexcept
        {
            return Road::PointSegmentDistSq2D({ ax, az }, { bx, bz }, { px, pz });
        }

        DetourXZ TcToDetour(float tcX, float tcY) noexcept
        {
            Point2 const p = Road::TcToDetour(tcX, tcY);
            return { p.x, p.z };
        }
    }
}
