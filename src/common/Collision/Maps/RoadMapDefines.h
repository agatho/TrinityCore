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

#ifndef TRINITYCORE_ROAD_MAP_DEFINES_H
#define TRINITYCORE_ROAD_MAP_DEFINES_H

#include "Define.h"

#include <array>
#include <cstdint>
#include <string>

// ----------------------------------------------------------------------------
// Road-aware mmaps parallel file format.
//
// The road-aware mmap workstream (design doc: ROAD_AWARE_PATHFINDING_PLAN.md)
// extends terrain extraction with a per-MCNK road mask. Rather than modify
// the existing .map file format (which would force ALL existing extracted
// maps to be re-generated), we store road metadata in a PARALLEL file:
//
//   maps/<NN>/<NN><XX><YY>.road
//
// Where NN is the map id (zero-padded), XX/YY are ADT tile coords.
// The format is fixed-size: 24-byte header + 256 bytes of mask = 280 bytes
// per ADT. Servers + mmaps_generator look for these files alongside the
// existing .map files; absence means "no road data, behave as today".
//
// Rationale:
//   1. Old maps without road files Just Work — zero forced re-extraction.
//   2. New extraction is additive: produces .map (unchanged) + .road (new).
//   3. mmaps regen is the ONLY hard step required to activate road behavior.
//   4. Format is self-contained — magic + version make corruption visible.
// ----------------------------------------------------------------------------

namespace TrinityCore::RoadMap
{
    // ASCII magic 'ROAD' (little-endian: stored as bytes R,O,A,D on disk).
    inline constexpr uint32 kRoadFileMagic   = 0x44414F52u;   // 'D','A','O','R' in LE = "ROAD" on disk
    inline constexpr uint32 kRoadFileVersion = 1u;

    // 16x16 MCNK grid per ADT.
    inline constexpr std::size_t kMcnksPerAdtSide = 16;
    inline constexpr std::size_t kMcnksPerAdt     =
        kMcnksPerAdtSide * kMcnksPerAdtSide;   // 256

    enum class RoadFileFlags : uint32
    {
        kNone           = 0,
        kLowConfidence  = 0x1,   // classifier flagged this regen as low-confidence
                                 // (e.g. corpus precision/recall below quality bar
                                 // at time of writing) — server logs a warning
        kExperimental   = 0x2,   // reserved for future iteration tags
    };

    // 24-byte header. Plain-old-data, written with fwrite, read with fread.
    // The mask follows immediately after the header in the file.
    struct RoadFileHeader
    {
        uint32 magic    = kRoadFileMagic;
        uint32 version  = kRoadFileVersion;
        uint32 flags    = 0;        // bitfield, see RoadFileFlags
        uint32 mapId    = 0;        // for sanity check at load time
        uint8  adtX     = 0;        // 0..63
        uint8  adtY     = 0;        // 0..63
        uint8  reserved[6] = { };   // pads to multiple of 4; future use
    };

    static_assert(sizeof(RoadFileHeader) == 24,
                  "RoadFileHeader must be exactly 24 bytes for forward compatibility");

    // Per-MCNK road byte. Nonzero = MCNK is road. Currently only 0/1
    // semantics are used, but the byte width is intentional so future
    // extensions (e.g. road sub-types: bit 0 = dirt road, bit 1 = paved,
    // bit 2 = bridge approach) can be added without a version bump as
    // long as nonzero continues to mean "is road".
    using RoadMaskGrid = std::array<uint8, kMcnksPerAdt>;

    static_assert(sizeof(RoadMaskGrid) == 256,
                  "RoadMaskGrid must be exactly 256 bytes (16x16 MCNK grid)");

    inline constexpr std::size_t kRoadFileSize =
        sizeof(RoadFileHeader) + sizeof(RoadMaskGrid);  // 280 bytes

    // Compose a road-file path for a given map directory, mapId, and ADT
    // coords. Mirrors the .map filename convention:
    //
    //   maps/<NN><XX><YY>.map     (existing)
    //   maps/<NN><XX><YY>.road    (this file)
    //
    // where NN/XX/YY are 4-digit / 2-digit / 2-digit zero-padded.
    //
    // The returned string is POSIX-slash-separated; callers wrap in their
    // platform's path type as needed.
    inline std::string MakeRoadFilePath(std::string const& mapsDir,
                                        uint32 mapId, uint8 adtX, uint8 adtY)
    {
        // Filename pattern mirrors TC's existing .map convention:
        //   maps/<mapId:04>_<adtX:02>_<adtY:02>.map
        // → maps/<mapId:04>_<adtX:02>_<adtY:02>.road
        // The underscores are required — TerrainBuilder::loadRoadMask uses
        // Trinity::StringFormat("{:04}_{:02}_{:02}.road", ...) to read.
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s/%04u_%02u_%02u.road",
                      mapsDir.c_str(), mapId,
                      static_cast<unsigned>(adtX),
                      static_cast<unsigned>(adtY));
        return std::string(buf);
    }
}

#endif // TRINITYCORE_ROAD_MAP_DEFINES_H
