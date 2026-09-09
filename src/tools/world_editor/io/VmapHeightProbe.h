/*
 * VmapHeightProbe - point-in-WMO floor-height queries over a LoadedVmap.
 *
 * The LoadedVmap from VmapReader is a flat ~5M-triangle world-space list.
 * For an interactive "snap-to-ground" inside a building, ray-down per
 * click would otherwise cost a 5M-tri linear scan.  This class builds a
 * 2D grid index (cell size configurable, default 16y) at construction
 * and answers queries in microseconds.
 *
 * Coordinate convention: TC world (X=north, Y=west, Z=up).  Ray is
 * straight down (-Z), starting from a given (x, y, z) probe point.
 * Returns the highest triangle hit BELOW the probe Z (so a spawn on the
 * upper floor doesn't snap through to the lower floor).
 *
 * Zero TC-runtime and zero Qt dependency.  Pure header/.cpp like the
 * rest of io/.
 */

#pragma once

#include "VmapReader.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace world_editor::io
{

class VmapHeightProbe
{
public:
    VmapHeightProbe() = default;
    explicit VmapHeightProbe(LoadedVmap const& mesh, float cellSize = 16.0f);

    // True if a triangle list was indexed (mesh.ok() AND non-empty
    // triangle list).  Queries on an !ok() probe return INVALID.
    [[nodiscard]] bool ok() const noexcept { return !m_cells.empty(); }

    // Ray-down from `probeZ` at world (x, y).  Returns the highest
    // triangle-hit Z below `probeZ` (allowing a small `epsilon` lenience
    // for spawns sitting right on a floor surface), or INVALID if no
    // triangle in the column was crossed.  Caller is expected to take
    // max(ADT-height, this) when both are present.
    static constexpr float INVALID = -100000.0f;
    [[nodiscard]] float floorBelow(float worldX, float worldY,
                                   float probeZ, float epsilon = 2.0f) const;

    // Indexing stats for the status bar.
    struct Stats
    {
        uint64_t triangleCount = 0;
        uint64_t cellCount     = 0;
        uint64_t cellEntries   = 0;   // sum over cells of indices
        float    cellSize      = 0.0f;
        float    minX = 0.0f, maxX = 0.0f;
        float    minY = 0.0f, maxY = 0.0f;
    };
    [[nodiscard]] Stats const& stats() const noexcept { return m_stats; }

private:
    // Each cell holds indices into `m_tris`.  Triangles that straddle
    // cells appear in every cell their (X, Y) AABB touches.
    std::vector<VmapTriangle> m_tris;
    std::vector<std::vector<uint32_t>> m_cells;  // row-major
    int   m_gridW = 0;
    int   m_gridH = 0;
    float m_cellSize = 16.0f;
    float m_originX = 0.0f;  // m_originX = floor(minX/cellSize) * cellSize
    float m_originY = 0.0f;
    Stats m_stats{};

    [[nodiscard]] int  cellIndex(int cx, int cy) const { return cy * m_gridW + cx; }
    void               cellRangeForTri(VmapTriangle const& t,
                                       int& outMinCx, int& outMinCy,
                                       int& outMaxCx, int& outMaxCy) const;
};

} // namespace world_editor::io
