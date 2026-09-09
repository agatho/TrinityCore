/*
 * MapTileCache - on-demand .map tile loader with bounded LRU eviction.
 *
 * Each .map tile is ~1 MB (heights + area + liquid + holes). A full
 * continent has 1000+ tiles so loading them all eagerly would burn
 * 1 GB+ of memory. The editor only needs height/area data for tiles
 * the operator interacts with (current view + a small ring around it),
 * so we load on first reference and keep a bounded LRU.
 *
 * Public API:
 *   - setMapsDir(): root directory holding "NNNN_XX_YY.map" files.
 *   - heightAt(mapId, worldX, worldY): bilinear-interp terrain height
 *     or ADT_INVALID_HEIGHT when the tile is missing / falls in a hole.
 *   - areaAt(mapId, worldX, worldY): MCNK area id under the point.
 *
 * Coordinate convention mirrors GridMap::getHeightFromFloat exactly
 * (src/server/game/Maps/GridMap.cpp:309-392):
 *   rx = MAP_RESOLUTION * (CENTER_GRID_ID - worldX / SIZE_OF_GRIDS)
 *   gx = floor(rx) / MAP_RESOLUTION
 *   lx = floor(rx) % MAP_RESOLUTION
 *   dx = rx - floor(rx)
 *
 * Where (gx, gy) is the file name pair (mapId_gx_gy.map) and (lx, ly)
 * + (dx, dy) feeds io::heightAtLocal/areaAtLocal.
 *
 * Thread-safety: NOT thread-safe. Lives on the UI thread only.
 */

#pragma once

#include "MapReader.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

namespace world_editor::io
{

class MapTileCache
{
public:
    explicit MapTileCache(size_t maxTilesInRam = 64);

    void setMapsDir(std::filesystem::path const& dir) { m_mapsDir = dir; }
    [[nodiscard]] std::filesystem::path const& mapsDir() const noexcept { return m_mapsDir; }

    // Returns ADT_INVALID_HEIGHT when the tile is missing or the
    // lookup falls in a hole.
    [[nodiscard]] float    heightAt(uint32_t mapId, float worldX, float worldY);
    [[nodiscard]] uint16_t areaAt  (uint32_t mapId, float worldX, float worldY);

    // Diagnostics.
    [[nodiscard]] size_t cachedTileCount() const noexcept { return m_lru.size(); }

    void clear();

private:
    struct Key
    {
        uint32_t mapId;
        int      gx;
        int      gy;
        bool operator==(Key const& o) const noexcept
        { return mapId == o.mapId && gx == o.gx && gy == o.gy; }
    };
    struct KeyHash
    {
        size_t operator()(Key const& k) const noexcept
        {
            return static_cast<size_t>(k.mapId) * 0x9E3779B97F4A7C15ull
                 ^ (static_cast<size_t>(k.gx) << 16)
                 ^ static_cast<size_t>(k.gy);
        }
    };

    struct Entry
    {
        Key                            key;
        std::unique_ptr<LoadedMapTile> tile;
        bool                           tried = false; // true if we already attempted load (positive or negative).
    };
    using Iter = std::list<Entry>::iterator;

    // Look up tile, loading if necessary.  Returns nullptr if the file
    // is missing.  Touches LRU.
    LoadedMapTile* find(uint32_t mapId, int gx, int gy);

    // Drop least-recently-used entries until we're within budget.
    void enforceBudget();

    std::filesystem::path                                m_mapsDir;
    size_t                                               m_budget;
    std::list<Entry>                                     m_lru;   // front = MRU.
    std::unordered_map<Key, Iter, KeyHash>               m_index;
};

} // namespace world_editor::io
