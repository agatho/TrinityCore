#include "VmapHeightProbe.h"

#include <algorithm>
#include <cmath>

namespace world_editor::io
{

namespace
{

// Ray-vs-triangle for a ray straight down (-Z) at (px, py).  Returns
// true if the triangle is hit; outZ receives the world Z of the hit
// (the intersection's Z, NOT the distance).  Mirrors the classic
// Moeller-Trumbore intersector specialized for axis-aligned -Z direction.
bool intersectRayDown(VmapTriangle const& t,
                      float px, float py, float& outZ)
{
    // Vertices.
    float const ax = t.v[0][0], ay = t.v[0][1], az = t.v[0][2];
    float const bx = t.v[1][0], by = t.v[1][1], bz = t.v[1][2];
    float const cx = t.v[2][0], cy = t.v[2][1], cz = t.v[2][2];

    // Edges projected to XY.
    float const e1x = bx - ax, e1y = by - ay;
    float const e2x = cx - ax, e2y = cy - ay;

    // det = (-Z dir) cross e1, projected against e2.  For a -Z ray,
    // det = e1.x*e2.y - e1.y*e2.x  (twice the signed XY area).
    float const det = e1x * e2y - e1y * e2x;
    if (std::fabs(det) < 1e-6f)
        return false; // degenerate or edge-on triangle
    float const invDet = 1.0f / det;

    // s = P - A in XY.
    float const sx = px - ax;
    float const sy = py - ay;

    float const u = (sx * e2y - sy * e2x) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    float const v = (e1x * sy - e1y * sx) * invDet;
    if (v < 0.0f || (u + v) > 1.0f) return false;

    // Z at the hit = az + u*(bz-az) + v*(cz-az).
    outZ = az + u * (bz - az) + v * (cz - az);
    return true;
}

} // namespace

VmapHeightProbe::VmapHeightProbe(LoadedVmap const& mesh, float cellSize)
{
    auto const& tris = mesh.triangles();
    if (tris.empty() || cellSize <= 0.0f) return;

    m_cellSize = cellSize;
    m_tris     = tris;

    float minX =  std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float minY =  std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    for (VmapTriangle const& t : m_tris)
    {
        for (int v = 0; v < 3; ++v)
        {
            float const x = t.v[v][0];
            float const y = t.v[v][1];
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    if (!(minX < maxX) || !(minY < maxY)) return;

    m_originX = std::floor(minX / cellSize) * cellSize;
    m_originY = std::floor(minY / cellSize) * cellSize;
    float const spanX = maxX - m_originX;
    float const spanY = maxY - m_originY;
    m_gridW = std::max(1, int(std::ceil(spanX / cellSize)) + 1);
    m_gridH = std::max(1, int(std::ceil(spanY / cellSize)) + 1);

    // Sanity cap: refuse to build > ~16M cells (256MB at sizeof(vector)=16).
    // The bot's continents top out around 200k cells at 16y resolution
    // so this is generous.
    constexpr uint64_t kMaxCells = 16ull * 1024 * 1024;
    if (uint64_t(m_gridW) * uint64_t(m_gridH) > kMaxCells)
    {
        m_cells.clear();
        return;
    }

    m_cells.assign(size_t(m_gridW) * size_t(m_gridH), {});
    uint64_t cellEntries = 0;
    for (size_t i = 0; i < m_tris.size(); ++i)
    {
        VmapTriangle const& t = m_tris[i];
        int minCx, minCy, maxCx, maxCy;
        cellRangeForTri(t, minCx, minCy, maxCx, maxCy);
        for (int cy = minCy; cy <= maxCy; ++cy)
        {
            for (int cx = minCx; cx <= maxCx; ++cx)
            {
                m_cells[cellIndex(cx, cy)].push_back(uint32_t(i));
                ++cellEntries;
            }
        }
    }

    m_stats.triangleCount = m_tris.size();
    m_stats.cellCount     = m_cells.size();
    m_stats.cellEntries   = cellEntries;
    m_stats.cellSize      = m_cellSize;
    m_stats.minX = minX; m_stats.maxX = maxX;
    m_stats.minY = minY; m_stats.maxY = maxY;
}

void VmapHeightProbe::cellRangeForTri(VmapTriangle const& t,
                                      int& outMinCx, int& outMinCy,
                                      int& outMaxCx, int& outMaxCy) const
{
    float minX = t.v[0][0], maxX = t.v[0][0];
    float minY = t.v[0][1], maxY = t.v[0][1];
    for (int v = 1; v < 3; ++v)
    {
        float const x = t.v[v][0];
        float const y = t.v[v][1];
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }
    outMinCx = std::clamp(int(std::floor((minX - m_originX) / m_cellSize)), 0, m_gridW - 1);
    outMaxCx = std::clamp(int(std::floor((maxX - m_originX) / m_cellSize)), 0, m_gridW - 1);
    outMinCy = std::clamp(int(std::floor((minY - m_originY) / m_cellSize)), 0, m_gridH - 1);
    outMaxCy = std::clamp(int(std::floor((maxY - m_originY) / m_cellSize)), 0, m_gridH - 1);
}

float VmapHeightProbe::floorBelow(float worldX, float worldY,
                                  float probeZ, float epsilon) const
{
    if (m_cells.empty()) return INVALID;
    // Out-of-bounds query: no WMO at this point.
    if (worldX < m_stats.minX || worldX > m_stats.maxX
     || worldY < m_stats.minY || worldY > m_stats.maxY)
        return INVALID;

    int const cx = std::clamp(int(std::floor((worldX - m_originX) / m_cellSize)), 0, m_gridW - 1);
    int const cy = std::clamp(int(std::floor((worldY - m_originY) / m_cellSize)), 0, m_gridH - 1);
    auto const& cell = m_cells[cellIndex(cx, cy)];
    if (cell.empty()) return INVALID;

    // Search ceiling = probe height + epsilon so a spawn that's sitting
    // exactly on a floor doesn't get rejected by floating-point noise.
    float const ceiling = probeZ + epsilon;
    float best = INVALID;
    for (uint32_t idx : cell)
    {
        float hitZ;
        if (!intersectRayDown(m_tris[idx], worldX, worldY, hitZ))
            continue;
        if (hitZ > ceiling)      continue;   // above the probe -> upper floor
        if (hitZ > best)         best = hitZ;
    }
    return best;
}

} // namespace world_editor::io
