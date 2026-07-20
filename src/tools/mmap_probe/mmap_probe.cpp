/*
 * mmap_probe - Standalone Detour findPath probe for a TrinityCore .mmap/.mmtile pair.
 *
 * Loads every .mmtile in the given directory for the requested mapId, sets up a
 * dtNavMesh + dtNavMeshQuery, and runs findNearestPoly + findPath between two
 * TC world coordinates. Reports outcome and exits 0 iff Detour returned a
 * complete (non-partial) path whose final waypoint is within 5y of dst.
 *
 * Pure Detour API; deliberately avoids the TC core (no Log, no MMapManager,
 * no PathGenerator) so it can be built and run without standing up the full
 * worldserver. See src/server/scripts/Commands/cs_mmaps.cpp and
 * src/server/game/Movement/PathGenerator.cpp for the production reference.
 */

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <DetourAlloc.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

// Header layout copied verbatim from src/common/mmaps_common/MMapDefines.h.
// MUST stay byte-for-byte identical with the generator output.
static constexpr uint32_t MMAP_MAGIC = 0x4d4d4150; // 'MMAP'
static constexpr uint32_t MMAP_VERSION_EXPECTED = 16;

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
static_assert(sizeof(MmapTileHeader) == 20, "MmapTileHeader size drift");

// Convert a TC area id to the bitmask Detour's filter expects (matches the
// formula in MMapDefines.h NavTerrainFlag). The probe enables WALK on the
// usual surfaces (ground, ground steep, water, road). Off-mesh links and
// magma/slime intentionally left off — same as PathGenerator's default
// "land creature" filter.
static constexpr uint16_t NAV_GROUND       = 1u << (11 - 11); // 0x01
static constexpr uint16_t NAV_GROUND_STEEP = 1u << (11 - 10); // 0x02
static constexpr uint16_t NAV_WATER        = 1u << (11 - 9);  // 0x04
// NAV_MAGMA_SLIME = 0x08
static constexpr uint16_t NAV_ROAD         = 1u << (11 - 7);  // 0x10

namespace
{
std::string FormatMmapPath(std::string const& dir, uint32_t mapId)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u.mmap", mapId);
    return (std::filesystem::path(dir) / buf).string();
}

bool ReadFile(std::string const& path, std::vector<uint8_t>& out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0)
    {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return n == out.size();
}

bool LoadNavMeshParams(std::string const& dir, uint32_t mapId, dtNavMeshParams& params)
{
    std::string path = FormatMmapPath(dir, mapId);
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        std::fprintf(stderr, "ERROR: cannot open %s\n", path.c_str());
        return false;
    }
    MmapNavMeshHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1)
    {
        std::fprintf(stderr, "ERROR: short read of %s\n", path.c_str());
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    if (header.mmapMagic != MMAP_MAGIC)
    {
        std::fprintf(stderr, "ERROR: %s magic mismatch (got 0x%08x, want 0x%08x)\n",
            path.c_str(), header.mmapMagic, MMAP_MAGIC);
        return false;
    }
    if (header.mmapVersion != MMAP_VERSION_EXPECTED)
    {
        std::fprintf(stderr, "ERROR: %s version mismatch (got %u, want %u)\n",
            path.c_str(), header.mmapVersion, MMAP_VERSION_EXPECTED);
        return false;
    }
    params = header.params;
    return true;
}

struct LoadedTile
{
    int x;
    int y;
};

// Add one .mmtile file to the dtNavMesh. Returns true on success.
bool AddTileFromFile(dtNavMesh* navMesh, std::string const& path, int tx, int ty)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        std::fprintf(stderr, "WARN: cannot open %s\n", path.c_str());
        return false;
    }

    MmapTileHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1)
    {
        std::fprintf(stderr, "WARN: short header read %s\n", path.c_str());
        std::fclose(f);
        return false;
    }
    if (header.mmapMagic != MMAP_MAGIC || header.mmapVersion != MMAP_VERSION_EXPECTED)
    {
        std::fprintf(stderr, "WARN: bad header for %s (magic=0x%08x ver=%u)\n",
            path.c_str(), header.mmapMagic, header.mmapVersion);
        std::fclose(f);
        return false;
    }

    void* data = dtAlloc(header.size, DT_ALLOC_PERM);
    if (!data)
    {
        std::fclose(f);
        return false;
    }
    if (std::fread(data, header.size, 1, f) != 1)
    {
        std::fprintf(stderr, "WARN: short data read %s\n", path.c_str());
        dtFree(data);
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    dtTileRef tileRef = 0;
    dtStatus s = navMesh->addTile(static_cast<unsigned char*>(data), header.size, DT_TILE_FREE_DATA, 0, &tileRef);
    if (dtStatusFailed(s))
    {
        std::fprintf(stderr, "WARN: addTile failed for [%d,%d] status=0x%08x\n", tx, ty, s);
        dtFree(data);
        return false;
    }
    return true;
}

int LoadAllTiles(dtNavMesh* navMesh, std::string const& dir, uint32_t mapId, std::vector<LoadedTile>& loaded)
{
    // Pattern: 0001_XX_YY.mmtile
    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "%04u_", mapId);
    std::regex re(R"((\d{4})_(\d{2})_(\d{2})\.mmtile)");

    int n = 0;
    for (auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;
        std::string name = entry.path().filename().string();
        std::smatch m;
        if (!std::regex_match(name, m, re))
            continue;
        if (std::stoul(m[1].str()) != mapId)
            continue;
        int tx = std::stoi(m[2].str());
        int ty = std::stoi(m[3].str());
        if (AddTileFromFile(navMesh, entry.path().string(), tx, ty))
        {
            loaded.push_back({ tx, ty });
            ++n;
        }
    }
    return n;
}

// Decode a Detour findPath result status into the requested label set.
const char* DescribeStatus(dtStatus st, bool partial, bool startFar, bool endFar, bool full, bool reached, int polys)
{
    if (dtStatusFailed(st))
        return "NOPATH";
    if (startFar)
        return "FARFROMPOLY_START";
    if (endFar)
        return "FARFROMPOLY_END";
    if (partial)
        return "PARTIAL";
    if (polys >= 74)
        return "SHORT";
    if (!reached)
        return "INCOMPLETE";
    if (full)
        return "OK";
    return "INCOMPLETE";
}
} // anonymous namespace

// Run one findPath probe on an already-loaded navMesh/query and print the
// same result block the original single-shot tool printed (STATUS, snap
// info, polyCount, straight-path dump with OFFMESH flags). Does NOT free
// navMesh/query -- the caller owns their lifetime (batch mode reuses them
// across many calls). Returns 0 iff Detour returned a complete path whose
// final waypoint is within 5y of dst, 1 otherwise.
int RunQuery(dtNavMesh* navMesh, dtNavMeshQuery* query,
    float srcX, float srcY, float srcZ,
    float dstX, float dstY, float dstZ)
{
    (void)navMesh;

    // Axis flip: TC (x, y, z) -> Detour (y, z, x). Matches PathGenerator.
    float startPoint[3] = { srcY, srcZ, srcX };
    float endPoint[3]   = { dstY, dstZ, dstX };
    // Search box: same as PathGenerator's default (3y horizontal, 5y vertical).
    float extents[3] = { 3.0f, 5.0f, 3.0f };

    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_WATER | NAV_ROAD);
    filter.setExcludeFlags(0);

    dtPolyRef startRef = 0;
    float startClosest[3] = { 0, 0, 0 };
    bool startFar = false;
    dtStatus s = query->findNearestPoly(startPoint, extents, &filter, &startRef, startClosest);
    if (dtStatusFailed(s) || startRef == 0)
    {
        // Retry with the wider vertical box PathGenerator uses on the
        // second attempt (50y).
        float wide[3] = { 3.0f, 50.0f, 3.0f };
        s = query->findNearestPoly(startPoint, wide, &filter, &startRef, startClosest);
        if (dtStatusFailed(s) || startRef == 0)
        {
            std::printf("  START: no poly found (status=0x%08x)\n", s);
            std::printf("STATUS: FARFROMPOLY_START\n");
            return 1;
        }
        startFar = true;
    }

    // Translate snapped point back into TC coordinates.
    float startTC[3] = { startClosest[2], startClosest[0], startClosest[1] };
    std::printf("  START: ref=%llu snapped (yzx Detour)=(%.3f,%.3f,%.3f) -> TC (%.3f,%.3f,%.3f)%s\n",
        static_cast<unsigned long long>(startRef),
        startClosest[0], startClosest[1], startClosest[2],
        startTC[0], startTC[1], startTC[2],
        startFar ? " [WIDE SEARCH]" : "");

    dtPolyRef endRef = 0;
    float endClosest[3] = { 0, 0, 0 };
    bool endFar = false;
    s = query->findNearestPoly(endPoint, extents, &filter, &endRef, endClosest);
    if (dtStatusFailed(s) || endRef == 0)
    {
        float wide[3] = { 3.0f, 50.0f, 3.0f };
        s = query->findNearestPoly(endPoint, wide, &filter, &endRef, endClosest);
        if (dtStatusFailed(s) || endRef == 0)
        {
            std::printf("  END:   no poly found (status=0x%08x)\n", s);
            std::printf("STATUS: FARFROMPOLY_END\n");
            return 1;
        }
        endFar = true;
    }

    float endTC[3] = { endClosest[2], endClosest[0], endClosest[1] };
    std::printf("  END:   ref=%llu snapped (yzx Detour)=(%.3f,%.3f,%.3f) -> TC (%.3f,%.3f,%.3f)%s\n",
        static_cast<unsigned long long>(endRef),
        endClosest[0], endClosest[1], endClosest[2],
        endTC[0], endTC[1], endTC[2],
        endFar ? " [WIDE SEARCH]" : "");

    constexpr int MAX_PATH_LENGTH = 74; // mirror PathGenerator.h
    dtPolyRef pathPolys[MAX_PATH_LENGTH] = { 0 };
    int polyCount = 0;
    s = query->findPath(startRef, endRef, startClosest, endClosest, &filter,
        pathPolys, &polyCount, MAX_PATH_LENGTH);

    bool partial = (s & DT_PARTIAL_RESULT) != 0;
    bool failed = dtStatusFailed(s);

    std::printf("  findPath: status=0x%08x partial=%d failed=%d polyCount=%d\n",
        s, partial ? 1 : 0, failed ? 1 : 0, polyCount);

    if (failed || polyCount == 0)
    {
        std::printf("STATUS: NOPATH\n");
        return 1;
    }

    // Run a straight-path string-pull to get waypoints, so we can check
    // distance from final waypoint to requested dst (in 3D Detour
    // coords) and report the last few points.
    constexpr int MAX_STRAIGHT_POINTS = MAX_PATH_LENGTH;
    float straight[MAX_STRAIGHT_POINTS * 3] = { 0 };
    unsigned char straightFlags[MAX_STRAIGHT_POINTS] = { 0 };
    dtPolyRef straightRefs[MAX_STRAIGHT_POINTS] = { 0 };
    int straightCount = 0;
    dtStatus s2 = query->findStraightPath(startClosest, endClosest, pathPolys, polyCount,
        straight, straightFlags, straightRefs, &straightCount, MAX_STRAIGHT_POINTS, 0);
    if (dtStatusFailed(s2))
    {
        std::printf("  findStraightPath failed: status=0x%08x\n", s2);
        straightCount = 0;
    }

    float lastWp[3] = { endClosest[0], endClosest[1], endClosest[2] };
    if (straightCount > 0)
    {
        lastWp[0] = straight[(straightCount - 1) * 3 + 0];
        lastWp[1] = straight[(straightCount - 1) * 3 + 1];
        lastWp[2] = straight[(straightCount - 1) * 3 + 2];
    }

    // Distance from last waypoint to the *requested* destination (NOT the
    // snapped endRef). Use Detour-frame coords for both sides.
    float dx = lastWp[0] - endPoint[0];
    float dy = lastWp[1] - endPoint[1];
    float dz = lastWp[2] - endPoint[2];
    float planarDist = std::sqrt(dx * dx + dz * dz); // Detour: y,z,x -> XZ is horizontal
    float dist3D = std::sqrt(dx * dx + dy * dy + dz * dz);

    bool reachedDest = (pathPolys[polyCount - 1] == endRef) && !partial;

    std::printf("  path: polys=%d planar_dist_to_requested_dst=%.3fy 3d_dist=%.3fy reached_endRef=%d\n",
        polyCount, planarDist, dist3D, reachedDest ? 1 : 0);

    // Full straight-path dump + degenerate-segment scan. _checkPathLengths()
    // (MoveSpline) rejects the WHOLE spline if any interior segment has
    // squaredLength < 0.01 (i.e. consecutive points < 0.1y apart). Off-mesh
    // connection endpoints can land near an adjacent corner -> such a segment.
    std::printf("  straight path (%d pts, TC coords; OFFMESH flag, seg-to-next):\n", straightCount);
    int degenerate = 0;
    for (int i = 0; i < straightCount; ++i)
    {
        float wx = straight[i * 3 + 2];
        float wy = straight[i * 3 + 0];
        float wz = straight[i * 3 + 1];
        bool offmesh = (straightFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
        float seg = -1.0f;
        if (i + 1 < straightCount)
        {
            float ndx = straight[(i+1)*3+0] - straight[i*3+0];
            float ndy = straight[(i+1)*3+1] - straight[i*3+1];
            float ndz = straight[(i+1)*3+2] - straight[i*3+2];
            seg = std::sqrt(ndx*ndx + ndy*ndy + ndz*ndz);
        }
        bool bad = (seg >= 0.0f && seg < 0.1f);
        if (bad) ++degenerate;
        std::printf("    [%2d]%s TC=(%.3f, %.3f, %.3f) seg=%.4f%s\n",
            i, offmesh ? " OFFMESH" : "       ", wx, wy, wz,
            seg, bad ? "  <<< DEGENERATE (<0.1y) -> _checkPathLengths REJECT" : "");
    }
    std::printf("  DEGENERATE_SEGMENTS=%d\n", degenerate);

    const char* label = DescribeStatus(s, partial, startFar, endFar,
        reachedDest, /*reached*/ planarDist <= 5.0f, polyCount);
    std::printf("STATUS: %s\n", label);

    bool ok = !partial && !failed && (planarDist <= 5.0f);
    std::printf("EXIT: %d\n", ok ? 0 : 1);

    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr,
            "Usage: %s <mmaps_dir> <mapId> <srcX> <srcY> <srcZ> <dstX> <dstY> <dstZ>\n"
            "       %s <mmaps_dir> <mapId> --batch   (reads 'sx sy sz dx dy dz' lines from stdin)\n",
            argv[0], argv[0]);
        return 2;
    }

    std::string dir = argv[1];
    uint32_t mapId = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
    bool batch = (argc == 4 && std::strcmp(argv[3], "--batch") == 0);
    if (!batch && argc != 9)
    {
        std::fprintf(stderr,
            "Usage: %s <mmaps_dir> <mapId> <srcX> <srcY> <srcZ> <dstX> <dstY> <dstZ>\n"
            "       %s <mmaps_dir> <mapId> --batch   (reads 'sx sy sz dx dy dz' lines from stdin)\n",
            argv[0], argv[0]);
        return 2;
    }

    std::printf("mmap_probe: dir=%s mapId=%u%s\n", dir.c_str(), mapId, batch ? " [BATCH]" : "");

    // Parsed early (and printed here) only for the single-shot path, so the
    // overall print order stays byte-identical to the original tool.
    float srcX = 0, srcY = 0, srcZ = 0, dstX = 0, dstY = 0, dstZ = 0;
    if (!batch)
    {
        srcX = std::strtof(argv[3], nullptr);
        srcY = std::strtof(argv[4], nullptr);
        srcZ = std::strtof(argv[5], nullptr);
        dstX = std::strtof(argv[6], nullptr);
        dstY = std::strtof(argv[7], nullptr);
        dstZ = std::strtof(argv[8], nullptr);
        std::printf("  TC src = (%.3f, %.3f, %.3f)\n", srcX, srcY, srcZ);
        std::printf("  TC dst = (%.3f, %.3f, %.3f)\n", dstX, dstY, dstZ);
    }

    dtNavMeshParams params{};
    if (!LoadNavMeshParams(dir, mapId, params))
        return 2;

    std::printf("  navMeshParams: orig=(%.3f,%.3f,%.3f) tileWidth=%.3f tileHeight=%.3f maxTiles=%d maxPolys=%d\n",
        params.orig[0], params.orig[1], params.orig[2],
        params.tileWidth, params.tileHeight, params.maxTiles, params.maxPolys);

    dtNavMesh* navMesh = dtAllocNavMesh();
    if (!navMesh || dtStatusFailed(navMesh->init(&params)))
    {
        std::fprintf(stderr, "ERROR: dtNavMesh init failed\n");
        return 2;
    }

    std::vector<LoadedTile> loaded;
    int nTiles = LoadAllTiles(navMesh, dir, mapId, loaded);
    std::printf("  loaded %d tile(s):", nTiles);
    for (auto const& t : loaded)
        std::printf(" [%02d,%02d]", t.x, t.y);
    std::printf("\n");
    if (nTiles == 0)
    {
        std::fprintf(stderr, "ERROR: no tiles loaded\n");
        return 2;
    }

    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query || dtStatusFailed(query->init(navMesh, 4096)))
    {
        std::fprintf(stderr, "ERROR: dtNavMeshQuery init failed\n");
        return 2;
    }

    int rc = 0;
    if (!batch)
    {
        rc = RunQuery(navMesh, query, srcX, srcY, srcZ, dstX, dstY, dstZ);
    }
    else
    {
        // Batch mode: mesh is loaded ONCE above; each stdin line is one
        // src->dst query. Emit the same result block RunQuery always
        // printed, terminated by a sentinel "END" line so the caller can
        // split stdout into per-query chunks without needing exit codes
        // (the process only exits once, at EOF).
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.find_first_not_of(" \t\r\n") == std::string::npos)
                continue;
            std::istringstream iss(line);
            float sx, sy, sz, dx, dy, dz;
            if (!(iss >> sx >> sy >> sz >> dx >> dy >> dz))
            {
                std::printf("STATUS: ERR\n");
                std::printf("END\n");
                std::fflush(stdout);
                continue;
            }
            std::printf("  TC src = (%.3f, %.3f, %.3f)\n", sx, sy, sz);
            std::printf("  TC dst = (%.3f, %.3f, %.3f)\n", dx, dy, dz);
            RunQuery(navMesh, query, sx, sy, sz, dx, dy, dz);
            std::printf("END\n");
            std::fflush(stdout);
        }
    }

    dtFreeNavMeshQuery(query);
    dtFreeNavMesh(navMesh);
    return rc;
}
