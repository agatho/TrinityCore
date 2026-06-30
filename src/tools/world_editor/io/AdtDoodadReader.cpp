/*
 * AdtDoodadReader - implementation.
 *
 * MDDF layout (32 bytes per record) matches src/tools/vmap4_extractor/
 * adtfile.h ADT::MDDF; reproduced here to keep the editor self-contained.
 * MMID (legacy index-into-MMDX) is a uint32 array; MMDX is a packed
 * cstring blob.  MDDF.Flags bit 0x40 means MDDF.Id is itself a FileDataId
 * (modern path -- no MMID/MMDX needed); bit 0x40 has been the canonical
 * "modern-FDID" doodad bit since BfA's 8.0 ADT rewrite.
 */

#include "AdtDoodadReader.h"

#include "CascClient.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace world_editor::io
{

namespace
{

constexpr float kTileSize  = 533.33333f;
constexpr float kPi        = 3.14159265358979323846f;
constexpr float kDegToRad  = kPi / 180.0f;
constexpr float kClientMid = 17066.66656f;  // (32 tiles * 533.33333) -- client origin.

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) noexcept
{
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

constexpr uint32_t kFccMMDX = MakeFourCC('X', 'D', 'M', 'M');
constexpr uint32_t kFccMMID = MakeFourCC('D', 'I', 'M', 'M');
constexpr uint32_t kFccMDDF = MakeFourCC('F', 'D', 'D', 'M');
constexpr uint32_t kFccMWMO = MakeFourCC('O', 'M', 'W', 'M');
constexpr uint32_t kFccMWID = MakeFourCC('D', 'I', 'W', 'M');
constexpr uint32_t kFccMODF = MakeFourCC('F', 'D', 'O', 'M');

#pragma pack(push, 1)
struct MddfEntry
{
    uint32_t nameId;        // index into MMID (legacy) OR an M2 FileDataId when Flags & 0x40.
    uint32_t uniqueId;
    float    posX;          // client frame: X.
    float    posY;          // client frame: Y (height).
    float    posZ;          // client frame: Z.
    float    rotX;          // degrees.
    float    rotY;          // degrees.
    float    rotZ;          // degrees.
    uint16_t scale;         // /1024 -> float scale.
    uint16_t flags;         // bit 0x40 => nameId is a FileDataId.
};
static_assert(sizeof(MddfEntry) == 36, "MddfEntry layout drift");

// MODF - mirrors src/tools/vmap4_extractor/adtfile.h ADT::MODF.  64 bytes.
struct ModfEntry
{
    uint32_t nameId;            // index into MWID/MWMO OR a WMO root FDID when Flags & 0x8.
    uint32_t uniqueId;
    float    posX, posY, posZ;
    float    rotX, rotY, rotZ;  // degrees.
    float    boundsMinX, boundsMinY, boundsMinZ;
    float    boundsMaxX, boundsMaxY, boundsMaxZ;
    uint16_t flags;             // bit 0x8 = FDID, bit 0x4 = scale field is valid.
    uint16_t doodadSet;
    uint16_t nameSet;
    uint16_t scale;             // /1024 -> float (only when flags & 0x4).
};
static_assert(sizeof(ModfEntry) == 64, "ModfEntry layout drift");
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

std::vector<std::string> ParseMmdx(uint8_t const* body, std::size_t size)
{
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < size)
    {
        std::size_t end = pos;
        while (end < size && body[end] != '\0')
            ++end;
        if (end > pos)
            out.emplace_back(reinterpret_cast<char const*>(body + pos), end - pos);
        else
            out.emplace_back();
        pos = end + 1;
    }
    return out;
}

// Build a vector of strings indexed via the MMID offset table.  Each MMID
// entry is the byte offset into the raw MMDX blob where that entry's
// cstring begins.
std::vector<std::string> ResolveMmidStrings(uint8_t const* mmdxBody, std::size_t mmdxSize,
                                            uint8_t const* mmidBody, std::size_t mmidSize)
{
    std::vector<std::string> out;
    std::size_t const count = mmidSize / sizeof(uint32_t);
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        uint32_t off;
        std::memcpy(&off, mmidBody + i * sizeof(uint32_t), sizeof(uint32_t));
        if (off >= mmdxSize)
        {
            out.emplace_back();
            continue;
        }
        std::size_t end = off;
        while (end < mmdxSize && mmdxBody[end] != '\0')
            ++end;
        out.emplace_back(reinterpret_cast<char const*>(mmdxBody + off), end - off);
    }
    return out;
}

} // namespace

bool loadAdtDoodads(CascClient& casc,
                    std::string const& mapDir,
                    uint32_t /*mapId*/,
                    int gx, int gy,
                    std::vector<DoodadInstance>& out,
                    uint32_t obj0Fdid,
                    uint32_t rootFdid)
{
    out.clear();
    if (mapDir.empty() || !casc.isOpen())
        return false;

    auto buildPath = [&](char const* suffix) {
        // Virtual-path fallback for Classic-era WDTs without MAID; see
        // AdtReader.cpp rootPath() for the geographic-correctness note.
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

    // Try the modern split-file obj0 sibling first; the doodad placements
    // live there for any BfA+ ADT.  Legacy maps shove them into the root
    // .adt instead.  Caller-supplied FDIDs (from WDT MAID) take priority.
    std::vector<uint8_t> bytes;
    bool ok = false;
    if (obj0Fdid != 0)
        ok = casc.readByFileDataId(obj0Fdid, bytes) && !bytes.empty();
    if (!ok && rootFdid != 0)
        ok = casc.readByFileDataId(rootFdid, bytes) && !bytes.empty();
    if (!ok)
    {
        if (!casc.readByPath(buildPath("_obj0.adt"), bytes) || bytes.empty())
        {
            if (!casc.readByPath(buildPath(".adt"), bytes) || bytes.empty())
                return false;
        }
    }

    // Phase 1: collect MMDX + MMID for legacy-indexed entries.
    uint8_t const* mmdxBody = nullptr; std::size_t mmdxSize = 0;
    uint8_t const* mmidBody = nullptr; std::size_t mmidSize = 0;
    uint8_t const* mddfBody = nullptr; std::size_t mddfSize = 0;
    ForEachTopLevelChunk(bytes.data(), bytes.size(),
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
        {
            if (magic == kFccMMDX && mmdxBody == nullptr) { mmdxBody = body; mmdxSize = bodySize; }
            if (magic == kFccMMID && mmidBody == nullptr) { mmidBody = body; mmidSize = bodySize; }
            if (magic == kFccMDDF && mddfBody == nullptr) { mddfBody = body; mddfSize = bodySize; }
            return true;
        });

    if (mddfBody == nullptr || mddfSize == 0)
        return true;     // ADT has no doodads -- success with empty list.

    std::vector<std::string> indexedPaths;
    if (mmdxBody != nullptr && mmidBody != nullptr && mmidSize >= sizeof(uint32_t))
        indexedPaths = ResolveMmidStrings(mmdxBody, mmdxSize, mmidBody, mmidSize);
    else if (mmdxBody != nullptr && mmdxSize > 0 && (mmidBody == nullptr || mmidSize == 0))
        indexedPaths = ParseMmdx(mmdxBody, mmdxSize);  // legacy: MMDX without MMID, sequential index.

    std::size_t const count = mddfSize / sizeof(MddfEntry);
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        MddfEntry e;
        std::memcpy(&e, mddfBody + i * sizeof(MddfEntry), sizeof(MddfEntry));

        DoodadInstance d;
        d.uniqueId = e.uniqueId;

        // 0x40 bit: nameId is a raw FileDataId (modern ADTs).
        if (e.flags & 0x40u)
        {
            d.modelFileDataId = e.nameId;
        }
        else
        {
            if (e.nameId < indexedPaths.size())
                d.modelPath = indexedPaths[e.nameId];
            // FDID stays 0; caller resolves via path -> CASC lookup if it can.
        }

        // Client -> TC frame: (clientX, clientY, clientZ) at origin 17066.66
        // converts to TC (X, Y, Z) = (mid - clientZ, mid - clientX, clientY).
        d.x = kClientMid - e.posZ;
        d.y = kClientMid - e.posX;
        d.z = e.posY;

        // Rotations: client stores degrees as (rotX, rotY, rotZ); the
        // vmap_extractor's Doodad::ExtractSet pipes them through G3D's
        // fromEulerAnglesZYX(rotY, rotX, rotZ) when composing world
        // matrices.  We mirror that here: store ZYX (radians) so the
        // renderer can apply Rz * Ry * Rx as a row of three matrix
        // builds.  rotY (client yaw around vertical axis) maps to the
        // renderer's Z rotation in the TC world frame.
        d.rotZ = e.rotY * kDegToRad;       // primary heading.
        d.rotY = e.rotX * kDegToRad;       // pitch -> TC Y.
        d.rotX = e.rotZ * kDegToRad;       // roll  -> TC X.

        d.scale = float(e.scale) / 1024.0f;
        if (d.scale <= 0.0f)
            d.scale = 1.0f;

        out.push_back(std::move(d));
    }

    return true;
}

bool loadAdtWmoPlacements(CascClient& casc,
                          std::string const& mapDir,
                          uint32_t /*mapId*/,
                          int gx, int gy,
                          std::vector<WmoPlacementInstance>& out,
                          uint32_t obj0Fdid,
                          uint32_t rootFdid)
{
    out.clear();
    if (mapDir.empty() || !casc.isOpen())
        return false;

    auto buildPath = [&](char const* suffix) {
        // Virtual-path fallback for Classic-era WDTs without MAID; see
        // AdtReader.cpp rootPath() for the geographic-correctness note.
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

    // WMO refs live in obj0 for modern ADTs, monolithic root for legacy.
    // Caller-supplied FDIDs (from WDT MAID) take priority over filename
    // resolution.
    std::vector<uint8_t> bytes;
    bool ok = false;
    if (obj0Fdid != 0)
        ok = casc.readByFileDataId(obj0Fdid, bytes) && !bytes.empty();
    if (!ok && rootFdid != 0)
        ok = casc.readByFileDataId(rootFdid, bytes) && !bytes.empty();
    if (!ok)
    {
        if (!casc.readByPath(buildPath("_obj0.adt"), bytes) || bytes.empty())
        {
            if (!casc.readByPath(buildPath(".adt"), bytes) || bytes.empty())
                return false;
        }
    }

    uint8_t const* mwmoBody = nullptr; std::size_t mwmoSize = 0;
    uint8_t const* mwidBody = nullptr; std::size_t mwidSize = 0;
    uint8_t const* modfBody = nullptr; std::size_t modfSize = 0;
    ForEachTopLevelChunk(bytes.data(), bytes.size(),
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
        {
            if (magic == kFccMWMO && mwmoBody == nullptr) { mwmoBody = body; mwmoSize = bodySize; }
            if (magic == kFccMWID && mwidBody == nullptr) { mwidBody = body; mwidSize = bodySize; }
            if (magic == kFccMODF && modfBody == nullptr) { modfBody = body; modfSize = bodySize; }
            return true;
        });

    if (modfBody == nullptr || modfSize == 0)
        return true;     // ADT has no placed WMOs.

    // Legacy MWMO/MWID path resolution: MWID entries are byte offsets into
    // the packed cstring MWMO blob.  We resolve to a path string and let
    // the caller try CASC path lookup if it needs the file - no FDID
    // listfile here, so legacy entries are surfaced for diag only.
    std::vector<std::string> indexedPaths;
    if (mwmoBody != nullptr && mwidBody != nullptr && mwidSize >= sizeof(uint32_t))
    {
        std::size_t const count = mwidSize / sizeof(uint32_t);
        indexedPaths.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            uint32_t off;
            std::memcpy(&off, mwidBody + i * sizeof(uint32_t), sizeof(uint32_t));
            if (off >= mwmoSize)
            {
                indexedPaths.emplace_back();
                continue;
            }
            std::size_t end = off;
            while (end < mwmoSize && mwmoBody[end] != '\0')
                ++end;
            indexedPaths.emplace_back(reinterpret_cast<char const*>(mwmoBody + off), end - off);
        }
    }

    std::size_t const count = modfSize / sizeof(ModfEntry);
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        ModfEntry e;
        std::memcpy(&e, modfBody + i * sizeof(ModfEntry), sizeof(ModfEntry));

        WmoPlacementInstance w;
        w.uniqueId = e.uniqueId;
        w.flags    = e.flags;
        w.doodadSet = e.doodadSet;

        if (e.flags & 0x8u)
            w.wmoRootFileDataId = e.nameId;
        else if (e.nameId < indexedPaths.size())
            w.wmoPath = indexedPaths[e.nameId];

        w.posXYZ[0] = e.posX;  w.posXYZ[1] = e.posY;  w.posXYZ[2] = e.posZ;
        w.rotDegXYZ[0] = e.rotX;  w.rotDegXYZ[1] = e.rotY;  w.rotDegXYZ[2] = e.rotZ;
        // MODF.Scale is /1024 when flags & 0x4; otherwise the WMO instance
        // is uniform-scaled at 1.0 (pre-Legion).
        w.scale = (e.flags & 0x4u) ? (float(e.scale) / 1024.0f) : 1.0f;
        if (w.scale <= 0.0f)
            w.scale = 1.0f;

        out.push_back(std::move(w));
    }

    return true;
}

} // namespace world_editor::io
