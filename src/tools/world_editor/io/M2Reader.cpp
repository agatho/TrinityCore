/*
 * M2Reader - implementation.
 *
 * The modern M2 layout: every byte offset inside the legacy
 * ModelHeader (`MD20`) is relative to the start of the MD20 body --
 * which is also where the file starts in the pre-BfA shape, OR the
 * start of the MD21-chunk payload in the modern shape.  We capture
 * `m2Base` (pointer + size of the MD20 body) and treat every legacy
 * offset against that pointer.
 *
 * Skin / texture chunks (SFID / TXID) live at top level in the chunked
 * container alongside MD21.  Their decode is unambiguous: u32 arrays
 * indexed by submesh's M2Batch.textureComboIndex (TXID) and by LOD
 * (SFID -- we pick entry 0).
 */

#include "M2Reader.h"

#include "CascClient.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

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

// Top-level container chunks.  Stored as bytes-on-disk in big-endian
// FourCC layout (same convention as AdtReader).
constexpr uint32_t kFccMD21 = MakeFourCC('1', '2', 'D', 'M');
constexpr uint32_t kFccSFID = MakeFourCC('D', 'I', 'F', 'S');
constexpr uint32_t kFccTXID = MakeFourCC('D', 'I', 'X', 'T');

#pragma pack(push, 1)

struct M2VertexRaw
{
    float    pos[3];
    uint8_t  boneWeights[4];
    uint8_t  boneIndices[4];
    float    normal[3];
    float    uv0[2];
    float    uv1[2];
};
static_assert(sizeof(M2VertexRaw) == 48, "M2 vertex size drift");

struct M2Array
{
    uint32_t count;
    uint32_t offset;
};

struct M2TextureEntry
{
    uint32_t type;          // 0 = filename-driven, >0 = bind-by-context (skin / hair / etc.)
    uint32_t flags;
    uint32_t filenameLen;   // includes terminator.
    uint32_t filenameOfs;
};
static_assert(sizeof(M2TextureEntry) == 16, "M2Texture size drift");

// Subset of the legacy ModelHeader: only the fields we read.
// Layout matches src/tools/vmap4_extractor/modelheaders.h ModelHeader
// up to the bone arrays; we stop at the field we don't use to keep this
// struct nimble.
struct Md20Header
{
    char     magic[4];          // "MD20"
    uint8_t  version[4];

    uint32_t nameLength;
    uint32_t nameOfs;
    uint32_t type;

    uint32_t nGlobalSequences;  uint32_t ofsGlobalSequences;
    uint32_t nAnimations;       uint32_t ofsAnimations;
    uint32_t nAnimationLookup;  uint32_t ofsAnimationLookup;
    uint32_t nBones;            uint32_t ofsBones;
    uint32_t nKeyBoneLookup;    uint32_t ofsKeyBoneLookup;

    uint32_t nVertices;         uint32_t ofsVertices;
    uint32_t nViews;            // skin file count; bind-pose only needs LOD 0.

    uint32_t nColors;           uint32_t ofsColors;
    uint32_t nTextures;         uint32_t ofsTextures;
    uint32_t nTransparency;     uint32_t ofsTransparency;
    uint32_t nTextureAnimations; uint32_t ofsTextureAnimations;
    uint32_t nTexReplace;       uint32_t ofsTexReplace;
    uint32_t nRenderFlags;      uint32_t ofsRenderFlags;
    uint32_t nBoneLookupTable;  uint32_t ofsBoneLookupTable;
    uint32_t nTexLookup;        uint32_t ofsTexLookup;
    uint32_t nTexUnits;         uint32_t ofsTexUnits;
};

// M2Material (a.k.a. "render flag") -- per legacy ofsRenderFlags array.
struct M2Material
{
    uint16_t flags;
    uint16_t blendingMode;
};
static_assert(sizeof(M2Material) == 4, "M2Material size drift");

// Skin file layout (modern .skin / SFID-pointed).  Magic "SKIN".
struct M2SkinHeader
{
    char    magic[4];
    M2Array vertices;       // u16 lookup back into the M2's vertex array.
    M2Array indices;        // u16 indices that index into `vertices` (NOT the M2 vertex array directly).
    M2Array bones;          // u8 quads, ignored (bind-pose only).
    M2Array submeshes;      // M2SkinSection.
    M2Array batches;        // M2Batch.
    uint32_t boneCountMax;
};

struct M2SkinSection
{
    uint16_t skinSectionId;
    uint16_t level;
    uint16_t vertexStart;
    uint16_t vertexCount;
    uint16_t indexStart;
    uint16_t indexCount;
    uint16_t boneCount;
    uint16_t boneComboIndex;
    uint16_t boneInfluences;
    uint16_t centerBoneIndex;
    float    centerPos[3];
    float    sortCenterPos[3];
    float    sortRadius;
};
static_assert(sizeof(M2SkinSection) == 48, "M2SkinSection size drift");

struct M2Batch
{
    uint8_t  flags;
    int8_t   priorityPlane;
    uint16_t shaderId;
    uint16_t skinSectionIndex;
    uint16_t geosetIndex;
    uint16_t colorIndex;
    uint16_t materialIndex;
    uint16_t materialLayer;
    uint16_t textureCount;
    uint16_t textureComboIndex;
    uint16_t textureCoordComboIndex;
    uint16_t textureWeightComboIndex;
    uint16_t textureTransformComboIndex;
};
static_assert(sizeof(M2Batch) == 24, "M2Batch size drift");

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

// Convert M2 client (X right, Y forward, Z up) to TC frame (X north, Y
// west, Z up).  Same transform vmap_extractor uses (fixCoordSystem).
inline void FixCoord(float& x, float& y, float& z) noexcept
{
    float const ox = x, oy = y, oz = z;
    // (x, y, z) -> (x, z, -y) -- matches Doodad::Extract / model.cpp.
    x = ox;
    y = oz;
    z = -oy;
}

// Pull a pointer + size pair for a legacy-offset array within MD20 space.
inline bool RangeOk(std::size_t base, std::size_t count, std::size_t stride, std::size_t spaceSize) noexcept
{
    if (count == 0) return true;
    if (stride == 0) return false;
    if (count > (std::numeric_limits<std::size_t>::max() / stride)) return false;
    std::size_t const need = count * stride;
    return base <= spaceSize && need <= (spaceSize - base);
}

bool LoadSkin(CascClient& casc,
              std::vector<uint8_t>& skinBytesScratch,
              uint8_t const* m2Base, std::size_t m2BaseSize,
              uint32_t skinFdid,
              Md20Header const& hdr,
              std::vector<M2VertexRaw> const& m2Vertices,
              std::vector<uint16_t> const& texLookup,
              std::vector<uint32_t> const& txidFdids,
              std::vector<std::string> const& legacyTexPaths,
              std::vector<M2Material> const& materials,
              M2Mesh& out)
{
    if (skinFdid == 0)
        return false;
    skinBytesScratch.clear();
    if (!casc.readByFileDataId(skinFdid, skinBytesScratch) || skinBytesScratch.size() < sizeof(M2SkinHeader))
        return false;

    M2SkinHeader sh;
    std::memcpy(&sh, skinBytesScratch.data(), sizeof(M2SkinHeader));
    if (std::memcmp(sh.magic, "SKIN", 4) != 0)
        return false;

    std::size_t const skinSize = skinBytesScratch.size();
    if (!RangeOk(sh.vertices.offset, sh.vertices.count, sizeof(uint16_t), skinSize)) return false;
    if (!RangeOk(sh.indices.offset,  sh.indices.count,  sizeof(uint16_t), skinSize)) return false;
    if (!RangeOk(sh.submeshes.offset, sh.submeshes.count, sizeof(M2SkinSection), skinSize)) return false;
    if (!RangeOk(sh.batches.offset,  sh.batches.count,  sizeof(M2Batch), skinSize)) return false;

    auto const* vlookup = reinterpret_cast<uint16_t const*>(skinBytesScratch.data() + sh.vertices.offset);
    auto const* tris    = reinterpret_cast<uint16_t const*>(skinBytesScratch.data() + sh.indices.offset);
    auto const* subs    = reinterpret_cast<M2SkinSection const*>(skinBytesScratch.data() + sh.submeshes.offset);
    auto const* batches = reinterpret_cast<M2Batch const*>(skinBytesScratch.data() + sh.batches.offset);

    // Build vertex buffer keyed by M2 vertex index used by *any* submesh.
    // We emit the full M2 vertex array as-is; index buffer is rewritten
    // to point straight at vertex indices (no skin indirection).  Cost:
    // the renderer drops the duplicate-vertex compaction that skinning
    // engines would do, in exchange for a much simpler upload step.
    out.vertices.clear();
    out.vertices.reserve(m2Vertices.size() * 10);
    float minX =  std::numeric_limits<float>::infinity();
    float minY =  std::numeric_limits<float>::infinity();
    float minZ =  std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();
    for (M2VertexRaw const& v : m2Vertices)
    {
        float px = v.pos[0],    py = v.pos[1],    pz = v.pos[2];
        float nx = v.normal[0], ny = v.normal[1], nz = v.normal[2];
        FixCoord(px, py, pz);
        FixCoord(nx, ny, nz);
        out.vertices.push_back(px);
        out.vertices.push_back(py);
        out.vertices.push_back(pz);
        out.vertices.push_back(nx);
        out.vertices.push_back(ny);
        out.vertices.push_back(nz);
        out.vertices.push_back(v.uv0[0]);
        out.vertices.push_back(v.uv0[1]);
        // uv1 (the M2 T2 UV): consumed by the Diffuse_T1_T2 combiner's second
        // texture.  Always emitted so the GL vertex stride stays constant; the
        // single-texture path simply never samples it.
        out.vertices.push_back(v.uv1[0]);
        out.vertices.push_back(v.uv1[1]);
        if (px < minX) minX = px;  if (px > maxX) maxX = px;
        if (py < minY) minY = py;  if (py > maxY) maxY = py;
        if (pz < minZ) minZ = pz;  if (pz > maxZ) maxZ = pz;
    }
    if (m2Vertices.empty())
    {
        minX = minY = minZ = 0.0f;
        maxX = maxY = maxZ = 0.0f;
    }
    out.bboxMinX = minX; out.bboxMaxX = maxX;
    out.bboxMinY = minY; out.bboxMaxY = maxY;
    out.bboxMinZ = minZ; out.bboxMaxZ = maxZ;

    // Index buffer + submeshes.  One subMesh per batch (the batches are
    // the render lists; submeshes are the section primitives that the
    // batches reference).
    out.indices.clear();
    out.subMeshes.clear();
    out.subMeshes.reserve(sh.batches.count);

    // Map skin-section -> (firstTriBaseIndex, lastTriBaseIndex, etc) is
    // already in M2SkinSection -- we just translate skin-local vertex
    // indices through vlookup back into M2-vertex indices.
    for (uint32_t bi = 0; bi < sh.batches.count; ++bi)
    {
        M2Batch const& b = batches[bi];
        if (b.skinSectionIndex >= sh.submeshes.count) continue;
        M2SkinSection const& s = subs[b.skinSectionIndex];

        if (s.level != 0) continue;  // bind pose / LOD 0 only.
        if (s.indexCount == 0) continue;

        std::size_t const triStart = s.indexStart;
        std::size_t const triEnd   = std::size_t(s.indexStart) + s.indexCount;
        if (triEnd > sh.indices.count) continue;

        M2SubMesh sm;
        sm.materialIndex = b.materialIndex;
        sm.indexStart    = uint32_t(out.indices.size());

        for (std::size_t ti = triStart; ti < triEnd; ++ti)
        {
            uint16_t const skinLocal = tris[ti];
            if (skinLocal >= sh.vertices.count) continue;
            uint16_t const m2Idx = vlookup[skinLocal];
            if (m2Idx >= m2Vertices.size()) continue;
            out.indices.push_back(uint32_t(m2Idx));
        }
        sm.indexCount = uint32_t(out.indices.size()) - sm.indexStart;
        if (sm.indexCount == 0) continue;

        // Texture lookup: batch.textureComboIndex -> texLookup -> M2 tex.
        uint32_t textureFdid = 0;
        std::string texturePath;
        if (b.textureCount > 0 && b.textureComboIndex < texLookup.size())
        {
            uint16_t const m2TexIdx = texLookup[b.textureComboIndex];
            if (m2TexIdx < txidFdids.size() && txidFdids[m2TexIdx] != 0)
                textureFdid = txidFdids[m2TexIdx];
            else if (m2TexIdx < legacyTexPaths.size() && !legacyTexPaths[m2TexIdx].empty())
                texturePath = legacyTexPaths[m2TexIdx];
        }
        sm.textureFileDataId = textureFdid;
        sm.texturePath       = std::move(texturePath);

        // STAGE A: resolve a second texture (tex1) when the batch references
        // more than one.  Mirrors the tex0 resolve at combo slot +1, gated on
        // textureCount > 1 and a bounds guard on texLookup (same RangeOk-style
        // pattern as everywhere else in this reader).
        uint32_t textureFdid2 = 0;
        std::string texturePath2;
        if (b.textureCount > 1
            && std::size_t(b.textureComboIndex) + 1 < texLookup.size())
        {
            uint16_t const m2TexIdx2 = texLookup[b.textureComboIndex + 1];
            if (m2TexIdx2 < txidFdids.size() && txidFdids[m2TexIdx2] != 0)
                textureFdid2 = txidFdids[m2TexIdx2];
            else if (m2TexIdx2 < legacyTexPaths.size() && !legacyTexPaths[m2TexIdx2].empty())
                texturePath2 = legacyTexPaths[m2TexIdx2];
        }
        sm.textureFileDataId2 = textureFdid2;
        sm.texturePath2       = std::move(texturePath2);

        // Blend mode from M2Material indexed by batch.materialIndex.
        uint8_t blendMode = 0;
        if (b.materialIndex < materials.size())
            blendMode = uint8_t(materials[b.materialIndex].blendingMode);
        sm.blendMode = blendMode;

        // STAGE A: pick the combiner.  This is a deliberate heuristic, NOT a
        // full ShaderMapper port (per m2_research_wowexport.md §2/§4): we
        // collapse to our 4-case enum.  The conservative guarantee holds --
        // when no second texture resolved we never emit a 2-tex combiner, so a
        // single-texture doodad decodes bit-identically to before.
        bool const hasTex2 = (textureFdid2 != 0) || !sm.texturePath2.empty();
        uint8_t combiner;
        if (!hasTex2)
            combiner = (blendMode == 1) ? uint8_t(M2Combiner::Mod)
                                        : uint8_t(M2Combiner::Opaque);
        else
        {
            // Two textures present.  Distinguish modulate (T1_T2) vs mod2x
            // (T1_Env) from the low shaderId bits per reference §2 (legacy
            // PS-2-tex decode: (shaderId & 0x70) selects the mod2x family).
            // High-bit shaderIDs that index SHADER_ARRAY[0] are the mod2x-NA
            // default, so treat those as Env too.  Coarse but documented.
            bool const mod2x = (b.shaderId & 0x8000) != 0 || (b.shaderId & 0x0070) != 0;
            combiner = mod2x ? uint8_t(M2Combiner::Diffuse_T1_Env)
                             : uint8_t(M2Combiner::Diffuse_T1_T2);
        }
        sm.combinerId = combiner;

        out.subMeshes.push_back(std::move(sm));
    }
    (void)m2Base;
    (void)m2BaseSize;
    (void)hdr;
    return !out.subMeshes.empty();
}

} // namespace

bool loadM2(CascClient& casc, uint32_t modelFileDataId, M2Mesh& out)
{
    out = {};
    if (modelFileDataId == 0 || !casc.isOpen())
        return false;

    std::vector<uint8_t> bytes;
    if (!casc.readByFileDataId(modelFileDataId, bytes) || bytes.size() < 4)
        return false;

    // Locate the MD20-headed payload.  Two shapes:
    //   - Legacy: bytes[0..3] == "MD20", payload starts at bytes[0].
    //   - Modern: top-level chunked container; MD21 chunk body starts
    //     with "MD20".
    uint8_t const* m2Base = nullptr;
    std::size_t    m2BaseSize = 0;
    std::vector<uint32_t> sfidFdids;
    std::vector<uint32_t> txidFdids;

    if (std::memcmp(bytes.data(), "MD20", 4) == 0)
    {
        m2Base = bytes.data();
        m2BaseSize = bytes.size();
        // Legacy MD20 files don't carry SFID/TXID; skin0/textures resolve
        // through filename siblings + M2Texture filenameOfs (legacy).
    }
    else
    {
        ForEachTopLevelChunk(bytes.data(), bytes.size(),
            [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
            {
                if (magic == kFccMD21 && m2Base == nullptr)
                {
                    m2Base = body;
                    m2BaseSize = bodySize;
                }
                else if (magic == kFccSFID)
                {
                    std::size_t const n = bodySize / sizeof(uint32_t);
                    sfidFdids.resize(n);
                    if (n > 0) std::memcpy(sfidFdids.data(), body, n * sizeof(uint32_t));
                }
                else if (magic == kFccTXID)
                {
                    std::size_t const n = bodySize / sizeof(uint32_t);
                    txidFdids.resize(n);
                    if (n > 0) std::memcpy(txidFdids.data(), body, n * sizeof(uint32_t));
                }
                return true;
            });
    }
    if (m2Base == nullptr || m2BaseSize < sizeof(Md20Header))
        return false;
    if (std::memcmp(m2Base, "MD20", 4) != 0)
        return false;

    Md20Header hdr;
    std::memcpy(&hdr, m2Base, sizeof(Md20Header));

    if (!RangeOk(hdr.ofsVertices, hdr.nVertices, sizeof(M2VertexRaw), m2BaseSize)) return false;
    std::vector<M2VertexRaw> verts(hdr.nVertices);
    if (hdr.nVertices > 0)
        std::memcpy(verts.data(), m2Base + hdr.ofsVertices, hdr.nVertices * sizeof(M2VertexRaw));

    // texLookup: uint16[] mapping batch.textureComboIndex -> m2 texture array index.
    std::vector<uint16_t> texLookup;
    if (RangeOk(hdr.ofsTexLookup, hdr.nTexLookup, sizeof(uint16_t), m2BaseSize) && hdr.nTexLookup > 0)
    {
        texLookup.resize(hdr.nTexLookup);
        std::memcpy(texLookup.data(), m2Base + hdr.ofsTexLookup, hdr.nTexLookup * sizeof(uint16_t));
    }

    // Legacy texture filenames -- only present when TXID is absent.  Each
    // M2Texture either carries a filename (type == 0) or is bind-by-
    // context (type > 0; not handled in v1 -- those textures render as
    // missing and the fragment shader falls through to a neutral grey).
    std::vector<std::string> legacyTexPaths;
    if (RangeOk(hdr.ofsTextures, hdr.nTextures, sizeof(M2TextureEntry), m2BaseSize) && hdr.nTextures > 0)
    {
        legacyTexPaths.resize(hdr.nTextures);
        for (uint32_t i = 0; i < hdr.nTextures; ++i)
        {
            M2TextureEntry te;
            std::memcpy(&te, m2Base + hdr.ofsTextures + i * sizeof(M2TextureEntry), sizeof(M2TextureEntry));
            if (te.type == 0 && te.filenameLen > 1
                && te.filenameOfs < m2BaseSize
                && te.filenameOfs + te.filenameLen <= m2BaseSize)
            {
                char const* p = reinterpret_cast<char const*>(m2Base + te.filenameOfs);
                std::size_t slen = 0;
                while (slen < te.filenameLen && p[slen] != '\0') ++slen;
                legacyTexPaths[i].assign(p, slen);
            }
        }
    }

    // Materials (render flags) for the blend mode.
    std::vector<M2Material> materials;
    if (RangeOk(hdr.ofsRenderFlags, hdr.nRenderFlags, sizeof(M2Material), m2BaseSize) && hdr.nRenderFlags > 0)
    {
        materials.resize(hdr.nRenderFlags);
        std::memcpy(materials.data(), m2Base + hdr.ofsRenderFlags,
                    hdr.nRenderFlags * sizeof(M2Material));
    }

    // Pick the LOD-0 skin file.  Modern: SFID[0].  Legacy: skin0 is not
    // FDID-addressable from inside the M2; we don't support it for the
    // editor v1 (legacy maps are tiny + the proportion of MD20-only
    // doodads in any modern build is single-digit).
    uint32_t skinFdid = sfidFdids.empty() ? 0u : sfidFdids[0];
    if (skinFdid == 0)
        return false;

    std::vector<uint8_t> skinScratch;
    if (!LoadSkin(casc, skinScratch, m2Base, m2BaseSize, skinFdid, hdr,
                  verts, texLookup, txidFdids, legacyTexPaths, materials, out))
        return false;

    return !out.subMeshes.empty();
}

} // namespace world_editor::io
