#include "MapReader.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>

// Binary layout mirrors src/common/Collision/Maps/MapDefines.h and
// src/common/Collision/Maps/MapDefines.cpp on the playerbot-dev branch
// at 2026-05-22.  Drift detected via:
//   - the four magic constants below
//   - the version constant
//   - the per-struct sizeof static_asserts.

namespace
{

using u_map_magic = std::array<char, 4>;

constexpr u_map_magic MAGIC_MAPS    = { { 'M', 'A', 'P', 'S' } };
constexpr u_map_magic MAGIC_AREA    = { { 'A', 'R', 'E', 'A' } };
constexpr u_map_magic MAGIC_MHGT    = { { 'M', 'H', 'G', 'T' } };
constexpr u_map_magic MAGIC_MLIQ    = { { 'M', 'L', 'I', 'Q' } };
constexpr uint32_t    MAP_VERSION   = 10;

// Heightmap flags - mirrors enum class map_heightHeaderFlags.
constexpr uint32_t HFLAG_NONE            = 0x0000;
constexpr uint32_t HFLAG_NO_HEIGHT       = 0x0001;
constexpr uint32_t HFLAG_HEIGHT_AS_INT16 = 0x0002;
constexpr uint32_t HFLAG_HEIGHT_AS_INT8  = 0x0004;
constexpr uint32_t HFLAG_FLIGHT_BOUNDS   = 0x0008;

// Area-header flags.
constexpr uint16_t AFLAG_NO_AREA         = 0x0001;

// Liquid-header flags.
constexpr uint8_t  LFLAG_NO_TYPE         = 0x0001;
constexpr uint8_t  LFLAG_NO_HEIGHT       = 0x0002;

#pragma pack(push, 1)
struct map_fileheader
{
    u_map_magic mapMagic;
    uint32_t versionMagic;
    uint32_t buildMagic;
    uint32_t areaMapOffset;
    uint32_t areaMapSize;
    uint32_t heightMapOffset;
    uint32_t heightMapSize;
    uint32_t liquidMapOffset;
    uint32_t liquidMapSize;
    uint32_t holesOffset;
    uint32_t holesSize;
};

struct map_areaHeader
{
    u_map_magic areaMagic;
    uint16_t    flags;
    uint16_t    gridArea;
};

struct map_heightHeader
{
    u_map_magic heightMagic;
    uint32_t    flags;
    float       gridHeight;
    float       gridMaxHeight;
};

struct map_liquidHeader
{
    u_map_magic liquidMagic;
    uint8_t     flags;
    uint8_t     liquidFlags;
    uint16_t    liquidType;
    uint8_t     offsetX;
    uint8_t     offsetY;
    uint8_t     width;
    uint8_t     height;
    float       liquidLevel;
};
#pragma pack(pop)

static_assert(sizeof(map_fileheader)   == 44, "map_fileheader size drift");
static_assert(sizeof(map_areaHeader)   == 8,  "map_areaHeader size drift");
static_assert(sizeof(map_heightHeader) == 16, "map_heightHeader size drift");
static_assert(sizeof(map_liquidHeader) == 16, "map_liquidHeader size drift");

bool magicEq(u_map_magic const& a, u_map_magic const& b)
{
    return std::memcmp(a.data(), b.data(), 4) == 0;
}

template <typename T>
bool readVector(std::FILE* in, std::vector<T>& dst, size_t count)
{
    dst.resize(count);
    if (count == 0)
        return true;
    return std::fread(dst.data(), sizeof(T), count, in) == count;
}

bool readAreaSection(std::FILE* in, uint32_t offset, world_editor::io::LoadedMapTile& tile)
{
    if (std::fseek(in, static_cast<long>(offset), SEEK_SET) != 0)
        return false;

    map_areaHeader header{};
    if (std::fread(&header, sizeof(header), 1, in) != 1)
        return false;
    if (!magicEq(header.areaMagic, MAGIC_AREA))
        return false;

    tile.gridAreaFlat = header.gridArea;
    if (!(header.flags & AFLAG_NO_AREA))
    {
        if (!readVector(in, tile.areaGrid,
                        static_cast<size_t>(world_editor::io::ADT_AREA_GRID) * world_editor::io::ADT_AREA_GRID))
            return false;
    }
    return true;
}

bool readHeightSection(std::FILE* in, uint32_t offset, world_editor::io::LoadedMapTile& tile)
{
    if (std::fseek(in, static_cast<long>(offset), SEEK_SET) != 0)
        return false;

    map_heightHeader header{};
    if (std::fread(&header, sizeof(header), 1, in) != 1)
        return false;
    if (!magicEq(header.heightMagic, MAGIC_MHGT))
        return false;

    tile.gridHeightFlat = header.gridHeight;
    tile.hasHeight      = false;

    if (header.flags & HFLAG_NO_HEIGHT)
        return true; // flat tile; gridHeightFlat is all we need.

    constexpr size_t V9 = static_cast<size_t>(world_editor::io::ADT_HEIGHT_GRID_V9)
                          * world_editor::io::ADT_HEIGHT_GRID_V9;
    constexpr size_t V8 = static_cast<size_t>(world_editor::io::ADT_HEIGHT_GRID_V8)
                          * world_editor::io::ADT_HEIGHT_GRID_V8;

    if (header.flags & HFLAG_HEIGHT_AS_INT16)
    {
        std::vector<uint16_t> rawV9, rawV8;
        if (!readVector(in, rawV9, V9) || !readVector(in, rawV8, V8))
            return false;
        float const mult = (header.gridMaxHeight - header.gridHeight) / 65535.0f;
        tile.heightV9.resize(V9);
        tile.heightV8.resize(V8);
        for (size_t i = 0; i < V9; ++i) tile.heightV9[i] = header.gridHeight + rawV9[i] * mult;
        for (size_t i = 0; i < V8; ++i) tile.heightV8[i] = header.gridHeight + rawV8[i] * mult;
    }
    else if (header.flags & HFLAG_HEIGHT_AS_INT8)
    {
        std::vector<uint8_t> rawV9, rawV8;
        if (!readVector(in, rawV9, V9) || !readVector(in, rawV8, V8))
            return false;
        float const mult = (header.gridMaxHeight - header.gridHeight) / 255.0f;
        tile.heightV9.resize(V9);
        tile.heightV8.resize(V8);
        for (size_t i = 0; i < V9; ++i) tile.heightV9[i] = header.gridHeight + rawV9[i] * mult;
        for (size_t i = 0; i < V8; ++i) tile.heightV8[i] = header.gridHeight + rawV8[i] * mult;
    }
    else
    {
        if (!readVector(in, tile.heightV9, V9) || !readVector(in, tile.heightV8, V8))
            return false;
    }

    tile.hasHeight = true;

    // Optional 9 + 9 int16 flight bounds.  We skip over them - the
    // editor doesn't need to read flight planes today; if we surface
    // them later they can be added without rewriting the loader.
    if (header.flags & HFLAG_FLIGHT_BOUNDS)
    {
        std::array<int16_t, 9> maxH{}, minH{};
        if (std::fread(maxH.data(), sizeof(int16_t), maxH.size(), in) != maxH.size() ||
            std::fread(minH.data(), sizeof(int16_t), minH.size(), in) != minH.size())
            return false;
    }
    return true;
}

bool readLiquidSection(std::FILE* in, uint32_t offset, world_editor::io::LoadedMapTile& tile)
{
    if (std::fseek(in, static_cast<long>(offset), SEEK_SET) != 0)
        return false;

    map_liquidHeader header{};
    if (std::fread(&header, sizeof(header), 1, in) != 1)
        return false;
    if (!magicEq(header.liquidMagic, MAGIC_MLIQ))
        return false;

    tile.liquidGlobalEntry = header.liquidType;
    tile.liquidGlobalFlags = header.liquidFlags;
    tile.liquidOffsetX     = header.offsetX;
    tile.liquidOffsetY     = header.offsetY;
    tile.liquidWidth       = header.width;
    tile.liquidHeight      = header.height;
    tile.liquidLevelFlat   = header.liquidLevel;

    if (!(header.flags & LFLAG_NO_TYPE))
    {
        size_t const cells = static_cast<size_t>(world_editor::io::ADT_AREA_GRID)
                             * world_editor::io::ADT_AREA_GRID;
        if (!readVector(in, tile.liquidEntry, cells))
            return false;
        if (!readVector(in, tile.liquidTypeFlags, cells))
            return false;
    }
    if (!(header.flags & LFLAG_NO_HEIGHT) && header.width > 0 && header.height > 0)
    {
        size_t const surfaceCount = static_cast<size_t>(header.width) * header.height;
        if (!readVector(in, tile.liquidMap, surfaceCount))
            return false;
    }
    return true;
}

bool readHolesSection(std::FILE* in, uint32_t offset, world_editor::io::LoadedMapTile& tile)
{
    if (std::fseek(in, static_cast<long>(offset), SEEK_SET) != 0)
        return false;
    constexpr size_t HOLES_BYTES = static_cast<size_t>(world_editor::io::ADT_HOLES_GRID)
                                   * world_editor::io::ADT_HOLES_GRID * 8;
    return readVector(in, tile.holes, HOLES_BYTES);
}

} // anonymous namespace

namespace world_editor::io
{

std::string mapTileFilename(uint32_t mapId, int gx, int gy)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u_%02d_%02d.map", mapId, gx, gy);
    return std::string(buf);
}

std::unique_ptr<LoadedMapTile>
loadTile(std::filesystem::path const& mapsDir, uint32_t mapId, int gx, int gy)
{
    std::filesystem::path const path = mapsDir / mapTileFilename(mapId, gx, gy);
    std::FILE* in = std::fopen(path.string().c_str(), "rb");
    if (!in)
        return nullptr;

    map_fileheader header{};
    if (std::fread(&header, sizeof(header), 1, in) != 1)
    {
        std::fclose(in);
        return nullptr;
    }
    if (!magicEq(header.mapMagic, MAGIC_MAPS) || header.versionMagic != MAP_VERSION)
    {
        std::fclose(in);
        return nullptr;
    }

    auto tile = std::make_unique<LoadedMapTile>();
    tile->mapId        = mapId;
    tile->gx           = gx;
    tile->gy           = gy;
    tile->versionMagic = header.versionMagic;
    tile->buildMagic   = header.buildMagic;

    if (header.areaMapOffset && !readAreaSection(in, header.areaMapOffset, *tile))
    {
        std::fclose(in);
        return nullptr;
    }
    if (header.heightMapOffset && !readHeightSection(in, header.heightMapOffset, *tile))
    {
        std::fclose(in);
        return nullptr;
    }
    if (header.liquidMapOffset && !readLiquidSection(in, header.liquidMapOffset, *tile))
    {
        std::fclose(in);
        return nullptr;
    }
    if (header.holesSize && !readHolesSection(in, header.holesOffset, *tile))
    {
        std::fclose(in);
        return nullptr;
    }

    std::fclose(in);
    return tile;
}

bool isHole(LoadedMapTile const& tile, int row, int col)
{
    if (tile.holes.empty())
        return false;
    if (row < 0 || col < 0 || row >= ADT_MAP_RESOLUTION || col >= ADT_MAP_RESOLUTION)
        return false;
    int const cellRow = row / 8;
    int const cellCol = col / 8;
    int const holeRow = row % 8;
    int const holeCol = col % 8;
    uint8_t const byte = tile.holes[static_cast<size_t>(cellRow * 16 * 8 + cellCol * 8 + holeRow)];
    return (byte & (1u << holeCol)) != 0;
}

float heightAtLocal(LoadedMapTile const& tile, float localX, float localY)
{
    if (!tile.hasHeight)
        return tile.gridHeightFlat;

    // Clamp into [0, 128). The .map files don't carry samples for
    // the 128th column / row - those live on the neighbouring tile.
    if (localX < 0.0f) localX = 0.0f;
    if (localY < 0.0f) localY = 0.0f;
    if (localX >= static_cast<float>(ADT_HEIGHT_GRID_V8))
        localX = static_cast<float>(ADT_HEIGHT_GRID_V8) - 0.0001f;
    if (localY >= static_cast<float>(ADT_HEIGHT_GRID_V8))
        localY = static_cast<float>(ADT_HEIGHT_GRID_V8) - 0.0001f;

    int const xi = static_cast<int>(localX);
    int const yi = static_cast<int>(localY);
    float const dx = localX - xi;
    float const dy = localY - yi;

    if (isHole(tile, xi, yi))
        return ADT_INVALID_HEIGHT;

    // The same triangle dispatcher GridMap::getHeightFromFloat uses.
    // V9 indexes: row*129 + col;  V8 indexes: row*128 + col.
    auto const V9 = [&](int r, int c) -> float {
        return tile.heightV9[static_cast<size_t>(r) * ADT_HEIGHT_GRID_V9 + c];
    };
    auto const V8 = [&](int r, int c) -> float {
        return tile.heightV8[static_cast<size_t>(r) * ADT_HEIGHT_GRID_V8 + c];
    };

    float a = 0.0f, b = 0.0f, c = 0.0f;
    if (dx + dy < 1.0f)
    {
        if (dx > dy)
        {
            // Triangle 1: (h1, h2, h5)
            float const h1 = V9(xi,     yi);
            float const h2 = V9(xi + 1, yi);
            float const h5 = 2.0f * V8(xi, yi);
            a = h2 - h1;
            b = h5 - h1 - h2;
            c = h1;
        }
        else
        {
            // Triangle 2: (h1, h3, h5)
            float const h1 = V9(xi, yi);
            float const h3 = V9(xi, yi + 1);
            float const h5 = 2.0f * V8(xi, yi);
            a = h5 - h1 - h3;
            b = h3 - h1;
            c = h1;
        }
    }
    else
    {
        if (dx > dy)
        {
            // Triangle 3: (h2, h4, h5)
            float const h2 = V9(xi + 1, yi);
            float const h4 = V9(xi + 1, yi + 1);
            float const h5 = 2.0f * V8(xi, yi);
            a = h2 + h4 - h5;
            b = h4 - h2;
            c = h5 - h4;
        }
        else
        {
            // Triangle 4: (h3, h4, h5)
            float const h3 = V9(xi,     yi + 1);
            float const h4 = V9(xi + 1, yi + 1);
            float const h5 = 2.0f * V8(xi, yi);
            a = h4 - h3;
            b = h3 + h4 - h5;
            c = h5 - h4;
        }
    }
    return a * dx + b * dy + c;
}

uint16_t areaAtLocal(LoadedMapTile const& tile, int localX, int localY)
{
    if (tile.areaGrid.empty())
        return tile.gridAreaFlat;
    if (localX < 0 || localY < 0 || localX >= ADT_AREA_GRID || localY >= ADT_AREA_GRID)
        return tile.gridAreaFlat;
    return tile.areaGrid[static_cast<size_t>(localX) * ADT_AREA_GRID + localY];
}

} // namespace world_editor::io
