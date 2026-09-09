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

#include "AdtTextureReader.h"
#include "CascHandles.h"
#include "ListfileMap.h"

#include <CascLib.h>

#include <algorithm>
#include <cstring>

namespace Road::AdtTexture
{
    // -----------------------------------------------------------------------
    // FourCC helpers + raw chunk walking.
    //
    // ADT chunks are little-endian. On disk a FourCC like "MCNK" is stored
    // as the bytes "K","N","C","M" (because the format predates utf-8 and
    // was written as 32-bit reversed strings). We match by raw uint32 to
    // avoid any per-byte iteration on the hot path.
    // -----------------------------------------------------------------------

    namespace
    {
        constexpr uint32 MakeFourCC(char a, char b, char c, char d) noexcept
        {
            // On-disk byte order is "dcba" because of how 32-bit FourCCs
            // were historically written.
            return static_cast<uint32>(static_cast<uint8>(a))
                 | (static_cast<uint32>(static_cast<uint8>(b)) << 8)
                 | (static_cast<uint32>(static_cast<uint8>(c)) << 16)
                 | (static_cast<uint32>(static_cast<uint8>(d)) << 24);
        }

        // Chunks-as-written ("KNCM" for "MCNK").
        constexpr uint32 kFccMVER = MakeFourCC('R', 'E', 'V', 'M');
        constexpr uint32 kFccMHDR = MakeFourCC('R', 'D', 'H', 'M');
        constexpr uint32 kFccMCNK = MakeFourCC('K', 'N', 'C', 'M');
        constexpr uint32 kFccMCLY = MakeFourCC('Y', 'L', 'C', 'M');
        constexpr uint32 kFccMCAL = MakeFourCC('L', 'A', 'C', 'M');
        constexpr uint32 kFccMTEX = MakeFourCC('X', 'E', 'T', 'M');
        constexpr uint32 kFccMDID = MakeFourCC('D', 'I', 'D', 'M');
        constexpr uint32 kFccMPHD = MakeFourCC('D', 'H', 'P', 'M');
        constexpr uint32 kFccMAIN = MakeFourCC('N', 'I', 'A', 'M');
        constexpr uint32 kFccMAID = MakeFourCC('D', 'I', 'A', 'M');

        struct ChunkHeader
        {
            uint32 magic;
            uint32 size;
        };

        // Iterate top-level chunks in a buffer. `cb` is called once per
        // chunk with (magic, body ptr, body size, chunk start ptr).
        // Halts if cb returns false.
        template<typename Cb>
        void ForEachTopLevelChunk(uint8 const* data, std::size_t dataSize,
                                  Cb&& cb)
        {
            std::size_t pos = 0;
            while (pos + sizeof(ChunkHeader) <= dataSize)
            {
                ChunkHeader const* hdr =
                    reinterpret_cast<ChunkHeader const*>(data + pos);
                std::size_t bodyOffset = pos + sizeof(ChunkHeader);
                std::size_t bodyEnd = bodyOffset + hdr->size;
                if (bodyEnd > dataSize)
                    break;
                if (!cb(hdr->magic, data + bodyOffset, hdr->size, data + pos))
                    return;
                pos = bodyEnd;
            }
        }

        // Find the first top-level chunk of the given FourCC.
        bool FindTopLevelChunk(uint8 const* data, std::size_t dataSize,
                                uint32 fcc,
                                uint8 const*& outBody,
                                std::size_t& outBodySize,
                                uint8 const*& outChunkStart)
        {
            bool found = false;
            ForEachTopLevelChunk(data, dataSize,
                [&](uint32 magic, uint8 const* body, std::size_t size,
                    uint8 const* chunkStart) {
                    if (magic == fcc)
                    {
                        outBody = body;
                        outBodySize = size;
                        outChunkStart = chunkStart;
                        found = true;
                        return false;   // stop iterating
                    }
                    return true;
                });
            return found;
        }

        // Find a sub-chunk inside an MCNK at the given offset relative to
        // the MCNK chunk start (FourCC byte 0). Returns the body pointer
        // and size, NOT the chunk header.
        bool FindMcnkSubChunk(uint8 const* mcnkStart, std::size_t mcnkBodySize,
                              std::size_t offsetFromMcnkStart, uint32 expectFcc,
                              uint8 const*& outBody, std::size_t& outBodySize)
        {
            // offsetFromMcnkStart is relative to MCNK FourCC byte.
            if (offsetFromMcnkStart == 0)
                return false;
            std::size_t totalAvail = mcnkBodySize + sizeof(ChunkHeader);
            if (offsetFromMcnkStart + sizeof(ChunkHeader) > totalAvail)
                return false;
            ChunkHeader const* hdr =
                reinterpret_cast<ChunkHeader const*>(mcnkStart + offsetFromMcnkStart);
            if (hdr->magic != expectFcc)
                return false;
            std::size_t bodyOffset = offsetFromMcnkStart + sizeof(ChunkHeader);
            if (bodyOffset + hdr->size > totalAvail)
                return false;
            outBody = mcnkStart + bodyOffset;
            outBodySize = hdr->size;
            return true;
        }

        // Linear search for an MCNK sub-chunk by FourCC, walking the MCNK
        // body as a stream of sub-chunks.
        //
        // This is the "modern split-file" path: in tex0.adt (and modern
        // root.adt to some extent) MCNKs do NOT have the legacy 128-byte
        // header — the chunk body IS the sub-chunks. MCLY/MCAL/MCSH live
        // at body[0] / body[N] / etc. without an addressing table.
        //
        // We accept both forms: try linear walk from body[0], and the
        // walk gracefully skips past a 128-byte legacy header because
        // anything that isn't a known FourCC is silently passed by the
        // outer FindMcnkSubChunk caller via offsMCLY when that's set.
        //
        // mcnkBodyStart points at the first byte AFTER the MCNK FourCC+size
        // (i.e. mcnkStart + 8).
        bool SearchMcnkSubChunkByFcc(uint8 const* mcnkBodyStart,
                                      std::size_t mcnkBodySize,
                                      uint32 expectFcc,
                                      uint8 const*& outBody,
                                      std::size_t& outBodySize)
        {
            std::size_t pos = 0;
            while (pos + sizeof(ChunkHeader) <= mcnkBodySize)
            {
                ChunkHeader const* hdr =
                    reinterpret_cast<ChunkHeader const*>(mcnkBodyStart + pos);
                if (pos + sizeof(ChunkHeader) + hdr->size > mcnkBodySize)
                    break;
                if (hdr->magic == expectFcc)
                {
                    outBody = mcnkBodyStart + pos + sizeof(ChunkHeader);
                    outBodySize = hdr->size;
                    return true;
                }
                pos += sizeof(ChunkHeader) + hdr->size;
            }
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // MCAL alpha-map decoder. Three branches:
    //
    //   1. MCLY flags 0x200 set      → RLE compressed (variable length).
    //   2. MPHD 0x4 or 0x80 set      → 4096-byte direct 8-bit copy.
    //   3. Else                      → 2048-byte 4-bit nibble-packed,
    //                                   each nibble multiplied by 17.
    //
    // Output is always a 4096-byte 64x64 row-major grid.
    // -----------------------------------------------------------------------

    bool DecodeMcalAlpha(uint8 const* src, std::size_t srcSize,
                         uint32 mclyFlags, bool useFullByteAlpha,
                         std::array<uint8, kAlphaPixels>& dst)
    {
        dst.fill(0);

        // Branch 1: RLE compressed.
        if (mclyFlags & 0x200u)
        {
            std::size_t srcPos = 0;
            std::size_t dstPos = 0;
            while (dstPos < kAlphaPixels && srcPos < srcSize)
            {
                uint8 control = src[srcPos++];
                bool fill = (control & 0x80u) != 0;
                uint8 count = control & 0x7Fu;
                if (count == 0)
                {
                    // Spec safety: count==0 means "skip" — advance not at all,
                    // but we exit the loop to avoid infinite spin.
                    break;
                }
                if (fill)
                {
                    if (srcPos >= srcSize)
                        return false;
                    uint8 value = src[srcPos++];
                    for (uint8 i = 0; i < count && dstPos < kAlphaPixels; ++i)
                        dst[dstPos++] = value;
                }
                else
                {
                    for (uint8 i = 0; i < count && dstPos < kAlphaPixels; ++i)
                    {
                        if (srcPos >= srcSize)
                            return false;
                        dst[dstPos++] = src[srcPos++];
                    }
                }
            }
            return dstPos == kAlphaPixels;
        }

        // Branch 2: 4096-byte 8-bit direct copy.
        if (useFullByteAlpha)
        {
            if (srcSize < kAlphaPixels)
                return false;
            std::memcpy(dst.data(), src, kAlphaPixels);
            return true;
        }

        // Branch 3: 2048-byte 4-bit nibble-packed, each nibble *17 to span
        // 0..255. Low nibble first, then high nibble per byte.
        if (srcSize < 2048)
            return false;
        std::size_t dstPos = 0;
        for (std::size_t i = 0; i < 2048; ++i)
        {
            uint8 b = src[i];
            uint8 low  = static_cast<uint8>((b & 0x0Fu) * 17u);
            uint8 high = static_cast<uint8>(((b >> 4) & 0x0Fu) * 17u);
            dst[dstPos++] = low;
            dst[dstPos++] = high;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Dominant-layer aggregator
    // -----------------------------------------------------------------------

    void AggregateDominantLayer(McnkTextureSummary& summary,
        std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> const& alphas)
    {
        summary.subcellsWonPerLayer.fill(0);

        uint32 n = std::min<uint32>(summary.nLayers, kMaxLayersPerMcnk);
        if (n == 0)
        {
            summary.dominantLayerIdx = 0;
            return;
        }

        // For each subcell (8x8 grid over the 64x64 alpha map), score every
        // upper layer by the MAX alpha over the full 8x8 subcell region.
        for (std::size_t sy = 0; sy < kSubcellsPerSide; ++sy)
        {
            for (std::size_t sx = 0; sx < kSubcellsPerSide; ++sx)
            {
                std::size_t const baseY = sy * 8;
                std::size_t const baseX = sx * 8;

                // For each upper layer, take the MAX alpha over the full 8x8
                // subcell (64 pixels) rather than a single center pixel. A layer
                // wins the subcell if its subcell-max is the highest value above
                // threshold; tie-break favours the higher layer index. The client
                // blends all 64x64 texels, so sampling only the centre pixel
                // dropped thin roads (<=8px) that thread between subcell centres.
                // Layer 0 is the implicit base and is never sampled.
                uint32 dominant = 0;
                uint8  bestMax  = 0;
                for (uint32 layer = 1; layer < n; ++layer)
                {
                    uint8 layerMax = 0;
                    for (std::size_t dy = 0; dy < 8; ++dy)
                    {
                        std::size_t const rowBase = (baseY + dy) * kAlphaSidePixels + baseX;
                        for (std::size_t dx = 0; dx < 8; ++dx)
                        {
                            uint8 const a = alphas[layer][rowBase + dx];
                            if (a > layerMax)
                                layerMax = a;
                        }
                    }
                    if (layerMax > kAlphaDominantThreshold && layerMax >= bestMax)
                    {
                        bestMax  = layerMax;
                        dominant = layer;
                    }
                }
                ++summary.subcellsWonPerLayer[dominant];
            }
        }

        // Pick the layer that won the most subcells. Tie-break: higher index.
        uint32 bestLayer = 0;
        uint16 bestCount = summary.subcellsWonPerLayer[0];
        for (uint32 l = 1; l < n; ++l)
        {
            if (summary.subcellsWonPerLayer[l] >= bestCount)
            {
                bestCount = summary.subcellsWonPerLayer[l];
                bestLayer = l;
            }
        }
        summary.dominantLayerIdx = bestLayer;
        summary.dominantTextureBlp = summary.layers[bestLayer].textureBlp;
        summary.dominantEffectId = summary.layers[bestLayer].effectId;
    }

    // -----------------------------------------------------------------------
    // CASC file reading helpers
    // -----------------------------------------------------------------------

    namespace
    {
        bool ReadCascFileBytes(CASC::Storage const& storage,
                                uint32 fileDataId,
                                std::vector<uint8>& out)
        {
            std::unique_ptr<CASC::File> f(
                storage.OpenFile(fileDataId, CASC_LOCALE_ALL_WOW, false));
            if (!f)
                return false;
            int64 sz = f->GetSize();
            if (sz < 0)
                return false;
            out.resize(static_cast<std::size_t>(sz));
            uint32 bytesRead = 0;
            if (!f->ReadFile(out.data(), static_cast<uint32>(sz), &bytesRead))
                return false;
            return bytesRead == static_cast<uint32>(sz);
        }

        // Parse MTEX into a vector of null-terminated strings.
        std::vector<std::string> ParseMtex(uint8 const* body, std::size_t size)
        {
            std::vector<std::string> out;
            std::size_t pos = 0;
            while (pos < size)
            {
                std::size_t end = pos;
                while (end < size && body[end] != '\0')
                    ++end;
                if (end > pos)
                    out.emplace_back(reinterpret_cast<char const*>(body + pos),
                                     end - pos);
                else if (end == pos && pos == 0)
                {
                    // first byte is null - empty entry
                    out.emplace_back();
                }
                pos = end + 1;
            }
            return out;
        }

        // Parse MDID into a vector of FileDataIDs.
        std::vector<uint32> ParseMdid(uint8 const* body, std::size_t size)
        {
            std::vector<uint32> out;
            std::size_t count = size / sizeof(uint32);
            out.resize(count);
            std::memcpy(out.data(), body, count * sizeof(uint32));
            return out;
        }
    }

    // -----------------------------------------------------------------------
    // WDT reading
    // -----------------------------------------------------------------------

    std::optional<WdtInfo> ReadWdt(CASC::Storage const& storage,
                                    uint32 mapId,
                                    uint32 wdtFileDataId)
    {
        std::vector<uint8> bytes;
        if (!ReadCascFileBytes(storage, wdtFileDataId, bytes))
            return std::nullopt;

        WdtInfo info;
        info.mapId = mapId;

        uint8 const* mphdBody = nullptr;
        std::size_t mphdSize = 0;
        uint8 const* mphdChunk = nullptr;
        if (FindTopLevelChunk(bytes.data(), bytes.size(), kFccMPHD,
                              mphdBody, mphdSize, mphdChunk))
        {
            if (mphdSize >= sizeof(uint32))
            {
                uint32 flags = *reinterpret_cast<uint32 const*>(mphdBody);
                info.flags.adtHasBigAlpha        = (flags & 0x4u)   != 0;
                info.flags.adtHasHeightTexturing = (flags & 0x80u)  != 0;
                info.flags.wdtHasMaid            = (flags & 0x200u) != 0;
            }
        }
        else
        {
            info.warnings.push_back("WDT has no MPHD chunk");
        }

        // MAID — modern (Legion+) lookup table. Required for FileDataID-based
        // ADT loading; falls back to MAIN-based filename loading otherwise.
        uint8 const* maidBody = nullptr;
        std::size_t maidSize = 0;
        uint8 const* maidChunk = nullptr;
        if (FindTopLevelChunk(bytes.data(), bytes.size(), kFccMAID,
                              maidBody, maidSize, maidChunk))
        {
            // The MAID chunk has the same layout as wdt_MAID.adt_files[64][64].
            constexpr std::size_t kAdtRecordSize = sizeof(uint32) * 8;   // 8 file IDs per ADT
            std::size_t expected = 64ull * 64ull * kAdtRecordSize;
            if (maidSize >= expected)
            {
                struct AdtRecord
                {
                    uint32 rootADT;
                    uint32 obj0ADT;
                    uint32 obj1ADT;
                    uint32 tex0ADT;
                    uint32 lodADT;
                    uint32 mapTexture;
                    uint32 mapTextureN;
                    uint32 minimapTexture;
                };
                AdtRecord const* rec =
                    reinterpret_cast<AdtRecord const*>(maidBody);
                for (std::size_t y = 0; y < 64; ++y)
                {
                    for (std::size_t x = 0; x < 64; ++x)
                    {
                        AdtRecord const& r = rec[y * 64 + x];
                        WdtAdtIds& ids = info.adts[y][x];
                        ids.rootADT    = r.rootADT;
                        ids.tex0ADT    = r.tex0ADT;
                        ids.obj0ADT    = r.obj0ADT;
                        ids.minimapBlp = r.minimapTexture;
                        ids.present    = (r.rootADT != 0);
                    }
                }
            }
            else
            {
                info.warnings.push_back(
                    "MAID chunk size " + std::to_string(maidSize) +
                    " < expected " + std::to_string(expected));
            }
        }

        // MAIN — every WDT has this. Use it to confirm 'present' bits if
        // MAID is missing.
        uint8 const* mainBody = nullptr;
        std::size_t mainSize = 0;
        uint8 const* mainChunk = nullptr;
        if (FindTopLevelChunk(bytes.data(), bytes.size(), kFccMAIN,
                              mainBody, mainSize, mainChunk))
        {
            constexpr std::size_t kCellSize = sizeof(uint32) * 2;   // flag + data1
            if (mainSize >= 64ull * 64ull * kCellSize)
            {
                struct MainCell
                {
                    uint32 flag;
                    uint32 data1;
                };
                MainCell const* cells =
                    reinterpret_cast<MainCell const*>(mainBody);
                for (std::size_t y = 0; y < 64; ++y)
                    for (std::size_t x = 0; x < 64; ++x)
                        if ((cells[y * 64 + x].flag & 0x1u) != 0)
                            info.adts[y][x].present = true;
            }
        }

        return info;
    }

    // -----------------------------------------------------------------------
    // ADT reading
    //
    // Walks root.adt for MCNK base info, then tex0.adt for MCLY/MCAL/MTEX
    // (or MDID). For legacy (pre-Cata) single-file ADTs, tex0AdtFileDataId
    // will be 0 and all data lives in root.adt.
    // -----------------------------------------------------------------------

    namespace
    {
        // adt_MCNK layout subset we need (matches the existing adt.h struct
        // up through 'effectId'). Repeated here so this file is self-
        // contained for testing.
        #pragma pack(push, 1)
        struct McnkHeader
        {
            uint32 fcc;            // "MCNK"
            uint32 size;
            uint32 flags;
            uint32 ix;
            uint32 iy;
            uint32 nLayers;
            uint32 nDoodadRefs;
            uint32 offset_or_holes_1;
            uint32 offset_or_holes_2;
            uint32 offsMCLY;
            uint32 offsMCRF;
            uint32 offsMCAL;
            uint32 sizeMCAL;
            uint32 offsMCSH;
            uint32 sizeMCSH;
            uint32 areaid;
            uint32 nMapObjRefs;
            uint32 holes;
            uint16 s0;
            uint16 s1;
            uint32 data1;
            uint32 data2;
            uint32 data3;
            uint32 predTex;
            uint32 nEffectDoodad;
            uint32 offsMCSE;
            uint32 nSndEmitters;
            uint32 offsMCLQ;
            uint32 sizeMCLQ;
            float  zpos;
            float  xpos;
            float  ypos;
            uint32 offsMCCV;
            uint32 props;
            uint32 effectId;
        };
        static_assert(sizeof(McnkHeader) == 8 + 128,
                      "McnkHeader layout drift");

        // MCLY layer record — 16 bytes per layer.
        struct MclyEntry
        {
            uint32 textureId;
            uint32 flags;
            uint32 offsetInMCAL;
            uint32 effectId;
        };
        static_assert(sizeof(MclyEntry) == 16,
                      "MclyEntry layout drift");
        #pragma pack(pop)

        // Parse MCLY into LayerInfo entries with alphaOffset populated.
        // textureBlp is left empty here; the reader fills it after MTEX/MDID
        // resolution.
        void ParseMcly(uint8 const* body, std::size_t bodySize,
                       uint32 nLayers, McnkTextureSummary& out,
                       std::vector<std::string>& warnings)
        {
            std::size_t entryCount = bodySize / sizeof(MclyEntry);
            uint32 useLayers = std::min<uint32>({
                nLayers,
                static_cast<uint32>(entryCount),
                static_cast<uint32>(kMaxLayersPerMcnk)
            });

            if (entryCount != nLayers)
            {
                warnings.push_back(
                    "MCLY entry count " + std::to_string(entryCount) +
                    " != MCNK nLayers " + std::to_string(nLayers));
            }

            MclyEntry const* entries =
                reinterpret_cast<MclyEntry const*>(body);
            for (uint32 i = 0; i < useLayers; ++i)
            {
                LayerInfo& li = out.layers[i];
                li.layerIdx    = i;
                li.textureIdx  = entries[i].textureId;
                li.mclyFlags   = entries[i].flags;
                li.alphaOffset = entries[i].offsetInMCAL;
                li.effectId    = entries[i].effectId;
            }
        }
    }

    // -----------------------------------------------------------------------
    // The reader
    // -----------------------------------------------------------------------

    AdtTextureReader::AdtTextureReader(std::shared_ptr<CASC::Storage const> storage)
        : _storage(std::move(storage))
    {
    }

    std::optional<AdtTextureSummary> AdtTextureReader::ReadAdt(
        uint32 mapId, uint8 adtX, uint8 adtY,
        WdtFlags const& flags,
        uint32 rootAdtFileDataId,
        uint32 tex0AdtFileDataId)
    {
        if (!_storage)
            return std::nullopt;

        std::vector<uint8> rootBytes;
        if (!ReadCascFileBytes(*_storage, rootAdtFileDataId, rootBytes))
            return std::nullopt;

        AdtTextureSummary out;
        out.mapId = mapId;
        out.adtX  = adtX;
        out.adtY  = adtY;
        out.wdtFlags = flags;

        // ---- Phase 1: walk root.adt MCNKs for base info ----
        {
            std::size_t mcnkSeqIdx = 0;
            ForEachTopLevelChunk(rootBytes.data(), rootBytes.size(),
                [&](uint32 magic, uint8 const* body, std::size_t size,
                    uint8 const* /*chunkStart*/) {
                    if (magic != kFccMCNK) return true;
                    if (size < sizeof(McnkHeader) - 8) return true;  // body smaller than header
                    if (mcnkSeqIdx >= kMcnksPerAdt) return false;

                    // body == &McnkHeader.flags (since fcc+size are 8 bytes
                    // before body). Reinterpret carefully:
                    auto const* mcnkData =
                        reinterpret_cast<McnkHeader const*>(body - 8);

                    McnkTextureSummary& m = out.mcnks[mcnkSeqIdx];
                    m.mcnkIdx = static_cast<uint16>(mcnkSeqIdx);
                    m.ix      = static_cast<uint8>(mcnkData->ix);
                    m.iy      = static_cast<uint8>(mcnkData->iy);
                    m.areaId  = mcnkData->areaid;
                    m.nLayers = mcnkData->nLayers;
                    m.hasHoles = (mcnkData->holes != 0) ||
                                 (mcnkData->flags & 0x10000u) != 0;
                    m.xpos = mcnkData->xpos;
                    m.ypos = mcnkData->ypos;
                    m.zpos = mcnkData->zpos;
                    ++mcnkSeqIdx;
                    return true;
                });
        }

        // ---- Phase 2: locate MTEX or MDID (modern uses MDID + listfile) ----
        // The MTEX/MDID chunk lives in either root.adt (legacy) or tex0.adt
        // (modern split-file). Search both.
        std::vector<std::string> mtexEntries;
        std::vector<uint32>      mdidEntries;

        auto extractMtexMdid = [&](uint8 const* data, std::size_t size) {
            uint8 const* body = nullptr;
            std::size_t bodySize = 0;
            uint8 const* chunkStart = nullptr;
            if (mtexEntries.empty() && FindTopLevelChunk(data, size, kFccMTEX,
                                                          body, bodySize, chunkStart))
                mtexEntries = ParseMtex(body, bodySize);
            if (mdidEntries.empty() && FindTopLevelChunk(data, size, kFccMDID,
                                                          body, bodySize, chunkStart))
                mdidEntries = ParseMdid(body, bodySize);
        };

        extractMtexMdid(rootBytes.data(), rootBytes.size());

        // ---- Phase 3: walk tex0.adt (if present) for MCLY + MCAL ----
        std::vector<uint8> texBytes;
        bool haveTex = false;
        if (tex0AdtFileDataId != 0)
            haveTex = ReadCascFileBytes(*_storage, tex0AdtFileDataId, texBytes);
        else
            out.warnings.push_back("no tex0.adt FileDataID — using root.adt for texture chunks");

        if (haveTex)
            extractMtexMdid(texBytes.data(), texBytes.size());

        // The MCNK source for MCLY/MCAL: tex0 if available, else root.
        uint8 const* texSource     = haveTex ? texBytes.data() : rootBytes.data();
        std::size_t  texSourceSize = haveTex ? texBytes.size() : rootBytes.size();

        std::size_t mcnkSeqIdx = 0;
        ForEachTopLevelChunk(texSource, texSourceSize,
            [&](uint32 magic, uint8 const* body, std::size_t size,
                uint8 const* chunkStart) {
                if (magic != kFccMCNK) return true;
                if (mcnkSeqIdx >= kMcnksPerAdt) return false;

                McnkTextureSummary& m = out.mcnks[mcnkSeqIdx];

                // Modern split-file ADT path: tex0.adt's MCNK body is a
                // stream of sub-chunks (MCLY + MCAL [+ MCSH]) with NO
                // 128-byte legacy header. Walk by FourCC linearly — this
                // works for both legacy (skips through the header bytes
                // looking for FourCCs) and modern (sub-chunks at body[0]).
                //
                // We derive nLayers from MCLY's body size: each MCLY entry
                // is 16 bytes (textureId + flags + offsetInMCAL + effectId).
                uint8 const* mclyBody = nullptr;
                std::size_t mclyBodySize = 0;
                if (SearchMcnkSubChunkByFcc(body, size, kFccMCLY,
                                              mclyBody, mclyBodySize))
                {
                    uint32 mclyDerivedLayers =
                        static_cast<uint32>(mclyBodySize / sizeof(MclyEntry));
                    m.nLayers = std::max<uint32>(m.nLayers, mclyDerivedLayers);
                    ParseMcly(mclyBody, mclyBodySize, m.nLayers, m,
                              out.warnings);
                }

                // Resolve BLP texture paths from MDID (preferred, Legion+
                // FileDataID lookup via listfile) or MTEX (legacy string
                // paths). When BOTH chunks exist, MDID wins — modern ADTs
                // often carry MTEX as a placeholder of empty strings and
                // the real data is in MDID. Trying MTEX first picks the
                // empty string and silently loses every modern road.
                //
                // Resolution runs BEFORE MCAL gating because many MCNKs
                // have MCLY without MCAL (uniform-texture cells). Those
                // resolve to layer 0 and we still need its textureBlp.
                uint32 layersToProcess =
                    std::min<uint32>(m.nLayers, kMaxLayersPerMcnk);
                bool const haveMdid = !mdidEntries.empty();
                for (uint32 i = 0; i < layersToProcess; ++i)
                {
                    uint32 idx = m.layers[i].textureIdx;
                    if (haveMdid)
                    {
                        if (idx < mdidEntries.size())
                        {
                            uint32 fdid = mdidEntries[idx];
                            if (_listfile)
                            {
                                if (auto resolved = _listfile->Lookup(fdid))
                                    m.layers[i].textureBlp = std::string(*resolved);
                                else
                                    m.layers[i].textureBlp =
                                        "[FDID:" + std::to_string(fdid) + "]";
                            }
                            else
                            {
                                m.layers[i].textureBlp =
                                    "[FDID:" + std::to_string(fdid) + "]";
                            }
                        }
                    }
                    else if (!mtexEntries.empty())
                    {
                        if (idx < mtexEntries.size())
                            m.layers[i].textureBlp = mtexEntries[idx];
                    }
                }

                // Always run dominant aggregation — when MCAL is absent,
                // alphas are all-zero so layer 0 wins (uniform-texture
                // case). When MCAL is present, decode and aggregate.
                std::array<std::array<uint8, kAlphaPixels>,
                           kMaxLayersPerMcnk> alphas{};
                uint8 const* mcalBody = nullptr;
                std::size_t mcalBodySize = 0;
                if (m.nLayers > 0 &&
                    SearchMcnkSubChunkByFcc(body, size, kFccMCAL,
                                              mcalBody, mcalBodySize))
                {
                    // Layer 0 has no alpha (implicit base) — leave
                    // alphas[0] zeroed. Layers 1..(n-1) decode from
                    // MCAL at their declared offsetInMCAL.
                    for (uint32 i = 1; i < layersToProcess; ++i)
                    {
                        uint32 ofs = m.layers[i].alphaOffset;
                        if (ofs >= mcalBodySize)
                            continue;
                        DecodeMcalAlpha(mcalBody + ofs,
                                        mcalBodySize - ofs,
                                        m.layers[i].mclyFlags,
                                        flags.UseFullByteAlpha(),
                                        alphas[i]);
                    }
                }
                if (m.nLayers > 0)
                    AggregateDominantLayer(m, alphas);

                ++mcnkSeqIdx;
                return true;
            });

        return out;
    }
}
