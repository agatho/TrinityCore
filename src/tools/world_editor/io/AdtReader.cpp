/*
 * AdtReader - implementation.
 *
 * Chunk-walking patterned after extractor_common/AdtTextureReader.cpp;
 * MCVT / MCNK header layout patterned after map_extractor/adt.h.
 * Repeated here (not shared) to keep map_extractor stable and the
 * editor target self-contained.
 */

#include "AdtReader.h"

#include "CascClient.h"

#include <QDebug>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace world_editor::io
{

namespace
{

constexpr float kTileSize  = 533.33333f;
constexpr float kChunkSize = kTileSize / 16.0f;       // 33.33333
constexpr float kUnitSize  = kChunkSize / 8.0f;       // 4.16666...

// FourCC bytes-on-disk values.  See AdtTextureReader.cpp for the same
// pattern (chunks were historically written as reversed 32-bit chars).
constexpr uint32_t MakeFourCC(char a, char b, char c, char d) noexcept
{
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

constexpr uint32_t kFccMHDR = MakeFourCC('R', 'D', 'H', 'M');
constexpr uint32_t kFccMCNK = MakeFourCC('K', 'N', 'C', 'M');
constexpr uint32_t kFccMCVT = MakeFourCC('T', 'V', 'C', 'M');
constexpr uint32_t kFccMCCV = MakeFourCC('V', 'C', 'C', 'M');
constexpr uint32_t kFccMCLY = MakeFourCC('Y', 'L', 'C', 'M');
constexpr uint32_t kFccMCAL = MakeFourCC('L', 'A', 'C', 'M');
constexpr uint32_t kFccMTEX = MakeFourCC('X', 'E', 'T', 'M');
constexpr uint32_t kFccMDID = MakeFourCC('D', 'I', 'D', 'M');
constexpr uint32_t kFccMPHD = MakeFourCC('D', 'H', 'P', 'M');
// STAGE B: MHID (height-texture FileDataIDs, parallel to MDID) and MTXP
// (per-texture parameters: scale bits + parallax height scale/offset).  Both
// are tex0.adt top-level chunks (same scope as MDID/MTEX), so the disk token
// is byte-reversed exactly like the constants above ("MHID" -> 'D','I','H','M').
constexpr uint32_t kFccMHID = MakeFourCC('D', 'I', 'H', 'M');
constexpr uint32_t kFccMTXP = MakeFourCC('P', 'X', 'T', 'M');

struct ChunkHeader
{
    uint32_t magic;
    uint32_t size;
};

#pragma pack(push, 1)
struct McnkHeader
{
    uint32_t fcc;
    uint32_t size;
    uint32_t flags;
    uint32_t ix;
    uint32_t iy;
    uint32_t nLayers;
    uint32_t nDoodadRefs;
    union
    {
        struct
        {
            uint32_t offsMCVT;
            uint32_t offsMCNR;
        } offsets;
        uint8_t HighResHoles[8];
    } union_5_3_0;
    uint32_t offsMCLY;
    uint32_t offsMCRF;
    uint32_t offsMCAL;
    uint32_t sizeMCAL;
    uint32_t offsMCSH;
    uint32_t sizeMCSH;
    uint32_t areaid;
    uint32_t nMapObjRefs;
    uint32_t holes;
    uint16_t s0;
    uint16_t s1;
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
    uint32_t predTex;
    uint32_t nEffectDoodad;
    uint32_t offsMCSE;
    uint32_t nSndEmitters;
    uint32_t offsMCLQ;
    uint32_t sizeMCLQ;
    float    zpos;
    float    xpos;
    float    ypos;   // map_extractor uses this as the base Z height
    uint32_t offsMCCV;
    uint32_t props;
    uint32_t effectId;
};
static_assert(sizeof(McnkHeader) == 8 + 128, "McnkHeader layout drift");

struct MclyEntry
{
    uint32_t textureId;
    uint32_t flags;
    uint32_t offsetInMCAL;
    uint32_t effectId;
};
static_assert(sizeof(MclyEntry) == 16, "MclyEntry layout drift");

// MTXP per-texture parameters (one 16-byte record per texture, same index
// space as MDID/MHID).  Canonical wowdev layout (verified against wow.export's
// ADTLoader: it reads `flags`, then `heightScale`, then `heightOffset`, then a
// trailing padding word).  The consumed parallax fields are:
//   heightScale  -> u_heightScale[layer]  (multiplies the sampled height)
//   heightOffset -> u_heightOffset[layer] (additive bias; 1.0 = flat/neutral)
// and the UV-tiling scale is 2^((flags & 0xF0) >> 4).  Word order is
// {flags, heightScale, heightOffset, padding} -- words 0,1,2 consumed, word 3
// ignored.  Defaults when MTXP is absent: heightScale=0, heightOffset=1 so the
// blend degrades to pure-alpha.
struct MtxpEntry
{
    uint32_t flags;
    float    heightScale;
    float    heightOffset;
    uint32_t padding;
};
static_assert(sizeof(MtxpEntry) == 16, "MtxpEntry layout drift");
#pragma pack(pop)

template<typename Cb>
void ForEachTopLevelChunk(uint8_t const* data, std::size_t size, Cb&& cb)
{
    std::size_t pos = 0;
    while (pos + sizeof(ChunkHeader) <= size)
    {
        auto const* hdr = reinterpret_cast<ChunkHeader const*>(data + pos);
        std::size_t bodyOffset = pos + sizeof(ChunkHeader);
        std::size_t bodyEnd    = bodyOffset + hdr->size;
        if (bodyEnd > size)
            break;
        if (!cb(hdr->magic, data + bodyOffset, hdr->size))
            return;
        pos = bodyEnd;
    }
}

bool FindTopLevelChunk(uint8_t const* data, std::size_t size, uint32_t fcc,
                       uint8_t const*& outBody, std::size_t& outBodySize)
{
    bool found = false;
    ForEachTopLevelChunk(data, size,
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
        {
            if (magic == fcc)
            {
                outBody = body;
                outBodySize = bodySize;
                found = true;
                return false;
            }
            return true;
        });
    return found;
}

// Find a sub-chunk of the given FourCC inside an MCNK body.  `mcnkBody`
// is the byte after the MCNK FourCC+size (i.e. mcnkStart+8).
//
// CRITICAL: this must NOT advance by the trusted chunk size the way a
// top-level walker does.  The ROOT-file MCNK begins with a 128-byte
// header (NOT a chunk), and modern split-file MCNKs interleave sub-chunks
// whose order/presence varies.  A size-trusting walk that starts on the
// header reads a garbage "size" from header bytes and derails to a random
// offset -- which is why some chunks found MCVT, some missed it (flat),
// and some read garbage floats (spikes).
//
// We instead replicate TC's FileChunk::parseSubChunks (loadlib.cpp): scan
// byte-by-byte for the EXACT target magic, and only trust the following
// u32 as a size once the magic matches (with a bounds check to reject the
// rare false-positive 4-byte coincidence).  Works for both the headered
// root file and the headerless tex0/obj0 files with no special-casing.
bool SearchMcnkSubChunk(uint8_t const* mcnkBody, std::size_t mcnkBodySize,
                        uint32_t fcc,
                        uint8_t const*& outBody, std::size_t& outBodySize)
{
    if (mcnkBodySize < 8)
        return false;
    for (std::size_t pos = 0; pos + 8 <= mcnkBodySize; ++pos)
    {
        uint32_t magic;
        std::memcpy(&magic, mcnkBody + pos, 4);
        if (magic != fcc)
            continue;
        uint32_t size;
        std::memcpy(&size, mcnkBody + pos + 4, 4);
        if (std::size_t(pos) + 8 + size > mcnkBodySize)
            continue;   // 4-byte coincidence, not a real chunk -- keep scanning.
        outBody = mcnkBody + pos + 8;
        outBodySize = size;
        return true;
    }
    return false;
}

std::vector<std::string> ParseMtex(uint8_t const* body, std::size_t size)
{
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < size)
    {
        std::size_t end = pos;
        while (end < size && body[end] != '\0')
            ++end;
        out.emplace_back(reinterpret_cast<char const*>(body + pos), end - pos);
        pos = end + 1;
    }
    return out;
}

std::vector<uint32_t> ParseMdid(uint8_t const* body, std::size_t size)
{
    std::vector<uint32_t> out(size / sizeof(uint32_t));
    if (!out.empty())
        std::memcpy(out.data(), body, out.size() * sizeof(uint32_t));
    return out;
}

// Decode an MCAL alpha entry.  Matches AdtTextureReader::DecodeMcalAlpha
// (three branches: RLE / 8-bit direct / 4-bit nibble-packed).
bool DecodeMcalAlpha(uint8_t const* src, std::size_t srcSize,
                     uint32_t mclyFlags, bool useFullByteAlpha,
                     std::array<uint8_t, 4096>& dst)
{
    dst.fill(0);

    if (mclyFlags & 0x200u)
    {
        std::size_t srcPos = 0, dstPos = 0;
        while (dstPos < dst.size() && srcPos < srcSize)
        {
            uint8_t control = src[srcPos++];
            bool fill   = (control & 0x80u) != 0;
            uint8_t count = control & 0x7Fu;
            if (count == 0)
                break;
            if (fill)
            {
                if (srcPos >= srcSize) return false;
                uint8_t value = src[srcPos++];
                for (uint8_t i = 0; i < count && dstPos < dst.size(); ++i)
                    dst[dstPos++] = value;
            }
            else
            {
                for (uint8_t i = 0; i < count && dstPos < dst.size(); ++i)
                {
                    if (srcPos >= srcSize) return false;
                    dst[dstPos++] = src[srcPos++];
                }
            }
        }
        return dstPos == dst.size();
    }

    if (useFullByteAlpha)
    {
        if (srcSize < dst.size())
            return false;
        std::memcpy(dst.data(), src, dst.size());
        return true;
    }

    if (srcSize < 2048)
        return false;
    std::size_t dstPos = 0;
    for (std::size_t i = 0; i < 2048; ++i)
    {
        uint8_t b = src[i];
        dst[dstPos++] = static_cast<uint8_t>((b & 0x0Fu) * 17u);
        dst[dstPos++] = static_cast<uint8_t>(((b >> 4) & 0x0Fu) * 17u);
    }
    return true;
}

// Look up MPHD flags inside a WDT blob.  Returns 0 when no MPHD found.
uint32_t ParseWdtFlags(std::vector<uint8_t> const& wdt)
{
    uint8_t const* body = nullptr;
    std::size_t bodySize = 0;
    if (!FindTopLevelChunk(wdt.data(), wdt.size(), kFccMPHD, body, bodySize))
        return 0;
    if (bodySize < sizeof(uint32_t))
        return 0;
    uint32_t flags = 0;
    std::memcpy(&flags, body, sizeof(uint32_t));
    return flags;
}

} // namespace

bool loadAdtTile(CascClient& casc,
                 std::string const& mapDir,
                 uint32_t mapId,
                 int gx, int gy,
                 AdtTile& out,
                 uint32_t rootFdid,
                 uint32_t tex0Fdid)
{
    out = {};
    out.mapId = mapId;
    out.gx = gx;
    out.gy = gy;

    if (mapDir.empty() || !casc.isOpen())
        return false;

    auto rootPath = [&](char const* suffix) {
        // Virtual-path fallback for Classic-era maps whose WDT predates
        // the MAID chunk.  Filename order matches TC's map_extractor:
        //   `<map>_<gx>_<gy>{suffix}.adt`
        // where gx is the row (north-south) and gy is the column
        // (east-west) -- wow.export's `WDTLoader.js` documents this as
        // the on-disk Blizzard convention.  CascClient::readByPath has
        // a listfile-FDID fallback when the path isn't in CASC's root.
        std::string p = "world/maps/";
        p += mapDir;
        p += '/';
        p += mapDir;
        p += '_';
        p += std::to_string(gx);
        p += '_';
        p += std::to_string(gy);
        p += suffix;
        return p;
    };

    // ---- Load root.adt + (optional) tex0.adt + WDT ----
    // Preferred path: FDID lookup via WDT MAID (caller-supplied).  Falls
    // back to virtual-path resolution when the caller didn't have a WDT
    // FDID (Classic maps / unit tests that bypass the WDT).
    std::vector<uint8_t> rootBytes;
    if (rootFdid != 0)
    {
        if (!casc.readByFileDataId(rootFdid, rootBytes) || rootBytes.empty())
            return false;
    }
    else
    {
        if (!casc.readByPath(rootPath(".adt"), rootBytes) || rootBytes.empty())
            return false;
    }

    std::vector<uint8_t> texBytes;
    bool haveTex = false;
    if (tex0Fdid != 0)
    {
        haveTex = casc.readByFileDataId(tex0Fdid, texBytes) && !texBytes.empty();
    }
    else
    {
        haveTex = casc.readByPath(rootPath("_tex0.adt"), texBytes)
                  && !texBytes.empty();
    }

    // WDT.MPHD tells us whether MCAL alphas are 8-bit (4096 bytes) or
    // 4-bit nibble-packed (2048 bytes).  Missing WDT == legacy alphas.
    uint32_t mphdFlags = 0;
    {
        std::vector<uint8_t> wdtBytes;
        std::string wdtPath = "world/maps/" + mapDir + "/" + mapDir + ".wdt";
        if (casc.readByPath(wdtPath, wdtBytes) && !wdtBytes.empty())
            mphdFlags = ParseWdtFlags(wdtBytes);
    }
    bool const useFullByteAlpha = (mphdFlags & 0x4u) != 0 || (mphdFlags & 0x80u) != 0;

    // ---- Phase 1: locate MTEX / MDID / MHID / MTXP (in either root or tex0) ----
    std::vector<std::string> mtexEntries;
    std::vector<uint32_t>    mdidEntries;
    std::vector<uint32_t>    mhidEntries;   // STAGE B: height-texture FileDataIDs
    std::vector<MtxpEntry>   mtxpEntries;   // STAGE B: per-texture parameters
    auto extractTextureTables = [&](uint8_t const* data, std::size_t size)
    {
        uint8_t const* body = nullptr;
        std::size_t bodySize = 0;
        if (mtexEntries.empty() && FindTopLevelChunk(data, size, kFccMTEX, body, bodySize))
            mtexEntries = ParseMtex(body, bodySize);
        if (mdidEntries.empty() && FindTopLevelChunk(data, size, kFccMDID, body, bodySize))
            mdidEntries = ParseMdid(body, bodySize);
        // MHID is a flat uint32[] of height-texture FileDataIDs in the same
        // index space as MDID -- parse identically.
        if (mhidEntries.empty() && FindTopLevelChunk(data, size, kFccMHID, body, bodySize))
            mhidEntries = ParseMdid(body, bodySize);
        // MTXP is an array of 16-byte records, one per texture id.
        if (mtxpEntries.empty() && FindTopLevelChunk(data, size, kFccMTXP, body, bodySize))
        {
            std::size_t const n = bodySize / sizeof(MtxpEntry);
            mtxpEntries.resize(n);
            if (n)
                std::memcpy(mtxpEntries.data(), body, n * sizeof(MtxpEntry));
        }
    };
    extractTextureTables(rootBytes.data(), rootBytes.size());
    if (haveTex)
        extractTextureTables(texBytes.data(), texBytes.size());

    // ---- Phase 2: walk root.adt MCNKs for header + MCVT heights ----
    out.chunks.resize(256);
    {
        std::size_t mcnkSeq = 0;
        ForEachTopLevelChunk(rootBytes.data(), rootBytes.size(),
            [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
            {
                if (magic != kFccMCNK) return true;
                if (bodySize < sizeof(McnkHeader) - 8) return true;
                if (mcnkSeq >= out.chunks.size()) return false;

                auto const* hdr = reinterpret_cast<McnkHeader const*>(body - 8);
                AdtChunk& ch = out.chunks[mcnkSeq];
                ch.ix = int(hdr->ix);
                ch.iy = int(hdr->iy);

                // Capture chunk(0,0)'s intrinsic world position from the MCNK
                // header so the caller can detect a wrong-tile / transposed
                // load.  TC convention: zpos = world X (north-south), xpos =
                // world Y (east-west).
                if (hdr->ix == 0 && hdr->iy == 0)
                {
                    out.intrinsicX0 = hdr->zpos;
                    out.intrinsicY0 = hdr->xpos;
                    out.hasIntrinsicPos = true;
                }

                // Chunk world XY footprint.  CRITICAL: world X is driven by
                // the MCNK *iy* index and world Y by *ix* -- matching TC's
                // map_extractor, which writes V9[cy=iy*8+y][cx=ix*8+x] and
                // GridMap/MapReader, which read V9[row=gridX(worldX)][col=
                // gridY(worldY)].  i.e. iy -> worldX, ix -> worldY.  Using
                // ix for X / iy for Y (the intuitive-but-wrong order) draws
                // every chunk at the diagonally-transposed cell, so chunks
                // show neighbouring-but-wrong data and don't tile (the
                // "rotated chunks / black gaps" artifact).  The chunk(0,0)
                // intrinsic-position check can't catch this because ix==iy==0
                // there.
                //
                // World-grid geometry: the world is a 64x64 grid of
                // 533.33331 yd tiles (constant 0x44055555), each tile a 16x16
                // grid of 33.333332 yd MCNK chunks (0x42055555).  The origin
                // sits at +17066.666 (0x46855555 = 32 * 533.333) on BOTH
                // horizontal axes, and tile/chunk indices grow as the world
                // coordinate DECREASES (inverted axis): the client computes
                //   chunkIndex = (17066.666 - coord) / 533.333
                // which is algebraically identical to this code's
                //   (32 - coord/size)
                // form.  This was proven at the instruction level in the
                // client's tile-windowing path and CMapChunk::Create.
                //
                // DO NOT swap ix<->iy here.  map_extractor is the source of
                // truth for V9/V8 layout, and the iy->worldX / ix->worldY
                // transpose above is verified-consistent with the client.
                // "Fixing" it to the intuitive order silently corrupts every
                // tile.
                float const tileMaxX = (32 - gx) * kTileSize;
                float const tileMaxY = (32 - gy) * kTileSize;
                float const chunkMaxX = tileMaxX - float(hdr->iy) * kChunkSize;
                float const chunkMaxY = tileMaxY - float(hdr->ix) * kChunkSize;
                ch.minX = chunkMaxX - kChunkSize;
                ch.minY = chunkMaxY - kChunkSize;

                // Holes: low-res (uint16) by default, high-res 8-byte
                // bitmask when MCNK.flags bit 0x10000 is set.
                if (hdr->flags & 0x10000u)
                {
                    std::memcpy(&ch.holesMask,
                                hdr->union_5_3_0.HighResHoles,
                                sizeof(uint64_t));
                }
                else if (hdr->holes != 0)
                {
                    // Low-res holes: 4x4 grid (16 bits), promote to 8x8.
                    uint64_t mask = 0;
                    for (int r = 0; r < 4; ++r)
                    {
                        for (int c = 0; c < 4; ++c)
                        {
                            int const bit = r * 4 + c;
                            if (hdr->holes & (1u << bit))
                            {
                                // Each low-res cell covers a 2x2 high-res block.
                                for (int dr = 0; dr < 2; ++dr)
                                {
                                    for (int dc = 0; dc < 2; ++dc)
                                    {
                                        int const rr = r * 2 + dr;
                                        int const cc = c * 2 + dc;
                                        mask |= (uint64_t(1) << (rr * 8 + cc));
                                    }
                                }
                            }
                        }
                    }
                    ch.holesMask = mask;
                }

                // Base Z (ADT MCNK.ypos in TC convention).  Default BOTH the
                // V9 outer grid AND the V8 inner (cell-centre) grid to the base
                // height, then MCVT adds per-vertex deltas.  Defaulting V8 is
                // essential: a chunk with no MCVT (flat / parse-missed) used to
                // leave heightsV8 at the zero-initialised 0.0 while the corners
                // sat at baseZ, so the 4-tris-per-cell fan plunged from terrain
                // height down to z=0 -- the "shattered spike" artifact.  A flat
                // chunk's centres ARE at base height, so this is also correct.
                float const baseZ = hdr->ypos;
                for (int i = 0; i < 81; ++i)
                    ch.heights[i] = baseZ;
                for (int i = 0; i < 64; ++i)
                    ch.heightsV8[i] = baseZ;
                // Default normals to straight up so an MCNR-less chunk shades
                // flat-lit instead of black (zero normal -> Lambert 0).
                for (int i = 0; i < 81; ++i)
                {
                    ch.normals[i][0] = 0.0f; ch.normals[i][1] = 0.0f; ch.normals[i][2] = 1.0f;
                }
                for (int i = 0; i < 64; ++i)
                {
                    ch.normalsV8[i][0] = 0.0f; ch.normalsV8[i][1] = 0.0f; ch.normalsV8[i][2] = 1.0f;
                }

                // SearchMcnkSubChunk byte-scans for the exact magic, so it
                // transparently skips the ROOT MCNK's 128-byte header (and
                // works for the headerless tex0/obj0 shape too) -- no manual
                // header offset needed.
                uint8_t const* mcvtBody = nullptr;
                std::size_t mcvtSize = 0;
                if (SearchMcnkSubChunk(body, bodySize, kFccMCVT, mcvtBody, mcvtSize))
                {
                    // MCVT layout: 145 floats interleaved.  V9[y][x] for
                    // y,x in [0..8] lives at index y*17 + x; the V8 inner
                    // grid (8x8) at y*17 + 9 + x.  We keep only V9 for
                    // the editor's v1.
                    auto const* mcvt = reinterpret_cast<float const*>(mcvtBody);
                    std::size_t const floats = mcvtSize / sizeof(float);
                    if (floats >= 145)
                    {
                        // V9 outer 9x9 at index y*17 + x.
                        for (int y = 0; y <= 8; ++y)
                            for (int x = 0; x <= 8; ++x)
                                ch.heights[y * 9 + x] = baseZ + mcvt[y * 17 + x];
                        // V8 inner 8x8 (cell centres) at index y*17 + 9 + x.
                        for (int y = 0; y < 8; ++y)
                            for (int x = 0; x < 8; ++x)
                                ch.heightsV8[y * 8 + x] = baseZ + mcvt[y * 17 + 9 + x];
                    }
                }

                // MCCV: per-vertex 145 BGRA bytes laid out exactly like MCVT's
                // 145-float V9+V8 grid.  Only the 81 V9 outer-grid entries are
                // surfaced (matches wow.export's `TerrainRenderer.js:643-656`).
                // Disk byte order is BGRA; we swizzle to RGBA so the shader's
                // `color.rgb * 2.0` multiplier matches retail terrain shading.
                // Default is neutral grey (0x7F == 0.5; 2x in shader == 1.0).
                for (int i = 0; i < 81; ++i)
                {
                    ch.mccv[i][0] = 0x7F;
                    ch.mccv[i][1] = 0x7F;
                    ch.mccv[i][2] = 0x7F;
                    ch.mccv[i][3] = 0xFF;
                }
                uint8_t const* mccvBody = nullptr;
                std::size_t mccvSize = 0;
                if (SearchMcnkSubChunk(body, bodySize, kFccMCCV, mccvBody, mccvSize)
                    && mccvSize >= 145u * 4u)
                {
                    for (int y = 0; y <= 8; ++y)
                    {
                        for (int x = 0; x <= 8; ++x)
                        {
                            uint8_t const* src = mccvBody + (y * 17 + x) * 4;
                            // disk order = B, G, R, A
                            ch.mccv[y * 9 + x][0] = src[2];   // R
                            ch.mccv[y * 9 + x][1] = src[1];   // G
                            ch.mccv[y * 9 + x][2] = src[0];   // B
                            ch.mccv[y * 9 + x][3] = src[3];   // A
                        }
                    }
                    ch.hasMccv = true;
                }

                ++mcnkSeq;
                return true;
            });
        // ADTs occasionally have fewer than 256 MCNKs at the corner of
        // the world; shrink so callers don't iterate zero-initialised
        // tails as if they were chunks.
        if (mcnkSeq < out.chunks.size())
            out.chunks.resize(mcnkSeq);
    }

    // Derive per-vertex normals from finite differences on the V9 grid.
    // For boundary samples the centred difference clamps to the nearest
    // interior step (one-sided) -- adequate for screen-space shading.
    for (AdtChunk& ch : out.chunks)
    {
        for (int y = 0; y <= 8; ++y)
        {
            for (int x = 0; x <= 8; ++x)
            {
                int const yL = std::max(0, y - 1);
                int const yR = std::min(8, y + 1);
                int const xL = std::max(0, x - 1);
                int const xR = std::min(8, x + 1);
                float const hYL = ch.heights[yL * 9 + x];
                float const hYR = ch.heights[yR * 9 + x];
                float const hXL = ch.heights[y * 9 + xL];
                float const hXR = ch.heights[y * 9 + xR];
                float const dY = float(yR - yL) * kUnitSize;
                float const dX = float(xR - xL) * kUnitSize;
                // Tangent along the row axis (TC -X): (-dY, 0, hYR-hYL).
                // Tangent along the col axis (TC -Y): (0, -dX, hXR-hXL).
                float const tRx = -dY, tRy = 0.0f, tRz = hYR - hYL;
                float const tCx = 0.0f, tCy = -dX, tCz = hXR - hXL;
                float nx = tRy * tCz - tRz * tCy;
                float ny = tRz * tCx - tRx * tCz;
                float nz = tRx * tCy - tRy * tCx;
                if (nz < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
                float const len = std::sqrt(nx * nx + ny * ny + nz * nz);
                int const idx = y * 9 + x;
                if (len > 1e-6f)
                {
                    ch.normals[idx][0] = nx / len;
                    ch.normals[idx][1] = ny / len;
                    ch.normals[idx][2] = nz / len;
                }
                else
                {
                    ch.normals[idx][0] = 0.0f;
                    ch.normals[idx][1] = 0.0f;
                    ch.normals[idx][2] = 1.0f;
                }
            }
        }
        // V8 centre normals: average of the 4 surrounding V9 corner normals,
        // renormalised.  Cheap and smooth enough for screen-space shading.
        for (int r = 0; r < 8; ++r)
        {
            for (int c = 0; c < 8; ++c)
            {
                float nx = ch.normals[r * 9 + c][0] + ch.normals[r * 9 + (c + 1)][0]
                         + ch.normals[(r + 1) * 9 + c][0] + ch.normals[(r + 1) * 9 + (c + 1)][0];
                float ny = ch.normals[r * 9 + c][1] + ch.normals[r * 9 + (c + 1)][1]
                         + ch.normals[(r + 1) * 9 + c][1] + ch.normals[(r + 1) * 9 + (c + 1)][1];
                float nz = ch.normals[r * 9 + c][2] + ch.normals[r * 9 + (c + 1)][2]
                         + ch.normals[(r + 1) * 9 + c][2] + ch.normals[(r + 1) * 9 + (c + 1)][2];
                float const len = std::sqrt(nx * nx + ny * ny + nz * nz);
                int const idx = r * 8 + c;
                if (len > 1e-6f) { ch.normalsV8[idx][0] = nx / len; ch.normalsV8[idx][1] = ny / len; ch.normalsV8[idx][2] = nz / len; }
                else             { ch.normalsV8[idx][0] = 0.0f; ch.normalsV8[idx][1] = 0.0f; ch.normalsV8[idx][2] = 1.0f; }
            }
        }
    }

    // ---- Phase 3: walk tex0 (or root) MCNKs for MCLY + MCAL ----
    uint8_t const* texSource     = haveTex ? texBytes.data() : rootBytes.data();
    std::size_t    texSourceSize = haveTex ? texBytes.size() : rootBytes.size();

    std::size_t mcnkSeq = 0;
    ForEachTopLevelChunk(texSource, texSourceSize,
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
        {
            if (magic != kFccMCNK) return true;
            if (mcnkSeq >= out.chunks.size()) return false;
            AdtChunk& ch = out.chunks[mcnkSeq++];

            uint8_t const* mclyBody = nullptr;
            std::size_t mclyBodySize = 0;
            if (!SearchMcnkSubChunk(body, bodySize, kFccMCLY, mclyBody, mclyBodySize))
                return true;

            std::size_t const layerCount = std::min<std::size_t>(8,
                mclyBodySize / sizeof(MclyEntry));
            ch.layers.resize(layerCount);
            auto const* entries = reinterpret_cast<MclyEntry const*>(mclyBody);
            bool const haveMdid = !mdidEntries.empty();
            for (std::size_t i = 0; i < layerCount; ++i)
            {
                AdtLayer& L = ch.layers[i];
                L.flags = entries[i].flags;
                uint32_t const tidx = entries[i].textureId;
                if (haveMdid)
                {
                    if (tidx < mdidEntries.size())
                        L.textureFileDataId = mdidEntries[tidx];
                }
                else if (tidx < mtexEntries.size())
                {
                    L.textureBlpPath = mtexEntries[tidx];
                }

                // STAGE B: surface the parallel height-texture FileDataID + the
                // MTXP per-texture parameters keyed by the SAME texture index.
                if (tidx < mhidEntries.size())
                    L.heightTextureFileDataId = mhidEntries[tidx];
                if (tidx < mtxpEntries.size())
                {
                    MtxpEntry const& p = mtxpEntries[tidx];
                    L.texParamFlags = p.flags;
                    L.heightScale   = p.heightScale;
                    L.heightOffset  = p.heightOffset;
                    // UV-tiling scale = 2^((flags & 0xF0) >> 4).
                    L.layerScale    = float(1u << ((p.flags & 0xF0u) >> 4));
                }
                // else: neutral defaults from AdtLayer (heightScale=0,
                // heightOffset=1, layerScale=1) leave the layer alpha-only.
            }

            // MCAL decode for layers 1..N-1.  Layer 0 = implicit full
            // coverage so we leave its `alpha` empty.
            uint8_t const* mcalBody = nullptr;
            std::size_t mcalBodySize = 0;
            if (layerCount > 1 &&
                SearchMcnkSubChunk(body, bodySize, kFccMCAL, mcalBody, mcalBodySize))
            {
                for (std::size_t i = 1; i < layerCount; ++i)
                {
                    uint32_t const ofs = entries[i].offsetInMCAL;
                    if (ofs >= mcalBodySize)
                        continue;
                    std::array<uint8_t, 4096> decoded{};
                    if (DecodeMcalAlpha(mcalBody + ofs, mcalBodySize - ofs,
                                        entries[i].flags, useFullByteAlpha, decoded))
                    {
                        ch.layers[i].alpha.assign(decoded.begin(), decoded.end());
                    }
                }
            }
            return true;
        });

    return true;
}

} // namespace world_editor::io
