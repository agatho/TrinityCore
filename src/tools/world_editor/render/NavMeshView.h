/*
 * NavMeshView - 2D top-down OpenGL viewer for a loaded dtNavMesh.
 *
 * Renders every walkable polygon in the mesh colour-keyed by NAV_AREA
 * (matches the palette mmap_world_dump uses, so the same map looks the
 * same in both tools).  Supports:
 *
 *   - Pan: left-mouse drag.
 *   - Zoom: mouse wheel, anchored at the cursor.
 *   - Hover read-out: emits hoverChanged(worldX, worldY, areaId).
 *   - Click read-out: emits clicked(worldX, worldY, ground_z).
 *   - Frame-to-mesh: F key (or call frameMesh()) recenters the view.
 *
 * The renderer is an immediate-mode triangle fan today.  It builds a
 * single VBO/IBO pair on every loaded-mesh swap (no per-frame upload)
 * so panning and zooming are GPU-driven; this scales to the ~9M tris
 * a full continent navmesh holds without dropping below 60fps.
 *
 * Phase 1 ships the pan/zoom + nav-poly layer.  Subsequent phases plug
 * extra layers (heightmap raster, WMO outlines, spawn icons) into the
 * same paint pipeline via setLayerVisible().
 */

#pragma once

#include "../io/MMapReader.h"
#include "../core/model/WorldEntities.h"
#include "Coords.h"

namespace world_editor::io { class MapTileCache; class CascClient; class MapDb2Lookup; class ListfileLookup; }

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QString>
#include <QStringList>

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QVector>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QContextMenuEvent;

namespace world_editor::render
{

enum class Layer : uint8_t
{
    NavMesh      = 0, // Navmesh polygons (Phase 1).
    Heightmap    = 1, // .map heightmap raster (Phase 1.5).
    Spawns       = 2, // creature/gameobject icons (Phase 1).
    Annotations  = 3, // playerbot_v2_world_metadata (Phase 2).
    Paths        = 4, // waypoint_path polylines (Phase 4).
    Areatriggers = 5, // areatrigger spawns (Phase 7).
    Graveyards   = 6, // world_safe_locs (Phase 7).
    WmoOutline   = 7, // Phase 5+.
    SpawnGroups  = 8, // Color-tint spawns by spawn_group / pool membership.
    Quests       = 9, // Quest-giver/ender markers + connecting lines.
    Minimap      = 10, // Real-map texture overlay (per-tile PNG from extracts).
    BattlemasterRange = 11, // Dashed yellow circle around UNIT_NPC_FLAG_BATTLEMASTER spawns.
    // Per-spawn faction tint - blends the kind-default color toward an
    // Alliance / Horde / Sanctuary / Contested / Neutral hue so the
    // operator can read territorial control at a glance.  Loses to the
    // SpawnGroups layer when both are on (groups override tint, not blend).
    FactionTint  = 12,
    // Per-spawn level-bracket heatmap - blends the kind-default toward
    // a 12-stop palette interpolated across creature_template.minlevel..
    // maxlevel midpoint (1=pale-yellow ... 80=red, 81+=dark-red, world
    // boss/level-1 marker = bright magenta).  Loses to SpawnGroups when
    // both are on, identical to FactionTint pattern.
    LevelHeatmap = 13,
    // Coarse 50-yard cell heatmap showing spawn density across the
    // visible world.  Useful for spotting overpopulated zones at a
    // glance.  Painted via QPainter (dozens of cells, not thousands)
    // BETWEEN the GL terrain/spawn passes so terrain shows through.
    // Honours the SpawnPhaseFilter so density reflects only spawns
    // the operator is actually inspecting.
    SpawnDensity = 14,
    // Taxi-node positions + flight-path edges from taxi_nodes / taxi_path.
    // GL_LINES (light blue) for edges, QPainter circles + name labels for
    // nodes.  Defaults OFF (persisted as viewer2d/flight_paths) so a fresh
    // editor opens without an extra DB roundtrip.
    FlightPaths  = 15,
    // Gathering-node heatmap: per-spawn icons over mining veins / herb
    // nodes / fishing pools / treasure goobers so the operator sees
    // gathering hotspots at a glance.  Classified by gameobject_template
    // name patterns (Vein/Ore/Deposit -> mining, Bloom/Lotus/etc -> herb)
    // plus type==26 -> fishing and "Treasure"/"Cache"/"Chest" -> treasure.
    // Defaults OFF (persisted as viewer2d/gathering_nodes).
    GatheringNodes = 16,
    // Sibling-highlight overlay: 2px golden ring around every spawn that
    // shares the selected spawn's creature_template entry on the current
    // map.  Set is push/clear via setHighlightedSiblings; the toggle just
    // gates the QPainter pass (an empty set is a no-op regardless).
    SiblingHighlight = 17,
    // Instance-entrance overlay: large purple ring around every areatrigger
    // on the current map whose areatrigger_teleport row points at a target
    // dungeon/raid map.  Painted via QPainter on top of the GL passes so
    // it stays legible at any zoom; ON by default and persisted as
    // viewer2d/instance_entrances.
    InstanceEntrance = 18,
    // Transport route overlay: orange polylines connecting consecutive
    // keyframes from transports + transport_animation (zeppelins/boats).
    // Schema-tolerant probe in MainWindow; falls back to keyframes-as-
    // world-positions when no spawn position lookup is available.  Default
    // OFF (persisted as viewer2d/transport_routes); silently empty when
    // neither schema variant exists on the connected DB.
    TransportRoutes  = 19,
    // linked_respawn dependency overlay: dotted dark-green segment from
    // the dependent spawn (`guid`) to the master spawn (`linkedGuid`) so
    // the operator can see which creature's respawn timer is gated on
    // which.  Painter-only (count is small).  Default OFF, persisted as
    // viewer2d/spawn_links.  Silently empty when linked_respawn is missing
    // or no row touches a spawn on the current map.
    SpawnLinks   = 20,
    // Road overlay: auto-extracted polylines walked off the dtNavMesh's
    // NAV_AREA_ROAD polygons (gold), plus an optional handcrafted-road
    // pass (coral red) populated by a separate agent through
    // NavMeshView::setHandcraftedRoadPolylines.  Drawn AFTER the minimap
    // pass so the polylines sit on top of the basemap + heightmap but
    // BEFORE the spawn / annotation / path overlays so clickable
    // entities still occlude the road skeleton.  Default ON; persisted
    // as viewer2d/show_roads.
    Roads        = 21,
    _Count       = 22
};

// The world-entity row DTOs (AnnotationKind/SpawnKind enums + PathNode, Path,
// Areatrigger, SmartScript, Condition, Graveyard, QuestMarker,
// QuestObjectiveMarker, Annotation, Spawn, FlightNode, FlightEdge) now live in
// core/model/WorldEntities.h (included above), still in namespace
// world_editor::render. They were physically moved out of this header so the db
// edit models depend on that lightweight DTO header instead of this
// QOpenGLWidget — fixing the db -> render include inversion.

// Display-name helper for AnnotationKind. Render-layer (returns short labels for
// the viewer/UI), defined in NavMeshView.cpp.
[[nodiscard]] char const* annotationKindName(AnnotationKind kind);


class NavMeshView final : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit NavMeshView(QWidget* parent = nullptr);
    ~NavMeshView() override;

    // Take ownership of a loaded navmesh and rebuild the GL buffers.
    // Passing an empty/!ok() LoadedMMap clears the view.
    void setNavMesh(io::LoadedMMap mesh);

    // Hand over the map-tile cache so the heightmap layer can pull .map
    // tiles on demand.  Borrowed pointer; the cache owner outlives the
    // viewer (it lives on MainWindow).  Pass nullptr to clear.
    void setMapTileCache(io::MapTileCache* cache);

    // Build (or rebuild) the heightmap GL textures for the current map.
    // Called automatically after setNavMesh once the cache is set; the
    // caller can re-invoke after switching maps dir.
    void rebuildHeightmapTiles(uint32_t mapId);

    // Replace the spawn list. The viewer keeps a flat vector and renders
    // it as point-style icons. Pass an empty vector to clear.
    void setSpawns(std::vector<Spawn> spawns);
    [[nodiscard]] std::vector<Spawn> const& spawns() const noexcept { return m_spawns; }

    // Replace the annotation list (world metadata).  Rendered as filled
    // circles in world units (radius is in yards).
    void setAnnotations(std::vector<Annotation> annotations);
    [[nodiscard]] std::vector<Annotation> const& annotations() const noexcept { return m_annotations; }

    // Highlight the currently selected annotation (index into annotations()),
    // or -1 to clear.  The selected row is drawn with a bright outer ring +
    // a small label tooltip in the QPainter pass so the operator can see
    // which row in the toolbox/dock corresponds to which disc on the map.
    void setSelectedAnnotation(int index);
    [[nodiscard]] int selectedAnnotation() const noexcept { return m_selectedAnnotation; }

    // Replace the waypoint-path list.  Rendered as colour-keyed
    // polylines connecting consecutive nodes.
    void setPaths(std::vector<Path> paths);
    [[nodiscard]] std::vector<Path> const& paths() const noexcept { return m_paths; }

    // Areatrigger spawn list (purple squares).
    void setAreatriggers(std::vector<Areatrigger> atrs);
    [[nodiscard]] std::vector<Areatrigger> const& areatriggers() const noexcept { return m_areatriggers; }

    // Graveyard list (cyan diamonds).
    void setGraveyards(std::vector<Graveyard> gys);
    [[nodiscard]] std::vector<Graveyard> const& graveyards() const noexcept { return m_graveyards; }

    // Flight-path graph from taxi_nodes / taxi_path.  Nodes are rendered
    // as small filled-white circles with a light-blue ring + name label,
    // edges as GL_LINES via the existing path shader.  Pass empty vectors
    // (or omit either side) to clear the overlay.
    void setFlightGraph(std::vector<FlightNode> nodes, std::vector<FlightEdge> edges);
    [[nodiscard]] std::vector<FlightNode> const& flightNodes() const noexcept { return m_flightNodes; }
    [[nodiscard]] std::vector<FlightEdge> const& flightEdges() const noexcept { return m_flightEdges; }

    // Transport route polylines.  Each outer entry is one transport's
    // ordered keyframe list (already-world-resolved positions); the
    // renderer emits GL_LINES segments connecting consecutive points,
    // colored orange with a 4px line width via the existing path shader.
    // Pass an empty outer vector (or routes with <2 points apiece) to
    // clear the layer.
    void setTransportRoutes(std::vector<std::vector<coords::WorldPos>> routes);
    [[nodiscard]] std::vector<std::vector<coords::WorldPos>> const& transportRoutes() const noexcept { return m_transportRoutes; }

    // Quest markers (one per spawn that's a quest starter / ender).
    // The viewer overlays ? / ! glyphs via QPainter and draws thin
    // world-space lines between starter+ender of the same quest.
    void setQuestMarkers(std::vector<QuestMarker> markers);
    [[nodiscard]] std::vector<QuestMarker> const& questMarkers() const noexcept { return m_questMarkers; }

    // Quest-objective overlay: kind-coded icons (kill / gather /
    // interact / talk / explore) painted over every spawn that's a
    // target for one of the current map's in-scope quests.  Visible
    // only when Layer::Quests is on AND m_questObjectivesVisible is
    // true (toggled via View menu, persisted in QSettings).
    void setQuestObjectiveMarkers(std::vector<QuestObjectiveMarker> markers);
    [[nodiscard]] std::vector<QuestObjectiveMarker> const& questObjectiveMarkers() const noexcept { return m_questObjectiveMarkers; }
    void setQuestObjectivesVisible(bool on);
    [[nodiscard]] bool questObjectivesVisible() const noexcept { return m_questObjectivesVisible; }

    // Battlemaster recruitment-radius overlay.  MainWindow pushes the
    // set of creature_template.entry values whose npcflag carries
    // UNIT_NPC_FLAG_BATTLEMASTER (0x100000); the viewer renders a dashed
    // yellow circle of radius `m_battlemasterRadiusYards` around every
    // creature spawn whose entry is in that set.  Layer-gated via
    // Layer::BattlemasterRange.
    void setBattlemasterEntries(std::vector<uint32_t> entries);
    void setBattlemasterRange(float radiusYards);
    [[nodiscard]] float battlemasterRange() const noexcept { return m_battlemasterRadiusYards; }

    // Instance-entrance overlay (areatrigger_teleport -> target_map).
    // Pushed by MainWindow at map-load time; the painter ring + label
    // pass keys off m_instanceEntrances + Layer::InstanceEntrance.
    //   spawnId     = areatrigger.SpawnId (unique per row, used as id only)
    //   x, y        = areatrigger.PosX / PosY (TC world frame)
    //   targetMapId = areatrigger_teleport.target_map (or schema variant)
    //   name        = display name (target map directory / dungeon label)
    struct InstanceEntrance
    {
        int64_t  spawnId     = 0;
        float    x           = 0.0f;
        float    y           = 0.0f;
        uint32_t targetMapId = 0;
        QString  name;
    };
    void setInstanceEntrances(std::vector<InstanceEntrance> entries);
    [[nodiscard]] std::vector<InstanceEntrance> const& instanceEntrances() const noexcept { return m_instanceEntrances; }

    // Spawn-dependency links from world.linked_respawn.  Each pair is
    // (fromGuid -> toGuid) where the FROM spawn's respawn timer is gated
    // on the TO spawn.  Painter-only overlay (count is small); the dashed
    // dark-green connector is drawn in the QPainter pass when both spawn
    // icons are present in m_spawns AND pass the phase filter.  Empty
    // vector clears the overlay.
    void setSpawnLinks(std::vector<std::pair<int64_t, int64_t>> links);
    [[nodiscard]] std::vector<std::pair<int64_t, int64_t>> const& spawnLinks() const noexcept { return m_spawnLinks; }

    // 2D WMO building-footprint overlay.  Each tuple is (minX, maxX,
    // minY, maxY) in TC world space; the painter pass draws a 1.5px dashed
    // dark-purple rect with a translucent fill for every footprint.  Empty
    // vector clears the overlay (the Layer::WmoOutline toggle gates the
    // visual independently so the operator can flip the layer off without
    // re-uploading vmap data).
    void setWmoFootprints(std::vector<std::tuple<float, float, float, float>> aabbs);
    [[nodiscard]] std::vector<std::tuple<float, float, float, float>> const& wmoFootprints() const noexcept { return m_wmoFootprints; }

    // Sibling-highlight set: every spawn whose guid is in this vector
    // gets a 2px golden ring drawn in the QPainter overlay pass so the
    // operator can see where the selected entry lives elsewhere on the
    // map.  Pass an empty vector to clear the highlight.  Layer-gated by
    // Layer::SiblingHighlight (default ON internally; toggle off just
    // clears the visual without dropping the set so re-enable is instant).
    void setHighlightedSiblings(std::vector<int64_t> guids);
    [[nodiscard]] std::vector<int64_t> const& highlightedSiblings() const noexcept { return m_siblingGuids; }

    // Replace the handcrafted-road polylines.  Pairs of consecutive
    // QVector2Ds are interpreted as GL_LINES segments (start, end) in
    // TC world coords.  When non-empty AND Layer::Roads is on, the
    // overlay renders these in coral red on top of the gold auto-
    // extracted polylines.  An empty vector clears the layer.  The
    // hook is intended for a separate agent that maintains a curated
    // road-graph SQL table; the auto-extracted pass is invalidated
    // automatically on setNavMesh() and does not interact with this
    // setter.
    void setHandcraftedRoadPolylines(std::vector<QVector2D> const& vertices);
    [[nodiscard]] size_t autoRoadPolylineVertexCount() const noexcept { return m_roadVertexCount; }
    [[nodiscard]] size_t handcraftedRoadPolylineVertexCount() const noexcept { return m_handcraftedRoadVertexCount; }

    // ---- Handcrafted-road segment placement mode ----
    //
    // Two-click FSM that drives the HandcraftedRoadDock "Add segment..."
    // flow.  Left-clicks land world coords; mouse-move during the
    // WaitingForEnd phase paints a coral preview line so the operator
    // sees the segment forming.  Esc cancels.  On the second click, the
    // viewer emits handcraftedSegmentPlaced(fromX, fromY, toX, toY).
    enum class SegmentPlacementState : uint8_t
    {
        None              = 0,
        WaitingForStart   = 1,
        WaitingForEnd     = 2,
    };
    void enterSegmentPlacementMode();
    void cancelSegmentPlacement();
    [[nodiscard]] SegmentPlacementState segmentPlacementState() const noexcept { return m_segmentPlacementState; }
    [[nodiscard]] int chainSegmentCount() const noexcept { return m_chainSegmentCount; }

    // Snap-target search used by the chain placement FSM.  Returns the
    // INDEX into `candidates` of the closest endpoint within
    // SNAP_RADIUS_YARDS of (qx, qy), or -1 when no candidate is close
    // enough.  Exposed for the smoketest to validate the snap logic
    // without spinning up a viewer.
    static constexpr float SNAP_RADIUS_YARDS = 6.0f;
    [[nodiscard]] static int findSnapTarget(float qx, float qy,
                                            std::vector<QVector2D> const& candidates,
                                            float radiusYards = SNAP_RADIUS_YARDS);

    // ---- Handcrafted-road impact preview ----
    //
    // ScanCorridor wrapper + temporary polygon-highlight overlay.  The
    // dock's "Preview impact" button calls this with the candidate
    // segment; the viewer paints the affected polygons in translucent
    // yellow for a short window and the polyRef count is returned so the
    // caller can show it in a popup.
    struct ImpactPreviewResult
    {
        std::vector<uint64_t> polyRefs;
        uint32_t              tilesScanned  = 0;
        uint32_t              polysExamined = 0;
    };
    [[nodiscard]] ImpactPreviewResult previewSegmentImpact(float fromX, float fromY,
                                                           float toX,   float toY,
                                                           float width);
    void clearSegmentImpactPreview();

    // Flip the area of every polygon the candidate segment intersects to
    // NAV_AREA_ROAD on the IN-MEMORY navmesh.  Returns the number of
    // polygons whose area actually changed.  Does NOT persist anything;
    // the worldserver re-applies handcrafted-road tagging at map load via
    // the `handcrafted_road` SQL table.  Use this to preview how the
    // gold auto-road overlay will look after the next worldserver restart.
    [[nodiscard]] size_t applyHandcraftedSegmentToLocalNavmesh(float fromX, float fromY,
                                                               float toX,   float toY,
                                                               float width);

    // Rebuild the auto-road overlay (gold polylines) from the current
    // navmesh.  Exposed so applyHandcraftedSegmentToLocalNavmesh can
    // surface its changes in the gold overlay immediately.
    void rebuildRoadOverlayFromCurrentNavmesh();

    // Visibility toggles for each layer (Layer::NavMesh defaults true).
    void setLayerVisible(Layer layer, bool visible);
    [[nodiscard]] bool isLayerVisible(Layer layer) const;

    // Phase-mask filter (TC spawn rows carry phaseId/phaseGroup).  When
    // `enabled` is true, the viewer hides any spawn whose phaseId AND
    // phaseGroup BOTH differ from the filter -- a spawn is shown when
    // EITHER its phaseId matches filter.phaseId OR its phaseGroup matches
    // filter.phaseGroup OR both of its phase fields are zero (the
    // "always visible / no phase restriction" convention TC uses).
    // The filter also gates spawn click hit-tests so the operator can't
    // click an invisible spawn by accident.
    struct SpawnPhaseFilter
    {
        bool     enabled    = false;
        uint32_t phaseId    = 0;
        uint32_t phaseGroup = 0;
    };
    void setSpawnPhaseFilter(SpawnPhaseFilter f);
    [[nodiscard]] SpawnPhaseFilter const& spawnPhaseFilter() const noexcept { return m_spawnPhaseFilter; }
    // Returns true if the given spawn passes the current filter.  Used
    // by both the geometry upload (skip) and the hit-test (skip).
    [[nodiscard]] bool spawnPassesPhaseFilter(Spawn const& s) const;
    // Count of spawns currently visible under the filter.  When the
    // filter is disabled this equals spawns().size().
    [[nodiscard]] size_t visibleSpawnCount() const;

    // Placement mode: when on, left-clicks emit placementRequested(x, y)
    // INSTEAD of pan/spawn-click semantics.
    void setPlacementMode(bool on);
    [[nodiscard]] bool placementMode() const noexcept { return m_placementMode; }

    // Path debug mode: 1st left-click sets start, 2nd sets end + runs
    // findRoute(), 3rd clears and restarts.  Disables pan/spawn/path
    // hit-tests while active (middle-mouse still pans).  See
    // pathDebugComputed signal for the result.
    void setPathDebugMode(bool on);
    [[nodiscard]] bool pathDebugMode() const noexcept { return m_pathDebugMode; }
    // Clear current debug state (markers + computed route).
    void clearPathDebug();

    // Reset the view to fit the mesh's world bbox in the widget.
    void frameMesh();
    // Pan the camera so world (worldX, worldY) is at the viewport
    // center.  Optionally also sets yardsPerPixel (zoom); pass 0 to
    // keep the current zoom.  Used by Find/Jump.
    void panTo(float worldX, float worldY, float yardsPerPixel = 0.0f);

    // Detour-driven auto-route from (startX, startY, startZ) to
    // (endX, endY, endZ) using the currently loaded navmesh.  Returns
    // a sequence of intermediate world-space waypoints (NOT including
    // the start point; the LAST point is the actual reached end which
    // may differ from the requested end if the navmesh can't reach it).
    // Empty vector on failure (no nav mesh, start/end off mesh, etc.).
    // Used by the Path-draw auto-route action so the operator clicks
    // two points and the tool inserts every node between.
    [[nodiscard]] std::vector<coords::WorldPos> findRoute(
        float startX, float startY, float startZ,
        float endX,   float endY,   float endZ,
        int maxStraightPathPoints = 256) const;

    // Current view transform - shared with overlays so spawn icons
    // (drawn by other widgets) end up at the same screen coords as
    // the mesh under them.
    [[nodiscard]] coords::ViewTransform const& viewTransform() const noexcept { return m_view; }

signals:
    void hoverChanged(float worldX, float worldY);
    void clicked(float worldX, float worldY);
    // Emitted when the user clicks within hit-radius of a spawn icon;
    // the index is into spawns().  -1 means no spawn under the cursor.
    void spawnHovered(int spawnIndex);
    void spawnClicked(int spawnIndex);
    // Emitted when the operator drags a spawn icon to a new world spot
    // and releases.  The signal carries the proposed world (X, Y) - Z
    // stays the responsibility of MainWindow (it decides whether to
    // snap-to-ground or keep the old value).
    void spawnMoved(int spawnIndex, float worldX, float worldY);
    // Annotation interaction (index is into annotations()).
    void annotationHovered(int annotationIndex);
    void annotationClicked(int annotationIndex);
    // Phase 7: areatrigger / graveyard click hit.
    void areatriggerClicked(int index);
    void graveyardClicked(int index);
    // Emitted while placementMode() is true and the operator clicks.
    void placementRequested(float worldX, float worldY);
    // Path-debug result.  Fires after the second click while
    // pathDebugMode() is true.  totalDistance is the cumulative XY
    // length of the route in yards.  reachedEnd is true when the last
    // waypoint is within 5 yards of the requested endpoint.
    void pathDebugComputed(int waypointCount, float totalDistance, bool reachedEnd);
    // Phase 3d: multi-selection changed (shift-click toggle or
    // box-select release). Vector is indices into spawns().
    void spawnSelectionChanged(QVector<int> const& indices);
    // Phase 4: clicked a path polyline (hit within hitRadiusPixels of
    // any segment).  Index is into paths().
    void pathClicked(int pathIndex);
    // Path node interaction.  nodeIndex is into paths()[pathIndex].nodes.
    // pathNodeMoved fires after a drag-release; the (worldX, worldY) is
    // the proposed new position.  The Z is left to MainWindow (it can
    // snap-to-ground on the new XY).
    void pathNodeClicked(int pathIndex, int nodeIndex);
    void pathNodeMoved(int pathIndex, int nodeIndex, float worldX, float worldY);
    // Right-click on a path node / segment.  MainWindow opens a QMenu
    // (Insert before / Insert after / Delete on the node form; Insert
    // node here on the segment form).  afterNodeIndex is the LOWER index
    // of the two segment endpoints, so insert goes between (after, after+1).
    void pathNodeContextMenuRequested(int pathIndex, int nodeIndex, QPoint globalPos);
    void pathSegmentContextMenuRequested(int pathIndex, int afterNodeIndex,
                                         float worldX, float worldY, QPoint globalPos);
    // Right-click on a spawn icon - MainWindow opens a context menu with
    // the "Clone..." entry.  spawnIndex is into spawns().
    void spawnContextMenuRequested(int spawnIndex, QPoint globalPos);
    // Two-click handcrafted-road placement (see enterSegmentPlacementMode).
    // Emitted on the second click with the captured TC world coords.
    void handcraftedSegmentPlaced(float fromX, float fromY, float toX, float toY);
    // Emitted on each transition of the placement state machine so the
    // caller (HandcraftedRoadDock + status bar) can update its hints.
    void handcraftedSegmentPlacementStateChanged(int newState);
    // Emitted whenever the running chain segment counter changes so the
    // status bar can surface "N segments placed" while the operator is
    // still chaining.  newCount is the count AFTER the just-placed segment.
    void handcraftedChainSegmentCountChanged(int newCount);
    // Emitted when the operator clicks a road connectivity-diagnostic marker
    // (a red near-miss gap ring or an amber dead-end ring) while the Roads
    // layer is on.  The dock responds by selecting the handcrafted segment(s)
    // whose endpoint sits at (worldX, worldY) so they can be edited / deleted /
    // reconnected.
    void roadDiagnosticClicked(float worldX, float worldY);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Vertex
    {
        float x, y;     // screen pixels at zoom 1.0; transformed by uniform per frame.
                        // We store world coords (TC X, TC Y) and let the vertex shader
                        // do the world->screen transform via the view uniforms.
        uint8_t r, g, b, a;
    };

    void rebuildBuffers();
    void uploadGeometry();
    void rebuildSpawnBuffers();
    void uploadSpawnGeometry();
    void rebuildAnnotationBuffers();
    void uploadAnnotationGeometry();
    [[nodiscard]] coords::WorldPos screenToWorld(QPoint const& p) const;
    // Returns spawn index whose icon contains the screen point (or -1).
    // Hit radius is hitRadiusPixels in widget pixels.
    [[nodiscard]] int hitTestSpawn(QPoint const& screen, float hitRadiusPixels = 8.0f) const;
    // Returns annotation index whose disk contains the world point at
    // the cursor (radius matched in world units, not pixels).  -1 if
    // none.
    [[nodiscard]] int hitTestAnnotation(QPoint const& screen) const;
    // Returns the path index whose polyline passes within hitRadiusPixels
    // of `screen`, or -1.
    [[nodiscard]] int hitTestPath(QPoint const& screen, float hitRadiusPixels = 5.0f) const;
    // Returns the (path index, node index) pair under the cursor within
    // hitRadiusPixels, or {-1, -1}.  Node-pick is tighter than segment-
    // pick so the operator can reliably click between two close nodes.
    struct PathNodeHit { int pathIndex = -1; int nodeIndex = -1; };
    [[nodiscard]] PathNodeHit hitTestPathNode(QPoint const& screen,
                                              float hitRadiusPixels = 7.0f) const;
    // Returns (path index, node index BEFORE the segment) for the
    // segment under the cursor, plus the projected world point on that
    // segment (used as the default position for "Insert node here").
    [[nodiscard]] int hitTestPathSegment(QPoint const& screen, int& outAfterNode,
                                         float& outProjX, float& outProjY,
                                         float hitRadiusPixels = 5.0f) const;

    io::LoadedMMap m_mesh;
    std::vector<Spawn>       m_spawns;
    std::vector<Annotation>  m_annotations;
    std::vector<Path>        m_paths;
    std::vector<Areatrigger> m_areatriggers;
    std::vector<Graveyard>   m_graveyards;
    std::vector<QuestMarker> m_questMarkers;
    std::vector<QuestObjectiveMarker> m_questObjectiveMarkers;
    bool m_questObjectivesVisible = false;

    // Set of creature_template.entry values flagged BATTLEMASTER.  The
    // viewer renders a dashed yellow recruitment-radius circle around
    // every Creature spawn whose entry is in this set.  Populated by
    // MainWindow at map-load time; cached as unordered_set for O(1) lookup.
    std::unordered_map<uint32_t, uint8_t> m_battlemasterEntries;
    float m_battlemasterRadiusYards = 5.0f;

    // Spawn-guid set highlighted as siblings of the currently selected
    // creature.  unordered_set provides O(1) per-spawn membership check
    // in the QPainter pass; the source vector is also kept so callers
    // can read it back via highlightedSiblings().
    std::vector<int64_t>          m_siblingGuids;
    std::unordered_set<int64_t>   m_siblingGuidSet;

    // Cached instance-entrance ring entries.  Painted by the QPainter
    // overlay when Layer::InstanceEntrance is on.
    std::vector<InstanceEntrance> m_instanceEntrances;

    // linked_respawn dependency edges (fromGuid -> toGuid).  Painter-only;
    // resolved against m_spawns at paint time so a spawn-list refresh
    // does not invalidate this cache.
    std::vector<std::pair<int64_t, int64_t>> m_spawnLinks;

    // 2D WMO building-footprint rectangles in TC world space.  Painted
    // when Layer::WmoOutline is on; the operator toggles this via
    // View -> WMO 2D footprints.  Empty when the operator hasn't loaded
    // vmaps (the menu handler emits a one-shot status bar tip in that case).
    std::vector<std::tuple<float, float, float, float>> m_wmoFootprints;

    // Returns objective-marker index within ~hitRadiusPixels of `screen`,
    // or -1 if none.  Uses the spawn position + 4-pixel southward offset
    // (where the icons are actually rendered).  Sub-linear scan suffices
    // since the marker list is bounded (<~1000 per map).
    [[nodiscard]] int hitTestQuestObjective(QPoint const& screen,
                                            float hitRadiusPixels = 8.0f) const;

    // GL buffer for quest starter <-> ender connector lines.  GL_LINES
    // via the path shader; rebuilt whenever setQuestMarkers runs.
    QOpenGLBuffer            m_questLineVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_questLineVao;
    GLsizei                  m_questLineVertexCount = 0;
    void rebuildQuestLineBuffer();
    // Layer-visibility bitmap.  Index 7 (WmoOutline) defaults OFF because
    // the 2D building-footprint overlay it gates is opt-in via
    // View -> WMO 2D footprints; the 3D WMO-mesh toggle does not depend
    // on this flag (it lives on SceneView3D::setWmoVisible).
    bool m_layerVisible[size_t(Layer::_Count)] = { true, true, true, true, true, true, true, false, true, true, true, true, true, true, false, false, false, true, true, false, false, true };

    // Per-spawn-guid color override for the SpawnGroups layer.  Keyed
    // by Spawn::guid; when the layer is on and a guid is in the map,
    // the spawn icon is rendered in this color instead of the
    // kind-default (creature red / gameobject blue).  Stored as packed
    // RGBA (0xAABBGGRR) for cheap upload.
public:
    void setSpawnGroupColors(std::unordered_map<int64_t, uint32_t> colors);
    // Per-entry faction-group classification for the FactionTint layer.
    // Keyed by creature_template.entry; value is one of:
    //   0 = Alliance (blue), 1 = Horde (red), 2 = Sanctuary (yellow),
    //   3 = Contested (purple), 4 = Other / Neutral (gray).
    // When Layer::FactionTint is on AND the map is non-empty, the
    // upload-time spawn-color computation blends the kind-default
    // toward the faction-group color by 60% BEFORE the SpawnGroups
    // layer override applies (so groups still win when both are on).
    void setFactionTintMap(std::unordered_map<uint32_t, uint8_t> map);
    // Per-entry creature level range (creature_template.minlevel/maxlevel)
    // for the LevelHeatmap layer.  Key = creature_template.entry, value =
    // (minLevel, maxLevel).  When Layer::LevelHeatmap is on, the spawn
    // color blends toward a 12-stop palette interpolated across the
    // (min+max)/2 midpoint.  Boss / world-boss markers (TC stores level
    // -1 or 0 for those) get a bright magenta override.  Like FactionTint,
    // SpawnGroups still wins when both layers are on.
    void setLevelMap(std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>> map);
    // Gathering-node classification.  Key = gameobject.guid (int64; the
    // primary key of the gameobject table), value = kind:
    //   0 = Mining vein, 1 = Herb node, 2 = Fishing pool, 3 = Treasure.
    // The viewer paints a small kind-coded icon above the spawn's normal
    // icon ONLY when Layer::GatheringNodes is on.  Pushing a fresh map
    // replaces the old one; pushing an empty map clears the overlay.
    void setGatheringNodes(std::unordered_map<int64_t, uint8_t> nodes);
private:
    std::unordered_map<int64_t, uint32_t> m_spawnGroupColors;
    std::unordered_map<uint32_t, uint8_t> m_factionTintMap;
    std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>> m_levelMap;
    std::unordered_map<int64_t, uint8_t>  m_gatheringNodes;

    // Phase-mask filter state.  Persisted by MainWindow in QSettings.
    SpawnPhaseFilter m_spawnPhaseFilter{};

    // SpawnDensity layer cache.  50-yard cells keyed by (cellX, cellY)
    // mapped to spawn-count.  Rebuilt lazily on first paint after
    // m_spawnDensityDirty flips (setSpawns + setSpawnPhaseFilter both
    // mark it dirty since the filter gates which spawns contribute).
    static constexpr float kSpawnDensityCellYards = 50.0f;
    struct CellKey
    {
        int cx = 0;
        int cy = 0;
        bool operator==(CellKey const& o) const noexcept { return cx == o.cx && cy == o.cy; }
    };
    struct CellKeyHash
    {
        size_t operator()(CellKey const& k) const noexcept
        {
            // Pack into 64 bits; cell indices are bounded by world size / 50y.
            uint64_t const u = (uint64_t(uint32_t(k.cx)) << 32) | uint32_t(k.cy);
            return std::hash<uint64_t>{}(u);
        }
    };
    mutable std::unordered_map<CellKey, uint32_t, CellKeyHash> m_spawnDensityCells;
    mutable bool m_spawnDensityDirty = true;
    void rebuildSpawnDensityGrid() const;

    // OpenGL resources (created in initializeGL, destroyed in dtor).
    QOpenGLShaderProgram*       m_program = nullptr;
    QOpenGLBuffer               m_vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject    m_vao;
    int                         m_uYardsPerPixel = -1;
    int                         m_uAnchorWorld   = -1;
    int                         m_uAnchorPixel   = -1;
    int                         m_uViewportSize  = -1;
    int                         m_uViewRotation  = -1;   // u_viewRotationRad
    GLsizei                     m_vertexCount    = 0;
    bool                        m_buffersReady   = false;

    // View transform + mesh-aabb cache (TC frame: world X north, Y west).
    coords::ViewTransform m_view{};
    float m_meshMinX = 0.0f, m_meshMaxX = 0.0f;
    float m_meshMinY = 0.0f, m_meshMaxY = 0.0f;
    bool  m_meshBoundsValid = false;

    // Continent-level view rotation (in degrees), applied as a screen-
    // space rotation around the viewport center on top of the existing
    // coords::worldToScreen/screenToWorld plumbing.  Default -90.0 (i.e.
    // rotate the 2D viewer 90 degrees clockwise relative to the legacy
    // image-east=screen-right layout).  Default is now 0.0f: the 2026-05-26
    // A/B follow-up showed that rotating the whole view 90 CW broke the
    // already-correct heightmap / spawn / path / annotation layers because
    // ONLY the minimap BLP layer had the wrong intrinsic axis mapping; the
    // fix lives in the per-tile minimap transform (m_minimapTransform),
    // not at the view level.  The View menu's "View rotation: 90 CW"
    // failsafe action is retained as an emergency toggle and persists via
    // QSettings("viewer2d/view_rotation_degrees"), but ships unchecked.
    float m_viewRotationDegrees = 0.0f;
public:
    // Read/write the screen-space view rotation.  Setter persists via
    // QSettings, updates the static s_currentViewRotationDegrees mirror,
    // and triggers a repaint.  Used by MainWindow's View menu toggle.
    [[nodiscard]] float viewRotationDegrees() const noexcept { return m_viewRotationDegrees; }
    void                setViewRotationDegrees(float deg);
private:
    static float        s_currentViewRotationDegrees;

    // World->screen helpers that pre/post-apply m_viewRotationDegrees on
    // top of coords::worldToScreen / coords::screenToWorld.  Every 2D
    // viewer call site MUST go through these so all overlay layers stay
    // aligned with the GL projection (which mirrors the same rotation
    // via the u_viewRotationRad shader uniform).
    [[nodiscard]] std::pair<float, float> worldToScreen2D(coords::WorldPos const& w) const noexcept;
    [[nodiscard]] coords::WorldPos        screenToWorld2D(float sx, float sy, float z = 0.0f) const noexcept;
    [[nodiscard]] coords::WorldPos        screenToWorld2D(QPointF const& p) const noexcept;
    [[nodiscard]] coords::WorldPos        screenToWorld2D(QPoint  const& p) const noexcept;
    // Inverse-rotate a screen-space pixel delta (used by mouseMove pan
    // so the world translates along the screen drag direction even when
    // the view is rotated).  Returns the delta in pre-rotation pixel space.
    [[nodiscard]] QPointF unrotateScreenDelta(QPointF const& d) const noexcept;

    // Pan state.
    bool   m_panning = false;
    QPoint m_panAnchor{};
    coords::WorldPos m_panAnchorWorld{};

    // Cache the geometry CPU-side until initializeGL fires so setNavMesh
    // works before the widget has a GL context.
    std::vector<Vertex> m_pendingVertices;
    bool                m_pendingDirty = false;

    // Spawn-icon GL resources (separate program because they need a
    // different vertex layout and a quad geometry shader trick - we
    // expand each spawn into a 2-triangle screen-space quad in the
    // vertex shader).
    QOpenGLShaderProgram*    m_spawnProgram = nullptr;
    QOpenGLBuffer            m_spawnVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_spawnVao;
    GLsizei                  m_spawnVertexCount = 0;
    int                      m_spawnUYpp        = -1;
    int                      m_spawnUAnchorW    = -1;
    int                      m_spawnUAnchorP    = -1;
    int                      m_spawnUViewport   = -1;
    int                      m_spawnUPixelSize  = -1;
    int                      m_spawnUViewRotation = -1;
    std::vector<Vertex>      m_pendingSpawnVerts;
    bool                     m_pendingSpawnDirty = false;

    int  m_hoveredSpawn      = -1;
    int  m_hoveredAnnotation = -1;
    int  m_selectedAnnotation = -1; // -1 when no annotation is selected
    bool m_placementMode     = false;

    // ---- Path debug mode (live navmesh route preview) ----
    // Two-click FSM: state 0 = awaiting start, 1 = awaiting end,
    // 2 = displayed (next click resets).  Route is the result of
    // findRoute() on the second click; rendered as a GL_LINES polyline
    // with per-segment color by length (green<10y, yellow<30y, red>=30y).
    bool                          m_pathDebugMode      = false;
    int                           m_pathDebugState     = 0;
    coords::WorldPos              m_pathDebugStart{};
    coords::WorldPos              m_pathDebugEnd{};
    std::vector<coords::WorldPos> m_pathDebugRoute;
    bool                          m_pathDebugReached   = false;
    // GL resources -- reuses the path shader (world XY + RGBA).
    QOpenGLBuffer                 m_pathDebugVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject      m_pathDebugVao;
    GLsizei                       m_pathDebugVertexCount = 0;
    void rebuildPathDebugBuffer();
    // Compute the ground Z for an XY click via the map-tile cache when
    // available; falls back to 0 (findRoute's 50y vertical extent covers
    // most cases regardless).
    [[nodiscard]] float probeGroundZ(float worldX, float worldY) const;

    // ---- Heightmap layer (Phase 1.5) ----
    io::MapTileCache* m_mapCache = nullptr; // borrowed
    struct HeightmapTile
    {
        int      gx        = 0;
        int      gy        = 0;
        GLuint   texture   = 0;
        float    worldMinX = 0.0f;  // TC X
        float    worldMaxX = 0.0f;
        float    worldMinY = 0.0f;  // TC Y
        float    worldMaxY = 0.0f;
    };
    std::vector<HeightmapTile> m_heightmapTiles;

public:
    // Late-bound minimap-tile directory.  When set, the heightmap
    // builder attempts to load
    //   <minimap_dir>/<mapId>/map<gx>_<gy>.png
    // for every tile.  Loaded textures are drawn by the Minimap layer
    // (which draws on TOP of the heightmap layer so missing tiles
    // gracefully fall back to elevation shading).  Pass an empty
    // string to disable.
    void setMinimapDir(QString const& dir);

    // Late-bound CASC client + Map.db2 lookup.  When set + open, the
    // minimap loader falls through to fetching BLP tiles directly from
    // CASC at "world/minimaps/<MapDirectory>/map<gx>_<gy>.blp".  PNG on
    // disk wins when present; CASC is used only as the fallback path.
    // Both pointers are BORROWED (MainWindow owns them).  Pass nullptr
    // to detach.
    void setCascClient(io::CascClient* casc, io::MapDb2Lookup* mapDb2);

    // Late-bound wow-listfile FDID resolver.  REQUIRED on modern (TWW
    // build 67186+) client data because many minimap BLPs are stored
    // FileDataID-only in CASC; path-based CascOpenFile() always misses.
    // When set, the minimap loader iterates the same candidate-path list
    // through ListfileLookup::resolveFdid and calls openByFileDataId on
    // the first hit.  Borrowed pointer; pass nullptr to detach.
    void setListfileLookup(io::ListfileLookup* listfile);

    // Iterate every (gx, gy) currently active in the heightmap build +
    // request a CASC-resolved BLP for it.  Exposes the resulting RGBA
    // image to the caller via a callback so MainWindow can write PNGs.
    // Returns the count of tiles successfully fetched.  Skips tiles
    // whose PNG already exists on disk (operator already extracted).
    struct ExportedTile { int gx; int gy; int width; int height; std::vector<uint8_t> rgba; };
    using ExportTileCallback = std::function<void(ExportedTile const&)>;
    int exportMinimapTilesFromCasc(ExportTileCallback const& sink);

    // Diagnostics getters surfaced to MinimapDiagnosticsDock.  All four are
    // running counters reset on destroyMinimapTextures() (i.e. map switch /
    // setMinimapDir).  Heightmap/cached counts are simple container sizes.
    [[nodiscard]] QString const& minimapDir() const noexcept { return m_minimapDir; }
    [[nodiscard]] int minimapSuccessfulLoads() const noexcept { return m_minimapSuccessfulLoads; }
    [[nodiscard]] int minimapFailedLoads()     const noexcept { return m_minimapFailedLoads; }
    [[nodiscard]] QString minimapLastTried()   const          { return m_minimapLastTried; }
    [[nodiscard]] int minimapCachedCount()     const noexcept { return int(m_minimapTextures.size()); }
    [[nodiscard]] int heightmapTileCount()     const noexcept { return int(m_heightmapTiles.size()); }

    // Run CASC enumeration for the current map's minimap directory and
    // return the discovered filenames (up to `max`).  Used by the
    // diagnostics dock's "Probe CASC paths" button so the operator can
    // see what naming convention is actually in the archive.  Also
    // populates the internal discovered-name cache so loadOrUpload picks
    // up new patterns without a restart.  Returns empty when CASC is
    // closed / Map.db2 is missing the directory.
    [[nodiscard]] QStringList probeMinimapCascNames(int max = 50);

    // ---- Minimap transform A/B test (panel-driven) -------------------
    //
    // Two research reports disagree on the right per-tile rotation that
    // takes a BLP minimap blob into the orientation our heightmap shader
    // samples it in (U=worldX, V=worldY).  Combined with a separate
    // continent-level 90 CW rotation the operator saw on screen, the
    // right answer is empirically determined: the panel exposes 10
    // candidate transforms via radio buttons so the operator can flip
    // between them at runtime without rebuilding.  The currently-active
    // transform is also published as a static so SceneView3D can mirror
    // it without a setter wiring.
    enum class MinimapTransform : uint8_t
    {
        Identity            = 0,  // No transform.
        Transpose           = 1,  // Bare transpose (swap rows/cols, prior default).
        Rotate90CW          = 2,  // Rotate 90 clockwise.
        Rotate90CCW         = 3,  // Rotate 90 counter-clockwise.
        Rotate180           = 4,  // Rotate 180.
        MirrorH             = 5,  // Mirrored horizontally.
        MirrorV             = 6,  // Mirrored vertically.
        Transpose_MirrorH   = 7,  // Transpose then mirror horizontally.
        Transpose_MirrorV   = 8,  // Transpose then mirror vertically.
        Transpose_Rot180    = 9,  // Transpose then rotate 180.
        Count_
    };
    void setMinimapTransform(MinimapTransform t);
    [[nodiscard]] MinimapTransform minimapTransform() const noexcept { return m_minimapTransform; }
    [[nodiscard]] static char const* minimapTransformName(MinimapTransform t) noexcept;
    [[nodiscard]] static QImage applyMinimapTransform(QImage const& img, MinimapTransform t);
    // Process-wide active transform, kept in sync with the 2D viewer's
    // selection so the 3D textured-terrain pass can apply the same
    // rotation without a direct setter wiring.  Default Transpose
    // preserves the historical baseline at process start.
    [[nodiscard]] static MinimapTransform currentMinimapTransform() noexcept { return s_currentMinimapTransform; }

    // Diagnostic dump for the "Inspect at canonical coords" button.  For
    // each canonical tile (map34_61, map32_48, map49_36) the loader
    // resolves the listfile FDID, probes CASC, and reports back the
    // resolved FDID + BLP encoding (DXT1/DXT3/DXT5/Pal8/ARGB) + decoded
    // QImage::Format value.  Used by MinimapDiagnosticsDock to surface a
    // side-by-side comparison without enabling verbose logging.  Always
    // safe to call: missing CASC/Map.db2/listfile fields are reported
    // verbatim as "n/a"; the call never mutates GL state and never adds
    // to m_minimapTextures.
    struct CanonicalTileReport
    {
        int     gx          = 0;
        int     gy          = 0;
        QString resolvedFdid;   // "n/a" if no listfile or no hit
        QString blpEncoding;    // "DXT1" / "DXT3" / "DXT5" / "Pal8" / "ARGB" / "n/a"
        QString qimageFormat;   // QImage::Format integer + textual hint
        QString notes;          // free-form (miss reason, decode failure, etc.)
    };
    [[nodiscard]] std::vector<CanonicalTileReport> inspectCanonicalTiles();
private:
    QString m_minimapDir;
    // Per-load counters for the diagnostics dock.  m_minimapLastTried holds
    // a short "gx,gy -> result" string the dock surfaces verbatim.
    int     m_minimapSuccessfulLoads = 0;
    int     m_minimapFailedLoads     = 0;
    QString m_minimapLastTried;
    io::CascClient*      m_cascClient = nullptr;   // borrowed
    io::MapDb2Lookup*    m_mapDb2     = nullptr;   // borrowed
    io::ListfileLookup*  m_listfile   = nullptr;   // borrowed
    // Resolved-FDID cache keyed by (gy << 16 | gx).  Populated the first
    // time a tile resolves through the listfile so subsequent renders of
    // the same tile skip the candidate-path probing entirely.  Cleared by
    // destroyMinimapTextures (i.e. map switch + setMinimapDir / setListfile).
    std::unordered_map<uint32_t, uint32_t> m_minimapFdidByTile;
    // Per-tile minimap texture lookup.  Keyed by (gx, gy) packed into
    // a 32-bit int (gy << 16 | gx).  Holds 0 when the load was
    // attempted but no PNG was found.
    std::unordered_map<uint32_t, GLuint> m_minimapTextures;
    // Diagnostic state for the minimap layer: which mapIds have already
    // logged a first-success / first-failure to qDebug so we don't spam
    // hundreds of identical lines.  Reset by destroyMinimapTextures().
    std::unordered_set<uint32_t> m_minimapLoggedSuccess;
    std::unordered_set<uint32_t> m_minimapLoggedFailure;
    // One-shot CASC enumeration: for each mapId where the first BLP lookup
    // misses, we list the minimap directory once and stash discovered
    // names so subsequent tiles try the right naming convention.  Keyed
    // by mapId; value is the raw filenames returned by listFiles().
    std::unordered_set<uint32_t> m_minimapEnumeratedMapIds;
    std::unordered_map<uint32_t, std::vector<std::string>> m_minimapDiscoveredNames;
    // Once-per-second tile draw counter for the minimap render pass.
    qint64 m_minimapLogLastMs = 0;
    int    m_minimapTilesDrawnCount   = 0;
    int    m_minimapTilesSkippedCount = 0;
    // One-shot orientation diagnostic: emits a single qDebug line on the
    // first tile uploaded after a map switch confirming the BLP was
    // vertically flipped (BLP top row -> GL bottom row) and reporting its
    // placement so the operator can verify "Eastern Kingdoms tall+thin"
    // against the on-screen result without enabling verbose logging.
    bool m_minimapOrientationLogged = false;

    // Active per-tile transform; driven by the MinimapDiagnosticsDock
    // radio panel via setMinimapTransform().  The 2D and 3D viewers
    // share the choice via the static s_currentMinimapTransform mirror.
    //
    // Default = Rotate90CW.  Operator A/B 2026-05-26 pass 1: Rotate180
    // looked correct PER TILE individually but the continent ended up
    // rotated 90 CW relative to the wow.export reference image when the
    // view-level rotation was OFF.  Pass 1 then enabled a view-level
    // -90 deg rotation, which fixed the minimap but visually broke the
    // heightmap / spawn / path / annotation layers (those layers were
    // already correct).  Pass 2 reverts the view-level rotation to 0
    // and folds the missing 90 deg counter-rotation into the per-tile
    // BLP transform: Rotate180 + an additional 90 CCW == Rotate90CW.
    // If the operator reports tiles look right but each is mirrored
    // relative to its neighbours, the radio panel exposes Rotate90CCW
    // for a one-click flip.
    MinimapTransform m_minimapTransform = MinimapTransform::Transpose_Rot180;
    static MinimapTransform s_currentMinimapTransform;
    // Force-restart helper for setMinimapTransform: flushes texture
    // cache + heightmap state so the chunked-build queue re-seeds on the
    // next paint.  Required so the visible panel actually re-streams
    // tiles rather than leaving the GL cache empty.
    void restartMinimapStreaming(char const* reason);

    // Last 50-tile multiple at which paintGL emitted a "streaming
    // progress: N/total" log line.  Reset by restartMinimapStreaming so
    // every transform-change pass produces a fresh trail.
    size_t m_streamProgressLoggedAt = 0;

    // Per-mapId cached `.map` file coverage scan.  Seeds the heightmap +
    // minimap tile queue with the UNION of (dtNavMesh-loaded tiles) and
    // (tiles found on disk under <mapsDir>/<padded mapId>_<gx>_<gy>.map),
    // so partially-regenerated mmtile sets don't gate minimap coverage.
    // Keyed by mapId, value is the set of (gx, gy) pairs discovered on disk.
    std::unordered_map<uint32_t, std::vector<std::pair<int, int>>> m_mapFileCoverage;
    // Returns the cached/scanned set of (gx, gy) tiles for which a `.map`
    // file exists on disk under the cache's mapsDir.  Scans once per mapId
    // and memoizes; subsequent calls are O(1).  Returns empty vector when
    // mapsDir is unset or the directory is unreadable.
    [[nodiscard]] std::vector<std::pair<int, int>> const&
    scanMapFileCoverageForMapId(uint32_t mapId);

    // Lazy load + GL upload of a single minimap tile.  Called from
    // the heightmap chunked-build loop alongside the elevation tile.
    // Returns 0 when the PNG is missing or unreadable.
    [[nodiscard]] GLuint loadOrUploadMinimapTile(int gx, int gy);
    void destroyMinimapTextures();
    QOpenGLShaderProgram*      m_heightProgram      = nullptr;
    QOpenGLBuffer              m_heightVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject   m_heightVao;
    int                        m_heightUYpp         = -1;
    int                        m_heightUAnchorW     = -1;
    int                        m_heightUAnchorP     = -1;
    int                        m_heightUViewport    = -1;
    int                        m_heightUTileBounds  = -1;
    int                        m_heightUViewRotation = -1;
    uint32_t                   m_heightmapMapId     = 0;
    bool                       m_heightmapBuilt     = false;
    bool                       m_heightmapPending   = false;
    // Chunked async build state.  First paint after pending=true seeds
    // the queue + index; each subsequent paint processes a small batch
    // and re-schedules via update().  Window stays interactive while a
    // 700+ tile continent streams in instead of freezing 5+ seconds.
    std::vector<std::pair<int, int>> m_heightmapBuildQueue;
    size_t                           m_heightmapBuildIndex = 0;
    bool                             m_heightmapBuilding   = false;

    void destroyHeightmapTextures();

    // ---- Path layer (Phase 4) ----
    QOpenGLShaderProgram*    m_pathProgram = nullptr;
    QOpenGLBuffer            m_pathVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_pathVao;
    GLsizei                  m_pathVertexCount = 0;
    int                      m_pathUYpp     = -1;
    int                      m_pathUAnchorW = -1;
    int                      m_pathUAnchorP = -1;
    int                      m_pathUViewport = -1;
    int                      m_pathUViewRotation = -1;
    bool                     m_pathDirty    = false;
    void rebuildPathBuffers();
    void uploadPathGeometry();

    // Per-node markers for paths -- drawn as small pixel-space quads via
    // the spawn shader so they stay visible at any zoom.  Re-uploaded
    // whenever the path geometry changes OR the drag-ghost moves.
    QOpenGLBuffer            m_pathNodeVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_pathNodeVao;
    GLsizei                  m_pathNodeVertexCount = 0;
    void uploadPathNodeGeometry();

    // Path-node drag state.  When m_draggingPathIdx >= 0, the node at
    // (m_draggingPathIdx, m_draggingNodeIdx) follows the cursor.  On
    // release we emit pathNodeMoved with the new world (X, Y) and let
    // MainWindow update the WaypointModel + snap-to-ground.
    int               m_draggingPathIdx = -1;
    int               m_draggingNodeIdx = -1;
    QPoint            m_pathDragStartScreen;
    coords::WorldPos  m_pathDragCurrentWorld{};
    bool              m_pathDragDidMove = false;

    // ---- Areatrigger / Graveyard markers (Phase 7a) ----
    // Reuses the spawn-icon shader (pixel-space quads with per-vertex
    // color); same SpawnVertex layout.
    QOpenGLBuffer            m_atrVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_atrVao;
    GLsizei                  m_atrVertexCount = 0;
    bool                     m_atrDirty = false;
    void rebuildAreatriggerBuffer();
    void uploadAreatriggerGeometry();
    [[nodiscard]] int hitTestAreatrigger(QPoint const& screen, float pixelTolerance = 7.0f) const;
    // Shape outlines for areatriggers: GL_LINES segments rendered via the
    // path shader (line strips per shape, world-space coords).
    QOpenGLBuffer            m_atrShapeVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_atrShapeVao;
    GLsizei                  m_atrShapeVertexCount = 0;
    void rebuildAreatriggerShapes();

    QOpenGLBuffer            m_gyVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_gyVao;
    GLsizei                  m_gyVertexCount = 0;
    bool                     m_gyDirty = false;
    void rebuildGraveyardBuffer();
    void uploadGraveyardGeometry();

    // ---- Road overlay (Layer::Roads) ----
    // Two separate VBOs/VAOs so the auto-extracted pass (gold) and the
    // handcrafted pass (coral) can be enabled independently and re-
    // uploaded on their own cadence.  Both use a dedicated shader with a
    // uniform color so we don't pay the per-vertex RGBA cost the path
    // shader carries.  Auto vertices are emitted by RoadOverlayBuilder
    // from the loaded dtNavMesh (NAV_AREA_ROAD polys); handcrafted
    // vertices are pushed by an external agent through
    // setHandcraftedRoadPolylines().
    QOpenGLShaderProgram*    m_roadProgram = nullptr;
    int                      m_roadUYpp     = -1;
    int                      m_roadUAnchorW = -1;
    int                      m_roadUAnchorP = -1;
    int                      m_roadUViewport = -1;
    int                      m_roadUViewRotation = -1;
    int                      m_roadUColor   = -1;
    QOpenGLBuffer            m_roadVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_roadVao;
    GLsizei                  m_roadVertexCount = 0;
    QOpenGLBuffer            m_handcraftedRoadVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_handcraftedRoadVao;
    GLsizei                  m_handcraftedRoadVertexCount = 0;
    // CPU-side staging so the build can happen before the GL context
    // exists (mirrors the pattern used by m_pendingVertices).
    std::vector<QVector2D>   m_pendingRoadVertices;
    std::vector<QVector2D>   m_pendingHandcraftedRoadVertices;
    bool                     m_pendingRoadDirty = false;
    bool                     m_pendingHandcraftedRoadDirty = false;

    // ---- Road-graph connectivity diagnostic ----
    // Mirrors the worldserver's HandcraftedRoadGraph: cluster handcrafted
    // segment endpoints within ROAD_NODE_MERGE_EPSILON_YARDS into nodes, then
    // surface degree-1 dangling ends (dead ends) and near-miss gaps (a loose
    // end close to another node but too far to merge) so the operator can see
    // where the authored network does NOT form a routable graph BEFORE bots
    // try to use it. The epsilon MUST match the server's RoadGraph.NodeMerge
    // Epsilon (default 3y) so "looks connected in the editor" == "merges in the
    // bot's graph". Recomputed only when the segment set changes, not per frame.
    static constexpr float ROAD_NODE_MERGE_EPSILON_YARDS = 3.0f;
    static constexpr float ROAD_GAP_WARN_YARDS           = 10.0f;
    void rebuildRoadConnectivityDiagnostic();
    std::vector<QVector2D> m_roadJunctionNodes;  // degree >= 3 (crossroads)
    std::vector<QVector2D> m_roadDanglingNodes;  // degree == 1 (dead ends)
    std::vector<std::pair<QVector2D, QVector2D>> m_roadGapPairs; // near-miss, >=1 dangling
    // Hit-test the actionable diagnostic markers (gap endpoints + dead ends).
    // On hit, fills out the node's TC world XY and returns true.  Junctions are
    // informational and intentionally excluded.
    [[nodiscard]] bool hitTestRoadDiagnostic(QPoint const& screen, float& outWorldX,
                                             float& outWorldY,
                                             float hitRadiusPixels = 9.0f) const;

    // ---- Handcrafted-road segment placement state ----
    SegmentPlacementState m_segmentPlacementState = SegmentPlacementState::None;
    coords::WorldPos      m_segmentStartWorld{};
    // Last-known cursor world coord during WaitingForEnd, used for
    // the in-progress preview line.  Refreshed in mouseMoveEvent.
    coords::WorldPos      m_segmentHoverWorld{};
    // Chain mode: every successful 2-click placement increments this
    // counter and keeps the FSM in WaitingForEnd (with start = previous
    // end) so the operator can rope segments together without re-clicking
    // "Add chain..." between every pair.  Reset to 0 by
    // enterSegmentPlacementMode + cancelSegmentPlacement.
    int                   m_chainSegmentCount = 0;

    // ---- Impact-preview overlay (translucent yellow polygons) ----
    //
    // After previewSegmentImpact runs, the per-polygon triangle fan list
    // is staged here as GL_TRIANGLES vertices in world space.  Painted
    // in the same pass as the nav mesh (re-using its shader / format)
    // immediately AFTER the nav-poly pass so the highlight reads on top.
    // Auto-clears after kImpactPreviewLifetimeMs ms; the timer is bumped
    // every time previewSegmentImpact is called.
    QOpenGLBuffer            m_impactVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_impactVao;
    GLsizei                  m_impactVertexCount = 0;
    bool                     m_impactDirty       = false;
    qint64                   m_impactDeadlineMs  = 0;
    std::vector<Vertex>      m_pendingImpactVerts;
    void uploadImpactGeometry();
    static constexpr qint64  kImpactPreviewLifetimeMs = 10000;
    // Rebuilds m_pendingRoadVertices from the currently loaded navmesh
    // (via RoadOverlayBuilder).  Called automatically from setNavMesh.
    void rebuildAutoRoadOverlay();
    void uploadAutoRoadOverlay();
    void uploadHandcraftedRoadOverlay();

    // Road-graph polyline overlay.  For every pair of Road annotations
    // within `kRoadGraphLinkRadius` yards, emit a GL_LINES segment so
    // the network is visible while the operator drops waypoints.  Uses
    // the existing path shader.  Rebuilt whenever setAnnotations runs.
    QOpenGLBuffer            m_roadGraphVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_roadGraphVao;
    GLsizei                  m_roadGraphVertexCount = 0;
    void rebuildRoadGraphBuffer();

    // Flight-path edge geometry (GL_LINES, path shader).  Nodes are
    // painted via QPainter so the labels stay legible at any zoom; the
    // GL buffer carries only the edge segments.
    std::vector<FlightNode>  m_flightNodes;
    std::vector<FlightEdge>  m_flightEdges;
    QOpenGLBuffer            m_flightEdgeVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_flightEdgeVao;
    GLsizei                  m_flightEdgeVertexCount = 0;
    void rebuildFlightEdgeBuffer();

    // Transport routes: each route is one polyline of WorldPos rendered as
    // GL_LINES segments via the path shader.  Sharing the path shader keeps
    // the GL state-change cost flat with the other line overlays.
    std::vector<std::vector<coords::WorldPos>> m_transportRoutes;
    QOpenGLBuffer            m_transportRouteVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_transportRouteVao;
    GLsizei                  m_transportRouteVertexCount = 0;
    void rebuildTransportRouteBuffer();
    [[nodiscard]] int hitTestGraveyard(QPoint const& screen, float pixelTolerance = 7.0f) const;

    // Spawn drag state.  When m_draggingSpawn >= 0 the spawn under that
    // index follows the cursor; on release the new world (X, Y) is
    // emitted as spawnMoved.  A drag of <DRAG_PIXEL_THRESHOLD pixels
    // collapses to a click for usability.
    int               m_draggingSpawn      = -1;
    QPoint            m_dragStartScreen;
    coords::WorldPos  m_dragCurrentWorld{}; // current ghost position in world coords
    bool              m_dragDidMove        = false;
    static constexpr int DRAG_PIXEL_THRESHOLD = 3;

    // Multi-select: indices (into m_spawns) currently selected.  Always
    // kept normalized (no duplicates, no out-of-range).  Single-click
    // collapses to a 1-element vector; shift+click toggles membership.
    QVector<int>      m_selection;
    // Box-select state.  m_boxSelecting is true while the operator is
    // holding shift and dragging on empty space.  m_boxStartScreen is
    // the press point; the current rect is built from that to the cursor.
    bool              m_boxSelecting      = false;
    QPoint            m_boxStartScreen;
    QPoint            m_boxCurrentScreen;

    void emitSelectionChanged();

    // Annotation GL resources.  Annotations are filled+outlined discs
    // in world units (radius = m_annotations[i].radius yards).  Each
    // disc is a screen-space quad expanded around a world point; the
    // fragment shader computes the disc fill based on world-space
    // distance, so zooming changes the disc size predictably.
    QOpenGLShaderProgram*    m_annotProgram = nullptr;
    QOpenGLBuffer            m_annotVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_annotVao;
    GLsizei                  m_annotVertexCount = 0;
    int                      m_annotUYpp        = -1;
    int                      m_annotUAnchorW    = -1;
    int                      m_annotUAnchorP    = -1;
    int                      m_annotUViewport   = -1;
    int                      m_annotUViewRotation = -1;
    bool                     m_pendingAnnotDirty = false;
};

} // namespace world_editor::render
