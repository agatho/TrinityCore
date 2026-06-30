/*
 * VmapReader - standalone loader for TrinityCore vmap collision data.
 *
 * Mirrors the binary layouts in
 *   src/common/Collision/Models/WorldModel.{h,cpp}          (.vmo, "WMOD"/"GMOD"/"VERT"/"TRIM"/"MBIH"/"GBIH"/"LIQU")
 *   src/common/Collision/Models/ModelInstance.{h,cpp}       (ModelSpawn::readFromFile)
 *   src/common/Collision/Maps/MapTree.{h,cpp}               (.vmtree NODE chunk, .vmtile spawn list)
 *   src/common/Collision/Management/VMapManager.cpp         (getMapFileName / getTileFileName)
 *
 * The editor needs collision geometry for any map without standing up a
 * VMapManager / MapManager / WorldModel sharing layer.  Triangles come out
 * pre-transformed to TC world space so the 3D layer can draw them next to
 * the heightmap.  Future ray-query work can reuse the same flat triangle
 * list (the structure is intentionally extensible: see LoadedVmap::Stats).
 *
 * Zero runtime dependency on TC core (no #include from src/common/Collision/),
 * zero Qt dependency.  Mirrors the style of MMapReader / MapReader.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace world_editor::io
{

// What kind of model the triangle came from.  Same heuristic the TC
// vmap assembler itself uses (TileAssembler.cpp:179): an M2 doodad has
// no precomputed AABB at extract time, so its ModelSpawn lacks the
// MOD_HAS_BOUND flag.  WMOs always have a bound (their group AABBs are
// known at extract).  Useful for color-coding in 3D so the operator
// can tell buildings from props.
enum class VmapSpawnKind : uint8_t
{
    Wmo = 0,    // building / structure -- collision bound at extract time
    M2  = 1,    // doodad / prop -- bound computed at assembly time
};

// One world-space collision triangle: 3 vertices x XYZ + kind tag.  TC
// world space (X=north, Y=west, Z=up); the reader applies
// ModelInstance pos+rot+scale at load time so consumers get a flat
// list - no per-frame transform.
struct VmapTriangle
{
    float          v[3][3];
    VmapSpawnKind  kind = VmapSpawnKind::Wmo;
};

// Per-WMO-instance world-space XY AABB.  Recorded during loadVmaps for
// every spawn whose underlying model is a WMO (not an M2 doodad).  The
// world_editor 2D viewer uses these to paint building footprints on the
// minimap so the operator can see where buildings sit before dropping
// indoor spawns.  Y axis follows TC convention (north = +X, west = +Y).
struct WmoInstanceAabb
{
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
};

// Aggregate statistics for the load run.  Useful for the smoketest +
// future status bar widgets.
struct VmapLoadStats
{
    uint32_t tilesLoaded      = 0;
    uint32_t tilesFailed      = 0;
    uint32_t instancesLoaded  = 0;
    uint32_t instancesFailed  = 0;
    uint32_t modelsLoaded     = 0;  // unique .vmo files actually parsed
    uint32_t modelsFailed     = 0;  // unique .vmo files that failed to parse
    uint64_t triangleCount    = 0;
    uint64_t wmoTriangleCount = 0;  // subset of triangleCount with kind=Wmo
    uint64_t m2TriangleCount  = 0;  // subset of triangleCount with kind=M2
    uint64_t bytesLoaded      = 0;
};

// Move-only handle for a fully materialized world-space triangle list.
class LoadedVmap
{
public:
    LoadedVmap() = default;
    LoadedVmap(std::vector<VmapTriangle>&& tris, VmapLoadStats stats);
    LoadedVmap(std::vector<VmapTriangle>&& tris,
               std::vector<WmoInstanceAabb>&& wmoAabbs,
               VmapLoadStats stats);
    LoadedVmap(LoadedVmap const&)            = delete;
    LoadedVmap& operator=(LoadedVmap const&) = delete;
    LoadedVmap(LoadedVmap&&) noexcept            = default;
    LoadedVmap& operator=(LoadedVmap&&) noexcept = default;
    ~LoadedVmap()                                = default;

    using Stats = VmapLoadStats;

    [[nodiscard]] bool         ok()            const noexcept { return m_stats.tilesLoaded > 0; }
    [[nodiscard]] std::size_t  triangleCount() const noexcept { return m_triangles.size(); }
    [[nodiscard]] std::vector<VmapTriangle> const& triangles() const noexcept { return m_triangles; }
    [[nodiscard]] std::vector<WmoInstanceAabb> const& wmoAabbs() const noexcept { return m_wmoAabbs; }
    [[nodiscard]] Stats const& stats()         const noexcept { return m_stats; }

private:
    std::vector<VmapTriangle>     m_triangles;
    std::vector<WmoInstanceAabb>  m_wmoAabbs;
    VmapLoadStats                 m_stats{};
};

// Load every .vmtile for `mapId` rooted at `vmapsDir/<mapId:04>/` and
// resolve every referenced .vmo (located flat under `vmapsDir/`).  Each
// unique model is parsed once, then its triangles are transformed into
// world space per ModelInstance and appended to the flat output list.
//
// `maxTilesOrZeroForAll` lets the smoketest cap the work; 0 = all tiles.
// Tile and model failures are tolerated silently; stats reflect them.
[[nodiscard]] LoadedVmap loadVmaps(std::filesystem::path const& vmapsDir,
                                   uint32_t mapId,
                                   int maxTilesOrZeroForAll = 0);

// Filename helpers -------------------------------------------------------

// "0000.vmtree" - lives inside vmapsDir/<mapId:04>/.
[[nodiscard]] std::string vmtreeFilename(uint32_t mapId);
// "0000_09_39.vmtile" - lives inside vmapsDir/<mapId:04>/.
[[nodiscard]] std::string vmtileFilename(uint32_t mapId, int tx, int ty);

} // namespace world_editor::io
