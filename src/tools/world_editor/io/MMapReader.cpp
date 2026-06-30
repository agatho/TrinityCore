#include "MMapReader.h"

#include <DetourAlloc.h>
#include <DetourNavMesh.h>
#include <DetourStatus.h>

#include <algorithm>
#include <cstdio>
#include <regex>
#include <string>
#include <system_error>
#include <vector>

// Binary layout mirrors src/common/mmaps_common/MMapDefines.h. We keep
// a local copy so the editor compiles standalone (no TC server runtime
// include path needed). If MMapDefines.h changes, drift here is
// detected via the static_assert sizes below + the version constant
// matched against the file header.

namespace
{
#pragma pack(push, 1)
struct MmapNavMeshHeader
{
    uint32_t mmapMagic;
    uint32_t mmapVersion;
    dtNavMeshParams params;
    uint32_t offmeshConnectionCount;
};
struct MmapTileHeader
{
    uint32_t mmapMagic;
    uint32_t dtVersion;
    uint32_t mmapVersion;
    uint32_t size;
    char     usesLiquids;
    char     padding[3];
};
#pragma pack(pop)

static_assert(sizeof(MmapNavMeshHeader) == 40, "MmapNavMeshHeader size drift");
static_assert(sizeof(MmapTileHeader)    == 20, "MmapTileHeader size drift");

constexpr uint32_t MMAP_MAGIC            = 0x4d4d4150; // 'MMAP'
constexpr uint32_t MMAP_VERSION_EXPECTED = 16;

bool readNavMeshParams(std::filesystem::path const& path, dtNavMeshParams& outParams,
                       uint32_t& outMmapVer)
{
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f)
        return false;

    MmapNavMeshHeader header{};
    bool const ok = std::fread(&header, sizeof(header), 1, f) == 1;
    std::fclose(f);

    if (!ok)
        return false;
    if (header.mmapMagic != MMAP_MAGIC)
        return false;
    if (header.mmapVersion != MMAP_VERSION_EXPECTED)
        return false;

    outParams  = header.params;
    outMmapVer = header.mmapVersion;
    return true;
}

bool addTileFromFile(dtNavMesh* navMesh, std::filesystem::path const& path,
                     uint64_t& bytesLoaded, uint32_t& outDtVer)
{
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f)
        return false;

    MmapTileHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1)
    {
        std::fclose(f);
        return false;
    }
    if (header.mmapMagic   != MMAP_MAGIC)            { std::fclose(f); return false; }
    if (header.mmapVersion != MMAP_VERSION_EXPECTED) { std::fclose(f); return false; }
    if (header.size == 0)                            { std::fclose(f); return false; }
    outDtVer = header.dtVersion;

    void* data = dtAlloc(static_cast<int>(header.size), DT_ALLOC_PERM);
    if (!data)
    {
        std::fclose(f);
        return false;
    }
    if (std::fread(data, header.size, 1, f) != 1)
    {
        dtFree(data);
        std::fclose(f);
        return false;
    }
    std::fclose(f);
    bytesLoaded += header.size;

    dtTileRef ref = 0;
    dtStatus const status = navMesh->addTile(
        static_cast<unsigned char*>(data),
        static_cast<int>(header.size),
        DT_TILE_FREE_DATA, 0, &ref);
    if (dtStatusFailed(status))
    {
        // addTile took ownership only if it succeeded.
        dtFree(data);
        return false;
    }
    return true;
}
} // namespace

namespace world_editor::io
{

LoadedMMap::LoadedMMap(dtNavMesh* mesh, MMapLoadStats stats)
    : m_mesh(mesh), m_stats(stats) {}

LoadedMMap::LoadedMMap(LoadedMMap&& other) noexcept
    : m_mesh(std::exchange(other.m_mesh, nullptr))
    , m_stats(std::exchange(other.m_stats, MMapLoadStats{}))
{
}

LoadedMMap& LoadedMMap::operator=(LoadedMMap&& other) noexcept
{
    if (this != &other)
    {
        if (m_mesh)
            dtFreeNavMesh(m_mesh);
        m_mesh  = std::exchange(other.m_mesh,  nullptr);
        m_stats = std::exchange(other.m_stats, MMapLoadStats{});
    }
    return *this;
}

LoadedMMap::~LoadedMMap()
{
    if (m_mesh)
        dtFreeNavMesh(m_mesh);
}

LoadedMMap loadMap(std::filesystem::path const& mmapsDir, uint32_t mapId)
{
    MMapLoadStats stats{};

    std::filesystem::path const navMeshPath = mmapsDir / mmapFilename(mapId);

    dtNavMeshParams params{};
    if (!readNavMeshParams(navMeshPath, params, stats.mmapVersion))
        return LoadedMMap{};

    dtNavMesh* navMesh = dtAllocNavMesh();
    if (!navMesh)
        return LoadedMMap{};

    if (dtStatusFailed(navMesh->init(&params)))
    {
        dtFreeNavMesh(navMesh);
        return LoadedMMap{};
    }

    // Glob "<mapId>_<tx>_<ty>.mmtile" siblings in the same directory.
    // The same regex/format mmap_world_dump uses, anchored to mapId so
    // we don't pick up neighbour-map tiles in shared dump dirs.
    char prefixBuf[8];
    std::snprintf(prefixBuf, sizeof(prefixBuf), "%04u_", mapId);
    std::string const prefix = prefixBuf;
    std::regex const tileRegex(R"(\d{4}_(\d{2})_(\d{2})\.mmtile)");

    std::error_code ec;
    auto it = std::filesystem::directory_iterator(mmapsDir, ec);
    if (ec)
    {
        return LoadedMMap{ navMesh, stats };
    }

    for (auto const& entry : it)
    {
        if (!entry.is_regular_file(ec) || ec)
            continue;

        std::string const filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) != 0)   // fast prefix filter before regex.
            continue;

        std::smatch match;
        if (!std::regex_match(filename, match, tileRegex))
            continue;

        if (addTileFromFile(navMesh, entry.path(), stats.bytesLoaded, stats.dtVersion))
            ++stats.tilesLoaded;
        else
            ++stats.tilesFailed;
    }

    // Compute aggregate poly count once - cheap and useful for status.
    // Resolve the const overload of getTile()/getMaxTiles() explicitly;
    // the non-const dtMeshTile* getTile(int) is private in Detour.
    dtNavMesh const* navMeshConst = navMesh;
    for (int ti = 0; ti < navMeshConst->getMaxTiles(); ++ti)
    {
        dtMeshTile const* tile = navMeshConst->getTile(ti);
        if (!tile || !tile->header)
            continue;
        stats.polyCount += static_cast<uint64_t>(tile->header->polyCount);
    }

    return LoadedMMap{ navMesh, stats };
}

std::string mmapFilename(uint32_t mapId)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04u.mmap", mapId);
    return std::string(buf);
}

std::string mmtileFilename(uint32_t mapId, int tx, int ty)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u_%02d_%02d.mmtile", mapId, tx, ty);
    return std::string(buf);
}

} // namespace world_editor::io
