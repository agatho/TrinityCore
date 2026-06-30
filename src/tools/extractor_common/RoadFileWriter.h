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

#ifndef TRINITYCORE_ROAD_FILE_WRITER_H
#define TRINITYCORE_ROAD_FILE_WRITER_H

#include "AdtTextureReader.h"
#include "RoadClassifier.h"
#include "RoadMapDefines.h"

#include <memory>
#include <string>

namespace CASC { class Storage; }

namespace Road
{
    // Pure function: given an ADT texture summary, build the per-MCNK
    // road mask grid. Applies the texture-name classifier + the secondary
    // GroundEffectTexture-confidence signal (>0.3 default), then runs the
    // contiguous-area filter to suppress isolated false positives.
    //
    // The threshold and confidence cutoffs match design doc §3.3 + §3.4.
    //
    // Returns the populated grid. `outNonZeroCount` (optional) is the
    // number of MCNKs flagged road after filtering.
    TrinityCore::RoadMap::RoadMaskGrid BuildRoadMaskGrid(
        AdtTexture::AdtTextureSummary const& summary,
        std::size_t* outNonZeroCount = nullptr);

    // I/O wrapper: open the ADT via the supplied AdtTextureReader, build
    // the mask, and write it to `<outputDir>/<NNNN><XX><YY>.road`.
    //
    // Returns true on success, false on failure (ADT could not be read,
    // file write failed, etc.).
    //
    // If `outRoadMcnkCount` is non-null, set to the count of MCNKs
    // flagged road in the final grid.
    //
    // Note: the file IS written even when the mask is empty (all zeros).
    // This keeps the on-disk fleet consistent — every ADT that has a
    // .map file also has a .road file. Detecting "no road data" at
    // load time then means "file is missing" not "file has all zeros".
    bool WriteRoadFileForAdt(AdtTexture::AdtTextureReader& reader,
                              std::string const& outputDir,
                              uint32 mapId, uint8 adtX, uint8 adtY,
                              AdtTexture::WdtFlags const& wdtFlags,
                              uint32 rootAdtFileDataId,
                              uint32 tex0AdtFileDataId,
                              std::size_t* outRoadMcnkCount = nullptr);

    // Lower-level write: just serialize an already-built mask to disk.
    // Useful for tests + dumper-style workflows.
    bool WriteRoadFile(std::string const& outputDir,
                        uint32 mapId, uint8 adtX, uint8 adtY,
                        TrinityCore::RoadMap::RoadMaskGrid const& mask,
                        uint32 flags = 0);
}

#endif // TRINITYCORE_ROAD_FILE_WRITER_H
