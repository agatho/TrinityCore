/*
 * WMOReader - implementation.
 *
 * The modern WMO is split across two file kinds:
 *   - Root file: a flat chunked container.  We read MOHD (header counts +
 *     bbox), MOMT (the material array), MOTX (legacy null-terminated
 *     texture path blob), MDID (modern per-material texture FileDataIds)
 *     and GFID (the per-group FileDataIds that point at the group files).
 *   - Group files: one MOGP "wrapper" chunk whose size spans the whole
 *     group body, with the actual render sub-chunks (MOVT/MONR/MOTV/
 *     MOVI/MOVX/MOBA/MOCV) nested after a 68-byte MOGP header.
 *
 * Texture resolution is the three-way MDID > inline-texture1 > MOTX-offset
 * scheme (chunks research §4): modern WMOs (7.1+) either ship an MDID array
 * (case A) or store the texture FileDataID inline in MOMT.texture1 with no
 * MOTX present (case B); legacy WMOs keep MOTX and treat MOMT.texture1 as a
 * byte offset into that blob (case C).
 *
 * Every array read is bounds-checked against the in-memory file size, the
 * same RangeOk pattern M2Reader uses, so a truncated or hostile file can
 * never overrun.
 */

#include "WMOReader.h"

#include "CascClient.h"

#include <cstring>
#include <limits>
#include <utility>

namespace world_editor::io
{

namespace
{

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) noexcept
{
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

// FourCCs are stored on disk reversed (same convention as M2Reader /
// AdtReader): pass the human-readable tag chars in reverse so the dword's
// low byte equals the first on-disk byte.
//   Root chunks:
constexpr uint32_t kFccMOHD = MakeFourCC('D', 'H', 'O', 'M');
constexpr uint32_t kFccMOMT = MakeFourCC('T', 'M', 'O', 'M');
constexpr uint32_t kFccMOTX = MakeFourCC('X', 'T', 'O', 'M');
constexpr uint32_t kFccMDID = MakeFourCC('D', 'I', 'D', 'M');
constexpr uint32_t kFccGFID = MakeFourCC('D', 'I', 'F', 'G');
//   Group chunks:
constexpr uint32_t kFccMOGP = MakeFourCC('P', 'G', 'O', 'M');
constexpr uint32_t kFccMOVI = MakeFourCC('I', 'V', 'O', 'M');
constexpr uint32_t kFccMOVX = MakeFourCC('X', 'V', 'O', 'M');
constexpr uint32_t kFccMOVT = MakeFourCC('T', 'V', 'O', 'M');
constexpr uint32_t kFccMONR = MakeFourCC('R', 'N', 'O', 'M');
constexpr uint32_t kFccMOTV = MakeFourCC('V', 'T', 'O', 'M');
constexpr uint32_t kFccMOBA = MakeFourCC('A', 'B', 'O', 'M');
constexpr uint32_t kFccMOCV = MakeFourCC('V', 'C', 'O', 'M');

// MOGP group flags.
constexpr uint32_t kMogpFlagInterior   = 0x00002000;
constexpr uint32_t kMogpFlagUnreachable = 0x00000080; // skip the whole group.
constexpr uint32_t kMogpFlagAntiportal = 0x04000000;  // skip the whole group.

#pragma pack(push, 1)

struct WmoMohd                  // 64 bytes
{
    uint32_t nTextures;         // = nMaterials
    uint32_t nGroups;
    uint32_t nPortals;
    uint32_t nLights;
    uint32_t nDoodadNames;
    uint32_t nDoodadDefs;
    uint32_t nDoodadSets;
    uint32_t color;             // ambient (BGRA) — Step-2 lighting only.
    uint32_t RootWMOID;
    float    bbcorn1[3];
    float    bbcorn2[3];
    uint16_t flags;
    uint16_t numLod;
};
static_assert(sizeof(WmoMohd) == 64, "WmoMohd size drift");

struct SMOMaterial              // 64 bytes (chunks research §2 MOMT)
{
    uint32_t flags;             // 0x00
    uint32_t shader;            // 0x04
    uint32_t blendMode;         // 0x08
    uint32_t texture1;          // 0x0C  three-way meaning (see resolveMaterial)
    uint32_t emissiveColor;     // 0x10
    uint32_t sidnEmissiveColor; // 0x14
    uint32_t texture2;          // 0x18
    uint32_t diffColor;         // 0x1C
    uint32_t groundType;        // 0x20
    uint32_t texture3;          // 0x24
    uint32_t color2;            // 0x28
    uint32_t flags2;            // 0x2C
    uint32_t runtimeData[4];    // 0x30..0x3F
};
static_assert(sizeof(SMOMaterial) == 64, "SMOMaterial size drift");

struct WmoMogp                  // 68 bytes — group header (chunks research §3)
{
    int32_t  groupName;         // 0x00
    int32_t  descGroupName;     // 0x04
    uint32_t mogpFlags;         // 0x08
    float    bbcorn1[3];        // 0x0C
    float    bbcorn2[3];        // 0x18
    uint16_t moprIdx;           // 0x24
    uint16_t moprNItems;        // 0x26
    uint16_t nBatchA;           // 0x28  transparent batch count
    uint16_t nBatchB;           // 0x2A
    uint32_t nBatchC;           // 0x2C
    uint32_t fogIdx;            // 0x30
    uint32_t groupLiquid;       // 0x34
    uint32_t groupWMOID;        // 0x38
    uint32_t mogpFlags2;        // 0x3C
    int16_t  splitParentOrFirstChild; // 0x40
    int16_t  splitNextChild;          // 0x42
};
static_assert(sizeof(WmoMogp) == 68, "WmoMogp size drift");

struct SMOBatch                 // 24 bytes (chunks research §3 MOBA)
{
    int16_t  bx, by, bz;        // 0x00 bounding
    int16_t  tx, ty, tz;        // 0x06 bounding (tz doubles as material override)
    uint32_t startIndex;        // 0x0C  first index into MOVI
    uint16_t count;             // 0x10  index count
    uint16_t minIndex;          // 0x12
    uint16_t maxIndex;          // 0x14
    uint8_t  flags;             // 0x16
    uint8_t  materialId;        // 0x17
};
static_assert(sizeof(SMOBatch) == 24, "SMOBatch size drift");

#pragma pack(pop)

struct ChunkHeader
{
    uint32_t magic;
    uint32_t size;
};

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

// Convert WMO client (X right, Y forward, Z up) to TC frame (X north, Y
// west, Z up).  Identical to M2Reader::FixCoord / vmap_extractor
// fixCoordSystem so WMO verts share the doodad model-matrix frame.
inline void FixCoord(float& x, float& y, float& z) noexcept
{
    float const ox = x, oy = y, oz = z;
    // EDITOR (Z-up) frame.  vmap's (x,z,-y) is Y-up; composing the position
    // transform's linear map (a,b,c)->(-c,-a,b) onto it lands verts Z-up
    // (model up Z -> world up Z) so buildings STAND.  A uniform 90 CW yaw
    // correction is applied in the model matrix (see drawTexturedWmos).
    x =  oy;
    y = -ox;
    z =  oz;
}

// Bounds guard for an array of `count` * `stride` bytes living at `base`
// inside a `spaceSize`-byte buffer.  Mirrors M2Reader::RangeOk.
inline bool RangeOk(std::size_t base, std::size_t count, std::size_t stride, std::size_t spaceSize) noexcept
{
    if (count == 0) return true;
    if (stride == 0) return false;
    if (count > (std::numeric_limits<std::size_t>::max() / stride)) return false;
    std::size_t const need = count * stride;
    return base <= spaceSize && need <= (spaceSize - base);
}

// Read a null-terminated string at blob[off] (bounds-checked).
std::string cstringAt(std::vector<uint8_t> const& blob, std::size_t off)
{
    if (off >= blob.size())
        return {};
    std::size_t end = off;
    while (end < blob.size() && blob[end] != '\0')
        ++end;
    return std::string(reinterpret_cast<char const*>(blob.data()) + off, end - off);
}

// Per-material texture resolution (one of fdid / path).
struct MaterialResolved
{
    uint32_t    fdid      = 0;
    std::string path;
    uint8_t     blendMode = 0;
};

// Three-way texture resolution (chunks research §4):
//   A) MDID array present -> mdid[i] is the texture FileDataID.
//   B) no MOTX (modern 7.1+) -> MOMT.texture1 IS the FileDataID inline.
//   C) MOTX present (legacy) -> MOMT.texture1 is a byte offset into MOTX.
MaterialResolved resolveMaterial(SMOMaterial const& m, std::size_t i,
                                 std::vector<uint8_t> const& motxBlob,
                                 std::vector<uint32_t> const& mdid)
{
    MaterialResolved r;
    r.blendMode = static_cast<uint8_t>(m.blendMode);
    if (!mdid.empty())                                   // case A
        r.fdid = (i < mdid.size()) ? mdid[i] : 0u;
    else if (motxBlob.empty())                           // case B (modern 7.1+)
        r.fdid = m.texture1;                             // texture1 IS the FDID
    else                                                 // case C (legacy)
        r.path = cstringAt(motxBlob, m.texture1);        // texture1 is a byte offset
    return r;
}

// Decode one group file into render geometry.  Returns false on hard I/O
// failure, an explicitly skipped group (unreachable / antiportal), or when
// the group yields no drawable submeshes.
bool loadGroup(CascClient& casc, uint32_t groupFdid,
               std::vector<MaterialResolved> const& mats, WmoGroupMesh& out)
{
    out = {};
    if (groupFdid == 0)
        return false;

    std::vector<uint8_t> bytes;
    if (!casc.readByFileDataId(groupFdid, bytes) || bytes.size() < sizeof(ChunkHeader) + sizeof(WmoMogp))
        return false;

    // Raw arrays collected from the nested MOGP body.
    std::vector<float>    rawPos;   // 3/vertex.
    std::vector<float>    rawNrm;   // 3/vertex.
    std::vector<float>    rawUv;    // 2/vertex — UV set 0 only.
    std::vector<uint32_t> tri;      // index buffer (MOVI u16 or MOVX u32).
    std::vector<SMOBatch> batches;
    std::vector<uint8_t>  rawCol;   // 4/vertex (BGRA), optional.
    bool                  haveMogp = false;
    bool                  skipGroup = false;

    ForEachTopLevelChunk(bytes.data(), bytes.size(),
        [&](uint32_t magic, uint8_t const* body, std::size_t n) -> bool
        {
            if (magic != kFccMOGP || n < sizeof(WmoMogp))
                return true; // root-style siblings are not expected at top level; skip.

            haveMogp = true;
            WmoMogp mogp{};
            std::memcpy(&mogp, body, sizeof(WmoMogp));

            if (mogp.mogpFlags & (kMogpFlagUnreachable | kMogpFlagAntiportal))
            {
                skipGroup = true;
                return false;
            }

            out.interior   = (mogp.mogpFlags & kMogpFlagInterior) != 0;
            out.bboxMin[0] = mogp.bbcorn1[0]; out.bboxMin[1] = mogp.bbcorn1[1]; out.bboxMin[2] = mogp.bbcorn1[2];
            out.bboxMax[0] = mogp.bbcorn2[0]; out.bboxMax[1] = mogp.bbcorn2[1]; out.bboxMax[2] = mogp.bbcorn2[2];

            // The MOGP chunk size spans the whole group body, but the header
            // is only 68 bytes; the render sub-chunks are nested after it.
            // Re-walk body+68 .. body+n as siblings.
            uint8_t const* inner    = body + sizeof(WmoMogp);
            std::size_t    innerSize = n - sizeof(WmoMogp);

            ForEachTopLevelChunk(inner, innerSize,
                [&](uint32_t imagic, uint8_t const* ibody, std::size_t in) -> bool
                {
                    if (imagic == kFccMOVT)
                    {
                        std::size_t const c = in / (sizeof(float) * 3);
                        if (RangeOk(0, c, sizeof(float) * 3, in) && c)
                        {
                            rawPos.resize(c * 3);
                            std::memcpy(rawPos.data(), ibody, c * 3 * sizeof(float));
                        }
                    }
                    else if (imagic == kFccMONR)
                    {
                        std::size_t const c = in / (sizeof(float) * 3);
                        if (RangeOk(0, c, sizeof(float) * 3, in) && c)
                        {
                            rawNrm.resize(c * 3);
                            std::memcpy(rawNrm.data(), ibody, c * 3 * sizeof(float));
                        }
                    }
                    else if (imagic == kFccMOTV)
                    {
                        // Capture UV set 0 ONLY.  A group can carry up to 4
                        // MOTV sub-chunks; keep the first and ignore the rest
                        // so texture coordinates are not shifted to a later set.
                        if (rawUv.empty())
                        {
                            std::size_t const c = in / (sizeof(float) * 2);
                            if (RangeOk(0, c, sizeof(float) * 2, in) && c)
                            {
                                rawUv.resize(c * 2);
                                std::memcpy(rawUv.data(), ibody, c * 2 * sizeof(float));
                            }
                        }
                    }
                    else if (imagic == kFccMOVX)
                    {
                        // u32 indices (preferred over MOVI when present).
                        std::size_t const c = in / sizeof(uint32_t);
                        if (RangeOk(0, c, sizeof(uint32_t), in) && c)
                        {
                            tri.resize(c);
                            std::memcpy(tri.data(), ibody, c * sizeof(uint32_t));
                        }
                    }
                    else if (imagic == kFccMOVI)
                    {
                        // u16 indices — only when MOVX hasn't already filled tri.
                        if (tri.empty())
                        {
                            std::size_t const c = in / sizeof(uint16_t);
                            if (RangeOk(0, c, sizeof(uint16_t), in) && c)
                            {
                                auto const* src = reinterpret_cast<uint16_t const*>(ibody);
                                tri.resize(c);
                                for (std::size_t k = 0; k < c; ++k)
                                    tri[k] = src[k];
                            }
                        }
                    }
                    else if (imagic == kFccMOBA)
                    {
                        std::size_t const c = in / sizeof(SMOBatch);
                        if (RangeOk(0, c, sizeof(SMOBatch), in) && c)
                        {
                            batches.resize(c);
                            std::memcpy(batches.data(), ibody, c * sizeof(SMOBatch));
                        }
                    }
                    else if (imagic == kFccMOCV)
                    {
                        // 4 bytes/vertex BGRA — first set only.
                        if (rawCol.empty() && in)
                            rawCol.assign(ibody, ibody + (in / 4) * 4);
                    }
                    return true;
                });

            return false; // MOGP is the only top-level chunk we care about.
        });

    if (!haveMogp || skipGroup)
        return false;
    if (rawPos.empty() || tri.empty() || batches.empty())
        return false;

    std::size_t const nVerts = rawPos.size() / 3;

    // Build the interleaved { x,y,z, nx,ny,nz, u,v } vertex buffer, applying
    // FixCoord to position + normal, and recompute the local AABB from the
    // converted verts (FixCoord changes the frame so the MOGP bbox no longer
    // applies directly).
    out.vertices.clear();
    out.vertices.reserve(nVerts * 8);
    float minX =  std::numeric_limits<float>::infinity();
    float minY =  std::numeric_limits<float>::infinity();
    float minZ =  std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();

    bool const haveNrm = (rawNrm.size() / 3) >= nVerts;
    bool const haveUv  = (rawUv.size() / 2) >= nVerts;

    for (std::size_t v = 0; v < nVerts; ++v)
    {
        float px = rawPos[v * 3 + 0], py = rawPos[v * 3 + 1], pz = rawPos[v * 3 + 2];
        float nx = haveNrm ? rawNrm[v * 3 + 0] : 0.0f;
        float ny = haveNrm ? rawNrm[v * 3 + 1] : 0.0f;
        float nz = haveNrm ? rawNrm[v * 3 + 2] : 0.0f;
        FixCoord(px, py, pz);
        FixCoord(nx, ny, nz);
        out.vertices.push_back(px);
        out.vertices.push_back(py);
        out.vertices.push_back(pz);
        out.vertices.push_back(nx);
        out.vertices.push_back(ny);
        out.vertices.push_back(nz);
        out.vertices.push_back(haveUv ? rawUv[v * 2 + 0] : 0.0f);
        out.vertices.push_back(haveUv ? rawUv[v * 2 + 1] : 0.0f);
        if (px < minX) minX = px;  if (px > maxX) maxX = px;
        if (py < minY) minY = py;  if (py > maxY) maxY = py;
        if (pz < minZ) minZ = pz;  if (pz > maxZ) maxZ = pz;
    }

    if (nVerts == 0)
    {
        minX = minY = minZ = 0.0f;
        maxX = maxY = maxZ = 0.0f;
    }
    out.bboxMin[0] = minX; out.bboxMin[1] = minY; out.bboxMin[2] = minZ;
    out.bboxMax[0] = maxX; out.bboxMax[1] = maxY; out.bboxMax[2] = maxZ;

    // Optional MOCV -> RGBA (swizzle BGRA->RGBA, parity with wow.export
    // color1).  Only emitted when there is one colour per vertex.
    if (rawCol.size() / 4 >= nVerts)
    {
        out.colours.clear();
        out.colours.reserve(nVerts * 4);
        for (std::size_t v = 0; v < nVerts; ++v)
        {
            uint8_t const* src = rawCol.data() + v * 4;
            out.colours.push_back(src[2]); // R
            out.colours.push_back(src[1]); // G
            out.colours.push_back(src[0]); // B
            out.colours.push_back(src[3]); // A
        }
    }

    // MOVI/MOVX index directly into MOVT (no skin indirection like M2).
    out.indices = std::move(tri);

    // One submesh per MOBA batch.
    out.subMeshes.clear();
    out.subMeshes.reserve(batches.size());
    for (SMOBatch const& b : batches)
    {
        // flags&2 -> material override lives in tz (possibleBox2[2]).
        uint32_t const matId = (b.flags & 0x2) ? static_cast<uint32_t>(static_cast<uint8_t>(b.tz))
                                               : static_cast<uint32_t>(b.materialId);

        // Bounds-check the draw range against the index buffer.  size_t-clean
        // so the uint32 startIndex + uint16 count arithmetic cannot wrap.
        if (std::size_t(b.startIndex) + std::size_t(b.count) > out.indices.size())
            continue;
        if (b.count == 0)
            continue;

        WmoSubMesh sm;
        sm.materialId = matId;
        sm.indexStart = b.startIndex;
        sm.indexCount = b.count;
        sm.interior   = out.interior;
        if (matId < mats.size())
        {
            sm.textureFileDataId = mats[matId].fdid;
            sm.texturePath       = mats[matId].path;
            sm.blendMode         = mats[matId].blendMode;
        }
        out.subMeshes.push_back(std::move(sm));
    }

    return !out.subMeshes.empty() && !out.vertices.empty();
}

} // namespace

bool loadWmo(CascClient& casc, uint32_t wmoRootFileDataId, WmoModel& out)
{
    out = {};
    if (wmoRootFileDataId == 0 || !casc.isOpen())
        return false;

    std::vector<uint8_t> rootBytes;
    if (!casc.readByFileDataId(wmoRootFileDataId, rootBytes) || rootBytes.size() < sizeof(ChunkHeader))
        return false;

    WmoMohd mohd{};
    bool                     haveMohd = false;
    std::vector<SMOMaterial> rawMats;
    std::vector<uint8_t>     motxBlob;
    std::vector<uint32_t>    mdid;
    std::vector<uint32_t>    groupFdids;

    ForEachTopLevelChunk(rootBytes.data(), rootBytes.size(),
        [&](uint32_t magic, uint8_t const* body, std::size_t n) -> bool
        {
            if (magic == kFccMOHD && n >= sizeof(WmoMohd))
            {
                std::memcpy(&mohd, body, sizeof(WmoMohd));
                haveMohd = true;
            }
            else if (magic == kFccMOMT)
            {
                std::size_t const c = n / sizeof(SMOMaterial);
                if (RangeOk(0, c, sizeof(SMOMaterial), n) && c)
                {
                    rawMats.resize(c);
                    std::memcpy(rawMats.data(), body, c * sizeof(SMOMaterial));
                }
            }
            else if (magic == kFccMOTX)
            {
                motxBlob.assign(body, body + n);
            }
            else if (magic == kFccMDID)
            {
                std::size_t const c = n / sizeof(uint32_t);
                if (RangeOk(0, c, sizeof(uint32_t), n) && c)
                {
                    mdid.resize(c);
                    std::memcpy(mdid.data(), body, c * sizeof(uint32_t));
                }
            }
            else if (magic == kFccGFID)
            {
                std::size_t const c = n / sizeof(uint32_t);
                if (RangeOk(0, c, sizeof(uint32_t), n) && c)
                {
                    groupFdids.resize(c);
                    std::memcpy(groupFdids.data(), body, c * sizeof(uint32_t));
                }
            }
            return true;
        });

    if (!haveMohd)
        return false;

    // Resolve every material's texture via the three-way scheme.
    std::vector<MaterialResolved> mats;
    mats.reserve(rawMats.size());
    for (std::size_t i = 0; i < rawMats.size(); ++i)
        mats.push_back(resolveMaterial(rawMats[i], i, motxBlob, mdid));

    // GFID lists nGroups FDIDs (modern).  When MOHD.flags & 0x10 the file
    // ships lodCount*nGroups FDIDs; keep only the FIRST nGroups (most
    // detailed set), matching vmap4_extractor.
    if (groupFdids.size() > mohd.nGroups)
        groupFdids.resize(mohd.nGroups);

    // Legacy WMOs without GFID would need name-based <root>_NNN.wmo path
    // construction; the editor's CASC is FDID-first and modern maps always
    // carry GFID, so an empty list means no geometry.
    if (groupFdids.empty())
        return false;

    out.bboxMin[0] = mohd.bbcorn1[0]; out.bboxMin[1] = mohd.bbcorn1[1]; out.bboxMin[2] = mohd.bbcorn1[2];
    out.bboxMax[0] = mohd.bbcorn2[0]; out.bboxMax[1] = mohd.bbcorn2[1]; out.bboxMax[2] = mohd.bbcorn2[2];

    for (uint32_t gf : groupFdids)
    {
        if (gf == 0)
            continue;
        WmoGroupMesh gm;
        if (loadGroup(casc, gf, mats, gm) && !gm.subMeshes.empty())
            out.groups.push_back(std::move(gm));
    }

    return !out.groups.empty();
}

} // namespace world_editor::io
