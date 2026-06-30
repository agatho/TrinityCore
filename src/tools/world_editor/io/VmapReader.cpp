#include "VmapReader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <regex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

// Binary layouts mirror (in this order):
//   src/common/Collision/Models/WorldModel.cpp     (.vmo parser)
//   src/common/Collision/Models/ModelInstance.cpp  (ModelSpawn::readFromFile, rotation Euler order)
//   src/common/Collision/Maps/MapTree.cpp          (.vmtree InitMap, .vmtile LoadMapTile)
//   src/common/Collision/Management/VMapManager.cpp  (file name helpers)
//
// Local copies of magic + constants keep this translation unit free of any
// TC runtime include path.  If the on-disk format drifts the file size /
// chunk-magic asserts at load time will reject it (and stats.tilesFailed /
// modelsFailed will surface it to the smoketest).

namespace
{

// 8-byte magic that prefixes every vmap-format file ("VMAP_4.E" + NUL).
constexpr char VMAP_MAGIC[8] = { 'V', 'M', 'A', 'P', '_', '4', '.', 'E' };

// ModelInstanceFlags - see src/common/Collision/Models/ModelInstance.h.
constexpr uint8_t MOD_HAS_BOUND    = 1 << 0;
constexpr uint8_t MOD_PATH_ONLY    = 1 << 2;

// ------------------------------------------------------------------ helpers

// 3x3 rotation matrix in row-major layout (m[row][col]).  Multiplication
// follows the right-hand "world = R * v" convention.
struct Mat3
{
    float m[3][3];

    static Mat3 identity()
    {
        Mat3 r{};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.f;
        return r;
    }

    // Replicates G3D::Matrix3::fromEulerAnglesZYX(yaw, pitch, roll) -
    // see G3D's source: composite is Rz(yaw) * Ry(pitch) * Rx(roll).
    static Mat3 fromEulerAnglesZYX(float yaw, float pitch, float roll)
    {
        float const cy = std::cos(yaw),   sy = std::sin(yaw);
        float const cp = std::cos(pitch), sp = std::sin(pitch);
        float const cr = std::cos(roll),  sr = std::sin(roll);
        Mat3 r{};
        // Rz * Ry * Rx
        r.m[0][0] =  cy * cp;
        r.m[0][1] =  cy * sp * sr - sy * cr;
        r.m[0][2] =  cy * sp * cr + sy * sr;
        r.m[1][0] =  sy * cp;
        r.m[1][1] =  sy * sp * sr + cy * cr;
        r.m[1][2] =  sy * sp * cr - cy * sr;
        r.m[2][0] = -sp;
        r.m[2][1] =  cp * sr;
        r.m[2][2] =  cp * cr;
        return r;
    }
};

inline void mulVec(Mat3 const& M, float const inV[3], float outV[3])
{
    float const x = inV[0], y = inV[1], z = inV[2];
    outV[0] = M.m[0][0] * x + M.m[0][1] * y + M.m[0][2] * z;
    outV[1] = M.m[1][0] * x + M.m[1][1] * y + M.m[1][2] * z;
    outV[2] = M.m[2][0] * x + M.m[2][1] * y + M.m[2][2] * z;
}

// readChunk - matches src/common/Collision/VMapDefinitions.h.  Reads
// `len` bytes and compares them byte-for-byte against `expected`.  Used
// for both the 8-byte VMAP_MAGIC header and the 4-byte tag chunks.
bool readChunkTag(std::FILE* f, char const* expected, std::size_t len)
{
    char buf[16];
    if (len > sizeof(buf))
        return false;
    if (std::fread(buf, 1, len, f) != len)
        return false;
    return std::memcmp(buf, expected, len) == 0;
}

// Skip the BIH payload after an MBIH/GBIH tag - the editor doesn't need
// the acceleration structure to draw triangles, only the geometry.  The
// payload is: low(3 floats), high(3 floats), treeSize(uint32),
// tree[treeSize](uint32), objCount(uint32), objects[objCount](uint32).
bool skipBih(std::FILE* f, uint64_t& bytes)
{
    // 6 floats of bounds.
    float bounds[6];
    if (std::fread(bounds, sizeof(float), 6, f) != 6) return false;
    bytes += sizeof(bounds);
    uint32_t treeSize = 0;
    if (std::fread(&treeSize, sizeof(uint32_t), 1, f) != 1) return false;
    bytes += sizeof(uint32_t);
    if (std::fseek(f, long(treeSize) * long(sizeof(uint32_t)), SEEK_CUR) != 0) return false;
    bytes += uint64_t(treeSize) * sizeof(uint32_t);
    uint32_t objCount = 0;
    if (std::fread(&objCount, sizeof(uint32_t), 1, f) != 1) return false;
    bytes += sizeof(uint32_t);
    if (std::fseek(f, long(objCount) * long(sizeof(uint32_t)), SEEK_CUR) != 0) return false;
    bytes += uint64_t(objCount) * sizeof(uint32_t);
    return true;
}

// Skip a single WmoLiquid payload (LIQU chunk).  See
// WmoLiquid::readFromFile + WmoLiquid::GetFileSize in WorldModel.cpp.
// The outer "LIQU" tag + chunkSize have already been consumed by the
// caller; `chunkSize` here is the value following the tag.
bool skipLiquid(std::FILE* f, uint32_t chunkSize, uint64_t& bytes)
{
    if (chunkSize == 0)
        return true;
    if (std::fseek(f, long(chunkSize), SEEK_CUR) != 0) return false;
    bytes += chunkSize;
    return true;
}

// ------------------------------------------------------------------ .vmo

struct LoadedModel
{
    bool ok = false;
    // WMOD-chunk flags field.  Bit 0 = IsM2 per
    // src/common/Collision/Models/WorldModel.h:40 (`ModelFlags::IsM2 = 0x1`).
    // After vmap4_assembler runs, EVERY spawn ends up with MOD_HAS_BOUND
    // (the assembler computes the bound + sets the flag for M2s at
    // TileAssembler.cpp:357), so the spawn flag isn't usable for the
    // WMO-vs-M2 distinction.  The PER-MODEL flag stored in the .vmo's
    // WMOD chunk header IS reliable; we propagate it here.
    uint32_t flags    = 0;
    // Per-group: flat vertex array, flat triangle index array (3 uint32 per tri).
    // Kept separate (not pre-meshed) so we can transform vertices once per
    // ModelInstance without re-walking triangles.
    struct Group
    {
        std::vector<float>    vertices;   // 3 floats per vertex
        std::vector<uint32_t> indices;    // 3 uint32 per triangle
    };
    std::vector<Group> groups;
    uint64_t           bytesRead = 0;

    [[nodiscard]] bool isM2() const noexcept { return (flags & 0x1) != 0; }
};

// .vmo layout (see WorldModel::readFile):
//   VMAP_MAGIC (8) | "WMOD"(4) | chunkSize(4) | flags(4) | RootWMOID(4)
//   [ optional "GMOD"(4) | count(4) | count x GroupModel
//                       | "GBIH"(4) | BIH ]
//
// Each GroupModel (see GroupModel::readFromFile):
//   iBound (G3D::AABox = 2 x Vector3 = 24 bytes)
//   iMogpFlags (4) | iGroupWMOID (4)
//   "VERT"(4) | chunkSize(4) | count(4) | count x Vector3
//      (if count == 0 -> group ends here, before TRIM/MBIH/LIQU)
//   "TRIM"(4) | chunkSize(4) | count(4) | count x MeshTriangle (3 uint32)
//   "MBIH"(4) | BIH
//   "LIQU"(4) | chunkSize(4) | [if chunkSize > 0: WmoLiquid payload]
LoadedModel loadVmo(std::filesystem::path const& path)
{
    LoadedModel out;
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f)
        return out;

    auto fail = [&]() { std::fclose(f); return out; };

    // Header.
    if (!readChunkTag(f, VMAP_MAGIC, 8))     return fail();
    out.bytesRead += 8;
    if (!readChunkTag(f, "WMOD", 4))         return fail();
    out.bytesRead += 4;

    uint32_t wmodSize = 0, flags = 0, rootWmoId = 0;
    if (std::fread(&wmodSize,  sizeof(uint32_t), 1, f) != 1) return fail();
    if (std::fread(&flags,     sizeof(uint32_t), 1, f) != 1) return fail();
    if (std::fread(&rootWmoId, sizeof(uint32_t), 1, f) != 1) return fail();
    out.bytesRead += 3 * sizeof(uint32_t);
    out.flags = flags;
    (void)wmodSize; (void)rootWmoId;

    // GMOD is optional - if absent, the model has no geometry (rare, but
    // legitimate per the writer in WorldModel::writeFile).
    char tag[4];
    std::size_t const peeked = std::fread(tag, 1, 4, f);
    if (peeked != 4)
    {
        // Empty / truncated tail: treat as valid empty-geometry model.
        out.ok = true;
        std::fclose(f);
        return out;
    }
    out.bytesRead += 4;
    if (std::memcmp(tag, "GMOD", 4) != 0)
    {
        // Unexpected tag - reject the model.
        return fail();
    }

    uint32_t groupCount = 0;
    if (std::fread(&groupCount, sizeof(uint32_t), 1, f) != 1) return fail();
    out.bytesRead += sizeof(uint32_t);
    out.groups.reserve(groupCount);

    for (uint32_t g = 0; g < groupCount; ++g)
    {
        LoadedModel::Group group;

        // Skip bound (24 bytes), iMogpFlags (4), iGroupWMOID (4).
        if (std::fseek(f, 24 + 4 + 4, SEEK_CUR) != 0) return fail();
        out.bytesRead += 32;

        // VERT
        if (!readChunkTag(f, "VERT", 4)) return fail();
        uint32_t vertChunkSize = 0, vertCount = 0;
        if (std::fread(&vertChunkSize, sizeof(uint32_t), 1, f) != 1) return fail();
        if (std::fread(&vertCount,     sizeof(uint32_t), 1, f) != 1) return fail();
        out.bytesRead += 4 + 2 * sizeof(uint32_t);
        (void)vertChunkSize;

        if (vertCount == 0)
        {
            // GroupModel::readFromFile early-outs here without TRIM/MBIH/LIQU.
            out.groups.push_back(std::move(group));
            continue;
        }

        group.vertices.resize(std::size_t(vertCount) * 3);
        if (std::fread(group.vertices.data(), sizeof(float), group.vertices.size(), f)
            != group.vertices.size()) return fail();
        out.bytesRead += group.vertices.size() * sizeof(float);

        // TRIM
        if (!readChunkTag(f, "TRIM", 4)) return fail();
        uint32_t trimChunkSize = 0, triCount = 0;
        if (std::fread(&trimChunkSize, sizeof(uint32_t), 1, f) != 1) return fail();
        if (std::fread(&triCount,      sizeof(uint32_t), 1, f) != 1) return fail();
        out.bytesRead += 4 + 2 * sizeof(uint32_t);
        (void)trimChunkSize;

        group.indices.resize(std::size_t(triCount) * 3);
        if (triCount > 0)
        {
            if (std::fread(group.indices.data(), sizeof(uint32_t), group.indices.size(), f)
                != group.indices.size()) return fail();
            out.bytesRead += group.indices.size() * sizeof(uint32_t);
        }

        // MBIH - skip the acceleration structure; not needed for drawing.
        if (!readChunkTag(f, "MBIH", 4)) return fail();
        out.bytesRead += 4;
        if (!skipBih(f, out.bytesRead)) return fail();

        // LIQU - optional payload (chunkSize == 0 means no liquid).
        if (!readChunkTag(f, "LIQU", 4)) return fail();
        out.bytesRead += 4;
        uint32_t liqSize = 0;
        if (std::fread(&liqSize, sizeof(uint32_t), 1, f) != 1) return fail();
        out.bytesRead += sizeof(uint32_t);
        if (!skipLiquid(f, liqSize, out.bytesRead)) return fail();

        out.groups.push_back(std::move(group));
    }

    // GBIH follows - skip it.
    if (groupCount > 0)
    {
        if (!readChunkTag(f, "GBIH", 4)) return fail();
        out.bytesRead += 4;
        if (!skipBih(f, out.bytesRead)) return fail();
    }

    std::fclose(f);
    out.ok = true;
    return out;
}

// ------------------------------------------------------------------ .vmtile

struct ModelSpawn
{
    uint8_t  flags    = 0;
    uint8_t  adtId    = 0;
    uint32_t id       = 0;
    float    pos[3]   = { 0.f, 0.f, 0.f };
    float    rot[3]   = { 0.f, 0.f, 0.f };
    float    scale    = 1.f;
    std::string name;
};

// Mirrors ModelSpawn::readFromFile in ModelInstance.cpp.
bool readModelSpawn(std::FILE* f, ModelSpawn& out, uint64_t& bytes)
{
    if (std::fread(&out.flags,  sizeof(uint8_t),  1, f) != 1) return false;
    if (std::fread(&out.adtId,  sizeof(uint8_t),  1, f) != 1) return false;
    if (std::fread(&out.id,     sizeof(uint32_t), 1, f) != 1) return false;
    if (std::fread(out.pos,     sizeof(float),    3, f) != 3) return false;
    if (std::fread(out.rot,     sizeof(float),    3, f) != 3) return false;
    if (std::fread(&out.scale,  sizeof(float),    1, f) != 1) return false;
    bytes += 1 + 1 + 4 + 12 + 12 + 4;

    bool const hasBound = (out.flags & MOD_HAS_BOUND) != 0;
    if (hasBound)
    {
        // 2 x Vector3 - 24 bytes - bounds (not needed for triangle materialization).
        if (std::fseek(f, 24, SEEK_CUR) != 0) return false;
        bytes += 24;
    }

    uint32_t nameLen = 0;
    if (std::fread(&nameLen, sizeof(uint32_t), 1, f) != 1) return false;
    bytes += sizeof(uint32_t);
    if (nameLen > 500)
        return false;
    out.name.resize(nameLen);
    if (nameLen > 0)
    {
        if (std::fread(out.name.data(), 1, nameLen, f) != nameLen) return false;
        bytes += nameLen;
    }
    return true;
}

bool readTileHeader(std::FILE* f, uint32_t& outSpawnCount, uint64_t& bytes)
{
    if (!readChunkTag(f, VMAP_MAGIC, 8)) return false;
    bytes += 8;
    if (std::fread(&outSpawnCount, sizeof(uint32_t), 1, f) != 1) return false;
    bytes += sizeof(uint32_t);
    return true;
}

// ------------------------------------------------------------------ paths

std::string mapIdDir(uint32_t mapId)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04u", mapId);
    return std::string(buf);
}

} // namespace

namespace world_editor::io
{

std::string vmtreeFilename(uint32_t mapId)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04u.vmtree", mapId);
    return std::string(buf);
}

std::string vmtileFilename(uint32_t mapId, int tx, int ty)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u_%02d_%02d.vmtile", mapId, tx, ty);
    return std::string(buf);
}

LoadedVmap::LoadedVmap(std::vector<VmapTriangle>&& tris, VmapLoadStats stats)
    : m_triangles(std::move(tris)), m_stats(stats)
{
}

LoadedVmap::LoadedVmap(std::vector<VmapTriangle>&& tris,
                       std::vector<WmoInstanceAabb>&& wmoAabbs,
                       VmapLoadStats stats)
    : m_triangles(std::move(tris)), m_wmoAabbs(std::move(wmoAabbs)), m_stats(stats)
{
}

LoadedVmap loadVmaps(std::filesystem::path const& vmapsDir, uint32_t mapId,
                     int maxTilesOrZeroForAll)
{
    VmapLoadStats stats{};
    std::vector<VmapTriangle> tris;
    // Per-WMO-instance XY AABBs (one entry per non-M2 spawn that yielded
    // at least one triangle).  Built alongside `tris` so the editor's 2D
    // viewer can paint building footprints without re-walking the flat
    // triangle list and re-clustering.
    std::vector<WmoInstanceAabb> wmoAabbs;

    std::filesystem::path const mapDir = vmapsDir / mapIdDir(mapId);

    std::error_code ec;
    if (!std::filesystem::exists(mapDir, ec) || ec)
        return LoadedVmap{ std::move(tris), std::move(wmoAabbs), stats };

    // Sanity-check that the .vmtree header is readable.  We don't use the
    // BIH; we just want to fail-fast on a wrong-format directory.
    std::filesystem::path const treePath = mapDir / vmtreeFilename(mapId);
    if (std::FILE* tf = std::fopen(treePath.string().c_str(), "rb"))
    {
        char hdr[8];
        if (std::fread(hdr, 1, 8, tf) != 8 || std::memcmp(hdr, VMAP_MAGIC, 8) != 0)
        {
            std::fclose(tf);
            return LoadedVmap{ std::move(tris), std::move(wmoAabbs), stats };
        }
        std::fclose(tf);
        stats.bytesLoaded += 8;
    }
    // (Tree file missing isn't fatal - a few maps ship tile data only.)

    // Glob the per-tile .vmtile siblings.
    char prefixBuf[8];
    std::snprintf(prefixBuf, sizeof(prefixBuf), "%04u_", mapId);
    std::string const prefix = prefixBuf;
    std::regex const tileRegex(R"(\d{4}_\d{2}_\d{2}\.vmtile)");

    // Unique model cache: filename -> parsed model.  Many tiles reference
    // the same WMO (city blocks, repeating props), so this is the perf win.
    std::unordered_map<std::string, LoadedModel> modelCache;

    auto it = std::filesystem::directory_iterator(mapDir, ec);
    if (ec)
        return LoadedVmap{ std::move(tris), std::move(wmoAabbs), stats };

    std::vector<std::filesystem::path> tilePaths;
    for (auto const& entry : it)
    {
        if (!entry.is_regular_file(ec) || ec)
            continue;
        std::string const filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) != 0)
            continue;
        if (!std::regex_match(filename, tileRegex))
            continue;
        tilePaths.push_back(entry.path());
    }
    // Deterministic order so the smoketest output is stable.
    std::sort(tilePaths.begin(), tilePaths.end());

    if (maxTilesOrZeroForAll > 0 && tilePaths.size() > std::size_t(maxTilesOrZeroForAll))
        tilePaths.resize(std::size_t(maxTilesOrZeroForAll));

    for (std::filesystem::path const& tilePath : tilePaths)
    {
        std::FILE* tf = std::fopen(tilePath.string().c_str(), "rb");
        if (!tf)
        {
            ++stats.tilesFailed;
            continue;
        }

        uint32_t spawnCount = 0;
        if (!readTileHeader(tf, spawnCount, stats.bytesLoaded))
        {
            ++stats.tilesFailed;
            std::fclose(tf);
            continue;
        }

        bool tileOk = true;
        for (uint32_t i = 0; i < spawnCount; ++i)
        {
            ModelSpawn spawn;
            if (!readModelSpawn(tf, spawn, stats.bytesLoaded))
            {
                tileOk = false;
                ++stats.instancesFailed;
                break;
            }
            // Skip "path only" placeholders - they aren't in the worldserver's
            // collision view, see LoadPathOnlyModels gate in MapTree.cpp.
            if (spawn.flags & MOD_PATH_ONLY)
                continue;

            // Resolve / cache the underlying .vmo (lives flat under vmapsDir).
            // Spawn names are stored without the .vmo extension - see
            // VMapManager::acquireModelInstance (basepath + filename + ".vmo").
            auto cacheIt = modelCache.find(spawn.name);
            if (cacheIt == modelCache.end())
            {
                LoadedModel model = loadVmo(vmapsDir / (spawn.name + ".vmo"));
                stats.bytesLoaded += model.bytesRead;
                if (model.ok)
                    ++stats.modelsLoaded;
                else
                    ++stats.modelsFailed;
                cacheIt = modelCache.emplace(spawn.name, std::move(model)).first;
            }
            LoadedModel const& model = cacheIt->second;
            if (!model.ok)
                continue;

            // Build the model->world transform.  ModelInstance.cpp stores
            // the inverse rotation: iInvRot = fromEulerAnglesZYX(yaw, pitch, roll).inverse().
            // The forward rotation is therefore fromEulerAnglesZYX(yaw, pitch, roll)
            // with arguments (pi * rot.y / 180, pi * rot.x / 180, pi * rot.z / 180).
            constexpr float kPi = 3.14159265358979323846f;
            Mat3 const rot = Mat3::fromEulerAnglesZYX(
                kPi * spawn.rot[1] / 180.f,   // yaw   <- rot.y
                kPi * spawn.rot[0] / 180.f,   // pitch <- rot.x
                kPi * spawn.rot[2] / 180.f);  // roll  <- rot.z

            // Materialize triangles in world space.
            //
            // WMO vs M2 distinction comes from the PER-MODEL flag stored
            // in the .vmo's WMOD chunk header (bit 0 = IsM2; see
            // src/common/Collision/Models/WorldModel.h:40).  Earlier
            // revisions used the per-spawn MOD_HAS_BOUND flag but
            // vmap4_assembler (TileAssembler.cpp:357) sets that flag
            // on M2s after computing their bound, so the spawn flag
            // is useless for the distinction post-assembly.
            VmapSpawnKind const spawnKind =
                model.isM2() ? VmapSpawnKind::M2 : VmapSpawnKind::Wmo;
            ++stats.instancesLoaded;
            // Accumulate this instance's world-XY AABB across all groups.
            // Only emitted into `wmoAabbs` for WMO spawns AND only when at
            // least one valid triangle was materialized.
            float instMinX =  std::numeric_limits<float>::infinity();
            float instMaxX = -std::numeric_limits<float>::infinity();
            float instMinY =  std::numeric_limits<float>::infinity();
            float instMaxY = -std::numeric_limits<float>::infinity();
            bool  instHasTri = false;
            for (auto const& grp : model.groups)
            {
                std::size_t const triCount = grp.indices.size() / 3;
                for (std::size_t t = 0; t < triCount; ++t)
                {
                    VmapTriangle out{};
                    out.kind = spawnKind;
                    bool indexOk = true;
                    for (int corner = 0; corner < 3; ++corner)
                    {
                        uint32_t const idx = grp.indices[t * 3 + corner];
                        if (std::size_t(idx) * 3 + 2 >= grp.vertices.size())
                        {
                            indexOk = false;
                            break;
                        }
                        float local[3] = {
                            grp.vertices[idx * 3 + 0],
                            grp.vertices[idx * 3 + 1],
                            grp.vertices[idx * 3 + 2],
                        };
                        // world = rot * (local * scale) + pos
                        local[0] *= spawn.scale;
                        local[1] *= spawn.scale;
                        local[2] *= spawn.scale;
                        float rotated[3];
                        mulVec(rot, local, rotated);
                        out.v[corner][0] = rotated[0] + spawn.pos[0];
                        out.v[corner][1] = rotated[1] + spawn.pos[1];
                        out.v[corner][2] = rotated[2] + spawn.pos[2];
                    }
                    if (indexOk)
                    {
                        tris.push_back(out);
                        if (spawnKind == VmapSpawnKind::Wmo)
                        {
                            for (int corner = 0; corner < 3; ++corner)
                            {
                                float const wx = out.v[corner][0];
                                float const wy = out.v[corner][1];
                                if (wx < instMinX) instMinX = wx;
                                if (wx > instMaxX) instMaxX = wx;
                                if (wy < instMinY) instMinY = wy;
                                if (wy > instMaxY) instMaxY = wy;
                            }
                            instHasTri = true;
                        }
                    }
                }
            }
            if (instHasTri)
            {
                WmoInstanceAabb aabb{};
                aabb.minX = instMinX;
                aabb.maxX = instMaxX;
                aabb.minY = instMinY;
                aabb.maxY = instMaxY;
                wmoAabbs.push_back(aabb);
            }
        }

        std::fclose(tf);
        if (tileOk)
            ++stats.tilesLoaded;
        else
            ++stats.tilesFailed;
    }

    stats.triangleCount = tris.size();
    stats.wmoTriangleCount = 0;
    stats.m2TriangleCount  = 0;
    for (VmapTriangle const& t : tris)
    {
        if (t.kind == VmapSpawnKind::M2) ++stats.m2TriangleCount;
        else                              ++stats.wmoTriangleCount;
    }
    return LoadedVmap{ std::move(tris), std::move(wmoAabbs), stats };
}

} // namespace world_editor::io
