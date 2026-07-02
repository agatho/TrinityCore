/*
 * SceneView3D - first-person 3D viewer for navmesh + spawns.
 *
 * Phase 5 MVP: separate QOpenGLWidget from NavMeshView; MainWindow's
 * QStackedWidget swaps between them via the View menu (F2 = 2D, F3 =
 * 3D).  Both widgets share the same data feed (setNavMesh / setSpawns)
 * - MainWindow pushes to both whenever the underlying model changes.
 *
 * Camera: free-fly.
 *   - W/S = forward/back along the camera's look vector.
 *   - A/D = strafe left/right.
 *   - Q/E = down/up in world Z.
 *   - Shift = 5x speed.  Alt = 0.2x speed (slow precision).
 *   - `[` / `]` = halve / double the base fly speed (persisted in QSettings).
 *   - Right-mouse drag = yaw + pitch.
 *   - F = frame camera to mesh AABB.
 *   - Tab = toggle on-screen HUD overlay (tiles drawn, camera position,
 *     height above terrain, fly speed).
 *
 * Coordinate convention: TC world frame (right-handed: +X north, +Y
 * west, +Z up).  Camera position + look stored in TC world coords;
 * shader does the world->view->NDC transform.
 */

#pragma once

#include "../io/BlpReader.h"
#include "../io/MMapReader.h"
#include "../io/VmapReader.h"
#include "NavMeshView.h"  // shares Spawn struct

#include <QImage>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QElapsedTimer>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QTimer>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace world_editor::io { class CascClient; class MapDb2Lookup; struct Wdt; struct AdtChunk; }

namespace world_editor::render
{

class TerrainTextureCache;


// STAGE B: QOpenGLExtraFunctions (superset of QOpenGLFunctions) is required for
// glTexImage3D / glTexSubImage3D + the GL_TEXTURE_2D_ARRAY upload path used by
// the 8-layer terrain alpha / height arrays.  Every existing 2D call still
// resolves through the inherited QOpenGLFunctions subset.
class SceneView3D final : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    explicit SceneView3D(QWidget* parent = nullptr);
    ~SceneView3D() override;

    // Same setter signatures as NavMeshView; MainWindow pushes data to
    // both viewers in lockstep.
    void setNavMesh(io::LoadedMMap mesh);
    void setSpawns(std::vector<Spawn> spawns);
    void setAnnotations(std::vector<Annotation> annotations);
    void setPaths(std::vector<Path> paths);
    // Bot dungeon-route chain (playerbot_dungeon_routes): rendered as a
    // distinct gold polyline with node markers, node-editable like paths.
    // One Path per difficulty; Path::pathId carries the difficulty.
    void setDungeonRoutes(std::vector<Path> routes);
    void setDungeonRoutesVisible(bool on);
    [[nodiscard]] bool dungeonRoutesVisible() const noexcept { return m_routesVisible; }
    // Camera world position (used to seed a new dungeon route at the operator's
    // current vantage point).
    [[nodiscard]] QVector3D cameraPosition() const { return { m_camX, m_camY, m_camZ }; }

    // ---- Pathfinding probe (bot-budget sandbox) ----
    // Runs Detour A->B on the loaded navmesh with the BOT'S constraints
    // (1024 query nodes, 74-poly corridor cap -- the worldserver
    // PathGenerator budget) and stores an overlay: green = complete,
    // orange = partial/truncated corridor, red dashed = unreached remainder.
    // The HUD prints the verdict + budget consumption.  This is a diagnosis
    // tool: it shows what the BOT's pathfinder would do, unlike the 2D
    // auto-route helper which uses a generous 4096-node authoring query.
    void runPathProbe(QVector3D const& startTc, QVector3D const& endTc);
    void clearPathProbe();
    [[nodiscard]] bool hasPathProbeResult() const noexcept { return m_probeValid; }

    // ---- Off-mesh connection overlay + authoring preview ----
    // Existing connections are read straight from the loaded dtNavMesh
    // tiles (violet arcs).  Newly authored ones (appended to offmesh.txt
    // but not yet baked by a regen) are queued as PENDING and drawn
    // brighter so the operator can tell live links from queued ones.
    void setOffmeshVisible(bool on);
    [[nodiscard]] bool offmeshVisible() const noexcept { return m_offmeshVisible; }
    void addPendingOffmesh(QVector3D const& fromTc, QVector3D const& toTc);
    void clearPendingOffmesh();
    void setAreatriggers(std::vector<Areatrigger> atrs);
    void setGraveyards(std::vector<Graveyard> gys);
    void setMapTileCache(io::MapTileCache* cache);
    void rebuildHeightmapTerrain(uint32_t mapId);
    // Phase 5 stretch goal: load standalone vmaps (WMO collision triangles)
    // and render them as a translucent overlay so building interiors and
    // bridges show up in 3D.  Pass an empty/ok-false LoadedVmap to clear.
    void setVmapMesh(io::LoadedVmap mesh);
    // Visibility toggle for the WMO triangle overlay (default = on).
    void setWmoVisible(bool on);
    [[nodiscard]] bool wmoVisible() const noexcept { return m_wmoVisible; }

    // Doodad layer toggle (M2 props -- trees / mailboxes / fences / etc).
    // Only takes effect when the realistic-textures pass is also on.
    void setDoodadsVisible(bool on);
    [[nodiscard]] bool doodadsVisible() const noexcept { return m_doodadsVisible; }

    // Textured WMO layer toggle (real wall/floor/ceiling/bridge geometry).
    // Only takes effect when the realistic-textures pass is also on; when
    // off (or when no client WMO data loads) the translucent collision /
    // flat-lit fallback draws instead.
    void setTexturedWmosVisible(bool on);
    [[nodiscard]] bool texturedWmosVisible() const noexcept { return m_texturedWmosVisible; }

    // Generic per-layer visibility, mirroring NavMeshView::setLayerVisible
    // so MainWindow's existing View-menu toggles can drive BOTH viewers.
    // Currently gates the spawn / areatrigger / graveyard / path /
    // annotation passes in the 3D view (the colored overlay dots that
    // otherwise drown out the terrain).  Unknown layers are ignored.
    void setLayerVisible(Layer layer, bool visible);
    [[nodiscard]] bool isLayerVisible(Layer layer) const noexcept;

    // Verbose per-tile / per-chunk diagnostic logging.  OFF by default
    // (persisted via the View menu).  When on, the ADT load path emits a
    // per-tile height/MCVT summary + per-pass inventory so terrain decode
    // problems can be diagnosed from debug.log without a rebuild.  Routed
    // through this flag so production sessions stay quiet.
    void setVerboseLogging(bool on) noexcept { m_verboseLogging = on; }
    [[nodiscard]] bool verboseLogging() const noexcept { return m_verboseLogging; }

    // Atmospheric layer: sky dome drawn before terrain with depth-write off,
    // distance fog mixed into the lit terrain / ADT / WMO / doodad passes.
    // All three follow the realistic-mode toggle; no realistic mode = no
    // sky + no fog so the flat-shading fallback view stays untouched.
    void setSkyVisible(bool on);
    [[nodiscard]] bool skyVisible() const noexcept { return m_skyVisible; }
    void setFogEnabled(bool on);
    [[nodiscard]] bool fogEnabled() const noexcept { return m_fogEnabled; }
    // Liquid (water / ocean / magma / slime) surface pass.  Default on;
    // gated by realistic-mode so the flat-shaded view stays unaffected.
    void setWaterVisible(bool on);
    [[nodiscard]] bool waterVisible() const noexcept { return m_waterVisible; }
    // Hour of the day in [0, 24).  Drives sun direction, sky gradient, fog
    // tint and ambient level via an internal per-hour LUT.  Default 12.0.
    void setTimeOfDay(float hours);
    [[nodiscard]] float timeOfDay() const noexcept { return m_timeOfDay; }

    // Retail-client-like textured + lit terrain + WMO pass.  When ON the
    // terrain renders as a per-tile lit textured surface (minimap PNG /
    // CASC BLP projected on the heightmap) and WMO faces are flat-lit
    // with a per-group hash tint; when OFF the legacy per-vertex
    // greyscale terrain + flat semi-transparent WMO overlay are used.
    void setRealistic(bool on);
    [[nodiscard]] bool realistic() const noexcept { return m_realistic; }

    // Late-bound CASC client + Map.db2 lookup, mirroring NavMeshView.
    // Used by the textured-terrain pass to fetch BLP minimap tiles
    // when no PNG is found on disk.  Both pointers are BORROWED.
    void setCascClient(io::CascClient* casc, io::MapDb2Lookup* mapDb2);
    // Late-bound minimap PNG root.  When set, the textured terrain pass
    // tries <minimap_dir>/<mapId>/map<gx>_<gy>.png (and conventional
    // variants) before falling back to CASC.
    void setMinimapDir(QString const& dir);

    void frameMesh();

    // Headless/screenshot support: set the free-fly camera pose directly
    // (world XYZ + yaw/pitch in radians).  Used by MainWindow::renderToPng to
    // pose the camera for an offscreen render without mouse/keyboard input.
    void setCamera(float x, float y, float z, float yawRad, float pitchRad);

    // ---- Click-to-place (3D authoring) ----
    //
    // When placement mode is on, a left-click does NOT select an existing
    // marker; instead it depth-unprojects the clicked pixel to the exact
    // world (x, y, z) on whatever solid geometry is under the cursor (terrain
    // / WMO floor / doodad) and emits placementRequested with that position.
    // Crucially the Z is the *authored* surface height the operator actually
    // clicked -- it is stored on the row, never recomputed at runtime (which
    // is ambiguous on bridges, multi-floor WMOs and overhangs).  This is the
    // 3D analogue of NavMeshView's 2D placement mode, which can only supply
    // X/Y from a top-down click.
    void setPlacementMode(bool on) noexcept { m_placementMode = on; }
    [[nodiscard]] bool placementMode() const noexcept { return m_placementMode; }

    // Animate the camera along the given path at `velocity` yards/sec.
    // Camera position interpolates between consecutive nodes; yaw points
    // toward the upcoming node so the operator sees the path "from the
    // ground" as a moving NPC would.  Stop with stopPathPlayback or Esc.
    //
    // velocity <= 0 falls back to the path's recorded velocity (if > 0)
    // or 5 y/s as a final fallback.
    void startPathPlayback(Path const& p, float velocity = 0.0f);
    void stopPathPlayback();
    [[nodiscard]] bool isPlayingPath() const noexcept { return m_playingPath; }

    // Returns the GL version string captured during initializeGL(); empty
    // until the GL context comes up.  Used by the About dialog.
    [[nodiscard]] QString glVersionString() const { return m_glVersionString; }
    [[nodiscard]] QString glRendererString() const { return m_glRendererString; }

    // ---- Async first-tile load payload types (public so the worker tasks
    // defined in the .cpp anonymous namespace can name them in their
    // run() signatures; private mutexes still gate the actual enqueue) ----
    struct AdtTerrainVertexPub { float x, y, z; float u, v; float nx, ny, nz; uint8_t r, g, b, a; };
    struct AdtChunkCpuPayload
    {
        std::vector<AdtTerrainVertexPub> verts;        // 145 unique verts (81 V9 + 64 V8)
        std::vector<uint16_t>            indices;      // 256 tris * 3, hole-culled
        // STAGE B: 7 contiguous R8 64x64 planes (one per layer 1..7), zero-
        // filled for absent layers so they contribute 0 coverage.  64*64*7.
        std::vector<uint8_t>             alphaPlanes;   // 64*64*7 R8 or empty
        bool                             hasAlpha   = false;
        int                              layerCount = 0;
        // Per-layer parallax metadata carried from MTXP (B0).  Texture handles
        // are still resolved on the GL thread; these are pure CPU values.
        float                            layerScale[8]   = { 1,1,1,1,1,1,1,1 };
        float                            heightScale[8]  = { 0,0,0,0,0,0,0,0 };
        float                            heightOffset[8] = { 1,1,1,1,1,1,1,1 };
        bool                             heightBlend     = false; // any non-neutral MTXP height
        // PERF: per-layer diffuse texture refs carried from the worker so the
        // GL drainer resolves textures WITHOUT re-decoding the whole ADT off
        // CASC (the old path re-parsed every tile's MCVT+MCAL on the render
        // thread purely to recover these -- the dominant per-frame stall).
        uint32_t                         layerFdid[8]    = { 0,0,0,0,0,0,0,0 };
        std::string                      layerPath[8];   // BLP-path fallback (legacy ADTs)
    };
    // PERF: textures the worker already CASC-read + BLP-decoded because the
    // shared cache didn't know the key at dispatch time.  The GL drain feeds
    // these to TerrainTextureCache::insertDecoded* (upload-only) BEFORE
    // resolving handles, so first-appearance textures no longer trigger a
    // synchronous CASC read + DXT decode on the render thread.
    using DecodedFdidTex = std::vector<std::pair<uint32_t, io::BlpImage>>;
    using DecodedPathTex = std::vector<std::pair<std::string, io::BlpImage>>;
    struct AdtTilePending
    {
        int                              gx = 0;
        int                              gy = 0;
        std::vector<AdtChunkCpuPayload>  chunks;
        bool                             ok = false;
        // Tile Z extent, computed on the worker so the GL drainer needn't keep
        // a re-decoded AdtTile around just for the bounding sphere.
        float                            tileMinZ = 0.0f;
        float                            tileMaxZ = 0.0f;
        DecodedFdidTex                   decodedFdidTex;   // deduped across chunks
        DecodedPathTex                   decodedPathTex;
    };
    // PERF: async water decode payload.  loadAdtLiquid (full ADT-root read +
    // MH2O parse, ~70ms/tile) used to run SYNCHRONOUSLY on the GL thread inside
    // streamWaterTiles and stalled every frame while flying.  A WaterLoadTask
    // now decodes off-thread and posts these CPU vertex arrays; the GL thread
    // only uploads the VBOs (~1ms).
    struct WaterVertex { float x, y, z; float kind; };
    struct WaterChunkCpu { std::vector<WaterVertex> verts; };
    struct WaterTilePending
    {
        int   gx = 0, gy = 0;
        bool  ok = false;
        std::vector<WaterChunkCpu> chunks;
        float minZ = 0.0f, maxZ = 0.0f;
    };
    struct MinimapPending
    {
        int             gx = 0;
        int             gy = 0;
        QImage          img;
        bool            ok = false;
    };
    // ---- STAGE B1: async WMO / M2 mesh payloads ----
    //
    // The worker (CASC read + io::loadWmo / io::loadM2 decode) builds a
    // render-ready CPU payload: the interleaved vertex array + index array +
    // per-submesh metadata (including the texture FileDataId / path -- the
    // BLP resolve + GL texture upload stays on the GL drain side, exactly like
    // the ADT layer textures) + the model-local bounding sphere.  NO GL handles
    // are touched on the worker.
    struct WmoCpuVertex   { float x, y, z; float nx, ny, nz; float u, v; uint8_t r, g, b, a; };
    // STAGE A: 10 floats / vertex (uv1 added for the Diffuse_T1_T2 combiner's
    // second texture).  Layout matches io::M2Mesh::vertices 1:1 so the worker
    // copy stays a flat memcpy.
    struct DoodadCpuVertex { float x, y, z; float nx, ny, nz; float u, v; float u2, v2; };
    struct WmoCpuSubMesh
    {
        uint32_t    indexStart        = 0;
        uint32_t    indexCount        = 0;
        uint8_t     blendMode         = 0;
        bool        interior          = false;
        uint32_t    textureFileDataId = 0;   // resolved -> GL handle on the drain.
        std::string texturePath;             // legacy fallback when fdid == 0.
    };
    struct DoodadCpuSubMesh
    {
        uint32_t    indexStart         = 0;
        uint32_t    indexCount         = 0;
        uint8_t     blendMode          = 0;
        uint32_t    textureFileDataId  = 0;
        std::string texturePath;
        uint32_t    textureFileDataId2 = 0;   // STAGE A: tex1; 0 -> no second texture.
        std::string texturePath2;             // tex1 legacy fallback.
        uint8_t     combinerId         = 0;   // io::M2Combiner; 0 = Opaque.
    };
    struct WmoPending
    {
        uint32_t                    fdid = 0;
        std::vector<WmoCpuVertex>   verts;
        std::vector<uint32_t>       indices;
        std::vector<WmoCpuSubMesh>  subMeshes;
        float                       centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
        float                       radius  = 0.0f;
        bool                        ok = false;
        DecodedFdidTex              decodedFdidTex;   // worker-pre-decoded submesh textures
        DecodedPathTex              decodedPathTex;
    };
    struct DoodadPending
    {
        uint32_t                       fdid = 0;
        std::vector<DoodadCpuVertex>   verts;
        std::vector<uint32_t>          indices;
        std::vector<DoodadCpuSubMesh>  subMeshes;
        float                          centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
        float                          radius  = 0.0f;
        bool                           ok = false;
        DecodedFdidTex                 decodedFdidTex;   // worker-pre-decoded submesh textures
        DecodedPathTex                 decodedPathTex;
    };
    // PERF: async prop-placement enumeration.  loadAdtDoodads /
    // loadAdtWmoPlacements / loadWmoDoodads are synchronous CASC reads that
    // used to run on the GL thread inside paintGL at every camera tile-cross
    // (rebuildDoodadInstances) -- a render hitch each time you fly across a
    // tile boundary.  A PropPlacementTask now enumerates placements on a
    // worker and posts back this payload; applyPropPlacements (GUI thread)
    // appends the instances.  Jobs are prepared on the GL thread (WDT MAID
    // lookups are cheap) so the worker never touches SceneView3D state.
    struct PropPlacementJob { int gx = 0, gy = 0; uint32_t obj0Fdid = 0, rootFdid = 0; };
    struct PropDoodadInstance
    {
        uint32_t modelFdid = 0;
        float    x = 0.0f, y = 0.0f, z = 0.0f;
        float    rotZ = 0.0f, rotY = 0.0f, rotX = 0.0f;
        float    scale = 1.0f;
    };
    struct PropWmoInstance
    {
        uint32_t wmoRootFdid = 0;
        float    x = 0.0f, y = 0.0f, z = 0.0f;
        float    rotZ = 0.0f, rotY = 0.0f, rotX = 0.0f;
        float    scale = 1.0f;
    };
    struct PropPlacementPending
    {
        std::vector<PropDoodadInstance> doodads;
        std::vector<PropWmoInstance>    wmos;
        std::vector<uint32_t>           wmoUids;     // MODF uniqueIds emitted here
        uint32_t                        generation = 0;
    };
    void applyPropPlacements(PropPlacementPending pending);
    // Worker -> GL thread inbox.  Called from a queued lambda after the
    // QRunnable finishes; the GL drainer pulls + uploads inside paintGL.
    void enqueueAdtPending    (AdtTilePending  pending);
    void enqueueWaterPending  (WaterTilePending pending);
    void enqueueMinimapPending(MinimapPending  pending);
    void enqueueWmoPending    (WmoPending      pending);
    void enqueueDoodadPending (DoodadPending   pending);

signals:
    // Mirrors NavMeshView: ray-vs-sphere pick of the spawn list.
    void spawnClicked(int spawnIndex);
    // 3D drag-to-move: emitted on release after dragging an existing spawn
    // across its horizontal plane.  Carries the new world position incl. the
    // drag-plane Z (constant altitude) so MainWindow keeps flying mobs aloft
    // when snap-to-ground is off, or re-grounds them when it's on.  Distinct
    // arity from NavMeshView::spawnMoved (which is XY-only, top-down).
    void spawnMoved(int spawnIndex, float worldX, float worldY, float worldZ);
    // Phase 5 polish: ray-pick for paths and annotations in 3D, mirroring
    // NavMeshView::pathClicked / annotationClicked.
    void pathClicked(int pathIndex);
    void annotationClicked(int annotationIndex);
    // Node-level path editing in 3D (parity with NavMeshView).  Unlike the
    // 2D signals these carry Z: the move Z is the drag-plane altitude and
    // the segment Z is lerped between the segment's endpoints, so stacked
    // dungeon floors don't snap edits onto the wrong storey.
    void pathNodeClicked(int pathIndex, int nodeIndex);
    void pathNodeMoved(int pathIndex, int nodeIndex,
                       float worldX, float worldY, float worldZ);
    void pathNodeContextMenuRequested(int pathIndex, int nodeIndex, QPoint globalPos);
    void pathSegmentContextMenuRequested(int pathIndex, int afterNodeIndex,
                                         float worldX, float worldY, float worldZ,
                                         QPoint globalPos);
    // Same node-editing surface for the bot dungeon-route chain.  routeIndex
    // indexes the setDungeonRoutes vector (one entry per difficulty).
    void routeNodeMoved(int routeIndex, int nodeIndex,
                        float worldX, float worldY, float worldZ);
    void routeNodeContextMenuRequested(int routeIndex, int nodeIndex, QPoint globalPos);
    void routeSegmentContextMenuRequested(int routeIndex, int afterNodeIndex,
                                          float worldX, float worldY, float worldZ,
                                          QPoint globalPos);
    // Phase 7+ polish: ray-pick areatriggers + graveyards in 3D.
    void areatriggerClicked(int areatriggerIndex);
    void graveyardClicked(int graveyardIndex);
    // Path playback progress events.  `current` is the most recently-
    // reached node (1-based); `total` is the node count.
    void pathPlaybackTick(int current, int total);
    void pathPlaybackFinished();
    // Emitted while placementMode() is true and the operator left-clicks on
    // solid geometry.  (x, y) match NavMeshView::placementRequested; z is the
    // depth-unprojected surface height under the cursor (authored, stored).
    void placementRequested(float worldX, float worldY, float worldZ);
    // Emitted instead of placementRequested when a placement-mode click lands
    // on sky / the far plane (no geometry to anchor to).  Lets MainWindow tell
    // the operator to aim at terrain rather than leaving the click silent.
    void placementMissed();
    // Placement-mode left-click: the world-space pick ray (origin + non-unit
    // direction).  MainWindow marches it against the authoritative terrain/WMO
    // height (snapToGround) to resolve the surface point -- depth-buffer
    // readback proved unreliable here, so we hand off geometry instead.
    void placementRayRequested(float ox, float oy, float oz,
                               float dx, float dy, float dz);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void onTick();

private:
    void rebuildNavmeshBuffer();
    void rebuildSpawnBuffer();
    void uploadNavmeshGeometry();
    void uploadSpawnGeometry();
    [[nodiscard]] QMatrix4x4 viewMatrix() const;
    [[nodiscard]] QMatrix4x4 projectionMatrix() const;

    io::LoadedMMap          m_mesh;
    std::vector<Spawn>      m_spawns;
    std::vector<Annotation> m_annotations;
    std::vector<Path>       m_paths;
    io::MapTileCache*       m_mapCache = nullptr;
    uint32_t                m_heightmapMapId = 0;
    bool                    m_heightmapPending = false;

    // GL: navmesh triangles in 3D.
    struct NavVertex { float x, y, z; uint8_t r, g, b, a; };
    QOpenGLShaderProgram*    m_navProgram = nullptr;
    QOpenGLBuffer            m_navVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_navVao;
    GLsizei                  m_navVertexCount = 0;
    int                      m_navUMvp        = -1;

    // GL: spawn billboards.  Each spawn is 6 vertices (2 tris) carrying
    // the spawn's world XYZ plus a unit-quad corner (in screen-space);
    // the vertex shader places the spawn point through MVP then offsets
    // the quad corner by a constant pixel scale in clip space.
    struct SpawnVertex { float x, y, z; float ox, oy; uint8_t r, g, b, a; };
    QOpenGLShaderProgram*    m_spawnProgram = nullptr;
    QOpenGLBuffer            m_spawnVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_spawnVao;
    GLsizei                  m_spawnVertexCount = 0;
    int                      m_spawnUMvp        = -1;
    int                      m_spawnUViewport   = -1;
    int                      m_spawnUPixelSize  = -1;

    // Camera state (TC world frame).
    float m_camX = 0.0f, m_camY = 0.0f, m_camZ = 200.0f;
    float m_yaw   = 0.0f;     // around world Z, radians, 0 = facing +X
    float m_pitch = -0.6f;    // negative = looking down

    // Input state.
    QSet<int> m_keys;
    bool      m_rotating = false;
    QPoint    m_lastMouse;
    QTimer    m_tickTimer;

    // Mesh AABB cached for frameMesh().
    float m_meshMinX = 0.0f, m_meshMaxX = 0.0f;
    float m_meshMinY = 0.0f, m_meshMaxY = 0.0f;
    float m_meshMinZ = 0.0f, m_meshMaxZ = 0.0f;
    bool  m_meshBoundsValid = false;

    bool m_buffersReady = false;
    bool m_navDirty     = false;
    bool m_spawnDirty   = false;

    // GL identity strings captured once at initializeGL() so the About
    // dialog can read them without needing the GL context current.
    QString m_glVersionString;
    QString m_glRendererString;

    // ---- Path playback ----
    // First-person fly-through of a waypoint_path.  Updated each tick;
    // m_playbackSegment is the index of the node we're moving FROM, so
    // [seg, seg+1] is the active line segment.  m_playbackT is the
    // [0..1] interpolation parameter within that segment.  Stop on Esc
    // or when the last segment completes.
    bool                  m_playingPath      = false;
    std::vector<PathNode> m_playbackNodes;
    float                 m_playbackVelocity = 5.0f;
    int                   m_playbackSegment  = 0;
    float                 m_playbackT        = 0.0f;
    qint64                m_playbackLastMs   = 0;
    // Whether the rotating control should be ignored while playback
    // is active (we hijack the camera so RMB-drag yaw would fight us).
    bool                  m_playbackHadCursor = false;

    // Terrain mesh (per-tile triangulated heightmap).
    struct TerrainVertex { float x, y, z; uint8_t r, g, b, a; };
    QOpenGLShaderProgram*    m_terrainProgram = nullptr;
    QOpenGLBuffer            m_terrainVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_terrainVao;
    GLsizei                  m_terrainVertexCount = 0;
    int                      m_terrainUMvp = -1;

    // Realistic (textured + lit) terrain pipeline.  One VBO per tile
    // because each tile binds its own texture; we do N draw calls per
    // paint instead of a single batched call.  Vertex carries world
    // XYZ + UV + normal so the fragment shader can do a Lambert
    // shading + texture lookup.  When the per-tile texture is missing
    // (no PNG, no CASC) we fall back to greyscale elevation shading
    // via a uniform flag instead of the texture lookup.
    struct LitTerrainVertex { float x, y, z; float u, v; float nx, ny, nz; uint8_t r, g, b, a; };
    struct LitTerrainTile
    {
        int                                gx = 0;
        int                                gy = 0;
        float                              minX = 0.0f, maxX = 0.0f;
        float                              minY = 0.0f, maxY = 0.0f;
        // Real per-tile Z extent (from the tile's heightmap samples).  Used
        // for the frustum cull sphere -- the continent-global mesh mid-Z was
        // wrong (one outlier tile inflated it -> every sphere centred off the
        // terrain -> 100% false-culled -> black terrain).
        float                              minZ = 0.0f, maxZ = 0.0f;
        std::unique_ptr<QOpenGLBuffer>             vbo;
        std::unique_ptr<QOpenGLVertexArrayObject>  vao;
        GLsizei                            vertexCount = 0;
        GLuint                             texture    = 0;   // 0 = no texture, draw fallback
    };
    QOpenGLShaderProgram*    m_litTerrainProgram = nullptr;
    std::vector<LitTerrainTile> m_litTerrainTiles;
    int                      m_litTerrainUMvp      = -1;
    int                      m_litTerrainUSunDir   = -1;
    int                      m_litTerrainUAmbient  = -1;
    int                      m_litTerrainUHasTex   = -1;
    int                      m_litTerrainUTexture  = -1;

    // Realistic WMO pipeline.  Same shader as the lit terrain but with
    // no UV (`u_hasTexture=0` always), tinted by a deterministic hash
    // of the centroid bucket so adjacent walls of the same building
    // share a colour and distinct buildings read distinctly.  Per-
    // vertex normals are flat (replicated per triangle).
    struct LitWmoVertex { float x, y, z; float nx, ny, nz; uint8_t r, g, b, a; };
    QOpenGLBuffer            m_litWmoVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_litWmoVao;
    GLsizei                  m_litWmoVertexCount = 0;
    bool                     m_litWmoDirty       = false;
    void rebuildLitWmoBuffer();
    void uploadLitWmoGeometry();

    // ---- Realistic ADT terrain (alpha-blended 4-texture composite) ----
    //
    // When realistic mode is on AND a CASC client is available we try
    // to load each tile's ADT and render the client-style alpha-blended
    // terrain.  Each MCNK chunk gets its own draw call (9x9 verts, 8x8
    // quads = 128 triangles) bound to up to 4 terrain textures + one
    // RGB packed alpha-map texture (channels = layer-1/2/3 alphas).
    //
    // Per-tile load is lazy on the first paint after the toggle flips
    // on / heightmap changes.  Tiles that fail to load fall through to
    // the minimap-projection LitTerrainTile path; tiles that succeed
    // replace it.
    struct AdtTerrainVertex { float x, y, z; float u, v; float nx, ny, nz; uint8_t r, g, b, a; };
    struct AdtChunkRender
    {
        std::unique_ptr<QOpenGLBuffer>            vbo;
        std::unique_ptr<QOpenGLBuffer>            ebo;   // index buffer (145-vert mesh)
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        GLsizei vertexCount = 0;                  // sync path: glDrawArrays
        GLsizei indexCount  = 0;                  // async path: glDrawElements
        // STAGE B: up to 8 diffuse layers.  layerTex[] holds BORROWED cache
        // handles (deduped per chunk -- a texture used by >1 layer occupies one
        // GPU sampler slot; slotForLayer[] maps each layer to its sampler unit).
        GLuint  layerTex[8]      = { 0, 0, 0, 0, 0, 0, 0, 0 }; // borrowed cache handles, deduped
        int     slotForLayer[8]  = { 0, 0, 0, 0, 0, 0, 0, 0 }; // layer i -> diffuse sampler slot
        int     slotCount        = 0;                          // # distinct diffuse samplers (<=8)
        GLuint  alphaArray  = 0;                // OWNED: GL_TEXTURE_2D_ARRAY R8 64x64x7 (alpha layers 1..7)
        GLuint  heightArray = 0;                // OWNED: GL_TEXTURE_2D_ARRAY R8 ...x8 (effective height per layer); 0 => plain-mix
        // Per-layer UV scale / parallax params, surfaced from MTXP (STAGE B).
        float   layerScale[8]   = { 1,1,1,1,1,1,1,1 };
        float   heightScale[8]  = { 0,0,0,0,0,0,0,0 };
        float   heightOffset[8] = { 1,1,1,1,1,1,1,1 };
        int     layerCount  = 0;                // 1..8
        bool    heightBlend = false;            // height-weighted blend available (B2)
    };
    struct AdtTileRender
    {
        int gx = 0;
        int gy = 0;
        std::vector<AdtChunkRender> chunks;     // 256 entries max (one per MCNK)
        bool loadAttempted = false;             // true after we tried; if !loaded it stays false-positive
        bool loaded        = false;             // false => fall through to minimap pass
        // Actual world-Z extent of this tile's geometry.  Used to build a
        // correct per-tile frustum-cull sphere -- a continent-wide mid-Z
        // wrongly culls high/low tiles (Teldrassil plateau, deep ocean).
        float minZ = 0.0f;
        float maxZ = 0.0f;
    };
    QOpenGLShaderProgram*    m_adtTerrainProgram = nullptr;
    std::vector<AdtTileRender> m_adtTerrainTiles;
    bool                     m_adtTerrainHidden = false; // diag key 3
    bool                     m_verboseLogging   = false; // View menu / settings
    // Camera-distance draw radius (yards) for ADT terrain.  The whole map
    // is loaded for complete coverage, but only tiles within this radius
    // of the camera are drawn -- bounds overdraw and hides distant terrain
    // the game would fog out.  ~8 tiles.  [ / ] could tune later.
    float                    m_drawRadiusYards  = 4266.6f;
    // Effective render/stream radius.  Fog HARD-clamps every fragment beyond
    // m_fogEnd to solid fogColor (invisible), so streaming + drawing tiles past
    // it is pure waste -- it produces identical pixels to the nearer fog while
    // multiplying draw calls and GL uploads.  Cap the radius ~1.5 tiles past
    // fogEnd (so tiles straddling the fog line still draw their near, visible
    // half) while fog is on; use the full configured radius when fog is off.
    [[nodiscard]] float renderRadiusYards() const noexcept
    {
        float const fogR = m_fogEnd + 800.0f;
        return (m_fogEnabled && fogR < m_drawRadiusYards) ? fogR : m_drawRadiusYards;
    }
    // Per-layer visibility for the overlay passes (spawns / areatriggers /
    // graveyards / paths / annotations).  Defaults all-visible; driven by
    // MainWindow's View-menu layer toggles via setLayerVisible().
    bool                     m_layer3dVisible[size_t(Layer::_Count)];
    std::unique_ptr<TerrainTextureCache> m_terrainTextureCache;
    int  m_adtUMvp        = -1;
    int  m_adtUSunDir     = -1;
    int  m_adtUAmbient    = -1;
    int  m_adtULayerCount = -1;
    int  m_adtUHasAlpha   = -1;
    int  m_adtULayerTex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    int  m_adtUAlphaArray   = -1;   // sampler2DArray (was m_adtUAlphaTex)
    // STAGE B parallax uniforms.
    int  m_adtUSlotForLayer = -1;   // int[8]: layer i -> diffuse sampler slot
    int  m_adtULayerScale   = -1;   // float[8]
    int  m_adtUHeightArray  = -1;   // sampler2DArray (unit 9)
    int  m_adtUHeightScale  = -1;   // float[8]
    int  m_adtUHeightOffset = -1;   // float[8]
    int  m_adtUHeightBlend  = -1;   // int flag
    GLint m_glMaxTexImageUnits = 0; // GL_MAX_TEXTURE_IMAGE_UNITS (fragment stage)
    void destroyAdtTerrainTiles();
    void rebuildAdtTerrainTiles();
    // Try to load a single ADT tile + upload its GL buffers.  Returns
    // true when at least one chunk produced renderable geometry.
    bool loadAndUploadAdtTile(int gx, int gy, AdtTileRender& out);

    // STAGE B shared chunk-texture setup (used by BOTH the async GL drainer and
    // the sync first-paint path so the dedup / alpha-array logic lives once).
    //   * Resolves up to 8 diffuse layers through the cache, de-duplicating to
    //     <=8 GPU sampler slots (a texture used by multiple layers shares one
    //     slot); fills ch.layerTex[], ch.slotForLayer[], ch.slotCount.
    //   * Zeroes the alpha plane of any layer whose diffuse failed to resolve
    //     (drops middle holes, not just trailing ones).
    //   * Builds the OWNED R8 GL_TEXTURE_2D_ARRAY alpha (7 slices) from the
    //     packed planes; copies per-layer scale/offset metadata.
    // Returns true when layer 0 resolved (chunk is renderable as terrain).
    // `alphaPlanes` may be empty (no MCAL) -> no alpha array, plain base layer.
    // `layerFdid`/`layerPath` supply each layer's diffuse texture ref (FDID
    // preferred, BLP path fallback).  Both the sync loader and the async GL
    // drainer pass these from data they already hold -- the drainer no longer
    // re-decodes the ADT off CASC just to recover them.
    bool buildAdtChunkTextures(int layerCount,
                               uint32_t const layerFdid[8],
                               std::string const layerPath[8],
                               std::vector<uint8_t>& alphaPlanes,
                               bool hasAlpha,
                               float const layerScale[8],
                               float const heightScale[8],
                               float const heightOffset[8],
                               bool heightBlend,
                               AdtChunkRender& ch);

    // ---- Realistic liquid surface pass (MH2O / MCLQ) ----
    //
    // For every navmesh-covered ADT tile we parse the liquid chunks
    // (MH2O for Cata+, MCLQ for vanilla) and build one VBO per chunk:
    // the 8x8 quad mesh derived from the exists-bitmap, each vertex
    // carrying world XYZ + a Kind enum used by the fragment shader to
    // pick base colour + Lambert toggling.  Translucent alpha-blended
    // pass; drawn AFTER terrain / WMO / doodads so opaque geometry is
    // visible through the water and we don't need depth sorting between
    // surfaces (back-to-front sort would cost more than the artefacts
    // it would hide).
    // WaterVertex is declared up with the async-water payload types.
    struct WaterChunkGpu
    {
        std::unique_ptr<QOpenGLBuffer>            vbo;
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        GLsizei vertexCount = 0;
    };
    struct WaterTile
    {
        int gx = 0;
        int gy = 0;
        std::vector<WaterChunkGpu> chunks;
        bool loadAttempted = false;
        bool hasGeometry   = false;
        // Real per-tile Z extent of the emitted (rendered) water surface,
        // accumulated across ALL liquid chunks of the tile while building
        // verts.  Used by the frustum-cull sphere instead of the coarse
        // continent-wide mid-Z so high/low water bodies aren't mis-culled.
        // Left at 0/0 when the tile has no renderable geometry (it is then
        // skipped via hasGeometry=false, so the default is harmless).
        float minZ = 0.0f;
        float maxZ = 0.0f;
    };
    QOpenGLShaderProgram*    m_waterProgram = nullptr;
    std::vector<WaterTile>   m_waterTiles;
    int  m_waterUMvp     = -1;
    int  m_waterUTime    = -1;
    int  m_waterUSkyTint = -1;
    bool m_waterVisible  = true;
    void destroyWaterTiles();
    void rebuildWaterTiles();
    bool loadAndUploadWaterTile(int gx, int gy, WaterTile& out);
    // STAGE B2/C2: camera-relative water streaming.  Instead of one-shot
    // building every navmesh tile's water on the first paint (a large hitch on
    // a continent), dispatch only the tiles inside the camera's load ring,
    // budgeting at most N *builds* per frame; evict water tiles the camera has
    // left behind.  Mirrors the ADT terrain residency model (dispatchAdtTileLoads
    // / unloadFarAdtTiles) but the build stays synchronous on the GL thread
    // (loadAndUploadWaterTile does GL uploads) -- the budget alone removes the
    // hitch since each tile's MH2O read is cheap relative to an ADT decode.
    void streamWaterTiles(int maxBuildsThisFrame);
    void unloadFarWaterTiles();
    // Camera tile at the last water re-scan; re-scan only on tile cross
    // (sentinel forces a scan on the first frame + after a map switch, which
    // resets these via destroyWaterTiles).
    int  m_waterStreamCamGx = -100000;
    int  m_waterStreamCamGy = -100000;
    // (gx,gy) of in-ring tiles enumerated at the last tile-cross but not yet
    // built; drained at most kUploadBudgetWater per frame so a continent's
    // worth of water spreads over many frames instead of one hitch.  Sorted
    // nearest-first when refilled.
    std::vector<std::pair<int, int>> m_waterPendingBuild;

    // ---- Realistic doodad pass (M2 props placed by ADT MDDF entries) ----
    //
    // Vertex layout matches io::M2Mesh::vertices: float3 pos, float3
    // normal, float2 uv (8 floats per vertex, interleaved).  Per-mesh
    // VBO+VAO cached by FileDataId; loaded lazily on the first instance
    // reference.  Per-instance world matrix uniform per draw (no
    // instancing in v1 -- the operator's view typically frustums to a
    // few thousand visible instances after culling, which is fine for
    // an editor on a discrete GPU).
    struct DoodadVertex { float x, y, z; float nx, ny, nz; float u, v; float u2, v2; };
    struct DoodadGpuSubMesh
    {
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
        GLuint   texture    = 0;      // tex0; borrowed from m_terrainTextureCache (also caches BLP M2 skins).
        uint8_t  blendMode  = 0;
        GLuint   texture2   = 0;      // STAGE A: tex1; 0 -> no second texture (u_hasTexture2 stays 0).
        uint8_t  combinerId = 0;      // io::M2Combiner; 0 = Opaque.
    };
    struct DoodadGpuMesh
    {
        std::unique_ptr<QOpenGLBuffer>            vbo;
        std::unique_ptr<QOpenGLBuffer>            ibo;
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        GLsizei                                   vertexCount = 0;
        GLsizei                                   indexCount  = 0;
        float                                     centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
        float                                     radius  = 0.0f;
        std::vector<DoodadGpuSubMesh>             subMeshes;
        bool                                      loadAttempted = false;
        bool                                      loaded        = false;
    };
    struct DoodadInstanceGpu
    {
        uint32_t modelFdid = 0;
        float    x = 0.0f, y = 0.0f, z = 0.0f;
        float    rotZ = 0.0f, rotY = 0.0f, rotX = 0.0f;
        float    scale = 1.0f;
        // PERF: column-major world matrix (translate * Rz * Ry * Rx * scale),
        // composed ONCE when the instance is created.  drawDoodads streams
        // the visible subset into the shared instance VBO each frame instead
        // of rebuilding a QMatrix4x4 + uploading a uniform per instance.
        float    model[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    };
    QOpenGLShaderProgram*    m_doodadProgram = nullptr;
    int  m_doodadUMvp     = -1;
    int  m_doodadUSunDir  = -1;
    int  m_doodadUAmbient = -1;
    int  m_doodadUHasTex  = -1;
    int  m_doodadUTexture = -1;
    int  m_doodadUAlphaCutoff = -1;
    int  m_doodadUTexture2   = -1;   // STAGE A: second sampler unit (GL_TEXTURE1).
    int  m_doodadUHasTex2    = -1;   // STAGE A: 1 when tex1 bound, else 0 (white no-op modulate).
    int  m_doodadUCombinerId = -1;   // STAGE A: M2Combiner switch selector.
    std::unordered_map<uint32_t, DoodadGpuMesh>  m_doodadMeshes;
    std::vector<DoodadInstanceGpu>               m_doodadInstances;
    // PERF: instanced doodad rendering.  Per FDID run drawDoodads gathers the
    // visible instances' cached matrices into the scratch, streams them into
    // this shared VBO (attribute locations 4..7, divisor 1) and issues ONE
    // glDrawElementsInstanced per submesh -- replacing a draw call + CPU
    // matrix build + u_model upload PER INSTANCE per submesh.
    QOpenGLBuffer                                m_doodadInstanceVbo{QOpenGLBuffer::VertexBuffer};
    std::vector<float>                           m_doodadInstanceScratch;
    bool                                         m_doodadsBuilt   = false;
    bool                                         m_doodadsVisible = true;
    void destroyDoodadResources();
    void rebuildDoodadInstances();
    static void composeDoodadModelMatrix(DoodadInstanceGpu& inst);
    bool ensureDoodadMeshLoaded(uint32_t fdid);
    void drawDoodads(QMatrix4x4 const& mvp);
    // STAGE B1: GL-thread upload of a worker-decoded M2 payload into a
    // DoodadGpuMesh (VBO/IBO/VAO + per-submesh texture resolve).  Returns
    // true when the mesh became renderable.
    bool uploadDoodadPayload(DoodadPending& p);

    // ---- Realistic textured WMO pass (client WMO group geometry) ----
    //
    // Real wall/floor/ceiling/bridge geometry decoded from the WMO root +
    // group files (io::loadWmo).  One VBO/IBO/VAO per WMO-root FDID (shared
    // by every instance); a per-instance u_model uniform per draw, exactly
    // like the doodad pass.  Uses a dedicated mpv_wmo-style program
    // (pos/normal/uv/colour + interior/exterior ambient + the shared fog +
    // hemisphere-ambient helpers) so MOCV vertex colour can tint the
    // baked-light look the retail client gives WMO interiors.  The existing
    // translucent collision overlay (m_wmoMesh) and the flat lit-tint pass
    // (m_litWmo*) remain as the fallback when no client WMO data loads.
    struct WmoVertex { float x, y, z; float nx, ny, nz; float u, v; uint8_t r, g, b, a; };
    struct WmoGpuSubMesh
    {
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
        GLuint   texture    = 0;   // borrowed from m_terrainTextureCache.
        uint8_t  blendMode  = 0;
        bool     interior   = false;
    };
    struct WmoGpuModel
    {
        std::unique_ptr<QOpenGLBuffer>            vbo;
        std::unique_ptr<QOpenGLBuffer>            ibo;
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        GLsizei                                   vertexCount = 0;
        GLsizei                                   indexCount  = 0;
        float                                     centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
        float                                     radius  = 0.0f;
        std::vector<WmoGpuSubMesh>                subMeshes;   // flattened across groups.
        bool                                      loadAttempted = false;
        bool                                      loaded        = false;
    };
    struct WmoInstanceGpu
    {
        uint32_t wmoRootFdid = 0;
        float    x = 0.0f, y = 0.0f, z = 0.0f;
        float    rotZ = 0.0f, rotY = 0.0f, rotX = 0.0f;
        float    scale = 1.0f;
    };
    QOpenGLShaderProgram*    m_wmoProgram = nullptr;   // dedicated mpv_wmo-style program.
    int  m_wmoUMvp        = -1;
    int  m_wmoUModel      = -1;
    int  m_wmoUHasTex     = -1;
    int  m_wmoUTexture    = -1;
    int  m_wmoUAlphaCutoff = -1;
    int  m_wmoUInterior   = -1;
    std::unordered_map<uint32_t, WmoGpuModel>    m_wmoModels;
    std::vector<WmoInstanceGpu>                  m_wmoInstances;
    bool                                         m_texturedWmosBuilt   = false;
    bool                                         m_texturedWmosVisible = true;
    void destroyTexturedWmoResources();
    bool ensureWmoModelLoaded(uint32_t fdid);
    void drawTexturedWmos(QMatrix4x4 const& mvp);
    // STAGE B1: GL-thread upload of a worker-decoded WMO payload into a
    // WmoGpuModel (VBO/IBO/VAO + per-submesh texture resolve).  Returns true
    // when the model became renderable.
    bool uploadWmoPayload(WmoPending& p);

    // Realistic mode toggle + minimap-tile texture sources.
    bool m_realistic = false;
    io::CascClient*   m_cascClient = nullptr;   // borrowed
    io::MapDb2Lookup* m_mapDb2     = nullptr;   // borrowed
    // Cached WDT for the current heightmap mapId; loaded lazily on the
    // first ADT dispatch.  MAID lookup is how the editor matches
    // wow.export's resolution path (rootADT / obj0 / tex0 FDIDs by
    // tile-(gx, gy)) without the community-listfile coverage gap.  The
    // ptr is null until the first successful load; subsequent map
    // switches reset it via dropWdtCache().
    std::unique_ptr<io::Wdt> m_wdt;
    uint32_t                 m_wdtMapId = 0;
    // Failure latch: true after a load failure for m_wdtMapId so callers
    // don't re-probe CASC (sync I/O) + re-log on every frame.  Re-armed by
    // dropWdtCache() / setCascClient() / map switch.
    bool                     m_wdtLoadFailed = false;
    void dropWdtCache();
    [[nodiscard]] io::Wdt const* ensureWdt();
    QString           m_minimapDir;
    // Per-tile minimap texture cache (gy << 16 | gx).  Zero = previous
    // load attempt failed; absent = not yet attempted.
    std::unordered_map<uint32_t, GLuint> m_minimapTextures;
    void destroyMinimapTextures();
    void destroyLitTerrainTiles();
    [[nodiscard]] GLuint loadOrUploadMinimapTile(int gx, int gy);
    void rebuildLitTerrainTiles();

    // Path lines (GL_LINES).
    struct PathVertex { float x, y, z; uint8_t r, g, b, a; };
    QOpenGLBuffer            m_pathVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_pathVao;
    GLsizei                  m_pathVertexCount = 0;
    bool                     m_pathDirty = false;
    void rebuildPathBuffer();
    void uploadPathGeometry();

    // Annotation disc fans (GL_TRIANGLE_FAN expanded into GL_TRIANGLES).
    struct AnnotVertex { float x, y, z; uint8_t r, g, b, a; };
    QOpenGLBuffer            m_annotVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_annotVao;
    GLsizei                  m_annotVertexCount = 0;
    bool                     m_annotDirty = false;
    void rebuildAnnotBuffer();
    void uploadAnnotGeometry();

    // Terrain build.
    void rebuildTerrainBuffer();
    void uploadTerrainGeometry();

    // Click-to-select.
    [[nodiscard]] int hitTestSpawn(QPoint const& screen, float pixelTolerance = 8.0f) const;
    // Project a world point to screen-space pixel coords; sets outBehind
    // when the point is behind the near plane (sx/sy are meaningless then).
    [[nodiscard]] bool projectToScreen(float wx, float wy, float wz,
                                       QMatrix4x4 const& mvp,
                                       float& outSx, float& outSy) const;
    // Distance from `screen` to the nearest segment of any path polyline,
    // returns the path index or -1.  Distance is in screen pixels;
    // segments behind the camera are skipped.
    [[nodiscard]] int hitTestPath(QPoint const& screen, float pixelTolerance = 6.0f) const;
    // Hit-test annotation discs: project center, then test if `screen` is
    // within the projected radius (approximated by projecting `center +
    // radius*east` and measuring screen distance).
    [[nodiscard]] int hitTestAnnotation(QPoint const& screen) const;
    // Mirrors hitTestSpawn for the areatrigger + graveyard marker lists.
    [[nodiscard]] int hitTestAreatrigger(QPoint const& screen, float pixelTolerance = 8.0f) const;
    [[nodiscard]] int hitTestGraveyard  (QPoint const& screen, float pixelTolerance = 8.0f) const;

    // Build the world-space pick ray through `screen` (logical px): outOrigin
    // on the near plane, outDir = (far - near) (NOT normalised).  Pure matrix
    // math -- safe to call from a mouse handler.  Backs both placement
    // (ray-march in MainWindow) and drag (screenRayToPlaneZ).
    [[nodiscard]] bool screenToWorldRay(QPoint const& screen,
                                        QVector3D& outOrigin, QVector3D& outDir) const;
    bool m_placementMode = false;

    // ---- Spawn drag-to-move ----
    // Begun on a left-press that hits a spawn billboard (outside placement
    // mode).  The spawn tracks the cursor in a horizontal plane at its start
    // Z until release; on release spawnMoved() fires (only if it actually
    // moved -- a pure click stays a selection, never a spurious edit).
    int    m_dragSpawnIndex = -1;
    float  m_dragPlaneZ     = 0.0f;
    bool   m_dragMoved      = false;
    // Intersect the camera ray through `screen` (logical px) with the
    // horizontal plane z == planeZ.  False when the ray is parallel to the
    // plane (grazing horizon) or the hit is behind the camera.
    [[nodiscard]] bool screenRayToPlaneZ(QPoint const& screen, float planeZ, QVector3D& out) const;

    // ---- Path node editing (parity with NavMeshView) ----
    // Screen-space picks: node = nearest projected node within tolerance;
    // segment = nearest projected polyline segment, returning the world
    // point lerped between its endpoints (so Z is floor-correct).  The *In
    // cores are shared between waypoint paths and the dungeon-route chain.
    [[nodiscard]] bool hitTestNodeIn(std::vector<Path> const& paths,
                                     QPoint const& screen,
                                     int& outPathIdx, int& outNodeIdx,
                                     float pixelTolerance) const;
    [[nodiscard]] bool hitTestSegmentIn(std::vector<Path> const& paths,
                                        QPoint const& screen,
                                        int& outPathIdx, int& outAfterNode,
                                        QVector3D& outWorld,
                                        float pixelTolerance) const;
    [[nodiscard]] bool hitTestPathNode(QPoint const& screen,
                                       int& outPathIdx, int& outNodeIdx,
                                       float pixelTolerance = 12.0f) const;
    [[nodiscard]] bool hitTestPathSegment(QPoint const& screen,
                                          int& outPathIdx, int& outAfterNode,
                                          QVector3D& outWorld,
                                          float pixelTolerance = 8.0f) const;
    // Node drag mirrors the spawn drag: horizontal plane at the node's
    // start Z, live line preview, single edit emitted on release.
    // m_dragIsRoute selects whether the drag edits m_paths or m_dungeonRoutes.
    int    m_dragPathIndex  = -1;
    int    m_dragNodeIndex  = -1;
    float  m_dragNodePlaneZ = 0.0f;
    bool   m_dragNodeMoved  = false;
    bool   m_dragIsRoute    = false;
    // ---- Pathfinding probe overlay state ----
    bool                     m_probeValid = false;
    QVector3D                m_probeStart;         // clicked TC world coords
    QVector3D                m_probeEnd;
    std::vector<PathNode>    m_probeStraight;      // straight-path polyline (TC)
    bool                     m_probePartial  = false;  // DT_PARTIAL_RESULT / cap hit
    bool                     m_probeNoPath   = false;  // no poly / findPath failed
    int                      m_probePolys    = 0;      // corridor polys used (cap 74)
    int                      m_probePoints   = 0;      // straight-path points
    float                    m_probeLenYd    = 0.0f;   // polyline length
    QString                  m_probeSummary;           // HUD one-liner
    QOpenGLBuffer            m_probeVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_probeVao;
    GLsizei                  m_probeVertexCount = 0;
    bool                     m_probeDirty = false;
    void rebuildProbeBuffer();
    void uploadProbeGeometry();

    // ---- Off-mesh connection overlay ----
    bool                     m_offmeshVisible = true;
    std::vector<std::pair<QVector3D, QVector3D>> m_pendingOffmesh;
    QOpenGLBuffer            m_offmeshVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_offmeshVao;
    GLsizei                  m_offmeshVertexCount = 0;
    bool                     m_offmeshDirty = false;
    void rebuildOffmeshBuffer();
    void uploadOffmeshGeometry();

    // ---- Bot dungeon-route chain (gold overlay + node editing) ----
    std::vector<Path>        m_dungeonRoutes;
    bool                     m_routesVisible = true;
    QOpenGLBuffer            m_routeVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_routeVao;
    GLsizei                  m_routeVertexCount = 0;
    bool                     m_routeDirty = false;
    void rebuildRouteBuffer();
    void uploadRouteGeometry();
    // RMB is camera-rotate when dragged; a click (no movement) opens the
    // path node/segment context menu instead.
    QPoint m_rmbPressPos;
    bool   m_rmbDragged     = false;

    // Areatrigger / Graveyard markers (reuse spawn shader; smaller billboards).
    std::vector<Areatrigger> m_areatriggers;
    std::vector<Graveyard>   m_graveyards;
    QOpenGLBuffer            m_atrVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_atrVao;
    GLsizei                  m_atrVertexCount = 0;
    bool                     m_atrDirty = false;
    void rebuildAreatriggerBuffer();
    void uploadAreatriggerGeometry();
    QOpenGLBuffer            m_gyVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_gyVao;
    GLsizei                  m_gyVertexCount = 0;
    bool                     m_gyDirty = false;
    void rebuildGraveyardBuffer();
    void uploadGraveyardGeometry();

    // ---- Atmospheric pass (sky dome + fog) ----
    //
    // Sky is a camera-anchored triangle-list ring dome (kSegments x kRings),
    // built once into m_skyVbo and drawn FIRST every frame with depth-write
    // OFF (depth-test stays LEQUAL so opaque geometry hides the dome).  The
    // vertex shader pins gl_Position.z to the far plane; the fragment shader
    // evaluates a 6-band vertical gradient (wow.export SkyRenderer.js parity:
    // zenith..horizon..below-horizon) plus a sun-glow disc.  Each vertex
    // carries a band parameter derived from its REAL elevation angle (not the
    // linear ring index) so the band stops land at wow.export's stated
    // elevation angles (70/40/20/8 deg).  Fog + sun uniforms are pushed into
    // every lit pass via applyFogAndSunUniforms() so terrain, WMO and doodads
    // share the same atmospheric tint.  The whole pass is guarded by
    // m_skyBufferReady + a non-null m_skyProgram: if the VBO or shader fails
    // to build the dome is silently skipped and the scene renders without it.
    QOpenGLShaderProgram*    m_skyProgram = nullptr;
    QOpenGLBuffer            m_skyVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_skyVao;
    int  m_skyUMvp         = -1;
    int  m_skyUCamPos      = -1;
    int  m_skyUZenith      = -1;
    int  m_skyUHorizon     = -1;
    int  m_skyUGround      = -1;     // location of u_skyGround (distinct sky-ground band)
    int  m_skyUSunDir      = -1;
    int  m_skyUSunColor    = -1;
    bool m_skyBufferReady  = false;
    GLsizei m_skyVertexCount = 0;     // ring-dome triangle list length
    void drawSky(QMatrix4x4 const& proj, QMatrix4x4 const& view);

    // Atmospheric state -- pulled from a small time-of-day LUT each paint
    // and pushed to every lit pass.  Defaults to clear-noon.
    bool  m_skyVisible    = true;
    bool  m_fogEnabled    = true;
    float m_timeOfDay     = 12.0f;   // 0..24 hours
    float m_sunDir[3]     = { 0.4f, 0.5f, 0.7f };
    float m_sunColor[3]   = { 1.0f, 1.0f, 1.0f };
    float m_ambient       = 0.35f;
    float m_zenithColor[3]  = { 0.30f, 0.55f, 0.92f };
    float m_horizonColor[3] = { 0.78f, 0.86f, 0.96f };
    float m_fogColor[3]     = { 0.78f, 0.86f, 0.96f };
    // Distinct below-horizon sky-ground band (wow.export SkyFogColor analogue,
    // band 5).  Decoupled from m_groundAmbient (a *lighting* term) so the lower
    // sky no longer reads as dirt.  Derived per-hour in refreshAtmosphere().
    float m_skyGroundColor[3] = { 0.10f, 0.12f, 0.18f };
    float m_fogStart        = 600.0f;
    float m_fogEnd          = 1500.0f;
    // Three-band hemisphere ambient (sky / horizon / ground).  Mirrors
    // wow.export's `mpv_light.inc.glsl:14-16`: ambient = sky*max(n.z,0) +
    // ground*max(-n.z,0) + horizon*(1-|n.z|).  Recomputed each paint from
    // the time-of-day LUT entries (zenith->sky, horizon stays, derived
    // ground = horizon * 0.45 to read as the warm dirt tone retail uses).
    float m_skyAmbient[3]     = { 0.35f, 0.42f, 0.55f };
    float m_horizonAmbient[3] = { 0.42f, 0.40f, 0.36f };
    float m_groundAmbient[3]  = { 0.18f, 0.16f, 0.14f };
    // Legacy-exp fog density.  Derived from the per-hour LUT fog range so
    // a single uniform drives `1 - exp(-density * dist)` in every lit
    // fragment shader -- matches `mpv_fog.inc.glsl:60-65`.
    float m_fogDensity       = 0.0015f;
    // Height-fog term (wow.export mpv_fog.inc.glsl:48-50, ported to TC +Z up):
    // fog thins above m_fogHeight (the world-Z fog plane) at the per-unit-Z rate
    // m_fogHeightFalloff.  Driven per-hour by refreshAtmosphere(); a falloff of
    // 0 makes applyFog's altitude term an exact no-op (regression-safe default).
    float m_fogHeight        = 0.0f;
    float m_fogHeightFalloff = 0.0f;
    // Recompute m_sun*/m_*Color/m_fog* from m_timeOfDay via the LUT.
    void refreshAtmosphere();
    // Push fog + sun uniforms to the bound shader.  Uniform names match
    // the GLSL declarations in every lit shader (u_fogStart / u_fogEnd /
    // u_fogColor + u_cameraPos + u_sunDir + u_ambient).
    void applyFogAndSunUniforms(QOpenGLShaderProgram& prog) const;

    // WMO triangle overlay (Phase 5 stretch).  Reuses nav shader; vertex
    // layout matches NavVertex (xyz + rgba).  Rendered semi-transparent
    // so terrain shows through.
    io::LoadedVmap           m_wmoMesh;
    QOpenGLBuffer            m_wmoVbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLVertexArrayObject m_wmoVao;
    GLsizei                  m_wmoVertexCount = 0;
    bool                     m_wmoDirty       = false;
    bool                     m_wmoVisible     = true;
    void rebuildWmoBuffer();
    void uploadWmoGeometry();

    // ---- Frustum culling shared across terrain / WMO / liquid / doodad ----
    //
    // Six planes extracted from the current MVP at the top of each paintGL;
    // every per-tile draw loop tests its bounding sphere against them and
    // skips the draw on reject.  Cached as flat array (a, b, c, d) tuples so
    // the inner test boils down to four fmuls + an add.
    struct FrustumPlane { float a, b, c, d; };
    FrustumPlane m_frustumPlanes[6];
    void extractFrustumPlanes(QMatrix4x4 const& mvp);
    [[nodiscard]] bool sphereInFrustum(float cx, float cy, float cz, float r) const;
    // Per-frame draw / cull counters, reported once per second to the
    // status-bar log + overlay.
    int      m_drawnTilesThisFrame  = 0;
    int      m_culledTilesThisFrame = 0;
    int      m_totalTilesThisFrame  = 0;
    qint64   m_lastTileLogMs        = 0;

    // ---- HUD overlay (Tab to toggle) ----
    //
    // A QPainter pass after the GL draw renders a small black/white text
    // block in the top-left.  Drives the operator's awareness of which tile
    // they're looking at, current fly speed, and height above terrain.
    bool                  m_overlayVisible = true;
    qint64                m_overlayUpdateMs = 0;     // throttle terrain sample to 1 Hz
    float                 m_heightAboveTerrain = 0.0f;
    float                 m_terrainZUnderCam   = 0.0f;
    bool                  m_haveTerrainSample  = false;
    void                  paintOverlay();

    // ---- Camera fly speed knobs ----
    //
    // Base speed is yards/sec applied each onTick; modifier multipliers
    // are Shift=5x (existing) + Alt=0.2x (new).  `[` / `]` halve / double
    // m_flySpeed at runtime and persist via QSettings (key "viewer3d/fly_speed").
    float    m_flySpeed             = 80.0f;
    void     loadFlySpeed();
    void     saveFlySpeed() const;

    // Set true on first paintGL after a navmesh load so frameMesh() runs
    // exactly once after the GL viewport has its real width/height -- a
    // proj-matrix-dependent reframe would otherwise pick up the widget's
    // zero-size initial state.
    bool     m_pendingInitialFrame  = false;

    // ---- Async first-tile load (ADT terrain + minimap textures) ----
    //
    // CASC reads + BLP decodes can stall the GL thread for tens of ms.  We
    // dispatch them onto QThreadPool::globalInstance(); each worker hands
    // back a CPU-only payload (vertex buffers, RGBA bytes) which the GL
    // thread picks up inside paintGL and turns into GL objects.  Keeps the
    // first paint smooth even on continent-scale maps.
    enum class TileLoadStatus : uint8_t { PENDING, LOADED, FAILED, GPU_READY };
    std::mutex                       m_adtPendingMutex;
    std::vector<AdtTilePending>      m_adtPending;             // payloads waiting for GL upload
    QSet<uint32_t>                   m_adtInFlight;            // gy<<16|gx of tiles enqueued
    // Async water (mirror of the ADT inbox): worker-decoded CPU vertex arrays
    // waiting for GL VBO upload, + the in-flight key set.
    std::mutex                       m_waterPendingMutex;
    std::vector<WaterTilePending>    m_waterPending;
    QSet<uint32_t>                   m_waterInFlight;
    // Water wave animation gate: true while the last paint actually drew at
    // least one water tile.  onTick turns this into a capped-cadence repaint
    // instead of the water pass calling update() unconditionally (which
    // forced a full-scene redraw at max framerate even with a still camera).
    bool                             m_waterAnimActive = false;
    QElapsedTimer                    m_waterAnimClock;
    void                             drainPendingWaterUploads(int maxThisFrame);
    void                             dispatchWaterLoad(int gx, int gy);
    std::atomic<bool>                m_adtScanDispatched{false};
    // Sticky flag: when the dispatch gate fails, log one diagnostic line
    // showing exactly which precondition was false.  Re-armed in
    // setRealistic() and setNavMesh() so state changes are surfaced.
    bool                             m_adtGateDiagLogged = false;
    void dispatchAdtTileLoads();
    void unloadFarAdtTiles();
    // Camera tile at the last streaming re-scan; re-scan only on tile cross.
    // Out-of-range sentinel (tiles are 0..63) forces a scan on the first
    // frame + after a map switch (which resets these via destroyAdtTerrainTiles).
    int                              m_streamCamGx = -100000;
    int                              m_streamCamGy = -100000;
    // Camera tile at the last doodad/WMO instance rebuild.  rebuildDoodad-
    // Instances only materialises props within a radius of the camera (NOT
    // the whole continent -- that loaded 488K instances + 3.1M WMO verts and
    // tanked the framerate), and re-streams when the camera crosses tiles.
    int                              m_doodadStreamCamGx = -100000;
    int                              m_doodadStreamCamGy = -100000;
    // ADDITIVE prop residency (Phase 1): tiles whose doodad/WMO instances are
    // already materialised, and the WMO uniqueIds already emitted (straddler
    // dedup).  rebuildDoodadInstances appends ONLY tiles not in here and never
    // clears -- so flying past a tile doesn't re-load/flicker its props (the
    // clear-and-rebuild-every-tile-cross churn).  Both cleared on map switch.
    std::unordered_set<uint64_t>     m_loadedPropTiles;
    std::unordered_set<uint32_t>     m_emittedWmoUids;
    // PERF: async prop-placement enumeration state.  One task in flight at a
    // time; the generation counter invalidates payloads that cross a map
    // switch (destroyDoodadResources / destroyTexturedWmoResources bump it).
    bool                             m_propPlacementInFlight = false;
    uint32_t                         m_propGeneration = 0;
    void dispatchPropPlacementLoads();
    void drainPendingAdtUploads(int maxChunksThisFrame);
    // PERF: chunk-budgeted staging for the ADT drain.  One tile used to be
    // uploaded whole (256 chunk VBOs + alpha arrays in a single frame -- a
    // multi-hundred-ms stall every time a tile landed while flying); now a
    // budget of chunks per frame trickles the active tile in.  The staged
    // AdtTileRender only becomes visible (pushed to m_adtTerrainTiles) once
    // every chunk is uploaded, so the lit fallback keeps covering the tile
    // during the trickle and no half-textured tile is ever drawn.
    bool                             m_adtUploadActive = false;
    AdtTilePending                   m_adtUploadPending;
    AdtTileRender                    m_adtUploadTile;
    size_t                           m_adtUploadNextChunk = 0;
    int                              m_adtUploadTexed = 0;
    int                              m_adtUploadAlpha = 0;
    int                              m_adtUploadGpuTex = 0;
    int                              m_adtUploadFirstTexed = -1;

    std::mutex                       m_minimapPendingMutex;
    std::vector<MinimapPending>      m_minimapPending;
    QSet<uint32_t>                   m_minimapInFlight;
    void dispatchMinimapLoadIfNeeded(int gx, int gy);
    void drainPendingMinimapUploads(int maxThisFrame);

    // ---- STAGE B1: async WMO / M2 mesh streaming ----
    //
    // ensureWmoModelLoaded / ensureDoodadMeshLoaded used to run the CASC read +
    // decode SYNCHRONOUSLY inside the draw loop -- the first frame a new model
    // became visible stalled the GL thread for that whole model.  Now the draw
    // loops only QUEUE the FDID (gated by a conservative per-instance frustum/
    // distance test so the whole city does not burst in on the first frame);
    // a worker decodes the geometry off-thread and the GL drainer below uploads
    // a budgeted few payloads per frame, identical in shape to the ADT pipeline.
    std::mutex                       m_wmoPendingMutex;
    std::vector<WmoPending>          m_wmoPending;
    QSet<uint32_t>                   m_wmoInFlight;     // FDIDs dispatched, awaiting upload.
    void dispatchWmoLoad(uint32_t fdid);
    void drainPendingWmoUploads(int maxThisFrame);

    std::mutex                       m_doodadPendingMutex;
    std::vector<DoodadPending>       m_doodadPending;
    QSet<uint32_t>                   m_doodadInFlight;
    void dispatchDoodadLoad(uint32_t fdid);
    void drainPendingDoodadUploads(int maxThisFrame);
};

} // namespace world_editor::render
