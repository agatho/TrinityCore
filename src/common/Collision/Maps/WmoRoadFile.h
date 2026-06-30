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

#ifndef TRINITYCORE_WMO_ROAD_FILE_H
#define TRINITYCORE_WMO_ROAD_FILE_H

#include "Define.h"

#include <cstdint>

// ----------------------------------------------------------------------------
// WMO road-aware mmaps parallel file format.
//
// Phase 2 of the road-aware pathfinding workstream (see
// ROAD_AWARE_PATHFINDING_PLAN.md §5 + WMO_ROAD_EXTRACTION_RESEARCH.md).
//
// vmap4_extractor writes one .vmo.road sidecar alongside each .vmo it
// produces in Buildings/. The sidecar carries per-collision-triangle road
// classification verdicts for that WMO (across all its groups).
//
// File layout:
//
//   [WmoRoadFileHeader (16 bytes)]
//     uint32 magic     'WROD' on disk
//     uint32 version   1
//     uint32 nGroups   count of group records following the header
//     uint32 flags     reserved (e.g. low_confidence_extraction marker)
//
//   For each of nGroups groups (in the same order the corresponding
//   groups appear in the main .vmo file):
//
//     [WmoRoadGroupHeader (8 bytes)]
//       uint32 nColTriangles  number of collision triangles in this group
//       uint32 flagsBytes     count of bytes that follow = ceil(nColTriangles/8)
//
//     [packed bit array]
//       uint8 bits[flagsBytes]    bit (i % 8) of byte (i / 8) = 1 iff
//                                  collision triangle i has a road material
//
// Rationale for sidecar (vs extending .vmo):
//   The .vmo binary format is read at runtime by VMapManager for LoS +
//   collision. Extending it would force the entire vmap fleet to be
//   regenerated AND break compat with any cached vmap files. The sidecar
//   approach is additive — old WMOs (no .vmo.road) simply have no road
//   tags applied; new ones do.
//
// Why per-collision-triangle (not just per-material bitmap):
//   ConvertToVMAPGroupWmo filters source triangles by collision flag and
//   only writes COLLISION triangles to .vmo. The mapping from .vmo triangle
//   index → source materialId is lost. Storing the road verdict per-
//   collision-triangle (after filtering) keeps the sidecar self-sufficient
//   and aligned with what VMapManager loads.
// ----------------------------------------------------------------------------

namespace TrinityCore::WmoRoad
{
    inline constexpr uint32 kWmoRoadMagic   = 0x444F5257u;   // 'D','O','R','W' on disk = "WROD"
    inline constexpr uint32 kWmoRoadVersion = 1u;

    enum class WmoRoadFileFlags : uint32
    {
        kNone           = 0,
        kLowConfidence  = 0x1,   // reserved
    };

    struct WmoRoadFileHeader
    {
        uint32 magic    = kWmoRoadMagic;
        uint32 version  = kWmoRoadVersion;
        uint32 nGroups  = 0;
        uint32 flags    = 0;
    };

    struct WmoRoadGroupHeader
    {
        uint32 nColTriangles = 0;
        uint32 flagsBytes    = 0;   // = (nColTriangles + 7) / 8
    };

    static_assert(sizeof(WmoRoadFileHeader)  == 16, "header must be 16 bytes");
    static_assert(sizeof(WmoRoadGroupHeader) == 8,  "group header must be 8 bytes");

    inline constexpr std::size_t BytesNeededForFlags(uint32 nColTriangles)
    {
        return (nColTriangles + 7) / 8;
    }

    inline bool GetTriangleRoadBit(uint8 const* bits, uint32 triangleIdx)
    {
        return (bits[triangleIdx / 8] >> (triangleIdx % 8)) & 1u;
    }

    inline void SetTriangleRoadBit(uint8* bits, uint32 triangleIdx)
    {
        bits[triangleIdx / 8] |= (uint8(1) << (triangleIdx % 8));
    }
}

#endif // TRINITYCORE_WMO_ROAD_FILE_H
