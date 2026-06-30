#include "MapTileCache.h"

#include <cmath>

namespace world_editor::io
{

namespace
{
// Mirrors src/server/game/Grids/GridDefines.h.  Keep these in sync if
// TC ever changes the grid scale (it hasn't in many versions).
constexpr int   CENTER_GRID_ID = 32;
constexpr float SIZE_OF_GRIDS  = 533.3333f;
constexpr int   MAP_RESOLUTION = 128;

// Convert a world coordinate axis to the resolution-scaled global value
// used by GridMap::getHeightFromFloat:
//   r = MAP_RESOLUTION * (CENTER_GRID_ID - axis / SIZE_OF_GRIDS)
inline float toResolution(float axis)
{
    return float(MAP_RESOLUTION) * (float(CENTER_GRID_ID) - axis / SIZE_OF_GRIDS);
}
} // namespace

MapTileCache::MapTileCache(size_t maxTilesInRam) : m_budget(maxTilesInRam)
{
}

void MapTileCache::clear()
{
    m_lru.clear();
    m_index.clear();
}

LoadedMapTile* MapTileCache::find(uint32_t mapId, int gx, int gy)
{
    Key const key{ mapId, gx, gy };
    if (auto it = m_index.find(key); it != m_index.end())
    {
        // Move to front (most-recently-used).
        m_lru.splice(m_lru.begin(), m_lru, it->second);
        it->second = m_lru.begin();
        return m_lru.front().tile.get();
    }
    // Miss - try to load.
    Entry e;
    e.key   = key;
    e.tried = true;
    if (!m_mapsDir.empty())
        e.tile = loadTile(m_mapsDir, mapId, gx, gy);
    m_lru.push_front(std::move(e));
    m_index[key] = m_lru.begin();
    enforceBudget();
    return m_lru.front().tile.get();
}

void MapTileCache::enforceBudget()
{
    while (m_lru.size() > m_budget)
    {
        Entry& back = m_lru.back();
        m_index.erase(back.key);
        m_lru.pop_back();
    }
}

float MapTileCache::heightAt(uint32_t mapId, float worldX, float worldY)
{
    float const rx = toResolution(worldX);
    float const ry = toResolution(worldY);
    int   const gx_global = int(std::floor(rx));
    int   const gy_global = int(std::floor(ry));
    if (gx_global < 0 || gy_global < 0)
        return ADT_INVALID_HEIGHT;
    int const gx = gx_global / MAP_RESOLUTION;
    int const gy = gy_global / MAP_RESOLUTION;
    if (gx < 0 || gx >= 64 || gy < 0 || gy >= 64)
        return ADT_INVALID_HEIGHT;

    LoadedMapTile* tile = find(mapId, gx, gy);
    if (!tile)
        return ADT_INVALID_HEIGHT;

    // Convert to tile-local 128-grid coords (matches heightAtLocal's
    // contract: caller passes [0..128] floats).
    float const localX = rx - float(gx) * float(MAP_RESOLUTION);
    float const localY = ry - float(gy) * float(MAP_RESOLUTION);
    return heightAtLocal(*tile, localX, localY);
}

uint16_t MapTileCache::areaAt(uint32_t mapId, float worldX, float worldY)
{
    float const rx = toResolution(worldX);
    float const ry = toResolution(worldY);
    int const gx_global = int(std::floor(rx));
    int const gy_global = int(std::floor(ry));
    if (gx_global < 0 || gy_global < 0)
        return 0;
    int const gx = gx_global / MAP_RESOLUTION;
    int const gy = gy_global / MAP_RESOLUTION;
    if (gx < 0 || gx >= 64 || gy < 0 || gy >= 64)
        return 0;
    LoadedMapTile* tile = find(mapId, gx, gy);
    if (!tile)
        return 0;
    // Area grid is 16x16 cells per tile - 8x downscale from the 128-grid.
    int const localX_16 = (gx_global % MAP_RESOLUTION) / 8;
    int const localY_16 = (gy_global % MAP_RESOLUTION) / 8;
    return areaAtLocal(*tile, localX_16, localY_16);
}

} // namespace world_editor::io
