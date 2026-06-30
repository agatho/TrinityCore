/*
 * AdtLiquidReader - implementation.
 *
 * Chunk-walking pattern matches AdtReader.cpp; MH2O offset arithmetic
 * follows src/tools/map_extractor/adt.h.  We parse two surface formats:
 *
 *   MH2O  (Cata+): top-level chunk right after MHDR.  Body starts with a
 *                  16x16 array of adt_LIQUID headers (Offset/used/Offset);
 *                  each used entry points to an adt_liquid_instance plus
 *                  parallel vertex-data + exists-bitmap blocks.  Vertex
 *                  format depends on LiquidVertexFormat (0=HeightDepth,
 *                  1=HeightTextureCoord, 2=Depth, 3=HeightDepthTC, ...).
 *
 *   MCLQ  (pre-Cata): per-MCNK sub-chunk after MCAL when the MCNK's flags
 *                     advertise water/ocean/magma bits.  9x9 height grid
 *                     with an 8x8 flag matrix; flag == 0x0F means "hole"
 *                     and the cell is skipped from the existsBitmap.
 *
 * v1 picks one layer per chunk -- highest maxHeight wins -- which covers
 * every Azeroth lake/ocean/lava we need to render.  No LiquidObject.db2
 * lookup is performed; for instances with LiquidVertexFormat >= 42
 * (modern hot-fixed liquids) we fall back to format 0 (HeightDepth)
 * which matches what 99% of the dataset uses.
 */

#include "AdtLiquidReader.h"

#include "CascClient.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace world_editor::io
{

namespace
{

constexpr float kTileSize  = 533.33333f;
constexpr float kChunkSize = kTileSize / 16.0f;       // 33.33333
constexpr float kUnitSize  = kChunkSize / 8.0f;       // 4.16666...

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) noexcept
{
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

constexpr uint32_t kFccMH2O = MakeFourCC('O', '2', 'H', 'M');
constexpr uint32_t kFccMCNK = MakeFourCC('K', 'N', 'C', 'M');
constexpr uint32_t kFccMCLQ = MakeFourCC('Q', 'L', 'C', 'M');

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
    uint8_t  pad0[8];
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
    float    ypos;
    uint32_t offsMCCV;
    uint32_t props;
    uint32_t effectId;
};
static_assert(sizeof(McnkHeader) == 8 + 128, "McnkHeader layout drift");

struct MH2OHeaderEntry
{
    uint32_t OffsetInstances;
    uint32_t LayerCount;
    uint32_t OffsetAttributes;
};
static_assert(sizeof(MH2OHeaderEntry) == 12, "MH2OHeaderEntry layout drift");

struct MH2OInstance
{
    uint16_t LiquidType;          // index into LiquidType.db2
    uint16_t LiquidVertexFormat;  // 0..5 = LiquidVertexFormatType; >=42 = LiquidObject lookup
    float    MinHeightLevel;
    float    MaxHeightLevel;
    uint8_t  OffsetX;
    uint8_t  OffsetY;
    uint8_t  Width;
    uint8_t  Height;
    uint32_t OffsetExistsBitmap;
    uint32_t OffsetVertexData;
};
static_assert(sizeof(MH2OInstance) == 24, "MH2OInstance layout drift");
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

// Find a sub-chunk inside an MCNK body.  Matches AdtReader::SearchMcnkSubChunk:
// byte-scan for the exact magic (NOT a size-trusting walk) so the ROOT MCNK's
// 128-byte header is skipped transparently and a garbage size from header
// bytes can't derail us.  See AdtReader.cpp for the full rationale.
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
            continue;
        outBody = mcnkBody + pos + 8;
        outBodySize = size;
        return true;
    }
    return false;
}

// Map LiquidType.db2 row -> renderer kind.  Hardcoded common rows; the
// fallback is Water which is safe for un-mapped river/lake/pond types.
LiquidChunk::Kind KindForLiquidType(uint16_t liquidType)
{
    switch (liquidType)
    {
        // Ocean (deep + coastal saltwater variants).
        case 2: case 14:
            return LiquidChunk::Kind::Ocean;
        // Magma / lava.
        case 3: case 7: case 11: case 19:
            return LiquidChunk::Kind::Magma;
        // Slime / acid.
        case 4: case 21: case 121: case 122:
            return LiquidChunk::Kind::Slime;
        // Generic fresh water (rivers, lakes, ponds).
        case 1: case 5: case 41: case 61: case 81:
            return LiquidChunk::Kind::Water;
        default:
            break;
    }
    // Zone-themed water buckets: 100..199 = various rivers; 200..255 =
    // misc water variants.  Anything we don't recognise => Water.
    if (liquidType >= 100 && liquidType <= 199)
        return LiquidChunk::Kind::Water;
    return LiquidChunk::Kind::Water;
}

// Mirror of map_extractor's adt_MH2O::GetLiquidVertexFormat() pragmatic
// branch: format 2 (Depth) for ocean type 2; everything else falls back
// to format 0 (HeightDepth) when LiquidVertexFormat >= 42 (we don't load
// the LiquidObject/LiquidMaterial DB2s here).
int ResolveVertexFormat(MH2OInstance const& inst)
{
    if (inst.LiquidVertexFormat < 42)
        return int(inst.LiquidVertexFormat);
    if (inst.LiquidType == 2)
        return 2; // Depth
    return 0;     // HeightDepth
}

float ReadHeightAt(uint8_t const* mh2oBase, std::size_t mh2oSize,
                   MH2OInstance const& inst, int format,
                   int width1, int height1, int pos, float fallback)
{
    if (!inst.OffsetVertexData)
        return fallback;
    std::size_t const baseOfs = std::size_t(inst.OffsetVertexData);
    int const vertCount = width1 * height1;
    auto safeRead = [&](std::size_t byteOfs, std::size_t stride) -> float
    {
        std::size_t const total = baseOfs + byteOfs + std::size_t(pos) * stride
                                + sizeof(float);
        if (total > mh2oSize)
            return fallback;
        float v = 0.0f;
        std::memcpy(&v, mh2oBase + baseOfs + byteOfs + std::size_t(pos) * stride,
                    sizeof(float));
        return v;
    };
    switch (format)
    {
        case 0: // HeightDepth: float heights then int8 depths.
        case 1: // HeightTextureCoord: float heights then uint16x2 uvs.
        case 3: // HeightDepthTextureCoord
            return safeRead(0, sizeof(float));
        case 2: // Depth: no heights stored.
            return fallback;
        case 4: // Unk4: 8 bytes per vertex, height at +4.
        case 5: // Unk5
            return safeRead(4, 8);
        default:
            return fallback;
    }
    (void)vertCount;
}

uint64_t ReadExistsBitmap(uint8_t const* mh2oBase, std::size_t mh2oSize,
                          MH2OInstance const& inst, int width, int height)
{
    if (!inst.OffsetExistsBitmap)
        return 0xFFFFFFFFFFFFFFFFuLL; // default: all 64 quads present.
    // Bitmap length = ceil(width*height / 8) bytes.
    std::size_t const bits = std::size_t(width) * std::size_t(height);
    std::size_t const bytes = (bits + 7) / 8;
    if (inst.OffsetExistsBitmap + bytes > mh2oSize)
        return 0;
    uint64_t mask = 0;
    int bit = 0;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t const b = mh2oBase[inst.OffsetExistsBitmap + std::size_t(bit / 8)];
            if (b & (1u << (bit & 7)))
                mask |= (uint64_t(1) << (y * 8 + x));
            ++bit;
        }
    }
    return mask;
}

// Synthesize a 9x9 vertex grid for a chunk's liquid surface.  cmx,cmy =
// chunk's most-northwest sample world XY (matches AdtReader convention).
void FillVertices(LiquidChunk& out,
                  float chunkMaxX, float chunkMaxY,
                  uint8_t offsetX, uint8_t offsetY,
                  uint8_t width, uint8_t height,
                  float defaultHeight,
                  uint8_t const* mh2oBase, std::size_t mh2oSize,
                  MH2OInstance const& inst, int format)
{
    int const width1  = int(width)  + 1;
    int const height1 = int(height) + 1;
    for (int y = 0; y <= 8; ++y)
    {
        for (int x = 0; x <= 8; ++x)
        {
            int const idx = y * 9 + x;
            LiquidVertex& v = out.vertices[idx];
            v.x = chunkMaxX - float(y) * kUnitSize;
            v.y = chunkMaxY - float(x) * kUnitSize;
            // Resolve height: when (x, y) sits inside the [OffsetX,
            // OffsetX+Width] x [OffsetY, OffsetY+Height] window we look
            // up the vertex-data array; otherwise use the chunk's
            // default min height as a safe baseline.
            int const lx = x - int(offsetX);
            int const ly = y - int(offsetY);
            if (lx >= 0 && lx < width1 && ly >= 0 && ly < height1)
            {
                int const pos = ly * width1 + lx;
                v.z = ReadHeightAt(mh2oBase, mh2oSize, inst, format,
                                   width1, height1, pos, defaultHeight);
            }
            else
            {
                v.z = defaultHeight;
            }
        }
    }
}

void ParseMH2O(uint8_t const* mh2oBase, std::size_t mh2oSize, int gx, int gy,
               AdtLiquid& out)
{
    if (mh2oSize < sizeof(MH2OHeaderEntry) * 256)
        return;
    auto const* headers = reinterpret_cast<MH2OHeaderEntry const*>(mh2oBase);
    // 256 entries are laid out [x=0..15][y=0..15] per the map_extractor
    // header (`adt_LIQUID liquid[16][16];`).  We keep the same indexing.
    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            MH2OHeaderEntry const& he = headers[x * 16 + y];
            if (!he.LayerCount || !he.OffsetInstances)
                continue;
            if (he.OffsetInstances + sizeof(MH2OInstance) > mh2oSize)
                continue;

            // Pick the highest-Z layer.  Most chunks carry a single
            // layer; stacked layers are rare and the visual difference
            // between picking max vs min is "lake-over-river" cases
            // where the lake reads as the dominant surface anyway.
            int bestIdx = 0;
            float bestZ = -1.0e30f;
            for (uint32_t li = 0; li < he.LayerCount; ++li)
            {
                std::size_t const ofs = std::size_t(he.OffsetInstances)
                                      + li * sizeof(MH2OInstance);
                if (ofs + sizeof(MH2OInstance) > mh2oSize) break;
                MH2OInstance inst{};
                std::memcpy(&inst, mh2oBase + ofs, sizeof(MH2OInstance));
                if (inst.MaxHeightLevel > bestZ)
                {
                    bestZ = inst.MaxHeightLevel;
                    bestIdx = int(li);
                }
            }
            MH2OInstance inst{};
            std::memcpy(&inst,
                        mh2oBase + he.OffsetInstances + bestIdx * sizeof(MH2OInstance),
                        sizeof(MH2OInstance));

            int const format = ResolveVertexFormat(inst);

            // For instances that store no per-vertex heights (Depth /
            // OffsetVertexData == 0) the surface is flat at
            // MinHeightLevel == MaxHeightLevel; treat it as a uniform
            // height across the full 8x8 footprint.
            uint8_t const offX = (inst.LiquidVertexFormat < 42) ? inst.OffsetX : 0;
            uint8_t const offY = (inst.LiquidVertexFormat < 42) ? inst.OffsetY : 0;
            uint8_t const wid  = (inst.LiquidVertexFormat < 42) ? inst.Width   : 8;
            uint8_t const hei  = (inst.LiquidVertexFormat < 42) ? inst.Height  : 8;

            LiquidChunk lc;
            lc.mcnkX = x;
            lc.mcnkY = y;
            lc.minHeight = inst.MinHeightLevel;
            lc.maxHeight = inst.MaxHeightLevel;
            lc.kind = KindForLiquidType(inst.LiquidType);

            // World-XY footprint of MCNK (x, y).  Mirrors AdtReader.
            float const tileMaxX = (32 - gx) * kTileSize;
            float const tileMaxY = (32 - gy) * kTileSize;
            float const chunkMaxX = tileMaxX - float(x) * kChunkSize;
            float const chunkMaxY = tileMaxY - float(y) * kChunkSize;

            float const defaultZ = inst.MinHeightLevel;
            FillVertices(lc, chunkMaxX, chunkMaxY, offX, offY, wid, hei,
                         defaultZ, mh2oBase, mh2oSize, inst, format);

            // Build the 8x8 exists bitmap from the instance window +
            // OffsetExistsBitmap.  Out-of-window quads are inactive.
            uint64_t const localExists = ReadExistsBitmap(mh2oBase, mh2oSize,
                                                          inst, wid, hei);
            uint64_t mask = 0;
            for (int qy = 0; qy < hei; ++qy)
            {
                for (int qx = 0; qx < wid; ++qx)
                {
                    int const localBit = qy * wid + qx;
                    if (!(localExists & (uint64_t(1) << localBit)))
                        continue;
                    int const gqx = qx + offX;
                    int const gqy = qy + offY;
                    if (gqx < 0 || gqx >= 8 || gqy < 0 || gqy >= 8) continue;
                    mask |= (uint64_t(1) << (gqy * 8 + gqx));
                }
            }
            lc.existsBitmap = mask;
            if (mask != 0)
                out.chunks.push_back(lc);
        }
    }
}

void ParseMCLQFromMCNK(uint8_t const* rootData, std::size_t rootSize,
                      int gx, int gy, AdtLiquid& out)
{
    // Walk MCNKs; when MCNK.flags advertises any of the water/ocean/
    // magma bits AND sizeMCLQ > 8 we parse the in-MCNK MCLQ sub-chunk.
    std::size_t mcnkSeq = 0;
    ForEachTopLevelChunk(rootData, rootSize,
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize) -> bool
        {
            if (magic != kFccMCNK) return true;
            if (bodySize < sizeof(McnkHeader) - 8) return true;

            auto const* hdr = reinterpret_cast<McnkHeader const*>(body - 8);
            uint32_t const f = hdr->flags;
            // bits: 0x04 = water, 0x08 = ocean, 0x10 = magma/slime.
            bool const haveLiquidBit = (f & 0x04u) || (f & 0x08u) || (f & 0x10u);
            (void)mcnkSeq;

            uint8_t const* mclqBody = nullptr;
            std::size_t    mclqSize = 0;
            if (!haveLiquidBit || hdr->sizeMCLQ <= 8 ||
                !SearchMcnkSubChunk(body, bodySize, kFccMCLQ, mclqBody, mclqSize))
            {
                ++mcnkSeq;
                return true;
            }

            // MCLQ layout: 4 bytes height1 + 4 bytes height2 + 9x9
            // (uint32 light + float height) + 8x8 flag bytes.
            constexpr std::size_t kHeaderBytes = 8;
            constexpr std::size_t kVertexBytes = std::size_t(9) * 9 * 8;
            constexpr std::size_t kFlagBytes   = std::size_t(8) * 8;
            if (mclqSize < kHeaderBytes + kVertexBytes + kFlagBytes)
            {
                ++mcnkSeq;
                return true;
            }

            LiquidChunk lc;
            lc.mcnkX = int(hdr->ix);
            lc.mcnkY = int(hdr->iy);

            if (f & 0x10u)      lc.kind = LiquidChunk::Kind::Magma; // magma/slime bit
            else if (f & 0x08u) lc.kind = LiquidChunk::Kind::Ocean;
            else                lc.kind = LiquidChunk::Kind::Water;

            // iy -> worldX, ix -> worldY (see AdtReader.cpp for the full
            // rationale -- must match map_extractor's V9[iy*8+y][ix*8+x]
            // layout so liquid lines up with the terrain).
            float const tileMaxX = (32 - gx) * kTileSize;
            float const tileMaxY = (32 - gy) * kTileSize;
            float const chunkMaxX = tileMaxX - float(hdr->iy) * kChunkSize;
            float const chunkMaxY = tileMaxY - float(hdr->ix) * kChunkSize;

            float minZ =  1.0e30f;
            float maxZ = -1.0e30f;
            uint8_t const* verts = mclqBody + kHeaderBytes;
            for (int y = 0; y <= 8; ++y)
            {
                for (int x = 0; x <= 8; ++x)
                {
                    std::size_t const ofs = (std::size_t(y) * 9 + x) * 8;
                    float h = 0.0f;
                    std::memcpy(&h, verts + ofs + 4, sizeof(float));
                    LiquidVertex& v = lc.vertices[y * 9 + x];
                    v.x = chunkMaxX - float(y) * kUnitSize;
                    v.y = chunkMaxY - float(x) * kUnitSize;
                    v.z = h;
                    if (h < minZ) minZ = h;
                    if (h > maxZ) maxZ = h;
                }
            }
            lc.minHeight = minZ;
            lc.maxHeight = maxZ;

            uint8_t const* flags = mclqBody + kHeaderBytes + kVertexBytes;
            uint64_t mask = 0;
            for (int y = 0; y < 8; ++y)
            {
                for (int x = 0; x < 8; ++x)
                {
                    uint8_t const flag = flags[y * 8 + x];
                    if ((flag & 0x0Fu) != 0x0Fu)
                        mask |= (uint64_t(1) << (y * 8 + x));
                }
            }
            lc.existsBitmap = mask;
            if (mask != 0)
                out.chunks.push_back(lc);

            ++mcnkSeq;
            return true;
        });
}

} // namespace

bool loadAdtLiquid(CascClient& casc,
                   std::string const& mapDir,
                   uint32_t mapId, int gx, int gy,
                   AdtLiquid& out,
                   uint32_t rootFdid)
{
    out = {};
    out.mapId = mapId;
    out.gx = gx;
    out.gy = gy;

    if (mapDir.empty() || !casc.isOpen())
        return false;

    std::vector<uint8_t> rootBytes;
    if (rootFdid != 0)
    {
        if (!casc.readByFileDataId(rootFdid, rootBytes) || rootBytes.empty())
            return false;
    }
    else
    {
        // Virtual-path fallback for Classic-era WDTs without MAID; see
        // AdtReader.cpp rootPath() for the geographic-correctness note.
        std::string rootPath = "world/maps/";
        rootPath += mapDir;
        rootPath += '/';
        rootPath += mapDir;
        rootPath += '_';
        rootPath += std::to_string(gx);
        rootPath += '_';
        rootPath += std::to_string(gy);
        rootPath += ".adt";
        if (!casc.readByPath(rootPath, rootBytes) || rootBytes.empty())
            return false;
    }

    // Prefer MH2O when present (Cata+); fall back to per-MCNK MCLQ for
    // legacy ADTs.  Both shapes are mutually exclusive in practice but
    // we tolerate either by not gating one on the absence of the other.
    uint8_t const* mh2oBody = nullptr;
    std::size_t    mh2oBodySize = 0;
    if (FindTopLevelChunk(rootBytes.data(), rootBytes.size(),
                          kFccMH2O, mh2oBody, mh2oBodySize))
    {
        ParseMH2O(mh2oBody, mh2oBodySize, gx, gy, out);
    }

    if (out.chunks.empty())
        ParseMCLQFromMCNK(rootBytes.data(), rootBytes.size(), gx, gy, out);

    return true;
}

} // namespace world_editor::io
