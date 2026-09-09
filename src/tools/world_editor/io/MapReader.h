/*
 * MapReader - standalone reader for TrinityCore .map tile files.
 *
 * One .map file = one ADT tile.  File layout mirrors
 * src/common/Collision/Maps/MapDefines.h and the runtime loader in
 * src/server/game/Maps/GridMap.cpp (read 2026-05-22).  We keep a local
 * copy of the binary structs so the editor has zero TC server runtime
 * dependency - same standalone pattern as MMapReader.
 *
 * What's exposed:
 *   - LoadedMapTile  : RAII container for one tile's height/area/liquid/holes buffers.
 *   - loadTile()     : load a single "<mapId>_<gx>_<gy>.map" off disk.
 *   - heightAtLocal(): interpolated Z for a point INSIDE the tile.
 *   - areaAtLocal()  : MCNK-level (16x16) area id within the tile.
 *
 * World <-> tile coordinate conversion stays in render/Coords.h.  This
 * file owns only per-tile arithmetic so we don't fan the convention
 * out across the codebase.
 *
 * Heights can be stored as float, uint16 or uint8 depending on the
 * map_heightHeaderFlags carried in the file - the reader expands all
 * three to a uniform float buffer at load time so callers don't have
 * to branch.
 */

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace world_editor::io
{

// Grid + tile constants mirror src/server/game/Grids/GridDefines.h.
inline constexpr int   ADT_MAP_RESOLUTION      = 128;        // V8 sub-grid resolution per tile.
inline constexpr int   ADT_HEIGHT_GRID_V8      = 128;        // 128 * 128 inner cells.
inline constexpr int   ADT_HEIGHT_GRID_V9      = 129;        // 129 * 129 corner samples.
inline constexpr int   ADT_AREA_GRID           = 16;         // 16 * 16 MCNK area cells.
inline constexpr int   ADT_HOLES_GRID          = 16;         // 16 * 16 cell holes (8 rows of 8 bits each).
inline constexpr float ADT_INVALID_HEIGHT      = -100000.0f; // matches INVALID_HEIGHT in GridDefines.h

struct LoadedMapTile
{
    uint32_t mapId = 0;
    int      gx    = 0; // tile X index along world X (0..63)
    int      gy    = 0; // tile Y index along world Y (0..63)
    uint32_t versionMagic = 0;
    uint32_t buildMagic   = 0;

    // Height: always uniform float, expanded from the on-disk format.
    // Empty when the tile carries no height data (NoHeight flag set);
    // in that case gridHeightFlat holds the constant level for the tile.
    std::vector<float> heightV9; // size 129 * 129 (row-major, row = gridX, col = gridY)
    std::vector<float> heightV8; // size 128 * 128
    float gridHeightFlat = ADT_INVALID_HEIGHT;
    bool  hasHeight      = false;

    // Area: 16*16 cells in tile, default _gridArea when NoArea flag is set.
    std::vector<uint16_t> areaGrid; // empty -> use gridAreaFlat
    uint16_t gridAreaFlat = 0;

    // Liquid (optional). When liquidWidth / liquidHeight are zero, no
    // per-cell liquid surface is encoded - only the global liquidLevel
    // / liquidFlags apply.
    std::vector<uint16_t> liquidEntry;          // 16*16 if hasLiquidType else empty
    std::vector<uint8_t>  liquidTypeFlags;      // 16*16 if hasLiquidType else empty
    std::vector<float>    liquidMap;            // liquidWidth*liquidHeight floats if hasLiquidHeight else empty
    uint16_t liquidGlobalEntry = 0;
    uint8_t  liquidGlobalFlags = 0;
    uint8_t  liquidOffsetX     = 0;
    uint8_t  liquidOffsetY     = 0;
    uint8_t  liquidWidth       = 0;
    uint8_t  liquidHeight      = 0;
    float    liquidLevelFlat   = ADT_INVALID_HEIGHT;

    // Holes: 16*16 cells * 8 bytes each (8 rows of 8 bits per cell).
    // Empty when no holes block exists.
    std::vector<uint8_t> holes;
};

// Compose the on-disk filename pattern used by mmaps_generator and
// the worldserver: "{:04}_{:02}_{:02}.map" (mapId, gx, gy).
[[nodiscard]] std::string mapTileFilename(uint32_t mapId, int gx, int gy);

// Load a single tile.  Returns nullptr if the file is missing or has
// the wrong magic / version.  Per-section read failures roll back the
// whole tile.
[[nodiscard]] std::unique_ptr<LoadedMapTile>
loadTile(std::filesystem::path const& mapsDir, uint32_t mapId, int gx, int gy);

// Interpolated terrain height at tile-local fractional coords
//   localX, localY in [0..ADT_HEIGHT_GRID_V8]  (i.e. 128-cell space).
// Mirrors GridMap::getHeightFromFloat in src/server/game/Maps/GridMap.cpp.
// Returns ADT_INVALID_HEIGHT when the lookup falls in a hole.
[[nodiscard]] float heightAtLocal(LoadedMapTile const& tile, float localX, float localY);

// MCNK-level (16x16) area id within the tile.
//   localX, localY in [0..ADT_AREA_GRID] (i.e. 16-cell space).
[[nodiscard]] uint16_t areaAtLocal(LoadedMapTile const& tile, int localX, int localY);

// Is the V8/V9 cell at (row, col) marked as a hole?  row/col in [0..127].
[[nodiscard]] bool isHole(LoadedMapTile const& tile, int row, int col);

} // namespace world_editor::io
