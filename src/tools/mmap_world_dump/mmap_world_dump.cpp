/*
 * mmap_world_dump - top-down navmesh rasterizer.
 *
 * Loads every .mmtile for a given mapId and rasterizes the walkable
 * polygons onto a 2D top-down PNG, plus emits a sidecar JSON that
 * carries the exact world-coord transform per-pixel. The road editor
 * (tools/road_editor/road_editor.html) auto-loads the pair: drop the
 * PNG and its .json next to each other; the editor reads the JSON,
 * skips manual calibration entirely, and every click yields exact TC
 * world coords by construction.
 *
 * Pure Detour + zlib. No TC core dependencies (mirrors mmap_probe).
 *
 * Usage:
 *   mmap_world_dump <mmaps_dir> <mapId> <out_basepath> [yards_per_pixel]
 *
 * Outputs:
 *   <out_basepath>.png   RGB top-down rasterization, area-coloured.
 *   <out_basepath>.json  Sidecar carrying world bounds + pixel transform.
 */

#include <DetourNavMesh.h>
#include <DetourAlloc.h>

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

// --- mmap file layout (mirrors src/common/mmaps_common/MMapDefines.h) ----

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
static_assert(sizeof(MmapTileHeader) == 20,    "MmapTileHeader size drift");

// Area ids (must match MMapDefines.h::NavArea).
static constexpr uint8_t NAV_AREA_GROUND       = 11;
static constexpr uint8_t NAV_AREA_GROUND_STEEP = 10;
static constexpr uint8_t NAV_AREA_WATER        = 9;
static constexpr uint8_t NAV_AREA_MAGMA_SLIME  = 8;
static constexpr uint8_t NAV_AREA_ROAD         = 7;

struct Rgb { uint8_t r, g, b; };

static constexpr Rgb COLOR_BG          = {  16,  16,  20 };
static constexpr Rgb COLOR_GROUND      = { 170, 170, 170 };
static constexpr Rgb COLOR_STEEP       = { 110, 110, 110 };
static constexpr Rgb COLOR_WATER       = {  48,  96, 168 };
static constexpr Rgb COLOR_MAGMA       = { 168,  48,  32 };
static constexpr Rgb COLOR_ROAD        = { 255, 170,   0 };
static constexpr Rgb COLOR_UNKNOWN     = {  90,  60, 110 };

static Rgb ColorForArea(uint8_t area)
{
    switch (area)
    {
        case NAV_AREA_GROUND:       return COLOR_GROUND;
        case NAV_AREA_GROUND_STEEP: return COLOR_STEEP;
        case NAV_AREA_WATER:        return COLOR_WATER;
        case NAV_AREA_MAGMA_SLIME:  return COLOR_MAGMA;
        case NAV_AREA_ROAD:         return COLOR_ROAD;
        default:                    return COLOR_UNKNOWN;
    }
}

// ----- file io -----

static std::string FormatMmapPath(std::string const& dir, uint32_t mapId)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u.mmap", mapId);
    return (std::filesystem::path(dir) / buf).string();
}

static bool LoadNavMeshParams(std::string const& dir, uint32_t mapId, dtNavMeshParams& params)
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

struct LoadedTile { int x, y; };

static bool AddTileFromFile(dtNavMesh* navMesh, std::string const& path, int tx, int ty)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    MmapTileHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1) { std::fclose(f); return false; }
    if (header.mmapMagic != MMAP_MAGIC || header.mmapVersion != MMAP_VERSION_EXPECTED)
    { std::fclose(f); return false; }

    void* data = dtAlloc(header.size, DT_ALLOC_PERM);
    if (!data) { std::fclose(f); return false; }
    if (std::fread(data, header.size, 1, f) != 1) { dtFree(data); std::fclose(f); return false; }
    std::fclose(f);

    dtTileRef tileRef = 0;
    dtStatus s = navMesh->addTile(static_cast<unsigned char*>(data), header.size, DT_TILE_FREE_DATA, 0, &tileRef);
    if (dtStatusFailed(s))
    {
        std::fprintf(stderr, "WARN: addTile failed for [%d,%d] status=0x%08x\n", tx, ty, s);
        dtFree(data);
        return false;
    }
    (void)tx; (void)ty;
    return true;
}

static int LoadAllTiles(dtNavMesh* navMesh, std::string const& dir, uint32_t mapId, std::vector<LoadedTile>& loaded)
{
    std::regex re(R"((\d{4})_(\d{2})_(\d{2})\.mmtile)");
    int n = 0;
    for (auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        std::smatch m;
        if (!std::regex_match(name, m, re)) continue;
        if (std::stoul(m[1].str()) != mapId) continue;
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

// ----- world bbox -----

struct WorldBBox { float minX, maxX, minY, maxY; };  // TC frame

static bool ComputeWorldBBox(dtNavMesh const* navMesh, WorldBBox& out)
{
    bool any = false;
    float minX =  1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    for (int ti = 0; ti < navMesh->getMaxTiles(); ++ti)
    {
        dtMeshTile const* tile = navMesh->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount <= 0)
            continue;
        // Detour stores AABBs in Detour frame (y, z, x).
        // bmin/bmax: index 0 = TC Y, 2 = TC X.
        float const* bmin = tile->header->bmin;
        float const* bmax = tile->header->bmax;
        // TC X span:
        minX = std::min(minX, bmin[2]);
        maxX = std::max(maxX, bmax[2]);
        // TC Y span:
        minY = std::min(minY, bmin[0]);
        maxY = std::max(maxY, bmax[0]);
        any = true;
    }
    if (!any) return false;
    out.minX = minX; out.maxX = maxX;
    out.minY = minY; out.maxY = maxY;
    return true;
}

// ----- rasterizer -----

struct Image
{
    int w = 0, h = 0;
    std::vector<uint8_t> rgb;  // size = w*h*3
    void Init(int width, int height, Rgb bg)
    {
        w = width; h = height;
        rgb.assign(size_t(w) * h * 3, 0);
        for (size_t i = 0; i < size_t(w) * h; ++i)
        {
            rgb[i*3 + 0] = bg.r;
            rgb[i*3 + 1] = bg.g;
            rgb[i*3 + 2] = bg.b;
        }
    }
    inline void Plot(int x, int y, Rgb c)
    {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        size_t i = (size_t(y) * w + x) * 3;
        rgb[i+0] = c.r; rgb[i+1] = c.g; rgb[i+2] = c.b;
    }
};

// Solid triangle rasterizer (top-left fill convention). Pixel (x, y) is
// covered if the triangle (in CCW order) wraps it.
static void RasterTriangle(Image& img, float ax, float ay, float bx, float by, float cx, float cy, Rgb color)
{
    int minX = int(std::floor(std::min({ax, bx, cx})));
    int maxX = int(std::ceil (std::max({ax, bx, cx})));
    int minY = int(std::floor(std::min({ay, by, cy})));
    int maxY = int(std::ceil (std::max({ay, by, cy})));
    if (maxX < 0 || maxY < 0 || minX >= img.w || minY >= img.h) return;
    minX = std::max(0, minX); minY = std::max(0, minY);
    maxX = std::min(img.w - 1, maxX); maxY = std::min(img.h - 1, maxY);

    auto edge = [](float x0, float y0, float x1, float y1, float px, float py) {
        return (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
    };

    float area = edge(ax, ay, bx, by, cx, cy);
    if (std::fabs(area) < 1e-6f) return;
    float sign = area > 0 ? 1.0f : -1.0f;

    for (int y = minY; y <= maxY; ++y)
    {
        float fy = y + 0.5f;
        for (int x = minX; x <= maxX; ++x)
        {
            float fx = x + 0.5f;
            float w0 = edge(bx, by, cx, cy, fx, fy) * sign;
            float w1 = edge(cx, cy, ax, ay, fx, fy) * sign;
            float w2 = edge(ax, ay, bx, by, fx, fy) * sign;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                img.Plot(x, y, color);
        }
    }
}

// Project Detour vertex (y, z, x) onto pixel space using the world bbox.
// Image x grows EAST  (decreasing TC Y);
// Image y grows SOUTH (decreasing TC X) — i.e. north-up.
static inline void ProjectVert(float const* v, WorldBBox const& bb, float yardsPerPixel, float& px, float& py)
{
    float worldY = v[0]; // TC Y
    float worldX = v[2]; // TC X
    px = (bb.maxY - worldY) / yardsPerPixel;
    py = (bb.maxX - worldX) / yardsPerPixel;
}

static uint64_t RasterizeNavMesh(dtNavMesh const* navMesh, WorldBBox const& bb, float yardsPerPixel, Image& img)
{
    uint64_t triCount = 0;
    for (int ti = 0; ti < navMesh->getMaxTiles(); ++ti)
    {
        dtMeshTile const* tile = navMesh->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount <= 0)
            continue;

        for (int p = 0; p < tile->header->polyCount; ++p)
        {
            dtPoly const& poly = tile->polys[p];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;
            int nv = poly.vertCount;
            if (nv < 3) continue;
            Rgb color = ColorForArea(poly.getArea());

            // Project verts to pixel space.
            float px[DT_VERTS_PER_POLYGON], py[DT_VERTS_PER_POLYGON];
            for (int i = 0; i < nv; ++i)
            {
                float const* v = &tile->verts[poly.verts[i] * 3];
                ProjectVert(v, bb, yardsPerPixel, px[i], py[i]);
            }

            // Triangle fan.
            for (int i = 1; i + 1 < nv; ++i)
            {
                RasterTriangle(img,
                    px[0],     py[0],
                    px[i],     py[i],
                    px[i + 1], py[i + 1],
                    color);
                ++triCount;
            }
        }
    }
    return triCount;
}

// ----- PNG writer -----

static void WriteU32BE(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(uint8_t(v >> 24));
    out.push_back(uint8_t(v >> 16));
    out.push_back(uint8_t(v >> 8));
    out.push_back(uint8_t(v));
}
static void WriteChunk(std::vector<uint8_t>& out, char const type[5], std::vector<uint8_t> const& data)
{
    WriteU32BE(out, uint32_t(data.size()));
    size_t crc_start = out.size();
    out.insert(out.end(), reinterpret_cast<uint8_t const*>(type), reinterpret_cast<uint8_t const*>(type) + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t crc = uint32_t(::crc32(0L, out.data() + crc_start, uLong(out.size() - crc_start)));
    WriteU32BE(out, crc);
}

static bool WritePng(std::string const& path, Image const& img)
{
    // Build raw scanlines: each row prefixed with filter byte 0 (None).
    std::vector<uint8_t> raw;
    raw.reserve(size_t(img.w) * img.h * 3 + img.h);
    for (int y = 0; y < img.h; ++y)
    {
        raw.push_back(0);
        size_t off = size_t(y) * img.w * 3;
        raw.insert(raw.end(), img.rgb.begin() + off, img.rgb.begin() + off + size_t(img.w) * 3);
    }

    // Compress with zlib's compress2 (level 6).
    uLongf cap = compressBound(uLong(raw.size()));
    std::vector<uint8_t> comp(cap);
    if (compress2(comp.data(), &cap, raw.data(), uLong(raw.size()), 6) != Z_OK)
    {
        std::fprintf(stderr, "ERROR: zlib compress2 failed\n");
        return false;
    }
    comp.resize(cap);

    std::vector<uint8_t> out;
    out.reserve(comp.size() + 256);
    // Signature.
    static const uint8_t SIG[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    out.insert(out.end(), SIG, SIG + 8);

    // IHDR.
    std::vector<uint8_t> ihdr;
    WriteU32BE(ihdr, uint32_t(img.w));
    WriteU32BE(ihdr, uint32_t(img.h));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // color type = RGB
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    WriteChunk(out, "IHDR", ihdr);
    // IDAT.
    WriteChunk(out, "IDAT", comp);
    // IEND.
    WriteChunk(out, "IEND", std::vector<uint8_t>{});

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "ERROR: cannot write %s\n", path.c_str()); return false; }
    bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

// ----- sidecar JSON -----
// The transform is intentionally minimal: image pixel (px, py) maps to
// world (wx, wy) via
//   wx = world_origin_x + px * world_x_per_pixel
//   wy = world_origin_y + py * world_y_per_pixel
// In our projection world_x_per_pixel and world_y_per_pixel are both
// NEGATIVE (north-up image: row 0 is the max TC X, col 0 is the max TC Y).

static bool WriteSidecar(std::string const& path,
                         uint32_t mapId,
                         Image const& img,
                         WorldBBox const& bb,
                         float yardsPerPixel,
                         uint64_t triCount,
                         int tileCount)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "ERROR: cannot write %s\n", path.c_str()); return false; }
    std::fprintf(f,
        "{\n"
        "  \"schema\": \"mmap_world_dump_v1\",\n"
        "  \"map_id\": %u,\n"
        "  \"image_width\": %d,\n"
        "  \"image_height\": %d,\n"
        "  \"yards_per_pixel\": %.6f,\n"
        "  \"world_x_min\": %.3f,\n"
        "  \"world_x_max\": %.3f,\n"
        "  \"world_y_min\": %.3f,\n"
        "  \"world_y_max\": %.3f,\n"
        "  \"world_origin_x\": %.6f,\n"
        "  \"world_origin_y\": %.6f,\n"
        "  \"world_x_per_pixel\": %.6f,\n"
        "  \"world_y_per_pixel\": %.6f,\n"
        "  \"tile_count\": %d,\n"
        "  \"tri_count\": %llu,\n"
        "  \"area_colors\": {\n"
        "    \"ground\":       \"#%02x%02x%02x\",\n"
        "    \"ground_steep\": \"#%02x%02x%02x\",\n"
        "    \"water\":        \"#%02x%02x%02x\",\n"
        "    \"magma_slime\":  \"#%02x%02x%02x\",\n"
        "    \"road\":         \"#%02x%02x%02x\"\n"
        "  }\n"
        "}\n",
        mapId, img.w, img.h, yardsPerPixel,
        bb.minX, bb.maxX, bb.minY, bb.maxY,
        /*origin_x at pixel (0,0)*/ bb.maxY, /*origin_y at pixel (0,0)*/ bb.maxX,
        /*world_x_per_pixel*/ -yardsPerPixel,
        /*world_y_per_pixel*/ -yardsPerPixel,
        tileCount, (unsigned long long)triCount,
        COLOR_GROUND.r, COLOR_GROUND.g, COLOR_GROUND.b,
        COLOR_STEEP.r,  COLOR_STEEP.g,  COLOR_STEEP.b,
        COLOR_WATER.r,  COLOR_WATER.g,  COLOR_WATER.b,
        COLOR_MAGMA.r,  COLOR_MAGMA.g,  COLOR_MAGMA.b,
        COLOR_ROAD.r,   COLOR_ROAD.g,   COLOR_ROAD.b);
    // NOTE on the origin/per-pixel fields:
    // Image col 0 corresponds to the highest TC Y (we render with TC Y
    // decreasing as image x increases). So at pixel (0, 0):
    //   wx = bb.maxY = world_origin_x (which is the TC X axis name in
    //                                   our struct? — careful here)
    // Convention used in this file: the operator works in TC frame and
    // the JSON encodes how to recover (TC_X, TC_Y) from (px, py).
    // We name JSON fields `world_x` = TC X (north-south), `world_y` = TC Y
    // (east-west) so the road editor matches the same naming as the
    // worldserver / GM commands. Mapping:
    //   row (py = 0) → TC X = bb.maxX (north)
    //   col (px = 0) → TC Y = bb.maxY (west)
    // Hence world_origin_x in JSON = bb.maxX, world_origin_y in JSON = bb.maxY.
    std::fclose(f);
    return true;
}

// ----- main -----

int main(int argc, char** argv)
{
    if (argc < 4 || argc > 5)
    {
        std::fprintf(stderr,
            "Usage: %s <mmaps_dir> <mapId> <out_basepath> [yards_per_pixel=2.0]\n"
            "       Outputs <out_basepath>.png and <out_basepath>.json\n",
            argv[0]);
        return 2;
    }

    std::string dir = argv[1];
    uint32_t mapId = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
    std::string basepath = argv[3];
    float yardsPerPixel = (argc == 5) ? std::strtof(argv[4], nullptr) : 2.0f;
    if (yardsPerPixel <= 0.05f) { std::fprintf(stderr, "ERROR: yards_per_pixel too small\n"); return 2; }

    std::printf("mmap_world_dump: dir=%s mapId=%u out=%s ypp=%.3f\n",
        dir.c_str(), mapId, basepath.c_str(), yardsPerPixel);

    dtNavMeshParams params{};
    if (!LoadNavMeshParams(dir, mapId, params))
        return 2;

    dtNavMesh* navMesh = dtAllocNavMesh();
    if (!navMesh || dtStatusFailed(navMesh->init(&params)))
    { std::fprintf(stderr, "ERROR: dtNavMesh init failed\n"); return 2; }

    std::vector<LoadedTile> loaded;
    int nTiles = LoadAllTiles(navMesh, dir, mapId, loaded);
    std::printf("  loaded %d tile(s)\n", nTiles);
    if (nTiles == 0) { std::fprintf(stderr, "ERROR: no tiles loaded\n"); dtFreeNavMesh(navMesh); return 2; }

    WorldBBox bb{};
    if (!ComputeWorldBBox(navMesh, bb))
    { std::fprintf(stderr, "ERROR: empty bbox\n"); dtFreeNavMesh(navMesh); return 2; }
    std::printf("  TC bbox: X=[%.1f .. %.1f] (%.1f y)  Y=[%.1f .. %.1f] (%.1f y)\n",
        bb.minX, bb.maxX, bb.maxX - bb.minX,
        bb.minY, bb.maxY, bb.maxY - bb.minY);

    // Image size: each axis in (yards / ypp) pixels.
    // Image width corresponds to TC Y axis (east-west). Image height to TC X (north-south).
    int imgW = std::max(1, int(std::ceil((bb.maxY - bb.minY) / yardsPerPixel)));
    int imgH = std::max(1, int(std::ceil((bb.maxX - bb.minX) / yardsPerPixel)));

    constexpr int MAX_DIM = 16384;
    if (imgW > MAX_DIM || imgH > MAX_DIM)
    {
        float scale = float(std::max(imgW, imgH)) / float(MAX_DIM);
        yardsPerPixel *= scale;
        imgW = std::max(1, int(std::ceil((bb.maxY - bb.minY) / yardsPerPixel)));
        imgH = std::max(1, int(std::ceil((bb.maxX - bb.minX) / yardsPerPixel)));
        std::printf("  image cap %d hit; rescaled ypp to %.3f -> %dx%d\n", MAX_DIM, yardsPerPixel, imgW, imgH);
    }
    std::printf("  image: %d x %d (%.1f MB raw RGB)\n",
        imgW, imgH, (double)imgW * imgH * 3 / 1048576.0);

    Image img;
    img.Init(imgW, imgH, COLOR_BG);

    uint64_t triCount = RasterizeNavMesh(navMesh, bb, yardsPerPixel, img);
    std::printf("  rasterized %llu triangle(s)\n", (unsigned long long)triCount);

    std::string pngPath  = basepath + ".png";
    std::string jsonPath = basepath + ".json";
    if (!WritePng(pngPath, img))  { dtFreeNavMesh(navMesh); return 2; }
    if (!WriteSidecar(jsonPath, mapId, img, bb, yardsPerPixel, triCount, nTiles)) { dtFreeNavMesh(navMesh); return 2; }
    std::printf("  wrote %s\n  wrote %s\n", pngPath.c_str(), jsonPath.c_str());

    dtFreeNavMesh(navMesh);
    return 0;
}
