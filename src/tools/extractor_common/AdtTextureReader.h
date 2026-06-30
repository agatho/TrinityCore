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

#ifndef TRINITYCORE_ADT_TEXTURE_READER_H
#define TRINITYCORE_ADT_TEXTURE_READER_H

#include "Define.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CASC { class Storage; }

namespace Road { class ListfileMap; }

namespace Road::AdtTexture
{
    // -----------------------------------------------------------------------
    // Constants. MCNK = 33.33 yards across the ADT's 16x16 grid. Each MCNK
    // has an 8x8 subcell sampling resolution and a 64x64 MCAL alpha map.
    // -----------------------------------------------------------------------

    constexpr std::size_t kMcnksPerAdtSide = 16;
    constexpr std::size_t kMcnksPerAdt     = kMcnksPerAdtSide * kMcnksPerAdtSide;  // 256
    constexpr std::size_t kSubcellsPerSide = 8;
    constexpr std::size_t kSubcellsPerMcnk = kSubcellsPerSide * kSubcellsPerSide;  // 64
    constexpr std::size_t kAlphaSidePixels = 64;
    constexpr std::size_t kAlphaPixels     = kAlphaSidePixels * kAlphaSidePixels;  // 4096
    constexpr std::size_t kMaxLayersPerMcnk = 4;

    // The 50% alpha cutoff above which a layer "dominates" at a given pixel.
    // Matches the dominant-layer rule in design doc §3.3.
    constexpr uint8 kAlphaDominantThreshold = 128;

    // -----------------------------------------------------------------------
    // WDT-level flags (read from MPHD, used to pick the MCAL decode path).
    // -----------------------------------------------------------------------

    struct WdtFlags
    {
        bool adtHasBigAlpha          = false;   // MPHD 0x4
        bool adtHasHeightTexturing   = false;   // MPHD 0x80 (Legion+)
        bool wdtHasMaid              = false;   // MPHD 0x200

        // True if MCAL alphas are stored as 8-bit (4096 bytes) rather than
        // 4-bit nibble-packed (2048 bytes). Either of the two flags above
        // implies the 8-bit form.
        bool UseFullByteAlpha() const noexcept
        {
            return adtHasBigAlpha || adtHasHeightTexturing;
        }
    };

    // -----------------------------------------------------------------------
    // Per-layer MCLY record (after parse + textureId resolution).
    // -----------------------------------------------------------------------

    struct LayerInfo
    {
        uint32 layerIdx       = 0;    // 0..3, 0 is implicit base
        uint32 mclyFlags      = 0;    // raw MCLY flags from the layer record
        uint32 textureIdx     = 0;    // MTEX index or position in MDID
        uint32 effectId       = 0;    // GroundEffectTexture.ID (MCLY.effectId)
        uint32 alphaOffset    = 0;    // offset into MCAL bytes
        std::string textureBlp;       // resolved BLP path from MTEX/MDID (or "" if unresolved)
    };

    // -----------------------------------------------------------------------
    // Per-MCNK summary — what the dumper writes one CSV row from.
    // -----------------------------------------------------------------------

    struct McnkTextureSummary
    {
        uint16 mcnkIdx          = 0;   // 0..255
        uint8  ix               = 0;   // 0..15 column within ADT
        uint8  iy               = 0;   // 0..15 row within ADT
        uint32 nLayers          = 0;
        uint32 areaId           = 0;
        bool   hasHoles         = false;
        float  xpos             = 0.f;
        float  ypos             = 0.f;
        float  zpos             = 0.f;

        // Up to 4 layers, filled by index (0..nLayers-1).
        std::array<LayerInfo, kMaxLayersPerMcnk> layers{};

        // The layer that "won" the most subcells across this MCNK. If a tie,
        // the higher-index layer wins (matches the design doc dominant rule).
        uint32 dominantLayerIdx     = 0;

        // BLP path of the dominant layer (convenience copy of
        // layers[dominantLayerIdx].textureBlp).
        std::string dominantTextureBlp;
        uint32 dominantEffectId     = 0;

        // How many subcells (out of 64) resolved to each layer.
        std::array<uint16, kMaxLayersPerMcnk> subcellsWonPerLayer{};
    };

    // -----------------------------------------------------------------------
    // Per-ADT result.
    // -----------------------------------------------------------------------

    struct AdtTextureSummary
    {
        uint32 mapId    = 0;
        uint8  adtX     = 0;
        uint8  adtY     = 0;
        WdtFlags wdtFlags{};

        // All 256 MCNKs (sparse — entries with nLayers == 0 will be
        // default-constructed empty).
        std::array<McnkTextureSummary, kMcnksPerAdt> mcnks{};

        // Non-fatal parse warnings.
        std::vector<std::string> warnings;
    };

    // -----------------------------------------------------------------------
    // Reader. One instance per CASC storage; thread-safe to clone, not
    // thread-safe to share a single instance across threads (CASC handle
    // limitation downstream).
    // -----------------------------------------------------------------------

    class AdtTextureReader
    {
    public:
        explicit AdtTextureReader(std::shared_ptr<CASC::Storage const> storage);

        // Install an optional FileDataID → path listfile. When present, the
        // reader resolves MDID texture references (Legion+ ADTs) into real
        // BLP paths so the classifier's substring matcher can run on them.
        // Without a listfile, MDID layers get `[FDID:N]` placeholders.
        // Pointer is borrowed — caller must keep the ListfileMap alive for
        // the duration of any ReadAdt call.
        void SetListfileMap(ListfileMap const* listfile) { _listfile = listfile; }
        ListfileMap const* GetListfileMap() const { return _listfile; }

        // Reads (root.adt, tex0.adt) and produces a full summary.
        // FileDataIDs come from WDT.MAID; the caller is responsible for
        // looking them up via ReadWdtFlagsAndAdtIds.
        //
        // Returns nullopt if root.adt cannot be opened. If tex0.adt cannot
        // be opened (older content with no split-file), proceeds with
        // root-only and reports a warning — but no texture data will be
        // populated.
        std::optional<AdtTextureSummary> ReadAdt(uint32 mapId,
                                                  uint8 adtX, uint8 adtY,
                                                  WdtFlags const& flags,
                                                  uint32 rootAdtFileDataId,
                                                  uint32 tex0AdtFileDataId);

    private:
        std::shared_ptr<CASC::Storage const> _storage;
        ListfileMap const* _listfile = nullptr;
    };

    // -----------------------------------------------------------------------
    // WDT helper: open the map's WDT, parse MPHD flags + MAID, return both.
    // The MAID's adt_files[adtY][adtX] entry has root + tex0 + obj0 + obj1
    // + lod + minimap FileDataIDs.
    //
    // Returns nullopt if the WDT cannot be opened or has no MPHD chunk.
    // -----------------------------------------------------------------------

    struct WdtAdtIds
    {
        uint32 rootADT       = 0;
        uint32 tex0ADT       = 0;
        uint32 obj0ADT       = 0;
        uint32 minimapBlp    = 0;
        bool   present       = false;   // true iff this (x,y) has terrain
    };

    struct WdtInfo
    {
        uint32 mapId         = 0;
        WdtFlags flags{};
        // 64x64 grid; entries are .present = false where no ADT exists.
        std::array<std::array<WdtAdtIds, 64>, 64> adts{};
        std::vector<std::string> warnings;
    };

    std::optional<WdtInfo> ReadWdt(CASC::Storage const& storage,
                                    uint32 mapId,
                                    uint32 wdtFileDataId);

    // -----------------------------------------------------------------------
    // Pure-function MCAL alpha-map decoder. Exposed for unit testing the
    // three decode branches in isolation.
    //
    // Inputs:
    //   src      - bytes starting at the layer's alphaOffset within MCAL
    //   srcSize  - bytes available from src (bounded by MCAL.sizeMCAL)
    //   flags    - the layer's MCLY.flags (0x200 = RLE compressed)
    //   useFullByteAlpha - true if WDT.MPHD has 0x4 OR 0x80
    //
    // Output: 4096 bytes in dst, one byte per pixel in a 64x64 grid, row-major.
    //
    // Returns true if decode succeeded, false if input bytes ran out.
    // -----------------------------------------------------------------------

    bool DecodeMcalAlpha(uint8 const* src, std::size_t srcSize,
                         uint32 mclyFlags, bool useFullByteAlpha,
                         std::array<uint8, kAlphaPixels>& dst);

    // -----------------------------------------------------------------------
    // Dominant-layer aggregator. Pure function over per-layer alpha grids.
    //
    // For each of 64 subcells (8x8), samples the alpha at the subcell's
    // center pixel for layers 1..(nLayers-1) and picks the highest-index
    // layer whose alpha > kAlphaDominantThreshold. If none qualify, layer 0
    // (the implicit base) wins.
    //
    // Then aggregates over all 64 subcells to find the layer that won the
    // most subcells (tie-break: higher index wins). Writes the per-layer
    // win counts and the winner to the summary fields.
    // -----------------------------------------------------------------------

    void AggregateDominantLayer(McnkTextureSummary& summary,
        std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> const& alphas);
}

#endif // TRINITYCORE_ADT_TEXTURE_READER_H
