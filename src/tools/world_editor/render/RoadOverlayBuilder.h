/*
 * RoadOverlayBuilder - extracts a "road skeleton" polyline set from a
 * loaded dtNavMesh.
 *
 * The mmaps_generator + RoadClassifier pipeline tags walkable polygons
 * with area == NAV_AREA_ROAD (7) when their underlying ADT MCNK or WMO
 * triangle is dominated by a road texture.  This builder walks every
 * tile in the navmesh, and for every road-area polygon emits a centroid
 * point + GL_LINES segments to each road-area neighbour (both intra-
 * tile via dtPoly::neis and cross-tile via the dtMeshTile::links chain).
 *
 * The result is a topological skeleton of the auto-extracted road
 * network suitable for a thin overlay rendered on top of the heightmap
 * + minimap layers in NavMeshView.  A separate hook is provided for a
 * future handcrafted-road overlay; that pass is owned by a different
 * agent and lives behind setHandcraftedRoadPolylines() on the viewer.
 *
 * Output vertices are in TC world coordinates (X = north, Y = west)
 * stored as QVector2D pairs - each consecutive pair forms one GL_LINES
 * segment, matching the existing PATH_VSHADER vertex stream.
 */

#pragma once

#include <QVector2D>

#include <cstddef>
#include <vector>

class dtNavMesh;

namespace world_editor::render
{

class RoadOverlayBuilder
{
public:
    RoadOverlayBuilder() = default;

    // Walk every tile in `nm`, collect centroids of NAV_AREA_ROAD polygons,
    // and emit one (start, end) vertex pair per ROAD <-> ROAD edge.  Cross-
    // tile neighbours are followed through tile->links.  Safe to call with
    // nm == nullptr (clears the buffer).
    void buildFromNavmesh(dtNavMesh const* nm);

    // GL_LINES vertex buffer (pairs of TC-world points).  Size is always
    // even; emit count is polyline_vertices().size() / 2.
    [[nodiscard]] std::vector<QVector2D> const& polyline_vertices() const noexcept { return m_vertices; }

    // Reset to empty.
    void clear() noexcept { m_vertices.clear(); }

private:
    std::vector<QVector2D> m_vertices;
};

} // namespace world_editor::render
