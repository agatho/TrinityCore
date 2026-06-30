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

#ifndef TRINITYCORE_ROAD_CORRIDOR_H
#define TRINITYCORE_ROAD_CORRIDOR_H

// RoadCorridor — pure geometric utility that finds navmesh polygons whose
// surface falls within `width/2` of a handcrafted road segment (a line
// from (fromX, fromY) to (toX, toY) in TrinityCore world coordinates).
//
// Two consumers share this code:
//   * world_editor — previews "which polys will get retagged" before commit
//   * worldserver  — applies retagging at map-load time
//
// Both use the same algorithm so the preview is byte-exact with the runtime
// apply. The implementation is intentionally header-only-friendly: no Qt,
// no SQL, no GL, only Detour types from dep/recastnavigation.
//
// Coordinate convention (matches PathGenerator.cpp:189):
//   TC world (x, y, z) ↔ Detour internal (y, z, x)
// so TC X (north-south) is Detour[2] and TC Y (west-east) is Detour[0].
// All inputs to this API are in TC frame; we convert internally.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

class dtNavMesh;

namespace Road
{
    // A handcrafted road segment in TrinityCore world coordinates.
    // width is full corridor width in yards (half on each side of the line).
    struct Segment
    {
        float fromX, fromY;
        float toX, toY;
        float width;
    };

    // Result of scanning a navmesh for polygons inside a corridor.
    // polyRefs holds the encoded dtPolyRef values (stored as uint64_t so this
    // header does not pull in DetourNavMesh.h).
    // tilesScanned / polysExamined are diagnostic counters useful for the
    // editor's preview HUD.
    struct CorridorResult
    {
        std::vector<uint64_t> polyRefs;
        uint32_t              tilesScanned   = 0;
        uint32_t              polysExamined  = 0;
    };

    // Scan a dtNavMesh for polygons whose CENTROID falls within `width/2`
    // of the line segment. The centroid is used as a coarse proxy for the
    // polygon surface — tile resolution is ~3 yards so this is sufficient
    // for road tagging while being much faster than per-triangle distance.
    //
    // Tiles whose AABB does not overlap the segment's inflated bounding box
    // are skipped without examining polygons.
    //
    // `maxResults` bounds the polyRef vector; the scan stops once it is
    // reached. Defaults to unlimited.
    [[nodiscard]] CorridorResult ScanCorridor(
        dtNavMesh const& nm,
        Segment const&   seg,
        std::size_t      maxResults = (std::numeric_limits<std::size_t>::max)());

    // Batch helper — runs ScanCorridor for each segment and returns the
    // union of their polyRefs (sorted ascending and deduplicated).
    [[nodiscard]] std::vector<uint64_t> ScanCorridorsBatch(
        dtNavMesh const&            nm,
        std::vector<Segment> const& segments);

    // In-place: find all polys in the corridor and flip their area to
    // NAV_AREA_ROAD (=7). Returns the count of polys whose area was changed.
    // Polys already tagged NAV_AREA_ROAD are still counted as "in corridor"
    // for the diagnostic return value.
    std::size_t ApplyCorridorToNavmesh(
        dtNavMesh&     nm,
        Segment const& seg);

    std::size_t ApplyCorridorsToNavmesh(
        dtNavMesh&                  nm,
        std::vector<Segment> const& segments);

    // -----------------------------------------------------------------
    // Detail namespace — exposed for unit tests of the underlying math
    // (point-to-segment distance, TC->Detour coord swap, corridor box
    // construction). Not part of the stable API; callers should use
    // ScanCorridor / ApplyCorridor* instead.
    // -----------------------------------------------------------------
    namespace Detail
    {
        // Squared distance from point (px, pz) to segment [(ax, az), (bx, bz)]
        // in the XZ plane (Detour frame). Clamped to the segment endpoints.
        [[nodiscard]] float PointSegmentDistSq2D(float ax, float az,
                                                 float bx, float bz,
                                                 float px, float pz) noexcept;

        // Convert TC (x, y) horizontal coords to Detour (X, Z) horizontal
        // coords. Returned as { detourX, detourZ }.
        struct DetourXZ { float x, z; };
        [[nodiscard]] DetourXZ TcToDetour(float tcX, float tcY) noexcept;
    }
}

#endif // TRINITYCORE_ROAD_CORRIDOR_H
