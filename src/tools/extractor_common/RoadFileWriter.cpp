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

#include "RoadFileWriter.h"

#include <cstdio>
#include <cstring>

namespace Road
{
    // -----------------------------------------------------------------------
    // BuildRoadMaskGrid — pure function. Texture name + secondary signal +
    // contiguous-area filter. Mirrors design doc §3.3 + §3.4.
    // -----------------------------------------------------------------------

    TrinityCore::RoadMap::RoadMaskGrid BuildRoadMaskGrid(
        AdtTexture::AdtTextureSummary const& summary,
        std::size_t* outNonZeroCount)
    {
        Road::McnkGrid working{};

        // Per-MCNK classifier: an MCNK is "road" if EITHER:
        //   (a) its dominant layer (highest-index layer with sufficient
        //       alpha coverage at the 8x8 subcell centers) has a road
        //       texture path — covers the "uniform road MCNK" case
        //       (Stormwind cobblestone plazas, etc.); OR
        //   (b) ANY layer in the MCNK has a road texture AND that layer's
        //       alpha exceeds the dominant threshold across at least
        //       kMinRoadSubcells of the 64 (8x8) subcells — covers the
        //       "road overlay on grass/dirt base" case (Kalimdor Gold
        //       Road, Mulgore village roads, etc.).
        //
        // Rule (b) is permissive enough to catch overlay roads that the
        // dominant rule misses. The subcell-count threshold keeps it from
        // tagging every MCNK that has trace amounts of road texture.
        constexpr std::size_t kMinRoadSubcells = 4;  // 4/64 = 6.25%

        for (AdtTexture::McnkTextureSummary const& m : summary.mcnks)
        {
            if (m.nLayers == 0)
                continue;

            // Combined secondary signal: effect confidence > 0.3 is the
            // gate (default 0.5 when no GroundEffectTexture table loaded,
            // so this is permissive in production).
            float effectConf = RoadConfidenceFromEffectId(m.dominantEffectId);
            if (effectConf <= 0.3f)
                continue;

            bool isRoad = false;

            // Rule (a): dominant layer's texture is a road.
            if (IsRoadTexturePath(m.dominantTextureBlp))
                isRoad = true;

            // Rule (b): any layer is a road texture AND wins enough
            // subcells (per subcellsWonPerLayer counts already computed
            // by AggregateDominantLayer).
            if (!isRoad)
            {
                uint32 nLayers = std::min<uint32>(m.nLayers,
                    AdtTexture::kMaxLayersPerMcnk);
                for (uint32 layer = 0; layer < nLayers; ++layer)
                {
                    if (m.subcellsWonPerLayer[layer] < kMinRoadSubcells)
                        continue;
                    if (IsRoadTexturePath(m.layers[layer].textureBlp))
                    {
                        isRoad = true;
                        break;
                    }
                }
            }

            if (!isRoad)
                continue;

            // Map MCNK ix/iy into the 16×16 grid. WoW's ADT convention is
            // row-major with iy going down (south). RoadMaskGrid uses the
            // same convention.
            std::size_t idx = m.iy * 16 + m.ix;
            if (idx >= working.size())
                continue;
            working[idx] = 1;
        }

        // Suppress isolated FP clusters. Default threshold 4 = ~133 yards.
        Road::ApplyContiguousAreaFilter(working, 4);

        TrinityCore::RoadMap::RoadMaskGrid out{};
        std::memcpy(out.data(), working.data(), out.size());

        if (outNonZeroCount)
        {
            std::size_t n = 0;
            for (uint8 v : out) if (v != 0) ++n;
            *outNonZeroCount = n;
        }
        return out;
    }

    // -----------------------------------------------------------------------
    // WriteRoadFile — low-level serialization.
    // -----------------------------------------------------------------------

    bool WriteRoadFile(std::string const& outputDir,
                        uint32 mapId, uint8 adtX, uint8 adtY,
                        TrinityCore::RoadMap::RoadMaskGrid const& mask,
                        uint32 flags)
    {
        std::string path = TrinityCore::RoadMap::MakeRoadFilePath(
            outputDir, mapId, adtX, adtY);
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;

        TrinityCore::RoadMap::RoadFileHeader header;
        header.magic    = TrinityCore::RoadMap::kRoadFileMagic;
        header.version  = TrinityCore::RoadMap::kRoadFileVersion;
        header.flags    = flags;
        header.mapId    = mapId;
        header.adtX     = adtX;
        header.adtY     = adtY;
        // reserved already zeroed by default-init.

        bool ok = (std::fwrite(&header, sizeof(header), 1, f) == 1) &&
                  (std::fwrite(mask.data(), mask.size(), 1, f) == 1);
        std::fclose(f);
        return ok;
    }

    // -----------------------------------------------------------------------
    // WriteRoadFileForAdt — end-to-end orchestration.
    // -----------------------------------------------------------------------

    bool WriteRoadFileForAdt(AdtTexture::AdtTextureReader& reader,
                              std::string const& outputDir,
                              uint32 mapId, uint8 adtX, uint8 adtY,
                              AdtTexture::WdtFlags const& wdtFlags,
                              uint32 rootAdtFileDataId,
                              uint32 tex0AdtFileDataId,
                              std::size_t* outRoadMcnkCount)
    {
        auto summary = reader.ReadAdt(mapId, adtX, adtY, wdtFlags,
                                       rootAdtFileDataId, tex0AdtFileDataId);
        if (!summary)
            return false;

        std::size_t nonZero = 0;
        auto mask = BuildRoadMaskGrid(*summary, &nonZero);
        if (outRoadMcnkCount)
            *outRoadMcnkCount = nonZero;

        return WriteRoadFile(outputDir, mapId, adtX, adtY, mask);
    }
}
