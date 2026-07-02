#include "MainWindow.h"

#include "../db/AnnotationModel.h"
#include "../db/AreatriggerModel.h"
#include "../db/ConditionsModel.h"
#include "../db/ConnectionDialog.h"
#include "../db/GraveyardModel.h"
#include "../db/MySqlClient.h"
#include "../db/SpawnModel.h"
#include "../io/BlpReader.h"
#include "../io/CascClient.h"
#include "../io/ListfileLookup.h"
#include "../io/MMapReader.h"
#include "../io/MapDb2Lookup.h"
#include "../io/MapReader.h"
#include "../io/MapTileCache.h"
#include "../io/VmapReader.h"
#include "../render/NavMeshView.h"
#include "../render/SceneView3D.h"
#include "../db/WaypointModel.h"
#include "AboutDialog.h"
#include "AnnotationToolbox.h"
#include "Bookmarks.h"
#include "BookmarksManagerDialog.h"
#include "AreatriggerCommitDialog.h"
#include "AreatriggerCreatePropsPicker.h"
#include "AreatriggerPropertiesDock.h"
#include "AnnotationPropertiesDock.h"
#include "UndoManager.h"
#include "HealthReportDialog.h"
#include "SmartScriptFlowDialog.h"
#include "SmartScriptDryRunDialog.h"
#include "VendorInventoryDock.h"
#include "TrainerSpellDock.h"
#include "LootTableDock.h"
#include "ConditionsDock.h"
#include "QuestRewardDock.h"
#include "AreaInfoDock.h"
#include "ZoneSummaryDock.h"
#include "SpellInfoDock.h"
#include "PlayerConditionDock.h"
#include "NpcTextDock.h"
#include "AreatriggerScriptDock.h"
#include "InfoInspectorDock.h"
#include "PropertyInspectorDock.h"
#include "HandcraftedRoadDock.h"
#include "MapPickerDialog.h"
#include "LogTailDock.h"

#include <QDir>
#include "GossipMenuDialog.h"
#include "GossipMenuEditDialog.h"
#include "GameEventEditDialog.h"
#include "NpcVendorDialog.h"
#include "DisablesEditDialog.h"
#include "WaypointPathDialog.h"
#include "AreaTriggerTeleportDialog.h"
#include "WorldSafeLocsDialog.h"
#include "QuestDialogTextDialog.h"
#include "AccessRequirementDialog.h"
#include "QuestGiverLinkageDialog.h"
#include "CreatureLootEditDialog.h"
#include "CreatureSummonGroupsDialog.h"
#include "CreatureTextEditDialog.h"
#include "CreatureEquipTemplateDialog.h"
#include "BroadcastTextDialog.h"
#include "CreatureTemplateAddonDialog.h"
#include "GameObjectInfoDock.h"
#include "ItemInfoDock.h"
#include "CurrencyTypeDock.h"
#include "FactionTemplateDock.h"
#include "BulkEditDialog.h"
#include "BulkTransformDialog.h"
#include "PropagateFieldsDialog.h"
#include "SpawnDiffDialog.h"
#include "CommitDialog.h"
#include "ConfirmSqlDialog.h"
#include "CreatureAddonEditDialog.h"
#include "GameObjectAddonEditDialog.h"
#include "LinkedRespawnDialog.h"
#include "SecondaryLootTablesDialog.h"
#include "GraveyardCommitDialog.h"
#include "GraveyardPropertiesDock.h"
#include "GraveyardZoneDialog.h"
#include "GroupsPoolsDialog.h"
#include "FindJumpDialog.h"
#include "FindSimilarDialog.h"
#include "SpawnSearchDialog.h"
#include "PathPropertiesDock.h"
#include "SmartScriptCommitDialog.h"
#include "SmartScriptEditDialog.h"
#include "SpawnDiagnosticsDock.h"
#include "MinimapDiagnosticsDock.h"
#include "MinimapSetupWizard.h"
#include "TransportEditDialog.h"
#include "../db/SmartScriptModel.h"
#include "CsvImportDialog.h"
#include "SpawnCloneDialog.h"
#include "SpawnCommitDialog.h"
#include "SpawnPropertiesEditor.h"
#include "ShortcutHelpDialog.h"
#include "CommandPaletteDialog.h"
#include "SqlPatchExporter.h"
#include "TemplatePickerDialog.h"
#include "WaypointCommitDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QStackedWidget>
#include <QToolBar>
#include <QFontDatabase>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QStringList>
#include <QMenuBar>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QOpenGLWidget>
#include <QPalette>
#include <QColor>
#include <QStyle>
#include <QStyleFactory>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QSettings>
#include <QTextStream>
#include <QShortcut>
#include <QShowEvent>
#include <QSize>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QtVersion>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <set>

namespace world_editor
{

namespace
{
constexpr char SETTINGS_MMAPS_DIR[]   = "paths/mmaps_dir";
constexpr char SETTINGS_MAPS_DIR[]    = "paths/maps_dir";
constexpr char SETTINGS_VMAPS_DIR[]   = "paths/vmaps_dir";
constexpr char SETTINGS_MINIMAP_DIR[] = "paths/minimap_dir";
constexpr char SETTINGS_CASC_DIR[]    = "paths/casc_client_dir";
// wow-listfile CSV path persisted between sessions so the FDID resolver
// re-loads at startup without operator intervention.  Required for live
// CASC reads on modern (TWW+) client data where minimap BLPs are
// FileDataID-only.
constexpr char SETTINGS_LISTFILE_CSV[] = "paths/listfile_csv";
constexpr char SETTINGS_GEOMETRY[]  = "ui/geometry";
constexpr char SETTINGS_STATE[]     = "ui/state";
constexpr char SETTINGS_REALISTIC3D[] = "viewer3d/realistic";
constexpr char SETTINGS_DOODADS3D[]   = "viewer3d/doodads";
constexpr char SETTINGS_TEXWMOS3D[]   = "viewer3d/textured_wmos";
constexpr char SETTINGS_SKY3D[]       = "viewer3d/sky";
constexpr char SETTINGS_FOG3D[]       = "viewer3d/fog";
constexpr char SETTINGS_WATER3D[]     = "viewer3d/water";
constexpr char SETTINGS_TIMEOFDAY3D[] = "viewer3d/timeOfDay";
constexpr char SETTINGS_VERBOSE3DLOG[] = "viewer3d/verbose_log";
constexpr char SETTINGS_QUEST_OBJECTIVES[] = "viewer2d/quest_objectives";
// Phase-mask filter: hide spawns whose phaseId/phaseGroup don't match.
// All three keys live under viewer2d/ to match the rest of the 2D
// viewer's persisted toggles.
constexpr char SETTINGS_PHASE_FILTER_ENABLED[] = "viewer2d/phase_filter_enabled";
constexpr char SETTINGS_PHASE_FILTER_ID[]      = "viewer2d/phase_id";
constexpr char SETTINGS_PHASE_FILTER_GROUP[]   = "viewer2d/phase_group";
// Battlemaster recruitment-radius overlay: dashed yellow ring around
// every creature spawn flagged UNIT_NPC_FLAG_BATTLEMASTER (0x100000).
// `battlemaster_overlay` defaults ON; `battlemaster_radius_yards`
// defaults 5 (matches TC NPC interaction range).
constexpr char SETTINGS_BATTLEMASTER_OVERLAY[] = "viewer2d/battlemaster_overlay";
constexpr char SETTINGS_BATTLEMASTER_RADIUS[]  = "viewer2d/battlemaster_radius_yards";
// Faction-territory tint: blends spawn icons toward Alliance / Horde /
// Sanctuary / Contested / Neutral palette so the operator can read
// territorial control at a glance.  Defaults OFF.
constexpr char SETTINGS_FACTION_TINT[]         = "viewer2d/faction_tint";
// Level-bracket heatmap: colors creature spawns by creature_template
// minlevel..maxlevel midpoint over a 12-stop yellow->dark-red palette.
// Defaults OFF; persists across editor restarts.
constexpr char SETTINGS_LEVEL_HEATMAP[]        = "viewer2d/level_heatmap";
// Spawn-density heatmap: 50-yard QPainter cells tinted by spawn count
// (light green 1-5, yellow 6-15, orange 16-30, red 31-50, bright red 51+).
// Defaults OFF; persists across editor restarts.
constexpr char SETTINGS_SPAWN_DENSITY[]        = "viewer2d/spawn_density";
// Flight-path graph overlay (taxi_nodes + taxi_path).  Defaults OFF;
// a one-shot status note flags absent tables (DB2-only post-Cata so
// many shards no longer hotfix the SQL mirrors).
constexpr char SETTINGS_FLIGHT_PATHS[]         = "viewer2d/flight_paths";
// Transport route overlay: thick orange polylines for transports +
// transport_animation keyframes (zeppelins/boats).  Default OFF; the
// DB query runs only on toggle so the cost is paid when needed.
constexpr char SETTINGS_TRANSPORT_ROUTES[]     = "viewer2d/transport_routes";
// Gathering-node heatmap overlay (mining veins / herb nodes / fishing
// pools / treasure goobers).  Defaults OFF; the DB query runs at map
// load when the toggle is on so the cost is paid only when needed.
constexpr char SETTINGS_GATHERING_NODES[]      = "viewer2d/gathering_nodes";
// Instance-entrance overlay: large purple rings drawn around every
// areatrigger_teleport on the current map.  Default ON; cheap to query
// (a few hundred rows even on continents) so we run it every map load.
constexpr char SETTINGS_INSTANCE_ENTRANCES[]   = "viewer2d/instance_entrances";
// linked_respawn dependency overlay: dotted dark-green segments between
// dependent spawn pairs.  Defaults OFF; the DB query runs on toggle and
// at map load when the toggle is on.
constexpr char SETTINGS_SPAWN_LINKS[]          = "viewer2d/spawn_links";
// Road network overlay: auto-extracted polylines (gold) walked off the
// dtNavMesh's NAV_AREA_ROAD polygons plus an optional handcrafted polyline
// pass (coral) pushed by a separate agent.  Default ON; both passes are
// cheap to build and only render when their respective VBO is non-empty.
constexpr char SETTINGS_SHOW_ROADS[]           = "viewer2d/show_roads";
// 2D building-footprint overlay sourced from the per-WMO AABBs captured
// during vmap load.  Default OFF; the AABB list is computed lazily on
// first enable and cached per loaded vmap so subsequent toggles are free.
constexpr char SETTINGS_WMO_FOOTPRINTS[]       = "viewer2d/wmo_footprints";
// Sibling-highlight overlay: golden ring around every other spawn that
// shares the selected creature's entry on the current map.  Default ON;
// when OFF, MainWindow skips both the sibling-walk computation and the
// push to the viewer so a heavy entry doesn't pay the scan cost.
constexpr char SETTINGS_SIBLING_HIGHLIGHT[]    = "viewer2d/sibling_highlight";
// Last-applied workspace preset name; replayed on next startup so the
// editor restores to that named combo of layer toggles in addition to
// the existing per-layer settings.
constexpr char SETTINGS_LAST_PRESET[]          = "viewer/last_preset";
// View -> Theme: one of "system" / "light" / "dark"; applied at startup
// from restoreSettings() and on every menu selection.
constexpr char SETTINGS_THEME[]                = "ui/theme";
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QApplication::applicationDisplayName());
    resize(1280, 800);

    m_annotationModel  = std::make_unique<db::AnnotationModel>();
    m_spawnModel       = std::make_unique<db::SpawnModel>();
    m_waypointModel    = std::make_unique<db::WaypointModel>();
    m_areatriggerModel = std::make_unique<db::AreatriggerModel>();
    m_graveyardModel   = std::make_unique<db::GraveyardModel>();
    m_conditionsModel  = std::make_unique<db::ConditionsModel>();

    m_undo = new app::UndoManager(this);
    connect(m_undo, &app::UndoManager::stateChanged,
            this, &MainWindow::onUndoStateChanged);
    // Bump LRU to 2000 so a full continent's heightmap tiles all fit
    // in cache after first eager build (Phase 1.5).  Snap-to-ground
    // still uses the same cache.
    m_mapTileCache    = std::make_unique<io::MapTileCache>(/*maxTilesInRam*/ 2000);
    m_cascClient      = std::make_unique<io::CascClient>();
    m_mapDb2          = std::make_unique<io::MapDb2Lookup>();
    // Seed the fallback table immediately so directoryFor() resolves
    // continent IDs even before CASC opens.
    m_mapDb2->loadFallbackOnly();

    buildCentralWidget();
    buildSpawnDock();
    buildAnnotationToolbox();
    buildPropertyInspectorDock();
    buildInfoInspectorDock();
    buildHandcraftedRoadDock();
    buildMenus();
    buildQuickToolbar();
    buildStatusBar();
    restoreSettings();

    if (m_viewer && m_mapTileCache)
        m_viewer->setMapTileCache(m_mapTileCache.get());
    if (m_viewer3d && m_mapTileCache)
        m_viewer3d->setMapTileCache(m_mapTileCache.get());

    // VS Code-style command palette: Ctrl+Shift+P opens a frameless modal
    // overlay that fuzzy-matches against every menu action.  Wired here
    // (after buildMenus()) so the menu bar is fully populated when the
    // palette walks it on first open.
    {
        auto* cpShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), this);
        cpShortcut->setContext(Qt::ApplicationShortcut);
        connect(cpShortcut, &QShortcut::activated, this, &MainWindow::onShowCommandPalette);
    }

    // The game world is experienced in 3D, so the editor opens in the 3D
    // view -- authoring in the same spatial frame the player sees is what
    // preserves the in-game experience (and the editor's value).  The 2D
    // top-down view remains one keypress away (F2) as an overview.  No map
    // is loaded yet, so this just shows the empty 3D scene until File->Open.
    if (m_centralStack && m_viewer3d)
        m_centralStack->setCurrentWidget(m_viewer3d);
}

MainWindow::~MainWindow() = default;

void MainWindow::setMMapsDir(QString const& dir)
{
    m_mmapsDir = dir;
}

void MainWindow::setMapsDir(QString const& dir)
{
    m_mapsDir = dir;
    if (m_mapTileCache)
        m_mapTileCache->setMapsDir(std::filesystem::path(dir.toStdString()));
}

void MainWindow::setVmapsDir(QString const& dir)
{
    m_vmapsDir = dir;
}

void MainWindow::requestOpenMap(uint32_t mapId)
{
    m_pendingOpenMapId = mapId;
}

bool MainWindow::renderToPng(uint32_t mapId, float camX, float camY, float camZ,
                             float yaw, float pitch, bool realistic,
                             int w, int h, int waitMs, QString const& outPath)
{
    if (!m_viewer3d || !m_centralStack)
        return false;

    // Show the 3D view first: (a) its GL context initialises, and (b) the
    // deferred 3D-mesh load fires on open (the gate checks currentWidget ==
    // m_viewer3d).  resize before grab so the FBO is the requested size.
    m_centralStack->setCurrentWidget(m_viewer3d);
    m_viewer3d->resize(w, h);
    m_viewer3d->setRealistic(realistic);
    // Force a known-good render config so the oracle image reflects the FULL
    // scene regardless of whatever layer toggles the operator persisted (a
    // disabled sky/fog/doodad toggle would otherwise make the headless shot
    // misleadingly dark/empty).
    if (realistic)
    {
        m_viewer3d->setSkyVisible(true);
        m_viewer3d->setFogEnabled(true);
        m_viewer3d->setWaterVisible(true);
        m_viewer3d->setDoodadsVisible(true);
        m_viewer3d->setWmoVisible(true);
        m_viewer3d->setTexturedWmosVisible(true);
        m_viewer3d->setTimeOfDay(12.0f);
    }
    QApplication::processEvents();

    loadAndDisplayMap(mapId);
    m_pendingOpenMapId.reset();
    QApplication::processEvents();

    // Diagnostic: WE_NO_MARKERS strips the DB authoring overlays (spawn dots,
    // paths, annotations) from the headless shot so the terrain/water/WMO
    // render can be inspected without thousands of markers occluding it.  This
    // is what the operator actually sees with markers filtered; it also gives
    // the Phase-4 regression harness a clean geometry-only baseline.
    if (qEnvironmentVariableIsSet("WE_NO_MARKERS"))
    {
        m_viewer3d->setSpawns({});
        QApplication::processEvents();
    }

    // Diagnostic: WE_NAVMESH=1 turns the navmesh polygon overlay ON for the
    // shot (it is opt-in/off by default in 3D).  Lets a headless pair of
    // renders compare walkable mesh vs real geometry at the same camera.
    if (qEnvironmentVariableIsSet("WE_NAVMESH"))
        m_viewer3d->setLayerVisible(render::Layer::NavMesh, true);

    // Diagnostic: WE_PROBE="x1,y1,z1,x2,y2,z2" runs the bot-budget
    // pathfinding probe (74 polys / 1024 nodes) A->B on the loaded mmap and
    // bakes its overlay + HUD verdict into the shot.  TC world coords.
    if (qEnvironmentVariableIsSet("WE_PROBE"))
    {
        QStringList const c = qEnvironmentVariable("WE_PROBE")
                                  .split(',', Qt::SkipEmptyParts);
        if (c.size() == 6)
        {
            m_viewer3d->runPathProbe(
                QVector3D(c[0].toFloat(), c[1].toFloat(), c[2].toFloat()),
                QVector3D(c[3].toFloat(), c[4].toFloat(), c[5].toFloat()));
        }
        else
        {
            qWarning("[screenshot] WE_PROBE needs 6 comma-separated floats "
                     "(x1,y1,z1,x2,y2,z2); got %d", int(c.size()));
        }
    }

    m_viewer3d->setCamera(camX, camY, camZ, yaw, pitch);

    // Diagnostic: WE_CAMSPIN sweeps the camera (pan + yaw) across the wait loop
    // instead of holding it still.  A fixed-camera shot CANNOT show the motion/
    // accumulation artifacts (stale depth, occlusion errors that only appear as
    // the view moves) the operator reports -- this drives real camera motion so
    // the final grabbed frame exhibits them.
    bool const camSpin = qEnvironmentVariableIsSet("WE_CAMSPIN");

    // Pump the event loop so the async ADT / WMO / minimap / doodad workers
    // stream their payloads in and paint.  grabFramebuffer() forces a
    // synchronous paint that drains the per-frame GPU-upload budget, so
    // repeated grabs over waitMs let the scene fill in before the final save.
    QElapsedTimer t; t.start();
    QImage img;
    while (t.elapsed() < waitMs)
    {
        if (camSpin)
        {
            float const e = float(t.elapsed()) * 0.001f;
            float const sYaw = yaw + 0.7f * std::sin(e * 0.9f);
            float const sX   = camX + 300.0f * std::sin(e * 0.55f);
            float const sY   = camY + 300.0f * std::cos(e * 0.55f);
            m_viewer3d->setCamera(sX, sY, camZ, sYaw, pitch);
        }
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        m_viewer3d->update();
        img = m_viewer3d->grabFramebuffer();
    }
    if (img.isNull())
        img = m_viewer3d->grabFramebuffer();

    bool const ok = !img.isNull() && img.save(outPath, "PNG");
    qInfo().noquote() << "[screenshot] map" << mapId
                      << "cam" << camX << camY << camZ << "yaw" << yaw << "pitch" << pitch
                      << "->" << outPath << (ok ? "OK" : "FAIL")
                      << img.width() << "x" << img.height();
    return ok;
}

void MainWindow::buildCentralWidget()
{
    m_centralStack = new QStackedWidget(this);
    m_viewer   = new render::NavMeshView(m_centralStack);
    m_viewer3d = new render::SceneView3D (m_centralStack);
    m_centralStack->addWidget(m_viewer);    // index 0 = 2D
    m_centralStack->addWidget(m_viewer3d);  // index 1 = 3D
    setCentralWidget(m_centralStack);
    // 3D click-to-select piggybacks on 2D's onSpawnClicked slot; same
    // for paths and annotations (Phase 5 polish), plus areatriggers and
    // graveyards (Phase 7+ polish).
    connect(m_viewer3d, &render::SceneView3D::spawnClicked,
            this, &MainWindow::onSpawnClicked);
    // 3D drag-to-move (Z-aware overload, distinct from the 2D XY-only one).
    connect(m_viewer3d, &render::SceneView3D::spawnMoved,
            this, &MainWindow::onSpawnMoved3D);
    connect(m_viewer3d, &render::SceneView3D::pathClicked,
            this, &MainWindow::onPathClicked);
    // 3D path-node editing (parity with the 2D view).  Both viewers receive
    // the identical path vector from pushPathsToViewer, so the 3D indexes
    // resolve through the same pathId mapping.  Move/segment carry Z
    // (drag-plane / segment-lerped) so multi-floor dungeons don't snap
    // edits onto the wrong storey.
    connect(m_viewer3d, &render::SceneView3D::pathNodeClicked,
            this, &MainWindow::onPathNodeClicked);
    connect(m_viewer3d, &render::SceneView3D::pathNodeMoved,
            this, &MainWindow::onPathNodeMoved3D);
    connect(m_viewer3d, &render::SceneView3D::pathNodeContextMenuRequested,
            this, &MainWindow::onPathNodeContextMenu);
    connect(m_viewer3d, &render::SceneView3D::pathSegmentContextMenuRequested,
            this, &MainWindow::onPathSegmentContextMenu3D);
    // Bot dungeon-route chain: same node-editing surface, separate store.
    connect(m_viewer3d, &render::SceneView3D::routeNodeMoved,
            this, &MainWindow::onRouteNodeMoved3D);
    connect(m_viewer3d, &render::SceneView3D::routeNodeContextMenuRequested,
            this, &MainWindow::onRouteNodeContextMenu3D);
    connect(m_viewer3d, &render::SceneView3D::routeSegmentContextMenuRequested,
            this, &MainWindow::onRouteSegmentContextMenu3D);
    connect(m_viewer3d, &render::SceneView3D::annotationClicked,
            this, &MainWindow::onAnnotationClicked);
    connect(m_viewer3d, &render::SceneView3D::areatriggerClicked,
            this, &MainWindow::onAreatriggerClicked);
    connect(m_viewer3d, &render::SceneView3D::graveyardClicked,
            this, &MainWindow::onGraveyardClicked);
    connect(m_viewer, &render::NavMeshView::hoverChanged,
            this, &MainWindow::onViewerHover);
    connect(m_viewer, &render::NavMeshView::clicked,
            this, &MainWindow::onViewerClick);
    connect(m_viewer, &render::NavMeshView::spawnClicked,
            this, &MainWindow::onSpawnClicked);
    connect(m_viewer, &render::NavMeshView::spawnHovered,
            this, &MainWindow::onSpawnHovered);
    connect(m_viewer, &render::NavMeshView::spawnMoved,
            this, &MainWindow::onSpawnMoved);
    connect(m_viewer, &render::NavMeshView::spawnSelectionChanged,
            this, &MainWindow::onSpawnSelectionChanged);
    connect(m_viewer, &render::NavMeshView::pathClicked,
            this, &MainWindow::onPathClicked);
    connect(m_viewer, &render::NavMeshView::pathNodeClicked,
            this, &MainWindow::onPathNodeClicked);
    connect(m_viewer, &render::NavMeshView::pathNodeMoved,
            this, &MainWindow::onPathNodeMoved);
    connect(m_viewer, &render::NavMeshView::pathNodeContextMenuRequested,
            this, &MainWindow::onPathNodeContextMenu);
    connect(m_viewer, &render::NavMeshView::pathSegmentContextMenuRequested,
            this, &MainWindow::onPathSegmentContextMenu);
    connect(m_viewer, &render::NavMeshView::spawnContextMenuRequested,
            this, &MainWindow::onSpawnContextMenu);
    connect(m_viewer, &render::NavMeshView::areatriggerClicked,
            this, &MainWindow::onAreatriggerClicked);
    connect(m_viewer, &render::NavMeshView::graveyardClicked,
            this, &MainWindow::onGraveyardClicked);
    connect(m_viewer, &render::NavMeshView::annotationClicked,
            this, &MainWindow::onAnnotationClicked);
    connect(m_viewer, &render::NavMeshView::annotationHovered,
            this, &MainWindow::onAnnotationHovered);
    connect(m_viewer, &render::NavMeshView::placementRequested,
            this, &MainWindow::onPlacementRequested);
    // 3D viewer carries the authored surface Z (depth-unprojected) -- route
    // it through the Z-aware overload so the clicked height is stored, not
    // recomputed at runtime.
    if (m_viewer3d)
    {
        // 3D placement: the viewer hands us the pick ray; we march it against
        // the authoritative terrain/WMO height (snapToGround) to resolve the
        // surface point, then place there with the authored Z.
        connect(m_viewer3d, &render::SceneView3D::placementRayRequested,
                this, &MainWindow::onPlacementRay);
    }
}

void MainWindow::buildAnnotationToolbox()
{
    auto* dock = new QDockWidget(tr("Annotation toolbox"), this);
    dock->setObjectName(QStringLiteral("annot_toolbox"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_annotationToolbox = new app::AnnotationToolbox(dock);
    dock->setWidget(m_annotationToolbox);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    connect(m_annotationToolbox, &app::AnnotationToolbox::placeModeChanged,
            this, &MainWindow::onPlaceModeChanged);
    connect(m_annotationToolbox, &app::AnnotationToolbox::selectedRowRadiusChanged,
            this, &MainWindow::onSelectedRadiusChanged);
    connect(m_annotationToolbox, &app::AnnotationToolbox::selectedRowLabelChanged,
            this, &MainWindow::onSelectedLabelChanged);
    connect(m_annotationToolbox, &app::AnnotationToolbox::selectedRowNotesChanged,
            this, &MainWindow::onSelectedNotesChanged);
    connect(m_annotationToolbox, &app::AnnotationToolbox::deleteSelectedRequested,
            this, &MainWindow::onDeleteSelectedAnnotation);
    connect(m_annotationToolbox, &app::AnnotationToolbox::commitRequested,
            this, &MainWindow::onCommitAnnotations);
    connect(m_annotationToolbox, &app::AnnotationToolbox::revertRequested,
            this, &MainWindow::onRevertAnnotations);
}

void MainWindow::buildSpawnDock()
{
    auto* dock = new QDockWidget(tr("Spawn properties"), this);
    dock->setObjectName(QStringLiteral("spawn_dock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_spawnEditor = new app::SpawnPropertiesEditor(dock);
    dock->setWidget(m_spawnEditor);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::rowEdited,
            this, &MainWindow::onSpawnRowEdited);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::deleteRequested,
            this, &MainWindow::onDeleteSelectedSpawn);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::commitRequested,
            this, &MainWindow::onCommitSpawns);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::revertRequested,
            this, &MainWindow::onRevertSpawns);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::bulkEditRequested,
            this, &MainWindow::onBulkEditSpawns);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::editAddonRequested,
            this, &MainWindow::onEditCreatureAddonForSelectedSpawn);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::editGameObjectAddonRequested,
            this, &MainWindow::onEditGameObjectAddonForSelectedSpawn);
    // SmartAI authoring + spawn-pool membership for the selected spawn reuse
    // the existing menu flows (onAddSmartScript seeds a row from the spawn's
    // entry; onShowGroupsPoolsDialog opens Groups & Pools).
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::editSmartAiRequested,
            this, &MainWindow::onAddSmartScript);
    connect(m_spawnEditor, &app::SpawnPropertiesEditor::editPoolRequested,
            this, &MainWindow::onShowGroupsPoolsDialog);

    addDockWidget(Qt::RightDockWidgetArea, dock);

    auto* annotDock = new QDockWidget(tr("Annotation properties"), this);
    annotDock->setObjectName(QStringLiteral("annot_dock"));
    annotDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_annotPropertyDock = new app::AnnotationPropertiesDock(annotDock);
    annotDock->setWidget(m_annotPropertyDock);
    addDockWidget(Qt::RightDockWidgetArea, annotDock);
    tabifyDockWidget(dock, annotDock);

    connect(m_annotPropertyDock, &app::AnnotationPropertiesDock::rowEdited,
            this, &MainWindow::onAnnotationRowEdited);
    connect(m_annotPropertyDock, &app::AnnotationPropertiesDock::deleteRequested,
            this, &MainWindow::onDeleteSelectedAnnotation);
    connect(m_annotPropertyDock, &app::AnnotationPropertiesDock::commitRequested,
            this, &MainWindow::onCommitAnnotations);
    connect(m_annotPropertyDock, &app::AnnotationPropertiesDock::revertRequested,
            this, &MainWindow::onRevertAnnotations);

    auto* diagDock = new QDockWidget(tr("Spawn diagnostics"), this);
    diagDock->setObjectName(QStringLiteral("diag_dock"));
    diagDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_diagDock = new app::SpawnDiagnosticsDock(diagDock);
    diagDock->setWidget(m_diagDock);
    addDockWidget(Qt::RightDockWidgetArea, diagDock);
    tabifyDockWidget(dock, diagDock);

    // MinimapDiagnosticsDock is hosted by the unified InfoInspectorDock
    // (see buildInfoInspectorDock); its signal wiring lives there too.

    // VendorInventoryDock stays as its own right-area dock (it's an
    // interactive editor with model edits, not a read-only info panel).
    auto* vendorDockWidget = new QDockWidget(tr("Vendor inventory"), this);
    vendorDockWidget->setObjectName(QStringLiteral("vendor_dock"));
    vendorDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_vendorDock = new app::VendorInventoryDock(nullptr, vendorDockWidget);
    vendorDockWidget->setWidget(m_vendorDock);
    addDockWidget(Qt::RightDockWidgetArea, vendorDockWidget);
    tabifyDockWidget(dock, vendorDockWidget);

    // ConditionsDock is similarly an interactive editor with model edits
    // (commit / revert against ConditionsModel) so it stays separate.
    auto* condDockWidget = new QDockWidget(tr("Conditions"), this);
    condDockWidget->setObjectName(QStringLiteral("conditions_dock"));
    condDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_conditionsDock = new app::ConditionsDock(condDockWidget);
    m_conditionsDock->setModel(m_conditionsModel.get());
    m_conditionsDock->setUndoManager(m_undo);
    condDockWidget->setWidget(m_conditionsDock);
    addDockWidget(Qt::RightDockWidgetArea, condDockWidget);
    tabifyDockWidget(dock, condDockWidget);

    // Worldserver log tail dock — operator wants this visible at the
    // BOTTOM during long ops, so it sits in the bottom area rather than
    // being folded into the InfoInspector.
    auto* logTailDockWidget = new QDockWidget(tr("Worldserver log tail"), this);
    logTailDockWidget->setObjectName(QStringLiteral("log_tail_dock"));
    logTailDockWidget->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_logTailDock = new app::LogTailDock(logTailDockWidget);
    logTailDockWidget->setWidget(m_logTailDock);
    addDockWidget(Qt::BottomDockWidgetArea, logTailDockWidget);
    logTailDockWidget->hide();
    connect(m_diagDock, &app::SpawnDiagnosticsDock::addLinkedRespawnRequested,
            this, &MainWindow::onAddLinkedRespawn);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::removeLinkedRespawnRequested,
            this, &MainWindow::onRemoveLinkedRespawn);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::addGameEventRequested,
            this, &MainWindow::onAddGameEvent);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::removeGameEventRequested,
            this, &MainWindow::onRemoveGameEvent);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::addSmartScriptRequested,
            this, &MainWindow::onAddSmartScript);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::editSmartScriptRequested,
            this, &MainWindow::onEditSmartScript);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::removeSmartScriptRequested,
            this, &MainWindow::onRemoveSmartScript);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::addTransportRequested,
            this, &MainWindow::onAddTransport);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::editTransportRequested,
            this, &MainWindow::onEditTransport);
    connect(m_diagDock, &app::SpawnDiagnosticsDock::removeTransportRequested,
            this, &MainWindow::onRemoveTransport);
}

void MainWindow::onSpawnRowEdited(render::Spawn const& proposed)
{
    if (!m_spawnModel || m_selectedSpawnIndex < 0)
        return;
    int const idx = m_selectedSpawnIndex;
    bool const changed = m_undo->recordIf(m_spawnModel.get(),
        tr("Edit spawn"), [&]() {
        return m_spawnModel->replaceRow(idx, proposed);
    });
    if (changed)
    {
        pushSpawnsToViewer();
        if (m_spawnEditor)
            m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
    }
}

void MainWindow::onSpawnMoved(int spawnIndex, float worldX, float worldY)
{
    // 2D top-down drag: no Z from the click, so the core snaps/keeps Z.
    applySpawnMove(spawnIndex, worldX, worldY, std::nullopt);
}

void MainWindow::onSpawnMoved3D(int spawnIndex, float worldX, float worldY, float worldZ)
{
    // 3D drag carries the drag-plane Z (constant altitude).  The core keeps
    // it when snap-to-ground is off (flying mobs) or re-grounds when it's on.
    applySpawnMove(spawnIndex, worldX, worldY, worldZ);
}

void MainWindow::applySpawnMove(int spawnIndex, float worldX, float worldY,
                                std::optional<float> draggedZ)
{
    if (!m_viewer || !m_spawnModel || spawnIndex < 0)
        return;
    // Viewer index is into the FILTERED display list; resolve back to
    // the SpawnModel by matching (kind, guid) on the dragged row.
    if (spawnIndex >= int(m_viewer->spawns().size()))
        return;
    // Copy, not reference: pushSpawnsToViewer() below replaces the
    // viewer's vector and would invalidate a reference held across that
    // call (real crash observed on drag-release, crash-2026-05-26).
    render::Spawn const display = m_viewer->spawns()[spawnIndex];

    auto const& current = m_spawnModel->current();
    int modelIndex = -1;
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (current[i].kind == display.kind && current[i].guid == display.guid)
        {
            modelIndex = static_cast<int>(i);
            break;
        }
    }
    if (modelIndex < 0)
        return;

    render::Spawn updated = current[modelIndex];
    updated.worldX = worldX;
    updated.worldY = worldY;

    // Z resolution priority:
    //   1. snap-to-ground ON  -> probe the floor at the new XY (composes ADT
    //      terrain + WMO height probe so a spawn dragged onto an upper floor
    //      sticks there).  The dragged-plane Z, when present, seeds the probe
    //      so we snap to the floor nearest the altitude the operator dropped at.
    //   2. snap OFF + 3D drag -> keep the drag-plane Z (preserves the altitude
    //      of flying mobs / floating GOs).
    //   3. snap OFF + 2D drag -> keep the existing Z (top-down has no Z to give).
    float const seedZ = draggedZ ? *draggedZ : updated.worldZ;
    if (m_spawnEditor && m_spawnEditor->snapToGroundEnabled())
    {
        updated.worldZ = snapToGround(updated.mapId, worldX, worldY,
                                      /*probeZ*/ seedZ + 5.0f,
                                      /*fallback*/ seedZ);
    }
    else if (draggedZ)
    {
        updated.worldZ = *draggedZ;
    }

    bool const moved = m_undo->recordIf(m_spawnModel.get(),
        tr("Move spawn"), [&]() {
        return m_spawnModel->replaceRow(modelIndex, updated);
    });
    if (moved)
    {
        // Re-select the moved row so the property editor refreshes.
        m_selectedSpawnIndex = modelIndex;
        if (m_spawnEditor)
        {
            m_spawnEditor->setRow(modelIndex, updated);
            m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
        }
        pushSpawnsToViewer();
        statusBar()->showMessage(
            tr("Moved %1 guid=%2 -> (%3, %4) - pending commit")
              .arg(display.kind == render::SpawnKind::Creature
                   ? QStringLiteral("creature") : QStringLiteral("GO"))
              .arg(display.guid)
              .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1),
            3000);
    }
}

void MainWindow::onDeleteSelectedSpawn()
{
    if (!m_spawnModel) return;
    // Multi-select: delete every selected row.  We iterate in descending
    // index so removals from m_current don't invalidate the indices yet
    // to come (removeRow currently only erases on Insert-cancel; DB-backed
    // rows are marked Delete and kept in m_current with a Delete flag,
    // so technically indices remain valid - but iterating descending is
    // future-proof if the model ever changes).
    if (m_spawnSelection.size() > 1)
    {
        QVector<int> sorted = m_spawnSelection;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>());
        size_t removed = 0;
        m_undo->recordOn(m_spawnModel.get(),
            tr("Delete %1 spawns").arg(sorted.size()), [&]() {
            for (int idx : sorted)
            {
                if (m_spawnModel->removeRow(idx))
                    ++removed;
            }
        });
        m_spawnSelection.clear();
        m_selectedSpawnIndex = -1;
        if (m_spawnEditor)
        {
            m_spawnEditor->setBulkMode(0);
            m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
        }
        pushSpawnsToViewer();
        statusBar()->showMessage(tr("Marked %1 spawn(s) for delete").arg(removed), 3000);
        return;
    }
    if (m_selectedSpawnIndex < 0)
        return;
    int const idx = m_selectedSpawnIndex;
    bool const changed = m_undo->recordIf(m_spawnModel.get(),
        tr("Delete spawn"), [&]() {
        return m_spawnModel->removeRow(idx);
    });
    if (changed)
    {
        m_selectedSpawnIndex = -1;
        if (m_spawnEditor)
        {
            m_spawnEditor->clear();
            m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
        }
        pushSpawnsToViewer();
    }
}

void MainWindow::onSpawnSelectionChanged(QVector<int> const& indices)
{
    m_spawnSelection = indices;
    // Toggle the Bulk-transform menu action's enabled state to mirror
    // the selection.  Lookup is by objectName to avoid carrying a
    // QAction* member just for this one toggle.
    if (auto* act = findChild<QAction*>(QStringLiteral("bulk_transform_action")))
        act->setEnabled(!m_spawnSelection.isEmpty());
    if (auto* act = findChild<QAction*>(QStringLiteral("diff_pair_action")))
    {
        act->setEnabled(m_spawnSelection.size() == 2);
        act->setToolTip(tr("Select exactly 2 spawns to enable this."));
    }
    if (auto* act = findChild<QAction*>(QStringLiteral("propagate_fields_action")))
        act->setEnabled(m_spawnSelection.size() == 1);
    if (auto* act = findChild<QAction*>(QStringLiteral("find_similar_action")))
        act->setEnabled(m_spawnSelection.size() == 1);
    // Export CSV is enabled when EITHER selection is non-empty OR the loaded map has spawns;
    // the selection branch is needed so the action lights up before pushSpawnsToViewer fires.
    if (auto* act = findChild<QAction*>(QStringLiteral("export_spawns_csv_action")))
    {
        bool const hasAny = m_spawnModel && !m_spawnModel->current().empty();
        act->setEnabled(!m_spawnSelection.isEmpty() || hasAny);
    }
    if (!m_spawnEditor)
        return;

    if (indices.size() == 1)
    {
        // Single-select: behave as today (single-row editor).
        int const idx = indices.first();
        if (idx >= 0 && m_spawnModel && idx < int(m_spawnModel->current().size()))
        {
            m_selectedSpawnIndex = idx;
            m_spawnEditor->setRow(idx, m_spawnModel->current()[idx]);
            m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
        }
    }
    else if (indices.size() > 1)
    {
        m_selectedSpawnIndex = -1;
        m_spawnEditor->setBulkMode(indices.size());
        m_spawnEditor->setPendingCount(m_spawnModel ? m_spawnModel->pendingCount() : 0);
        // Multi-select has no single "entry" anchor, so drop any sibling
        // ring set from the previous single-click selection.
        if (m_viewer)
            m_viewer->setHighlightedSiblings({});
    }
    else
    {
        m_selectedSpawnIndex = -1;
        m_spawnEditor->setBulkMode(0);
        m_spawnEditor->setPendingCount(m_spawnModel ? m_spawnModel->pendingCount() : 0);
        // Empty selection (click on empty space, Esc, etc) -> clear the
        // sibling highlight overlay.
        if (m_viewer)
            m_viewer->setHighlightedSiblings({});
    }
}

void MainWindow::onBulkEditSpawns()
{
    if (!m_spawnModel || m_spawnSelection.size() < 2)
        return;
    app::BulkEditDialog dlg(*m_spawnModel, m_spawnSelection, this, m_undo);
    if (dlg.exec() != QDialog::Accepted)
        return;
    int const n = dlg.rowsTouched();
    pushSpawnsToViewer();
    if (m_spawnEditor)
        m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
    statusBar()->showMessage(tr("Bulk edit applied to %1 spawns.").arg(n), 3000);
}

void MainWindow::onDiffSelectedSpawnPair()
{
    if (!m_spawnModel || m_spawnSelection.size() != 2)
        return;
    auto const& rows = m_spawnModel->current();
    int const ia = m_spawnSelection[0];
    int const ib = m_spawnSelection[1];
    if (ia < 0 || ib < 0 || ia >= int(rows.size()) || ib >= int(rows.size()))
        return;
    app::SpawnDiffDialog dlg(rows[ia], rows[ib], this);
    dlg.exec();
}

void MainWindow::onPropagateFieldsFromSelection()
{
    // Single-canonical contract: only fires with exactly one selected spawn.
    if (!m_spawnModel || m_spawnSelection.size() != 1)
        return;

    auto const& rows = m_spawnModel->current();
    int const idx = m_spawnSelection.first();
    if (idx < 0 || idx >= int(rows.size()))
        return;

    render::Spawn const canonical = rows[idx];

    // Collect receivers: same kind + entry + mapId, different guid.
    // Filter by guid (not array index) so locally-inserted rows with
    // negative guids still get a unique identity, but the canonical row
    // itself is skipped.
    QVector<render::Spawn> receivers;
    receivers.reserve(int(rows.size()));
    for (render::Spawn const& s : rows)
    {
        if (s.guid == canonical.guid && s.kind == canonical.kind)
            continue;
        if (s.kind != canonical.kind) continue;
        if (s.entry != canonical.entry) continue;
        if (s.mapId != canonical.mapId) continue;
        receivers.push_back(s);
    }

    app::PropagateFieldsDialog dlg(canonical, receivers, this);
    connect(&dlg, &app::PropagateFieldsDialog::propagateRequested,
            this, &MainWindow::onPropagateRequested);
    dlg.exec();
}

void MainWindow::onPropagateRequested(render::Spawn const& canonical, QSet<QString> selectedFields)
{
    if (!m_spawnModel || selectedFields.isEmpty())
        return;

    // Re-walk the live model rather than trusting the dialog's snapshot,
    // so an undo/redo or external edit between dialog-open and Apply can't
    // desync our indices.  Receivers are matched on kind + entry + mapId,
    // skipping the canonical row itself by guid.
    auto const& rows = m_spawnModel->current();
    QVector<int> receiverIndices;
    receiverIndices.reserve(int(rows.size()));
    for (int i = 0; i < int(rows.size()); ++i)
    {
        render::Spawn const& s = rows[i];
        if (s.kind != canonical.kind) continue;
        if (s.entry != canonical.entry) continue;
        if (s.mapId != canonical.mapId) continue;
        if (s.guid == canonical.guid) continue;
        receiverIndices.push_back(i);
    }
    if (receiverIndices.isEmpty())
        return;

    // Field-token -> mutation helper.  Hidden / unchecked tokens are
    // absent from `selectedFields`, so the lambdas never run for them.
    auto applyAll = [&]() {
        int touched = 0;
        for (int rIdx : receiverIndices)
        {
            if (rIdx < 0 || rIdx >= int(m_spawnModel->current().size()))
                continue;
            render::Spawn row = m_spawnModel->current()[rIdx];

            using D = app::PropagateFieldsDialog;
            if (selectedFields.contains(D::kSpawntimesecs))     row.spawntimesecs     = canonical.spawntimesecs;
            if (selectedFields.contains(D::kPhaseUseFlags))     row.phaseUseFlags     = canonical.phaseUseFlags;
            if (selectedFields.contains(D::kPhaseId))           row.phaseId           = canonical.phaseId;
            if (selectedFields.contains(D::kPhaseGroup))        row.phaseGroup        = canonical.phaseGroup;
            if (selectedFields.contains(D::kSpawnDifficulties)) row.spawnDifficulties = canonical.spawnDifficulties;
            if (selectedFields.contains(D::kScriptName))        row.scriptName        = canonical.scriptName;
            if (selectedFields.contains(D::kStringId))          row.stringId          = canonical.stringId;
            if (selectedFields.contains(D::kNpcflag))           row.npcflag           = canonical.npcflag;
            if (selectedFields.contains(D::kUnitFlags1))        row.unitFlags1        = canonical.unitFlags1;
            if (selectedFields.contains(D::kUnitFlags2))        row.unitFlags2        = canonical.unitFlags2;
            if (selectedFields.contains(D::kUnitFlags3))        row.unitFlags3        = canonical.unitFlags3;
            if (selectedFields.contains(D::kMovementType))      row.movementType      = canonical.movementType;
            if (selectedFields.contains(D::kModelid))           row.modelid           = canonical.modelid;
            if (selectedFields.contains(D::kEquipmentId))       row.equipmentId       = canonical.equipmentId;
            if (selectedFields.contains(D::kCurHealthPct))      row.curHealthPct      = canonical.curHealthPct;
            if (selectedFields.contains(D::kWanderDistance))    row.wanderDistance    = canonical.wanderDistance;
            if (selectedFields.contains(D::kRotation0))         row.rotation0         = canonical.rotation0;
            if (selectedFields.contains(D::kRotation1))         row.rotation1         = canonical.rotation1;
            if (selectedFields.contains(D::kRotation2))         row.rotation2         = canonical.rotation2;
            if (selectedFields.contains(D::kRotation3))         row.rotation3         = canonical.rotation3;
            if (selectedFields.contains(D::kGoState))           row.goState           = canonical.goState;
            if (selectedFields.contains(D::kAnimprogress))      row.animprogress      = canonical.animprogress;

            if (m_spawnModel->replaceRow(rIdx, row))
                ++touched;
        }
        // Stash row-touched count outside the lambda via a captured ref.
        // Done by writing back into a sibling variable below.
        m_propagateLastTouched = touched;
    };

    m_propagateLastTouched = 0;

    // Single recordOn frame so the entire propagation is one Ctrl+Z step.
    if (m_undo)
        m_undo->recordOn(m_spawnModel.get(),
                         tr("Propagate fields to %1 spawns").arg(receiverIndices.size()),
                         applyAll);
    else
        applyAll();

    pushSpawnsToViewer();
    if (m_spawnEditor)
        m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());

    statusBar()->showMessage(tr("Propagated %1 field(s) to %2 spawns of entry %3.")
                              .arg(selectedFields.size())
                              .arg(m_propagateLastTouched)
                              .arg(canonical.entry), 5000);
}

void MainWindow::onBulkTransformSpawns()
{
    if (!m_spawnModel || m_spawnSelection.isEmpty())
        return;

    // Snapshot current rows for the dialog preview.  Out-of-range
    // indices are silently dropped so a stale selection doesn't crash.
    QVector<render::Spawn> selectedRows;
    selectedRows.reserve(m_spawnSelection.size());
    auto const& rows = m_spawnModel->current();
    for (int idx : m_spawnSelection)
        if (idx >= 0 && idx < int(rows.size()))
            selectedRows.push_back(rows[idx]);
    if (selectedRows.isEmpty())
        return;

    app::BulkTransformDialog dlg(selectedRows, this);
    connect(&dlg, &app::BulkTransformDialog::transformRequested,
            this, &MainWindow::onTransformRequested);
    dlg.exec();
}

void MainWindow::onTransformRequested(float dx, float dy, float dz,
                                      float rotDegrees, bool rotateAroundCentroid,
                                      float scale)
{
    if (!m_spawnModel || m_spawnSelection.isEmpty())
        return;

    // Re-snapshot from the LIVE model (rather than trusting the dialog
    // payload) so an undo+redo cycle between dialog open and Apply
    // can't desync our indices.
    auto const& rows = m_spawnModel->current();
    QVector<int> validSel;
    validSel.reserve(m_spawnSelection.size());
    for (int idx : m_spawnSelection)
        if (idx >= 0 && idx < int(rows.size()))
            validSel.push_back(idx);
    if (validSel.isEmpty())
        return;

    // Centroid over the current selection (XY+Z).  Used for both the
    // rotate-around-centroid and the scale-around-centroid transforms.
    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (int idx : validSel)
    {
        sx += rows[idx].worldX;
        sy += rows[idx].worldY;
        sz += rows[idx].worldZ;
    }
    double const cx = sx / double(validSel.size());
    double const cy = sy / double(validSel.size());
    double const cz = sz / double(validSel.size());

    // Use a local Pi constant to keep this TU free of <cmath> M_PI
    // platform-define dance (Windows requires _USE_MATH_DEFINES).
    constexpr double kPi = 3.14159265358979323846;
    double const radians = double(rotDegrees) * (kPi / 180.0);
    double const cosR    = std::cos(radians);
    double const sinR    = std::sin(radians);

    int const count = validSel.size();
    int touched = 0;

    // Single recordOn frame so Ctrl+Z reverses the whole batch atomically.
    m_undo->recordOn(m_spawnModel.get(),
        tr("Bulk transform %1 spawns").arg(count), [&]() {
        for (int idx : validSel)
        {
            render::Spawn row = m_spawnModel->current()[idx];

            // Translate to centroid origin, rotate (XY), scale, then add
            // back centroid + caller's (dx, dy, dz) translation.
            double ox = double(row.worldX) - cx;
            double oy = double(row.worldY) - cy;
            double oz = double(row.worldZ) - cz;

            if (rotateAroundCentroid && rotDegrees != 0.0f)
            {
                double const rx = ox * cosR - oy * sinR;
                double const ry = ox * sinR + oy * cosR;
                ox = rx;
                oy = ry;
            }
            if (scale != 1.0f)
            {
                ox *= double(scale);
                oy *= double(scale);
                oz *= double(scale);
            }

            row.worldX = float(cx + ox + double(dx));
            row.worldY = float(cy + oy + double(dy));
            row.worldZ = float(cz + oz + double(dz));
            // Orientation: every spawn rotates by the same delta whether
            // or not positions move - matches user expectation that
            // "rotate 90 deg" turns the bots regardless of mode.
            row.orientation = float(double(row.orientation) + radians);

            if (m_spawnModel->replaceRow(idx, row))
                ++touched;
        }
    });

    pushSpawnsToViewer();
    if (m_spawnEditor)
        m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
    statusBar()->showMessage(tr("Transformed %1 spawns.").arg(touched), 3000);
}

void MainWindow::onRevertSpawns()
{
    if (!m_spawnModel) return;
    m_spawnModel->revertAll();
    m_selectedSpawnIndex = -1;
    if (m_spawnEditor)
    {
        m_spawnEditor->clear();
        m_spawnEditor->setPendingCount(0);
    }
    pushSpawnsToViewer();
    statusBar()->showMessage(tr("Spawn edits reverted"), 2000);
}

void MainWindow::onCommitSpawns()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before committing."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map before committing spawn edits."));
        return;
    }
    if (!m_spawnModel || m_spawnModel->pendingCount() == 0)
    {
        statusBar()->showMessage(tr("Nothing to commit."), 2000);
        return;
    }

    app::SpawnCommitDialog dlg(m_worldDb.get(), *m_spawnModel, *m_currentMapId, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto committed = dlg.committedRows();
    size_t const creatureCount = std::count_if(committed.begin(), committed.end(),
        [](render::Spawn const& s) { return s.kind == render::SpawnKind::Creature; });
    size_t const goCount = committed.size() - creatureCount;
    m_spawnStatsLabel->setText(tr("creatures=%1  GOs=%2").arg(creatureCount).arg(goCount));

    m_spawnModel->acceptCommit(std::move(committed));
    m_selectedSpawnIndex = -1;
    if (m_spawnEditor)
    {
        m_spawnEditor->clear();
        m_spawnEditor->setPendingCount(0);
    }
    pushSpawnsToViewer();
    refreshGuidReservation();  // bump local block to MAX(guid)+1 across the table.
    statusBar()->showMessage(tr("Spawn commit applied."), 3000);
}

void MainWindow::buildMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* openMmapsAction = fileMenu->addAction(tr("Set &mmaps directory..."));
    connect(openMmapsAction, &QAction::triggered, this, &MainWindow::onOpenMmapsDir);

    auto* openMapsAction = fileMenu->addAction(tr("Set m&aps directory..."));
    connect(openMapsAction, &QAction::triggered, this, &MainWindow::onOpenMapsDir);

    auto* openMinimapAction = fileMenu->addAction(tr("Set m&inimap directory..."));
    connect(openMinimapAction, &QAction::triggered, this, &MainWindow::onSetMinimapDir);

    auto* openVmapsAction = fileMenu->addAction(tr("Set &vmaps directory..."));
    connect(openVmapsAction, &QAction::triggered, this, &MainWindow::onOpenVmapsDir);

    auto* openCascAction = fileMenu->addAction(tr("Set Wo&W client directory..."));
    openCascAction->setToolTip(tr(
        "Point the editor at a live WoW install (e.g. C:/World of Warcraft) "
        "to enable live BLP minimap reads from CASC.  Subdir _retail_ / "
        "_classic_ / _ptr_ is auto-resolved."));
    connect(openCascAction, &QAction::triggered, this, &MainWindow::onSetCascClientDir);

    auto* openListfileAction = fileMenu->addAction(tr("Set &listfile CSV..."));
    openListfileAction->setToolTip(tr(
        "Load a wow-listfile (https://github.com/wowdev/wow-listfile) CSV so "
        "the minimap loader can resolve FileDataIDs.  REQUIRED on modern "
        "(TWW build 67186+) client data because many minimap BLPs have no "
        "virtual path in the CASC root."));
    connect(openListfileAction, &QAction::triggered, this, &MainWindow::onSetListfileCsv);

    fileMenu->addSeparator();

    auto* openMapAction = fileMenu->addAction(tr("&Open map..."));
    openMapAction->setShortcut(QKeySequence::Open);
    connect(openMapAction, &QAction::triggered, this, &MainWindow::onOpenMap);

    // File -> Recent maps: dynamic submenu of the last 15 mapIds opened,
    // sourced from QSettings "editor/recent_maps".  Built on aboutToShow
    // so a newly-opened map appears the next time the menu is shown.
    m_recentMapsMenu = fileMenu->addMenu(tr("&Recent maps"));
    connect(m_recentMapsMenu, &QMenu::aboutToShow, this, &MainWindow::onRebuildRecentMapsMenu);
    onRebuildRecentMapsMenu();

    auto* findAction = fileMenu->addAction(tr("&Find / Jump..."));
    findAction->setShortcut(QKeySequence::Find);  // Ctrl+F
    connect(findAction, &QAction::triggered, this, &MainWindow::onShowFindJumpDialog);

    fileMenu->addSeparator();

    m_exportPendingAction = fileMenu->addAction(tr("&Export pending changes..."));
    m_exportPendingAction->setEnabled(false);
    connect(m_exportPendingAction, &QAction::triggered,
            this, &MainWindow::onExportPendingChanges);

    // Bulk CSV import for spawn rows.  Modal dialog parses + previews; on accept,
    // MainWindow wraps every row's addNew() in a single UndoManager frame.
    auto* importCsvAction = fileMenu->addAction(tr("&Import spawns from CSV..."));
    connect(importCsvAction, &QAction::triggered,
            this, &MainWindow::onImportSpawnsFromCsv);

    // Export-spawns counterpart.  Writes selection-or-all to a CSV that re-imports
    // cleanly via the dialog above.  Enabled either when a selection is non-empty or
    // when the current map has at least one spawn loaded; onSpawnSelectionChanged +
    // pushSpawnsToViewer refresh the state via objectName lookup.
    auto* exportCsvAction = fileMenu->addAction(tr("E&xport spawns to CSV..."));
    exportCsvAction->setObjectName(QStringLiteral("export_spawns_csv_action"));
    exportCsvAction->setEnabled(false);
    connect(exportCsvAction, &QAction::triggered,
            this, &MainWindow::onExportSpawnsToCsv);

    fileMenu->addSeparator();

    // Screenshot exports.  Reads the visible central widget's GL backbuffer
    // (grabFramebuffer) and writes a PNG.  High-res variant upscales 4x.
    auto* exportViewPngAction = fileMenu->addAction(tr("Export &view as PNG..."));
    connect(exportViewPngAction, &QAction::triggered,
            this, &MainWindow::onExportViewAsPng);

    auto* exportViewPngHiResAction = fileMenu->addAction(
        tr("Export view as PNG (&high-res 4x)..."));
    connect(exportViewPngHiResAction, &QAction::triggered,
            this, &MainWindow::onExportViewAsPngHighRes);

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto* undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &MainWindow::onUndo);
    auto* redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    QMenu* dbMenu = menuBar()->addMenu(tr("&Database"));
    auto* dbConnectAction = dbMenu->addAction(tr("&Connect..."));
    connect(dbConnectAction, &QAction::triggered, this, &MainWindow::onDbConnect);
    auto* dbDisconnectAction = dbMenu->addAction(tr("&Disconnect"));
    connect(dbDisconnectAction, &QAction::triggered, this, &MainWindow::onDbDisconnect);

    // Create menu: every "place a new world object" verb in one predictable
    // place.  These were previously scattered across four top-level menus
    // (Spawn / Path / Areatrigger / Graveyard -- the latter two existing
    // solely to hold one action each).  Each action arms the corresponding
    // placement mode; the status-bar mode badge shows how to finish/exit.
    QMenu* createMenu = menuBar()->addMenu(tr("&Create"));
    auto* newFromTemplateAction = createMenu->addAction(tr("New &spawn from template..."));
    newFromTemplateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newFromTemplateAction, &QAction::triggered,
            this, &MainWindow::onNewSpawnFromTemplate);
    auto* newPathAction = createMenu->addAction(tr("New &path"));
    newPathAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(newPathAction, &QAction::triggered, this, &MainWindow::onNewPath);
    auto* newAtrAction = createMenu->addAction(tr("New &areatrigger from create-properties..."));
    newAtrAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A));
    connect(newAtrAction, &QAction::triggered,
            this, &MainWindow::onNewAreatriggerFromCreateProps);
    auto* newGyAction = createMenu->addAction(tr("New &graveyard..."));
    newGyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G));
    connect(newGyAction, &QAction::triggered, this, &MainWindow::onNewGraveyard);

    QMenu* spawnMenu = menuBar()->addMenu(tr("&Spawn"));
    auto* addToGroupAction = spawnMenu->addAction(tr("&Add selected creature to group..."));
    connect(addToGroupAction, &QAction::triggered,
            this, &MainWindow::onAddSelectedSpawnToGroup);

    // Bulk transform action.  Enabled only while >=1 spawn is selected.
    auto* bulkTransformAction = spawnMenu->addAction(tr("Bulk &transform selected..."));
    bulkTransformAction->setObjectName(QStringLiteral("bulk_transform_action"));
    bulkTransformAction->setEnabled(!m_spawnSelection.isEmpty());
    connect(bulkTransformAction, &QAction::triggered,
            this, &MainWindow::onBulkTransformSpawns);

    // Diff-selected-pair action.  Enabled only with exactly 2 spawns picked;
    // onSpawnSelectionChanged toggles state via objectName lookup.
    auto* diffPairAction = spawnMenu->addAction(tr("&Diff selected pair..."));
    diffPairAction->setObjectName(QStringLiteral("diff_pair_action"));
    diffPairAction->setEnabled(m_spawnSelection.size() == 2);
    diffPairAction->setToolTip(tr("Select exactly 2 spawns to enable this."));
    connect(diffPairAction, &QAction::triggered,
            this, &MainWindow::onDiffSelectedSpawnPair);

    // Propagate-fields-from-selection action.  Enabled only when exactly
    // one spawn is selected (the canonical row).  onSpawnSelectionChanged
    // toggles the action's enabled state via objectName lookup.
    auto* propagateFieldsAction = spawnMenu->addAction(tr("&Propagate fields from selection..."));
    propagateFieldsAction->setObjectName(QStringLiteral("propagate_fields_action"));
    propagateFieldsAction->setEnabled(m_spawnSelection.size() == 1);
    propagateFieldsAction->setToolTip(tr(
        "Copy selected per-row fields from the single selected spawn to "
        "every other spawn of the same entry on the same map."));
    connect(propagateFieldsAction, &QAction::triggered,
            this, &MainWindow::onPropagateFieldsFromSelection);

    // Spawn -> Find similar spawns... opens FindSimilarDialog keyed on
    // the currently selected spawn.  Enabled only with exactly one
    // spawn selected; onSpawnSelectionChanged toggles via objectName.
    auto* findSimilarAction = spawnMenu->addAction(tr("&Find similar spawns..."));
    findSimilarAction->setObjectName(QStringLiteral("find_similar_action"));
    findSimilarAction->setEnabled(m_spawnSelection.size() == 1);
    findSimilarAction->setToolTip(tr(
        "Search creature for other spawns sharing entry/map/zone/area/phase/"
        "npcflag/faction/proximity with the selected one."));
    connect(findSimilarAction, &QAction::triggered,
            this, &MainWindow::onShowFindSimilarDialog);

    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto* groupsPoolsAction = toolsMenu->addAction(tr("&Groups && Pools..."));
    connect(groupsPoolsAction, &QAction::triggered,
            this, &MainWindow::onShowGroupsPoolsDialog);

    auto* handcraftedRoadAction = toolsMenu->addAction(tr("&Handcrafted roads..."));
    handcraftedRoadAction->setToolTip(tr(
        "Open the handcrafted-road CRUD dock: list / add / edit / delete "
        "segments in the `handcrafted_road` world-DB table, preview which "
        "navmesh polygons would be retagged NAV_AREA_ROAD, and optionally "
        "apply the corridor to the in-memory navmesh so the gold road "
        "overlay surfaces the change immediately (DB unchanged)."));
    connect(handcraftedRoadAction, &QAction::triggered,
            this, &MainWindow::onToggleHandcraftedRoadDock);

    auto* graveyardZoneAction = toolsMenu->addAction(tr("&Graveyard zones..."));
    connect(graveyardZoneAction, &QAction::triggered,
            this, &MainWindow::onShowGraveyardZoneDialog);

    auto* autoTagAction = toolsMenu->addAction(tr("Auto-tag &NPCs (vendor / innkeeper / mailbox)..."));
    connect(autoTagAction, &QAction::triggered, this, &MainWindow::onAutoTagNpcs);

    auto* exportMinimapAction = toolsMenu->addAction(tr("&Export minimap cache to PNGs..."));
    exportMinimapAction->setToolTip(tr(
        "Decode every BLP tile reachable via CASC for the currently loaded "
        "map and write PNGs into the minimap PNG directory.  Skips tiles "
        "whose PNG is already on disk.  Takes minutes for a full continent."));
    connect(exportMinimapAction, &QAction::triggered, this, &MainWindow::onExportMinimapCache);

    auto* healthAction = toolsMenu->addAction(tr("&Health report..."));
    connect(healthAction, &QAction::triggered, this, &MainWindow::onShowHealthReport);

    // Surface the minimap diagnostics dock without forcing the operator
    // to hunt through the Window/View menu.  Action raises the dock and
    // pumps an immediate refresh.
    auto* minimapDiagAction = toolsMenu->addAction(tr("&Minimap diagnostics"));
    minimapDiagAction->setToolTip(tr(
        "Open the minimap diagnostics dock - shows CASC dir / storage status, "
        "Map.db2 entry count, minimap PNG dir, current map, tile counts, "
        "successful / failed minimap loads, and last-tried tile."));
    connect(minimapDiagAction, &QAction::triggered, this, &MainWindow::onShowMinimapDiagnostics);

    auto* spawnSearchAction = toolsMenu->addAction(tr("Spawn &search..."));
    spawnSearchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    spawnSearchAction->setToolTip(tr(
        "Multi-criteria spawn finder (name/entry/npcflag/faction/map/drops-item). "
        "Double-click a result row to jump the viewer there."));
    connect(spawnSearchAction, &QAction::triggered, this, &MainWindow::onShowSpawnSearchDialog);

    auto* saiFlowAction = toolsMenu->addAction(tr("&Smart script flow..."));
    saiFlowAction->setToolTip(tr(
        "Browse the event/action/linked chain for a given (entryorguid, "
        "source_type).  Expands CALL_TIMED_ACTIONLIST + linked successors "
        "inline so a SAI script reads top-to-bottom."));
    connect(saiFlowAction, &QAction::triggered, this, &MainWindow::onShowSmartScriptFlow);

    auto* saiDryRunAction = toolsMenu->addAction(tr("Smart-script &dry-run..."));
    saiDryRunAction->setToolTip(tr(
        "Heuristic 'what would fire?' simulator for an (entryorguid, source_type) ruleset.  "
        "Pick an event-type / phase / hp / combat / param-value, click Run, see the FIRE / "
        "SKIP trace for every smart_scripts rule under those inputs."));
    connect(saiDryRunAction, &QAction::triggered, this, &MainWindow::onShowSmartScriptDryRun);

    // Lookup NPCText by ID - prompts the operator for an id then pushes
    // it to the NpcTextDock + raises that dock so the result is visible.
    auto* lookupNpcTextAction = toolsMenu->addAction(tr("Lookup &NPCText by ID..."));
    lookupNpcTextAction->setToolTip(tr(
        "Resolve an `npc_text` id (referenced from gossip_menu.TextID, "
        "npc_gossip, scripted gossip handlers) and render its 8 variants "
        "in the NPCText dock."));
    connect(lookupNpcTextAction, &QAction::triggered, this, [this]() {
        if (!m_npcTextDock)
            return;
        bool okPick = false;
        int const id = QInputDialog::getInt(this,
            tr("Lookup NPCText"),
            tr("npc_text.ID:"),
            /*value=*/1,
            /*min=*/0,
            /*max=*/2147483647,
            /*step=*/1,
            &okPick);
        if (!okPick)
            return;
        if (m_infoInspector)
            m_infoInspector->openNpcText(static_cast<uint32_t>(id));
        if (auto* infoDockWidget = findChild<QDockWidget*>(QStringLiteral("info_inspector_dock")))
        {
            infoDockWidget->show();
            infoDockWidget->raise();
        }
    });

    // Areatrigger script registry - opens the dock in summary mode so the
    // operator can browse every distinct ScriptName referenced by the
    // `areatrigger` table.
    auto* atrScriptAction = toolsMenu->addAction(tr("&Areatrigger scripts..."));
    atrScriptAction->setToolTip(tr(
        "Surface which C++ areatrigger scripts (`areatrigger.ScriptName` / "
        "`areatrigger_template.ScriptName`) are registered and where each "
        "is spawned.  Double-click a row to narrow / jump."));
    connect(atrScriptAction, &QAction::triggered, this, [this]() {
        if (!m_atrScriptDock)
            return;
        m_atrScriptDock->setScriptName(QString{});
        if (m_infoInspector)
            m_infoInspector->showPage(app::InfoInspectorDock::Page::AreatriggerScript);
        if (auto* infoDockWidget = findChild<QDockWidget*>(QStringLiteral("info_inspector_dock")))
        {
            infoDockWidget->show();
            infoDockWidget->raise();
        }
    });

    // Tail worldserver log - opens the LogTailDock and focuses its path
    // edit so the operator can immediately type / paste a log file path.
    auto* tailLogAction = toolsMenu->addAction(tr("&Tail worldserver log..."));
    tailLogAction->setToolTip(tr(
        "Open the worldserver log tail dock and focus its path field.  "
        "The dock polls the file every 2 seconds, keeps the last 500 lines, "
        "and color-tints ERROR / WARN / DEBUG lines."));
    connect(tailLogAction, &QAction::triggered, this, [this]() {
        if (!m_logTailDock)
            return;
        if (auto* logTailDockWidget = findChild<QDockWidget*>(QStringLiteral("log_tail_dock")))
        {
            logTailDockWidget->show();
            logTailDockWidget->raise();
        }
        if (auto* edit = m_logTailDock->pathEdit())
            edit->setFocus(Qt::OtherFocusReason);
    });

    // Gossip menu walker - prompts for a root MenuID then opens a modal
    // dialog that recursively walks gossip_menu / gossip_menu_option.
    auto* gossipWalkAction = toolsMenu->addAction(tr("&Gossip menu walker..."));
    gossipWalkAction->setToolTip(tr(
        "Resolve a creature's gossip_menu_id and walk the entire "
        "gossip_menu / gossip_menu_option tree, expanding ActionMenuID "
        "jumps to a max depth of 8."));
    connect(gossipWalkAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::information(this, tr("Gossip menu walker"),
                tr("Connect to the world DB first."));
            return;
        }
        bool okPick = false;
        int const id = QInputDialog::getInt(this,
            tr("Gossip menu walker"),
            tr("Root gossip_menu_id:"),
            /*value=*/1, /*min=*/0, /*max=*/2147483647, /*step=*/1, &okPick);
        if (!okPick || id <= 0)
            return;
        statusBar()->showMessage(tr("Walking gossip menu %1...").arg(id), 3000);
        auto* dlg = new app::GossipMenuDialog(m_worldDb.get(),
                                              static_cast<uint32_t>(id), this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Game events editor - inspect / edit `game_event` and its creature /
    // gameobject membership links so an operator can verify which spawns a
    // seasonal event (Hallow's End, Brewfest, ...) covers on a given map.
    auto* gameEventAction = toolsMenu->addAction(tr("Game &events..."));
    gameEventAction->setToolTip(tr(
        "Open the game_event editor: properties (start/end, occurence, length, "
        "holiday, description, world_event, announce) plus the linked "
        "game_event_creature and game_event_gameobject rows."));
    connect(gameEventAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Game events"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::GameEventEditDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // NPC vendor editor - inspect and edit `npc_vendor` rows that associate
    // creature_template entries with sellable items.  Covers slot, maxcount /
    // incrtime restock pacing, ExtendedCost and the optional type=2 currency
    // vendors.
    auto* npcVendorAction = toolsMenu->addAction(tr("NPC &vendor editor..."));
    npcVendorAction->setToolTip(tr(
        "Open the npc_vendor editor: per-creature_template list of sellable "
        "items with slot, maxcount, incrtime, ExtendedCost, type, "
        "BonusListIDs, PlayerConditionID and IgnoreFiltering."));
    connect(npcVendorAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("NPC vendor editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::NpcVendorDialog(m_worldDb.get(), dbName, this, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Disables editor - inspect/toggle TC's `disables` runtime kill-switch
    // table.  Per (sourceType, entry) PK: spells, quests, maps, BGs,
    // achievement criteria, outdoor PvP, vmap LOS/heights, game events,
    // loot templates, MMAP tiles.
    auto* disablesAction = toolsMenu->addAction(tr("&Disables (kill-switch table)..."));
    disablesAction->setToolTip(tr(
        "Open the disables editor: per-(sourceType, entry) kill-switch rows "
        "for spells, quests, maps, BGs, achievement criteria, outdoor PvP, "
        "vmap LOS/heights, game events, loot templates and MMAP tiles."));
    connect(disablesAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Disables editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::DisablesEditDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Waypoint path editor - inspect/edit creature patrol paths.  Two tables:
    // waypoint_path (header) + waypoint_path_node (ordered nodes), composite
    // PK (PathId, NodeId).
    auto* waypointPathAction = toolsMenu->addAction(tr("&Waypoint paths..."));
    waypointPathAction->setToolTip(tr(
        "Open the waypoint_path editor: edit creature patrol headers "
        "(MoveType / Flags / Velocity / Comment) and ordered nodes "
        "(X, Y, Z, Orientation, Delay).  Supports clone + renumber."));
    connect(waypointPathAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Waypoint paths"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::WaypointPathDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Gossip menu editor - direct row-level editor for gossip_menu /
    // gossip_menu_option / npc_text.  Lives alongside the read-only
    // GossipMenuDialog walker (kept; useful for graph traversal).
    auto* gossipEditAction = toolsMenu->addAction(tr("&Gossip menu editor..."));
    gossipEditAction->setToolTip(tr(
        "Open the gossip_menu editor: edit menu headers, option rows "
        "(OptionID/OptionNpc/OptionText/Action*/SpellID/BoxMoney) and the "
        "linked npc_text BroadcastTextID/Probability fan-out."));
    connect(gossipEditAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Gossip menu editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::GossipMenuEditDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Areatrigger teleports - server-side destination map/position/orientation
    // for instance entrances, BG portals, etc.  Composite PK = ID.
    auto* atrTeleportAction = toolsMenu->addAction(tr("&Areatrigger teleports..."));
    atrTeleportAction->setToolTip(tr(
        "Open the areatrigger_teleport editor: inspect and edit teleport "
        "destinations (target_map, x, y, z, orientation) keyed by AreaTrigger ID.  "
        "Jump-to-destination pans the viewer to the target position."));
    connect(atrTeleportAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Areatrigger teleports"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::AreaTriggerTeleportDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        // Route jumpRequested through the existing pan-to-XY handler so a
        // double-click destination lands the viewer on the map.
        connect(dlg, &app::AreaTriggerTeleportDialog::jumpRequested, this,
                [this](uint32_t mapId, float worldX, float worldY) {
            onJumpRequested(mapId, worldX, worldY, std::nullopt);
        });
        dlg->show();
    });

    // Creature loot editor - direct row-level editor for
    // creature_loot_template.  Sibling to the read-only LootTableDock
    // (which stays as the spawn-centric viewer).
    auto* creatureLootAction = toolsMenu->addAction(tr("Creature &loot editor..."));
    creatureLootAction->setToolTip(tr(
        "Open the creature_loot_template editor: add / edit / remove drop "
        "entries for a creature_template.entry, with full control over "
        "Chance / QuestRequired / LootMode / GroupId / MinCount / MaxCount / "
        "Comment.  Composite PK (Entry, Item)."));
    connect(creatureLootAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Creature loot editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::CreatureLootEditDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Creature text editor - direct row-level editor for creature_text
    // (Say / Yell / Whisper / TextEmote / BossEmote events scripted by
    // SmartAI / creature scripts).  Composite PK (CreatureID, GroupID, ID).
    auto* creatureTextAction = toolsMenu->addAction(tr("Creature &text editor..."));
    creatureTextAction->setToolTip(tr(
        "Open the creature_text editor: add / edit / remove text events "
        "for a creature_template.entry, with control over Type / Language / "
        "Probability / Emote / Duration / Sound / BroadcastTextId / "
        "TextRange.  Composite PK (CreatureID, GroupID, ID); rows sharing "
        "(CreatureID, GroupID) form a random-selection bucket."));
    connect(creatureTextAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Creature text editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::CreatureTextEditDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Creature equipment editor - direct row-level editor for
    // creature_equip_template (weapon/shield/ranged loadouts).  Composite
    // PK (CreatureID, ID); a single creature_template can carry multiple
    // equip variants and TC picks one randomly on spawn.
    auto* creatureEquipAction = toolsMenu->addAction(tr("Creature e&quipment editor..."));
    creatureEquipAction->setToolTip(tr(
        "Open the creature_equip_template editor: add / edit / remove "
        "equip sets for a creature_template.entry, with control over "
        "ItemID / AppearanceModID / ItemVisual across all three slots "
        "(Main Hand / Off Hand / Ranged).  Composite PK (CreatureID, ID)."));
    connect(creatureEquipAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Creature equipment editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::CreatureEquipTemplateDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Broadcast text editor - row-level editor for `broadcast_text`, TC's
    // central NPC speech repository.  Editing one row updates speech every-
    // where it's referenced (creature_text.BroadcastTextId, gossip_menu_option,
    // quest_offer_reward, etc.).  Includes a "Show references" probe + a
    // delete-warning that surfaces dangling-pointer impact.
    auto* broadcastTextAction = toolsMenu->addAction(tr("&Broadcast text editor..."));
    broadcastTextAction->setToolTip(tr(
        "Open the broadcast_text editor: add / edit / remove rows in TC's "
        "central NPC speech table.  MaleText / FemaleText / Language / "
        "EmoteID1-3 / EmoteDelay1-3 / SoundEntriesID1-2 / EmotesID / Flags / "
        "ConditionID.  Schema-tolerant: probes INFORMATION_SCHEMA at open."));
    connect(broadcastTextAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Broadcast text editor"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::BroadcastTextDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Creature template addon editor - row-level editor for
    // `creature_template_addon`, which carries per-creature_template defaults
    // (mount, sheath, auras, anim kits, ...) that apply to every spawn of
    // that entry unless overridden by a per-spawn `creature_addon` row.
    auto* creatureTemplateAddonAction = toolsMenu->addAction(tr("Creature &template addon..."));
    creatureTemplateAddonAction->setToolTip(tr(
        "Open the creature_template_addon editor: load by creature entry, "
        "edit defaults (PathId / mount / StandState / AnimTier / VisFlags / "
        "SheathState / PvpFlags / emote / AnimKits / VisibilityDistanceType / "
        "auras), Save UPSERT, Delete to revert to TC fallbacks.  "
        "Schema-tolerant: probes INFORMATION_SCHEMA for column-name spelling."));
    connect(creatureTemplateAddonAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Creature template addon"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::CreatureTemplateAddonDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Linked respawn editor - direct row-level editor for `linked_respawn`,
    // the table that defines spawn dependency (boss killed -> mob respawn,
    // and similar chain rules).  Composite PK (guid, linkType); same
    // dependent guid can carry one link per linkType.
    auto* linkedRespawnAction = toolsMenu->addAction(tr("&Linked respawn..."));
    linkedRespawnAction->setToolTip(tr(
        "Open the linked_respawn editor: add / edit / remove dependency "
        "links between creature and gameobject spawns.  Each row says "
        "'when linkedGuid is killed, guid respawns' (or the inverse, "
        "depending on the boss script wiring it up).  Composite PK "
        "(guid, linkType)."));
    connect(linkedRespawnAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Linked respawn"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::LinkedRespawnDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &app::LinkedRespawnDialog::jumpRequested, this,
                [this](uint32_t mapId, float worldX, float worldY) {
            onJumpRequested(mapId, worldX, worldY, std::nullopt);
        });
        dlg->show();
    });

    auto* worldSafeLocsAction = toolsMenu->addAction(tr("&World safe locs (graveyards/portals)..."));
    worldSafeLocsAction->setToolTip(tr(
        "Open the world_safe_locs editor: add / edit / remove named "
        "teleport destinations and graveyard points.  Referenced by "
        "areatrigger_teleport.PortLocID, graveyard_zone.ID, and "
        "instance_template - editing one row here changes every teleport "
        "destination and ghost-resurrection anchor that points at it.  "
        "Use 'Show references' before deleting to see what would break."));
    connect(worldSafeLocsAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("World safe locs"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::WorldSafeLocsDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &app::WorldSafeLocsDialog::jumpRequested, this,
                [this](uint32_t mapId, float worldX, float worldY) {
            onJumpRequested(mapId, worldX, worldY, std::nullopt);
        });
        dlg->show();
    });

    // Unified editor for the ten secondary loot-template tables that share
    // `creature_loot_template`'s schema (gameobject / item / skinning / fishing
    // / pickpocketing / disenchant / milling / prospecting / reference / mail).
    // Table-name substitution is locked to a fixed allow-list inside the dialog.
    auto* secondaryLootAction = toolsMenu->addAction(tr("&Secondary loot tables..."));
    secondaryLootAction->setToolTip(tr(
        "Open the unified editor for the ten secondary loot-template tables "
        "that share creature_loot_template's schema: gameobject_loot_template, "
        "item_loot_template, skinning_loot_template, fishing_loot_template, "
        "pickpocketing_loot_template, disenchant_loot_template, "
        "milling_loot_template, prospecting_loot_template, "
        "reference_loot_template, mail_loot_template.  Composite PK "
        "(Entry, ItemType, Item)."));
    connect(secondaryLootAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Secondary loot tables"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::SecondaryLootTablesDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Editor for `creature_summon_groups` - named summon group rows consumed
    // by SAI SUMMON_CREATURE_GROUP actions and by creature_template_summon.
    auto* creatureSummonGroupsAction = toolsMenu->addAction(tr("Creature s&ummon groups..."));
    creatureSummonGroupsAction->setToolTip(tr(
        "Open the creature_summon_groups editor: named groups of summons keyed by "
        "(summonerId, summonerType, groupId).  summonerType 0/1 cover creature "
        "summoners (entry / guid); 2 is gameobject_template.entry.  Each row "
        "carries a position (relative to the summoner), summonType (TempSummonType "
        "enum) and summonTime in ms.  Composite PK is "
        "(summonerId, summonerType, groupId, entry, position_x)."));
    connect(creatureSummonGroupsAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Creature summon groups"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::CreatureSummonGroupsDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &app::CreatureSummonGroupsDialog::jumpRequested, this,
                [this](uint32_t mapId, float worldX, float worldY) {
            onJumpRequested(mapId, worldX, worldY, std::nullopt);
        });
        dlg->show();
    });

    // Quest dialog text editor - single modal that round-trips all three quest-
    // text aux tables (quest_offer_reward / quest_request_items / quest_details)
    // for one quest ID at a time.  Each tab carries its own UPSERT Save button;
    // schema-tolerant via INFORMATION_SCHEMA.COLUMNS probes so legacy / forked
    // schemas with missing optional emote slots still load + save.
    auto* questDialogTextAction = toolsMenu->addAction(tr("&Quest dialog text..."));
    questDialogTextAction->setToolTip(tr(
        "Open the quest dialog-text editor: load by quest_template.ID, edit "
        "RewardText + 4x emote channels (quest_offer_reward), CompletionText "
        "+ OnComplete/OnIncomplete emote channels (quest_request_items), and "
        "4x pickup-time emote channels (quest_details).  Each tab Saves "
        "independently with INSERT-if-missing / UPDATE-if-present UPSERT "
        "wrapped in START TRANSACTION / COMMIT."));
    connect(questDialogTextAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Quest dialog text"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::QuestDialogTextDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Instance access requirements editor - composite (mapId, difficulty) PK
    // gating instance entry by minimum level, required keys, prerequisite
    // quests, and required achievement.  All DML in START TRANSACTION /
    // COMMIT.  Mirrors the dialog shape used by WorldSafeLocsDialog.
    auto* accessReqAction = toolsMenu->addAction(tr("&Instance access requirements..."));
    accessReqAction->setToolTip(tr(
        "Open the access_requirement editor: filter by mapId/difficulty, "
        "browse the gate list, Add/Edit/Remove rows for level_min/max + key "
        "items + prerequisite quests + required achievement + custom "
        "failure text.  Lookup item template surfaces the selected `item` "
        "field for the ItemInfoDock."));
    connect(accessReqAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Instance access requirements"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::AccessRequirementDialog(m_worldDb.get(), dbName, this, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Quest-giver linkage editor - unified UI for the 4 (id, quest) tables:
    // creature_queststarter / creature_questender / gameobject_queststarter /
    // gameobject_questender.  Combo box picks the table; all DML wraps
    // START TRANSACTION / COMMIT / ROLLBACK; table-name token is locked
    // to a 4-entry allowlist so the operator can never inject a free-form
    // identifier into the SQL.
    auto* questLinkageAction = toolsMenu->addAction(tr("Quest &giver linkage..."));
    questLinkageAction->setToolTip(tr(
        "Edit creature_queststarter / creature_questender / "
        "gameobject_queststarter / gameobject_questender rows from one "
        "dialog.  Filter by quest ID and/or template entry, add/remove "
        "links inside a transaction, and look up the joined "
        "creature_template / gameobject_template + quest_template row."));
    connect(questLinkageAction, &QAction::triggered, this, [this]() {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            QMessageBox::warning(this, tr("Quest giver linkage"),
                tr("Connect to the world DB first."));
            return;
        }
        QString const worldDbName = QString::fromStdString(
            db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
        QString const dbName = worldDbName.isEmpty()
            ? QStringLiteral("playerbot_world") : worldDbName;
        auto* dlg = new app::QuestGiverLinkageDialog(m_worldDb.get(), dbName, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    // Path menu holds the IN-MODE verbs; starting a path lives under
    // Create -> New path (Ctrl+Shift+N) with the other placement verbs.
    QMenu* pathMenu = menuBar()->addMenu(tr("&Path"));
    auto* finishPathAction = pathMenu->addAction(tr("&Finish path"));
    finishPathAction->setShortcut(Qt::Key_Return);
    connect(finishPathAction, &QAction::triggered, this, &MainWindow::onFinishPath);
    auto* cancelPathAction = pathMenu->addAction(tr("&Cancel path"));
    cancelPathAction->setShortcut(Qt::Key_Escape);
    connect(cancelPathAction, &QAction::triggered, this, &MainWindow::onCancelPath);
    auto* autoRouteAction = pathMenu->addAction(tr("Auto-&route to next click"));
    autoRouteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    autoRouteAction->setToolTip(tr(
        "While drawing a path OR road, arm this action then click "
        "anywhere on the navmesh.  Detour computes the route from the "
        "last node/road-point to the clicked point and inserts every "
        "intermediate waypoint."));
    connect(autoRouteAction, &QAction::triggered, this, &MainWindow::onArmAutoRoute);

    auto* playPathAction = pathMenu->addAction(tr("&Play selected path in 3D"));
    playPathAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    playPathAction->setToolTip(tr(
        "Animate the 3D camera along the currently-selected waypoint_path "
        "at its recorded velocity (or 5 yards/sec when velocity is 0).  "
        "Press Esc in the 3D view to stop."));
    connect(playPathAction, &QAction::triggered, this, &MainWindow::onPlayPathIn3D);

    pathMenu->addSeparator();
    auto* startRoadAction = pathMenu->addAction(tr("Road network &draw mode"));
    startRoadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
    startRoadAction->setToolTip(tr(
        "Each click drops an AnnotationKind::Road waypoint (snap-to-"
        "ground).  Arm auto-route (Ctrl+R) to have Detour fill in every "
        "intermediate Road waypoint between two clicks -- the bot's "
        "mmap regen consumes these via the world_metadata table."));
    connect(startRoadAction, &QAction::triggered, this, &MainWindow::onStartRoadDraw);
    auto* finishRoadAction = pathMenu->addAction(tr("&Exit road draw mode"));
    connect(finishRoadAction, &QAction::triggered, this, &MainWindow::onFinishRoadDraw);

    // Bot dungeon-route chain (playerbot_dungeon_routes, shared schema):
    // gold overlay in the 3D view, node-editable there, committed as a
    // full-map rewrite.  Grouped here because routes are path-shaped data.
    pathMenu->addSeparator();
    auto* routeLayerAction = pathMenu->addAction(tr("Show bot &dungeon route (3D)"));
    routeLayerAction->setCheckable(true);
    routeLayerAction->setChecked(true);
    connect(routeLayerAction, &QAction::toggled, this, [this](bool on)
    {
        if (m_viewer3d) m_viewer3d->setDungeonRoutesVisible(on);
    });
    auto* reloadRoutesAction = pathMenu->addAction(tr("Re&load dungeon route from DB"));
    connect(reloadRoutesAction, &QAction::triggered,
            this, &MainWindow::onReloadDungeonRoutes);
    auto* newRouteAction = pathMenu->addAction(tr("Start new dungeon route at camera (3D)"));
    connect(newRouteAction, &QAction::triggered,
            this, &MainWindow::onNewDungeonRouteAtCamera);
    auto* commitRoutesAction = pathMenu->addAction(tr("Commit dungeon route..."));
    connect(commitRoutesAction, &QAction::triggered,
            this, &MainWindow::onCommitDungeonRoutes);

    // Pathfinding probe: bot-budget A->B sandbox (74 polys / 1024 nodes,
    // matching the worldserver PathGenerator) -- diagnoses INCOMPLETE /
    // NOPATH spots without a live server run.
    pathMenu->addSeparator();
    auto* probeAction = pathMenu->addAction(tr("Path&finding probe (bot budget)"));
    probeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
    probeAction->setToolTip(tr(
        "Click a START point then a destination; Detour runs with the BOT'S "
        "budget (74-poly corridor, 1024 query nodes) and the 3D view shows "
        "the result: green = complete, orange = partial corridor, red dashed "
        "= unreached remainder.  The HUD prints polys/points/length."));
    connect(probeAction, &QAction::triggered, this, &MainWindow::onStartPathProbe);
    auto* clearProbeAction = pathMenu->addAction(tr("Clear probe result"));
    connect(clearProbeAction, &QAction::triggered, this, &MainWindow::onClearPathProbe);

    // Off-mesh connection authoring: the bridges that fix cave/den/topology
    // gaps.  Existing links render as violet arcs (straight from the loaded
    // mmap); authored ones append to offmesh.txt and show magenta until the
    // next regen bakes them.
    pathMenu->addSeparator();
    auto* offmeshLayerAction = pathMenu->addAction(tr("Show off-&mesh connections (3D)"));
    offmeshLayerAction->setCheckable(true);
    offmeshLayerAction->setChecked(true);
    connect(offmeshLayerAction, &QAction::toggled, this, [this](bool on)
    {
        if (m_viewer3d) m_viewer3d->setOffmeshVisible(on);
    });
    auto* offmeshDrawAction = pathMenu->addAction(tr("Draw off-mesh connection (FROM -> TO)"));
    offmeshDrawAction->setToolTip(tr(
        "Click the FROM point then the TO point; a bidirectional off-mesh "
        "connection line is appended to offmesh.txt in the exact format "
        "mmaps_generator parses.  Run the regen to bake it, then reload the "
        "map to see it as a violet arc."));
    connect(offmeshDrawAction, &QAction::triggered, this, &MainWindow::onStartOffmeshDraw);
    auto* offmeshFileAction = pathMenu->addAction(tr("Set offmesh.txt path..."));
    connect(offmeshFileAction, &QAction::triggered, this, &MainWindow::onSetOffmeshFile);

    // (The former single-action Areatrigger and Graveyard top-level menus
    // moved into the Create menu above.)

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    auto* navLayer = viewMenu->addAction(tr("&Navmesh layer"));
    navLayer->setCheckable(true);
    navLayer->setChecked(true);
    navLayer->setShortcut(Qt::Key_1);
    connect(navLayer, &QAction::toggled, this, &MainWindow::onToggleNavLayer);
    m_layerToggles.push_back({navLayer, render::Layer::NavMesh, true, nullptr});

    auto* spawnLayer = viewMenu->addAction(tr("&Spawn layer"));
    spawnLayer->setCheckable(true);
    spawnLayer->setChecked(true);
    spawnLayer->setShortcut(Qt::Key_2);
    connect(spawnLayer, &QAction::toggled, this, &MainWindow::onToggleSpawnLayer);
    m_layerToggles.push_back({spawnLayer, render::Layer::Spawns, true, nullptr});

    auto* minimapLayer = viewMenu->addAction(tr("Mini&map texture layer"));
    minimapLayer->setCheckable(true);
    minimapLayer->setChecked(true);
    minimapLayer->setShortcut(Qt::Key_3);
    minimapLayer->setToolTip(tr(
        "Overlay real-map PNG tiles from the configured minimap "
        "directory.  Tiles without a PNG fall back to the heightmap "
        "shading underneath.  Set the source folder via File -> Set "
        "minimap directory; expected layout: "
        "<dir>/<mapId>/map<gx>_<gy>.png"));
    connect(minimapLayer, &QAction::toggled, this, &MainWindow::onToggleMinimapLayer);
    m_layerToggles.push_back({minimapLayer, render::Layer::Minimap, true, nullptr});

    auto* heightmapLayer = viewMenu->addAction(tr("&Heightmap layer"));
    heightmapLayer->setCheckable(true);
    {
        QSettings settings;
        heightmapLayer->setChecked(settings.value(QStringLiteral("editor/show_heightmap"), true).toBool());
    }
    heightmapLayer->setShortcut(Qt::Key_4);
    connect(heightmapLayer, &QAction::toggled, this, &MainWindow::onToggleHeightmapLayer);
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::Heightmap, heightmapLayer->isChecked());
    m_layerToggles.push_back({heightmapLayer, render::Layer::Heightmap, true, "editor/show_heightmap"});

    auto* pathLayer = viewMenu->addAction(tr("&Path layer (waypoints)"));
    pathLayer->setCheckable(true);
    pathLayer->setChecked(true);
    pathLayer->setShortcut(Qt::Key_5);
    connect(pathLayer, &QAction::toggled, this, &MainWindow::onTogglePathLayer);
    m_layerToggles.push_back({pathLayer, render::Layer::Paths, true, nullptr});

    auto* atrLayer = viewMenu->addAction(tr("Area&trigger layer"));
    atrLayer->setCheckable(true);
    atrLayer->setChecked(true);
    atrLayer->setShortcut(Qt::Key_6);
    connect(atrLayer, &QAction::toggled, this, &MainWindow::onToggleAreatriggerLayer);
    m_layerToggles.push_back({atrLayer, render::Layer::Areatriggers, true, nullptr});

    auto* gyLayer = viewMenu->addAction(tr("&Graveyard layer"));
    gyLayer->setCheckable(true);
    gyLayer->setChecked(true);
    gyLayer->setShortcut(Qt::Key_7);
    connect(gyLayer, &QAction::toggled, this, &MainWindow::onToggleGraveyardLayer);
    m_layerToggles.push_back({gyLayer, render::Layer::Graveyards, true, nullptr});

    auto* wmoLayer = viewMenu->addAction(tr("&WMO mesh layer (3D)"));
    wmoLayer->setCheckable(true);
    wmoLayer->setChecked(true);
    wmoLayer->setShortcut(Qt::Key_8);
    connect(wmoLayer, &QAction::toggled, this, &MainWindow::onToggleWmoLayer);
    m_layerToggles.push_back({wmoLayer, render::Layer::WmoOutline, true, nullptr});

    // 2D WMO building-footprint overlay.  Painted as dashed dark-purple
    // rectangles using the per-instance world-XY AABBs captured during
    // vmap load.  Default OFF so a fresh editor doesn't surprise the
    // operator with a layer they didn't ask for; persisted across runs.
    auto* wmoFootprintLayer = viewMenu->addAction(tr("WMO 2D &footprints"));
    wmoFootprintLayer->setCheckable(true);
    {
        QSettings s;
        wmoFootprintLayer->setChecked(s.value(SETTINGS_WMO_FOOTPRINTS, false).toBool());
    }
    wmoFootprintLayer->setToolTip(tr(
        "Draw a dashed purple rectangle over every WMO instance on the "
        "current map using the per-building AABB captured at vmap load.  "
        "Helps spot indoor placement targets without switching to 3D."));
    connect(wmoFootprintLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_WMO_FOOTPRINTS, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::WmoOutline, on);
        pushWmoFootprintsToViewer(on);
    });
    m_layerToggles.push_back({wmoFootprintLayer, render::Layer::WmoOutline, false, SETTINGS_WMO_FOOTPRINTS});

    // Retail-client-like textured + lit 3D pass.  Default OFF so the
    // operator who just wants navmesh polys over a heightmap is not
    // forced to wait on a CASC fetch.  Persists across sessions.
    {
        QSettings settings;
        bool const realistic = settings.value(SETTINGS_REALISTIC3D, false).toBool();
        auto* realisticLayer = viewMenu->addAction(tr("&Realistic 3D textures"));
        realisticLayer->setCheckable(true);
        realisticLayer->setChecked(realistic);
        realisticLayer->setToolTip(tr(
            "Textured + lit 3D terrain using minimap PNGs (CASC fallback) "
            "and lit WMO geometry.  When off, the 3D viewer uses the "
            "elevation-shaded terrain + flat translucent WMO overlay."));
        connect(realisticLayer, &QAction::toggled, this, &MainWindow::onToggleRealistic3D);
        if (m_viewer3d)
            m_viewer3d->setRealistic(realistic);

        bool const doodads = settings.value(SETTINGS_DOODADS3D, true).toBool();
        auto* doodadLayer = viewMenu->addAction(tr("Show M2 &doodads"));
        doodadLayer->setCheckable(true);
        doodadLayer->setChecked(doodads);
        doodadLayer->setToolTip(tr(
            "Render ADT MDDF doodad instances (trees, rocks, mailboxes, "
            "fences, lampposts) as M2 static meshes in the realistic 3D "
            "pass.  Only takes effect when 'Realistic 3D textures' is on."));
        connect(doodadLayer, &QAction::toggled, this, [this](bool on) {
            if (m_viewer3d)
                m_viewer3d->setDoodadsVisible(on);
            QSettings s;
            s.setValue(SETTINGS_DOODADS3D, on);
            statusBar()->showMessage(
                on ? tr("3D viewer: doodads ON") : tr("3D viewer: doodads OFF"),
                3000);
        });
        if (m_viewer3d)
            m_viewer3d->setDoodadsVisible(doodads);

        bool const texWmos = settings.value(SETTINGS_TEXWMOS3D, true).toBool();
        auto* texWmoLayer = viewMenu->addAction(tr("Show textured &WMOs"));
        texWmoLayer->setCheckable(true);
        texWmoLayer->setChecked(texWmos);
        texWmoLayer->setToolTip(tr(
            "Render real textured WMO wall/floor/ceiling/bridge geometry from "
            "the client WMO root + group files in the realistic 3D pass.  When "
            "off (or when no client WMO data loads) the translucent collision "
            "overlay / flat-lit fallback draws instead.  Only takes effect when "
            "'Realistic 3D textures' is on."));
        connect(texWmoLayer, &QAction::toggled, this, [this](bool on) {
            if (m_viewer3d)
                m_viewer3d->setTexturedWmosVisible(on);
            QSettings s;
            s.setValue(SETTINGS_TEXWMOS3D, on);
            statusBar()->showMessage(
                on ? tr("3D viewer: textured WMOs ON") : tr("3D viewer: textured WMOs OFF"),
                3000);
        });
        if (m_viewer3d)
            m_viewer3d->setTexturedWmosVisible(texWmos);

        bool const verbose3d = settings.value(SETTINGS_VERBOSE3DLOG, false).toBool();
        auto* verboseLayer = viewMenu->addAction(tr("&Verbose 3D debug log"));
        verboseLayer->setCheckable(true);
        verboseLayer->setChecked(verbose3d);
        verboseLayer->setToolTip(tr(
            "Write per-tile terrain-decode diagnostics + per-pass geometry "
            "inventory to debug.log (the [scene3d-adt-v] / [scene3d-pass] "
            "lines).  Useful while diagnosing 3D rendering; leave off for "
            "normal use to keep the log quiet."));
        connect(verboseLayer, &QAction::toggled, this, [this](bool on) {
            if (m_viewer3d) m_viewer3d->setVerboseLogging(on);
            QSettings s;
            s.setValue(SETTINGS_VERBOSE3DLOG, on);
            statusBar()->showMessage(
                on ? tr("3D viewer: verbose log ON") : tr("3D viewer: verbose log OFF"),
                3000);
        });
        if (m_viewer3d)
            m_viewer3d->setVerboseLogging(verbose3d);
    }

    // Sky + fog + time-of-day.  Atmospheric layer that completes the
    // retail-client-look set; gated on top of realistic-mode by the
    // viewer itself so the legacy non-realistic view stays flat-shaded.
    {
        QSettings settings;
        QMenu* skyMenu = viewMenu->addMenu(tr("&Sky"));
        bool const sky = settings.value(SETTINGS_SKY3D, true).toBool();
        auto* skyAction = skyMenu->addAction(tr("Show s&ky dome"));
        skyAction->setCheckable(true);
        skyAction->setChecked(sky);
        skyAction->setToolTip(tr(
            "Render a gradient sky behind the world geometry.  Tinted by "
            "the time-of-day slider; clears to dark grey when off."));
        connect(skyAction, &QAction::toggled, this, [this](bool on) {
            if (m_viewer3d) m_viewer3d->setSkyVisible(on);
            QSettings s;
            s.setValue(SETTINGS_SKY3D, on);
            statusBar()->showMessage(
                on ? tr("3D viewer: sky dome ON") : tr("3D viewer: sky dome OFF"),
                3000);
        });
        if (m_viewer3d)
            m_viewer3d->setSkyVisible(sky);

        bool const fog = settings.value(SETTINGS_FOG3D, true).toBool();
        auto* fogAction = skyMenu->addAction(tr("Show distance &fog"));
        fogAction->setCheckable(true);
        fogAction->setChecked(fog);
        fogAction->setToolTip(tr(
            "Mix the horizon colour into terrain, WMO and doodad albedo "
            "by world-space distance.  Hides the far-clip pop, matches "
            "the retail-client haze.  Off => terrain stays fully saturated."));
        connect(fogAction, &QAction::toggled, this, [this](bool on) {
            if (m_viewer3d) m_viewer3d->setFogEnabled(on);
            QSettings s;
            s.setValue(SETTINGS_FOG3D, on);
        });
        if (m_viewer3d)
            m_viewer3d->setFogEnabled(fog);

        bool const water = settings.value(SETTINGS_WATER3D, true).toBool();
        auto* waterAction = skyMenu->addAction(tr("Show &water"));
        waterAction->setCheckable(true);
        waterAction->setChecked(water);
        waterAction->setToolTip(tr(
            "Render parsed MH2O / MCLQ liquid bodies as translucent "
            "shaded surfaces with animated waves and Lambert lighting.  "
            "Off => flat terrain elevation only."));
        connect(waterAction, &QAction::toggled, this, [this](bool on) {
            if (m_viewer3d) m_viewer3d->setWaterVisible(on);
            QSettings s;
            s.setValue(SETTINGS_WATER3D, on);
            statusBar()->showMessage(
                on ? tr("3D viewer: water ON") : tr("3D viewer: water OFF"),
                3000);
        });
        if (m_viewer3d)
            m_viewer3d->setWaterVisible(water);

        float const tod = float(settings.value(SETTINGS_TIMEOFDAY3D, 12.0).toDouble());
        if (m_viewer3d)
            m_viewer3d->setTimeOfDay(tod);
        auto* todAction = skyMenu->addAction(tr("&Time of day..."));
        todAction->setToolTip(tr(
            "Hour in [0, 24).  Drives sun direction, sky gradient, fog "
            "tint and ambient via an internal per-hour LUT."));
        connect(todAction, &QAction::triggered, this, [this]() {
            QSettings s;
            double const cur = s.value(SETTINGS_TIMEOFDAY3D, 12.0).toDouble();
            bool ok = false;
            double const v = QInputDialog::getDouble(this,
                tr("Time of day"),
                tr("Hour (0.0 - 24.0):"),
                cur, 0.0, 24.0, 2, &ok);
            if (!ok) return;
            if (m_viewer3d) m_viewer3d->setTimeOfDay(float(v));
            s.setValue(SETTINGS_TIMEOFDAY3D, v);
            statusBar()->showMessage(
                tr("3D viewer: time of day = %1h").arg(v, 0, 'f', 2),
                3000);
        });
    }

    auto* questLayer = viewMenu->addAction(tr("&Quest marker layer"));
    questLayer->setCheckable(true);
    questLayer->setChecked(false);
    questLayer->setShortcut(Qt::Key_0);
    questLayer->setToolTip(tr(
        "Overlay ? on quest givers and ! on quest enders for every "
        "creature/gameobject on the current map.  Thin teal lines "
        "connect starter+ender of the same quest."));
    connect(questLayer, &QAction::toggled, this, [this](bool on) {
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::Quests, on);
        if (on)
            loadQuestMarkers();
        else if (m_viewer)
        {
            m_viewer->setQuestMarkers({});
            m_viewer->setQuestObjectiveMarkers({});
        }
    });
    m_layerToggles.push_back({questLayer, render::Layer::Quests, false, nullptr});

    // Quest objective overlay: kind-coded icons (kill/gather/interact/
    // talk/explore) painted on every spawn that's a target for one of
    // the current map's in-scope quests.  Gated by the Quests layer:
    // when Quests is off the overlay is hidden too (data is still in
    // memory; no extra query cost on toggle).  Default OFF.
    auto* questObjLayer = viewMenu->addAction(tr("Quest objective &overlay"));
    questObjLayer->setCheckable(true);
    {
        QSettings s;
        bool const def = s.value(SETTINGS_QUEST_OBJECTIVES, false).toBool();
        questObjLayer->setChecked(def);
        if (m_viewer)
            m_viewer->setQuestObjectivesVisible(def);
    }
    questObjLayer->setToolTip(tr(
        "Overlay kind-coded icons (kill / gather / interact / talk / "
        "explore) on every spawn that's an objective target for a quest "
        "involving this map.  Visible only when the Quest marker layer "
        "is also on.  Hover an icon for the matching quest ids."));
    connect(questObjLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_QUEST_OBJECTIVES, on);
        if (m_viewer)
            m_viewer->setQuestObjectivesVisible(on);
    });
    // QuestObjectives has no dedicated render::Layer; reuse Quests for
    // the unused layer field -- presets drive via setChecked() which
    // fires the slot above to push state into the viewer.
    m_layerToggles.push_back({questObjLayer, render::Layer::Quests, false, SETTINGS_QUEST_OBJECTIVES});

    auto* groupLayer = viewMenu->addAction(tr("Spawn-&group tint"));
    groupLayer->setCheckable(true);
    groupLayer->setChecked(false);
    groupLayer->setShortcut(Qt::Key_9);
    groupLayer->setToolTip(tr(
        "Color-tint spawn icons by spawn_group / pool membership.  "
        "Loads spawn_group rows on first toggle; each group gets a "
        "deterministic color so groups read at a glance."));
    connect(groupLayer, &QAction::toggled, this, [this](bool on) {
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::SpawnGroups, on);
        if (on)
            loadSpawnGroupColors();
        else if (m_viewer)
            m_viewer->setSpawnGroupColors({});
    });
    m_layerToggles.push_back({groupLayer, render::Layer::SpawnGroups, false, nullptr});

    // Faction-territory tint: Alliance=blue, Horde=red, Sanctuary=yellow,
    // Contested=purple, Other=gray.  Loses to SpawnGroups when both on.
    auto* factionLayer = viewMenu->addAction(tr("&Faction tint overlay"));
    factionLayer->setCheckable(true);
    {
        QSettings s;
        factionLayer->setChecked(s.value(SETTINGS_FACTION_TINT, false).toBool());
    }
    factionLayer->setToolTip(tr(
        "Color-tint creature spawn icons by creature_template.faction so "
        "Alliance / Horde / Sanctuary / Contested territories read at a "
        "glance.  SpawnGroup tint overrides this when both are on."));
    connect(factionLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_FACTION_TINT, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::FactionTint, on);
        if (on)
            loadFactionTintMap();
        else if (m_viewer)
            m_viewer->setFactionTintMap({});
    });
    m_layerToggles.push_back({factionLayer, render::Layer::FactionTint, false, SETTINGS_FACTION_TINT});

    // Level-bracket heatmap: yellow=1-10 ... red=71-80, magenta=boss.
    // Reads creature_template.minlevel/maxlevel for entries on this map.
    auto* levelHeatmapLayer = viewMenu->addAction(tr("&Level heatmap"));
    levelHeatmapLayer->setCheckable(true);
    {
        QSettings s;
        levelHeatmapLayer->setChecked(s.value(SETTINGS_LEVEL_HEATMAP, false).toBool());
    }
    levelHeatmapLayer->setToolTip(tr(
        "Color-tint creature spawn icons by creature_template.minlevel..maxlevel "
        "midpoint over a 12-stop palette (yellow L1-10, green 11-20, blue 21-40, "
        "purple 41-50, pink 51-60, orange 61-70, red 71-80, magenta=boss).  "
        "SpawnGroup tint overrides this when both are on."));
    connect(levelHeatmapLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_LEVEL_HEATMAP, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::LevelHeatmap, on);
        if (on)
        {
            loadLevelMap();
            statusBar()->showMessage(tr("Level heatmap: yellow=1-10, green=11-20, "
                "blue=21-40, purple=41-50, pink=51-60, orange=61-70, red=71-80, "
                "magenta=boss"), 0);
        }
        else
        {
            if (m_viewer)
                m_viewer->setLevelMap({});
            statusBar()->clearMessage();
        }
    });
    m_layerToggles.push_back({levelHeatmapLayer, render::Layer::LevelHeatmap, false, SETTINGS_LEVEL_HEATMAP});

    // Spawn-density heatmap: 50-yard cells colored by spawn count so
    // overpopulated zones jump out at a glance.  No DB query needed --
    // the data is m_spawns; the viewer caches it under a dirty flag.
    auto* spawnDensityLayer = viewMenu->addAction(tr("Spawn &density heatmap"));
    spawnDensityLayer->setCheckable(true);
    {
        QSettings s;
        spawnDensityLayer->setChecked(s.value(SETTINGS_SPAWN_DENSITY, false).toBool());
    }
    spawnDensityLayer->setToolTip(tr(
        "Overlay a translucent 50-yard grid heatmap colored by spawn "
        "count per cell (green=1-5, yellow=6-15, orange=16-30, red=31-50, "
        "bright red=51+).  Honors the phase filter."));
    connect(spawnDensityLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_SPAWN_DENSITY, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::SpawnDensity, on);
    });
    m_layerToggles.push_back({spawnDensityLayer, render::Layer::SpawnDensity, false, SETTINGS_SPAWN_DENSITY});

    // Flight path graph overlay: taxi_nodes (light blue rings) +
    // taxi_path (light blue line segments).  Default OFF -- the DB
    // query runs only on toggle so a fresh editor doesn't pay the cost.
    auto* flightLayer = viewMenu->addAction(tr("&Flight path graph"));
    flightLayer->setCheckable(true);
    {
        QSettings s;
        flightLayer->setChecked(s.value(SETTINGS_FLIGHT_PATHS, false).toBool());
    }
    flightLayer->setToolTip(tr(
        "Overlay taxi_nodes positions and taxi_path edges so the operator "
        "can see the in-game flight network on this map.  Falls back to an "
        "empty layer when taxi_nodes is absent (DB2-only on modern shards)."));
    connect(flightLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_FLIGHT_PATHS, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::FlightPaths, on);
        if (on)
            loadFlightGraph();
        else if (m_viewer)
            m_viewer->setFlightGraph({}, {});
    });
    m_layerToggles.push_back({flightLayer, render::Layer::FlightPaths, false, SETTINGS_FLIGHT_PATHS});

    // Transport route overlay: orange polylines from transports +
    // transport_animation keyframes.  Default OFF -- DB schema varies
    // across forks (transports vs transport_template, transport_animation
    // vs transport_keyframes) so we probe lazily on toggle.
    auto* transportLayer = viewMenu->addAction(tr("&Transport routes"));
    transportLayer->setCheckable(true);
    {
        QSettings s;
        transportLayer->setChecked(s.value(SETTINGS_TRANSPORT_ROUTES, false).toBool());
    }
    transportLayer->setToolTip(tr(
        "Overlay GameObject transport waypoint paths (zeppelins/boats) "
        "drawn from transports + transport_animation as thick orange "
        "polylines.  Schema-tolerant; silently empty when the relevant "
        "tables are absent on the connected DB."));
    connect(transportLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_TRANSPORT_ROUTES, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::TransportRoutes, on);
        if (on)
            loadTransportRoutes();
        else if (m_viewer)
            m_viewer->setTransportRoutes({});
    });
    m_layerToggles.push_back({transportLayer, render::Layer::TransportRoutes, false, SETTINGS_TRANSPORT_ROUTES});

    // Gathering-node heatmap overlay: classifies map GOs as mining vein
    // / herb node / fishing pool / treasure and overlays kind-coded
    // icons above each spawn.  Default OFF -- the classification DB
    // query runs only on toggle so a fresh editor doesn't pay the cost.
    auto* gatheringLayer = viewMenu->addAction(tr("&Gathering nodes"));
    gatheringLayer->setCheckable(true);
    {
        QSettings s;
        gatheringLayer->setChecked(s.value(SETTINGS_GATHERING_NODES, false).toBool());
    }
    gatheringLayer->setToolTip(tr(
        "Highlight mining veins (brown), herb nodes (green), fishing pools (blue) "
        "and treasure goobers (gold) so the operator can spot gathering hotspots. "
        "Classified by gameobject_template type + name patterns."));
    connect(gatheringLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_GATHERING_NODES, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::GatheringNodes, on);
        if (on)
            loadGatheringNodes();
        else if (m_viewer)
            m_viewer->setGatheringNodes({});
    });
    m_layerToggles.push_back({gatheringLayer, render::Layer::GatheringNodes, false, SETTINGS_GATHERING_NODES});

    // Instance-entrance overlay: large purple rings around every
    // areatrigger_teleport portal on the current map.  Default ON.
    auto* instanceEntrancesLayer = viewMenu->addAction(tr("&Instance entrances"));
    instanceEntrancesLayer->setCheckable(true);
    {
        QSettings s;
        instanceEntrancesLayer->setChecked(s.value(SETTINGS_INSTANCE_ENTRANCES, true).toBool());
    }
    instanceEntrancesLayer->setToolTip(tr(
        "Highlight every areatrigger on this map whose areatrigger_teleport row "
        "points at a dungeon/raid target map with a large purple ring and the "
        "target map name."));
    connect(instanceEntrancesLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_INSTANCE_ENTRANCES, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::InstanceEntrance, on);
        if (on)
            loadInstanceEntrances();
        else if (m_viewer)
            m_viewer->setInstanceEntrances({});
    });
    m_layerToggles.push_back({instanceEntrancesLayer, render::Layer::InstanceEntrance, true, SETTINGS_INSTANCE_ENTRANCES});

    // linked_respawn dependency overlay.  Default OFF; the DB query runs
    // on toggle (and at map load when persisted ON) so the cost is paid
    // only when needed.  Silently empty when linked_respawn is absent or
    // no row touches a spawn on the current map.
    auto* spawnLinksLayer = viewMenu->addAction(tr("Spawn &links"));
    spawnLinksLayer->setCheckable(true);
    {
        QSettings s;
        spawnLinksLayer->setChecked(s.value(SETTINGS_SPAWN_LINKS, false).toBool());
    }
    spawnLinksLayer->setToolTip(tr(
        "Overlay world.linked_respawn dependency arrows: a thin dotted "
        "dark-green line connects each dependent spawn to the master spawn "
        "whose respawn timer it tracks.  Silently empty when the linked_"
        "respawn table is absent or holds no row on the current map."));
    connect(spawnLinksLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_SPAWN_LINKS, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::SpawnLinks, on);
        if (on)
            loadSpawnLinks();
        else if (m_viewer)
            m_viewer->setSpawnLinks({});
    });
    m_layerToggles.push_back({spawnLinksLayer, render::Layer::SpawnLinks, false, SETTINGS_SPAWN_LINKS});

    // Road network overlay: gold polylines walked off the dtNavMesh's
    // NAV_AREA_ROAD-tagged polygons + an optional coral handcrafted pass
    // pushed by a separate agent.  Default ON; the auto-extracted skeleton
    // is rebuilt automatically on every setNavMesh() call so it stays in
    // sync with the loaded map without operator intervention.
    auto* showRoadsLayer = viewMenu->addAction(tr("Show road &network"));
    showRoadsLayer->setCheckable(true);
    {
        QSettings s;
        showRoadsLayer->setChecked(s.value(SETTINGS_SHOW_ROADS, true).toBool());
    }
    showRoadsLayer->setToolTip(tr(
        "Overlay the road network on top of the heightmap/minimap.  Gold "
        "polylines come from the auto-extracted NAV_AREA_ROAD polygons in "
        "the loaded mmtile set; coral red polylines (when present) come "
        "from a curated handcrafted-road table maintained separately."));
    connect(showRoadsLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_SHOW_ROADS, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::Roads, on);
    });
    m_layerToggles.push_back({showRoadsLayer, render::Layer::Roads, true, SETTINGS_SHOW_ROADS});

    // Sibling-highlight overlay: golden ring around every spawn whose
    // creature_template.entry matches the currently selected spawn.
    // Default ON; the per-click sibling walk costs O(|spawns|) which is
    // a few hundred microseconds for a populated continent so the cost
    // is paid only on the click itself, not per-frame.
    auto* siblingLayer = viewMenu->addAction(tr("Show spawn &siblings"));
    siblingLayer->setCheckable(true);
    {
        QSettings s;
        siblingLayer->setChecked(s.value(SETTINGS_SIBLING_HIGHLIGHT, true).toBool());
    }
    siblingLayer->setToolTip(tr(
        "When ON, clicking a creature spawn draws a golden ring around every "
        "other spawn that shares the same creature_template.entry on this "
        "map, capped at 200 siblings."));
    connect(siblingLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_SIBLING_HIGHLIGHT, on);
        if (m_viewer)
        {
            m_viewer->setLayerVisible(render::Layer::SiblingHighlight, on);
            // Toggling OFF mid-selection clears the visible highlight set
            // so the operator's screen matches the menu state immediately.
            if (!on)
                m_viewer->setHighlightedSiblings({});
        }
    });
    m_layerToggles.push_back({siblingLayer, render::Layer::SiblingHighlight, true, SETTINGS_SIBLING_HIGHLIGHT});

    // Phase-mask filter submenu: enable + set phaseId + set phaseGroup.
    // Persisted in QSettings under viewer2d/phase_*; the viewer hides
    // spawns whose phaseId AND phaseGroup both fail to match.  Spawns
    // with both phase fields == 0 are always shown (TC "no phase
    // restriction" convention).
    QMenu* phaseMenu = viewMenu->addMenu(tr("&Phase filter"));
    m_phaseEnableAction = phaseMenu->addAction(tr("&Enable phase filter"));
    m_phaseEnableAction->setCheckable(true);
    {
        QSettings s;
        m_phaseEnableAction->setChecked(s.value(SETTINGS_PHASE_FILTER_ENABLED, false).toBool());
    }
    m_phaseEnableAction->setToolTip(tr(
        "Hide spawns whose phaseId AND phaseGroup both differ from the "
        "configured filter.  Spawns with both phase fields == 0 are "
        "always visible.  Useful in phased zones (Wrathgate, Garrison)."));
    connect(m_phaseEnableAction, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_PHASE_FILTER_ENABLED, on);
        applyPhaseFilter();
    });
    auto* phaseIdAction = phaseMenu->addAction(tr("Set &phaseId..."));
    connect(phaseIdAction, &QAction::triggered, this, [this]() {
        QSettings s;
        int const cur = s.value(SETTINGS_PHASE_FILTER_ID, 0).toInt();
        bool ok = false;
        int const v = QInputDialog::getInt(this,
            tr("Phase filter: phaseId"),
            tr("phaseId (0 = ignore this axis):"),
            cur, 0, 0x7fffffff, 1, &ok);
        if (!ok) return;
        s.setValue(SETTINGS_PHASE_FILTER_ID, v);
        applyPhaseFilter();
    });
    auto* phaseGroupAction = phaseMenu->addAction(tr("Set phase&Group..."));
    connect(phaseGroupAction, &QAction::triggered, this, [this]() {
        QSettings s;
        int const cur = s.value(SETTINGS_PHASE_FILTER_GROUP, 0).toInt();
        bool ok = false;
        int const v = QInputDialog::getInt(this,
            tr("Phase filter: phaseGroup"),
            tr("phaseGroup (0 = ignore this axis):"),
            cur, 0, 0x7fffffff, 1, &ok);
        if (!ok) return;
        s.setValue(SETTINGS_PHASE_FILTER_GROUP, v);
        applyPhaseFilter();
    });

    // Battlemaster recruitment-radius overlay.  Draws a dashed yellow
    // ring around every creature spawn flagged UNIT_NPC_FLAG_BATTLEMASTER
    // so the operator can see where a player has to stand to queue for
    // a BG.  Radius defaults to 5 yards (TC default interaction range);
    // operator can override via the sibling action.
    auto* battlemasterLayer = viewMenu->addAction(tr("&Battlemaster overlay"));
    battlemasterLayer->setCheckable(true);
    {
        QSettings s;
        bool const on = s.value(SETTINGS_BATTLEMASTER_OVERLAY, true).toBool();
        battlemasterLayer->setChecked(on);
        double const radius = s.value(SETTINGS_BATTLEMASTER_RADIUS, 5.0).toDouble();
        if (m_viewer)
        {
            m_viewer->setLayerVisible(render::Layer::BattlemasterRange, on);
            m_viewer->setBattlemasterRange(static_cast<float>(radius));
        }
    }
    battlemasterLayer->setToolTip(tr(
        "Overlay a dashed yellow recruitment-radius ring around every "
        "creature spawn flagged UNIT_NPC_FLAG_BATTLEMASTER (the NPCs "
        "that queue players for battlegrounds).  Radius is configurable."));
    connect(battlemasterLayer, &QAction::toggled, this, [this](bool on) {
        QSettings s;
        s.setValue(SETTINGS_BATTLEMASTER_OVERLAY, on);
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::BattlemasterRange, on);
    });
    m_layerToggles.push_back({battlemasterLayer, render::Layer::BattlemasterRange, true, SETTINGS_BATTLEMASTER_OVERLAY});
    auto* battlemasterRadius = viewMenu->addAction(tr("Battlemaster &radius..."));
    battlemasterRadius->setToolTip(tr(
        "Set the recruitment-radius ring radius in yards (1..50).  TC's "
        "default NPC interaction range is 5 yards."));
    connect(battlemasterRadius, &QAction::triggered, this, [this]() {
        QSettings s;
        double const cur = s.value(SETTINGS_BATTLEMASTER_RADIUS, 5.0).toDouble();
        bool ok = false;
        double const v = QInputDialog::getDouble(this,
            tr("Battlemaster recruitment radius"),
            tr("Radius (yards):"),
            cur, 1.0, 50.0, 1, &ok);
        if (!ok) return;
        s.setValue(SETTINGS_BATTLEMASTER_RADIUS, v);
        if (m_viewer)
            m_viewer->setBattlemasterRange(static_cast<float>(v));
    });

    viewMenu->addSeparator();
    auto* view2DAction = viewMenu->addAction(tr("&2D view"));
    view2DAction->setShortcut(Qt::Key_F2);
    connect(view2DAction, &QAction::triggered, this, &MainWindow::onSwitchTo2D);
    auto* view3DAction = viewMenu->addAction(tr("&3D view"));
    view3DAction->setShortcut(Qt::Key_F3);
    connect(view3DAction, &QAction::triggered, this, &MainWindow::onSwitchTo3D);

    // Continent-level view rotation failsafe.  When checked, the 2D
    // viewer renders rotated 90 degrees clockwise relative to the legacy
    // image-east=screen-right layout - matching the wow.export reference
    // image after the 2026-05-26 A/B test (tall narrow EK with Tirisfal
    // at the top, Stranglethorn at the bottom).  Unchecked falls back to
    // the legacy unrotated layout for side-by-side comparison.  Persisted
    // via QSettings("viewer2d/view_rotation_degrees") on the viewer side.
    auto* viewRotationAction = viewMenu->addAction(tr("View rotation: &90 CW"));
    viewRotationAction->setCheckable(true);
    {
        // Default UNCHECKED on 2026-05-26 pass 2: the prior default-ON
        // build rotated every overlay layer (heightmap / spawns / paths /
        // annotations) in lock-step with the minimap, but those layers
        // were already correct -- only the minimap BLP had the wrong
        // intrinsic axis mapping.  The fix lives in the per-tile minimap
        // transform; this action is now a failsafe toggle that ships
        // unchecked.  Persistence in QSettings is preserved so an operator
        // who manually flips it gets the same state next launch.
        QSettings s;
        bool ok = false;
        double const persisted = s.value(QStringLiteral("viewer2d/view_rotation_degrees"),
                                         0.0).toDouble(&ok);
        bool const initiallyRotated = ok && std::abs(persisted - (-90.0)) < 0.01;
        viewRotationAction->setChecked(initiallyRotated);
    }
    viewRotationAction->setToolTip(tr(
        "Rotate the 2D viewer 90 degrees clockwise around the viewport "
        "center.  Default OFF: the minimap-rotation bug is fixed at the "
        "per-tile transform level, so rotating the whole view (which "
        "would also rotate the already-correct heightmap / spawn / path "
        "/ annotation layers) is unnecessary.  Toggle ON only as an "
        "emergency failsafe for side-by-side comparison."));
    connect(viewRotationAction, &QAction::toggled, this, [this](bool on) {
        if (m_viewer)
            m_viewer->setViewRotationDegrees(on ? -90.0f : 0.0f);
    });

    auto* annotLayer = viewMenu->addAction(tr("&Annotation layer"));
    annotLayer->setCheckable(true);
    annotLayer->setChecked(true);
    // Ctrl+3, NOT bare 3: the number row belongs to the primary layer
    // toggles and 3 already means "minimap texture layer".
    annotLayer->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(annotLayer, &QAction::toggled, this, &MainWindow::onToggleAnnotationLayer);
    m_layerToggles.push_back({annotLayer, render::Layer::Annotations, true, nullptr});

    viewMenu->addSeparator();

    // Path debug mode: 2-click navmesh route preview.  Long red segments
    // hint at off-mesh links the operator should be aware of.  Captured
    // into m_pathDebugAction so the "Path debug" workspace preset can
    // auto-enable it when applying.
    m_pathDebugAction = viewMenu->addAction(tr("&Path debug mode"));
    m_pathDebugAction->setCheckable(true);
    m_pathDebugAction->setChecked(false);
    // Ctrl+Alt+P: Ctrl+Shift+P is the command palette (application-wide
    // QShortcut) -- the old duplicate binding made both ambiguous.
    m_pathDebugAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P));
    connect(m_pathDebugAction, &QAction::toggled, this, [this](bool on)
    {
        if (m_viewer)
            m_viewer->setPathDebugMode(on);
        if (on)
            statusBar()->showMessage(tr("Click start, then end. Long red segments suggest off-mesh links."), 0);
        else
            statusBar()->clearMessage();
    });
    if (m_viewer)
    {
        connect(m_viewer, &render::NavMeshView::pathDebugComputed,
                this, [this](int waypointCount, float totalDistance, bool reachedEnd)
        {
            QString const tail = reachedEnd ? tr("reached") : tr("partial (off-mesh / no link)");
            statusBar()->showMessage(tr("Path: %1 waypoints, %2 yards total, %3")
                                         .arg(waypointCount)
                                         .arg(QString::number(totalDistance, 'f', 1))
                                         .arg(tail), 0);
        });
    }

    auto* frameAction = viewMenu->addAction(tr("&Frame mesh"));
    frameAction->setShortcut(Qt::Key_F);
    connect(frameAction, &QAction::triggered, this, &MainWindow::onFrameView);

    // Workspace presets: flip a named combo of layer toggles in one click.
    // The actions just route to applyWorkspacePreset(name); the impl walks
    // m_layerToggles + drives the two non-layer toggles (path debug, phase
    // filter) explicitly so unrelated state is preserved.
    QMenu* presetMenu = viewMenu->addMenu(tr("&Workspace preset"));
    QStringList const presetNames = {
        tr("Quest design"),
        tr("Spawn debug"),
        tr("Path debug"),
        tr("Faction overview"),
        tr("Phase work"),
        tr("Reset to defaults")
    };
    for (QString const& name : presetNames)
    {
        auto* act = presetMenu->addAction(name);
        connect(act, &QAction::triggered, this, [this, name]() {
            applyWorkspacePreset(name);
        });
    }

    // View -> Bookmarks: persistent named viewpoints, auto-grouped by
    // folder.  The submenu rebuilds itself on aboutToShow so newly-added
    // bookmarks appear without an editor restart.
    m_bookmarksMenu = viewMenu->addMenu(tr("&Bookmarks"));
    connect(m_bookmarksMenu, &QMenu::aboutToShow, this, &MainWindow::onRebuildBookmarksMenu);
    onRebuildBookmarksMenu();

    // View -> Theme: radio-style submenu (System / Light / Dark).  The
    // chosen theme is persisted under "ui/theme" and re-applied on every
    // editor start from restoreSettings().
    {
        QMenu* themeMenu = viewMenu->addMenu(tr("&Theme"));
        auto* themeGroup = new QActionGroup(this);
        themeGroup->setExclusive(true);
        QSettings settings;
        QString const current = settings.value(SETTINGS_THEME, QStringLiteral("system")).toString();
        struct ThemeEntry { char const* key; QString label; };
        ThemeEntry const entries[] = {
            { "system", tr("&System") },
            { "light",  tr("&Light")  },
            { "dark",   tr("&Dark")   },
        };
        for (ThemeEntry const& e : entries)
        {
            auto* act = themeMenu->addAction(e.label);
            act->setCheckable(true);
            act->setActionGroup(themeGroup);
            QString const key = QString::fromLatin1(e.key);
            act->setChecked(current.compare(key, Qt::CaseInsensitive) == 0);
            connect(act, &QAction::triggered, this, [this, key]() { onSelectTheme(key); });
        }
    }

    // Window menu: one checkable toggle per dock.  QDockWidget::
    // toggleViewAction() tracks the dock's visibility automatically, so the
    // checkmarks never desync.  This is the canonical recovery path for a
    // closed dock -- previously 6 of the 10 docks had no reopen affordance
    // at all (the toolbar toggled only 3, and Reset window layout skipped
    // the left annotation toolbox).
    {
        QMenu* windowMenu = menuBar()->addMenu(tr("&Window"));
        for (char const* name : {
                "spawn_dock",
                "annot_dock",
                "annot_toolbox",
                "diag_dock",
                "vendor_dock",
                "conditions_dock",
                "property_inspector_dock",
                "info_inspector_dock",
                "handcrafted_road_dock",
                "log_tail_dock" })
        {
            if (auto* d = findChild<QDockWidget*>(QString::fromLatin1(name)))
                windowMenu->addAction(d->toggleViewAction());
        }
        windowMenu->addSeparator();
        auto* resetLayoutAction = windowMenu->addAction(tr("&Reset window layout"));
        connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetWindowLayout);
    }

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* shortcutsAction = helpMenu->addAction(tr("&Keyboard shortcuts..."));
    // F1 is the conventional "help index" key on Windows/Linux; Ctrl+? is
    // an alternate that works on layouts where F1 is captured.  Both fire
    // the same slot.
    shortcutsAction->setShortcuts({ QKeySequence(QKeySequence::HelpContents),
                                    QKeySequence(QStringLiteral("Ctrl+?")) });
    shortcutsAction->setToolTip(tr("Browse every menu action with its shortcut and description"));
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::onShowShortcuts);
    helpMenu->addSeparator();
    auto* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    // Initial enabled-state pass for Export pending changes (refreshes
    // on every refreshAllViewers() thereafter).
    updateExportPendingActionEnabled();
}

void MainWindow::buildStatusBar()
{
    // Editing-mode badge: permanent (never hidden by transient showMessage
    // text) and first in the permanent cluster.  Colour is always paired
    // with the mode NAME + exit hint, so state is never colour-only.
    m_modeBadge = new QLabel(this);
    statusBar()->addPermanentWidget(m_modeBadge);
    updateModeBadge();

    m_coordsLabel = new QLabel(tr("(no map loaded)"), this);
    m_coordsLabel->setMinimumWidth(220);
    statusBar()->addPermanentWidget(m_coordsLabel);

    m_meshStatsLabel = new QLabel(QString{}, this);
    m_meshStatsLabel->setMinimumWidth(280);
    statusBar()->addWidget(m_meshStatsLabel);

    m_spawnStatsLabel = new QLabel(QString{}, this);
    m_spawnStatsLabel->setMinimumWidth(180);
    statusBar()->addWidget(m_spawnStatsLabel);

    m_annotStatsLabel = new QLabel(QString{}, this);
    m_annotStatsLabel->setMinimumWidth(180);
    statusBar()->addWidget(m_annotStatsLabel);

    m_dbStatusLabel = new QLabel(tr("DB: disconnected"), this);
    m_dbStatusLabel->setMinimumWidth(180);
    statusBar()->addPermanentWidget(m_dbStatusLabel);

    // Coordinate-jump quick-input on the right-hand side of the status
    // bar.  Kept narrow so it doesn't crowd the DB-status / coord labels.
    // Parsing happens in onGotoLineEditReturn so the slot stays unit-
    // testable in isolation if we later add a header-only parser split.
    m_gotoEdit = new QLineEdit(this);
    m_gotoEdit->setPlaceholderText(tr("Goto: X Y [mapId]"));
    m_gotoEdit->setFixedWidth(200);
    m_gotoEdit->setToolTip(tr("Jump: 'X Y', 'X Y mapId', '/entry', '#guid', '?name-fragment'"));
    statusBar()->addPermanentWidget(m_gotoEdit);
    connect(m_gotoEdit, &QLineEdit::returnPressed, this, &MainWindow::onGotoLineEditReturn);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::applyWorkspacePreset(QString const& name)
{
    // Each preset is a list of (action-pointer -> target check-state)
    // entries.  We resolve the action by pointer-equality against the
    // m_layerToggles vector so the lookup survives menu reordering.
    // Layers not mentioned in a preset keep their current state, except
    // for "Reset to defaults" which restores every toggle to its
    // hardcoded default.

    // Build a lookup pointer -> LayerToggle for quick reference.
    auto findAction = [this](char const* tag) -> QAction* {
        // Lookup helper: linear scan over m_layerToggles by action text
        // tag; cheaper than threading 15 named action pointers through
        // the preset table.  Matches the leading non-mnemonic prefix of
        // the menu label so localization changes don't silently break.
        for (auto const& lt : m_layerToggles)
        {
            if (!lt.action) continue;
            QString const txt = lt.action->text();
            // Strip Qt mnemonic '&' for prefix match.
            QString stripped = txt;
            stripped.remove(QLatin1Char('&'));
            if (stripped.startsWith(QString::fromLatin1(tag), Qt::CaseInsensitive))
                return lt.action;
        }
        return nullptr;
    };

    auto setToggle = [&](char const* tag, bool on) {
        if (QAction* a = findAction(tag))
        {
            if (a->isChecked() != on)
                a->setChecked(on);  // fires toggled() => slot pushes state to viewer + QSettings
        }
    };

    // Preset table.  Names match the menu entries exactly (untranslated
    // source string compared via QString::fromLatin1 fallback).  Using
    // the source-string form lets the comparison work even when tr()
    // returns the same string on the en_US default.
    bool const isQuest    = (name == tr("Quest design"));
    bool const isSpawn    = (name == tr("Spawn debug"));
    bool const isPath     = (name == tr("Path debug"));
    bool const isFaction  = (name == tr("Faction overview"));
    bool const isPhase    = (name == tr("Phase work"));
    bool const isReset    = (name == tr("Reset to defaults"));

    if (isReset)
    {
        for (auto const& lt : m_layerToggles)
        {
            if (lt.action && lt.action->isChecked() != lt.defaultOn)
                lt.action->setChecked(lt.defaultOn);
        }
        // Clear path-debug + phase-filter to their hardcoded defaults (both off).
        if (m_pathDebugAction && m_pathDebugAction->isChecked())
            m_pathDebugAction->setChecked(false);
        if (m_phaseEnableAction && m_phaseEnableAction->isChecked())
            m_phaseEnableAction->setChecked(false);
    }
    else if (isQuest)
    {
        setToggle("Navmesh",       false);
        setToggle("Spawn layer",   true);
        setToggle("Heightmap",     true);
        setToggle("Minimap",       true);
        setToggle("Quest marker",  true);
        setToggle("Quest objective", true);
        setToggle("Path layer",    false);
        setToggle("Areatrigger",   false);
        setToggle("Graveyard",     false);
        setToggle("WMO",           false);
        setToggle("Battlemaster overlay", false);
        setToggle("Faction tint",  false);
        setToggle("Level heatmap", false);
        setToggle("Spawn-group",   false);
    }
    else if (isSpawn)
    {
        setToggle("Navmesh",   true);
        setToggle("Spawn layer", true);
        setToggle("Heightmap", true);
        setToggle("Path layer", true);
        setToggle("Minimap",   false);
        setToggle("Areatrigger", false);
        setToggle("Graveyard", false);
        setToggle("WMO",       false);
        setToggle("Quest marker", false);
        setToggle("Quest objective", false);
        setToggle("Spawn-group", false);
        setToggle("Battlemaster overlay", false);
        setToggle("Faction tint", false);
        setToggle("Level heatmap", false);
    }
    else if (isPath)
    {
        setToggle("Navmesh",   true);
        setToggle("Spawn layer", false);
        setToggle("Heightmap", true);
        setToggle("Path layer", true);
        setToggle("Areatrigger", false);
        setToggle("Minimap",   false);
        setToggle("Graveyard", false);
        setToggle("WMO",       false);
        setToggle("Quest marker", false);
        setToggle("Quest objective", false);
        setToggle("Spawn-group", false);
        setToggle("Battlemaster overlay", false);
        setToggle("Faction tint", false);
        setToggle("Level heatmap", false);
        // Auto-enable path-debug mode if it's not already on.
        if (m_pathDebugAction && !m_pathDebugAction->isChecked())
            m_pathDebugAction->setChecked(true);
    }
    else if (isFaction)
    {
        setToggle("Navmesh",   false);
        setToggle("Spawn layer", true);
        setToggle("Heightmap", true);
        setToggle("Minimap",   true);
        setToggle("Faction tint", true);
        setToggle("Level heatmap", false);
        setToggle("Path layer", false);
        setToggle("Areatrigger", false);
        setToggle("Graveyard", false);
        setToggle("WMO",       false);
        setToggle("Quest marker", false);
        setToggle("Quest objective", false);
        setToggle("Spawn-group", false);
        setToggle("Battlemaster overlay", false);
    }
    else if (isPhase)
    {
        setToggle("Spawn layer", true);
        setToggle("Heightmap", true);
        setToggle("Annotation", true);
        // Phase filter slot remains as-is by spec; nudge the operator
        // toward the configuration shortcuts living under View -> Phase
        // filter -> Set phaseId / phaseGroup.
        statusBar()->showMessage(
            tr("Phase work preset: configure filter via View -> Phase filter -> Set phaseId / phaseGroup."),
            6000);
    }
    else
    {
        // Unknown preset name -- bail without touching layers.
        return;
    }

    // Persist last-applied preset so restoreSettings() can replay it.
    QSettings settings;
    settings.setValue(SETTINGS_LAST_PRESET, name);

    statusBar()->showMessage(tr("Workspace preset '%1' applied.").arg(name), 4000);
}

void MainWindow::restoreSettings()
{
    QSettings settings;
    if (m_mmapsDir.isEmpty())
        m_mmapsDir = settings.value(SETTINGS_MMAPS_DIR).toString();
    if (m_mapsDir.isEmpty())
        m_mapsDir = settings.value(SETTINGS_MAPS_DIR).toString();
    if (m_vmapsDir.isEmpty())
        m_vmapsDir = settings.value(SETTINGS_VMAPS_DIR).toString();
    if (m_minimapDir.isEmpty())
        m_minimapDir = settings.value(SETTINGS_MINIMAP_DIR).toString();
    if (m_cascClientDir.isEmpty())
        m_cascClientDir = settings.value(SETTINGS_CASC_DIR).toString();
    if (m_listfileCsvPath.isEmpty())
        m_listfileCsvPath = settings.value(SETTINGS_LISTFILE_CSV).toString();
    if (m_viewer && !m_minimapDir.isEmpty())
        m_viewer->setMinimapDir(m_minimapDir);
    if (m_viewer3d && !m_minimapDir.isEmpty())
        m_viewer3d->setMinimapDir(m_minimapDir);
    // Auto-load the persisted listfile silently — failures surface on the
    // status bar instead of a modal so startup never blocks on a dialog.
    if (!m_listfileCsvPath.isEmpty())
    {
        if (!m_listfile)
            m_listfile = std::make_unique<io::ListfileLookup>();
        QString err;
        if (!m_listfile->loadFromFile(m_listfileCsvPath, &err))
        {
            statusBar()->showMessage(tr("Listfile auto-load failed: %1").arg(err), 8000);
        }
        else
        {
            statusBar()->showMessage(
                tr("Listfile loaded: %1 entries").arg(qulonglong(m_listfile->entryCount())),
                6000);
            if (m_viewer)
                m_viewer->setListfileLookup(m_listfile.get());
            // Wire the listfile into CascClient too so any reader using
            // readByPath() (ADT, doodad obj0, liquid) transparently
            // falls back to FDID lookup when CASC's internal root catalog
            // doesn't know the vpath.
            if (m_cascClient)
                m_cascClient->setListfile(m_listfile.get());
        }
    }
    // Diagnostic: surface the two directories the minimap layer reads
    // from on every startup.  When the 2D view comes up "missing real-map
    // textures" the qDebug output answers the first question -- is this
    // operator configured for CASC, PNG, neither, or both?
    qDebug().nospace()
        << "[world_editor] CASC client dir: "
        << (m_cascClientDir.isEmpty() ? QStringLiteral("<unset>") : m_cascClientDir)
        << " | minimap PNG dir: "
        << (m_minimapDir.isEmpty()    ? QStringLiteral("<unset>") : m_minimapDir);
    // Best-effort auto-open of the CASC storage if a path is saved.  Failures
    // surface on the status bar (not modally -- startup must never block on
    // a dialog).  The operator can re-pick via File -> Set WoW client
    // directory, which uses the modal failure path.
    if (!m_cascClientDir.isEmpty())
    {
        QString err;
        if (!openCascAndMapDb2(err))
            statusBar()->showMessage(tr("CASC auto-open failed: %1").arg(err), 6000);
        else if (!err.isEmpty())
            statusBar()->showMessage(tr("CASC opened; Map.db2 load failed (%1) -- using fallback table.").arg(err), 8000);
    }
    // Fall back to sibling-of-mapsDir convention (data/maps, data/vmaps).
    if (m_vmapsDir.isEmpty() && !m_mapsDir.isEmpty())
    {
        std::filesystem::path p = std::filesystem::path(m_mapsDir.toStdString()).parent_path() / "vmaps";
        if (std::filesystem::exists(p))
            m_vmapsDir = QString::fromStdString(p.string());
    }
    // Propagate the restored maps dir into MapTileCache.  Reading the
    // QSettings value into m_mapsDir alone was the bug: the cache stayed
    // empty across sessions, so every reopen broke heightmap render +
    // snap-to-ground until the operator re-picked the maps directory
    // from the File menu.
    if (m_mapTileCache && !m_mapsDir.isEmpty())
        m_mapTileCache->setMapsDir(std::filesystem::path(m_mapsDir.toStdString()));
    restoreGeometry(settings.value(SETTINGS_GEOMETRY).toByteArray());
    restoreState(settings.value(SETTINGS_STATE).toByteArray());
    // Push the persisted phase filter into the viewer.  Safe to call
    // even when no spawns are loaded yet -- it just stages the filter.
    applyPhaseFilter();
    // Replay the last-applied workspace preset on top of the per-layer
    // QSettings restore so the named combo wins when both are present.
    // Skipped silently when no preset was ever applied.
    QString const lastPreset = settings.value(SETTINGS_LAST_PRESET).toString();
    if (!lastPreset.isEmpty())
        applyWorkspacePreset(lastPreset);
    // Apply the persisted theme so the operator's last choice survives a
    // restart.  main.cpp already forces Fusion at startup; applyTheme()
    // overwrites the palette to match the chosen mode.
    applyTheme(settings.value(SETTINGS_THEME, QStringLiteral("system")).toString());
}

void MainWindow::onSelectTheme(QString const& name)
{
    applyTheme(name);
    QSettings settings;
    settings.setValue(SETTINGS_THEME, name);
    statusBar()->showMessage(tr("Theme: %1").arg(name), 3000);
}

void MainWindow::applyTheme(QString const& name)
{
    // Always use Fusion: it renders identically on every platform and
    // honors palette overrides (native styles often ignore palette).
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QString const key = name.toLower();
    if (key == QStringLiteral("dark"))
    {
        QPalette p;
        p.setColor(QPalette::Window,          QColor(43, 43, 43));
        p.setColor(QPalette::WindowText,      Qt::white);
        p.setColor(QPalette::Base,            QColor(30, 30, 30));
        p.setColor(QPalette::AlternateBase,   QColor(53, 53, 53));
        p.setColor(QPalette::ToolTipBase,     Qt::white);
        p.setColor(QPalette::ToolTipText,     Qt::white);
        p.setColor(QPalette::Text,            Qt::white);
        p.setColor(QPalette::Button,          QColor(58, 58, 58));
        p.setColor(QPalette::ButtonText,      Qt::white);
        p.setColor(QPalette::BrightText,      Qt::red);
        p.setColor(QPalette::Link,            QColor(42, 130, 218));
        p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::black);
        // Disabled-role overrides so greyed-out controls are still legible
        // against the dark window background instead of vanishing.
        p.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::Text,            QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(80, 80, 80));
        QApplication::setPalette(p);
        return;
    }
    if (key == QStringLiteral("light"))
    {
        QPalette p;
        p.setColor(QPalette::Window,          Qt::white);
        p.setColor(QPalette::WindowText,      Qt::black);
        p.setColor(QPalette::Base,            QColor(248, 248, 248));
        p.setColor(QPalette::AlternateBase,   QColor(233, 233, 233));
        p.setColor(QPalette::ToolTipBase,     Qt::black);
        p.setColor(QPalette::ToolTipText,     Qt::black);
        p.setColor(QPalette::Text,            Qt::black);
        p.setColor(QPalette::Button,          QColor(240, 240, 240));
        p.setColor(QPalette::ButtonText,      Qt::black);
        p.setColor(QPalette::BrightText,      Qt::red);
        p.setColor(QPalette::Link,            QColor(42, 100, 200));
        p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(140, 140, 140));
        p.setColor(QPalette::Disabled, QPalette::Text,            QColor(140, 140, 140));
        p.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(140, 140, 140));
        QApplication::setPalette(p);
        return;
    }
    // "system" (or any unrecognized value): clear the explicit palette so
    // QApplication falls back to the Fusion style's standard palette,
    // which closely tracks the host OS' light/dark setting on Qt 6.
    QApplication::setPalette(QApplication::style()->standardPalette());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(SETTINGS_MMAPS_DIR, m_mmapsDir);
    settings.setValue(SETTINGS_MAPS_DIR,  m_mapsDir);
    settings.setValue(SETTINGS_VMAPS_DIR, m_vmapsDir);
    settings.setValue(SETTINGS_MINIMAP_DIR, m_minimapDir);
    settings.setValue(SETTINGS_CASC_DIR,    m_cascClientDir);
    settings.setValue(SETTINGS_LISTFILE_CSV, m_listfileCsvPath);
    settings.setValue(SETTINGS_GEOMETRY,  saveGeometry());
    settings.setValue(SETTINGS_STATE,     saveState());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // Auto-connect once the main window is visible.  We don't do this in
    // the ctor because the diag dock + status label aren't connected
    // until the first show, and finishWorldDbConnect drives them.
    if (!m_autoConnectAttempted)
    {
        m_autoConnectAttempted = true;
        tryAutoConnectWorldDb();
    }
    if (m_pendingOpenMapId.has_value())
    {
        loadAndDisplayMap(*m_pendingOpenMapId);
        m_pendingOpenMapId.reset();
    }
}

void MainWindow::onOpenMmapsDir()
{
    QString const dir = QFileDialog::getExistingDirectory(
        this, tr("Select mmaps directory"), m_mmapsDir);
    if (!dir.isEmpty())
    {
        m_mmapsDir = dir;
        statusBar()->showMessage(tr("mmaps dir: %1").arg(m_mmapsDir), 3000);
    }
}

void MainWindow::onOpenMapsDir()
{
    QString const dir = QFileDialog::getExistingDirectory(
        this, tr("Select maps directory"), m_mapsDir);
    if (!dir.isEmpty())
    {
        m_mapsDir = dir;
        if (m_mapTileCache)
        {
            m_mapTileCache->clear();
            m_mapTileCache->setMapsDir(std::filesystem::path(dir.toStdString()));
        }
        statusBar()->showMessage(tr("maps dir: %1").arg(m_mapsDir), 3000);
    }
}

void MainWindow::onOpenVmapsDir()
{
    QString const dir = QFileDialog::getExistingDirectory(
        this, tr("Select vmaps directory"), m_vmapsDir);
    if (!dir.isEmpty())
    {
        m_vmapsDir = dir;
        statusBar()->showMessage(tr("vmaps dir: %1 (re-open map to reload WMO)").arg(m_vmapsDir), 4000);
    }
}

void MainWindow::onToggleWmoLayer(bool checked)
{
    if (m_viewer3d) m_viewer3d->setWmoVisible(checked);
}

void MainWindow::onToggleRealistic3D(bool checked)
{
    if (m_viewer3d)
        m_viewer3d->setRealistic(checked);
    QSettings settings;
    settings.setValue(SETTINGS_REALISTIC3D, checked);
    statusBar()->showMessage(
        checked ? tr("3D viewer: realistic textures ON")
                : tr("3D viewer: realistic textures OFF"),
        3000);
}

bool MainWindow::openCascAndMapDb2(QString& outError)
{
    if (!m_cascClient || !m_mapDb2)
    {
        outError = tr("CASC subsystem not initialized.");
        return false;
    }
    if (m_cascClientDir.isEmpty())
    {
        outError = tr("No CASC client directory configured.");
        return false;
    }
    if (!m_cascClient->open(m_cascClientDir.toStdString()))
    {
        outError = QString::fromStdString(m_cascClient->lastError());
        return false;
    }
    // load() merges the on-disk DB2 with the hardcoded fallback table;
    // a parse failure is non-fatal -- we still get the fallback.
    bool const db2Ok = m_mapDb2->load(*m_cascClient);
    if (!db2Ok)
        outError = QString::fromStdString(m_mapDb2->lastError());

    if (m_viewer)
        m_viewer->setCascClient(m_cascClient.get(), m_mapDb2.get());
    if (m_viewer3d)
        m_viewer3d->setCascClient(m_cascClient.get(), m_mapDb2.get());

    return true;
}

void MainWindow::onSetCascClientDir()
{
    QString const initial = m_cascClientDir.isEmpty()
        ? QDir::homePath() : m_cascClientDir;
    QString const dir = QFileDialog::getExistingDirectory(this,
        tr("Pick WoW client install directory"), initial);
    if (dir.isEmpty())
        return;

    m_cascClientDir = dir;
    QString err;
    bool const ok = openCascAndMapDb2(err);
    saveSettings();
    if (!ok)
    {
        // CASC storage didn't open -- bad path, missing _retail_/Data/, or
        // CascLib startup failure.  Surface as a modal so a first-time
        // operator doesn't think the minimap layer is silently broken.
        QMessageBox::warning(this, tr("CASC open failed"),
            tr("Could not open the WoW storage at:\n  %1\n\n%2\n\n"
               "Verify the folder contains a _retail_/Data/ subtree "
               "(or _classic_/_ptr_/etc.).").arg(dir, err));
        return;
    }
    // CASC opened but Map.db2 failed to parse -- minimap path resolution
    // falls back to the hardcoded MapId->directory table.  Surface modally
    // so the operator knows why an obscure map id might not resolve.
    if (!err.isEmpty())
    {
        QMessageBox::warning(this, tr("Map.db2 load failed"),
            tr("CASC storage opened at:\n  %1\n\n"
               "but Map.db2 didn't load (%2).  Minimap path resolution will "
               "use the hardcoded fallback table; most retail map ids still "
               "work but uncommon ones may not.").arg(dir, err));
    }
    QString const msg = tr("CASC client opened (%1 maps in Map.db2).")
                        .arg(qulonglong(m_mapDb2->size()));
    statusBar()->showMessage(msg, 6000);
}

void MainWindow::onSetMinimapDir()
{
    QString const initial = m_minimapDir.isEmpty()
        ? QDir::homePath() : m_minimapDir;
    QString const dir = QFileDialog::getExistingDirectory(this,
        tr("Pick minimap PNG root"), initial);
    if (dir.isEmpty())
        return;
    m_minimapDir = dir;
    if (m_viewer)   m_viewer->setMinimapDir(m_minimapDir);
    if (m_viewer3d) m_viewer3d->setMinimapDir(m_minimapDir);
    saveSettings();
    statusBar()->showMessage(tr("Minimap directory: %1").arg(dir), 4000);
}

void MainWindow::onSetListfileCsv()
{
    QString const initial = m_listfileCsvPath.isEmpty()
        ? QDir::homePath() : m_listfileCsvPath;
    QString const csv = QFileDialog::getOpenFileName(this,
        tr("Pick wow-listfile CSV"), initial,
        tr("CSV listfiles (*.csv);;All files (*.*)"));
    if (csv.isEmpty())
        return;
    if (!m_listfile)
        m_listfile = std::make_unique<io::ListfileLookup>();
    QString err;
    bool const ok = m_listfile->loadFromFile(csv, &err);
    m_listfileCsvPath = csv;
    QSettings settings;
    settings.setValue(SETTINGS_LISTFILE_CSV, m_listfileCsvPath);
    // Push the loaded listfile to the viewer for FDID-driven minimap reads.
    if (m_viewer)
        m_viewer->setListfileLookup(m_listfile.get());
    // Same wiring for CascClient: ADT/doodad/liquid readers fall back to
    // FDID via this listfile when readByPath misses.
    if (m_cascClient)
        m_cascClient->setListfile(m_listfile.get());
    if (!ok)
    {
        QMessageBox::warning(this, tr("Listfile load failed"),
            err.isEmpty() ? tr("Failed to read '%1'.").arg(csv) : err);
        statusBar()->showMessage(tr("Listfile load FAILED: %1").arg(err), 6000);
    }
    else
    {
        statusBar()->showMessage(
            tr("Listfile loaded: %1 entries from %2")
                .arg(qulonglong(m_listfile->entryCount()))
                .arg(csv),
            6000);
    }
    onRefreshMinimapDiagnostics();
}

void MainWindow::onToggleMinimapLayer(bool on)
{
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::Minimap, on);
    if (!on)
        return;
    // First-run guide.  If the operator enables the layer but neither a
    // CASC client dir nor a minimap PNG dir is configured, surface the
    // walkthrough so the empty overlay isn't silent.
    QSettings settings;
    QString const cascDir   = settings.value(SETTINGS_CASC_DIR).toString();
    QString const minimapPng = settings.value(SETTINGS_MINIMAP_DIR).toString();
    if (cascDir.isEmpty() && minimapPng.isEmpty())
    {
        app::MinimapSetupWizard wiz(this, this);
        wiz.exec();
    }
}

void MainWindow::onExportMinimapCache()
{
    if (!m_viewer || !m_currentMapId)
    {
        QMessageBox::information(this, tr("No map loaded"),
            tr("Load a map first (File -> Open map...)."));
        return;
    }
    if (!m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
    {
        QMessageBox::information(this, tr("CASC not open"),
            tr("Point the editor at a WoW client directory first "
               "(File -> Set WoW client directory...)."));
        return;
    }
    if (m_minimapDir.isEmpty())
    {
        QMessageBox::information(this, tr("Minimap directory not set"),
            tr("Set the minimap PNG output directory first "
               "(File -> Set minimap directory...)."));
        return;
    }
    auto dir = m_mapDb2->directoryFor(*m_currentMapId);
    if (!dir)
    {
        QMessageBox::warning(this, tr("Unknown map directory"),
            tr("No Map.db2 / fallback entry for map id %1.")
                .arg(*m_currentMapId));
        return;
    }

    // Single-pass over the heightmap tile list.  We can't know the total
    // upfront without a probe pass, so the progress dialog uses an
    // indeterminate range until each tile resolves.
    QProgressDialog progress(tr("Exporting minimap tiles for map %1...").arg(*m_currentMapId),
        tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(200);
    progress.setValue(0);

    QString const baseDir = QStringLiteral("%1/%2").arg(m_minimapDir).arg(*m_currentMapId);
    QDir().mkpath(baseDir);

    int written = 0;
    int skipped = 0;
    int failed  = 0;
    bool aborted = false;
    auto sink = [&](render::NavMeshView::ExportedTile const& tile)
    {
        if (progress.wasCanceled()) { aborted = true; return; }
        QString const path = QStringLiteral("%1/map%2_%3.png")
            .arg(baseDir).arg(tile.gx).arg(tile.gy);
        if (QFile::exists(path))
        {
            ++skipped;
        }
        else
        {
            QImage img(tile.rgba.data(), tile.width, tile.height,
                       tile.width * 4, QImage::Format_RGBA8888);
            if (img.save(path, "PNG"))
                ++written;
            else
                ++failed;
        }
        progress.setLabelText(tr("Exporting map %1: %2 written, %3 skipped, %4 failed")
                              .arg(*m_currentMapId).arg(written).arg(skipped).arg(failed));
        QCoreApplication::processEvents();
    };
    m_viewer->exportMinimapTilesFromCasc(sink);
    progress.close();

    QString const summary = tr("Minimap export complete: %1 written, %2 skipped, %3 failed%4.")
        .arg(written).arg(skipped).arg(failed)
        .arg(aborted ? tr(" (cancelled)") : QString{});
    statusBar()->showMessage(summary, 8000);
    if (failed > 0)
        QMessageBox::warning(this, tr("Minimap export"), summary);
}

// Compose a default screenshot path under the user's home directory.  Used
// by both the 1x and 4x view-export handlers so the operator gets a sane
// timestamped filename in QFileDialog.
static QString defaultScreenshotPath(QString const& suffix)
{
    QString const home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString const base = home.isEmpty() ? QDir::currentPath() : home;
    QString const stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QStringLiteral("%1/world_editor_%2%3.png").arg(base, stamp, suffix);
}

// Returns the QImage of `widget`'s currently visible pixel buffer.  For
// QOpenGLWidget-derived widgets we read the GL backbuffer directly; the
// fallback uses Qt's standard paint pipeline so the action stays useful
// if the central widget ever swaps to a non-GL implementation.
static QImage grabWidgetImage(QWidget* widget)
{
    if (auto* gl = qobject_cast<QOpenGLWidget*>(widget))
        return gl->grabFramebuffer();
    return widget->grab().toImage();
}

void MainWindow::onExportViewAsPng()
{
    QWidget* widget = m_centralStack ? m_centralStack->currentWidget() : nullptr;
    if (!widget)
    {
        QMessageBox::information(this, tr("No view"),
            tr("No view is currently active to export."));
        return;
    }

    QString const defaultPath = defaultScreenshotPath(QString{});
    QString const path = QFileDialog::getSaveFileName(this,
        tr("Export view as PNG"), defaultPath, tr("PNG image (*.png)"));
    if (path.isEmpty())
        return;

    // Force a fresh paint before sampling the backbuffer so the user does
    // not capture a stale frame (e.g. while a dialog was on top).
    widget->update();
    QApplication::processEvents();

    QImage const img = grabWidgetImage(widget);
    if (img.isNull() || !img.save(path, "PNG"))
    {
        QMessageBox::warning(this, tr("Export view"),
            tr("Failed to write PNG to %1.").arg(path));
        return;
    }
    statusBar()->showMessage(tr("Exported view to %1").arg(path), 8000);
}

void MainWindow::onExportViewAsPngHighRes()
{
    QWidget* widget = m_centralStack ? m_centralStack->currentWidget() : nullptr;
    if (!widget)
    {
        QMessageBox::information(this, tr("No view"),
            tr("No view is currently active to export."));
        return;
    }

    QString const defaultPath = defaultScreenshotPath(QStringLiteral("_4x"));
    QString const path = QFileDialog::getSaveFileName(this,
        tr("Export view as PNG (high-res 4x)"), defaultPath, tr("PNG image (*.png)"));
    if (path.isEmpty())
        return;

    // Preserve original geometry so we can restore it even if anything
    // below (resize / paint / save) throws or fails.  The widget's
    // minimum/maximum size are also briefly relaxed so the 4x resize is
    // not clamped by the layout policy of the QStackedWidget.
    QSize    const origSize     = widget->size();
    QSize    const origMinSize  = widget->minimumSize();
    QSize    const origMaxSize  = widget->maximumSize();
    QSize    const bigSize(origSize.width() * 4, origSize.height() * 4);

    auto restore = [&]()
    {
        widget->setMinimumSize(origMinSize);
        widget->setMaximumSize(origMaxSize);
        widget->resize(origSize);
        widget->update();
        QApplication::processEvents();
    };

    bool        ok      = false;
    QString     errMsg;
    QImage      img;
    try
    {
        widget->setMinimumSize(bigSize);
        widget->setMaximumSize(bigSize);
        widget->resize(bigSize);
        widget->update();
        // One paint cycle for the GL widget to lay out + redraw at the new size.
        QApplication::processEvents();
        QApplication::processEvents();

        img = grabWidgetImage(widget);
        if (img.isNull())
            errMsg = tr("grabFramebuffer returned a null image.");
        else if (!img.save(path, "PNG"))
            errMsg = tr("QImage::save returned false for %1.").arg(path);
        else
            ok = true;
    }
    catch (...)
    {
        errMsg = tr("Unexpected exception during high-res capture.");
    }

    // Always restore the widget; the operator must not be left staring at
    // a 4x oversized view because the save step happened to fail.
    restore();

    if (!ok)
    {
        QMessageBox::warning(this, tr("Export view (high-res)"),
            tr("Failed to write PNG: %1").arg(errMsg));
        return;
    }
    statusBar()->showMessage(
        tr("Exported view (4x %1x%2) to %3").arg(img.width()).arg(img.height()).arg(path),
        8000);
}

void MainWindow::onOpenMap()
{
    if (m_mmapsDir.isEmpty())
    {
        QMessageBox::warning(this, tr("mmaps directory not set"),
            tr("Set the mmaps directory first (File -> Set mmaps directory)."));
        return;
    }

    // Which maps can actually be loaded (have mmaps on disk). The tile files are
    // named "<mapId>_<gx>_<gy>.mmtile" (+ optional "<mapId>.mmap"); the leading
    // numeric run is the map id.
    std::set<uint32_t> available;
    {
        QDir dir(m_mmapsDir);
        QStringList filters; filters << QStringLiteral("*.mmtile") << QStringLiteral("*.mmap");
        for (QString const& f : dir.entryList(filters, QDir::Files))
        {
            int n = 0;
            while (n < f.size() && f[n].isDigit()) ++n;
            if (n == 0) continue;
            bool ok = false;
            uint const id = f.left(n).toUInt(&ok);
            if (ok) available.insert(static_cast<uint32_t>(id));
        }
    }

    // No mmaps discovered — fall back to the manual id prompt.
    if (available.empty())
    {
        bool ok = false;
        int const mapId = QInputDialog::getInt(
            this, tr("Open map"), tr("Map id:"), 0, 0, 100000, 1, &ok);
        if (ok)
            loadAndDisplayMap(static_cast<uint32_t>(mapId));
        return;
    }

    // Enrich each loadable map with Map.db2 metadata (name/type/expansion) when
    // CASC is available; otherwise it shows as a bare id under "Other".
    std::vector<io::MapMetadata> maps;
    maps.reserve(available.size());
    for (uint32_t id : available)
    {
        io::MapMetadata m;
        m.mapId = id;
        if (m_mapDb2)
            if (auto md = m_mapDb2->metadataFor(id))
                m = *md;
        maps.push_back(std::move(m));
    }

    // Per-map authored-road counts from the shared DB (best-effort badge).
    std::map<uint32_t, int> roadCounts;
    if (m_worldDb && m_worldDb->isConnected())
    {
        std::string const sql =
            "SELECT mapId, COUNT(*) FROM " + m_worldDb->qualify("handcrafted_road") + " GROUP BY mapId";
        db::QueryResult res;
        if (m_worldDb->query(sql, res).ok())
            for (size_t i = 0; i < res.rowCount(); ++i)
                roadCounts[static_cast<uint32_t>(res.asUInt64(i, 0).value_or(0))] =
                    static_cast<int>(res.asUInt64(i, 1).value_or(0));
    }

    // Recent ids, most-recent-first (parsed inline; the shared helper lives
    // later in this TU).
    std::vector<uint32_t> recent;
    {
        QSettings settings;
        QString const raw = settings.value(QStringLiteral("editor/recent_maps")).toString();
        for (QString const& t : raw.split(QLatin1Char(';'), Qt::SkipEmptyParts))
        {
            QStringList const parts = t.split(QLatin1Char('|'));
            if (parts.isEmpty()) continue;
            bool ok = false;
            uint const id = parts[0].toUInt(&ok);
            if (ok) recent.push_back(static_cast<uint32_t>(id));
        }
    }

    app::MapPickerDialog dlg(std::move(maps), std::move(available), std::move(roadCounts),
                        std::move(recent), this);
    if (dlg.exec() == QDialog::Accepted && dlg.selectedMapId() >= 0)
        loadAndDisplayMap(static_cast<uint32_t>(dlg.selectedMapId()));
}

void MainWindow::loadAndDisplayMap(uint32_t mapId)
{
    if (m_mmapsDir.isEmpty())
    {
        QMessageBox::warning(this, tr("mmaps directory not set"),
            tr("Set the mmaps directory first."));
        return;
    }

    // Comprehensive timing so a stall is visible in debug.log instead of
    // looking like an indefinite hang.  Every phase logs start + elapsed.
    QElapsedTimer phaseT;
    QElapsedTimer totalT;
    totalT.start();
    qInfo("[mapload] BEGIN mapId=%u", mapId);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(tr("Loading map %1 (reading mmaps)...").arg(mapId));
    QApplication::processEvents();

    std::filesystem::path const mmapsPath = m_mmapsDir.toStdString();

    phaseT.start();
    io::LoadedMMap mesh = io::loadMap(mmapsPath, mapId);
    qInfo("[mapload] io::loadMap done in %lld ms (tiles=%llu polys=%llu bytes=%llu)",
        static_cast<long long>(phaseT.elapsed()),
        static_cast<unsigned long long>(mesh.stats().tilesLoaded),
        static_cast<unsigned long long>(mesh.stats().polyCount),
        static_cast<unsigned long long>(mesh.stats().bytesLoaded));

    if (!mesh.ok())
    {
        QApplication::restoreOverrideCursor();
        m_meshStatsLabel->setText(QString{});
        QMessageBox::critical(this, tr("Open map failed"),
            tr("Could not load mmap for map %1.\nLooked for: %2/%3")
            .arg(mapId)
            .arg(m_mmapsDir)
            .arg(QString::fromStdString(io::mmapFilename(mapId))));
        statusBar()->showMessage(tr("Load failed"), 5000);
        qInfo("[mapload] FAILED mapId=%u (mmap not found)", mapId);
        return;
    }

    auto const& stats = mesh.stats();
    m_meshStatsLabel->setText(
        tr("map %1  tiles=%2 (failed %3)  polys=%4  %5 MiB")
            .arg(mapId)
            .arg(stats.tilesLoaded)
            .arg(stats.tilesFailed)
            .arg(stats.polyCount)
            .arg(QString::number(double(stats.bytesLoaded) / (1024.0 * 1024.0), 'f', 1)));

    // The 2D viewer gets the mesh immediately.  The 3D viewer's mesh is
    // deferred to first activation -- a second io::loadMap iterates the
    // whole mmaps directory and parses 1000+ .mmtile files, which on a
    // continent like Kalimdor stalls the UI for many seconds with no
    // visible feedback.  ensureViewer3dMeshLoaded() consumes the queued
    // mapId when the user switches to 3D view.
    statusBar()->showMessage(tr("Loading map %1 (uploading navmesh)...").arg(mapId));
    QApplication::processEvents();
    phaseT.restart();
    m_viewer->setNavMesh(std::move(mesh));
    qInfo("[mapload] 2D setNavMesh done in %lld ms", static_cast<long long>(phaseT.elapsed()));

    m_pendingViewer3dMapId = mapId;
    // If the 3D viewer is currently visible, drain the queue right now so
    // the user sees it populated instead of an empty pane.
    if (m_viewer3d && m_centralStack
        && m_centralStack->currentWidget() == m_viewer3d)
    {
        ensureViewer3dMeshLoaded();
    }
    m_currentMapId = mapId;
    // Keep the handcrafted-road dock's filter in sync with the viewer's
    // current map so its table tracks map switches automatically.
    if (m_handcraftedRoadDock)
        m_handcraftedRoadDock->setCurrentMapId(mapId);
    // Record this open in the Recent maps list; covers both onOpenMap()
    // and any startup --open / external requestOpenMap() entry point.
    recordRecentMap(mapId);
    if (m_mapTileCache && !m_mapTileCache->mapsDir().empty())
    {
        statusBar()->showMessage(tr("Loading map %1 (heightmap tiles)...").arg(mapId));
        QApplication::processEvents();
        phaseT.restart();
        m_viewer->rebuildHeightmapTiles(mapId);
        qInfo("[mapload] 2D rebuildHeightmapTiles done in %lld ms",
            static_cast<long long>(phaseT.elapsed()));
        if (m_viewer3d && m_centralStack
            && m_centralStack->currentWidget() == m_viewer3d)
        {
            phaseT.restart();
            m_viewer3d->rebuildHeightmapTerrain(mapId);
            qInfo("[mapload] 3D rebuildHeightmapTerrain done in %lld ms",
                static_cast<long long>(phaseT.elapsed()));
        }
    }
    // One-time hint when neither minimap source is configured.  Without
    // this the 2D view silently shows just the heightmap relief and the
    // operator has no clue why colors are absent.  Uses a long timeout
    // (12s) since the message has to outlive the other map-open noise.
    bool const cascReady = m_cascClient && m_cascClient->isOpen();
    if (m_minimapDir.isEmpty() && !cascReady)
    {
        statusBar()->showMessage(
            tr("Minimap textures: configure File > Set WoW client directory "
               "or File > Set minimap directory to see real map textures."),
            12000);
    }
    // Phase 5 stretch: load vmaps for the 3D WMO outline overlay AND
    // build the height probe (HANDOFF §10.6 - snap-to-ground inside
    // buildings).  Both steps are blocking, so we keep the cap small by
    // default and pump processEvents around the heavy work so the
    // window doesn't appear frozen.  The operator can raise the cap via
    // File -> Reload vmaps later if they need a wider WMO view.
    if (!m_vmapsDir.isEmpty())
    {
        statusBar()->showMessage(tr("Loading vmaps..."), 0);
        QApplication::processEvents();
        std::filesystem::path const vmapsPath = m_vmapsDir.toStdString();
        constexpr int kVmapTileCap = 10;
        io::LoadedVmap wmo = io::loadVmaps(vmapsPath, mapId, kVmapTileCap);
        QApplication::processEvents();
        auto const& s = wmo.stats();
        if (wmo.ok())
        {
            statusBar()->showMessage(tr("Building WMO height index..."), 0);
            QApplication::processEvents();
            m_vmapProbe = io::VmapHeightProbe(wmo, /*cellSize*/ 16.0f);
            auto const& ps = m_vmapProbe.stats();
            // Snapshot per-WMO-instance AABBs before the wmo is moved to
            // the 3D viewer; the 2D footprint layer reads from this cache.
            m_wmoFootprintsCache = wmo.wmoAabbs();
            m_wmoFootprintsPushed = false;
            statusBar()->showMessage(
                tr("WMO: tiles=%1/%2 models=%3 tris=%4 (wmo=%5/m2=%6) probe-cells=%7 footprints=%8 (tile cap=%9)")
                    .arg(s.tilesLoaded).arg(s.tilesLoaded + s.tilesFailed)
                    .arg(s.modelsLoaded).arg(qulonglong(s.triangleCount))
                    .arg(qulonglong(s.wmoTriangleCount)).arg(qulonglong(s.m2TriangleCount))
                    .arg(qulonglong(ps.cellCount))
                    .arg(qulonglong(m_wmoFootprintsCache.size()))
                    .arg(kVmapTileCap), 5000);
        }
        else
        {
            m_vmapProbe = io::VmapHeightProbe{};
            m_wmoFootprintsCache.clear();
            m_wmoFootprintsPushed = false;
        }
        if (m_viewer3d) m_viewer3d->setVmapMesh(std::move(wmo));
        QApplication::processEvents();
    }
    else
    {
        m_vmapProbe = io::VmapHeightProbe{};
        m_wmoFootprintsCache.clear();
        m_wmoFootprintsPushed = false;
        if (m_viewer3d) m_viewer3d->setVmapMesh(io::LoadedVmap{});
    }
    // If the WMO 2D-footprint layer is currently enabled, refresh the
    // viewer with the new map's cache (or a one-shot status hint if the
    // operator hasn't configured a vmaps dir yet).
    {
        QSettings s;
        bool const layerOn = s.value(SETTINGS_WMO_FOOTPRINTS, false).toBool();
        if (layerOn)
            pushWmoFootprintsToViewer(true);
        else if (m_viewer)
            m_viewer->setWmoFootprints({});
    }
    statusBar()->showMessage(tr("Map %1 loaded").arg(mapId), 3000);

    if (m_worldDb && m_worldDb->isConnected())
    {
        reloadSpawnsForMap(mapId);
        reloadAnnotationsForMap(mapId);
        reloadPathsForMap(mapId);
        reloadDungeonRoutesForMap(mapId);
        reloadAreatriggersForMap(mapId);
        reloadGraveyardsForMap(mapId);
    }
    else
    {
        m_viewer->setSpawns({});
        m_viewer->setAnnotations({});
        m_viewer->setPaths({});
        m_viewer->setAreatriggers({});
        m_viewer->setGraveyards({});
        m_viewer->setInstanceEntrances({});
        if (m_viewer3d)
        {
            m_viewer3d->setAreatriggers({});
            m_viewer3d->setGraveyards({});
        }
        if (m_areatriggerModel) m_areatriggerModel->setBaseline({});
        if (m_areatriggerDock)  { m_areatriggerDock->clear(); m_areatriggerDock->setPendingCount(0); }
        if (m_graveyardModel)   m_graveyardModel->setBaseline({});
        if (m_graveyardDock)    { m_graveyardDock->clear();   m_graveyardDock->setPendingCount(0); }
    }
    QApplication::restoreOverrideCursor();
    qInfo("[mapload] END mapId=%u (total %lld ms)", mapId,
        static_cast<long long>(totalT.elapsed()));
    statusBar()->showMessage(tr("Map %1 loaded.").arg(mapId), 4000);
}

void MainWindow::ensureViewer3dMeshLoaded()
{
    if (!m_viewer3d || !m_pendingViewer3dMapId.has_value())
        return;
    uint32_t const mapId = *m_pendingViewer3dMapId;
    // Re-entry guard: if the 3D viewer already shows this mapId we have
    // nothing to do.  Common case after the user toggles 2D <-> 3D.
    if (m_viewer3dMapLoaded && *m_viewer3dMapLoaded == mapId)
    {
        m_pendingViewer3dMapId.reset();
        return;
    }
    if (m_mmapsDir.isEmpty())
        return;

    QElapsedTimer t;
    t.start();
    qInfo("[mapload-3d] BEGIN deferred 3D mesh load mapId=%u", mapId);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(tr("Loading 3D view for map %1...").arg(mapId));
    QApplication::processEvents();

    std::filesystem::path const mmapsPath = m_mmapsDir.toStdString();
    QElapsedTimer phaseT;
    phaseT.start();
    io::LoadedMMap meshFor3D = io::loadMap(mmapsPath, mapId);
    qInfo("[mapload-3d] io::loadMap done in %lld ms (tiles=%llu)",
        static_cast<long long>(phaseT.elapsed()),
        static_cast<unsigned long long>(meshFor3D.stats().tilesLoaded));

    if (meshFor3D.ok())
    {
        phaseT.restart();
        m_viewer3d->setNavMesh(std::move(meshFor3D));
        qInfo("[mapload-3d] setNavMesh done in %lld ms",
            static_cast<long long>(phaseT.elapsed()));
        if (m_mapTileCache && !m_mapTileCache->mapsDir().empty())
        {
            phaseT.restart();
            m_viewer3d->rebuildHeightmapTerrain(mapId);
            qInfo("[mapload-3d] rebuildHeightmapTerrain done in %lld ms",
                static_cast<long long>(phaseT.elapsed()));
        }
        m_viewer3dMapLoaded = mapId;
    }
    else
    {
        qInfo("[mapload-3d] FAILED (mmap not found) mapId=%u", mapId);
    }
    m_pendingViewer3dMapId.reset();
    QApplication::restoreOverrideCursor();
    qInfo("[mapload-3d] END mapId=%u (total %lld ms)", mapId,
        static_cast<long long>(t.elapsed()));
}

void MainWindow::reloadAreatriggersForMap(uint32_t mapId)
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    // JOIN with areatrigger_create_properties to pull Shape + ShapeData0..7
    // so the viewer can render the actual sphere/box outline.
    char sql[1500];
    std::snprintf(sql, sizeof(sql),
        "SELECT a.SpawnId, a.AreaTriggerCreatePropertiesId, a.IsCustom, a.MapId, "
        "       a.SpawnDifficulties, a.PosX, a.PosY, a.PosZ, a.Orientation, "
        "       COALESCE(a.PhaseUseFlags, 0), COALESCE(a.PhaseId, 0), "
        "       COALESCE(a.PhaseGroup, 0), "
        "       a.ScriptName, COALESCE(a.Comment, ''), a.VerifiedBuild, "
        "       COALESCE(p.Shape, 0), "
        "       COALESCE(p.ShapeData0, 0), COALESCE(p.ShapeData1, 0), "
        "       COALESCE(p.ShapeData2, 0), COALESCE(p.ShapeData3, 0), "
        "       COALESCE(p.ShapeData4, 0), COALESCE(p.ShapeData5, 0), "
        "       COALESCE(p.ShapeData6, 0), COALESCE(p.ShapeData7, 0) "
        "FROM areatrigger a "
        "LEFT JOIN areatrigger_create_properties p "
        "       ON p.Id = a.AreaTriggerCreatePropertiesId "
        "      AND p.IsCustom = a.IsCustom "
        "WHERE a.MapId = %u", mapId);
    db::QueryResult res;
    auto const err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("areatrigger query failed: %1")
            .arg(QString::fromStdString(err.message)), 4000);
        m_viewer->setAreatriggers({});
        m_areatriggerModel->setBaseline({});
        return;
    }
    std::vector<render::Areatrigger> atrs;
    atrs.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Areatrigger a;
        a.spawnId            = res.asInt64 (r, 0).value_or(0);
        a.createPropsId      = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));
        a.isCustom           = static_cast<uint8_t>(res.asUInt64(r, 2).value_or(0));
        a.mapId              = static_cast<uint32_t>(res.asUInt64(r, 3).value_or(mapId));
        a.spawnDifficulties  = QString::fromStdString(res.cell(r, 4));
        a.x                  = static_cast<float>(res.asDouble(r, 5).value_or(0.0));
        a.y                  = static_cast<float>(res.asDouble(r, 6).value_or(0.0));
        a.z                  = static_cast<float>(res.asDouble(r, 7).value_or(0.0));
        a.orientation        = static_cast<float>(res.asDouble(r, 8).value_or(0.0));
        a.phaseUseFlags      = static_cast<uint8_t>(res.asUInt64(r, 9).value_or(0));
        a.phaseId            = static_cast<uint32_t>(res.asUInt64(r, 10).value_or(0));
        a.phaseGroup         = static_cast<uint32_t>(res.asUInt64(r, 11).value_or(0));
        a.scriptName         = QString::fromStdString(res.cell(r, 12));
        a.comment            = QString::fromStdString(res.cell(r, 13));
        a.verifiedBuild      = static_cast<uint32_t>(res.asUInt64(r, 14).value_or(0));
        a.shape              = uint8_t(res.asUInt64(r, 15).value_or(0));
        for (int k = 0; k < 8; ++k)
            a.shapeData[k] = float(res.asDouble(r, 16 + k).value_or(0.0));
        atrs.push_back(std::move(a));
    }
    m_areatriggerModel->setBaseline(atrs);
    if (m_viewer3d) m_viewer3d->setAreatriggers(atrs);
    m_viewer->setAreatriggers(std::move(atrs));
    refreshAreatriggerSpawnIdReservation();
}

void MainWindow::reloadGraveyardsForMap(uint32_t mapId)
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT ID, COALESCE(MapID, 0), COALESCE(LocX, 0), COALESCE(LocY, 0), "
        "       COALESCE(LocZ, 0), COALESCE(Facing, 0), "
        "       COALESCE(TransportSpawnId, 0), COALESCE(Comment, '') "
        "FROM world_safe_locs WHERE MapID = %u", mapId);
    db::QueryResult res;
    auto const err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("world_safe_locs query failed: %1")
            .arg(QString::fromStdString(err.message)), 4000);
        m_viewer->setGraveyards({});
        if (m_graveyardModel) m_graveyardModel->setBaseline({});
        return;
    }
    std::vector<render::Graveyard> gys;
    gys.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Graveyard g;
        g.id               = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        g.mapId            = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(mapId));
        g.x                = static_cast<float>(res.asDouble(r, 2).value_or(0.0));
        g.y                = static_cast<float>(res.asDouble(r, 3).value_or(0.0));
        g.z                = static_cast<float>(res.asDouble(r, 4).value_or(0.0));
        g.facing           = static_cast<float>(res.asDouble(r, 5).value_or(0.0));
        g.transportSpawnId = res.asUInt64(r, 6).value_or(0);
        g.comment          = QString::fromStdString(res.cell(r, 7));
        gys.push_back(std::move(g));
    }
    statusBar()->showMessage(tr("areatriggers + graveyards loaded"), 2000);
    if (m_graveyardModel) m_graveyardModel->setBaseline(gys);
    if (m_viewer3d) m_viewer3d->setGraveyards(gys);
    m_viewer->setGraveyards(std::move(gys));
    refreshGraveyardIdReservation();
}

void MainWindow::onToggleAreatriggerLayer(bool checked)
{
    if (m_viewer) m_viewer->setLayerVisible(render::Layer::Areatriggers, checked);
    if (m_viewer3d) m_viewer3d->setLayerVisible(render::Layer::Areatriggers, checked);
}

void MainWindow::onToggleGraveyardLayer(bool checked)
{
    if (m_viewer) m_viewer->setLayerVisible(render::Layer::Graveyards, checked);
    if (m_viewer3d) m_viewer3d->setLayerVisible(render::Layer::Graveyards, checked);
}

void MainWindow::onAreatriggerClicked(int idx)
{
    if (!m_viewer || idx < 0 || idx >= int(m_viewer->areatriggers().size())) return;
    render::Areatrigger const& displayed = m_viewer->areatriggers()[idx];

    // The viewer index is into the FILTERED display list (deletes pruned);
    // resolve back to the model row by SpawnId.
    int modelIndex = -1;
    if (m_areatriggerModel)
    {
        auto const& current = m_areatriggerModel->current();
        for (size_t i = 0; i < current.size(); ++i)
        {
            if (current[i].spawnId == displayed.spawnId)
            {
                modelIndex = static_cast<int>(i);
                break;
            }
        }
    }
    m_selectedAreatriggerIndex = modelIndex;
    if (modelIndex >= 0 && m_areatriggerDock && m_areatriggerModel)
    {
        m_areatriggerDock->setAreatrigger(modelIndex,
            m_areatriggerModel->current()[modelIndex]);
        m_areatriggerDock->setPendingCount(m_areatriggerModel->pendingCount());
        if (m_propertyInspector)
            m_propertyInspector->showTab(app::PropertyInspectorDock::Tab::Areatrigger);
    }
    if (m_conditionsDock)
        m_conditionsDock->setAreatriggerScope(displayed.createPropsId);
    // Push the script registry scope: non-empty -> spawn list for that
    // ScriptName; empty -> clear (operator opens the summary from Tools).
    if (m_atrScriptDock)
    {
        if (displayed.scriptName.isEmpty())
            m_atrScriptDock->clear();
        else
            m_atrScriptDock->setScriptName(displayed.scriptName);
    }
    statusBar()->showMessage(
        tr("areatrigger spawnId=%1 createProps=%2 script='%3' comment='%4'")
            .arg(displayed.spawnId).arg(displayed.createPropsId)
            .arg(displayed.scriptName.isEmpty() ? QStringLiteral("-") : displayed.scriptName)
            .arg(displayed.comment.isEmpty()    ? QStringLiteral("-") : displayed.comment),
        8000);
}

void MainWindow::onGraveyardClicked(int idx)
{
    if (!m_viewer || idx < 0 || idx >= int(m_viewer->graveyards().size())) return;
    render::Graveyard const& displayed = m_viewer->graveyards()[idx];

    int modelIndex = -1;
    if (m_graveyardModel)
    {
        auto const& current = m_graveyardModel->current();
        for (size_t i = 0; i < current.size(); ++i)
        {
            if (current[i].id == displayed.id)
            {
                modelIndex = static_cast<int>(i);
                break;
            }
        }
    }
    m_selectedGraveyardIndex = modelIndex;
    if (modelIndex >= 0 && m_graveyardDock && m_graveyardModel)
    {
        m_graveyardDock->setGraveyard(modelIndex,
            m_graveyardModel->current()[modelIndex]);
        m_graveyardDock->setPendingCount(m_graveyardModel->pendingCount());
        if (m_propertyInspector)
            m_propertyInspector->showTab(app::PropertyInspectorDock::Tab::Graveyard);
    }
    statusBar()->showMessage(
        tr("graveyard id=%1 at (%2, %3, %4) - %5")
            .arg(displayed.id)
            .arg(displayed.x, 0, 'f', 1).arg(displayed.y, 0, 'f', 1).arg(displayed.z, 0, 'f', 1)
            .arg(displayed.comment.isEmpty() ? QStringLiteral("(no comment)") : displayed.comment),
        8000);
}

void MainWindow::buildPropertyInspectorDock()
{
    // Single QDockWidget hosting Path / Areatrigger / Graveyard / Pool /
    // SpawnGroup as tabs of one PropertyInspectorDock widget.
    auto* dock = new QDockWidget(tr("Property inspector"), this);
    dock->setObjectName(QStringLiteral("property_inspector_dock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertyInspector = new app::PropertyInspectorDock(dock);
    dock->setWidget(m_propertyInspector);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    if (auto* anchor = findChild<QDockWidget*>(QStringLiteral("spawn_dock")))
        tabifyDockWidget(anchor, dock);

    // Keep the legacy member pointers populated so the rest of MainWindow
    // (commit / edit / revert paths) keeps working unchanged.
    m_pathDock        = m_propertyInspector->pathDock();
    m_areatriggerDock = m_propertyInspector->areatriggerDock();
    m_graveyardDock   = m_propertyInspector->graveyardDock();

    connect(m_pathDock, &app::PathPropertiesDock::pathEdited,
            this, &MainWindow::onPathEdited);
    connect(m_pathDock, &app::PathPropertiesDock::deletePathRequested,
            this, &MainWindow::onDeleteSelectedPath);
    connect(m_pathDock, &app::PathPropertiesDock::commitRequested,
            this, &MainWindow::onCommitPaths);
    connect(m_pathDock, &app::PathPropertiesDock::revertRequested,
            this, &MainWindow::onRevertPaths);
    connect(m_pathDock, &app::PathPropertiesDock::assignToSelectedSpawnRequested,
            this, &MainWindow::onAssignPathToSelectedSpawn);

    connect(m_areatriggerDock, &app::AreatriggerPropertiesDock::areatriggerEdited,
            this, &MainWindow::onAreatriggerEdited);
    connect(m_areatriggerDock, &app::AreatriggerPropertiesDock::deleteAreatriggerRequested,
            this, &MainWindow::onDeleteSelectedAreatrigger);
    connect(m_areatriggerDock, &app::AreatriggerPropertiesDock::commitRequested,
            this, &MainWindow::onCommitAreatriggers);
    connect(m_areatriggerDock, &app::AreatriggerPropertiesDock::revertRequested,
            this, &MainWindow::onRevertAreatriggers);

    connect(m_graveyardDock, &app::GraveyardPropertiesDock::graveyardEdited,
            this, &MainWindow::onGraveyardEdited);
    connect(m_graveyardDock, &app::GraveyardPropertiesDock::deleteGraveyardRequested,
            this, &MainWindow::onDeleteSelectedGraveyard);
    connect(m_graveyardDock, &app::GraveyardPropertiesDock::commitRequested,
            this, &MainWindow::onCommitGraveyards);
    connect(m_graveyardDock, &app::GraveyardPropertiesDock::revertRequested,
            this, &MainWindow::onRevertGraveyards);

    connect(m_propertyInspector, &app::PropertyInspectorDock::openPoolEditorRequested,
            this, &MainWindow::onShowPoolEditorFromInspector);
    connect(m_propertyInspector, &app::PropertyInspectorDock::openSpawnGroupEditorRequested,
            this, &MainWindow::onShowSpawnGroupEditorFromInspector);
}

void MainWindow::buildInfoInspectorDock()
{
    // Single QDockWidget hosting ~14 read-only info panels as pages of
    // one InfoInspectorDock widget (combo + QStackedWidget).
    auto* dock = new QDockWidget(tr("Info inspector"), this);
    dock->setObjectName(QStringLiteral("info_inspector_dock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_infoInspector = new app::InfoInspectorDock(nullptr, dock);
    m_infoInspector->setMinimapViewer(m_viewer);
    dock->setWidget(m_infoInspector);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    if (auto* anchor = findChild<QDockWidget*>(QStringLiteral("spawn_dock")))
        tabifyDockWidget(anchor, dock);

    // Mirror the inner-page pointers into the legacy MainWindow members
    // so every existing setItem / setSpell / clear / setZone call site
    // keeps working without per-call-site rewrites.
    m_itemDock        = m_infoInspector->itemDock();
    m_lootDock        = m_infoInspector->lootDock();
    m_questRewardDock = m_infoInspector->questRewardDock();
    m_spellDock       = m_infoInspector->spellDock();
    m_factionDock     = m_infoInspector->factionDock();
    m_areaDock        = m_infoInspector->areaDock();
    m_currencyDock    = m_infoInspector->currencyDock();
    m_playerCondDock  = m_infoInspector->playerCondDock();
    m_goInfoDock      = m_infoInspector->goInfoDock();
    m_npcTextDock     = m_infoInspector->npcTextDock();
    m_trainerDock     = m_infoInspector->trainerDock();
    m_atrScriptDock   = m_infoInspector->atrScriptDock();
    m_zoneSummaryDock = m_infoInspector->zoneSummaryDock();
    m_minimapDiagDock = m_infoInspector->minimapDiagDock();

    // Wire signals that previously lived in buildSpawnDock — they are
    // identical, only the receiver pointer source moved.
    if (m_conditionsDock)
    {
        connect(m_conditionsDock, &app::ConditionsDock::playerConditionSelected,
                this, [this](uint32_t pcId) {
            if (m_infoInspector) m_infoInspector->openPlayerCondition(pcId);
        });
    }
    if (m_lootDock)
    {
        connect(m_lootDock, &app::LootTableDock::itemSelected,
                this, [this](uint32_t id) {
            if (m_infoInspector) m_infoInspector->openItemInfo(id);
        });
    }
    if (m_vendorDock)
    {
        connect(m_vendorDock, &app::VendorInventoryDock::itemSelected,
                this, [this](uint32_t id) {
            if (m_infoInspector) m_infoInspector->openItemInfo(id);
        });
        connect(m_vendorDock, &app::VendorInventoryDock::currencySelected,
                this, [this](uint32_t id) {
            if (m_infoInspector) m_infoInspector->openCurrency(id);
        });
    }
    if (m_atrScriptDock)
    {
        connect(m_atrScriptDock, &app::AreatriggerScriptDock::jumpRequested,
                this, &MainWindow::onJumpRequested);
    }
    if (m_minimapDiagDock)
    {
        connect(m_minimapDiagDock, &app::MinimapDiagnosticsDock::refreshRequested,
                this, &MainWindow::onRefreshMinimapDiagnostics);
        connect(m_minimapDiagDock, &app::MinimapDiagnosticsDock::forceReloadRequested,
                this, &MainWindow::onRefreshMinimapDiagnostics);
    }
}

void MainWindow::buildHandcraftedRoadDock()
{
    auto* dock = new QDockWidget(tr("Handcrafted roads"), this);
    dock->setObjectName(QStringLiteral("handcrafted_road_dock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
                          | Qt::BottomDockWidgetArea);
    m_handcraftedRoadDock = new app::HandcraftedRoadDock(dock);
    dock->setWidget(m_handcraftedRoadDock);
    // Default to hidden; the operator opens via Tools menu / toolbar.
    addDockWidget(Qt::RightDockWidgetArea, dock);
    if (auto* anchor = findChild<QDockWidget*>(QStringLiteral("property_inspector_dock")))
        tabifyDockWidget(anchor, dock);
    dock->hide();

    // Hand the dock the viewer immediately so the polyline push works
    // even before the first map opens.  The DB client wire-up runs
    // after the operator connects (we'll re-call setMySqlClient from
    // finishWorldDbConnect).
    if (m_handcraftedRoadDock && m_viewer)
    {
        qDebug() << "[handcrafted-road] MainWindow::buildHandcraftedRoadDock: "
                    "wiring viewer pointer + (DB client deferred to "
                    "finishWorldDbConnect; mapId deferred to first map switch)";
        m_handcraftedRoadDock->setNavMeshView(m_viewer);
    }
    else
    {
        qDebug() << "[handcrafted-road] MainWindow::buildHandcraftedRoadDock: "
                    "viewer pointer NOT available yet -- dock will sync on "
                    "onToggleHandcraftedRoadDock";
    }

    connect(m_handcraftedRoadDock, &app::HandcraftedRoadDock::addSegmentRequested,
            this, &MainWindow::onHandcraftedAddSegmentRequested);
    connect(m_handcraftedRoadDock, &app::HandcraftedRoadDock::cancelAddSegmentRequested,
            this, &MainWindow::onHandcraftedCancelAddSegment);
    // Update the dock title bar with the live committed-count so the
    // operator sees the segment total even when the dock is tabbed or
    // minimised.  Captures the QDockWidget pointer (parent of the inner
    // CRUD widget); the dock owns its title.
    connect(m_handcraftedRoadDock, &app::HandcraftedRoadDock::segmentCountChanged,
            this, [dock](int count) {
                if (dock)
                    dock->setWindowTitle(QObject::tr("Handcrafted roads (%1)").arg(count));
            });
    if (m_viewer)
    {
        connect(m_viewer, &render::NavMeshView::handcraftedSegmentPlaced,
                this, &MainWindow::onHandcraftedSegmentPlaced);
        // Clicking a road connectivity-diagnostic marker (red gap / amber dead
        // end) selects the offending segment(s) in the dock so the operator can
        // Edit / Delete / reconnect them.
        connect(m_viewer, &render::NavMeshView::roadDiagnosticClicked,
                this, [this](float wx, float wy) {
                    if (m_handcraftedRoadDock)
                        m_handcraftedRoadDock->selectSegmentsNear(wx, wy);
                });
        // Surface running chain count + state transitions in the status
        // bar so the operator sees progress while chaining and a clear
        // "chain finished" hint on exit.
        connect(m_viewer, &render::NavMeshView::handcraftedChainSegmentCountChanged,
                this, [this](int newCount) {
                    if (newCount > 0)
                        statusBar()->showMessage(
                            tr("Chain mode: %1 segment(s) placed.  "
                               "Click to extend, Esc/right-click to finish.")
                            .arg(newCount), 0);
                });
        connect(m_viewer, &render::NavMeshView::handcraftedSegmentPlacementStateChanged,
                this, [this](int newState) {
                    if (newState == 0) // None
                        statusBar()->showMessage(tr("Handcrafted road chain finished."), 4000);
                });
    }
}

void MainWindow::onToggleHandcraftedRoadDock()
{
    auto* dock = findChild<QDockWidget*>(QStringLiteral("handcrafted_road_dock"));
    if (!dock) return;
    if (dock->isVisible())
    {
        dock->hide();
    }
    else
    {
        dock->setFloating(false);
        dock->show();
        dock->raise();
        // Make sure the dock's repo + viewer references are up to date
        // each time the operator opens it — useful when the DB was
        // connected after the dock was constructed.
        if (m_handcraftedRoadDock)
        {
            m_handcraftedRoadDock->setMySqlClient(m_worldDb.get());
            m_handcraftedRoadDock->setNavMeshView(m_viewer);
            if (m_currentMapId.has_value())
                m_handcraftedRoadDock->setCurrentMapId(*m_currentMapId);
        }
    }
}

void MainWindow::onHandcraftedAddSegmentRequested()
{
    if (!m_viewer)
        return;
    m_viewer->enterSegmentPlacementMode();
    statusBar()->showMessage(
        tr("Chain mode: click first point.  Subsequent clicks extend the "
           "chain.  Esc/right-click to finish."),
        8000);
}

void MainWindow::onHandcraftedCancelAddSegment()
{
    if (m_viewer)
        m_viewer->cancelSegmentPlacement();
    statusBar()->showMessage(tr("Handcrafted road placement cancelled."), 3000);
}

void MainWindow::onHandcraftedSegmentPlaced(float fromX, float fromY, float toX, float toY)
{
    if (!m_handcraftedRoadDock)
        return;
    m_handcraftedRoadDock->handleSegmentPlaced(fromX, fromY, toX, toY);
}

void MainWindow::pushAreatriggersToViewer()
{
    if (!m_viewer || !m_areatriggerModel) return;
    auto const& current = m_areatriggerModel->current();
    auto const& changes = m_areatriggerModel->changes();
    std::vector<render::Areatrigger> visible;
    visible.reserve(current.size());
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (i < changes.size() && changes[i].kind == db::AreatriggerChangeKind::Delete)
            continue;
        visible.push_back(current[i]);
    }
    if (m_viewer3d) m_viewer3d->setAreatriggers(visible);
    m_viewer->setAreatriggers(std::move(visible));
    if (m_areatriggerDock)
        m_areatriggerDock->setPendingCount(m_areatriggerModel->pendingCount());
}

void MainWindow::refreshAreatriggerSpawnIdReservation()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        m_nextAreatriggerSpawnId = 0;
        return;
    }
    db::QueryResult res;
    auto const err = m_worldDb->query("SELECT COALESCE(MAX(SpawnId), 0) FROM areatrigger", res);
    if (!err.ok() || res.rowCount() == 0)
    {
        m_nextAreatriggerSpawnId = 0;
        return;
    }
    m_nextAreatriggerSpawnId = res.asUInt64(0, 0).value_or(0) + 1;
}

void MainWindow::onAreatriggerEdited(render::Areatrigger const& proposed)
{
    if (!m_areatriggerModel || m_selectedAreatriggerIndex < 0)
        return;
    int const idx = m_selectedAreatriggerIndex;
    bool const changed = m_undo->recordIf(m_areatriggerModel.get(),
        tr("Edit areatrigger"), [&]() {
        return m_areatriggerModel->replaceRow(idx, proposed);
    });
    if (changed)
    {
        pushAreatriggersToViewer();
        if (m_areatriggerDock)
            m_areatriggerDock->setPendingCount(m_areatriggerModel->pendingCount());
    }
}

void MainWindow::onDeleteSelectedAreatrigger()
{
    if (!m_areatriggerModel || m_selectedAreatriggerIndex < 0)
        return;
    int const idx = m_selectedAreatriggerIndex;
    bool const changed = m_undo->recordIf(m_areatriggerModel.get(),
        tr("Delete areatrigger"), [&]() {
        return m_areatriggerModel->removeRow(idx);
    });
    if (changed)
    {
        m_selectedAreatriggerIndex = -1;
        if (m_areatriggerDock)
        {
            m_areatriggerDock->clear();
            m_areatriggerDock->setPendingCount(m_areatriggerModel->pendingCount());
        }
        pushAreatriggersToViewer();
    }
}

void MainWindow::onRevertAreatriggers()
{
    if (!m_areatriggerModel) return;
    m_areatriggerModel->revertAll();
    m_selectedAreatriggerIndex = -1;
    if (m_areatriggerDock)
    {
        m_areatriggerDock->clear();
        m_areatriggerDock->setPendingCount(0);
    }
    pushAreatriggersToViewer();
    statusBar()->showMessage(tr("Areatrigger edits reverted"), 2000);
}

void MainWindow::onCommitAreatriggers()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before committing."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map before committing areatrigger edits."));
        return;
    }
    if (!m_areatriggerModel || m_areatriggerModel->pendingCount() == 0)
    {
        statusBar()->showMessage(tr("Nothing to commit."), 2000);
        return;
    }

    app::AreatriggerCommitDialog dlg(m_worldDb.get(), *m_areatriggerModel, *m_currentMapId, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto committed = dlg.committedRows();
    m_areatriggerModel->acceptCommit(std::move(committed));
    m_selectedAreatriggerIndex = -1;
    if (m_areatriggerDock)
    {
        m_areatriggerDock->clear();
        m_areatriggerDock->setPendingCount(0);
    }
    pushAreatriggersToViewer();
    refreshAreatriggerSpawnIdReservation();
    statusBar()->showMessage(tr("Areatrigger commit applied."), 3000);
}

void MainWindow::onNewAreatriggerFromCreateProps()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before placing new areatriggers."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map first - new areatriggers are placed by clicking on it."));
        return;
    }
    if (m_nextAreatriggerSpawnId == 0)
        refreshAreatriggerSpawnIdReservation();

    app::AreatriggerCreatePropsPicker picker(m_worldDb.get(), this);
    if (picker.exec() != QDialog::Accepted)
        return;
    app::PickedAreatriggerProps const picked = picker.picked();
    if (!picked.valid)
        return;

    auto tpl = std::make_unique<AreatriggerSpawnTemplate>();
    tpl->createPropsId = picked.id;
    tpl->isCustom      = picked.isCustom;
    tpl->shape         = picked.shape;
    for (int k = 0; k < 8; ++k)
        tpl->shapeData[k] = picked.shapeData[k];
    tpl->scriptName    = picked.scriptName;
    m_pickedAreatriggerProps = std::move(tpl);

    setPlacementKind(PlacementKind::Areatrigger);
    if (m_viewer) m_viewer->setPlacementMode(true); if (m_viewer3d) m_viewer3d->setPlacementMode(true);

    statusBar()->showMessage(
        tr("Areatrigger placement armed: CreatePropsId=%1 IsCustom=%2 shape=%3 - click on the map to drop. Esc to exit.")
          .arg(picked.id).arg(int(picked.isCustom)).arg(int(picked.shape)),
        0);
}

void MainWindow::pushGraveyardsToViewer()
{
    if (!m_viewer || !m_graveyardModel) return;
    auto const& current = m_graveyardModel->current();
    auto const& changes = m_graveyardModel->changes();
    std::vector<render::Graveyard> visible;
    visible.reserve(current.size());
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (i < changes.size() && changes[i].kind == db::GraveyardChangeKind::Delete)
            continue;
        visible.push_back(current[i]);
    }
    if (m_viewer3d) m_viewer3d->setGraveyards(visible);
    m_viewer->setGraveyards(std::move(visible));
    if (m_graveyardDock)
        m_graveyardDock->setPendingCount(m_graveyardModel->pendingCount());
}

float MainWindow::snapToGround(uint32_t mapId,
                               float worldX, float worldY,
                               float probeZ, float fallbackZ) const
{
    float best = fallbackZ;
    bool  found = false;
    // ADT terrain.
    if (m_mapTileCache && !m_mapTileCache->mapsDir().empty())
    {
        float const adt = m_mapTileCache->heightAt(mapId, worldX, worldY);
        if (adt > io::ADT_INVALID_HEIGHT)
        {
            best  = adt;
            found = true;
        }
    }
    // WMO floor below the probe Z (highest floor wins so upper floors
    // don't drop us through the building).  Composes with ADT - WMO
    // floors above terrain take precedence inside a building, while
    // outside buildings the probe returns INVALID and ADT wins.
    if (m_vmapProbe.ok())
    {
        float const wmo = m_vmapProbe.floorBelow(worldX, worldY, probeZ);
        if (wmo > io::VmapHeightProbe::INVALID)
        {
            if (!found || wmo > best)
            {
                best  = wmo;
                found = true;
            }
        }
    }
    return best;
}

void MainWindow::refreshGraveyardIdReservation()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        m_nextGraveyardId = 0;
        return;
    }
    db::QueryResult res;
    auto const err = m_worldDb->query("SELECT COALESCE(MAX(ID), 0) FROM world_safe_locs", res);
    if (!err.ok() || res.rowCount() == 0)
    {
        m_nextGraveyardId = 0;
        return;
    }
    m_nextGraveyardId = res.asUInt64(0, 0).value_or(0) + 1;
}

void MainWindow::onGraveyardEdited(render::Graveyard const& proposed)
{
    if (!m_graveyardModel || m_selectedGraveyardIndex < 0)
        return;
    int const idx = m_selectedGraveyardIndex;
    bool const changed = m_undo->recordIf(m_graveyardModel.get(),
        tr("Edit graveyard"), [&]() {
        return m_graveyardModel->replaceRow(idx, proposed);
    });
    if (changed)
    {
        pushGraveyardsToViewer();
        if (m_graveyardDock)
            m_graveyardDock->setPendingCount(m_graveyardModel->pendingCount());
    }
}

void MainWindow::onDeleteSelectedGraveyard()
{
    if (!m_graveyardModel || m_selectedGraveyardIndex < 0)
        return;
    int const idx = m_selectedGraveyardIndex;
    bool const changed = m_undo->recordIf(m_graveyardModel.get(),
        tr("Delete graveyard"), [&]() {
        return m_graveyardModel->removeRow(idx);
    });
    if (changed)
    {
        m_selectedGraveyardIndex = -1;
        if (m_graveyardDock)
        {
            m_graveyardDock->clear();
            m_graveyardDock->setPendingCount(m_graveyardModel->pendingCount());
        }
        pushGraveyardsToViewer();
    }
}

void MainWindow::onRevertGraveyards()
{
    if (!m_graveyardModel) return;
    m_graveyardModel->revertAll();
    m_selectedGraveyardIndex = -1;
    if (m_graveyardDock)
    {
        m_graveyardDock->clear();
        m_graveyardDock->setPendingCount(0);
    }
    pushGraveyardsToViewer();
    statusBar()->showMessage(tr("Graveyard edits reverted"), 2000);
}

void MainWindow::onCommitGraveyards()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before committing."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map before committing graveyard edits."));
        return;
    }
    if (!m_graveyardModel || m_graveyardModel->pendingCount() == 0)
    {
        statusBar()->showMessage(tr("Nothing to commit."), 2000);
        return;
    }

    app::GraveyardCommitDialog dlg(m_worldDb.get(), *m_graveyardModel, *m_currentMapId, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto committed = dlg.committedRows();
    m_graveyardModel->acceptCommit(std::move(committed));
    m_selectedGraveyardIndex = -1;
    if (m_graveyardDock)
    {
        m_graveyardDock->clear();
        m_graveyardDock->setPendingCount(0);
    }
    pushGraveyardsToViewer();
    refreshGraveyardIdReservation();
    statusBar()->showMessage(tr("Graveyard commit applied."), 3000);
}

void MainWindow::onNewGraveyard()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before placing new graveyards."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map first - new graveyards are placed by clicking on it."));
        return;
    }
    if (m_nextGraveyardId == 0)
        refreshGraveyardIdReservation();
    if (m_nextGraveyardId == 0)
    {
        statusBar()->showMessage(
            tr("Could not reserve a graveyard ID - is the DB connected?"), 4000);
        return;
    }
    setPlacementKind(PlacementKind::Graveyard);
    if (m_viewer) m_viewer->setPlacementMode(true); if (m_viewer3d) m_viewer3d->setPlacementMode(true);
    statusBar()->showMessage(
        tr("Graveyard placement armed: next ID=%1 - click on the map to drop. Esc to exit.")
          .arg(m_nextGraveyardId), 0);
}

void MainWindow::pushPathsToViewer()
{
    if (!m_viewer || !m_waypointModel) return;
    auto const& current = m_waypointModel->current();
    auto const& changes = m_waypointModel->changes();
    std::vector<render::Path> visible;
    visible.reserve(current.size() + (m_drawingPath ? 1 : 0));
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (i < changes.size() && changes[i].kind == db::PathChangeKind::Delete)
            continue;
        visible.push_back(current[i]);
    }
    if (m_drawingPath && m_drawingPathBuf.nodes.size() >= 2)
        visible.push_back(m_drawingPathBuf);
    if (m_viewer3d) m_viewer3d->setPaths(visible);
    m_viewer->setPaths(std::move(visible));
    if (m_pathDock)
        m_pathDock->setPendingCount(m_waypointModel->pendingCount());
}

void MainWindow::reloadPathsForMap(uint32_t mapId)
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;

    // Paths referenced by creatures on this map.  HANDOFF section 8.4
    // said the FK lives in creature.currentwaypoint, but on this server
    // it's actually creature_addon.PathId - currentwaypoint is always 0
    // (the waypoint movement system uses the addon table per modern TC).
    // We JOIN through creature_addon to find which paths are in use on
    // a given map.
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT wp.PathId, wp.MoveType, wp.Flags, "
        "       COALESCE(wp.Velocity, 0), COALESCE(wp.Comment, '') "
        "FROM waypoint_path wp "
        "JOIN (SELECT DISTINCT ca.PathId AS pid "
        "      FROM creature_addon ca "
        "      JOIN creature c ON c.guid = ca.guid "
        "      WHERE c.map = %u AND ca.PathId > 0) used "
        "ON used.pid = wp.PathId", mapId);
    db::QueryResult pathRes;
    db::QueryError err = m_worldDb->query(sql, pathRes);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("path query failed: %1")
            .arg(QString::fromStdString(err.message)), 4000);
        m_viewer->setPaths({});
        return;
    }

    std::vector<render::Path> paths;
    paths.reserve(pathRes.rowCount());
    std::vector<uint32_t> pathIds;
    pathIds.reserve(pathRes.rowCount());
    for (size_t r = 0; r < pathRes.rowCount(); ++r)
    {
        render::Path p;
        p.pathId   = static_cast<uint32_t>(pathRes.asUInt64(r, 0).value_or(0));
        p.moveType = static_cast<uint8_t> (pathRes.asUInt64(r, 1).value_or(0));
        p.flags    = static_cast<uint8_t> (pathRes.asUInt64(r, 2).value_or(0));
        p.velocity = static_cast<float>(pathRes.asDouble(r, 3).value_or(0.0));
        p.comment  = QString::fromStdString(pathRes.cell(r, 4));
        pathIds.push_back(p.pathId);
        paths.push_back(std::move(p));
    }

    if (paths.empty())
    {
        m_viewer->setPaths({});
        statusBar()->showMessage(tr("paths=0 (no creature_addon.PathId entries on this map)"), 3000);
        return;
    }

    // Build an IN-list for the nodes query.  9k path ids fits comfortably
    // in one query - if this ever exceeds MAX_ALLOWED_PACKET we'd chunk.
    std::string in_list;
    in_list.reserve(pathIds.size() * 12);
    for (size_t i = 0; i < pathIds.size(); ++i)
    {
        if (i) in_list += ',';
        in_list += std::to_string(pathIds[i]);
    }
    std::string nodeSql =
        "SELECT PathId, NodeId, PositionX, PositionY, PositionZ, "
        "       COALESCE(Orientation, 0), Delay "
        "FROM waypoint_path_node WHERE PathId IN (" + in_list + ") "
        "ORDER BY PathId, NodeId";
    db::QueryResult nodeRes;
    err = m_worldDb->query(nodeSql, nodeRes);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("node query failed: %1")
            .arg(QString::fromStdString(err.message)), 4000);
        m_viewer->setPaths({});
        return;
    }

    // Bucket nodes by PathId.  Same row order as our paths vector
    // because both are ORDER BY PathId.
    size_t pathCursor = 0;
    for (size_t r = 0; r < nodeRes.rowCount(); ++r)
    {
        uint32_t const pid = static_cast<uint32_t>(nodeRes.asUInt64(r, 0).value_or(0));
        while (pathCursor < paths.size() && paths[pathCursor].pathId != pid)
            ++pathCursor;
        if (pathCursor >= paths.size())
            break;
        render::PathNode n;
        n.nodeId      = int(nodeRes.asUInt64(r, 1).value_or(0));
        n.x           = static_cast<float>(nodeRes.asDouble(r, 2).value_or(0.0));
        n.y           = static_cast<float>(nodeRes.asDouble(r, 3).value_or(0.0));
        n.z           = static_cast<float>(nodeRes.asDouble(r, 4).value_or(0.0));
        n.orientation = static_cast<float>(nodeRes.asDouble(r, 5).value_or(0.0));
        n.delay       = static_cast<uint32_t>(nodeRes.asUInt64(r, 6).value_or(0));
        paths[pathCursor].nodes.push_back(std::move(n));
    }

    size_t nodeCount = 0;
    for (auto const& p : paths) nodeCount += p.nodes.size();
    statusBar()->showMessage(tr("paths=%1  nodes=%2").arg(paths.size()).arg(nodeCount), 3000);
    m_waypointModel->setBaseline(std::move(paths));
    m_selectedPathIndex = -1;
    if (m_pathDock) { m_pathDock->clear(); m_pathDock->setPendingCount(0); }
    pushPathsToViewer();

    // Refresh PathId reservation on every map (re-checks MAX after edits
    // to other maps would have bumped it).
    db::QueryResult maxRes;
    if (m_worldDb->query("SELECT COALESCE(MAX(PathId), 0)+1 FROM waypoint_path", maxRes).ok()
        && maxRes.rowCount() > 0)
    {
        m_nextPathId = static_cast<uint32_t>(maxRes.asUInt64(0, 0).value_or(0));
    }
}

void MainWindow::onNewPath()
{
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"), tr("Open a map first."));
        return;
    }
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("Connect to the world DB before drawing paths."));
        return;
    }
    if (m_nextPathId == 0)
    {
        db::QueryResult maxRes;
        if (m_worldDb->query("SELECT COALESCE(MAX(PathId), 0)+1 FROM waypoint_path", maxRes).ok()
            && maxRes.rowCount() > 0)
            m_nextPathId = static_cast<uint32_t>(maxRes.asUInt64(0, 0).value_or(0));
    }
    if (m_nextPathId == 0)
    {
        QMessageBox::warning(this, tr("PathId reservation failed"),
            tr("Could not query MAX(PathId)+1."));
        return;
    }

    m_drawingPath = true;
    m_drawingPathBuf = render::Path{};
    m_drawingPathBuf.pathId   = m_nextPathId;
    m_drawingPathBuf.moveType = 0;
    m_drawingPathBuf.flags    = 0;
    m_drawingPathBuf.velocity = 0.0f;
    m_drawingPathBuf.comment  = tr("(new path)");

    setPlacementKind(PlacementKind::PathDraw);
    if (m_viewer)
        m_viewer->setPlacementMode(true); if (m_viewer3d) m_viewer3d->setPlacementMode(true);
    statusBar()->showMessage(
        tr("Drawing new path id=%1: click to add nodes, Enter to finish, Esc to cancel.")
            .arg(m_nextPathId), 0);
}

void MainWindow::onFinishPath()
{
    if (!m_drawingPath) return;
    if (m_drawingPathBuf.nodes.size() < 2)
    {
        statusBar()->showMessage(tr("Need at least 2 nodes; press Esc to cancel instead."), 3000);
        return;
    }
    m_undo->recordOn(m_waypointModel.get(),
        tr("Add new path"), [&]() {
        m_waypointModel->addNewPath(m_drawingPathBuf);
    });
    ++m_nextPathId;
    m_drawingPath = false;
    m_drawingPathBuf = render::Path{};
    setPlacementKind(PlacementKind::None);
    if (m_viewer) m_viewer->setPlacementMode(false); if (m_viewer3d) m_viewer3d->setPlacementMode(false);
    pushPathsToViewer();
    statusBar()->showMessage(tr("Path finished - pending commit"), 3000);
}

void MainWindow::onCancelPath()
{
    if (!m_drawingPath)
    {
        // Escape doubles as the exit for the one-shot placement modes
        // (spawn / areatrigger / graveyard / annotation / road draw) --
        // every arming message has always promised "Esc to exit", but only
        // path drawing actually honoured it until now.
        cancelActivePlacement();
        return;
    }
    m_drawingPath = false;
    m_drawingPathBuf = render::Path{};
    setPlacementKind(PlacementKind::None);
    m_autoRouteNextClick = false;
    if (m_viewer) m_viewer->setPlacementMode(false); if (m_viewer3d) m_viewer3d->setPlacementMode(false);
    pushPathsToViewer();
    statusBar()->showMessage(tr("Path cancelled"), 2000);
}

void MainWindow::onArmAutoRoute()
{
    bool const inPath = m_drawingPath && !m_drawingPathBuf.nodes.empty();
    bool const inRoad = m_drawingRoad && m_hasRoadAnchor;
    if (!inPath && !inRoad)
    {
        QMessageBox::information(this, tr("No anchor"),
            tr("Auto-route needs an anchor.  Start a path (Ctrl+Shift+N) "
               "or enter road draw mode (Ctrl+Alt+R), drop one waypoint, "
               "then arm auto-route."));
        return;
    }
    m_autoRouteNextClick = true;
    if (inPath)
    {
        statusBar()->showMessage(
            tr("Auto-route armed: click anywhere on the navmesh -- Detour "
               "will insert every waypoint from path node %1 to the click.")
                .arg(int(m_drawingPathBuf.nodes.size())), 0);
    }
    else
    {
        statusBar()->showMessage(
            tr("Auto-route armed: click anywhere on the navmesh -- Detour "
               "will drop a Road annotation at every waypoint from the last "
               "anchor to the click."), 0);
    }
}

void MainWindow::onStartRoadDraw()
{
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map first.  Road waypoints are placed by clicking on it."));
        return;
    }
    if (m_drawingPath)
    {
        QMessageBox::information(this, tr("Path in progress"),
            tr("Finish or cancel the current path first (Path -> Finish/Cancel) "
               "before entering road draw mode."));
        return;
    }
    m_drawingRoad        = true;
    m_hasRoadAnchor      = false;
    m_autoRouteNextClick = false;
    setPlacementKind(PlacementKind::RoadDraw);
    if (m_viewer) m_viewer->setPlacementMode(true); if (m_viewer3d) m_viewer3d->setPlacementMode(true);
    statusBar()->showMessage(
        tr("Road draw mode: each click drops a Road annotation (snap-to-"
           "ground).  Ctrl+R arms auto-route for the next click.  "
           "Exit via Path -> Exit road draw mode."), 0);
}

void MainWindow::onFinishRoadDraw()
{
    if (!m_drawingRoad) return;
    m_drawingRoad        = false;
    m_hasRoadAnchor      = false;
    m_autoRouteNextClick = false;
    setPlacementKind(PlacementKind::None);
    if (m_viewer) m_viewer->setPlacementMode(false); if (m_viewer3d) m_viewer3d->setPlacementMode(false);
    statusBar()->showMessage(
        tr("Road draw mode exited.  Don't forget to Commit annotations to "
           "persist your new Road waypoints to the DB."), 4000);
}

void MainWindow::onPathClicked(int pathIndex)
{
    if (!m_viewer || pathIndex < 0 || pathIndex >= int(m_viewer->paths().size()))
        return;
    render::Path const& display = m_viewer->paths()[pathIndex];
    int const modelIdx = m_waypointModel ? m_waypointModel->indexForPathId(display.pathId) : -1;
    m_selectedPathIndex = modelIdx;
    if (modelIdx >= 0 && m_pathDock)
    {
        m_pathDock->setPath(modelIdx, m_waypointModel->current()[modelIdx]);
        m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        if (m_propertyInspector)
            m_propertyInspector->showTab(app::PropertyInspectorDock::Tab::Path);
    }
}

void MainWindow::onPathEdited(render::Path const& proposed)
{
    if (!m_waypointModel || m_selectedPathIndex < 0) return;
    int const idx = m_selectedPathIndex;
    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        tr("Edit path"), [&]() {
        return m_waypointModel->replacePath(idx, proposed);
    });
    if (changed)
    {
        pushPathsToViewer();
        if (m_pathDock)
            m_pathDock->setPendingCount(m_waypointModel->pendingCount());
    }
}

// Resolve a viewer-path index to the WaypointModel current() index by
// matching pathId.  Returns -1 if the viewer index is the in-progress
// drawing buffer or no match exists.
static int resolveViewerPathToModel(render::NavMeshView const* viewer,
                                    db::WaypointModel const* model,
                                    int viewerPathIdx)
{
    if (!viewer || !model) return -1;
    if (viewerPathIdx < 0 || viewerPathIdx >= int(viewer->paths().size()))
        return -1;
    uint32_t const pathId = viewer->paths()[viewerPathIdx].pathId;
    return model->indexForPathId(pathId);
}

void MainWindow::onPathNodeClicked(int viewerPathIdx, int nodeIndex)
{
    int const modelIdx = resolveViewerPathToModel(m_viewer, m_waypointModel.get(),
                                                  viewerPathIdx);
    if (modelIdx < 0) return;
    m_selectedPathIndex = modelIdx;
    if (m_pathDock)
    {
        m_pathDock->setPath(modelIdx, m_waypointModel->current()[modelIdx]);
        m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        if (m_propertyInspector)
            m_propertyInspector->showTab(app::PropertyInspectorDock::Tab::Path);
    }
    auto const& nodes = m_waypointModel->current()[modelIdx].nodes;
    if (nodeIndex >= 0 && nodeIndex < int(nodes.size()))
    {
        statusBar()->showMessage(tr("Path %1 node %2/%3 at (%4, %5, %6)")
            .arg(m_waypointModel->current()[modelIdx].pathId)
            .arg(nodeIndex + 1)
            .arg(nodes.size())
            .arg(nodes[nodeIndex].x, 0, 'f', 1)
            .arg(nodes[nodeIndex].y, 0, 'f', 1)
            .arg(nodes[nodeIndex].z, 0, 'f', 1), 4000);
    }
}

void MainWindow::onPathNodeMoved(int viewerPathIdx, int nodeIndex,
                                 float worldX, float worldY)
{
    int const modelIdx = resolveViewerPathToModel(m_viewer, m_waypointModel.get(),
                                                  viewerPathIdx);
    if (modelIdx < 0) return;
    render::Path proposed = m_waypointModel->current()[modelIdx];
    if (nodeIndex < 0 || nodeIndex >= int(proposed.nodes.size()))
        return;
    proposed.nodes[nodeIndex].x = worldX;
    proposed.nodes[nodeIndex].y = worldY;
    // Snap-to-ground if the cache is available so dropped node Z lands
    // on the navmesh-walkable surface, matching how spawn drag works.
    if (m_mapTileCache && m_currentMapId.has_value())
    {
        float const groundZ = m_mapTileCache->heightAt(*m_currentMapId, worldX, worldY);
        if (groundZ > -1.0e5f)
            proposed.nodes[nodeIndex].z = groundZ;
    }
    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        tr("Move path node"), [&]() {
        return m_waypointModel->replacePath(modelIdx, proposed);
    });
    if (changed)
    {
        pushPathsToViewer();
        m_selectedPathIndex = modelIdx;
        if (m_pathDock)
        {
            m_pathDock->setPath(modelIdx, proposed);
            m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        }
    }
}

void MainWindow::onPathNodeMoved3D(int viewerPathIdx, int nodeIndex,
                                   float worldX, float worldY, float worldZ)
{
    int const modelIdx = resolveViewerPathToModel(m_viewer, m_waypointModel.get(),
                                                  viewerPathIdx);
    if (modelIdx < 0) return;
    render::Path proposed = m_waypointModel->current()[modelIdx];
    if (nodeIndex < 0 || nodeIndex >= int(proposed.nodes.size()))
        return;
    proposed.nodes[nodeIndex].x = worldX;
    proposed.nodes[nodeIndex].y = worldY;
    // Ground-snap ONLY when the .map ground is close to the drag plane.
    // In multi-floor dungeons heightAt(x, y) can return a different storey
    // (or garbage for WMO interiors); the drag-plane Z is then the truth.
    float z = worldZ;
    if (m_mapTileCache && m_currentMapId.has_value())
    {
        float const groundZ = m_mapTileCache->heightAt(*m_currentMapId, worldX, worldY);
        if (groundZ > -1.0e5f && std::abs(groundZ - worldZ) < 10.0f)
            z = groundZ;
    }
    proposed.nodes[nodeIndex].z = z;
    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        tr("Move path node"), [&]() {
        return m_waypointModel->replacePath(modelIdx, proposed);
    });
    if (changed)
    {
        pushPathsToViewer();
        m_selectedPathIndex = modelIdx;
        if (m_pathDock)
        {
            m_pathDock->setPath(modelIdx, proposed);
            m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        }
    }
}

void MainWindow::onPathNodeContextMenu(int viewerPathIdx, int nodeIndex,
                                       QPoint globalPos)
{
    int const modelIdx = resolveViewerPathToModel(m_viewer, m_waypointModel.get(),
                                                  viewerPathIdx);
    if (modelIdx < 0) return;
    render::Path const& path = m_waypointModel->current()[modelIdx];
    if (nodeIndex < 0 || nodeIndex >= int(path.nodes.size())) return;

    QMenu menu(this);
    QAction* const insertBefore = menu.addAction(tr("Insert node BEFORE this"));
    QAction* const insertAfter  = menu.addAction(tr("Insert node AFTER this"));
    menu.addSeparator();
    QAction* const deleteNode   = menu.addAction(tr("Delete this node"));
    if (path.nodes.size() <= 2)
        deleteNode->setEnabled(false); // path needs >=2 nodes to render

    QAction* const chosen = menu.exec(globalPos);
    if (!chosen) return;

    render::Path proposed = path;
    render::PathNode const& ref = path.nodes[nodeIndex];
    if (chosen == insertBefore || chosen == insertAfter)
    {
        // New node is placed mid-way between the reference node and its
        // neighbour (so the operator gets a sensible default; they can
        // then drag it).  If there's no neighbour on that side, offset
        // 5 yards along the path's other neighbour direction.
        render::PathNode neu = ref;
        if (chosen == insertBefore)
        {
            if (nodeIndex > 0)
            {
                render::PathNode const& prev = path.nodes[nodeIndex - 1];
                neu.x = (prev.x + ref.x) * 0.5f;
                neu.y = (prev.y + ref.y) * 0.5f;
                neu.z = (prev.z + ref.z) * 0.5f;
            }
            else
            {
                // First node - offset opposite from node[1] (if any).
                if (path.nodes.size() > 1)
                {
                    float const dx = ref.x - path.nodes[1].x;
                    float const dy = ref.y - path.nodes[1].y;
                    float const len = std::sqrt(dx*dx + dy*dy);
                    if (len > 0.1f)
                    {
                        neu.x = ref.x + 5.0f * dx / len;
                        neu.y = ref.y + 5.0f * dy / len;
                    }
                }
            }
            proposed.nodes.insert(proposed.nodes.begin() + nodeIndex, neu);
        }
        else // insertAfter
        {
            if (nodeIndex + 1 < int(path.nodes.size()))
            {
                render::PathNode const& nxt = path.nodes[nodeIndex + 1];
                neu.x = (nxt.x + ref.x) * 0.5f;
                neu.y = (nxt.y + ref.y) * 0.5f;
                neu.z = (nxt.z + ref.z) * 0.5f;
            }
            else
            {
                // Last node - offset opposite from node[size-2] (if any).
                if (path.nodes.size() > 1)
                {
                    render::PathNode const& prev = path.nodes[path.nodes.size() - 2];
                    float const dx = ref.x - prev.x;
                    float const dy = ref.y - prev.y;
                    float const len = std::sqrt(dx*dx + dy*dy);
                    if (len > 0.1f)
                    {
                        neu.x = ref.x + 5.0f * dx / len;
                        neu.y = ref.y + 5.0f * dy / len;
                    }
                }
            }
            proposed.nodes.insert(proposed.nodes.begin() + nodeIndex + 1, neu);
        }
        // Renumber nodeIds 1..N to match the schema's contiguous index.
        for (size_t i = 0; i < proposed.nodes.size(); ++i)
            proposed.nodes[i].nodeId = static_cast<int>(i);
    }
    else if (chosen == deleteNode)
    {
        proposed.nodes.erase(proposed.nodes.begin() + nodeIndex);
        for (size_t i = 0; i < proposed.nodes.size(); ++i)
            proposed.nodes[i].nodeId = static_cast<int>(i);
    }

    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        chosen == deleteNode ? tr("Delete path node") : tr("Insert path node"),
        [&]() {
        return m_waypointModel->replacePath(modelIdx, proposed);
    });
    if (changed)
    {
        m_selectedPathIndex = modelIdx;
        pushPathsToViewer();
        if (m_pathDock)
        {
            m_pathDock->setPath(modelIdx, proposed);
            m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        }
    }
}

void MainWindow::onPathSegmentContextMenu(int viewerPathIdx, int afterNodeIndex,
                                          float worldX, float worldY,
                                          QPoint globalPos)
{
    int const modelIdx = resolveViewerPathToModel(m_viewer, m_waypointModel.get(),
                                                  viewerPathIdx);
    if (modelIdx < 0) return;
    render::Path const& path = m_waypointModel->current()[modelIdx];
    if (afterNodeIndex < 0 || afterNodeIndex + 1 >= int(path.nodes.size()))
        return;

    QMenu menu(this);
    QAction* const insertHere = menu.addAction(tr("Insert node here"));
    QAction* const chosen = menu.exec(globalPos);
    if (chosen != insertHere) return;

    render::Path proposed = path;
    render::PathNode neu = path.nodes[afterNodeIndex];
    neu.x = worldX;
    neu.y = worldY;
    if (m_mapTileCache && m_currentMapId.has_value())
    {
        float const groundZ = m_mapTileCache->heightAt(*m_currentMapId, worldX, worldY);
        if (groundZ > -1.0e5f)
            neu.z = groundZ;
    }
    proposed.nodes.insert(proposed.nodes.begin() + afterNodeIndex + 1, neu);
    for (size_t i = 0; i < proposed.nodes.size(); ++i)
        proposed.nodes[i].nodeId = static_cast<int>(i);

    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        tr("Insert path node on segment"), [&]() {
        return m_waypointModel->replacePath(modelIdx, proposed);
    });
    if (changed)
    {
        m_selectedPathIndex = modelIdx;
        pushPathsToViewer();
        if (m_pathDock)
        {
            m_pathDock->setPath(modelIdx, proposed);
            m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        }
    }
}

void MainWindow::onPathSegmentContextMenu3D(int viewerPathIdx, int afterNodeIndex,
                                            float worldX, float worldY, float worldZ,
                                            QPoint globalPos)
{
    int const modelIdx = resolveViewerPathToModel(m_viewer, m_waypointModel.get(),
                                                  viewerPathIdx);
    if (modelIdx < 0) return;
    render::Path const& path = m_waypointModel->current()[modelIdx];
    if (afterNodeIndex < 0 || afterNodeIndex + 1 >= int(path.nodes.size()))
        return;

    QMenu menu(this);
    QAction* const insertHere = menu.addAction(tr("Insert node here"));
    QAction* const chosen = menu.exec(globalPos);
    if (chosen != insertHere) return;

    render::Path proposed = path;
    render::PathNode neu = path.nodes[afterNodeIndex];
    neu.x = worldX;
    neu.y = worldY;
    // Same multi-floor guard as onPathNodeMoved3D: the segment-lerped Z is
    // authoritative unless the ground is genuinely nearby.
    neu.z = worldZ;
    if (m_mapTileCache && m_currentMapId.has_value())
    {
        float const groundZ = m_mapTileCache->heightAt(*m_currentMapId, worldX, worldY);
        if (groundZ > -1.0e5f && std::abs(groundZ - worldZ) < 10.0f)
            neu.z = groundZ;
    }
    proposed.nodes.insert(proposed.nodes.begin() + afterNodeIndex + 1, neu);
    for (size_t i = 0; i < proposed.nodes.size(); ++i)
        proposed.nodes[i].nodeId = static_cast<int>(i);

    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        tr("Insert path node on segment"), [&]() {
        return m_waypointModel->replacePath(modelIdx, proposed);
    });
    if (changed)
    {
        m_selectedPathIndex = modelIdx;
        pushPathsToViewer();
        if (m_pathDock)
        {
            m_pathDock->setPath(modelIdx, proposed);
            m_pathDock->setPendingCount(m_waypointModel->pendingCount());
        }
    }
}

void MainWindow::onStartPathProbe()
{
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Open a map first."), 3000);
        return;
    }
    m_probeHaveStart = false;
    setPlacementKind(PlacementKind::PathProbe);
    if (m_viewer)   m_viewer->setPlacementMode(true);
    if (m_viewer3d) m_viewer3d->setPlacementMode(true);
    statusBar()->showMessage(
        tr("Path probe armed (bot budget: 74 polys / 1024 nodes) - click the "
           "START point, then the destination. Esc exits."), 0);
}

void MainWindow::onClearPathProbe()
{
    if (m_viewer3d)
        m_viewer3d->clearPathProbe();
    m_probeHaveStart = false;
    statusBar()->showMessage(tr("Path-probe overlay cleared."), 3000);
}

// ---------------------------------------------------------------------------
// Off-mesh connection authoring: two clicks -> one line appended to the
// offmesh.txt that mmaps_generator auto-loads from its input directory.
// The magenta PENDING arc reminds the operator a regen is still needed.
// ---------------------------------------------------------------------------

QString MainWindow::offmeshFilePath() const
{
    QSettings s;
    return s.value(QStringLiteral("paths/offmesh_txt"),
                   QStringLiteral("M:/WorldofWarcraft/offmesh.txt")).toString();
}

void MainWindow::onSetOffmeshFile()
{
    QString const initial = offmeshFilePath();
    QString const f = QFileDialog::getSaveFileName(this,
        tr("Pick offmesh.txt (mmaps_generator input)"), initial,
        tr("Text files (*.txt);;All files (*.*)"),
        nullptr, QFileDialog::DontConfirmOverwrite);
    if (f.isEmpty())
        return;
    QSettings s;
    s.setValue(QStringLiteral("paths/offmesh_txt"), f);
    statusBar()->showMessage(
        tr("Off-mesh connections will be appended to %1").arg(f), 5000);
}

void MainWindow::onStartOffmeshDraw()
{
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Open a map first."), 3000);
        return;
    }
    m_offmeshHaveStart = false;
    setPlacementKind(PlacementKind::OffmeshDraw);
    if (m_viewer)   m_viewer->setPlacementMode(true);
    if (m_viewer3d) m_viewer3d->setPlacementMode(true);
    statusBar()->showMessage(
        tr("Off-mesh draw armed - click the FROM point, then the TO point. "
           "The connection is appended to %1 (bidirectional, radius 12). "
           "Esc exits.").arg(offmeshFilePath()), 0);
}

bool MainWindow::appendOffmeshConnection(float fx, float fy, float fz,
                                         float tx, float ty, float tz)
{
    QString const path = offmeshFilePath();
    QFile file(path);
    bool const isNew = !file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("offmesh.txt append failed"),
            tr("Could not open %1 for appending.").arg(path));
        return false;
    }
    QTextStream out(&file);
    if (isNew)
        out << "# offmesh.txt -- manual off-mesh navmesh connections for mmaps_generator\n"
               "# Format: MapId TileX,TileY (FromX FromY FromZ) (ToX ToY ToZ) Radius [AreaId] [Flags]\n"
               "# Auto-loaded from <input>/offmesh.txt; connections are bidirectional.\n";
    // Tile of the FROM point (TC grid convention: gx = 32 - x/533.33).
    constexpr float kTile = 533.33333f;
    int const gx = int(std::floor(32.0f - fx / kTile));
    int const gy = int(std::floor(32.0f - fy / kTile));
    QSettings s;
    double const radius = s.value(QStringLiteral("editor/offmesh_radius"), 12.0).toDouble();
    out << QStringLiteral("%1 %2,%3 (%4 %5 %6) (%7 %8 %9) %10\n")
        .arg(*m_currentMapId).arg(gx).arg(gy)
        .arg(double(fx), 0, 'f', 1).arg(double(fy), 0, 'f', 1).arg(double(fz), 0, 'f', 1)
        .arg(double(tx), 0, 'f', 1).arg(double(ty), 0, 'f', 1).arg(double(tz), 0, 'f', 1)
        .arg(radius, 0, 'f', 1);
    return true;
}

// ---------------------------------------------------------------------------
// Bot dungeon-route chain (shared-schema playerbot_dungeon_routes).
// One render::Path per difficulty; Path::pathId carries the difficulty.
// Edits mutate the in-memory working copy and stay pending until
// "Commit dungeon route..." rewrites the map's rows in one transaction.
// ---------------------------------------------------------------------------

QString MainWindow::sharedDbSchema() const
{
    // Mirrors the server's Playerbot.SharedDatabase config; the editor keeps
    // its own copy in QSettings so it works without reading worldserver.conf.
    QSettings s;
    return s.value(QStringLiteral("db/shared_schema"),
                   QStringLiteral("wowc_playerbot")).toString();
}

void MainWindow::reloadDungeonRoutesForMap(uint32_t mapId)
{
    m_dungeonRoutes.clear();
    m_dungeonRoutesDirty = false;
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        pushDungeonRoutesToViewer();
        return;
    }
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT difficulty, seq, position_x, position_y, position_z "
        "FROM %s.playerbot_dungeon_routes WHERE map_id = %u "
        "ORDER BY difficulty, seq",
        sharedDbSchema().toUtf8().constData(), mapId);
    db::QueryResult res;
    db::QueryError err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        // Table/schema may simply not exist on a non-playerbot setup: keep
        // this quiet-ish (status bar, no modal) and leave the layer empty.
        statusBar()->showMessage(tr("dungeon-route query failed: %1")
            .arg(QString::fromStdString(err.message)), 4000);
        pushDungeonRoutesToViewer();
        return;
    }
    render::Path* cur = nullptr;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const diff = uint32_t(res.asUInt64(r, 0).value_or(0));
        if (!cur || cur->pathId != diff)
        {
            render::Path p;
            p.pathId  = diff;   // pathId doubles as the difficulty key
            p.comment = tr("bot route (difficulty %1)").arg(diff);
            m_dungeonRoutes.push_back(std::move(p));
            cur = &m_dungeonRoutes.back();
        }
        render::PathNode n;
        n.nodeId = int(res.asUInt64(r, 1).value_or(0));
        n.x = float(res.asDouble(r, 2).value_or(0.0));
        n.y = float(res.asDouble(r, 3).value_or(0.0));
        n.z = float(res.asDouble(r, 4).value_or(0.0));
        cur->nodes.push_back(n);
    }
    pushDungeonRoutesToViewer();
    if (!m_dungeonRoutes.empty())
    {
        size_t nodes = 0;
        for (auto const& p : m_dungeonRoutes) nodes += p.nodes.size();
        statusBar()->showMessage(tr("Loaded bot dungeon route: %1 chain(s), %2 waypoint(s)")
            .arg(m_dungeonRoutes.size()).arg(nodes), 4000);
    }
}

void MainWindow::pushDungeonRoutesToViewer()
{
    if (m_viewer3d)
        m_viewer3d->setDungeonRoutes(m_dungeonRoutes);
}

void MainWindow::onRouteNodeMoved3D(int routeIndex, int nodeIndex,
                                    float worldX, float worldY, float worldZ)
{
    if (routeIndex < 0 || routeIndex >= int(m_dungeonRoutes.size())) return;
    auto& nodes = m_dungeonRoutes[size_t(routeIndex)].nodes;
    if (nodeIndex < 0 || nodeIndex >= int(nodes.size())) return;
    nodes[nodeIndex].x = worldX;
    nodes[nodeIndex].y = worldY;
    // Same multi-floor guard as waypoint 3D moves: trust the drag plane
    // unless the .map ground is genuinely nearby.
    float z = worldZ;
    if (m_mapTileCache && m_currentMapId.has_value())
    {
        float const groundZ = m_mapTileCache->heightAt(*m_currentMapId, worldX, worldY);
        if (groundZ > -1.0e5f && std::abs(groundZ - worldZ) < 10.0f)
            z = groundZ;
    }
    nodes[nodeIndex].z = z;
    m_dungeonRoutesDirty = true;
    pushDungeonRoutesToViewer();
    statusBar()->showMessage(
        tr("Route waypoint %1 moved - PENDING until Path > Commit dungeon route")
            .arg(nodeIndex), 4000);
}

void MainWindow::onRouteNodeContextMenu3D(int routeIndex, int nodeIndex, QPoint globalPos)
{
    if (routeIndex < 0 || routeIndex >= int(m_dungeonRoutes.size())) return;
    auto& nodes = m_dungeonRoutes[size_t(routeIndex)].nodes;
    if (nodeIndex < 0 || nodeIndex >= int(nodes.size())) return;

    QMenu menu(this);
    QAction* const insertBefore = menu.addAction(tr("Insert route waypoint BEFORE this"));
    QAction* const insertAfter  = menu.addAction(tr("Insert route waypoint AFTER this"));
    menu.addSeparator();
    QAction* const deleteNode   = menu.addAction(tr("Delete this route waypoint"));
    if (nodes.size() <= 2)
        deleteNode->setEnabled(false);

    QAction* const chosen = menu.exec(globalPos);
    if (!chosen) return;

    render::PathNode const ref = nodes[size_t(nodeIndex)];
    if (chosen == insertBefore || chosen == insertAfter)
    {
        render::PathNode neu = ref;
        int const at = (chosen == insertBefore) ? nodeIndex : nodeIndex + 1;
        int const nb = (chosen == insertBefore) ? nodeIndex - 1 : nodeIndex + 1;
        if (nb >= 0 && nb < int(nodes.size()))
        {
            neu.x = (nodes[size_t(nb)].x + ref.x) * 0.5f;
            neu.y = (nodes[size_t(nb)].y + ref.y) * 0.5f;
            neu.z = (nodes[size_t(nb)].z + ref.z) * 0.5f;
        }
        else
        {
            neu.x = ref.x + 5.0f;   // chain end: nudge so the marker is grabbable
        }
        nodes.insert(nodes.begin() + at, neu);
    }
    else if (chosen == deleteNode)
    {
        nodes.erase(nodes.begin() + nodeIndex);
    }
    for (size_t i = 0; i < nodes.size(); ++i)
        nodes[i].nodeId = int(i);
    m_dungeonRoutesDirty = true;
    pushDungeonRoutesToViewer();
    statusBar()->showMessage(
        tr("Route edited - PENDING until Path > Commit dungeon route"), 4000);
}

void MainWindow::onRouteSegmentContextMenu3D(int routeIndex, int afterNodeIndex,
                                             float worldX, float worldY, float worldZ,
                                             QPoint globalPos)
{
    if (routeIndex < 0 || routeIndex >= int(m_dungeonRoutes.size())) return;
    auto& nodes = m_dungeonRoutes[size_t(routeIndex)].nodes;
    if (afterNodeIndex < 0 || afterNodeIndex + 1 >= int(nodes.size())) return;

    QMenu menu(this);
    QAction* const insertHere = menu.addAction(tr("Insert route waypoint here"));
    QAction* const chosen = menu.exec(globalPos);
    if (chosen != insertHere) return;

    render::PathNode neu = nodes[size_t(afterNodeIndex)];
    neu.x = worldX;
    neu.y = worldY;
    neu.z = worldZ;   // segment-lerped Z is floor-correct in dungeons
    if (m_mapTileCache && m_currentMapId.has_value())
    {
        float const groundZ = m_mapTileCache->heightAt(*m_currentMapId, worldX, worldY);
        if (groundZ > -1.0e5f && std::abs(groundZ - worldZ) < 10.0f)
            neu.z = groundZ;
    }
    nodes.insert(nodes.begin() + afterNodeIndex + 1, neu);
    for (size_t i = 0; i < nodes.size(); ++i)
        nodes[i].nodeId = int(i);
    m_dungeonRoutesDirty = true;
    pushDungeonRoutesToViewer();
    statusBar()->showMessage(
        tr("Route waypoint inserted - PENDING until Path > Commit dungeon route"), 4000);
}

void MainWindow::onReloadDungeonRoutes()
{
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Open a map first."), 3000);
        return;
    }
    if (m_dungeonRoutesDirty
        && QMessageBox::question(this, tr("Discard route edits?"),
               tr("The dungeon route has uncommitted edits. Discard them and reload from the DB?"))
           != QMessageBox::Yes)
        return;
    reloadDungeonRoutesForMap(*m_currentMapId);
}

void MainWindow::onNewDungeonRouteAtCamera()
{
    if (!m_currentMapId.has_value() || !m_viewer3d)
    {
        statusBar()->showMessage(tr("Open a map (3D view) first."), 3000);
        return;
    }
    if (!m_dungeonRoutes.empty())
    {
        statusBar()->showMessage(
            tr("A route already exists for this map - edit its waypoints instead."), 4000);
        return;
    }
    QVector3D const cam = m_viewer3d->cameraPosition();
    render::Path p;
    p.pathId  = 0;   // difficulty 0 = any
    p.comment = tr("bot route (difficulty 0)");
    render::PathNode a;
    a.nodeId = 0; a.x = cam.x(); a.y = cam.y(); a.z = cam.z();
    render::PathNode b = a;
    b.nodeId = 1; b.x += 10.0f;
    p.nodes = { a, b };
    m_dungeonRoutes.push_back(std::move(p));
    m_dungeonRoutesDirty = true;
    pushDungeonRoutesToViewer();
    statusBar()->showMessage(
        tr("New 2-waypoint route seeded at the camera - drag the gold markers into "
           "place, insert more via right-click, then Path > Commit dungeon route."), 8000);
}

void MainWindow::onCommitDungeonRoutes()
{
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Open a map first."), 3000);
        return;
    }
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first."));
        return;
    }
    uint32_t const mapId = *m_currentMapId;
    size_t totalNodes = 0;
    for (auto const& p : m_dungeonRoutes) totalNodes += p.nodes.size();
    QString const schema = sharedDbSchema();
    if (QMessageBox::question(this, tr("Commit dungeon route"),
            tr("Rewrite the bot dungeon route for map %1 in %2.playerbot_dungeon_routes?\n"
               "%3 chain(s), %4 waypoint(s). Existing rows for this map are replaced.\n\n"
               "Note: dungeons whose C++ script hand-authors route_waypoints "
               "(currently Deadmines) ignore these DB rows until the authored "
               "chain is removed.")
              .arg(mapId).arg(schema)
              .arg(m_dungeonRoutes.size()).arg(totalNodes))
        != QMessageBox::Yes)
        return;

    QByteArray const schemaUtf8 = schema.toUtf8();
    auto err = m_worldDb->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Commit failed"),
            QString::fromStdString(err.message));
        return;
    }
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "DELETE FROM %s.playerbot_dungeon_routes WHERE map_id = %u",
        schemaUtf8.constData(), mapId);
    err = m_worldDb->exec(buf);
    if (!err.ok())
    {
        (void)m_worldDb->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Commit failed"),
            QString::fromStdString(err.message));
        return;
    }
    for (auto const& p : m_dungeonRoutes)
    {
        for (size_t i = 0; i < p.nodes.size(); ++i)
        {
            std::snprintf(buf, sizeof(buf),
                "INSERT INTO %s.playerbot_dungeon_routes "
                "(map_id, difficulty, seq, position_x, position_y, position_z) "
                "VALUES (%u, %u, %zu, %.3f, %.3f, %.3f)",
                schemaUtf8.constData(), mapId, p.pathId, i,
                double(p.nodes[i].x), double(p.nodes[i].y), double(p.nodes[i].z));
            err = m_worldDb->exec(buf);
            if (!err.ok())
            {
                (void)m_worldDb->exec("ROLLBACK");
                QMessageBox::warning(this, tr("Commit failed"),
                    QString::fromStdString(err.message));
                return;
            }
        }
    }
    err = m_worldDb->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_worldDb->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Commit failed"),
            QString::fromStdString(err.message));
        return;
    }
    m_dungeonRoutesDirty = false;
    statusBar()->showMessage(
        tr("Dungeon route committed: %1 waypoint(s) for map %2 (worldserver reloads on restart)")
            .arg(totalNodes).arg(mapId), 6000);
}

void MainWindow::onSpawnContextMenu(int spawnIndex, QPoint globalPos)
{
    if (!m_viewer || !m_spawnModel) return;
    auto const& spawns = m_viewer->spawns();
    if (spawnIndex < 0 || spawnIndex >= int(spawns.size())) return;

    QMenu menu(this);
    QAction* const cloneAct = menu.addAction(tr("Clone..."));
    QAction* const chosen = menu.exec(globalPos);
    if (chosen != cloneAct) return;

    // Capture the source row index up-front; the modal dialog may indirectly
    // change selection state, but the clone op must stay anchored to this row.
    m_cloneSourceSpawnIndex = spawnIndex;

    app::SpawnCloneDialog dlg(this);
    connect(&dlg, &app::SpawnCloneDialog::cloneRequested,
            this, &MainWindow::onCloneRequested);
    dlg.exec();
    m_cloneSourceSpawnIndex = -1;
}

void MainWindow::onCloneRequested(int count, int patternIdx, float radius,
                                  bool snap, bool preserveOri)
{
    if (!m_spawnModel || !m_viewer) return;
    if (m_cloneSourceSpawnIndex < 0) return;
    auto const& current = m_spawnModel->current();
    if (m_cloneSourceSpawnIndex >= int(current.size())) return;
    if (count <= 0) return;

    render::Spawn const source = current[m_cloneSourceSpawnIndex];

    // Deterministic-per-session RNG; std::rand is good enough for placement
    // jitter and avoids dragging <random> + seed plumbing for one feature.
    auto frand01 = []() -> float {
        return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    };
    constexpr float kTwoPi = 6.2831853071795864769f;

    // Single undo frame wraps the entire batch so Ctrl+Z reverts all clones.
    m_undo->recordOn(m_spawnModel.get(), tr("Clone spawn x%1").arg(count), [&]() {
        // Grid pre-compute: side length + offset of slot 0 from center.
        int const gridSide = (patternIdx == 2) ? static_cast<int>(std::ceil(std::sqrt(double(count)))) : 0;
        for (int i = 0; i < count; ++i)
        {
            float dx = 0.0f, dy = 0.0f;
            switch (patternIdx)
            {
                case 1: // Ring
                {
                    float const angle = float(i) * kTwoPi / float(count);
                    dx = radius * std::cos(angle);
                    dy = radius * std::sin(angle);
                    break;
                }
                case 2: // Grid (square, centered, `radius` spacing)
                {
                    int const col = i % gridSide;
                    int const row = i / gridSide;
                    float const half = float(gridSide - 1) * 0.5f;
                    dx = (float(col) - half) * radius;
                    dy = (float(row) - half) * radius;
                    break;
                }
                case 0: // Random scatter
                default:
                {
                    float const theta = frand01() * kTwoPi;
                    float const r     = frand01() * radius;
                    dx = r * std::cos(theta);
                    dy = r * std::sin(theta);
                    break;
                }
            }

            render::Spawn s = source;
            s.worldX = source.worldX + dx;
            s.worldY = source.worldY + dy;
            s.worldZ = source.worldZ;
            if (snap)
                s.worldZ = snapToGround(s.mapId, s.worldX, s.worldY,
                                        /*probeZ*/ source.worldZ + 50.0f,
                                        /*fallback*/ source.worldZ);
            if (!preserveOri)
                s.orientation = frand01() * kTwoPi;

            // Reserve a fresh guid per clone from the same pool the placement
            // path uses; SpawnCommitDialog will translate to real auto_increment
            // values on commit but the local model needs uniqueness now.
            if (s.kind == render::SpawnKind::Creature)
                s.guid = static_cast<int64_t>(m_nextCreatureGuid++);
            else
                s.guid = static_cast<int64_t>(m_nextGoGuid++);

            m_spawnModel->addNew(s);
        }
    });

    pushSpawnsToViewer();
    if (m_spawnEditor)
        m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());

    char const* patternName = (patternIdx == 1) ? "Ring"
                            : (patternIdx == 2) ? "Grid"
                                                : "Random scatter";
    statusBar()->showMessage(tr("Cloned spawn %1 times (pattern=%2, radius=%3)")
        .arg(count).arg(QString::fromLatin1(patternName)).arg(radius, 0, 'f', 1), 4000);
}

void MainWindow::onDeleteSelectedPath()
{
    if (!m_waypointModel || m_selectedPathIndex < 0) return;
    int const idx = m_selectedPathIndex;
    bool const changed = m_undo->recordIf(m_waypointModel.get(),
        tr("Delete path"), [&]() {
        return m_waypointModel->removePath(idx);
    });
    if (changed)
    {
        m_selectedPathIndex = -1;
        if (m_pathDock) { m_pathDock->clear(); m_pathDock->setPendingCount(m_waypointModel->pendingCount()); }
        pushPathsToViewer();
    }
}

void MainWindow::onRevertPaths()
{
    if (!m_waypointModel) return;
    m_waypointModel->revertAll();
    m_selectedPathIndex = -1;
    if (m_pathDock) { m_pathDock->clear(); m_pathDock->setPendingCount(0); }
    pushPathsToViewer();
}

void MainWindow::onCommitPaths()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("Connect before committing."));
        return;
    }
    if (!m_currentMapId.has_value() || !m_waypointModel) return;
    if (m_waypointModel->pendingCount() == 0)
    {
        statusBar()->showMessage(tr("Nothing to commit."), 2000);
        return;
    }
    app::WaypointCommitDialog dlg(m_worldDb.get(), *m_waypointModel, *m_currentMapId, this);
    if (dlg.exec() != QDialog::Accepted) return;
    auto committed = dlg.committedRows();
    m_waypointModel->acceptCommit(std::move(committed));
    m_selectedPathIndex = -1;
    if (m_pathDock) { m_pathDock->clear(); m_pathDock->setPendingCount(0); }
    pushPathsToViewer();
    // Bump PathId reservation high-water mark.
    db::QueryResult maxRes;
    if (m_worldDb->query("SELECT COALESCE(MAX(PathId), 0)+1 FROM waypoint_path", maxRes).ok()
        && maxRes.rowCount() > 0)
        m_nextPathId = static_cast<uint32_t>(maxRes.asUInt64(0, 0).value_or(0));
    statusBar()->showMessage(tr("Path commit applied."), 3000);
}

void MainWindow::onAddSelectedSpawnToGroup()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("Connect to the world DB first."));
        return;
    }
    if (!m_spawnModel || m_selectedSpawnIndex < 0)
    {
        QMessageBox::warning(this, tr("No spawn selected"),
            tr("Click a creature spawn icon first."));
        return;
    }
    render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
    if (s.kind != render::SpawnKind::Creature)
    {
        QMessageBox::warning(this, tr("Wrong kind"),
            tr("Only creatures can be added to a spawn_group (this UI; gameobjects use the same table with spawnType=2 if needed)."));
        return;
    }

    bool ok = false;
    int const groupId = QInputDialog::getInt(this,
        tr("Add to group"),
        tr("groupId (must exist in spawn_group_template):"),
        1, 1, INT_MAX, 1, &ok);
    if (!ok)
        return;

    // Verify the group exists before the INSERT so we never write an orphan.
    char checkSql[128];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT groupName FROM spawn_group_template WHERE groupId = %d", groupId);
    db::QueryResult checkRes;
    auto const checkErr = m_worldDb->query(checkSql, checkRes);
    if (!checkErr.ok() || checkRes.rowCount() == 0)
    {
        QMessageBox::warning(this, tr("Group not found"),
            tr("No row in spawn_group_template with groupId=%1.").arg(groupId));
        return;
    }
    QString const groupName = QString::fromStdString(checkRes.cell(0, 0));

    QString const sql = QString(
        "INSERT INTO spawn_group (groupId, spawnType, spawnId) "
        "VALUES (%1, 1, %2);")
        .arg(groupId).arg(s.guid);

    app::ConfirmSqlDialog confirm(m_worldDb.get(),
        tr("Add creature guid=%1 entry=%2 to group %3 \"%4\"")
            .arg(s.guid).arg(s.entry).arg(groupId).arg(groupName),
        sql, this);
    if (confirm.exec() != QDialog::Accepted)
        return;
    statusBar()->showMessage(
        tr("Added guid=%1 to groupId=%2 (affected=%3)")
            .arg(s.guid).arg(groupId).arg(qulonglong(confirm.affectedRows())),
        5000);
}

void MainWindow::onShowGroupsPoolsDialog()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first."));
        return;
    }
    QString const worldDbName = QString::fromStdString(
        db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
    QString const dbName = worldDbName.isEmpty()
        ? QStringLiteral("playerbot_world") : worldDbName;
    auto* dlg = new app::GroupsPoolsDialog(m_worldDb.get(), dbName, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &app::GroupsPoolsDialog::highlightSpawnGuids,
            this, &MainWindow::onHighlightSpawnGuids);
    dlg->show();
}

void MainWindow::onShowGraveyardZoneDialog()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first."));
        return;
    }
    app::GraveyardZoneDialog dlg(m_worldDb.get(), this);
    dlg.exec();
    // After dlg closes, refresh graveyards on the current map in case
    // operator edited a row that affects the visible graveyard set.
    if (m_currentMapId.has_value() && m_worldDb && m_worldDb->isConnected())
        reloadGraveyardsForMap(*m_currentMapId);
}

void MainWindow::onResetWindowLayout()
{
    // Clear the saved layout in QSettings AND clear the runtime state.
    // The QMainWindow rebuilds its default dock layout from the
    // tabifyDockWidget calls done in build*Dock at next save/restore.
    QSettings settings;
    settings.remove(QStringLiteral("ui/geometry"));
    settings.remove(QStringLiteral("ui/state"));
    // Re-tabify the existing docks back into the default group.  Without
    // this, the runtime layout stays as-is until next restart.
    auto* spawnDock = findChild<QDockWidget*>(QStringLiteral("spawn_dock"));
    if (!spawnDock) return;
    // Right-pane docks that should tab back onto spawn_dock (UI declutter:
    // the old per-info dock names are now folded into info_inspector_dock).
    for (QString const& name : {
            QStringLiteral("annot_dock"),
            QStringLiteral("diag_dock"),
            QStringLiteral("vendor_dock"),
            QStringLiteral("conditions_dock"),
            QStringLiteral("info_inspector_dock"),
            QStringLiteral("property_inspector_dock") })
    {
        if (auto* d = findChild<QDockWidget*>(name))
        {
            d->setFloating(false);
            d->show();
            addDockWidget(Qt::RightDockWidgetArea, d);
            tabifyDockWidget(spawnDock, d);
        }
    }
    // The annotation toolbox is the lone LEFT-area dock; restore it too --
    // it used to be the one dock Reset couldn't bring back.
    if (auto* d = findChild<QDockWidget*>(QStringLiteral("annot_toolbox")))
    {
        d->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, d);
        d->show();
    }
    // LogTailDock stays in the bottom dock area (hidden by default; the
    // Window menu / Tools -> Tail worldserver log reopen it on demand).
    if (auto* d = findChild<QDockWidget*>(QStringLiteral("log_tail_dock")))
    {
        d->setFloating(false);
        addDockWidget(Qt::BottomDockWidgetArea, d);
    }
    // Handcrafted roads is opt-in (hidden by default) but must still land
    // back in the right tab group when the operator had moved it.
    if (auto* d = findChild<QDockWidget*>(QStringLiteral("handcrafted_road_dock")))
    {
        d->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, d);
        if (spawnDock) tabifyDockWidget(spawnDock, d);
        d->hide();
    }
    // Surface the spawn dock's tab group front-most so the reset visibly
    // lands the user somewhere sensible.
    spawnDock->show();
    spawnDock->raise();
    statusBar()->showMessage(tr("Window layout reset."), 3000);
}

void MainWindow::onShowMinimapDiagnostics()
{
    // MinimapDiagnostics is now a page of the unified InfoInspectorDock.
    // Surface the dock + flip the page; refresh runs unconditionally so
    // already-visible state still gets fresh numbers.
    if (m_infoInspector)
        m_infoInspector->showPage(app::InfoInspectorDock::Page::MinimapDiagnostics);
    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("info_inspector_dock")))
    {
        dock->setFloating(false);
        dock->show();
        dock->raise();
    }
    onRefreshMinimapDiagnostics();
}

void MainWindow::onRefreshMinimapDiagnostics()
{
    if (!m_minimapDiagDock)
        return;
    // Resolve the current map's Map.db2 directory name when both the
    // mapId and the lookup table are available; falls back to an empty
    // QString which the dock renders as "<unknown directory>".
    QString mapDir;
    uint32_t mapIdValue = 0;
    if (m_currentMapId.has_value())
    {
        mapIdValue = *m_currentMapId;
        if (m_mapDb2)
        {
            if (auto d = m_mapDb2->directoryFor(mapIdValue); d)
                mapDir = QString::fromStdString(*d);
        }
    }
    bool const cascOpen = m_cascClient && m_cascClient->isOpen();
    int const mapDb2Entries = m_mapDb2 ? int(m_mapDb2->size()) : 0;
    int const heightmapTiles  = m_viewer ? m_viewer->heightmapTileCount()      : 0;
    int const cachedTextures  = m_viewer ? m_viewer->minimapCachedCount()      : 0;
    int const successfulLoads = m_viewer ? m_viewer->minimapSuccessfulLoads()  : 0;
    int const failedLoads     = m_viewer ? m_viewer->minimapFailedLoads()      : 0;
    QString const lastTried   = m_viewer ? m_viewer->minimapLastTried()        : QString();
    // Listfile + PNG-dir-count surfacing for the dock.  PNG count -1 means
    // "dir not configured" so the dock can render a dim placeholder.
    int const listfileEntries = m_listfile ? int(m_listfile->entryCount()) : 0;
    int pngDirCount = -1;
    if (!m_minimapDir.isEmpty())
    {
        QDir d(m_minimapDir);
        if (d.exists())
        {
            // entryList(Files|Dirs, NoFilter) walks the top-level only; the
            // operator's per-mapId subdirs are what we care about, so count
            // anything under the root — empty root vs. populated root is the
            // signal we want.
            QStringList const entries = d.entryList(
                QDir::AllEntries | QDir::NoDotAndDotDot);
            pngDirCount = int(entries.size());
        }
        else
        {
            pngDirCount = 0;
        }
    }
    m_minimapDiagDock->setMinimapInfo(
        m_cascClientDir, cascOpen, mapDb2Entries,
        m_minimapDir, mapIdValue, mapDir,
        heightmapTiles, cachedTextures,
        successfulLoads, failedLoads,
        lastTried,
        m_listfileCsvPath, listfileEntries, pngDirCount);
    // Road-overlay counts: vertex pairs -> polyline count.  Both numbers
    // are zero when no navmesh is loaded / no handcrafted polylines have
    // been pushed.  The auto count usually grows into the hundreds for a
    // populated continent; handcrafted stays 0 until a separate agent
    // wires up its data feed via NavMeshView::setHandcraftedRoadPolylines.
    if (m_viewer)
    {
        int const autoLines = int(m_viewer->autoRoadPolylineVertexCount() / 2);
        int const handLines = int(m_viewer->handcraftedRoadPolylineVertexCount() / 2);
        m_minimapDiagDock->setRoadOverlayInfo(autoLines, handLines);
    }
    else
    {
        m_minimapDiagDock->setRoadOverlayInfo(0, 0);
    }
}

void MainWindow::onUndo()
{
    if (!m_undo || !m_undo->canUndo()) return;
    m_undo->undo();
    refreshAllViewers();
}

void MainWindow::onRedo()
{
    if (!m_undo || !m_undo->canRedo()) return;
    m_undo->redo();
    refreshAllViewers();
}

void MainWindow::onUndoStateChanged(QString const& label)
{
    if (!label.isEmpty())
        statusBar()->showMessage(label, 2000);
}

void MainWindow::refreshAllViewers()
{
    // Drop dock selections; the index might no longer point at the same
    // row after a state snapshot restore.  Operators can re-pick.
    m_selectedAnnotationIndex = -1;
    m_selectedPathIndex       = -1;
    m_selectedAreatriggerIndex = -1;
    m_selectedGraveyardIndex   = -1;
    if (m_annotationToolbox)    m_annotationToolbox->clearSelectedRow();
    if (m_annotPropertyDock)    m_annotPropertyDock->clear();
    if (m_pathDock)             m_pathDock->clear();
    if (m_areatriggerDock)      m_areatriggerDock->clear();
    if (m_graveyardDock)        m_graveyardDock->clear();
    if (m_spawnEditor)          m_spawnEditor->clear();

    pushSpawnsToViewer();
    pushAnnotationsToViewer();
    pushPathsToViewer();
    pushAreatriggersToViewer();
    pushGraveyardsToViewer();
    updateExportPendingActionEnabled();
}

void MainWindow::updateExportPendingActionEnabled()
{
    if (!m_exportPendingAction)
        return;
    auto pending = [](auto const* m) -> size_t { return m ? m->pendingCount() : 0u; };
    size_t const total =
        pending(m_annotationModel.get())  + pending(m_spawnModel.get()) +
        pending(m_waypointModel.get())    + pending(m_areatriggerModel.get()) +
        pending(m_graveyardModel.get())   + pending(m_conditionsModel.get());
    // Smart-script model lives inside ConditionsDock for now; not exposed
    // as a member.  When that wiring lands, fold its pendingCount() in
    // here and pass the model into onExportPendingChanges().
    m_exportPendingAction->setEnabled(total > 0);
}

void MainWindow::onExportPendingChanges()
{
    QString const defaultName = QStringLiteral("world_editor_pending_%1.sql")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    QString const path = QFileDialog::getSaveFileName(
        this,
        tr("Export pending changes as SQL patch"),
        defaultName,
        tr("SQL files (*.sql);;All files (*)"));
    if (path.isEmpty())
        return;

    app::SqlPatchExporter exporter;
    auto const result = exporter.exportAll(
        path,
        m_worldDb.get(),
        m_annotationModel.get(),
        m_spawnModel.get(),
        m_waypointModel.get(),
        m_areatriggerModel.get(),
        m_graveyardModel.get(),
        /*sai*/ nullptr,
        m_conditionsModel.get());

    if (!result.errMsg.isEmpty())
    {
        QMessageBox::critical(this, tr("Export failed"),
            tr("Could not export pending changes:\n%1").arg(result.errMsg));
        statusBar()->showMessage(tr("Export failed: %1").arg(result.errMsg), 8000);
        return;
    }

    statusBar()->showMessage(
        tr("Exported %1 pending statements to %2").arg(result.statements).arg(path), 8000);
}

void MainWindow::onAutoTagNpcs()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before auto-tagging NPCs."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::information(this, tr("No map loaded"),
            tr("Open a map first - the scan walks creatures on the current map only."));
        return;
    }

    // Pull creature joined with creature_template for the current map,
    // filtering to rows with a non-zero npcflag.  We project the bits we
    // care about into the SELECT so per-row decoding stays cheap.
    //
    //   bit  meaning              kind we map to
    //   0x80 / 0x100 / 0x200 / 0x800   vendor mask  -> AnnotationKind::Vendor (7)
    //   0x10000  innkeeper                          -> AnnotationKind::Innkeeper (9)
    //   0x04000000 mailbox                          -> AnnotationKind::Mailbox (8)
    //   0x00200000 auctioneer                       -> AnnotationKind::Vendor (7) with label
    QString const sql = QStringLiteral(
        "SELECT c.guid, c.id, c.position_x, c.position_y, c.position_z, "
        "       ct.name, ct.npcflag "
        "FROM creature c "
        "INNER JOIN creature_template ct ON c.id = ct.entry "
        "WHERE c.map = %1 AND ct.npcflag <> 0").arg(*m_currentMapId);

    db::QueryResult res;
    auto const err = m_worldDb->query(sql.toStdString(), res);
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Query failed"),
            tr("Could not scan creatures: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    constexpr uint64_t kVendorMask     = 0x00000080ull | 0x00000100ull
                                       | 0x00000200ull | 0x00000400ull
                                       | 0x00000800ull;
    constexpr uint64_t kInnkeeperFlag  = 0x00010000ull;
    constexpr uint64_t kMailboxFlag    = 0x04000000ull;
    constexpr uint64_t kAuctioneerFlag = 0x00200000ull;
    constexpr float    kDuplicateRadius = 20.0f;
    constexpr float    kDuplicateRadiusSq = kDuplicateRadius * kDuplicateRadius;

    // Pre-index existing annotations by kind for the duplicate check.
    auto const& currentAnnots = m_annotationModel->current();
    struct Probe { float x, y; };
    std::array<std::vector<Probe>, size_t(render::AnnotationKind::Count_)> byKind;
    for (render::Annotation const& a : currentAnnots)
    {
        if (a.mapId != *m_currentMapId) continue;
        auto const idx = size_t(a.kind);
        if (idx < byKind.size())
            byKind[idx].push_back({ a.x, a.y });
    }

    auto withinExisting = [&](render::AnnotationKind kind, float x, float y) {
        auto const idx = size_t(kind);
        if (idx >= byKind.size()) return false;
        for (Probe const& p : byKind[idx])
        {
            float const dx = p.x - x;
            float const dy = p.y - y;
            if (dx * dx + dy * dy <= kDuplicateRadiusSq)
                return true;
        }
        return false;
    };

    struct Candidate
    {
        render::AnnotationKind kind;
        float                  x, y, z;
        QString                label;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(res.rowCount() * 2);

    for (size_t row = 0; row < res.rowCount(); ++row)
    {
        auto const npcflag = res.asUInt64(row, 6).value_or(0);
        auto const x = float(res.asDouble(row, 2).value_or(0.0));
        auto const y = float(res.asDouble(row, 3).value_or(0.0));
        auto const z = float(res.asDouble(row, 4).value_or(0.0));
        QString const name = QString::fromStdString(res.cell(row, 5));

        // Each NPC can be more than one role -- emit one annotation per
        // role it qualifies for so the route planner can see all roles.
        if ((npcflag & kVendorMask) != 0
            && !withinExisting(render::AnnotationKind::Vendor, x, y))
        {
            candidates.push_back({ render::AnnotationKind::Vendor, x, y, z, name });
        }
        if ((npcflag & kAuctioneerFlag) != 0
            && !withinExisting(render::AnnotationKind::Vendor, x, y))
        {
            candidates.push_back({ render::AnnotationKind::Vendor, x, y, z,
                                   tr("%1 (Auctioneer)").arg(name) });
        }
        if ((npcflag & kInnkeeperFlag) != 0
            && !withinExisting(render::AnnotationKind::Innkeeper, x, y))
        {
            candidates.push_back({ render::AnnotationKind::Innkeeper, x, y, z, name });
        }
        if ((npcflag & kMailboxFlag) != 0
            && !withinExisting(render::AnnotationKind::Mailbox, x, y))
        {
            candidates.push_back({ render::AnnotationKind::Mailbox, x, y, z, name });
        }
    }

    if (candidates.empty())
    {
        QMessageBox::information(this, tr("Auto-tag NPCs"),
            tr("Nothing to do -- every flagged NPC on this map already has an "
               "annotation of the matching kind within %1y, or no flagged NPCs "
               "were found.").arg(kDuplicateRadius));
        return;
    }

    auto const choice = QMessageBox::question(this, tr("Auto-tag NPCs"),
        tr("Drop %1 annotation(s) for vendor / innkeeper / mailbox / auctioneer "
           "NPCs on map %2?\n\nExisting annotations within %3y of an NPC of the "
           "same kind are skipped.  This is a pending edit -- you can undo with "
           "Ctrl+Z or discard via the annotation dock's Revert button.")
            .arg(candidates.size())
            .arg(*m_currentMapId)
            .arg(kDuplicateRadius),
        QMessageBox::Ok | QMessageBox::Cancel);
    if (choice != QMessageBox::Ok)
        return;

    QString const created = tr("auto-tag npcflag scan");
    m_undo->recordOn(m_annotationModel.get(),
        tr("Auto-tag %1 NPC annotations").arg(candidates.size()), [&]() {
        for (Candidate const& c : candidates)
        {
            render::Annotation a;
            a.mapId     = *m_currentMapId;
            a.zoneId    = 0;
            a.kind      = c.kind;
            a.x         = c.x;
            a.y         = c.y;
            a.z         = c.z;
            // Match the default per-kind radii used by AnnotationToolbox
            // so the operator's manual placements look the same.
            switch (c.kind)
            {
                case render::AnnotationKind::Vendor:    a.radius = 6.0f; break;
                case render::AnnotationKind::Innkeeper: a.radius = 6.0f; break;
                case render::AnnotationKind::Mailbox:   a.radius = 5.0f; break;
                default:                                a.radius = 6.0f; break;
            }
            a.label     = c.label;
            a.notes     = QString();
            a.createdBy = created;
            m_annotationModel->addNew(a);
        }
    });
    pushAnnotationsToViewer();
    statusBar()->showMessage(
        tr("Auto-tagged %1 NPC annotation(s) - Ctrl+Z to undo.")
            .arg(candidates.size()), 5000);
}

void MainWindow::loadQuestMarkers()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer
        || !m_currentMapId.has_value())
    {
        if (m_viewer) m_viewer->setQuestMarkers({});
        return;
    }
    uint32_t const mapId = *m_currentMapId;

    // One UNION ALL covers creature + gameobject, starter + ender so a
    // single query trip pulls everything for this map.  Each row tags
    // its (spawnKind, role) so the C++ side can bin into starts/ends.
    QString const sql = QStringLiteral(
        "SELECT c.guid, 0 AS spawnKind, 1 AS role, qs.quest, "
        "       c.position_x, c.position_y, c.position_z "
        "FROM creature c "
        "INNER JOIN creature_queststarter qs ON qs.id = c.id "
        "WHERE c.map = %1 "
        "UNION ALL "
        "SELECT c.guid, 0, 2, qe.quest, "
        "       c.position_x, c.position_y, c.position_z "
        "FROM creature c "
        "INNER JOIN creature_questender qe ON qe.id = c.id "
        "WHERE c.map = %1 "
        "UNION ALL "
        "SELECT g.guid, 1, 1, gqs.quest, "
        "       g.position_x, g.position_y, g.position_z "
        "FROM gameobject g "
        "INNER JOIN gameobject_queststarter gqs ON gqs.id = g.id "
        "WHERE g.map = %1 "
        "UNION ALL "
        "SELECT g.guid, 1, 2, gqe.quest, "
        "       g.position_x, g.position_y, g.position_z "
        "FROM gameobject g "
        "INNER JOIN gameobject_questender gqe ON gqe.id = g.id "
        "WHERE g.map = %1").arg(mapId);

    db::QueryResult res;
    auto const err = m_worldDb->query(sql.toStdString(), res);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("Quest scan failed: %1")
            .arg(QString::fromStdString(err.message)), 5000);
        m_viewer->setQuestMarkers({});
        return;
    }

    // Bin by (spawnKind, guid) -> QuestMarker accumulating quests.
    struct Key { int64_t guid; uint8_t kind; };
    struct KeyHash
    {
        size_t operator()(std::pair<int64_t, uint8_t> const& k) const noexcept
        {
            return std::hash<int64_t>()(k.first) ^ (size_t(k.second) << 1);
        }
    };
    std::unordered_map<std::pair<int64_t, uint8_t>, render::QuestMarker, KeyHash> byKey;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int64_t  const guid    = int64_t(res.asUInt64(r, 0).value_or(0));
        uint8_t  const kind    = uint8_t(res.asUInt64(r, 1).value_or(0));
        uint64_t const role    = res.asUInt64(r, 2).value_or(0);
        uint32_t const questId = uint32_t(res.asUInt64(r, 3).value_or(0));
        float    const x       = float(res.asDouble(r, 4).value_or(0.0));
        float    const y       = float(res.asDouble(r, 5).value_or(0.0));
        float    const z       = float(res.asDouble(r, 6).value_or(0.0));
        auto const k = std::make_pair(guid, kind);
        auto& m = byKey[k];
        if (m.spawnGuid == 0)
        {
            m.spawnGuid = guid;
            m.spawnKind = kind;
            m.x = x; m.y = y; m.z = z;
        }
        if (role == 1) m.startsQuests.push_back(questId);
        else           m.endsQuests.push_back(questId);
    }
    // Pull every unique quest id this map's NPCs are involved in and
    // resolve its faction via quest_template.AllowableRaces.  A race
    // mask is alliance / horde / both per TC's ChrRaces table; a 0
    // mask means "any race" (neutral).
    //
    // Alliance race bits (CHR_RACES_MASK_ALLIANCE in TC):
    //   0x00000001 Human
    //   0x00000004 Dwarf
    //   0x00000008 NightElf
    //   0x00000040 Gnome
    //   0x00000400 Draenei
    //   0x00200000 Worgen
    //   0x01000000 LightforgedDraenei
    //   0x02000000 VoidElf
    //   0x10000000 DarkIronDwarf
    //   0x40000000 KulTiran
    //   0x80000000 Mechagnome
    // Horde race bits:
    //   0x00000002 Orc
    //   0x00000010 Undead
    //   0x00000020 Tauren
    //   0x00000080 Troll
    //   0x00000200 BloodElf
    //   0x00000100 Goblin
    //   0x00400000 Nightborne (Allied)
    //   0x04000000 HighmountainTauren
    //   0x08000000 MagharOrc
    //   0x20000000 ZandalariTroll
    //   0x100000000 (overflow) Vulpera -- TC truncates to 32-bit
    constexpr uint64_t kAllianceRaces =
        0x00000001ull | 0x00000004ull | 0x00000008ull | 0x00000040ull
      | 0x00000400ull | 0x00200000ull | 0x01000000ull | 0x02000000ull
      | 0x10000000ull | 0x40000000ull | 0x80000000ull;
    constexpr uint64_t kHordeRaces =
        0x00000002ull | 0x00000010ull | 0x00000020ull | 0x00000080ull
      | 0x00000200ull | 0x00000100ull | 0x00400000ull | 0x04000000ull
      | 0x08000000ull | 0x20000000ull;

    std::unordered_map<uint32_t, uint8_t> factionByQuest;
    std::unordered_set<uint32_t> questIds;
    for (auto const& [k, m] : byKey)
    {
        for (uint32_t q : m.startsQuests) questIds.insert(q);
        for (uint32_t q : m.endsQuests)   questIds.insert(q);
    }
    if (!questIds.empty())
    {
        QString questList;
        questList.reserve(int(questIds.size() * 7));
        bool first = true;
        for (uint32_t q : questIds)
        {
            if (!first) questList += ',';
            questList += QString::number(q);
            first = false;
        }
        QString const qsql = QStringLiteral(
            "SELECT ID, AllowableRaces FROM quest_template WHERE ID IN (%1)")
            .arg(questList);
        db::QueryResult qres;
        auto const qerr = m_worldDb->query(qsql.toStdString(), qres);
        if (qerr.ok())
        {
            for (size_t r = 0; r < qres.rowCount(); ++r)
            {
                uint32_t const qid  = uint32_t(qres.asUInt64(r, 0).value_or(0));
                uint64_t const mask = qres.asUInt64(r, 1).value_or(0);
                uint8_t  faction = 0;
                if (mask == 0)
                    faction = 0;
                else
                {
                    bool const hasAlli = (mask & kAllianceRaces) != 0;
                    bool const hasHorde = (mask & kHordeRaces)    != 0;
                    if (hasAlli && hasHorde) faction = 3;
                    else if (hasAlli)        faction = 1;
                    else if (hasHorde)       faction = 2;
                    else                     faction = 0;
                }
                factionByQuest[qid] = faction;
            }
        }
    }

    std::vector<render::QuestMarker> markers;
    markers.reserve(byKey.size());
    for (auto& [k, v] : byKey)
    {
        (void)k;
        std::sort(v.startsQuests.begin(), v.startsQuests.end());
        v.startsQuests.erase(std::unique(v.startsQuests.begin(), v.startsQuests.end()),
                             v.startsQuests.end());
        std::sort(v.endsQuests.begin(), v.endsQuests.end());
        v.endsQuests.erase(std::unique(v.endsQuests.begin(), v.endsQuests.end()),
                           v.endsQuests.end());
        // Aggregate faction across this NPC's quests.  A neutral NPC
        // that hosts an alliance-only quest reports alliance; one that
        // hosts both alliance + horde quests reports both (a cross-
        // faction quest hub e.g. Booty Bay).
        uint8_t faction = 0;
        auto accum = [&](uint32_t qid) {
            auto it = factionByQuest.find(qid);
            if (it == factionByQuest.end()) return;
            faction = uint8_t(faction | it->second);
        };
        for (uint32_t q : v.startsQuests) accum(q);
        for (uint32_t q : v.endsQuests)   accum(q);
        v.faction = faction;
        markers.push_back(std::move(v));
    }
    m_viewer->setQuestMarkers(std::move(markers));

    // ---- Quest-objective overlay (the "kill / loot / interact / talk"
    // markers, distinct from the ?/! starter/ender glyphs above).
    //
    // For every quest this map's NPCs are involved in, pull its
    // quest_objective rows, decode Type to a kind-bit, resolve the
    // ObjectID to spawns on this map, and accumulate a marker per
    // (spawnKind, guid).  Type values follow TC's QuestObjectiveType
    // enum (src/server/game/Quests/QuestDef.h):
    //   0 = MONSTER (kill creature), 1 = ITEM (loot drop),
    //   2 = GAMEOBJECT (interact), 3 = TALKTO (creature),
    //   4..  = SKIP (CRITERIA_TREE / AREA / PLAYERKILLS / battle-pet).
    std::vector<render::QuestObjectiveMarker> objectiveMarkers;
    if (!questIds.empty())
    {
        // Stash the per-spawn aggregator inline so it has the lifetimes
        // it needs and doesn't clutter the header.
        struct ObjKey { int64_t guid; uint8_t kind; };
        struct ObjKeyHash
        {
            size_t operator()(std::pair<int64_t, uint8_t> const& k) const noexcept
            {
                return std::hash<int64_t>()(k.first) ^ (size_t(k.second) << 1);
            }
        };
        std::unordered_map<std::pair<int64_t, uint8_t>,
                           render::QuestObjectiveMarker, ObjKeyHash> objByKey;
        // Per-key set of quest ids for the tooltip aggregation.
        std::unordered_map<std::pair<int64_t, uint8_t>,
                           std::vector<uint32_t>, ObjKeyHash> objQuestsByKey;

        QString questList;
        questList.reserve(int(questIds.size() * 7));
        {
            bool first = true;
            for (uint32_t q : questIds)
            {
                if (!first) questList += ',';
                questList += QString::number(q);
                first = false;
            }
        }

        QString const osql = QStringLiteral(
            "SELECT QuestID, Type, ObjectID FROM quest_objective "
            "WHERE QuestID IN (%1)").arg(questList);
        db::QueryResult ores;
        auto const oerr = m_worldDb->query(osql.toStdString(), ores);

        // Per-(kind,objectID) -> kind-bit table; one quest objective
        // may resolve to many spawns of the same template, so we
        // collect template ids and run two batch resolution queries.
        // entryKindBit: 0=kill (creature), 2=interact (GO), 3=talk
        // (creature).  ITEM objectives go through a separate item->
        // template indirection further down.
        // creatureBits[entry] = OR'd kind bits for that creature entry
        // (kill, talk, or gather-via-drop).  Same for GO entries.
        std::unordered_map<uint32_t, uint8_t> creatureBits;
        std::unordered_map<uint32_t, uint8_t> goBits;
        // Per-entry list of quest ids that contributed -- copied to
        // every spawn of that template at resolution time so the
        // tooltip can name the quests.
        std::unordered_map<uint32_t, std::vector<uint32_t>> creatureQuests;
        std::unordered_map<uint32_t, std::vector<uint32_t>> goQuests;
        // Items needing drop resolution (item -> bit-aggregated kinds +
        // quest-id list, though items are always "gather" / bit 1).
        std::unordered_map<uint32_t, std::vector<uint32_t>> itemQuests;

        if (oerr.ok())
        {
            for (size_t r = 0; r < ores.rowCount(); ++r)
            {
                uint32_t const qid    = uint32_t(ores.asUInt64(r, 0).value_or(0));
                int32_t  const type   = int32_t (ores.asInt64 (r, 1).value_or(0));
                int32_t  const objId  = int32_t (ores.asInt64 (r, 2).value_or(0));
                if (objId <= 0)
                    continue;
                switch (type)
                {
                    case 0: // MONSTER -> kill (bit 0) on creature entry.
                        creatureBits[uint32_t(objId)] = uint8_t(creatureBits[uint32_t(objId)] | 0x01);
                        creatureQuests[uint32_t(objId)].push_back(qid);
                        break;
                    case 1: // ITEM -> gather (bit 1); resolved via loot tables.
                        itemQuests[uint32_t(objId)].push_back(qid);
                        break;
                    case 2: // GAMEOBJECT -> interact (bit 2) on GO entry.
                        goBits[uint32_t(objId)] = uint8_t(goBits[uint32_t(objId)] | 0x04);
                        goQuests[uint32_t(objId)].push_back(qid);
                        break;
                    case 3: // TALKTO -> talk (bit 3) on creature entry.
                        creatureBits[uint32_t(objId)] = uint8_t(creatureBits[uint32_t(objId)] | 0x08);
                        creatureQuests[uint32_t(objId)].push_back(qid);
                        break;
                    default:
                        break; // SKIP everything else (criteria-tree, area, player-kills, battle-pet, area-trigger).
                }
            }
        }
        else
        {
            statusBar()->showMessage(tr("Quest-objective scan failed: %1")
                .arg(QString::fromStdString(oerr.message)), 5000);
        }

        // Resolve item drops via loot templates.  We join through
        // creature_loot_template / gameobject_loot_template into the
        // template tables to find which entries drop each item, then
        // fold the result into creatureBits / goBits (bit 1 = gather)
        // and into the per-entry quest-id lists.
        if (!itemQuests.empty())
        {
            QString itemList;
            itemList.reserve(int(itemQuests.size() * 7));
            {
                bool first = true;
                for (auto const& [iid, _] : itemQuests)
                {
                    if (!first) itemList += ',';
                    itemList += QString::number(iid);
                    first = false;
                }
            }
            // creature_loot_template: Entry column = creature_template.lootid.
            QString const csql = QStringLiteral(
                "SELECT lt.Item, ct.entry "
                "FROM creature_loot_template lt "
                "JOIN creature_template ct ON ct.lootid = lt.Entry "
                "WHERE lt.Item IN (%1)").arg(itemList);
            db::QueryResult cres;
            if (m_worldDb->query(csql.toStdString(), cres).ok())
            {
                for (size_t r = 0; r < cres.rowCount(); ++r)
                {
                    uint32_t const itemId = uint32_t(cres.asUInt64(r, 0).value_or(0));
                    uint32_t const entry  = uint32_t(cres.asUInt64(r, 1).value_or(0));
                    if (!entry) continue;
                    creatureBits[entry] = uint8_t(creatureBits[entry] | 0x02);
                    auto it = itemQuests.find(itemId);
                    if (it != itemQuests.end())
                    {
                        auto& dst = creatureQuests[entry];
                        dst.insert(dst.end(), it->second.begin(), it->second.end());
                    }
                }
            }
            // gameobject_loot_template: gameobject_template.Data1 = loot Entry
            // (chests + similar lootable GOs).
            QString const gsql = QStringLiteral(
                "SELECT lt.Item, gt.entry "
                "FROM gameobject_loot_template lt "
                "JOIN gameobject_template gt ON gt.Data1 = lt.Entry "
                "WHERE lt.Item IN (%1)").arg(itemList);
            db::QueryResult gres;
            if (m_worldDb->query(gsql.toStdString(), gres).ok())
            {
                for (size_t r = 0; r < gres.rowCount(); ++r)
                {
                    uint32_t const itemId = uint32_t(gres.asUInt64(r, 0).value_or(0));
                    uint32_t const entry  = uint32_t(gres.asUInt64(r, 1).value_or(0));
                    if (!entry) continue;
                    goBits[entry] = uint8_t(goBits[entry] | 0x02);
                    auto it = itemQuests.find(itemId);
                    if (it != itemQuests.end())
                    {
                        auto& dst = goQuests[entry];
                        dst.insert(dst.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }

        // Resolve entries -> spawn rows on the current map.  Two
        // queries (one per kind); each returns guid + xyz so we can
        // populate the marker directly.
        auto runEntryResolve = [&](QString const& table, uint8_t spawnKind,
                                   std::unordered_map<uint32_t, uint8_t> const& bits,
                                   std::unordered_map<uint32_t, std::vector<uint32_t>> const& quests)
        {
            if (bits.empty()) return;
            QString idList;
            idList.reserve(int(bits.size() * 7));
            {
                bool first = true;
                for (auto const& [e, _] : bits)
                {
                    if (!first) idList += ',';
                    idList += QString::number(e);
                    first = false;
                }
            }
            QString const sql2 = QStringLiteral(
                "SELECT guid, id, position_x, position_y, position_z "
                "FROM %1 WHERE map = %2 AND id IN (%3)")
                .arg(table).arg(mapId).arg(idList);
            db::QueryResult rres;
            auto const rerr = m_worldDb->query(sql2.toStdString(), rres);
            if (!rerr.ok()) return;
            for (size_t r = 0; r < rres.rowCount(); ++r)
            {
                int64_t  const guid  = int64_t (rres.asUInt64(r, 0).value_or(0));
                uint32_t const entry = uint32_t(rres.asUInt64(r, 1).value_or(0));
                float    const x     = float(rres.asDouble(r, 2).value_or(0.0));
                float    const y     = float(rres.asDouble(r, 3).value_or(0.0));
                float    const z     = float(rres.asDouble(r, 4).value_or(0.0));
                auto const k = std::make_pair(guid, spawnKind);
                auto& m = objByKey[k];
                if (m.spawnGuid == 0)
                {
                    m.spawnGuid = guid;
                    m.spawnKind = spawnKind;
                    m.x = x; m.y = y; m.z = z;
                }
                auto bIt = bits.find(entry);
                if (bIt != bits.end())
                    m.kinds = uint8_t(m.kinds | bIt->second);
                auto qIt = quests.find(entry);
                if (qIt != quests.end())
                {
                    auto& dst = objQuestsByKey[k];
                    dst.insert(dst.end(), qIt->second.begin(), qIt->second.end());
                }
            }
        };
        runEntryResolve(QStringLiteral("creature"),   0, creatureBits, creatureQuests);
        runEntryResolve(QStringLiteral("gameobject"), 1, goBits,       goQuests);

        // Flatten the aggregator into the output vector; build the
        // tooltip quest-id string (truncated at 8 + "...").
        objectiveMarkers.reserve(objByKey.size());
        for (auto& [k, mm] : objByKey)
        {
            auto qIt = objQuestsByKey.find(k);
            if (qIt != objQuestsByKey.end())
            {
                auto& qs = qIt->second;
                std::sort(qs.begin(), qs.end());
                qs.erase(std::unique(qs.begin(), qs.end()), qs.end());
                QStringList shown;
                size_t const cap = std::min<size_t>(qs.size(), 8);
                for (size_t i = 0; i < cap; ++i)
                    shown << QString::number(qs[i]);
                if (qs.size() > cap)
                    shown << QStringLiteral("...");
                mm.quests = shown.join(QStringLiteral(", "));
            }
            objectiveMarkers.push_back(std::move(mm));
        }
    }
    m_viewer->setQuestObjectiveMarkers(std::move(objectiveMarkers));

    statusBar()->showMessage(tr("Loaded %1 quest-involved NPC/GO spawns "
                                "(%2 distinct quests).")
        .arg(byKey.size()).arg(questIds.size()), 4000);
}

void MainWindow::applyPhaseFilter()
{
    if (!m_viewer)
        return;
    QSettings s;
    render::NavMeshView::SpawnPhaseFilter f;
    f.enabled    = s.value(SETTINGS_PHASE_FILTER_ENABLED, false).toBool();
    f.phaseId    = uint32_t(s.value(SETTINGS_PHASE_FILTER_ID,    0).toUInt());
    f.phaseGroup = uint32_t(s.value(SETTINGS_PHASE_FILTER_GROUP, 0).toUInt());
    m_viewer->setSpawnPhaseFilter(f);
    // Force a buffer rebuild so the icon set picks up the new filter
    // even if the spawn list itself didn't change.
    pushSpawnsToViewer();
    if (f.enabled)
    {
        size_t const total   = m_viewer->spawns().size();
        size_t const visible = m_viewer->visibleSpawnCount();
        statusBar()->showMessage(
            tr("Phase filter ON: id=%1 group=%2 (%3 of %4 spawns visible)")
                .arg(f.phaseId).arg(f.phaseGroup).arg(visible).arg(total),
            0);
    }
    else
    {
        statusBar()->showMessage(tr("Phase filter OFF"), 2000);
    }
}

namespace
{
// Probe INFORMATION_SCHEMA.COLUMNS for the column set on `table` in the
// connected schema (DATABASE()).  Empty result == table absent or
// permission denied; caller treats both as "fall through to fallback".
// Local copy of the SpellInfoDock pattern -- pulling that helper out to
// a shared header is a refactor we leave for a future cleanup pass.
std::set<std::string, std::less<>> discoverFlightColumns(
    world_editor::db::MySqlClient& db, char const* table)
{
    std::set<std::string, std::less<>> cols;
    std::string const sql =
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) + "'";
    world_editor::db::QueryResult res;
    auto const err = db.query(sql, res);
    if (!err.ok()) return cols;
    for (size_t r = 0; r < res.rowCount(); ++r)
        cols.insert(res.cell(r, 0));
    return cols;
}

// Pick the first column from `candidates` present in `cols`; empty when
// none match.  Mirrors SpellInfoDock::pickColumn but local to keep the
// build graph unchanged for this overlay.
std::string pickFlightColumn(std::set<std::string, std::less<>> const& cols,
                             std::initializer_list<char const*> candidates)
{
    for (char const* c : candidates)
    {
        auto it = cols.find(c);
        if (it != cols.end())
            return *it;
    }
    return {};
}
} // namespace

void MainWindow::loadFlightGraph()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    if (!m_currentMapId.has_value())
    {
        m_viewer->setFlightGraph({}, {});
        return;
    }

    // taxi_nodes is DB2-only in modern TC builds; many shards no longer
    // hotfix the SQL mirror.  Probe INFORMATION_SCHEMA first so absent
    // tables degrade gracefully instead of spamming "table not found".
    auto const nodeCols = discoverFlightColumns(*m_worldDb, "taxi_nodes");
    auto const pathCols = discoverFlightColumns(*m_worldDb, "taxi_path");
    if (nodeCols.empty())
    {
        m_viewer->setFlightGraph({}, {});
        if (!m_flightTablesMissingNoticeShown)
        {
            m_flightTablesMissingNoticeShown = true;
            statusBar()->showMessage(
                tr("Flight path graph: taxi_nodes table not found; layer will be empty (hotfix DB not connected)."),
                6000);
        }
        return;
    }

    // taxi_nodes schema drift: 12.x ships ID/MapID/Pos1..3/Flags, while
    // older hotfix shadows used name + position_x/y/z.  Project the
    // canonical column set positionally, falling back per-field.
    std::string const idCol    = pickFlightColumn(nodeCols, { "ID", "id", "Id" });
    std::string const mapCol   = pickFlightColumn(nodeCols, { "ContinentID", "MapID", "map_id", "map", "MapId" });
    std::string const px       = pickFlightColumn(nodeCols, { "Pos1", "PosX", "x", "position_x" });
    std::string const py       = pickFlightColumn(nodeCols, { "Pos2", "PosY", "y", "position_y" });
    std::string const pz       = pickFlightColumn(nodeCols, { "Pos3", "PosZ", "z", "position_z" });
    std::string const flagsCol = pickFlightColumn(nodeCols, { "Flags", "flags" });
    std::string const nameCol  = pickFlightColumn(nodeCols, { "Name_lang", "Name", "name" });
    if (idCol.empty() || mapCol.empty() || px.empty() || py.empty() || pz.empty())
    {
        m_viewer->setFlightGraph({}, {});
        statusBar()->showMessage(
            tr("Flight path graph: taxi_nodes schema not recognized (missing id / map / position columns)."),
            5000);
        return;
    }

    // Build the SELECT, COALESCE-ing nullable / absent columns so the
    // projection stays positional.
    std::string sql = "SELECT " + idCol + ", " + mapCol + ", "
        + px + ", " + py + ", " + pz + ", "
        + (flagsCol.empty() ? std::string("0") : std::string("COALESCE(") + flagsCol + ",0)") + ", "
        + (nameCol.empty()  ? std::string("''") : std::string("COALESCE(") + nameCol  + ",'')") + " "
        + "FROM taxi_nodes WHERE " + mapCol + " = " + std::to_string(*m_currentMapId);

    db::QueryResult res;
    auto err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        statusBar()->showMessage(
            tr("taxi_nodes query failed: %1").arg(QString::fromStdString(err.message)),
            5000);
        m_viewer->setFlightGraph({}, {});
        return;
    }

    std::vector<render::FlightNode> nodes;
    nodes.reserve(res.rowCount());
    std::vector<uint32_t> nodeIds;
    nodeIds.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::FlightNode n;
        n.id    = uint32_t(res.asUInt64(r, 0).value_or(0));
        n.mapId = uint32_t(res.asUInt64(r, 1).value_or(*m_currentMapId));
        n.x     = float(res.asDouble(r, 2).value_or(0.0));
        n.y     = float(res.asDouble(r, 3).value_or(0.0));
        n.z     = float(res.asDouble(r, 4).value_or(0.0));
        n.flags = uint32_t(res.asUInt64(r, 5).value_or(0));
        if (!res.isNull(r, 6))
            n.name = QString::fromStdString(res.cell(r, 6));
        nodes.push_back(std::move(n));
        nodeIds.push_back(n.id);
    }

    std::vector<render::FlightEdge> edges;
    if (!pathCols.empty() && !nodeIds.empty())
    {
        // taxi_path links nodes via FromTaxiNode -> ToTaxiNode in modern
        // builds; older snapshots used from_node / to_node.  Probe to
        // pick the right pair before we issue the IN-list query.
        std::string const fromCol = pickFlightColumn(pathCols,
            { "FromTaxiNode", "from_node", "fromNode", "From" });
        std::string const toCol   = pickFlightColumn(pathCols,
            { "ToTaxiNode",   "to_node",   "toNode",   "To" });
        if (!fromCol.empty() && !toCol.empty())
        {
            // Cap the IN-list at 4000 ids defensively to stay well clear
            // of the libmysql packet ceiling; flight-node sets per map
            // top out in the low hundreds in retail so this never trips.
            std::string idList;
            idList.reserve(nodeIds.size() * 8);
            for (size_t i = 0; i < nodeIds.size() && i < 4000; ++i)
            {
                if (i > 0) idList.push_back(',');
                idList += std::to_string(nodeIds[i]);
            }
            std::string const pathSql =
                "SELECT " + fromCol + ", " + toCol + " FROM taxi_path "
                "WHERE " + fromCol + " IN (" + idList + ") "
                "AND "  + toCol   + " IN (" + idList + ")";
            db::QueryResult pathRes;
            err = m_worldDb->query(pathSql, pathRes);
            if (err.ok())
            {
                edges.reserve(pathRes.rowCount());
                for (size_t r = 0; r < pathRes.rowCount(); ++r)
                {
                    render::FlightEdge e;
                    e.fromId = uint32_t(pathRes.asUInt64(r, 0).value_or(0));
                    e.toId   = uint32_t(pathRes.asUInt64(r, 1).value_or(0));
                    if (e.fromId && e.toId)
                        edges.push_back(e);
                }
            }
            else
            {
                statusBar()->showMessage(
                    tr("taxi_path query failed: %1").arg(QString::fromStdString(err.message)),
                    5000);
            }
        }
    }

    size_t const edgeCount = edges.size();
    size_t const nodeCount = nodes.size();
    m_viewer->setFlightGraph(std::move(nodes), std::move(edges));
    statusBar()->showMessage(
        tr("Flight path graph: %1 nodes, %2 edges loaded.").arg(nodeCount).arg(edgeCount),
        3000);
}

void MainWindow::loadTransportRoutes()
{
    // Transport routes: zeppelins/boats follow waypoint paths stored in
    // transports + transport_animation.  Schema varies (some forks rename
    // to transport_template + transport_keyframes); we probe both and
    // fall back to raw keyframe positions when no spawn-position lookup
    // is available.  Silent no-op when neither schema is present.
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    if (!m_currentMapId.has_value())
    {
        m_viewer->setTransportRoutes({});
        return;
    }

    auto const transportCols = discoverFlightColumns(*m_worldDb, "transports");
    auto const altTransportCols = transportCols.empty()
        ? discoverFlightColumns(*m_worldDb, "transport_template")
        : std::set<std::string, std::less<>>{};
    auto const animCols  = discoverFlightColumns(*m_worldDb, "transport_animation");
    auto const kfCols    = animCols.empty()
        ? discoverFlightColumns(*m_worldDb, "transport_keyframes")
        : std::set<std::string, std::less<>>{};

    bool const haveTransports = !transportCols.empty() || !altTransportCols.empty();
    bool const haveAnim       = !animCols.empty() || !kfCols.empty();
    if (!haveTransports || !haveAnim)
    {
        // Silent empty layer per the task spec when tables are missing.
        m_viewer->setTransportRoutes({});
        return;
    }

    // Resolve table + column names per probed schema.
    std::string const transportTable = !transportCols.empty() ? "transports" : "transport_template";
    auto const&        tCols         = !transportCols.empty() ? transportCols : altTransportCols;
    std::string const tEntryCol = pickFlightColumn(tCols,
        { "GameObjectId", "entry", "Entry", "Id", "ID" });
    if (tEntryCol.empty())
    {
        m_viewer->setTransportRoutes({});
        return;
    }

    std::string const animTable    = !animCols.empty() ? "transport_animation" : "transport_keyframes";
    auto const&        aCols       = !animCols.empty() ? animCols : kfCols;
    std::string const aEntryCol    = pickFlightColumn(aCols,
        { "TransportEntry", "transport_entry", "TransportId", "transport_id", "Entry", "entry" });
    std::string const aSeqCol      = pickFlightColumn(aCols,
        { "SeqTime", "seq_time", "TimeSeq", "time", "Time", "Index", "idx" });
    std::string const aXCol        = pickFlightColumn(aCols, { "X", "x", "PosX", "pos_x" });
    std::string const aYCol        = pickFlightColumn(aCols, { "Y", "y", "PosY", "pos_y" });
    std::string const aZCol        = pickFlightColumn(aCols, { "Z", "z", "PosZ", "pos_z" });
    if (aEntryCol.empty() || aXCol.empty() || aYCol.empty() || aZCol.empty())
    {
        m_viewer->setTransportRoutes({});
        return;
    }

    // Join the two tables and order by (entry, seq) so we can group
    // consecutive rows into one polyline per transport.  Ordering by
    // entry first guarantees the grouping loop below sees contiguous
    // runs even when SeqTime would otherwise interleave entries.
    std::string sql = "SELECT t." + tEntryCol + ", "
        + "a." + aXCol + ", a." + aYCol + ", a." + aZCol + " "
        + "FROM " + transportTable + " t "
        + "JOIN " + animTable + " a ON a." + aEntryCol + " = t." + tEntryCol + " "
        + "ORDER BY t." + tEntryCol;
    if (!aSeqCol.empty())
        sql += ", a." + aSeqCol;

    db::QueryResult res;
    auto const err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        statusBar()->showMessage(
            tr("Transport routes query failed: %1")
                .arg(QString::fromStdString(err.message)),
            5000);
        m_viewer->setTransportRoutes({});
        return;
    }

    // Probe transports for an optional MapID column so we can filter to
    // the current map when present; many schemas omit it (transports are
    // truly cross-map by design) in which case we render every route the
    // editor knows about as a global hint.
    std::string const tMapCol = pickFlightColumn(tCols,
        { "MapID", "map", "Map", "map_id", "MapId" });
    std::unordered_set<uint32_t> mapFilter;
    if (!tMapCol.empty())
    {
        std::string const mapSql = "SELECT " + tEntryCol + " FROM " + transportTable
            + " WHERE " + tMapCol + " = " + std::to_string(*m_currentMapId);
        db::QueryResult mapRes;
        if (m_worldDb->query(mapSql, mapRes).ok())
        {
            mapFilter.reserve(mapRes.rowCount());
            for (size_t r = 0; r < mapRes.rowCount(); ++r)
                mapFilter.insert(uint32_t(mapRes.asUInt64(r, 0).value_or(0)));
        }
    }

    // Group rows into one route per transport entry.  Keyframes are
    // stored relative to the transport's spawn position; absent a robust
    // spawn-position lookup (the operator may not have gameobject rows
    // for the transport on this map) we plot the raw keyframes -- the
    // overlay is opt-in and approximate, which the operator knows.
    std::vector<std::vector<coords::WorldPos>> routes;
    std::vector<coords::WorldPos> currentRoute;
    uint32_t currentEntry = 0;
    bool currentMatchesMap = mapFilter.empty();
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const entry = uint32_t(res.asUInt64(r, 0).value_or(0));
        if (entry != currentEntry)
        {
            if (currentMatchesMap && currentRoute.size() >= 2)
                routes.push_back(std::move(currentRoute));
            currentRoute.clear();
            currentEntry = entry;
            currentMatchesMap = mapFilter.empty() || mapFilter.count(entry) > 0;
        }
        if (!currentMatchesMap)
            continue;
        coords::WorldPos p{};
        p.x = float(res.asDouble(r, 1).value_or(0.0));
        p.y = float(res.asDouble(r, 2).value_or(0.0));
        p.z = float(res.asDouble(r, 3).value_or(0.0));
        currentRoute.push_back(p);
    }
    if (currentMatchesMap && currentRoute.size() >= 2)
        routes.push_back(std::move(currentRoute));

    size_t const routeCount = routes.size();
    m_viewer->setTransportRoutes(std::move(routes));
    statusBar()->showMessage(
        tr("Transport routes: %1 polylines loaded.").arg(routeCount),
        3000);
}

void MainWindow::loadGatheringNodes()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    if (!m_currentMapId.has_value())
    {
        m_viewer->setGatheringNodes({});
        return;
    }

    // Pull every GO spawn on the current map joined with its template's
    // type + name.  Heuristics applied client-side (cheaper than coding
    // them into SQL CASE arms + decouples us from gameobject_template
    // schema drift across TC branches).
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT g.guid, gt.type, gt.name "
        "FROM gameobject g "
        "JOIN gameobject_template gt ON gt.entry = g.id "
        "WHERE g.map = %u",
        *m_currentMapId);

    db::QueryResult res;
    db::QueryError const err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        statusBar()->showMessage(
            tr("Gathering nodes query failed: %1").arg(QString::fromStdString(err.message)),
            5000);
        m_viewer->setGatheringNodes({});
        return;
    }

    // Name-pattern classifier (case-insensitive substring match).  Order:
    // mining > treasure > herb so a "Treasure Chest of Ore" lands in
    // treasure (the more specific bucket), and herb keywords come last
    // because "Bloom" / "Lotus" etc are common in flavor names.
    static constexpr char const* kMiningTokens[] = {
        "Vein", "Ore", "Deposit"
    };
    static constexpr char const* kHerbTokens[] = {
        "Bloom", "Lotus", "Root", "Bush", "Grass", "Petal",
        "Sprout", "Plant", "Flower", "Spores"
    };
    static constexpr char const* kTreasureTokens[] = {
        "Treasure", "Cache", "Chest"
    };

    auto containsAny = [](QString const& haystack, char const* const* tokens, size_t count) -> bool
    {
        for (size_t i = 0; i < count; ++i)
            if (haystack.contains(QLatin1String(tokens[i]), Qt::CaseInsensitive))
                return true;
        return false;
    };

    std::unordered_map<int64_t, uint8_t> kindByGuid;
    kindByGuid.reserve(res.rowCount());
    size_t mining = 0, herb = 0, fishing = 0, treasure = 0;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int64_t const guid = res.asInt64(r, 0).value_or(0);
        if (guid == 0)
            continue;
        uint32_t const type = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));
        QString const name  = QString::fromStdString(res.cell(r, 2));

        // Fishing first: GO type 26 is FISHINGHOLE (TC GameObjectType),
        // unambiguous regardless of name.
        if (type == 26)
        {
            kindByGuid.emplace(guid, uint8_t(2));
            ++fishing;
            continue;
        }

        // Mining beats other buckets when an ore-related token is present
        // (e.g. "Iron Deposit").
        if (containsAny(name, kMiningTokens, sizeof(kMiningTokens)/sizeof(kMiningTokens[0])))
        {
            kindByGuid.emplace(guid, uint8_t(0));
            ++mining;
            continue;
        }

        // Treasure: type 25 is GAMEOBJECT_TYPE_CHEST (TC); we accept
        // type 25 OR type 12 (GOOBER, often used for quest treasure) when
        // the name contains a treasure token.  Plain "Chest" is enough.
        if ((type == 25 || type == 12) && containsAny(name, kTreasureTokens, sizeof(kTreasureTokens)/sizeof(kTreasureTokens[0])))
        {
            kindByGuid.emplace(guid, uint8_t(3));
            ++treasure;
            continue;
        }

        // Herb last: matched on flavor tokens only.  type isn't a reliable
        // gate (herb nodes also use GAMEOBJECT_TYPE_CHEST=25 internally).
        if (containsAny(name, kHerbTokens, sizeof(kHerbTokens)/sizeof(kHerbTokens[0])))
        {
            kindByGuid.emplace(guid, uint8_t(1));
            ++herb;
            continue;
        }
    }

    size_t const total = kindByGuid.size();
    m_viewer->setGatheringNodes(std::move(kindByGuid));
    statusBar()->showMessage(
        tr("Gathering nodes: %1 total (mining=%2 herb=%3 fishing=%4 treasure=%5).")
            .arg(total).arg(mining).arg(herb).arg(fishing).arg(treasure),
        3000);
}

void MainWindow::pushWmoFootprintsToViewer(bool layerOn)
{
    if (!m_viewer)
        return;
    if (!layerOn)
    {
        // Cheap: drop the geometry from the viewer; the cache stays warm
        // so re-enable on the same map is a single vector copy.
        m_viewer->setWmoFootprints({});
        m_wmoFootprintsPushed = false;
        return;
    }
    if (m_wmoFootprintsCache.empty())
    {
        m_viewer->setWmoFootprints({});
        // One-time hint nudging the operator to load vmaps.  Avoid spamming
        // the status bar across map switches if they've already seen it.
        if (!m_wmoFootprintsEmptyNoticeShown)
        {
            statusBar()->showMessage(
                tr("WMO footprints empty: load a vmaps directory first "
                   "(File -> Set vmaps directory)"),
                6000);
            m_wmoFootprintsEmptyNoticeShown = true;
        }
        m_wmoFootprintsPushed = false;
        return;
    }
    // Build the tuple list lazily on first push for this cache; cheap O(N)
    // copy over the saved AABBs (a populated continent caps around a few
    // thousand WMO instances even with the higher kVmapTileCap).
    std::vector<std::tuple<float, float, float, float>> footprints;
    footprints.reserve(m_wmoFootprintsCache.size());
    for (io::WmoInstanceAabb const& a : m_wmoFootprintsCache)
        footprints.emplace_back(a.minX, a.maxX, a.minY, a.maxY);
    m_viewer->setWmoFootprints(std::move(footprints));
    m_wmoFootprintsPushed = true;
}

void MainWindow::loadInstanceEntrances()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    if (!m_currentMapId.has_value())
    {
        m_viewer->setInstanceEntrances({});
        return;
    }

    // areatrigger_teleport schema varies across TC builds:
    //   - Modern TC: ID + PortLocID + Name; target map resolved via
    //     world_safe_locs (PortLocID -> world_safe_locs.ID -> MapID).
    //   - Older forks: areatrigger_teleport carries target_map / Target_Map_Id
    //     directly.
    //   - Some shards expose areatrigger_teleport_dbc as a DB2 mirror.
    // Probe INFORMATION_SCHEMA so the query degrades gracefully when the
    // expected columns / tables are absent (returns empty overlay).
    auto colsFor = [&](char const* table) {
        std::set<std::string, std::less<>> cols;
        std::string const probe =
            "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) + "'";
        db::QueryResult r;
        if (m_worldDb->query(probe, r).ok())
            for (size_t i = 0; i < r.rowCount(); ++i)
                cols.insert(r.cell(i, 0));
        return cols;
    };
    auto pick = [](std::set<std::string, std::less<>> const& cols,
                   std::initializer_list<char const*> cands) -> std::string {
        for (char const* c : cands)
            if (cols.find(c) != cols.end())
                return c;
        return {};
    };

    // Try areatrigger_teleport first; fall back to areatrigger_teleport_dbc.
    std::string atrTeleport = "areatrigger_teleport";
    auto teleCols = colsFor(atrTeleport.c_str());
    if (teleCols.empty())
    {
        atrTeleport = "areatrigger_teleport_dbc";
        teleCols = colsFor(atrTeleport.c_str());
    }
    if (teleCols.empty())
    {
        m_viewer->setInstanceEntrances({});
        return;
    }

    std::string const teleIdCol = pick(teleCols, { "ID", "id", "Id" });
    std::string const teleTargetCol = pick(teleCols,
        { "Target_Map_Id", "target_map", "TargetMapId", "target_map_id" });
    std::string const telePortLocCol = pick(teleCols, { "PortLocID", "PortLocId", "port_loc_id" });
    std::string const teleNameCol = pick(teleCols, { "Name", "name" });
    if (teleIdCol.empty())
    {
        m_viewer->setInstanceEntrances({});
        return;
    }

    // Build the projection.  When the target column lives on the teleport
    // table directly we read it; otherwise we join through world_safe_locs.
    std::string const wslCols = telePortLocCol.empty()
        ? std::string()
        : "world_safe_locs";
    bool const useDirectTarget = !teleTargetCol.empty();
    bool const useSafeLocsHop  = !useDirectTarget && !telePortLocCol.empty();

    if (!useDirectTarget && !useSafeLocsHop)
    {
        // Neither hop is wired -- nothing we can resolve.
        m_viewer->setInstanceEntrances({});
        return;
    }

    std::string sql;
    sql.reserve(1024);
    sql += "SELECT a.SpawnId, a.PosX, a.PosY, ";
    if (useDirectTarget)
        sql += "atct." + teleTargetCol + ", ";
    else
        sql += "wsl.MapID, ";
    if (!teleNameCol.empty())
        sql += "COALESCE(atct." + teleNameCol + ", '') ";
    else
        sql += "'' ";
    sql += "FROM areatrigger a ";
    sql += "JOIN " + atrTeleport + " atct ON atct." + teleIdCol + " = a.SpawnId ";
    if (useSafeLocsHop)
        sql += "JOIN world_safe_locs wsl ON wsl.ID = atct." + telePortLocCol + " ";
    sql += "WHERE a.MapId = " + std::to_string(*m_currentMapId);
    if (useDirectTarget)
        sql += " AND atct." + teleTargetCol + " IS NOT NULL";
    sql += " LIMIT 1000";

    db::QueryResult res;
    auto const err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        statusBar()->showMessage(
            tr("Instance entrances query failed: %1")
                .arg(QString::fromStdString(err.message)), 4000);
        m_viewer->setInstanceEntrances({});
        return;
    }

    std::vector<render::NavMeshView::InstanceEntrance> entries;
    entries.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::NavMeshView::InstanceEntrance e;
        e.spawnId     = res.asInt64(r, 0).value_or(0);
        e.x           = float(res.asDouble(r, 1).value_or(0.0));
        e.y           = float(res.asDouble(r, 2).value_or(0.0));
        e.targetMapId = uint32_t(res.asUInt64(r, 3).value_or(0));
        QString dbName = QString::fromStdString(res.cell(r, 4));
        // Prefer the Map.db2 directory name when available -- gives a
        // stable label that matches what the operator sees elsewhere.
        if (m_mapDb2)
        {
            if (auto dir = m_mapDb2->directoryFor(e.targetMapId); dir && !dir->empty())
                e.name = QString::fromStdString(*dir);
        }
        if (e.name.isEmpty())
            e.name = dbName.isEmpty() ? QStringLiteral("map %1").arg(e.targetMapId) : dbName;
        if (e.targetMapId == 0)
            continue;
        entries.push_back(std::move(e));
    }

    size_t const count = entries.size();
    m_viewer->setInstanceEntrances(std::move(entries));
    statusBar()->showMessage(
        tr("Instance entrances: %1 portal(s) on map %2.")
            .arg(count).arg(*m_currentMapId),
        3000);
}

void MainWindow::loadSpawnLinks()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    if (!m_currentMapId.has_value())
    {
        m_viewer->setSpawnLinks({});
        return;
    }

    // Probe linked_respawn column names: the canonical TC schema uses
    // (guid, linkedGuid) BIGINT signed, but a few forks rename to
    // (spawnId, linkedSpawnId).  INFORMATION_SCHEMA probe so we silently
    // degrade on schema drift.
    auto colsFor = [&](char const* table) {
        std::set<std::string, std::less<>> cols;
        std::string const probe =
            "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) + "'";
        db::QueryResult r;
        if (m_worldDb->query(probe, r).ok())
            for (size_t i = 0; i < r.rowCount(); ++i)
                cols.insert(r.cell(i, 0));
        return cols;
    };
    auto pick = [](std::set<std::string, std::less<>> const& cols,
                   std::initializer_list<char const*> cands) -> std::string {
        for (char const* c : cands)
            if (cols.find(c) != cols.end())
                return c;
        return {};
    };

    auto cols = colsFor("linked_respawn");
    if (cols.empty())
    {
        m_viewer->setSpawnLinks({});
        return;
    }
    std::string const fromCol = pick(cols, { "guid", "spawnId", "spawn_id" });
    std::string const toCol   = pick(cols, { "linkedGuid", "linkedSpawnId", "linked_spawn_id", "linked_guid" });
    if (fromCol.empty() || toCol.empty())
    {
        m_viewer->setSpawnLinks({});
        return;
    }

    // Restrict to rows whose BOTH endpoints live on the current map.  TC's
    // linked_respawn semantics permit cross-map only for transports, which
    // the 2D viewer cannot draw anyway.  Run the join unconditionally and
    // fall back to empty on error -- some shards rename `creature` or use
    // signed BIGINTs in unexpected ways.
    std::string sql;
    sql.reserve(512);
    sql += "SELECT lr." + fromCol + ", lr." + toCol + " FROM linked_respawn lr ";
    sql += "JOIN creature c1 ON c1.guid = lr." + fromCol + " AND c1.map = "
        + std::to_string(*m_currentMapId) + " ";
    sql += "JOIN creature c2 ON c2.guid = lr." + toCol   + " AND c2.map = "
        + std::to_string(*m_currentMapId) + " ";
    sql += "LIMIT 5000";

    db::QueryResult res;
    auto const err = m_worldDb->query(sql, res);
    if (!err.ok())
    {
        // Silent empty on error -- schema drift is the most common cause.
        m_viewer->setSpawnLinks({});
        return;
    }

    std::vector<std::pair<int64_t, int64_t>> links;
    links.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int64_t const from = res.asInt64(r, 0).value_or(0);
        int64_t const to   = res.asInt64(r, 1).value_or(0);
        if (from == 0 || to == 0 || from == to)
            continue;
        links.emplace_back(from, to);
    }

    size_t const count = links.size();
    m_viewer->setSpawnLinks(std::move(links));
    statusBar()->showMessage(
        tr("Spawn links: %1 dependency edge(s) on map %2.")
            .arg(count).arg(*m_currentMapId),
        3000);
}

void MainWindow::loadSpawnGroupColors()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;

    // Pull spawn_group rows scoped to the current map's spawns.  When
    // no map is loaded, fetch the global table so the toggle works
    // before opening a map.  We join against creature.guid to scope:
    //   spawn_group.spawnType=0 -> creature; spawnType=1 -> gameobject.
    // The viewer carries both kinds; we tint either.
    QString const sql = m_currentMapId.has_value()
        ? QStringLiteral(
            "SELECT sg.groupId, sg.spawnType, sg.spawnId FROM spawn_group sg "
            "LEFT JOIN creature c ON sg.spawnType = 0 AND c.guid = sg.spawnId "
            "LEFT JOIN gameobject g ON sg.spawnType = 1 AND g.guid = sg.spawnId "
            "WHERE COALESCE(c.map, g.map) = %1").arg(*m_currentMapId)
        : QStringLiteral(
            "SELECT groupId, spawnType, spawnId FROM spawn_group");

    db::QueryResult res;
    auto const err = m_worldDb->query(sql.toStdString(), res);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("spawn_group query failed: %1")
            .arg(QString::fromStdString(err.message)), 5000);
        return;
    }

    // Deterministic group-id -> packed RGBA via a simple hash.  We pick
    // saturated colors in HSV space so each group is visually distinct.
    auto groupColor = [](uint32_t groupId) -> uint32_t {
        // golden-ratio hue progression keeps adjacent group ids visually
        // distant (a property of phi mod 1).
        constexpr double kPhi = 0.61803398875;
        double const hue = std::fmod(double(groupId) * kPhi, 1.0);
        // HSV -> RGB at saturation 0.7, value 0.95.
        double const s = 0.70, v = 0.95;
        double const h6 = hue * 6.0;
        int    const i  = int(std::floor(h6)) % 6;
        double const f  = h6 - std::floor(h6);
        double const p  = v * (1.0 - s);
        double const q  = v * (1.0 - s * f);
        double const t  = v * (1.0 - s * (1.0 - f));
        double rd = 0.0, gd = 0.0, bd = 0.0;
        switch (i)
        {
            case 0: rd=v;  gd=t;  bd=p;  break;
            case 1: rd=q;  gd=v;  bd=p;  break;
            case 2: rd=p;  gd=v;  bd=t;  break;
            case 3: rd=p;  gd=q;  bd=v;  break;
            case 4: rd=t;  gd=p;  bd=v;  break;
            case 5: rd=v;  gd=p;  bd=q;  break;
        }
        uint8_t const R = uint8_t(std::clamp(rd, 0.0, 1.0) * 255.0);
        uint8_t const G = uint8_t(std::clamp(gd, 0.0, 1.0) * 255.0);
        uint8_t const B = uint8_t(std::clamp(bd, 0.0, 1.0) * 255.0);
        // packed RGBA = AABBGGRR (matches NavMeshView extraction).
        return uint32_t(R) | (uint32_t(G) << 8) | (uint32_t(B) << 16) | (0xFFu << 24);
    };

    std::unordered_map<int64_t, uint32_t> colors;
    colors.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const groupId   = uint32_t(res.asUInt64(r, 0).value_or(0));
        uint64_t const spawnType = res.asUInt64(r, 1).value_or(0);
        int64_t  const spawnId   = int64_t(res.asUInt64(r, 2).value_or(0));
        // creature.guid range is positive; gameobject.guid is also positive
        // but they live in separate tables.  SpawnModel stores both via the
        // SpawnKind enum; the guid alone uniquely identifies a row WITHIN
        // its kind.  Two spawns with the same guid (one creature, one GO)
        // would collide -- we accept this since collisions across the kind
        // boundary are vanishingly rare in practice.
        (void)spawnType;
        colors[spawnId] = groupColor(groupId);
    }
    m_viewer->setSpawnGroupColors(std::move(colors));
    statusBar()->showMessage(
        tr("Loaded %1 spawn_group entries for tint.").arg(res.rowCount()), 4000);
}

void MainWindow::loadFactionTintMap()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;

    // Fallback classifier over the small set of TC base factions we
    // expect to see on most spawns.  Returns 0..4 per the FactionTint
    // contract (Alliance / Horde / Sanctuary / Contested / Other).
    auto classifyFallback = [](uint32_t faction) -> uint8_t {
        switch (faction)
        {
            // Alliance capitals: Stormwind, Ironforge, Gnomeregan,
            // Darnassus, Exodar, plus generic Alliance grunts.
            case 7: case 29: case 55: case 68: case 79: case 80:
            case 1734: case 1735: case 1745: case 1791: case 1801:
                return 0;
            // Horde capitals: Orgrimmar, Darkspear, Thunder Bluff,
            // Undercity, Bilgewater + generic Horde grunts.
            case 21: case 71: case 72: case 85: case 88: case 92:
            case 1604: case 1610: case 1690: case 1738: case 1771:
                return 1;
            // Sanctuary (Shattrath, Dalaran).
            case 32: case 35: case 188: case 1909: case 1075: case 1077:
                return 2;
            // Contested (PvP-flagged neutral) - 188 historically straddled
            // sanctuary/contested; we keep the modern Contested-Guard value.
            case 14: case 16: case 250: case 1881: case 2007:
                return 3;
            default:
                return 4; // Other / Neutral
        }
    };

    // Scope the query to the entries actually present on this map so we
    // don't drag the whole creature_template through the wire.  When no
    // map is loaded we just give up -- the viewer carries no spawns to
    // tint anyway.
    std::unordered_map<uint32_t, uint8_t> tintMap;
    if (!m_currentMapId.has_value())
    {
        m_viewer->setFactionTintMap({});
        return;
    }

    // First try: join creature_template -> faction_template_dbc so we
    // get the authoritative FactionGroup column.  Older DB dumps may
    // lack faction_template_dbc; in that case the query fails and we
    // fall back to the hard-coded classifier below.
    QString const sqlJoin = QStringLiteral(
        "SELECT ct.entry, ft.FactionGroup, ct.faction "
        "FROM creature_template ct "
        "LEFT JOIN faction_template_dbc ft ON ft.ID = ct.faction "
        "WHERE ct.entry IN (SELECT DISTINCT id FROM creature WHERE map = %1)")
        .arg(*m_currentMapId);

    db::QueryResult res;
    db::QueryError err = m_worldDb->query(sqlJoin.toStdString(), res);
    bool haveDbc = err.ok();
    if (!haveDbc)
    {
        // Fallback: pull just creature_template.faction and classify.
        QString const sqlBare = QStringLiteral(
            "SELECT ct.entry, NULL AS FactionGroup, ct.faction "
            "FROM creature_template ct "
            "WHERE ct.entry IN (SELECT DISTINCT id FROM creature WHERE map = %1)")
            .arg(*m_currentMapId);
        err = m_worldDb->query(sqlBare.toStdString(), res);
        if (!err.ok())
        {
            statusBar()->showMessage(tr("faction tint query failed: %1")
                .arg(QString::fromStdString(err.message)), 5000);
            return;
        }
    }

    tintMap.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const entry   = uint32_t(res.asUInt64(r, 0).value_or(0));
        auto const dbcGroupOpt = haveDbc ? res.asUInt64(r, 1) : std::optional<uint64_t>{};
        uint32_t const faction = uint32_t(res.asUInt64(r, 2).value_or(0));
        uint8_t group = 4;
        if (dbcGroupOpt.has_value())
        {
            // faction_template_dbc.FactionGroup is a bitmask in TC: bit
            // 0 = Player Horde, bit 1 = Player Alliance, bit 2 = Monster,
            // bit 3 = Player Sanctuary.  Map to our 0..4 scheme.
            uint64_t const fg = *dbcGroupOpt;
            if      (fg & 0x8) group = 2;       // Sanctuary
            else if (fg & 0x2) group = 0;       // Alliance
            else if (fg & 0x1) group = 1;       // Horde
            else if (fg == 0)  group = 3;       // Contested (no friendlies)
            else               group = classifyFallback(faction);
        }
        else
        {
            group = classifyFallback(faction);
        }
        tintMap[entry] = group;
    }

    m_viewer->setFactionTintMap(std::move(tintMap));
    statusBar()->showMessage(
        tr("Loaded faction tint for %1 entries (%2).")
            .arg(res.rowCount())
            .arg(haveDbc ? tr("via faction_template_dbc") : tr("fallback classifier")),
        4000);
}

void MainWindow::loadLevelMap()
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_viewer)
        return;
    // Scope to entries actually present on this map - same shape as
    // loadFactionTintMap (no point pulling the global creature_template).
    if (!m_currentMapId.has_value())
    {
        m_viewer->setLevelMap({});
        return;
    }
    QString const sql = QStringLiteral(
        "SELECT ct.entry, ct.minlevel, ct.maxlevel "
        "FROM creature_template ct "
        "WHERE ct.entry IN (SELECT DISTINCT id FROM creature WHERE map = %1)")
        .arg(*m_currentMapId);
    db::QueryResult res;
    db::QueryError err = m_worldDb->query(sql.toStdString(), res);
    if (!err.ok())
    {
        statusBar()->showMessage(tr("level heatmap query failed: %1")
            .arg(QString::fromStdString(err.message)), 5000);
        return;
    }
    std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>> levelMap;
    levelMap.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const entry = uint32_t(res.asUInt64(r, 0).value_or(0));
        // TC stores boss/world-boss as int8 -1; the unsigned getter
        // returns the bit-cast (255 or similar) for those rows.  We
        // canonicalize anything outside the 1..255 sane band to 0 so
        // the viewer's "midLevel<=0 -> magenta" rule fires.
        auto const minV = res.asInt64(r, 1).value_or(0);
        auto const maxV = res.asInt64(r, 2).value_or(0);
        uint16_t const minL = (minV >= 1 && minV <= 255) ? uint16_t(minV) : 0;
        uint16_t const maxL = (maxV >= 1 && maxV <= 255) ? uint16_t(maxV) : 0;
        levelMap[entry] = { minL, maxL };
    }
    size_t const rowCount = res.rowCount();
    m_viewer->setLevelMap(std::move(levelMap));
    statusBar()->showMessage(
        tr("Loaded level heatmap for %1 entries.").arg(rowCount), 4000);
}

void MainWindow::onPlayPathIn3D()
{
    if (!m_waypointModel || m_selectedPathIndex < 0)
    {
        statusBar()->showMessage(tr("Select a path first - click any path "
            "polyline in the 2D view."), 3000);
        return;
    }
    render::Path const& p = m_waypointModel->current()[m_selectedPathIndex];
    if (p.nodes.size() < 2)
    {
        statusBar()->showMessage(tr("Path %1 has fewer than 2 nodes - cannot play.")
            .arg(p.pathId), 3000);
        return;
    }
    if (!m_viewer3d || !m_centralStack)
    {
        statusBar()->showMessage(tr("3D view is not available."), 3000);
        return;
    }
    // Switch to the 3D view first so the operator sees the playback.
    ensureViewer3dMeshLoaded();
    m_centralStack->setCurrentWidget(m_viewer3d);
    m_viewer3d->setFocus();
    m_viewer3d->startPathPlayback(p);
    statusBar()->showMessage(tr("Playing path %1 (%2 nodes) at %3 y/s.  Esc to stop.")
        .arg(p.pathId)
        .arg(p.nodes.size())
        .arg(p.velocity > 0.0f ? p.velocity : 5.0f, 0, 'f', 1), 5000);
}

void MainWindow::onShowSmartScriptFlow()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before opening the SAI flow viewer."));
        return;
    }
    // Seed with whatever's currently selected: a spawn (entryorguid =
    // -guid for per-spawn SAI, or +entry for template-level SAI) or a
    // freshly-prompted value when nothing is selected.
    int64_t initialEoid = 0;
    uint8_t initialSrc  = 0;
    if (m_selectedSpawnIndex >= 0 && m_spawnModel
        && m_selectedSpawnIndex < int(m_spawnModel->current().size()))
    {
        render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
        // Per-spawn SAI uses entryorguid = -guid.  Try that first; the
        // operator can flip the spinbox to the template entry if no
        // rows match.
        initialEoid = -int64_t(s.guid);
        initialSrc  = (s.kind == render::SpawnKind::Creature) ? 0 : 1;
    }
    auto* dlg = new app::SmartScriptFlowDialog(m_worldDb.get(),
                                               initialEoid, initialSrc, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    // Spell-info dock auto-populates on rule-selection-change when the
    // selected action references a spell id (action_type 11/15/28/...).
    connect(dlg, &app::SmartScriptFlowDialog::spellReferenced, this,
            [this](uint32_t spellId) {
        if (m_spellDock)
        {
            if (spellId == 0) m_spellDock->clear();
            else              m_spellDock->setSpell(spellId);
        }
    });
    connect(dlg, &app::SmartScriptFlowDialog::editRequested, this,
            [this](qlonglong eoid, int srcType, int id, int link) {
        onEditSmartScript(eoid, srcType, id, link);
        // Also scope the Conditions dock to this SAI rule so the
        // operator can see condition rows gating it without leaving
        // the flow viewer.  This is the "why isn't this firing?" path.
        if (m_conditionsDock)
        {
            m_conditionsDock->setSmartScriptScope(int64_t(eoid),
                uint16_t(id), uint8_t(srcType));
            (void)link;
        }
    });
    dlg->show();
}

void MainWindow::onShowSmartScriptDryRun()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before opening the SAI dry-run trace."));
        return;
    }
    // Seed defaults from the current spawn selection when possible (same
    // convention as the flow viewer: per-spawn SAI uses entryorguid = -guid).
    int64_t defaultEoid = 0;
    int defaultSrc = 0;
    if (m_selectedSpawnIndex >= 0 && m_spawnModel
        && m_selectedSpawnIndex < int(m_spawnModel->current().size()))
    {
        render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
        defaultEoid = -int64_t(s.guid);
        defaultSrc  = (s.kind == render::SpawnKind::Creature) ? 0 : 1;
    }

    bool ok = false;
    qlonglong const eoid = QInputDialog::getInt(this,
        tr("Smart-script dry-run"),
        tr("entryorguid (negative = per-spawn guid, positive = template entry):"),
        /*value=*/int(defaultEoid),
        /*min=*/std::numeric_limits<int>::min(),
        /*max=*/std::numeric_limits<int>::max(),
        /*step=*/1, &ok);
    if (!ok) return;

    int const srcType = QInputDialog::getInt(this,
        tr("Smart-script dry-run"),
        tr("source_type (0=creature, 1=gameobject, 2=areatrigger, 9=action_list, ...):"),
        /*value=*/defaultSrc,
        /*min=*/0, /*max=*/255, /*step=*/1, &ok);
    if (!ok) return;

    auto* dlg = new app::SmartScriptDryRunDialog(m_worldDb.get(),
                                                 int64_t(eoid),
                                                 uint8_t(srcType),
                                                 this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onShowHealthReport()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before running the health report."));
        return;
    }
    auto* dlg = new app::HealthReportDialog(m_worldDb.get(), m_currentMapId, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &app::HealthReportDialog::jumpRequested, this,
            [this](uint32_t mapId, float worldX, float worldY) {
        onJumpRequested(mapId, worldX, worldY, std::nullopt);
    });
    dlg->show();
}

void MainWindow::onRebuildBookmarksMenu()
{
    if (!m_bookmarksMenu)
        return;
    m_bookmarksMenu->clear();

    // Fixed actions at the top of the submenu, always present.
    auto* manageAct = m_bookmarksMenu->addAction(tr("&Manage bookmarks..."));
    connect(manageAct, &QAction::triggered, this, &MainWindow::onShowBookmarksManager);
    auto* addAct = m_bookmarksMenu->addAction(tr("&Add current viewer position..."));
    connect(addAct, &QAction::triggered, this, &MainWindow::onAddBookmarkAtCurrentView);
    m_bookmarksMenu->addSeparator();

    QVector<app::Bookmark> const all = app::bookmarks::loadAll();
    if (all.isEmpty())
    {
        auto* empty = m_bookmarksMenu->addAction(tr("(no bookmarks yet)"));
        empty->setEnabled(false);
        return;
    }

    // Group by folder.  Empty-folder rows go into the "Quick" top-level
    // section; named folders become nested submenus, alpha-sorted.
    QVector<app::Bookmark> quick;
    QMap<QString, QVector<app::Bookmark>> grouped;
    for (app::Bookmark const& b : all)
    {
        if (b.folder.trimmed().isEmpty())
            quick.push_back(b);
        else
            grouped[b.folder].push_back(b);
    }

    auto jumpHandler = [this](app::Bookmark b)
    {
        // Route through the same slot the FindJumpDialog uses so the
        // viewer pan + status-bar message stay consistent.
        onJumpRequested(b.mapId, b.x, b.y, std::nullopt);
    };

    if (!quick.isEmpty())
    {
        // Top-level "Quick" section is rendered as inline actions (no
        // wrapping submenu) so a single-folder operator's flat list
        // still works in one click.
        for (app::Bookmark const& b : quick)
        {
            auto* act = m_bookmarksMenu->addAction(app::bookmarks::actionTitleFor(b));
            connect(act, &QAction::triggered, this, [jumpHandler, b]() { jumpHandler(b); });
        }
        if (!grouped.isEmpty())
            m_bookmarksMenu->addSeparator();
    }
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it)
    {
        QMenu* sub = m_bookmarksMenu->addMenu(it.key());
        for (app::Bookmark const& b : it.value())
        {
            auto* act = sub->addAction(app::bookmarks::actionTitleFor(b));
            connect(act, &QAction::triggered, this, [jumpHandler, b]() { jumpHandler(b); });
        }
    }
}

void MainWindow::onShowBookmarksManager()
{
    app::BookmarksManagerDialog dlg(this);
    connect(&dlg, &app::BookmarksManagerDialog::bookmarksChanged,
            this, &MainWindow::onRebuildBookmarksMenu);
    dlg.exec();
    // bookmarksChanged() fires on every mutation but the menu only
    // rebuilds when next shown anyway -- this final call covers the
    // close-without-apply path where nothing was edited.
    onRebuildBookmarksMenu();
}

void MainWindow::onAddBookmarkAtCurrentView()
{
    uint32_t const mapId = m_currentMapId.value_or(0);
    float curX = 0.0f, curY = 0.0f, curZ = 0.0f;
    if (m_viewer)
    {
        auto const& vt = m_viewer->viewTransform();
        curX = vt.anchorWorld.x;
        curY = vt.anchorWorld.y;
    }
    bool ok = false;
    QString const proposed = tr("map %1 at (%2, %3)")
        .arg(mapId).arg(curX, 0, 'f', 1).arg(curY, 0, 'f', 1);
    QString const name = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Name:"), QLineEdit::Normal, proposed, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    QString const folder = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Folder (leave blank for Quick):"), QLineEdit::Normal, QString{}, &ok);
    if (!ok)
        return;
    QString const tags = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Tags (comma-separated, optional):"), QLineEdit::Normal, QString{}, &ok);
    if (!ok)
        return;

    QVector<app::Bookmark> all = app::bookmarks::loadAll();
    app::Bookmark b;
    b.name   = name.trimmed();
    b.folder = folder.trimmed();
    b.tags   = tags.trimmed();
    b.mapId  = mapId;
    b.x      = curX;
    b.y      = curY;
    b.z      = curZ;
    all.push_back(b);
    app::bookmarks::saveAll(all);
    onRebuildBookmarksMenu();
    statusBar()->showMessage(tr("Saved bookmark: %1").arg(b.name), 3000);
}

// ---------------------------------------------------------------------------
// File -> Recent maps submenu.  Recents persist in QSettings as a
// semicolon-separated list of "<mapId>|<dirName>|<unix-secs>" tuples.  The
// list is capped at 15, deduped by mapId (newer push wins), and most-recent-
// first so a fresh open always lands at the top of the submenu.

namespace
{

QString humanizeElapsed(qint64 nowSecs, qint64 thenSecs)
{
    qint64 const d = std::max<qint64>(0, nowSecs - thenSecs);
    if (d < 60)        return QObject::tr("%1s ago").arg(d);
    if (d < 3600)      return QObject::tr("%1m ago").arg(d / 60);
    if (d < 86400)     return QObject::tr("%1h ago").arg(d / 3600);
    if (d < 86400 * 7) return QObject::tr("%1d ago").arg(d / 86400);
    return QObject::tr("%1w ago").arg(d / (86400 * 7));
}

struct RecentMapEntry
{
    uint32_t mapId   = 0;
    QString  dirName;
    qint64   tsSecs  = 0;
};

QVector<RecentMapEntry> loadRecentMaps()
{
    QSettings settings;
    QString const raw = settings.value(QStringLiteral("editor/recent_maps")).toString();
    QVector<RecentMapEntry> out;
    if (raw.isEmpty())
        return out;
    QStringList const tuples = raw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    out.reserve(tuples.size());
    for (QString const& t : tuples)
    {
        QStringList const parts = t.split(QLatin1Char('|'));
        if (parts.size() != 3) continue;
        bool okId = false, okTs = false;
        uint const id = parts[0].toUInt(&okId);
        qint64 const ts = parts[2].toLongLong(&okTs);
        if (!okId || !okTs) continue;
        RecentMapEntry e;
        e.mapId   = static_cast<uint32_t>(id);
        e.dirName = parts[1];
        e.tsSecs  = ts;
        out.push_back(e);
    }
    return out;
}

void saveRecentMaps(QVector<RecentMapEntry> const& list)
{
    QStringList tuples;
    tuples.reserve(list.size());
    for (RecentMapEntry const& e : list)
    {
        // Sanitize: strip any embedded separators so the round-trip survives.
        QString dir = e.dirName;
        dir.replace(QLatin1Char(';'), QLatin1Char('_'));
        dir.replace(QLatin1Char('|'), QLatin1Char('_'));
        tuples << QStringLiteral("%1|%2|%3")
                      .arg(e.mapId).arg(dir).arg(e.tsSecs);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("editor/recent_maps"), tuples.join(QLatin1Char(';')));
}

} // namespace

void MainWindow::recordRecentMap(uint32_t mapId)
{
    // dirName: prefer mmapsDir basename (most stable across runs).  If the
    // operator hasn't set it yet (unlikely at this point), fall back to
    // mapsDir or vmapsDir basename, then to an empty string.
    QString dirName;
    for (QString const& candidate : { m_mmapsDir, m_mapsDir, m_vmapsDir })
    {
        if (candidate.isEmpty()) continue;
        QString const base = QDir(candidate).dirName();
        if (!base.isEmpty()) { dirName = base; break; }
    }

    qint64 const nowSecs = QDateTime::currentSecsSinceEpoch();
    QVector<RecentMapEntry> list = loadRecentMaps();
    // Drop any existing entry for this mapId so the fresh push wins.
    QVector<RecentMapEntry> deduped;
    deduped.reserve(list.size() + 1);
    RecentMapEntry head;
    head.mapId   = mapId;
    head.dirName = dirName;
    head.tsSecs  = nowSecs;
    deduped.push_back(head);
    for (RecentMapEntry const& e : list)
    {
        if (e.mapId == mapId) continue;
        deduped.push_back(e);
        if (deduped.size() >= 15) break;
    }
    saveRecentMaps(deduped);
}

void MainWindow::onRebuildRecentMapsMenu()
{
    if (!m_recentMapsMenu)
        return;
    m_recentMapsMenu->clear();

    QVector<RecentMapEntry> const list = loadRecentMaps();
    if (list.isEmpty())
    {
        auto* empty = m_recentMapsMenu->addAction(tr("(no recent maps)"));
        empty->setEnabled(false);
        return;
    }

    qint64 const nowSecs = QDateTime::currentSecsSinceEpoch();
    for (RecentMapEntry const& e : list)
    {
        QString const title = tr("%1: %2  (%3)")
            .arg(e.mapId)
            .arg(e.dirName.isEmpty() ? tr("<unknown>") : e.dirName)
            .arg(humanizeElapsed(nowSecs, e.tsSecs));
        auto* act = m_recentMapsMenu->addAction(title);
        uint32_t const mapId = e.mapId;
        connect(act, &QAction::triggered, this, [this, mapId]() { loadAndDisplayMap(mapId); });
    }

    m_recentMapsMenu->addSeparator();
    auto* clearAct = m_recentMapsMenu->addAction(tr("&Clear recent"));
    connect(clearAct, &QAction::triggered, this, &MainWindow::onClearRecentMaps);
}

void MainWindow::onClearRecentMaps()
{
    QSettings settings;
    settings.remove(QStringLiteral("editor/recent_maps"));
    onRebuildRecentMapsMenu();
    statusBar()->showMessage(tr("Cleared recent maps list."), 3000);
}

void MainWindow::onShowFindJumpDialog()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first - Find/Jump queries spawn + template tables."));
        return;
    }
    uint32_t const mapId = m_currentMapId.value_or(0);
    // Pull the current 2D view's anchor so the Coords tab + "Add
    // current view" bookmark capture the operator's actual viewpoint,
    // not (0, 0).
    float currentX = 0.0f, currentY = 0.0f;
    if (m_viewer)
    {
        auto const& vt = m_viewer->viewTransform();
        currentX = vt.anchorWorld.x;
        currentY = vt.anchorWorld.y;
    }
    app::FindJumpDialog dlg(m_worldDb.get(), mapId, currentX, currentY, this);
    connect(&dlg, &app::FindJumpDialog::jumpRequested,
            this, &MainWindow::onJumpRequested);
    dlg.exec();
}

void MainWindow::onShowSpawnSearchDialog()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first - Spawn search queries spawn + template + loot tables."));
        return;
    }
    uint32_t const mapId = m_currentMapId.value_or(0);
    app::SpawnSearchDialog dlg(m_worldDb.get(), mapId, this);
    connect(&dlg, &app::SpawnSearchDialog::jumpRequested,
            this, &MainWindow::onJumpRequested);
    dlg.exec();
}

void MainWindow::onShowFindSimilarDialog()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first - Find similar spawns queries the creature table."));
        return;
    }
    if (m_selectedSpawnIndex < 0 || !m_spawnModel
        || m_selectedSpawnIndex >= int(m_spawnModel->current().size()))
    {
        QMessageBox::information(this, tr("No spawn selected"),
            tr("Select exactly one spawn before opening Find similar."));
        return;
    }
    render::Spawn const& ref = m_spawnModel->current()[m_selectedSpawnIndex];
    if (ref.kind != render::SpawnKind::Creature)
    {
        QMessageBox::information(this, tr("Creature only"),
            tr("Find similar spawns currently only supports creature spawns."));
        return;
    }
    app::FindSimilarDialog dlg(m_worldDb.get(), ref, this);
    connect(&dlg, &app::FindSimilarDialog::jumpRequested,
            this, &MainWindow::onJumpRequested);
    dlg.exec();
}

void MainWindow::onJumpRequested(uint32_t mapId, float worldX, float worldY,
                                 std::optional<int64_t> guid)
{
    // If we're not already on the requested map, open it.  loadAndDisplayMap
    // is heavy (mmap load + DB reloads) so we only call it on a real switch.
    if (!m_currentMapId.has_value() || *m_currentMapId != mapId)
        loadAndDisplayMap(mapId);

    if (m_viewer)
        m_viewer->panTo(worldX, worldY, /*ypp=*/ 2.0f);  // ~2 yards per pixel
    // 2D should be the active stack page after a jump so the operator
    // sees the result.
    if (m_centralStack)
        m_centralStack->setCurrentWidget(m_viewer);

    statusBar()->showMessage(
        tr("Jumped to map %1 at (%2, %3)%4")
            .arg(mapId)
            .arg(worldX, 0, 'f', 1)
            .arg(worldY, 0, 'f', 1)
            .arg(guid.has_value()
                 ? tr(" (guid=%1)").arg(qlonglong(*guid)) : QString{}),
        4000);

    // If a specific spawn was requested, try to select it in the model + dock.
    if (guid.has_value() && m_spawnModel)
    {
        int64_t const want = *guid;
        auto const& current = m_spawnModel->current();
        for (size_t i = 0; i < current.size(); ++i)
        {
            if (current[i].guid == want)
            {
                m_selectedSpawnIndex = int(i);
                if (m_spawnEditor)
                {
                    m_spawnEditor->setRow(int(i), current[i]);
                    m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
                }
                if (m_diagDock)
                    m_diagDock->setSelection(current[i].kind, current[i].guid, current[i].entry);
                break;
            }
        }
    }
}

void MainWindow::onGotoLineEditReturn()
{
    // Status-bar coordinate jump.  Accepts:
    //   "X Y"            -> pan current map to (X, Y)
    //   "X Y mapId"      -> switch to mapId, then pan
    //   "/<entry>"       -> first creature.id=entry on current map
    //   "#<guid>"        -> creature.guid lookup (any map)
    //   "?<name frag>"   -> first creature_template.name LIKE %frag%
    // Whitespace AND commas are treated as separators so paste-from-SQL
    // ("123, 456") works without manual cleanup.

    if (!m_gotoEdit)
        return;

    QString const raw = m_gotoEdit->text().trimmed();
    if (raw.isEmpty())
        return;

    auto showError = [this](QString const& msg) {
        statusBar()->showMessage(tr("Goto: %1").arg(msg), 3000);
    };

    // Branch on the leading sigil.  '/', '#', '?' all require a DB
    // connection -- we surface that explicitly rather than silently
    // failing the SQL.
    QChar const sigil = raw.at(0);
    if (sigil == QLatin1Char('/') || sigil == QLatin1Char('#') || sigil == QLatin1Char('?'))
    {
        if (!m_worldDb || !m_worldDb->isConnected())
        {
            showError(tr("DB not connected (needed for lookup)"));
            return;
        }
        QString const arg = raw.mid(1).trimmed();
        if (arg.isEmpty())
        {
            showError(tr("missing argument after '%1'").arg(sigil));
            return;
        }

        if (sigil == QLatin1Char('/'))
        {
            // Creature template entry lookup, scoped to current map.
            if (!m_currentMapId.has_value())
            {
                showError(tr("no map loaded; load a map first or use 'X Y mapId'"));
                return;
            }
            bool ok = false;
            qlonglong const entry = arg.toLongLong(&ok);
            if (!ok || entry <= 0)
            {
                showError(tr("'/%1' is not a positive integer entry").arg(arg));
                return;
            }
            char sql[256];
            std::snprintf(sql, sizeof(sql),
                "SELECT guid, map, position_x, position_y FROM creature "
                "WHERE id = %lld AND map = %u ORDER BY guid LIMIT 1",
                (long long)entry, *m_currentMapId);
            db::QueryResult res;
            auto const err = m_worldDb->query(sql, res);
            if (!err.ok())
            {
                showError(tr("creature query failed: %1").arg(QString::fromStdString(err.message)));
                return;
            }
            if (res.rowCount() == 0)
            {
                showError(tr("no creature with entry=%1 on map %2").arg(entry).arg(*m_currentMapId));
                return;
            }
            int64_t const guid = res.asInt64(0, 0).value_or(0);
            uint32_t const mapId = uint32_t(res.asUInt64(0, 1).value_or(*m_currentMapId));
            float const x = float(res.asDouble(0, 2).value_or(0.0));
            float const y = float(res.asDouble(0, 3).value_or(0.0));
            onJumpRequested(mapId, x, y, std::make_optional<int64_t>(guid));
            return;
        }

        if (sigil == QLatin1Char('#'))
        {
            // Creature guid lookup, any map.
            bool ok = false;
            qlonglong const guid = arg.toLongLong(&ok);
            if (!ok || guid == 0)
            {
                showError(tr("'#%1' is not a numeric guid").arg(arg));
                return;
            }
            char sql[256];
            std::snprintf(sql, sizeof(sql),
                "SELECT map, position_x, position_y FROM creature WHERE guid = %lld LIMIT 1",
                (long long)guid);
            db::QueryResult res;
            auto const err = m_worldDb->query(sql, res);
            if (!err.ok())
            {
                showError(tr("creature query failed: %1").arg(QString::fromStdString(err.message)));
                return;
            }
            if (res.rowCount() == 0)
            {
                showError(tr("no creature with guid=%1").arg(guid));
                return;
            }
            uint32_t const mapId = uint32_t(res.asUInt64(0, 0).value_or(0));
            float const x = float(res.asDouble(0, 1).value_or(0.0));
            float const y = float(res.asDouble(0, 2).value_or(0.0));
            onJumpRequested(mapId, x, y, std::make_optional<int64_t>(int64_t(guid)));
            return;
        }

        // sigil == '?': name fragment LIKE match against creature_template.
        // Prefer spawns on the current map if one exists, but fall back to
        // any-map; mirrors FindJumpDialog::onSearchTemplate's preference
        // ordering.
        std::string esc;
        esc.reserve(arg.size() + 8);
        for (QChar const ch : arg)
        {
            char const c = ch.toLatin1();
            // Escape SQL metacharacters defensively; the input is operator-
            // typed but we still don't want '%' / '_' / '\'' silently
            // changing the semantics.
            if (c == '\'' || c == '\\' || c == '%' || c == '_')
                esc.push_back('\\');
            esc.push_back(c == 0 ? '?' : c);
        }
        uint32_t const preferMap = m_currentMapId.value_or(0);
        char sql[1024];
        std::snprintf(sql, sizeof(sql),
            "SELECT c.guid, c.map, c.position_x, c.position_y "
            "FROM creature c JOIN creature_template t ON t.entry = c.id "
            "WHERE t.name LIKE '%%%s%%' "
            "ORDER BY (c.map = %u) DESC, c.guid LIMIT 1",
            esc.c_str(), preferMap);
        db::QueryResult res;
        auto const err = m_worldDb->query(sql, res);
        if (!err.ok())
        {
            showError(tr("name lookup failed: %1").arg(QString::fromStdString(err.message)));
            return;
        }
        if (res.rowCount() == 0)
        {
            showError(tr("no creature_template.name matches '%1'").arg(arg));
            return;
        }
        int64_t const guid = res.asInt64(0, 0).value_or(0);
        uint32_t const mapId = uint32_t(res.asUInt64(0, 1).value_or(preferMap));
        float const x = float(res.asDouble(0, 2).value_or(0.0));
        float const y = float(res.asDouble(0, 3).value_or(0.0));
        onJumpRequested(mapId, x, y, std::make_optional<int64_t>(guid));
        return;
    }

    // Numeric form: "X Y" or "X Y mapId".  Accept whitespace AND commas as
    // separators to be forgiving on copy-paste from SQL output.
    QStringList toks = raw.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    if (toks.size() != 2 && toks.size() != 3)
    {
        showError(tr("expected 'X Y' or 'X Y mapId' (got %1 tokens)").arg(toks.size()));
        return;
    }
    bool okX = false;
    bool okY = false;
    float const x = toks[0].toFloat(&okX);
    float const y = toks[1].toFloat(&okY);
    if (!okX || !okY)
    {
        showError(tr("X and Y must be numbers"));
        return;
    }
    uint32_t mapId = m_currentMapId.value_or(0);
    if (toks.size() == 3)
    {
        bool okM = false;
        uint const m = toks[2].toUInt(&okM);
        if (!okM)
        {
            showError(tr("mapId must be a non-negative integer"));
            return;
        }
        mapId = uint32_t(m);
    }
    else if (!m_currentMapId.has_value())
    {
        showError(tr("no map loaded; use 'X Y mapId' to pick one"));
        return;
    }
    onJumpRequested(mapId, x, y, std::nullopt);
}

void MainWindow::onHighlightSpawnGuids(QVector<qlonglong> const& guids)
{
    if (!m_viewer || !m_spawnModel) return;
    // Resolve guid -> viewer-index for creature spawns; build selection.
    QSet<qlonglong> wanted;
    for (qlonglong g : guids) wanted.insert(g);
    QVector<int> selection;
    auto const& display = m_viewer->spawns();
    for (size_t i = 0; i < display.size(); ++i)
    {
        if (display[i].kind == render::SpawnKind::Creature
            && wanted.contains(qlonglong(display[i].guid)))
        {
            selection.append(int(i));
        }
    }
    // Push selection into the viewer + property dock via the same path
    // box-select uses.
    onSpawnSelectionChanged(selection);
    statusBar()->showMessage(
        tr("Highlighted %1 group members on the map (%2 visible)")
            .arg(qulonglong(guids.size())).arg(qulonglong(selection.size())),
        4000);
}

void MainWindow::onAssignPathToSelectedSpawn()
{
    if (!m_waypointModel || m_selectedPathIndex < 0)
    {
        QMessageBox::warning(this, tr("No path"), tr("Click a path polyline first."));
        return;
    }
    if (!m_spawnModel || m_selectedSpawnIndex < 0)
    {
        QMessageBox::warning(this, tr("No spawn selected"),
            tr("Click a creature spawn icon first - the path will be assigned to that creature_addon row."));
        return;
    }
    render::Spawn const& spawn = m_spawnModel->current()[m_selectedSpawnIndex];
    if (spawn.kind != render::SpawnKind::Creature)
    {
        QMessageBox::warning(this, tr("Wrong kind"),
            tr("Only creatures can be assigned a waypoint path."));
        return;
    }
    uint32_t const pid = m_waypointModel->current()[m_selectedPathIndex].pathId;
    int64_t  const guid = spawn.guid;

    // UPSERT creature_addon.PathId and bump creature.MovementType=2 in one transaction.
    auto exec = [&](std::string const& sql) -> bool {
        auto err = m_worldDb->exec(sql);
        if (!err.ok())
        {
            QMessageBox::critical(this, tr("Assign failed"),
                tr("SQL error: [%1] %2").arg(err.code).arg(QString::fromStdString(err.message)));
            return false;
        }
        return true;
    };
    if (!exec("START TRANSACTION")) return;
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO creature_addon (guid, PathId) VALUES (%lld, %u) "
        "ON DUPLICATE KEY UPDATE PathId = VALUES(PathId)",
        static_cast<long long>(guid), pid);
    if (!exec(buf)) { (void)m_worldDb->exec("ROLLBACK"); return; }
    std::snprintf(buf, sizeof(buf),
        "UPDATE creature SET MovementType = 2 WHERE guid = %lld",
        static_cast<long long>(guid));
    if (!exec(buf)) { (void)m_worldDb->exec("ROLLBACK"); return; }
    if (!exec("COMMIT")) return;

    statusBar()->showMessage(
        tr("Assigned PathId=%1 to creature guid=%2 (MovementType set to 2)")
            .arg(pid).arg(guid), 5000);
    // Refresh spawns so the editor reflects the new MovementType.
    if (m_currentMapId.has_value())
        reloadSpawnsForMap(*m_currentMapId);
}

void MainWindow::reloadSpawnsForMap(uint32_t mapId)
{
    if (!m_worldDb || !m_worldDb->isConnected())
        return;

    statusBar()->showMessage(tr("Querying spawns for map %1...").arg(mapId));
    QApplication::processEvents();

    std::vector<render::Spawn> spawns;

    auto queryCreatures = [&]() -> bool
    {
        char sql[1024];
        std::snprintf(sql, sizeof(sql),
            "SELECT guid, id, map, zoneId, areaId, spawnDifficulties, "
            "       phaseUseFlags, PhaseId, PhaseGroup, terrainSwapMap, "
            "       modelid, equipment_id, position_x, position_y, position_z, "
            "       orientation, spawntimesecs, wander_distance, currentwaypoint, "
            "       curHealthPct, MovementType, npcflag, "
            "       unit_flags, unit_flags2, unit_flags3, "
            "       ScriptName, StringId, VerifiedBuild "
            "FROM creature WHERE map = %u", mapId);
        db::QueryResult res;
        db::QueryError const err = m_worldDb->query(sql, res);
        if (!err.ok())
        {
            QMessageBox::warning(this, tr("creature query failed"),
                tr("[%1] %2").arg(err.code).arg(QString::fromStdString(err.message)));
            return false;
        }
        spawns.reserve(spawns.size() + res.rowCount());
        for (size_t r = 0; r < res.rowCount(); ++r)
        {
            render::Spawn s;
            s.kind            = render::SpawnKind::Creature;
            s.guid            = res.asInt64 (r, *res.columnIndex("guid")).value_or(0);
            s.entry           = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("id")).value_or(0));
            s.mapId           = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("map")).value_or(mapId));
            s.zoneId          = static_cast<uint16_t>(res.asUInt64(r, *res.columnIndex("zoneId")).value_or(0));
            s.areaId          = static_cast<uint16_t>(res.asUInt64(r, *res.columnIndex("areaId")).value_or(0));
            s.spawnDifficulties = QString::fromStdString(res.cell(r, *res.columnIndex("spawnDifficulties")));
            s.phaseUseFlags   = static_cast<uint8_t>(res.asUInt64(r, *res.columnIndex("phaseUseFlags")).value_or(0));
            s.phaseId         = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("PhaseId")).value_or(0));
            s.phaseGroup      = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("PhaseGroup")).value_or(0));
            s.terrainSwapMap  = static_cast<int32_t>(res.asInt64 (r, *res.columnIndex("terrainSwapMap")).value_or(-1));
            s.modelid         = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("modelid")).value_or(0));
            s.equipmentId     = static_cast<uint8_t> (res.asUInt64(r, *res.columnIndex("equipment_id")).value_or(0));
            s.worldX          = static_cast<float>(res.asDouble(r, *res.columnIndex("position_x")).value_or(0.0));
            s.worldY          = static_cast<float>(res.asDouble(r, *res.columnIndex("position_y")).value_or(0.0));
            s.worldZ          = static_cast<float>(res.asDouble(r, *res.columnIndex("position_z")).value_or(0.0));
            s.orientation     = static_cast<float>(res.asDouble(r, *res.columnIndex("orientation")).value_or(0.0));
            s.spawntimesecs   = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("spawntimesecs")).value_or(120));
            s.wanderDistance  = static_cast<float>(res.asDouble(r, *res.columnIndex("wander_distance")).value_or(0.0));
            s.currentwaypoint = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("currentwaypoint")).value_or(0));
            s.curHealthPct    = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("curHealthPct")).value_or(100));
            s.movementType    = static_cast<uint8_t>(res.asUInt64(r, *res.columnIndex("MovementType")).value_or(0));
            s.npcflag         = res.asUInt64(r, *res.columnIndex("npcflag")).value_or(0);
            s.unitFlags1      = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("unit_flags")).value_or(0));
            s.unitFlags2      = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("unit_flags2")).value_or(0));
            s.unitFlags3      = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("unit_flags3")).value_or(0));
            s.scriptName      = QString::fromStdString(res.cell(r, *res.columnIndex("ScriptName")));
            s.stringId        = QString::fromStdString(res.cell(r, *res.columnIndex("StringId")));
            s.verifiedBuild   = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("VerifiedBuild")).value_or(0));
            spawns.push_back(std::move(s));
        }
        return true;
    };

    auto queryGameObjects = [&]() -> bool
    {
        char sql[1024];
        std::snprintf(sql, sizeof(sql),
            "SELECT guid, id, map, zoneId, areaId, spawnDifficulties, "
            "       phaseUseFlags, PhaseId, PhaseGroup, terrainSwapMap, "
            "       position_x, position_y, position_z, orientation, "
            "       rotation0, rotation1, rotation2, rotation3, "
            "       spawntimesecs, animprogress, state, "
            "       ScriptName, StringId, VerifiedBuild "
            "FROM gameobject WHERE map = %u", mapId);
        db::QueryResult res;
        db::QueryError const err = m_worldDb->query(sql, res);
        if (!err.ok())
        {
            QMessageBox::warning(this, tr("gameobject query failed"),
                tr("[%1] %2").arg(err.code).arg(QString::fromStdString(err.message)));
            return false;
        }
        spawns.reserve(spawns.size() + res.rowCount());
        for (size_t r = 0; r < res.rowCount(); ++r)
        {
            render::Spawn s;
            s.kind            = render::SpawnKind::GameObject;
            s.guid            = res.asInt64 (r, *res.columnIndex("guid")).value_or(0);
            s.entry           = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("id")).value_or(0));
            s.mapId           = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("map")).value_or(mapId));
            s.zoneId          = static_cast<uint16_t>(res.asUInt64(r, *res.columnIndex("zoneId")).value_or(0));
            s.areaId          = static_cast<uint16_t>(res.asUInt64(r, *res.columnIndex("areaId")).value_or(0));
            s.spawnDifficulties = QString::fromStdString(res.cell(r, *res.columnIndex("spawnDifficulties")));
            s.phaseUseFlags   = static_cast<uint8_t> (res.asUInt64(r, *res.columnIndex("phaseUseFlags")).value_or(0));
            s.phaseId         = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("PhaseId")).value_or(0));
            s.phaseGroup      = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("PhaseGroup")).value_or(0));
            s.terrainSwapMap  = static_cast<int32_t> (res.asInt64 (r, *res.columnIndex("terrainSwapMap")).value_or(-1));
            s.worldX          = static_cast<float>(res.asDouble(r, *res.columnIndex("position_x")).value_or(0.0));
            s.worldY          = static_cast<float>(res.asDouble(r, *res.columnIndex("position_y")).value_or(0.0));
            s.worldZ          = static_cast<float>(res.asDouble(r, *res.columnIndex("position_z")).value_or(0.0));
            s.orientation     = static_cast<float>(res.asDouble(r, *res.columnIndex("orientation")).value_or(0.0));
            s.rotation0       = static_cast<float>(res.asDouble(r, *res.columnIndex("rotation0")).value_or(0.0));
            s.rotation1       = static_cast<float>(res.asDouble(r, *res.columnIndex("rotation1")).value_or(0.0));
            s.rotation2       = static_cast<float>(res.asDouble(r, *res.columnIndex("rotation2")).value_or(0.0));
            s.rotation3       = static_cast<float>(res.asDouble(r, *res.columnIndex("rotation3")).value_or(1.0));
            s.spawntimesecs   = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("spawntimesecs")).value_or(0));
            s.animprogress    = static_cast<uint8_t> (res.asUInt64(r, *res.columnIndex("animprogress")).value_or(100));
            s.goState         = static_cast<uint8_t> (res.asUInt64(r, *res.columnIndex("state")).value_or(1));
            s.scriptName      = QString::fromStdString(res.cell(r, *res.columnIndex("ScriptName")));
            s.stringId        = QString::fromStdString(res.cell(r, *res.columnIndex("StringId")));
            s.verifiedBuild   = static_cast<uint32_t>(res.asUInt64(r, *res.columnIndex("VerifiedBuild")).value_or(0));
            spawns.push_back(std::move(s));
        }
        return true;
    };

    queryCreatures();
    queryGameObjects();

    // Battlemaster recruitment-radius overlay data.  npcflag is a column
    // on creature_template (per-entry, not per-spawn), so we pull the
    // distinct entries flagged BATTLEMASTER (0x100000) once per map load
    // and hand the set to the viewer.  Failures here are non-fatal; the
    // overlay just stays empty.
    if (m_viewer)
    {
        std::vector<uint32_t> bmEntries;
        char bmSql[256];
        std::snprintf(bmSql, sizeof(bmSql),
            "SELECT entry FROM creature_template WHERE (npcflag & 0x100000) <> 0");
        db::QueryResult bmRes;
        db::QueryError const bmErr = m_worldDb->query(bmSql, bmRes);
        if (bmErr.ok())
        {
            bmEntries.reserve(bmRes.rowCount());
            for (size_t r = 0; r < bmRes.rowCount(); ++r)
            {
                auto const v = bmRes.asUInt64(r, 0);
                if (v.has_value())
                    bmEntries.push_back(static_cast<uint32_t>(*v));
            }
        }
        m_viewer->setBattlemasterEntries(std::move(bmEntries));
    }

    // Faction-territory tint map.  We always build the map (cheap; one
    // query per map load) but the viewer only consumes it when the
    // Layer::FactionTint toggle is on.  The setting drives the layer
    // visibility; the map itself is hot so toggling at runtime is free.
    {
        QSettings s;
        bool const factionTintOn = s.value(SETTINGS_FACTION_TINT, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::FactionTint, factionTintOn);
        if (factionTintOn)
            loadFactionTintMap();
    }

    // Level-heatmap map.  One query per map load when the layer toggle
    // is on; same hot-toggle pattern as faction tint.
    {
        QSettings s;
        bool const levelHeatOn = s.value(SETTINGS_LEVEL_HEATMAP, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::LevelHeatmap, levelHeatOn);
        if (levelHeatOn)
            loadLevelMap();
    }

    // Spawn-density layer: pure viewer-side cache, no DB query.  Toggle
    // forwards to the viewer; setSpawns() already marks the grid dirty.
    {
        QSettings s;
        bool const densityOn = s.value(SETTINGS_SPAWN_DENSITY, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::SpawnDensity, densityOn);
    }

    // Flight path graph: re-query taxi_nodes / taxi_path for the new
    // map when the toggle is on; otherwise just push visibility so a
    // later toggle has a clean slate.
    {
        QSettings s;
        bool const flightOn = s.value(SETTINGS_FLIGHT_PATHS, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::FlightPaths, flightOn);
        if (flightOn)
            loadFlightGraph();
        else if (m_viewer)
            m_viewer->setFlightGraph({}, {});
    }

    // Transport route overlay: refresh polylines for the new map when
    // the toggle is on; clear otherwise so a later enable starts clean.
    {
        QSettings s;
        bool const transportOn = s.value(SETTINGS_TRANSPORT_ROUTES, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::TransportRoutes, transportOn);
        if (transportOn)
            loadTransportRoutes();
        else if (m_viewer)
            m_viewer->setTransportRoutes({});
    }

    // Gathering-node heatmap: classify GOs on the new map as
    // mining/herb/fishing/treasure when the toggle is on; clear
    // otherwise so a later toggle has a clean slate.
    {
        QSettings s;
        bool const gatheringOn = s.value(SETTINGS_GATHERING_NODES, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::GatheringNodes, gatheringOn);
        if (gatheringOn)
            loadGatheringNodes();
        else if (m_viewer)
            m_viewer->setGatheringNodes({});
    }

    // Instance-entrance overlay: re-query areatrigger_teleport for the
    // new map when the toggle is on; clear otherwise so the operator's
    // screen matches the menu state immediately.
    {
        QSettings s;
        bool const entrancesOn = s.value(SETTINGS_INSTANCE_ENTRANCES, true).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::InstanceEntrance, entrancesOn);
        if (entrancesOn)
            loadInstanceEntrances();
        else if (m_viewer)
            m_viewer->setInstanceEntrances({});
    }

    // linked_respawn dependency overlay: re-query the dependency table for
    // the new map when the toggle is on; clear otherwise so the operator's
    // screen matches the menu state.
    {
        QSettings s;
        bool const linksOn = s.value(SETTINGS_SPAWN_LINKS, false).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::SpawnLinks, linksOn);
        if (linksOn)
            loadSpawnLinks();
        else if (m_viewer)
            m_viewer->setSpawnLinks({});
    }

    // Road network overlay: sync the layer visibility to the persisted
    // QSettings on every map load.  The viewer rebuilds the auto-
    // extracted polyline set inside setNavMesh, so we only have to flip
    // the visibility bit here -- the GL buffer is already fresh.
    {
        QSettings s;
        bool const roadsOn = s.value(SETTINGS_SHOW_ROADS, true).toBool();
        if (m_viewer)
            m_viewer->setLayerVisible(render::Layer::Roads, roadsOn);
    }

    // Sibling-highlight layer visibility: sync to QSettings on every
    // map load + drop any leftover highlight set so the new map starts
    // with a clean slate.
    {
        QSettings s;
        bool const siblingsOn = s.value(SETTINGS_SIBLING_HIGHLIGHT, true).toBool();
        if (m_viewer)
        {
            m_viewer->setLayerVisible(render::Layer::SiblingHighlight, siblingsOn);
            m_viewer->setHighlightedSiblings({});
        }
    }

    size_t const creatureCount   = std::count_if(spawns.begin(), spawns.end(),
        [](render::Spawn const& s) { return s.kind == render::SpawnKind::Creature; });
    size_t const gameObjectCount = spawns.size() - creatureCount;

    m_spawnStatsLabel->setText(tr("creatures=%1  GOs=%2")
        .arg(creatureCount).arg(gameObjectCount));

    // Hand to viewer + SpawnModel baseline.
    m_spawnModel->setBaseline(spawns);
    m_selectedSpawnIndex = -1;
    if (m_spawnEditor)
        m_spawnEditor->clear();
    pushSpawnsToViewer();
    statusBar()->showMessage(tr("Spawn refresh done"), 2000);
}

void MainWindow::pushSpawnsToViewer()
{
    if (!m_viewer || !m_spawnModel)
        return;
    auto const& current = m_spawnModel->current();
    auto const& changes = m_spawnModel->changes();
    std::vector<render::Spawn> visible;
    visible.reserve(current.size());
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (i < changes.size() && changes[i].kind == db::SpawnChangeKind::Delete)
            continue;
        visible.push_back(current[i]);
    }
    if (m_viewer3d) m_viewer3d->setSpawns(visible);
    m_viewer->setSpawns(std::move(visible));
    // Keep File -> Export spawns to CSV enabled state in sync with the loaded set so the
    // action lights up the instant a map's spawns land in the model (and goes dark on a
    // map switch that yields zero rows, modulo an active selection).
    if (auto* act = findChild<QAction*>(QStringLiteral("export_spawns_csv_action")))
    {
        bool const hasAny = !m_spawnModel->current().empty();
        act->setEnabled(!m_spawnSelection.isEmpty() || hasAny);
    }
    // Refresh phase-filter status line so the visible/total spawn count
    // tracks reloads (DB refresh, map switch, undo/redo, etc.).
    auto const& f = m_viewer->spawnPhaseFilter();
    if (f.enabled)
    {
        size_t const total      = m_viewer->spawns().size();
        size_t const visibleCnt = m_viewer->visibleSpawnCount();
        statusBar()->showMessage(
            tr("Phase filter ON: id=%1 group=%2 (%3 of %4 spawns visible)")
                .arg(f.phaseId).arg(f.phaseGroup).arg(visibleCnt).arg(total),
            0);
    }
}

void MainWindow::onSwitchTo2D()
{
    if (m_centralStack) m_centralStack->setCurrentIndex(0);
    statusBar()->showMessage(tr("2D view"), 1500);
}

void MainWindow::onSwitchTo3D()
{
    // Drain the deferred 3D mesh load before flipping the stack so the
    // user sees a populated 3D view in the same frame.
    ensureViewer3dMeshLoaded();
    if (m_centralStack)
    {
        m_centralStack->setCurrentIndex(1);
        if (m_viewer3d) m_viewer3d->setFocus();
    }
    statusBar()->showMessage(tr("3D view  -  WASD/QE move, right-drag look, F frames mesh"), 4000);
}

void MainWindow::reloadAnnotationsForMap(uint32_t mapId)
{
    if (!m_worldDb || !m_worldDb->isConnected())
        return;

    // Cross-DB query into the operator-configured shared playerbot schema
    // (the connection's "Shared playerbot DB" field); the world connection
    // reaches it provided the DB user has SELECT there. Never a hardcoded DB
    // name. Mirrors the WorldMetadataStore table layout exactly.
    std::string const metaTbl = m_worldDb->qualify("playerbot_v2_world_metadata");
    char sqlBuf[400];
    std::snprintf(sqlBuf, sizeof(sqlBuf),
        "SELECT id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, "
        "       label, notes, created_by "
        "FROM %s "
        "WHERE map_id = %u", metaTbl.c_str(), mapId);

    db::QueryResult res;
    db::QueryError const err = m_worldDb->query(sqlBuf, res);
    if (!err.ok())
    {
        // Most likely cause: characters DB lives on a different host,
        // or the connecting user lacks SELECT on it.  Surface but do
        // not block; map navmesh + spawns still work.
        m_annotStatsLabel->setText(tr("annotations: query failed (%1)").arg(err.code));
        statusBar()->showMessage(tr("Annotation query failed: %1")
            .arg(QString::fromStdString(err.message)), 5000);
        m_viewer->setAnnotations({});
        if (m_annotationToolbox)
        {
            m_annotationToolbox->setCommittedCount(0, mapId);
            m_annotationToolbox->showToast(
                tr("\xE2\x9C\x97 Annotation query failed: %1")
                    .arg(QString::fromStdString(err.message)),
                QStringLiteral("err"));
        }
        return;
    }

    auto const idxId        = res.columnIndex("id");
    auto const idxMap       = res.columnIndex("map_id");
    auto const idxZone      = res.columnIndex("zone_id");
    auto const idxKind      = res.columnIndex("kind");
    auto const idxX         = res.columnIndex("pos_x");
    auto const idxY         = res.columnIndex("pos_y");
    auto const idxZ         = res.columnIndex("pos_z");
    auto const idxR         = res.columnIndex("radius");
    auto const idxLabel     = res.columnIndex("label");
    auto const idxNotes     = res.columnIndex("notes");
    auto const idxCreatedBy = res.columnIndex("created_by");
    if (!idxId || !idxKind || !idxX || !idxY || !idxZ || !idxR)
    {
        m_annotStatsLabel->setText(tr("annotations: schema mismatch"));
        m_viewer->setAnnotations({});
        if (m_annotationToolbox)
        {
            m_annotationToolbox->setCommittedCount(0, mapId);
            m_annotationToolbox->showToast(
                tr("\xE2\x9C\x97 Annotation schema mismatch on map %1").arg(mapId),
                QStringLiteral("err"));
        }
        return;
    }

    std::vector<render::Annotation> annotations;
    annotations.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Annotation a;
        a.id     = res.asInt64 (r, *idxId).value_or(0);
        a.mapId  = static_cast<uint32_t>(res.asUInt64(r, *idxMap).value_or(mapId));
        a.zoneId = idxZone ? static_cast<uint32_t>(res.asUInt64(r, *idxZone).value_or(0)) : 0;
        uint64_t const kindRaw = res.asUInt64(r, *idxKind).value_or(0);
        a.kind   = (kindRaw < static_cast<uint64_t>(render::AnnotationKind::Count_))
                 ? static_cast<render::AnnotationKind>(kindRaw)
                 : render::AnnotationKind::Unknown;
        a.x      = static_cast<float>(res.asDouble(r, *idxX).value_or(0.0));
        a.y      = static_cast<float>(res.asDouble(r, *idxY).value_or(0.0));
        a.z      = static_cast<float>(res.asDouble(r, *idxZ).value_or(0.0));
        a.radius = static_cast<float>(res.asDouble(r, *idxR).value_or(10.0));
        if (idxLabel     && !res.isNull(r, *idxLabel))
            a.label     = QString::fromStdString(res.cell(r, *idxLabel));
        if (idxNotes     && !res.isNull(r, *idxNotes))
            a.notes     = QString::fromStdString(res.cell(r, *idxNotes));
        if (idxCreatedBy && !res.isNull(r, *idxCreatedBy))
            a.createdBy = QString::fromStdString(res.cell(r, *idxCreatedBy));
        annotations.push_back(std::move(a));
    }

    size_t const baselineCount = annotations.size();
    m_annotStatsLabel->setText(tr("annotations=%1").arg(baselineCount));
    m_annotationModel->setBaseline(std::move(annotations));
    m_selectedAnnotationIndex = -1;
    if (m_viewer) m_viewer->setSelectedAnnotation(-1);
    if (m_annotationToolbox)
    {
        m_annotationToolbox->clearSelectedRow();
        m_annotationToolbox->setPendingCount(0);
        m_annotationToolbox->setCommittedCount(baselineCount, mapId);
    }
    pushAnnotationsToViewer();
}

void MainWindow::pushAnnotationsToViewer()
{
    if (!m_viewer || !m_annotationModel)
        return;
    if (m_annotPropertyDock)
        m_annotPropertyDock->setPendingCount(m_annotationModel->pendingCount());

    // Build the viewer's display list: every current row whose changelog
    // entry isn't Delete.
    auto const& current = m_annotationModel->current();
    auto const& changes = m_annotationModel->changes();
    std::vector<render::Annotation> visible;
    visible.reserve(current.size());
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (i < changes.size() && changes[i].kind == db::ChangeKind::Delete)
            continue;
        visible.push_back(current[i]);
    }
    if (m_viewer3d) m_viewer3d->setAnnotations(visible);
    m_viewer->setAnnotations(std::move(visible));

    if (m_annotationToolbox)
        m_annotationToolbox->setPendingCount(m_annotationModel->pendingCount());
}

void MainWindow::onFrameView()
{
    if (m_viewer)
        m_viewer->frameMesh();
}

void MainWindow::onToggleNavLayer(bool checked)
{
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::NavMesh, checked);
    if (m_viewer3d)
        m_viewer3d->setLayerVisible(render::Layer::NavMesh, checked);
}

void MainWindow::onToggleSpawnLayer(bool checked)
{
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::Spawns, checked);
    if (m_viewer3d)
        m_viewer3d->setLayerVisible(render::Layer::Spawns, checked);
}

void MainWindow::onToggleHeightmapLayer(bool checked)
{
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::Heightmap, checked);
    QSettings settings;
    settings.setValue(QStringLiteral("editor/show_heightmap"), checked);
}

void MainWindow::onTogglePathLayer(bool checked)
{
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::Paths, checked);
    if (m_viewer3d)
        m_viewer3d->setLayerVisible(render::Layer::Paths, checked);
}

void MainWindow::onToggleAnnotationLayer(bool checked)
{
    if (m_viewer)
        m_viewer->setLayerVisible(render::Layer::Annotations, checked);
    if (m_viewer3d)
        m_viewer3d->setLayerVisible(render::Layer::Annotations, checked);
}

void MainWindow::onAnnotationClicked(int annotationIndex)
{
    if (!m_viewer || annotationIndex < 0 ||
        annotationIndex >= int(m_viewer->annotations().size()))
        return;
    render::Annotation const& a = m_viewer->annotations()[annotationIndex];

    // The viewer index is into its filtered display list; resolve to the
    // model index by matching id (or for locally-added rows, by guid pair).
    int modelIndex = -1;
    auto const& current = m_annotationModel->current();
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (current[i].id == a.id)
        {
            modelIndex = static_cast<int>(i);
            break;
        }
    }
    m_selectedAnnotationIndex = modelIndex;
    if (modelIndex >= 0 && m_annotationToolbox)
        m_annotationToolbox->setSelectedRow(modelIndex, current[modelIndex]);
    if (m_annotPropertyDock && modelIndex >= 0)
        m_annotPropertyDock->setRow(modelIndex, current[modelIndex]);
    else if (m_annotPropertyDock)
        m_annotPropertyDock->setRow(-1, a);
    // Push the visible-list index back so the viewer can paint the
    // selection halo.  Note: annotationIndex is into the filtered visible
    // list, which is what NavMeshView::setSelectedAnnotation expects.
    m_viewer->setSelectedAnnotation(annotationIndex);
}

void MainWindow::onAnnotationHovered(int annotationIndex)
{
    if (!m_viewer || annotationIndex < 0 ||
        annotationIndex >= int(m_viewer->annotations().size()))
        return;
    render::Annotation const& a = m_viewer->annotations()[annotationIndex];
    m_coordsLabel->setText(QString::asprintf("%s r=%.1fy id=%lld",
        render::annotationKindName(a.kind), a.radius, (long long)a.id));
}

void MainWindow::onPlaceModeChanged(bool placing)
{
    if (m_viewer)
        m_viewer->setPlacementMode(placing);
    if (placing)
    {
        setPlacementKind(PlacementKind::Annotation);
    }
    else if (m_placementKind == PlacementKind::Annotation)
    {
        setPlacementKind(PlacementKind::None);
    }
    statusBar()->showMessage(placing
        ? tr("Placement mode ON - left-click drops an annotation")
        : tr("Placement mode off"), 3000);
}

void MainWindow::setPlacementKind(PlacementKind kind)
{
    m_placementKind = kind;
    updateModeBadge();
}

void MainWindow::updateModeBadge()
{
    if (!m_modeBadge)
        return;
    QString text;
    char const* bg = "#5a6472";   // neutral slate for Select
    switch (m_placementKind)
    {
        case PlacementKind::None:
            text = tr("SELECT");
            break;
        case PlacementKind::Annotation:
            text = tr("ANNOTATE - click places | Esc exits");
            bg = "#8e44ad";
            break;
        case PlacementKind::Spawn:
            text = tr("PLACE SPAWN - click drops | Esc exits");
            bg = "#27824c";
            break;
        case PlacementKind::PathDraw:
            text = tr("DRAW PATH - click adds node | Return finishes | Esc cancels");
            bg = "#2d6fb3";
            break;
        case PlacementKind::Areatrigger:
            text = tr("PLACE AREATRIGGER - click drops | Esc exits");
            bg = "#a03aa0";
            break;
        case PlacementKind::Graveyard:
            text = tr("PLACE GRAVEYARD - click drops | Esc exits");
            bg = "#1d8a8a";
            break;
        case PlacementKind::RoadDraw:
            text = tr("ROAD DRAW - click drops waypoint | Esc exits");
            bg = "#b3672d";
            break;
        case PlacementKind::PathProbe:
            text = tr("PATH PROBE - click START then END (bot budget) | Esc exits");
            bg = "#1f7a70";
            break;
        case PlacementKind::OffmeshDraw:
            text = tr("OFF-MESH DRAW - click FROM then TO (appends to offmesh.txt) | Esc exits");
            bg = "#a12a6e";
            break;
    }
    m_modeBadge->setText(text);
    m_modeBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: white; font-weight: 600; "
        "padding: 2px 8px; border-radius: 3px; }").arg(QLatin1String(bg)));
}

void MainWindow::cancelActivePlacement()
{
    switch (m_placementKind)
    {
        case PlacementKind::None:
        case PlacementKind::PathDraw:   // onCancelPath owns this one.
            return;
        case PlacementKind::Annotation:
            // Uncheck the toolbox "Place" box; its toggled signal funnels
            // through onPlaceModeChanged which clears the mode + viewers.
            if (m_annotationToolbox)
                m_annotationToolbox->setPlacing(false);
            else
                setPlacementKind(PlacementKind::None);
            break;
        case PlacementKind::RoadDraw:
            onFinishRoadDraw();
            return;   // onFinishRoadDraw resets kind + viewers itself.
        case PlacementKind::Spawn:
        case PlacementKind::Areatrigger:
        case PlacementKind::Graveyard:
            setPlacementKind(PlacementKind::None);
            break;
        case PlacementKind::PathProbe:
            m_probeHaveStart = false;
            setPlacementKind(PlacementKind::None);
            break;
        case PlacementKind::OffmeshDraw:
            m_offmeshHaveStart = false;
            setPlacementKind(PlacementKind::None);
            break;
    }
    if (m_viewer)   m_viewer->setPlacementMode(false);
    if (m_viewer3d) m_viewer3d->setPlacementMode(false);
    statusBar()->showMessage(tr("Placement mode exited."), 3000);
}

void MainWindow::onPlacementRay(float ox, float oy, float oz,
                               float dx, float dy, float dz)
{
    if (m_placementKind == PlacementKind::None)
        return;
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Load a map first."), 3000);
        return;
    }
    uint32_t const map = *m_currentMapId;

    float const len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f)
        return;
    dx /= len; dy /= len; dz /= len;

    // snapToGround returns this sentinel when there is no floor at (x, y).
    constexpr float kNoGround = -1.0e6f;
    auto groundAt = [&](float x, float y) -> float {
        return snapToGround(map, x, y, /*probeZ*/ 100000.0f, /*fallbackZ*/ kNoGround);
    };

    // Coarse-march the ray until it crosses from above the surface to below it
    // -- that bracket contains the terrain/WMO hit.  Using the authoritative
    // height source (the same one 2D placement + drag-snap use) keeps this
    // reliable where depth-buffer readback was not.
    constexpr float kStep    = 8.0f;     // yards between coarse samples
    constexpr float kMaxDist = 8000.0f;  // give up beyond this
    float prevT = 0.0f;
    bool  havePrev = false, prevAbove = false;
    float crossLo = -1.0f, crossHi = -1.0f;
    for (float t = 0.0f; t <= kMaxDist; t += kStep)
    {
        float const x = ox + dx*t, y = oy + dy*t, z = oz + dz*t;
        float const g = groundAt(x, y);
        if (g <= kNoGround + 1.0f) { havePrev = false; continue; }   // gap with no terrain
        bool const above = (z >= g);
        if (havePrev && prevAbove && !above)
        {
            crossLo = prevT;
            crossHi = t;
            break;
        }
        prevT = t; prevAbove = above; havePrev = true;
    }

    if (crossHi < 0.0f)
    {
        statusBar()->showMessage(
            tr("No ground under the cursor - aim at terrain and click again."), 3000);
        return;
    }

    // Bisect the bracket for an accurate surface hit.
    for (int i = 0; i < 28; ++i)
    {
        float const m = 0.5f * (crossLo + crossHi);
        float const x = ox + dx*m, y = oy + dy*m, z = oz + dz*m;
        float const g = groundAt(x, y);
        if (g <= kNoGround + 1.0f) { crossLo = m; continue; }
        if (z >= g) crossLo = m; else crossHi = m;
    }
    float const t  = crossHi;
    float const hx = ox + dx*t, hy = oy + dy*t;
    float const hz = groundAt(hx, hy);

    // Place at the resolved surface point, carrying the surface Z as authored.
    onPlacementRequested3D(hx, hy, hz);
}

void MainWindow::onPlacementRequested3D(float worldX, float worldY, float worldZ)
{
    // worldZ is the surface height resolved by the 3D pick (ray-marched against
    // terrain/WMO height).  Use it as the authored Z -- stored on the row, not
    // recomputed at runtime.  The flag is consumed by the shared placement body
    // via resolvePlacementZ() and cleared immediately after.
    m_haveAuthoredZ = true;
    m_authoredZ     = worldZ;
    onPlacementRequested(worldX, worldY);
    m_haveAuthoredZ = false;
}

float MainWindow::resolvePlacementZ(uint32_t mapId, float worldX, float worldY, float fallback)
{
    if (m_haveAuthoredZ)
        return m_authoredZ;     // exact surface height from the 3D click
    // 2D top-down fallback: probe the highest floor below a tall ray.
    return snapToGround(mapId, worldX, worldY, /*probeZ*/ 10000.0f, fallback);
}

void MainWindow::onPlacementRequested(float worldX, float worldY)
{
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Load a map first."), 3000);
        return;
    }

    switch (m_placementKind)
    {
        case PlacementKind::Annotation:
        {
            if (!m_annotationToolbox) return;
            render::Annotation a;
            a.mapId     = *m_currentMapId;
            a.zoneId    = 0;
            a.kind      = m_annotationToolbox->currentKind();
            a.x         = worldX;
            a.y         = worldY;
            // Authored surface Z from a 3D click (0 for a 2D top-down drop,
            // where annotations are Z-agnostic markers anyway).
            a.z         = m_haveAuthoredZ ? m_authoredZ : 0.0f;
            a.radius    = m_annotationToolbox->currentRadius();
            a.label     = m_annotationToolbox->currentLabel();
            a.notes     = m_annotationToolbox->currentNotes();
            a.createdBy = m_annotationToolbox->currentCreatedBy();
            m_undo->recordOn(m_annotationModel.get(),
                tr("Place annotation"), [&]() {
                m_annotationModel->addNew(a);
            });
            pushAnnotationsToViewer();
            size_t const totalNow = m_viewer ? m_viewer->annotations().size() : 0;
            qDebug().noquote() << QString::asprintf(
                "[annotation] placed: id=(pending) kind=%s radius=%.1f world=(%.1f,%.1f)"
                " - viewer received %zu total annotations",
                render::annotationKindName(a.kind), double(a.radius),
                double(worldX), double(worldY), totalNow);
            m_annotationToolbox->showToast(
                tr("\xE2\x9C\x93 Annotation placed (%1 at %2, %3) - pending commit")
                    .arg(QString::fromLatin1(render::annotationKindName(a.kind)))
                    .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1),
                QStringLiteral("ok"));
            statusBar()->showMessage(tr("Placed %1 at (%2, %3) - pending commit")
                .arg(QString::fromLatin1(render::annotationKindName(a.kind)))
                .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1), 3000);
            return;
        }

        case PlacementKind::Spawn:
        {
            if (!m_pickedTemplate || !m_spawnModel) return;

            render::Spawn s;
            s.kind  = m_pickedTemplate->kind;
            s.entry = m_pickedTemplate->entry;
            s.mapId = *m_currentMapId;
            s.zoneId = 0;
            s.areaId = 0;
            s.spawnDifficulties = QStringLiteral("0");
            s.phaseUseFlags = 0;
            s.phaseId       = 0;
            s.phaseGroup    = 0;
            s.terrainSwapMap = -1;
            s.spawntimesecs = 120;
            s.worldX = worldX;
            s.worldY = worldY;
            s.worldZ = 0.0f;
            s.orientation = 0.0f;
            if (m_haveAuthoredZ)
            {
                // 3D click already resolved the real surface height under the
                // cursor -- store it verbatim (overrides the snap toggle,
                // which exists only for the Z-less 2D top-down path).
                s.worldZ = m_authoredZ;
            }
            else if (m_spawnEditor && m_spawnEditor->snapToGroundEnabled())
            {
                // 2D fallback: probeZ=10000 means "find ANY floor below" since
                // we have no initial Z; first placement of a new spawn
                // typically happens at ground level so this matches the
                // operator's intent (the highest reasonable floor at that XY).
                s.worldZ = snapToGround(s.mapId, worldX, worldY,
                                        /*probeZ*/ 10000.0f, /*fallback*/ 0.0f);
            }
            // Reserve guid.
            if (s.kind == render::SpawnKind::Creature)
                s.guid = static_cast<int64_t>(m_nextCreatureGuid++);
            else
                s.guid = static_cast<int64_t>(m_nextGoGuid++);

            int newIndex = -1;
            m_undo->recordOn(m_spawnModel.get(),
                tr("Place spawn"), [&]() {
                newIndex = m_spawnModel->addNew(s);
            });
            // Select the new row so the property editor reflects it
            // immediately - operator can tweak before clicking elsewhere.
            m_selectedSpawnIndex = newIndex;
            if (m_spawnEditor)
            {
                m_spawnEditor->setRow(newIndex, s);
                m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
            }
            pushSpawnsToViewer();
            statusBar()->showMessage(tr("Placed %1 entry=%2 guid=%3 at (%4, %5, %6) - pending commit")
                .arg(s.kind == render::SpawnKind::Creature
                     ? QStringLiteral("creature") : QStringLiteral("gameobject"))
                .arg(s.entry).arg(s.guid)
                .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1).arg(s.worldZ, 0, 'f', 1),
                3000);
            return;
        }

        case PlacementKind::PathDraw:
        {
            if (!m_drawingPath) return;

            // Auto-route branch: Detour computes every waypoint from
            // the last placed node to the click.  Falls back to a
            // single manual node when the navmesh can't reach.
            if (m_autoRouteNextClick && !m_drawingPathBuf.nodes.empty() && m_viewer)
            {
                m_autoRouteNextClick = false;
                render::PathNode const& last = m_drawingPathBuf.nodes.back();
                float const endZ = snapToGround(*m_currentMapId, worldX, worldY,
                                                /*probeZ*/ 10000.0f,
                                                /*fallback*/ last.z);
                auto const route = m_viewer->findRoute(
                    last.x, last.y, last.z,
                    worldX, worldY, endZ,
                    /*maxStraightPathPoints*/ 1024);
                if (route.empty())
                {
                    statusBar()->showMessage(
                        tr("Auto-route failed (no navmesh path from node %1 to click); "
                           "falling back to single manual node.  "
                           "Try clicking closer to walkable terrain.")
                            .arg(int(m_drawingPathBuf.nodes.size())), 6000);
                    // Fall through to the manual-single-node path below.
                }
                else
                {
                    int firstId = int(m_drawingPathBuf.nodes.size()) + 1;
                    for (coords::WorldPos const& p : route)
                    {
                        render::PathNode rn;
                        rn.nodeId      = firstId++;
                        rn.x           = p.x;
                        rn.y           = p.y;
                        rn.z           = p.z;
                        rn.orientation = 0.0f;
                        rn.delay       = 0;
                        m_drawingPathBuf.nodes.push_back(std::move(rn));
                    }
                    pushPathsToViewer();
                    statusBar()->showMessage(
                        tr("Auto-routed %1 waypoint(s) -- path now %2 nodes.  "
                           "Enter to finish, Esc to cancel, Ctrl+R again to route to another point.")
                            .arg(qulonglong(route.size()))
                            .arg(qulonglong(m_drawingPathBuf.nodes.size())), 0);
                    return;
                }
            }

            render::PathNode n;
            n.nodeId = static_cast<int>(m_drawingPathBuf.nodes.size()) + 1;
            n.x = worldX;
            n.y = worldY;
            // Authored 3D-pick Z when available; else snap-to-ground (composes
            // ADT + WMO so a path through Stormwind's gates sits on the road
            // surface, not under it).  Operator can refine Z per-node later
            // via the dock.
            n.z = resolvePlacementZ(*m_currentMapId, worldX, worldY, /*fallback*/ 0.0f);
            n.orientation = 0.0f;
            n.delay       = 0;
            m_drawingPathBuf.nodes.push_back(std::move(n));
            pushPathsToViewer();
            statusBar()->showMessage(
                tr("Path node %1 placed (%2 nodes total). Enter to finish, Esc to cancel.")
                    .arg(qulonglong(m_drawingPathBuf.nodes.size()))
                    .arg(qulonglong(m_drawingPathBuf.nodes.size())), 0);
            return;
        }

        case PlacementKind::Areatrigger:
        {
            if (!m_pickedAreatriggerProps || !m_areatriggerModel) return;
            if (m_nextAreatriggerSpawnId == 0)
                refreshAreatriggerSpawnIdReservation();
            if (m_nextAreatriggerSpawnId == 0)
            {
                statusBar()->showMessage(
                    tr("Could not reserve a SpawnId - is the DB connected?"), 4000);
                return;
            }

            render::Areatrigger a;
            a.spawnId            = static_cast<int64_t>(m_nextAreatriggerSpawnId++);
            a.createPropsId      = m_pickedAreatriggerProps->createPropsId;
            a.isCustom           = m_pickedAreatriggerProps->isCustom;
            a.mapId              = *m_currentMapId;
            a.spawnDifficulties  = QStringLiteral("0");
            a.x                  = worldX;
            a.y                  = worldY;
            a.z                  = 0.0f;
            a.orientation        = 0.0f;
            a.phaseUseFlags      = 0;
            a.phaseId            = 0;
            a.phaseGroup         = 0;
            a.scriptName         = m_pickedAreatriggerProps->scriptName;
            a.comment            = QStringLiteral("placed by world_editor");
            a.verifiedBuild      = 0;
            a.shape              = m_pickedAreatriggerProps->shape;
            for (int k = 0; k < 8; ++k)
                a.shapeData[k] = m_pickedAreatriggerProps->shapeData[k];
            // Authored 3D-pick Z when available; else snap-to-ground composes
            // ADT + WMO floor (HANDOFF §10.6).
            a.z = resolvePlacementZ(a.mapId, worldX, worldY, /*fallback*/ 0.0f);

            int newIndex = -1;
            m_undo->recordOn(m_areatriggerModel.get(),
                tr("Place areatrigger"), [&]() {
                newIndex = m_areatriggerModel->addNew(a);
            });
            m_selectedAreatriggerIndex = newIndex;
            if (newIndex >= 0 && m_areatriggerDock)
            {
                m_areatriggerDock->setAreatrigger(newIndex, a);
                m_areatriggerDock->setPendingCount(m_areatriggerModel->pendingCount());
            }
            pushAreatriggersToViewer();
            statusBar()->showMessage(
                tr("Placed areatrigger SpawnId=%1 createProps=%2 at (%3, %4, %5) - pending commit")
                    .arg(a.spawnId).arg(a.createPropsId)
                    .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1).arg(a.z, 0, 'f', 1),
                3000);
            return;
        }

        case PlacementKind::RoadDraw:
        {
            if (!m_drawingRoad || !m_annotationModel) return;
            // Authored 3D-pick Z for the clicked endpoint when available; else
            // snap-to-ground.  (Auto-route intermediate nodes still take their
            // Z from the navmesh route below.)
            float const endZ = resolvePlacementZ(*m_currentMapId, worldX, worldY, /*fallback*/ 0.0f);

            // Pull defaults from the annotation toolbox -- operator's
            // current radius/label/notes/createdBy choices flow through
            // here so they don't have to retype per-Road.
            float   defaultRadius  = 12.0f;
            QString defaultLabel;
            QString defaultNotes;
            QString defaultCreator;
            if (m_annotationToolbox)
            {
                defaultRadius  = m_annotationToolbox->currentRadius();
                if (defaultRadius <= 0.0f) defaultRadius = 12.0f;
                defaultLabel   = m_annotationToolbox->currentLabel();
                defaultNotes   = m_annotationToolbox->currentNotes();
                defaultCreator = m_annotationToolbox->currentCreatedBy();
            }

            auto dropRoadAnnot = [&](float x, float y, float z) {
                render::Annotation a;
                a.mapId     = *m_currentMapId;
                a.zoneId    = 0;
                a.kind      = render::AnnotationKind::Road;
                a.x         = x;
                a.y         = y;
                a.z         = z;
                a.radius    = defaultRadius;
                a.label     = defaultLabel;
                a.notes     = defaultNotes;
                a.createdBy = defaultCreator;
                m_annotationModel->addNew(a);
            };

            int dropped = 0;
            m_undo->recordOn(m_annotationModel.get(),
                tr("Place road waypoint(s)"), [&]() {
            if (m_autoRouteNextClick && m_hasRoadAnchor && m_viewer)
            {
                m_autoRouteNextClick = false;
                auto const route = m_viewer->findRoute(
                    m_roadAnchorX, m_roadAnchorY, m_roadAnchorZ,
                    worldX, worldY, endZ,
                    /*maxStraightPathPoints*/ 1024);
                if (route.empty())
                {
                    statusBar()->showMessage(
                        tr("Auto-route failed (no navmesh path from last anchor "
                           "to click); falling back to single manual Road waypoint."), 5000);
                    dropRoadAnnot(worldX, worldY, endZ);
                    ++dropped;
                    m_roadAnchorX = worldX;
                    m_roadAnchorY = worldY;
                    m_roadAnchorZ = endZ;
                }
                else
                {
                    for (coords::WorldPos const& p : route)
                    {
                        dropRoadAnnot(p.x, p.y, p.z);
                        ++dropped;
                    }
                    coords::WorldPos const& last = route.back();
                    m_roadAnchorX = last.x;
                    m_roadAnchorY = last.y;
                    m_roadAnchorZ = last.z;
                }
            }
            else
            {
                dropRoadAnnot(worldX, worldY, endZ);
                ++dropped;
                m_roadAnchorX = worldX;
                m_roadAnchorY = worldY;
                m_roadAnchorZ = endZ;
            }
            });
            m_hasRoadAnchor = true;
            pushAnnotationsToViewer();
            statusBar()->showMessage(
                tr("Road waypoint(s) +%1 placed (radius %2y).  Ctrl+R then click "
                   "for auto-route.  Path -> Exit road draw mode to stop.")
                    .arg(dropped).arg(defaultRadius, 0, 'f', 1), 0);
            return;
        }

        case PlacementKind::Graveyard:
        {
            if (!m_graveyardModel) return;
            if (m_nextGraveyardId == 0)
                refreshGraveyardIdReservation();
            if (m_nextGraveyardId == 0)
            {
                statusBar()->showMessage(
                    tr("Could not reserve a graveyard ID - is the DB connected?"), 4000);
                return;
            }
            render::Graveyard g;
            g.id               = static_cast<uint32_t>(m_nextGraveyardId++);
            g.mapId            = *m_currentMapId;
            g.x                = worldX;
            g.y                = worldY;
            g.z                = 0.0f;
            g.facing           = 0.0f;
            g.transportSpawnId = 0;
            g.comment          = QStringLiteral("placed by world_editor");
            // Authored 3D-pick Z when available; else snap-to-ground composes
            // ADT + WMO floor.
            g.z = resolvePlacementZ(g.mapId, worldX, worldY, /*fallback*/ 0.0f);

            int newIndex = -1;
            m_undo->recordOn(m_graveyardModel.get(),
                tr("Place graveyard"), [&]() {
                newIndex = m_graveyardModel->addNew(g);
            });
            m_selectedGraveyardIndex = newIndex;
            if (newIndex >= 0 && m_graveyardDock)
            {
                m_graveyardDock->setGraveyard(newIndex, g);
                m_graveyardDock->setPendingCount(m_graveyardModel->pendingCount());
            }
            pushGraveyardsToViewer();
            statusBar()->showMessage(
                tr("Placed graveyard ID=%1 at (%2, %3, %4) - pending commit")
                    .arg(g.id)
                    .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1).arg(g.z, 0, 'f', 1),
                3000);
            return;
        }

        case PlacementKind::PathProbe:
        {
            // Two-click bot-budget pathfinding probe.  Z comes from the 3D
            // pick when available (m_haveAuthoredZ), else ground snap --
            // matching how a bot would stand at the clicked point.
            float const z = m_haveAuthoredZ
                ? m_authoredZ
                : snapToGround(*m_currentMapId, worldX, worldY,
                               /*probeZ*/ 10000.0f, /*fallback*/ 0.0f);
            if (!m_probeHaveStart)
            {
                m_probeStartX = worldX;
                m_probeStartY = worldY;
                m_probeStartZ = z;
                m_probeHaveStart = true;
                statusBar()->showMessage(
                    tr("Probe START (%1, %2, %3) - now click the destination.")
                        .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1).arg(z, 0, 'f', 1), 0);
            }
            else
            {
                if (m_viewer3d)
                    m_viewer3d->runPathProbe(
                        QVector3D(m_probeStartX, m_probeStartY, m_probeStartZ),
                        QVector3D(worldX, worldY, z));
                m_probeHaveStart = false;   // next click begins a fresh probe
                statusBar()->showMessage(
                    tr("Probe complete - see the HUD verdict. Click to start another; Esc exits."),
                    6000);
            }
            return;
        }

        case PlacementKind::OffmeshDraw:
        {
            float const z = m_haveAuthoredZ
                ? m_authoredZ
                : snapToGround(*m_currentMapId, worldX, worldY,
                               /*probeZ*/ 10000.0f, /*fallback*/ 0.0f);
            if (!m_offmeshHaveStart)
            {
                m_offmeshStartX = worldX;
                m_offmeshStartY = worldY;
                m_offmeshStartZ = z;
                m_offmeshHaveStart = true;
                statusBar()->showMessage(
                    tr("Off-mesh FROM (%1, %2, %3) - now click the TO point.")
                        .arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1).arg(z, 0, 'f', 1), 0);
            }
            else
            {
                if (appendOffmeshConnection(m_offmeshStartX, m_offmeshStartY, m_offmeshStartZ,
                                            worldX, worldY, z))
                {
                    if (m_viewer3d)
                        m_viewer3d->addPendingOffmesh(
                            QVector3D(m_offmeshStartX, m_offmeshStartY, m_offmeshStartZ),
                            QVector3D(worldX, worldY, z));
                    statusBar()->showMessage(
                        tr("Off-mesh connection appended to %1 - PENDING until the "
                           "next mmaps regen (magenta arc). Click FROM for another; Esc exits.")
                            .arg(offmeshFilePath()), 8000);
                }
                m_offmeshHaveStart = false;
            }
            return;
        }

        case PlacementKind::None:
            statusBar()->showMessage(
                tr("Placement mode is on but nothing is armed - this is a bug."), 2000);
            return;
    }
}

void MainWindow::onSelectedRadiusChanged(float newRadius)
{
    if (m_selectedAnnotationIndex < 0) return;
    if (m_annotationModel->editRadius(m_selectedAnnotationIndex, newRadius))
        pushAnnotationsToViewer();
}

void MainWindow::onSelectedLabelChanged(QString const& newLabel)
{
    if (m_selectedAnnotationIndex < 0) return;
    if (m_annotationModel->editLabel(m_selectedAnnotationIndex, newLabel))
        pushAnnotationsToViewer();
}

void MainWindow::onSelectedNotesChanged(QString const& newNotes)
{
    if (m_selectedAnnotationIndex < 0) return;
    if (m_annotationModel->editNotes(m_selectedAnnotationIndex, newNotes))
        pushAnnotationsToViewer();
}

void MainWindow::onAnnotationRowEdited(int index, render::Annotation const& proposed)
{
    if (index < 0 || !m_annotationModel) return;
    auto const& current = m_annotationModel->current();
    if (index >= int(current.size())) return;
    auto const baseline = current[index];
    bool const changed = m_undo->recordIf(m_annotationModel.get(),
        tr("Edit annotation"), [&]() {
        bool any = false;
        if (baseline.radius != proposed.radius)
            any = m_annotationModel->editRadius(index, proposed.radius) || any;
        if (baseline.label != proposed.label)
            any = m_annotationModel->editLabel(index, proposed.label) || any;
        if (baseline.notes != proposed.notes)
            any = m_annotationModel->editNotes(index, proposed.notes) || any;
        return any;
    });
    if (changed)
    {
        pushAnnotationsToViewer();
        if (m_annotPropertyDock)
            m_annotPropertyDock->setPendingCount(m_annotationModel->pendingCount());
    }
}

void MainWindow::onDeleteSelectedAnnotation()
{
    if (m_selectedAnnotationIndex < 0) return;
    int const idx = m_selectedAnnotationIndex;
    int64_t const deletedId = (idx >= 0 && idx < int(m_annotationModel->current().size()))
        ? m_annotationModel->current()[idx].id : 0;
    bool const changed = m_undo->recordIf(m_annotationModel.get(),
        tr("Delete annotation"), [&]() {
        return m_annotationModel->removeRow(idx);
    });
    if (changed)
    {
        m_selectedAnnotationIndex = -1;
        if (m_annotationToolbox) m_annotationToolbox->clearSelectedRow();
        if (m_viewer) m_viewer->setSelectedAnnotation(-1);
        if (m_annotPropertyDock)  m_annotPropertyDock->clear();
        pushAnnotationsToViewer();
        if (m_annotPropertyDock)
            m_annotPropertyDock->setPendingCount(m_annotationModel->pendingCount());
        qDebug() << "[annotation] commit \xE2\x9C\x93: action=delete, id=" << deletedId;
        if (m_annotationToolbox)
            m_annotationToolbox->showToast(
                tr("\xE2\x9C\x93 Marked annotation id=%1 for deletion (pending)").arg(deletedId),
                QStringLiteral("ok"));
    }
}

void MainWindow::onRevertAnnotations()
{
    size_t const pendingWas = m_annotationModel->pendingCount();
    m_annotationModel->revertAll();
    m_selectedAnnotationIndex = -1;
    if (m_annotationToolbox) m_annotationToolbox->clearSelectedRow();
    if (m_viewer) m_viewer->setSelectedAnnotation(-1);
    if (m_annotPropertyDock)
    {
        m_annotPropertyDock->clear();
        m_annotPropertyDock->setPendingCount(0);
    }
    pushAnnotationsToViewer();
    if (m_annotationToolbox)
        m_annotationToolbox->showToast(
            tr("Reverted %1 pending annotation edit(s) to baseline").arg(pendingWas),
            QStringLiteral("warn"));
    statusBar()->showMessage(tr("Reverted to baseline"), 2000);
}

void MainWindow::onCommitAnnotations()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before committing."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map before committing annotation edits."));
        return;
    }
    if (m_annotationModel->pendingCount() == 0)
    {
        statusBar()->showMessage(tr("Nothing to commit."), 2000);
        return;
    }

    app::CommitDialog dlg(m_worldDb.get(), *m_annotationModel, *m_currentMapId, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto committed = dlg.committedRows();
    size_t const committedCount = committed.size();
    m_annotStatsLabel->setText(tr("annotations=%1").arg(committedCount));
    m_annotationModel->acceptCommit(std::move(committed));
    m_selectedAnnotationIndex = -1;
    if (m_annotationToolbox) m_annotationToolbox->clearSelectedRow();
    if (m_viewer) m_viewer->setSelectedAnnotation(-1);
    if (m_annotPropertyDock)
    {
        m_annotPropertyDock->clear();
        m_annotPropertyDock->setPendingCount(0);
    }
    pushAnnotationsToViewer();
    if (m_annotationToolbox)
    {
        m_annotationToolbox->setCommittedCount(committedCount, *m_currentMapId);
        m_annotationToolbox->showToast(
            tr("\xE2\x9C\x93 Commit applied: %1 annotation(s) now on map").arg(committedCount),
            QStringLiteral("ok"));
    }
    qDebug() << "[annotation] commit \xE2\x9C\x93: action=commit, rows="
             << committedCount << "map=" << *m_currentMapId;
    statusBar()->showMessage(tr("Commit applied."), 3000);
}

void MainWindow::onSpawnClicked(int spawnIndex)
{
    if (!m_viewer || spawnIndex < 0 || spawnIndex >= int(m_viewer->spawns().size()))
        return;
    render::Spawn const& s = m_viewer->spawns()[spawnIndex];

    // The viewer index is into the FILTERED display list; resolve it
    // to the model's row by matching (kind, guid).
    int modelIndex = -1;
    auto const& current = m_spawnModel->current();
    for (size_t i = 0; i < current.size(); ++i)
    {
        if (current[i].kind == s.kind && current[i].guid == s.guid)
        {
            modelIndex = static_cast<int>(i);
            break;
        }
    }
    m_selectedSpawnIndex = modelIndex;
    if (modelIndex >= 0 && m_spawnEditor)
    {
        m_spawnEditor->setRow(modelIndex, current[modelIndex]);
        m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());

        // spawn_group membership probe.  Joins spawn_group_template for friendly
        // names; missing table (older schemas) silently falls back to "none".
        QString membershipText = tr("Spawn groups: none");
        if (m_worldDb && m_worldDb->isConnected())
        {
            render::Spawn const& sel = current[modelIndex];
            uint32_t const spawnType = (sel.kind == render::SpawnKind::Creature) ? 0u : 1u;
            QString const sql = QStringLiteral(
                "SELECT sg.groupId, COALESCE(sgt.groupName, '') "
                "FROM spawn_group sg "
                "LEFT JOIN spawn_group_template sgt ON sgt.groupId = sg.groupId "
                "WHERE sg.spawnType = %1 AND sg.spawnId = %2")
                .arg(spawnType).arg(sel.guid);
            db::QueryResult res;
            db::QueryError const err = m_worldDb->query(sql.toStdString(), res);
            // MySQL ER_NO_SUCH_TABLE = 1146; any error -> silent "none".
            if (err.ok() && res.rowCount() > 0)
            {
                QStringList names;
                size_t const total = res.rowCount();
                size_t const shown = std::min<size_t>(total, 5);
                for (size_t r = 0; r < shown; ++r)
                {
                    uint64_t const gid = res.asUInt64(r, 0).value_or(0);
                    QString name = QString::fromStdString(res.cell(r, 1));
                    if (name.isEmpty())
                        name = QStringLiteral("(unnamed)");
                    if (total == 1)
                        names << tr("%1 (id %2)").arg(name).arg(gid);
                    else
                        names << name;
                }
                if (total == 1)
                    membershipText = tr("Spawn group: %1").arg(names.first());
                else
                {
                    QString joined = names.join(QStringLiteral(", "));
                    if (total > shown)
                        joined += tr(", +%1 more").arg(total - shown);
                    membershipText = tr("Spawn groups: %1").arg(joined);
                }
            }
        }
        m_spawnEditor->setGroupMembershipText(membershipText);
    }
    if (modelIndex >= 0 && m_diagDock)
    {
        render::Spawn const& sel = current[modelIndex];
        m_diagDock->setSelection(sel.kind, sel.guid, sel.entry);
    }
    if (m_vendorDock)
    {
        // Vendor inventory is creature-only (the npc_vendor schema keys
        // on creature_template.entry).  Pass 0 for GOs so the panel
        // shows its empty-state message instead of a confusing zero
        // count.
        if (modelIndex >= 0 && current[modelIndex].kind == render::SpawnKind::Creature)
            m_vendorDock->setVendorEntry(current[modelIndex].entry);
        else
            m_vendorDock->clear();
    }
    if (m_trainerDock)
    {
        // Trainer dock activates only when the selected creature template
        // carries any UNIT_NPC_FLAG_TRAINER (0x10) / TRAINER_CLASS (0x20)
        // / TRAINER_PROFESSION (0x40) bit; otherwise clear so we don't
        // run trainer-spell queries against random NPCs.
        bool shouldQuery = false;
        if (modelIndex >= 0
            && current[modelIndex].kind == render::SpawnKind::Creature
            && m_worldDb && m_worldDb->isConnected())
        {
            uint32_t const entry = current[modelIndex].entry;
            QString const sql = QStringLiteral(
                "SELECT npcflag FROM creature_template WHERE entry = %1 LIMIT 1").arg(entry);
            db::QueryResult res;
            if (m_worldDb->query(sql.toStdString(), res).ok() && res.rowCount() > 0)
            {
                uint64_t const flags = res.asUInt64(0, 0).value_or(0);
                constexpr uint64_t kTrainerMask = 0x10ULL | 0x20ULL | 0x40ULL;
                if ((flags & kTrainerMask) != 0)
                    shouldQuery = true;
            }
        }
        if (shouldQuery)
            m_trainerDock->setTrainerForCreature(current[modelIndex].entry);
        else
            m_trainerDock->clear();
    }
    if (m_lootDock)
    {
        // Loot table dispatches on spawn kind: creature -> LootID via
        // creature_template_difficulty, GO -> Data1 -> gameobject_loot_template.
        if (modelIndex >= 0 && current[modelIndex].kind == render::SpawnKind::Creature)
            m_lootDock->setCreatureEntry(current[modelIndex].entry);
        else if (modelIndex >= 0 && current[modelIndex].kind == render::SpawnKind::GameObject)
            m_lootDock->setGameObjectEntry(current[modelIndex].entry);
        else
            m_lootDock->clear();
    }
    if (m_factionDock)
    {
        // Faction template is creature-only (resolves creature_template.faction).
        // GOs and clears render the empty state.
        if (modelIndex >= 0 && current[modelIndex].kind == render::SpawnKind::Creature)
            m_factionDock->setCreatureEntry(current[modelIndex].entry);
        else
            m_factionDock->clear();
    }
    if (m_goInfoDock)
    {
        // GO info is GameObject-only; creature selections and clears
        // render the empty state.
        if (modelIndex >= 0 && current[modelIndex].kind == render::SpawnKind::GameObject)
            m_goInfoDock->setGameObjectEntry(current[modelIndex].entry);
        else
            m_goInfoDock->clear();
    }
    if (m_areaDock)
    {
        // Both creature and gameobject rows carry a denormalized
        // `areaId` column; surface its AreaTable.db2 row whichever is
        // selected.  id=0 simply clears the panel.
        if (modelIndex >= 0)
            m_areaDock->setArea(current[modelIndex].areaId);
        else
            m_areaDock->clear();
    }
    if (m_zoneSummaryDock)
    {
        // Zone summary keys off the spawn's denormalized zoneId + the
        // map it lives on.  Clears when no spawn is selected; the dock
        // caches per-zone so chained clicks within the same zone are
        // free.
        if (modelIndex >= 0)
            m_zoneSummaryDock->setZone(current[modelIndex].zoneId,
                                       current[modelIndex].mapId);
        else
            m_zoneSummaryDock->clear();
    }
    if (m_conditionsDock && modelIndex >= 0)
    {
        render::Spawn const& sel = current[modelIndex];
        m_conditionsDock->setSpawnScope(sel.entry,
            sel.kind == render::SpawnKind::Creature ? 0 : 1);
    }
    // Sibling-highlight: collect every other creature spawn that shares
    // the selected creature's (entry, mapId) on the current map.  Capped
    // at 200 so a hot entry (boars in Elwynn) doesn't flood the painter.
    // GOs intentionally skip this -- the visual is creature-centric and
    // GO templates rarely have meaningful "siblings".  When the toggle
    // is OFF, MainWindow neither walks nor pushes so the cost is zero.
    if (m_viewer)
    {
        QSettings ss;
        bool const siblingsOn = ss.value(SETTINGS_SIBLING_HIGHLIGHT, true).toBool();
        if (siblingsOn
            && modelIndex >= 0
            && current[modelIndex].kind == render::SpawnKind::Creature)
        {
            render::Spawn const& sel = current[modelIndex];
            std::vector<int64_t> siblings;
            siblings.reserve(64);
            constexpr size_t kMaxSiblings = 200;
            for (render::Spawn const& other : current)
            {
                if (other.kind != render::SpawnKind::Creature)
                    continue;
                if (other.entry != sel.entry)
                    continue;
                if (other.mapId != sel.mapId)
                    continue;
                if (other.guid == sel.guid)
                    continue;
                siblings.push_back(other.guid);
                if (siblings.size() >= kMaxSiblings)
                    break;
            }
            m_viewer->setHighlightedSiblings(std::move(siblings));
            size_t const siblingCount = m_viewer->highlightedSiblings().size();
            if (siblingCount > 0)
            {
                // Append-to-existing keeps any prior message tail
                // (move/commit notifications) honest while still flagging
                // the sibling count for the operator.
                QString const cur = statusBar()->currentMessage();
                QString const tail = tr(" (%1 siblings)").arg(siblingCount);
                if (!cur.contains(tail))
                    statusBar()->showMessage(cur + tail, 5000);
            }
        }
        else
        {
            m_viewer->setHighlightedSiblings({});
        }
    }

    if (m_questRewardDock)
    {
        // Quest rewards are quest-keyed, not spawn-keyed.  Resolve the
        // first quest this creature offers (starter preferred, then
        // ender) via creature_queststarter / creature_questender.  GOs
        // and selection clears all surface as the empty state.
        if (modelIndex >= 0
            && current[modelIndex].kind == render::SpawnKind::Creature
            && m_worldDb && m_worldDb->isConnected())
        {
            uint32_t const entry = current[modelIndex].entry;
            uint32_t questId = 0;
            db::QueryResult res;
            QString const starterSql = QStringLiteral(
                "SELECT quest FROM creature_queststarter WHERE id = %1 ORDER BY quest LIMIT 1")
                .arg(entry);
            if (m_worldDb->query(starterSql.toStdString(), res).ok() && res.rowCount() > 0)
                questId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
            if (questId == 0)
            {
                QString const enderSql = QStringLiteral(
                    "SELECT quest FROM creature_questender WHERE id = %1 ORDER BY quest LIMIT 1")
                    .arg(entry);
                if (m_worldDb->query(enderSql.toStdString(), res).ok() && res.rowCount() > 0)
                    questId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
            }
            if (questId != 0)
                m_questRewardDock->setQuest(questId);
            else
                m_questRewardDock->clear();
        }
        else
        {
            m_questRewardDock->clear();
        }
    }
}

void MainWindow::onSpawnHovered(int spawnIndex)
{
    if (!m_viewer || spawnIndex < 0 || spawnIndex >= int(m_viewer->spawns().size()))
    {
        m_coordsLabel->setText(tr("hover"));
        return;
    }
    render::Spawn const& s = m_viewer->spawns()[spawnIndex];
    QString const kind = (s.kind == render::SpawnKind::Creature)
                       ? QStringLiteral("creature") : QStringLiteral("GO");
    m_coordsLabel->setText(QString::asprintf("%s guid=%lld entry=%u",
        qPrintable(kind), (long long)s.guid, s.entry));
}

void MainWindow::onViewerHover(float worldX, float worldY)
{
    m_coordsLabel->setText(QString::asprintf("hover  X %.1f  Y %.1f", worldX, worldY));
}

void MainWindow::onViewerClick(float worldX, float worldY)
{
    m_coordsLabel->setText(QString::asprintf("click  X %.1f  Y %.1f", worldX, worldY));
}

bool MainWindow::finishWorldDbConnect(db::ConnectionParams const& params,
                                      QString& outErrorMessage)
{
    QElapsedTimer phaseT;
    QElapsedTimer totalT;
    totalT.start();
    qInfo("[dbconnect] BEGIN host=%s db=%s",
        params.host.c_str(), params.database.c_str());

    if (!m_worldDb)
        m_worldDb = std::make_unique<db::MySqlClient>();
    phaseT.start();
    db::QueryError const err = m_worldDb->connect(params);
    qInfo("[dbconnect] MySQL connect: %lld ms (ok=%d)",
        static_cast<long long>(phaseT.elapsed()), err.ok() ? 1 : 0);
    if (!err.ok())
    {
        outErrorMessage = QString("[%1] %2").arg(err.code)
            .arg(QString::fromStdString(err.message));
        m_worldDb.reset();
        m_dbStatusLabel->setText(tr("DB: disconnected"));
        return false;
    }
    qInfo("[dbconnect] connect OK, setting status label");
    m_dbStatusLabel->setText(tr("DB: %1@%2/%3  (server %4)")
        .arg(QString::fromStdString(params.user))
        .arg(QString::fromStdString(params.host))
        .arg(QString::fromStdString(params.database))
        .arg(QString::fromStdString(m_worldDb->serverVersion())));
    qInfo("[dbconnect] status label set, beginning dock attach chain");

    // Each setDbClient call is wrapped in a timing log so any dock that
    // fires a heavy synchronous query on attach is visible in debug.log.
    // Macro avoids repeating the pattern 16 times while leaving each dock
    // type intact for the actual member-function call.
#define TC_PUSH_DB(name, dock, method)                                                                   \
    do {                                                                                                 \
        if (dock) {                                                                                      \
            qInfo("[dbconnect] dock %s " #method " BEGIN", name);                                        \
            phaseT.restart();                                                                            \
            (dock)->method(m_worldDb.get());                                                             \
            qInfo("[dbconnect] dock %s " #method " END (%lld ms)",                                       \
                name, static_cast<long long>(phaseT.elapsed()));                                         \
            QApplication::processEvents();                                                               \
        }                                                                                                \
    } while (false)
    TC_PUSH_DB("diag",        m_diagDock,        setDbClient);
    TC_PUSH_DB("vendor",      m_vendorDock,      setDbClient);
    TC_PUSH_DB("trainer",     m_trainerDock,     setDbClient);
    TC_PUSH_DB("loot",        m_lootDock,        setDbClient);
    TC_PUSH_DB("conditions",  m_conditionsDock,  setDbClient);
    TC_PUSH_DB("questReward", m_questRewardDock, setDbClient);
    TC_PUSH_DB("spell",       m_spellDock,       setDbClient);
    TC_PUSH_DB("item",        m_itemDock,        setDbClient);
    TC_PUSH_DB("goInfo",      m_goInfoDock,      setDbClient);
    TC_PUSH_DB("currency",    m_currencyDock,    setDbClient);
    TC_PUSH_DB("area",        m_areaDock,        setDbClient);
    TC_PUSH_DB("zoneSummary", m_zoneSummaryDock, setDbClient);
    TC_PUSH_DB("faction",     m_factionDock,     setDbClient);
    TC_PUSH_DB("playerCond",  m_playerCondDock,  setDbClient);
    TC_PUSH_DB("npcText",     m_npcTextDock,     setDbClient);
    TC_PUSH_DB("atrScript",   m_atrScriptDock,   setDbClient);
    if (m_handcraftedRoadDock)
    {
        qInfo("[dbconnect] dock handcraftedRoad setMySqlClient BEGIN");
        phaseT.restart();
        m_handcraftedRoadDock->setMySqlClient(m_worldDb.get());
        qInfo("[dbconnect] dock handcraftedRoad setMySqlClient END (%lld ms)",
            static_cast<long long>(phaseT.elapsed()));
        QApplication::processEvents();
    }
    phaseT.restart();
    refreshGuidReservation();
    qInfo("[dbconnect] refreshGuidReservation: %lld ms", static_cast<long long>(phaseT.elapsed()));
    QApplication::processEvents();
    phaseT.restart();
    refreshAreatriggerSpawnIdReservation();
    qInfo("[dbconnect] refreshAreatriggerSpawnIdReservation: %lld ms",
        static_cast<long long>(phaseT.elapsed()));
    QApplication::processEvents();
    phaseT.restart();
    refreshGraveyardIdReservation();
    qInfo("[dbconnect] refreshGraveyardIdReservation: %lld ms",
        static_cast<long long>(phaseT.elapsed()));
    QApplication::processEvents();
    qInfo("[dbconnect] END total %lld ms", static_cast<long long>(totalT.elapsed()));

    // If a map is already loaded, refresh spawns + annotations + paths
    // + areatriggers + graveyards.
    if (m_currentMapId.has_value())
    {
        reloadSpawnsForMap(*m_currentMapId);
        reloadAnnotationsForMap(*m_currentMapId);
        reloadPathsForMap(*m_currentMapId);
        reloadDungeonRoutesForMap(*m_currentMapId);
        reloadAreatriggersForMap(*m_currentMapId);
        reloadGraveyardsForMap(*m_currentMapId);
    }
    return true;
}

void MainWindow::tryAutoConnectWorldDb()
{
    // Silent auto-connect from the saved QSettings profile.  If the
    // operator hit "Connect..." at least once before, host+user+database
    // are populated.  We only attempt connection when those three core
    // fields look valid; otherwise just stay disconnected.
    db::ConnectionParams const params =
        db::ConnectionDialog::loadProfile(QStringLiteral("world"));
    if (params.host.empty() || params.user.empty() || params.database.empty())
        return;
    QString errorMessage;
    if (finishWorldDbConnect(params, errorMessage))
    {
        statusBar()->showMessage(tr("Auto-connected to world DB."), 3000);
    }
    else
    {
        // No popup -- the operator will see "DB: disconnected" in the
        // status bar and can recover via Database -> Connect...  But log
        // it to the transient status bar message so they know we tried.
        statusBar()->showMessage(
            tr("Auto-connect to world DB failed: %1 -- use Database -> Connect...")
                .arg(errorMessage), 6000);
    }
}

void MainWindow::onDbConnect()
{
    db::ConnectionDialog dlg(this);
    dlg.setProfileName(QStringLiteral("world"));
    dlg.setInitialParams(db::ConnectionDialog::loadProfile(QStringLiteral("world")));
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString errorMessage;
    if (!finishWorldDbConnect(dlg.params(), errorMessage))
    {
        QMessageBox::critical(this, tr("DB connect failed"), errorMessage);
        return;
    }
    statusBar()->showMessage(tr("Connected to world DB"), 3000);
}

void MainWindow::refreshGuidReservation()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        m_nextCreatureGuid = 0;
        m_nextGoGuid       = 0;
        return;
    }
    auto runMax = [&](char const* table) -> uint64_t
    {
        char sql[128];
        std::snprintf(sql, sizeof(sql),
            "SELECT COALESCE(MAX(guid), 0) FROM %s", table);
        db::QueryResult res;
        auto const err = m_worldDb->query(sql, res);
        if (!err.ok() || res.rowCount() == 0)
            return 0;
        return res.asUInt64(0, 0).value_or(0);
    };
    m_nextCreatureGuid = runMax("creature")   + 1;
    m_nextGoGuid       = runMax("gameobject") + 1;
    statusBar()->showMessage(
        tr("GUID reservation: creature >= %1, gameobject >= %2")
            .arg(m_nextCreatureGuid).arg(m_nextGoGuid), 3000);
}

void MainWindow::onNewSpawnFromTemplate()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before placing new spawns."));
        return;
    }
    if (!m_currentMapId.has_value())
    {
        QMessageBox::warning(this, tr("No map"),
            tr("Open a map first - new spawns are placed by clicking on it."));
        return;
    }
    if (m_nextCreatureGuid == 0 && m_nextGoGuid == 0)
    {
        refreshGuidReservation();
    }

    QString const worldDbName = QString::fromStdString(
        db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
    // Fall back to the current connection's default DB if we can't read
    // the profile.  serverVersion() being non-empty implies a connection.
    QString const dbName = worldDbName.isEmpty()
        ? QStringLiteral("playerbot_world") : worldDbName;

    app::TemplatePickerDialog picker(m_worldDb.get(), dbName, this);
    if (picker.exec() != QDialog::Accepted)
        return;

    m_pickedTemplate = std::make_unique<app::PickedTemplate>(picker.picked());
    setPlacementKind(PlacementKind::Spawn);
    m_viewer->setPlacementMode(true); if (m_viewer3d) m_viewer3d->setPlacementMode(true);

    statusBar()->showMessage(
        tr("Spawn placement armed: %1 entry=%2 \"%3\" - click on the map to drop. Esc / re-trigger menu to exit.")
          .arg(m_pickedTemplate->kind == render::SpawnKind::Creature
               ? QStringLiteral("creature") : QStringLiteral("gameobject"))
          .arg(m_pickedTemplate->entry)
          .arg(m_pickedTemplate->name),
        0);
}

void MainWindow::onImportSpawnsFromCsv()
{
    if (!m_spawnModel)
    {
        QMessageBox::warning(this, tr("No spawn model"),
            tr("Open a map first so the spawn model is initialized."));
        return;
    }

    // Bulk-imported rows need locally-unique guids.  Mirror the placement path: reserve from
    // the live DB if connected, otherwise fall back to the in-process counter (negative-only,
    // matching the SpawnModel local-guid contract).
    if (m_worldDb && m_worldDb->isConnected() && m_nextCreatureGuid == 0 && m_nextGoGuid == 0)
        refreshGuidReservation();

    app::CsvImportDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto const& rows = dlg.parsedRows();
    if (rows.empty())
        return;

    QString const filename = QFileInfo(dlg.sourcePath()).fileName();
    QString const label    = tr("Import %1 spawn(s) from CSV").arg(rows.size());

    // One undo frame for the whole batch so Ctrl+Z reverts the entire import atomically.
    m_undo->recordOn(m_spawnModel.get(), label, [&]() {
        for (render::Spawn s : rows)
        {
            // Assign a fresh local guid in the right family so SpawnCommitDialog can later
            // translate to real auto_increment values on commit.
            if (s.kind == render::SpawnKind::Creature)
                s.guid = static_cast<int64_t>(m_nextCreatureGuid++);
            else
                s.guid = static_cast<int64_t>(m_nextGoGuid++);
            m_spawnModel->addNew(s);
        }
    });

    pushSpawnsToViewer();
    if (m_spawnEditor)
        m_spawnEditor->setPendingCount(m_spawnModel->pendingCount());
    updateExportPendingActionEnabled();

    statusBar()->showMessage(
        tr("Imported %1 spawns from %2. Press Ctrl+Z to undo, or commit via Database menu.")
            .arg(rows.size()).arg(filename),
        0);
}

namespace
{
// CSV-quote a string per RFC 4180 lite: wrap in double quotes only when the value contains
// a comma, double quote, CR or LF; internal quotes are doubled.  Everything else passes
// through verbatim so numeric/empty cells stay unquoted (matches the import dialog's
// tolerant splitter which is unquoted-only).
QString csvEscape(QString const& in)
{
    bool needsQuote = false;
    for (QChar const c : in)
    {
        if (c == QLatin1Char(',') || c == QLatin1Char('"') ||
            c == QLatin1Char('\n') || c == QLatin1Char('\r'))
        {
            needsQuote = true;
            break;
        }
    }
    if (!needsQuote)
        return in;
    QString out;
    out.reserve(in.size() + 2);
    out.append(QLatin1Char('"'));
    for (QChar const c : in)
    {
        if (c == QLatin1Char('"'))
            out.append(QLatin1Char('"'));
        out.append(c);
    }
    out.append(QLatin1Char('"'));
    return out;
}
} // namespace

void MainWindow::onExportSpawnsToCsv()
{
    if (!m_spawnModel)
    {
        QMessageBox::warning(this, tr("No spawn model"),
            tr("Open a map first so the spawn model is initialized."));
        return;
    }

    auto const& all = m_spawnModel->current();
    auto const& changes = m_spawnModel->changes();

    // Selection branch: only the picked rows; otherwise every spawn on the loaded map,
    // excluding rows already pending Delete so the export reflects the operator's intent.
    std::vector<render::Spawn> rowsOut;
    if (!m_spawnSelection.isEmpty())
    {
        rowsOut.reserve(m_spawnSelection.size());
        for (int idx : m_spawnSelection)
        {
            if (idx < 0 || idx >= int(all.size()))
                continue;
            rowsOut.push_back(all[size_t(idx)]);
        }
    }
    else
    {
        rowsOut.reserve(all.size());
        for (size_t i = 0; i < all.size(); ++i)
        {
            if (i < changes.size() && changes[i].kind == db::SpawnChangeKind::Delete)
                continue;
            rowsOut.push_back(all[i]);
        }
    }

    if (rowsOut.empty())
    {
        QMessageBox::information(this, tr("Nothing to export"),
            tr("No spawns available to export. Load a map or select rows first."));
        return;
    }

    QSettings settings;
    QString const lastPath = settings.value(QLatin1String("editor/csv_export_last_path")).toString();
    QString const startDir = !lastPath.isEmpty()
        ? QFileInfo(lastPath).absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString const defaultName = m_currentMapId
        ? tr("spawns_map%1.csv").arg(*m_currentMapId)
        : tr("spawns.csv");
    QString const path = QFileDialog::getSaveFileName(
        this, tr("Export spawns to CSV"),
        QDir(startDir).filePath(defaultName),
        tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QMessageBox::critical(this, tr("Export failed"),
            tr("Cannot open %1 for writing: %2").arg(path).arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    uint32_t const headerMapId = m_currentMapId.value_or(rowsOut.front().mapId);

    // Single leading comment + header line.  Header tokens MUST stay in sync with the set
    // CsvImportDialog::resolveHeader() recognises -- import currently ignores tokens it
    // doesn't know, so additions here are forward-compatible.
    out << "# Exported from world_editor on "
        << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        << " from map " << headerMapId << '\n';
    out << "entry,x,y,z,orientation,mapId,spawntimesecs,phaseId,phaseGroup,phaseUseFlags,"
        << "wanderDistance,curHealthPct,modelid,equipmentId,scriptName,stringId\n";

    for (render::Spawn const& s : rowsOut)
    {
        out << s.entry << ','
            << QString::number(s.worldX, 'f', 3) << ','
            << QString::number(s.worldY, 'f', 3) << ','
            << QString::number(s.worldZ, 'f', 3) << ','
            << QString::number(s.orientation, 'f', 3) << ','
            << s.mapId << ','
            << s.spawntimesecs << ','
            << s.phaseId << ','
            << s.phaseGroup << ','
            << uint32_t(s.phaseUseFlags) << ','
            << QString::number(s.wanderDistance, 'f', 1) << ','
            << s.curHealthPct << ','
            << s.modelid << ','
            << uint32_t(s.equipmentId) << ','
            << csvEscape(s.scriptName) << ','
            << csvEscape(s.stringId) << '\n';
    }

    out.flush();
    if (file.error() != QFile::NoError)
    {
        QMessageBox::critical(this, tr("Export failed"),
            tr("Write error on %1: %2").arg(path).arg(file.errorString()));
        file.close();
        return;
    }
    file.close();

    settings.setValue(QLatin1String("editor/csv_export_last_path"), path);

    statusBar()->showMessage(
        tr("Exported %1 spawns to %2").arg(rowsOut.size()).arg(path), 0);
}

void MainWindow::onDbDisconnect()
{
    m_worldDb.reset();
    m_dbStatusLabel->setText(tr("DB: disconnected"));
    m_spawnStatsLabel->setText(QString{});
    m_annotStatsLabel->setText(QString{});
    if (m_diagDock)
    {
        m_diagDock->setDbClient(nullptr);
        m_diagDock->clear();
    }
    if (m_viewer)
    {
        m_viewer->setSpawns({});
        m_viewer->setAnnotations({});
    }
    statusBar()->showMessage(tr("Disconnected"), 2000);
}

void MainWindow::onShowShortcuts()
{
    // Modal dialog owned by `this` - destroyed when closed.
    app::ShortcutHelpDialog dlg(menuBar(), this);
    dlg.exec();
}

void MainWindow::onShowCommandPalette()
{
    // Frameless modal overlay positioned at the top-center of MainWindow.
    // Walks the live QMenuBar on construction so dynamically rebuilt menus
    // (e.g. Bookmarks) reflect their current state.
    app::CommandPaletteDialog dlg(menuBar(), this);
    dlg.positionOver(this);
    dlg.exec();
}

void MainWindow::onAbout()
{
    // Polished About dialog (replaces the legacy QMessageBox::about call).
    // Borrowed pointers - the dialog reads live status without owning the
    // DB client or the 3D viewer.
    app::AboutDialog dlg(m_worldDb.get(), m_viewer3d, this);
    dlg.exec();
}

void MainWindow::onAddLinkedRespawn(qlonglong fromGuid, qlonglong toGuid, int linkType)
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB first."));
        return;
    }
    QString const sql = QString(
        "INSERT INTO linked_respawn (guid, linkedGuid, linkType) "
        "VALUES (%1, %2, %3);").arg(fromGuid).arg(toGuid).arg(linkType);
    QString const summary = tr("Add linked_respawn: guid=%1 -> linkedGuid=%2 (type %3)")
        .arg(fromGuid).arg(toGuid).arg(linkType);
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied() && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onRemoveLinkedRespawn(qlonglong guid, int linkType)
{
    if (!m_worldDb || !m_worldDb->isConnected()) return;
    QString const sql = QString(
        "DELETE FROM linked_respawn WHERE guid=%1 AND linkType=%2;")
        .arg(guid).arg(linkType);
    QString const summary = tr("Remove linked_respawn row guid=%1 type=%2")
        .arg(guid).arg(linkType);
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied() && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onAddGameEvent(int eventEntry)
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_diagDock || m_selectedSpawnIndex < 0
     || !m_spawnModel)
        return;
    render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
    char const* table = (s.kind == render::SpawnKind::Creature)
        ? "game_event_creature" : "game_event_gameobject";
    QString const sql = QString(
        "INSERT INTO %1 (eventEntry, guid) VALUES (%2, %3);")
        .arg(QString::fromLatin1(table)).arg(eventEntry).arg(qlonglong(s.guid));
    QString const summary = tr("Bind %1 guid=%2 to game event %3")
        .arg(QString::fromLatin1(table)).arg(qlonglong(s.guid)).arg(eventEntry);
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied())
        m_diagDock->refresh();
}

void MainWindow::onRemoveGameEvent(int eventEntry)
{
    if (!m_worldDb || !m_worldDb->isConnected() || !m_diagDock || m_selectedSpawnIndex < 0
     || !m_spawnModel)
        return;
    render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
    char const* table = (s.kind == render::SpawnKind::Creature)
        ? "game_event_creature" : "game_event_gameobject";
    QString const sql = QString(
        "DELETE FROM %1 WHERE eventEntry=%2 AND guid=%3;")
        .arg(QString::fromLatin1(table)).arg(eventEntry).arg(qlonglong(s.guid));
    QString const summary = tr("Unbind %1 guid=%2 from game event %3")
        .arg(QString::fromLatin1(table)).arg(qlonglong(s.guid)).arg(eventEntry);
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied())
        m_diagDock->refresh();
}

namespace
{
// Read a smart_scripts row by composite PK into a render::SmartScript.
// Returns true on success.
bool loadSmartScriptByPk(world_editor::db::MySqlClient* client,
                         int64_t entryorguid, uint8_t sourceType,
                         uint16_t id, uint16_t link,
                         world_editor::render::SmartScript& out)
{
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT entryorguid, source_type, id, link, Difficulties, "
        "       event_type, event_phase_mask, event_chance, event_flags, "
        "       event_param1, event_param2, event_param3, event_param4, event_param5, "
        "       event_param_string, "
        "       action_type, action_param1, action_param2, action_param3, "
        "       action_param4, action_param5, action_param6, action_param7, "
        "       action_param_string, "
        "       target_type, target_param1, target_param2, target_param3, "
        "       target_param4, target_param_string, "
        "       target_x, target_y, target_z, target_o, COALESCE(comment, '') "
        "FROM smart_scripts WHERE entryorguid=%lld AND source_type=%u "
        "  AND id=%u AND link=%u LIMIT 1",
        (long long)entryorguid, unsigned(sourceType), unsigned(id), unsigned(link));
    world_editor::db::QueryResult res;
    auto const err = client->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;
    auto getU = [&](int col) -> uint64_t { return res.asUInt64(0, col).value_or(0); };
    auto getI = [&](int col) -> int64_t  { return res.asInt64(0, col).value_or(0); };
    auto getF = [&](int col) -> float    { return float(res.asDouble(0, col).value_or(0.0)); };
    auto getS = [&](int col) -> QString  { return QString::fromStdString(res.cell(0, col)); };
    out.entryorguid          = getI(0);
    out.sourceType           = uint8_t(getU(1));
    out.id                   = uint16_t(getU(2));
    out.link                 = uint16_t(getU(3));
    out.difficulties         = getS(4);
    out.eventType            = uint8_t(getU(5));
    out.eventPhaseMask       = uint16_t(getU(6));
    out.eventChance          = uint8_t(getU(7));
    out.eventFlags           = uint16_t(getU(8));
    out.eventParam1          = uint32_t(getU(9));
    out.eventParam2          = uint32_t(getU(10));
    out.eventParam3          = uint32_t(getU(11));
    out.eventParam4          = uint32_t(getU(12));
    out.eventParam5          = uint32_t(getU(13));
    out.eventParamString     = getS(14);
    out.actionType           = uint8_t(getU(15));
    out.actionParam1         = uint32_t(getU(16));
    out.actionParam2         = uint32_t(getU(17));
    out.actionParam3         = uint32_t(getU(18));
    out.actionParam4         = uint32_t(getU(19));
    out.actionParam5         = uint32_t(getU(20));
    out.actionParam6         = uint32_t(getU(21));
    out.actionParam7         = uint32_t(getU(22));
    out.actionParamStringIsNull = res.isNull(0, 23);
    out.actionParamString    = out.actionParamStringIsNull ? QString{} : getS(23);
    out.targetType           = uint8_t(getU(24));
    out.targetParam1         = uint32_t(getU(25));
    out.targetParam2         = uint32_t(getU(26));
    out.targetParam3         = uint32_t(getU(27));
    out.targetParam4         = uint32_t(getU(28));
    out.targetParamStringIsNull = res.isNull(0, 29);
    out.targetParamString    = out.targetParamStringIsNull ? QString{} : getS(29);
    out.targetX              = getF(30);
    out.targetY              = getF(31);
    out.targetZ              = getF(32);
    out.targetO              = getF(33);
    out.comment              = getS(34);
    return true;
}
} // namespace

void MainWindow::onAddSmartScript()
{
    if (!m_worldDb || !m_worldDb->isConnected() || m_selectedSpawnIndex < 0
     || !m_spawnModel)
        return;
    render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];

    // Seed a blank row with the current spawn's entry as entryorguid and
    // the matching source_type.  Operator can change to -guid for a
    // per-spawn SAI override before commit.
    render::SmartScript blank;
    blank.entryorguid = int64_t(s.entry);
    blank.sourceType  = (s.kind == render::SpawnKind::Creature) ? 0 : 1;
    blank.id          = 0;
    blank.link        = 0;
    blank.eventChance = 100;

    app::SmartScriptEditDialog editor(this);
    editor.setRow(blank);
    editor.setKeyEditable(true);
    if (editor.exec() != QDialog::Accepted)
        return;
    render::SmartScript const proposed = editor.rowSnapshot();

    db::SmartScriptModel model;
    model.setBaseline({});      // no existing rows -- pure insert
    model.addNew(proposed);

    app::SmartScriptCommitDialog dlg(m_worldDb.get(), model, this);
    if (dlg.exec() == QDialog::Accepted && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onEditSmartScript(qlonglong entryorguid, int sourceType, int id, int link)
{
    if (!m_worldDb || !m_worldDb->isConnected()) return;
    render::SmartScript before;
    if (!loadSmartScriptByPk(m_worldDb.get(), int64_t(entryorguid),
                             uint8_t(sourceType), uint16_t(id), uint16_t(link), before))
    {
        QMessageBox::warning(this, tr("Row not found"),
            tr("Could not re-read smart_scripts row (%1, %2, %3, %4) - perhaps it was removed externally?")
                .arg(entryorguid).arg(sourceType).arg(id).arg(link));
        return;
    }

    app::SmartScriptEditDialog editor(this);
    editor.setRow(before);
    editor.setKeyEditable(false);    // operator must Delete + Add to re-key
    if (editor.exec() != QDialog::Accepted)
        return;
    render::SmartScript const proposed = editor.rowSnapshot();

    db::SmartScriptModel model;
    model.setBaseline({ before });
    model.replaceRow(0, proposed);

    app::SmartScriptCommitDialog dlg(m_worldDb.get(), model, this);
    if (dlg.exec() == QDialog::Accepted && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onRemoveSmartScript(qlonglong entryorguid, int sourceType, int id, int link)
{
    if (!m_worldDb || !m_worldDb->isConnected()) return;
    render::SmartScript before;
    if (!loadSmartScriptByPk(m_worldDb.get(), int64_t(entryorguid),
                             uint8_t(sourceType), uint16_t(id), uint16_t(link), before))
    {
        QMessageBox::warning(this, tr("Row not found"),
            tr("Could not re-read smart_scripts row (%1, %2, %3, %4).")
                .arg(entryorguid).arg(sourceType).arg(id).arg(link));
        return;
    }
    db::SmartScriptModel model;
    model.setBaseline({ before });
    model.removeRow(0);

    app::SmartScriptCommitDialog dlg(m_worldDb.get(), model, this);
    if (dlg.exec() == QDialog::Accepted && m_diagDock)
        m_diagDock->refresh();
}

namespace
{
// Read a transports row by guid PK.  Returns true if the row exists.
bool loadTransportByGuid(world_editor::db::MySqlClient* client,
                        int64_t guid,
                        world_editor::app::TransportRow& out)
{
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT guid, entry, COALESCE(name, ''), phaseUseFlags, phaseid, "
        "       phasegroup, ScriptName "
        "FROM transports WHERE guid = %lld LIMIT 1",
        static_cast<long long>(guid));
    world_editor::db::QueryResult res;
    auto const err = client->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;
    out.guid          = res.asInt64 (0, 0).value_or(0);
    out.entry         = static_cast<uint32_t>(res.asUInt64(0, 1).value_or(0));
    out.name          = QString::fromStdString(res.cell(0, 2));
    out.phaseUseFlags = static_cast<uint8_t>(res.asUInt64(0, 3).value_or(0));
    out.phaseId       = static_cast<int32_t>(res.asInt64 (0, 4).value_or(0));
    out.phaseGroup    = static_cast<int32_t>(res.asInt64 (0, 5).value_or(0));
    out.scriptName    = QString::fromStdString(res.cell(0, 6));
    return true;
}

// Build the INSERT statement (also used as the body of an UPSERT for
// backup re-applyability if we ever add a Transport commit dialog).
QString formatTransportInsert(world_editor::db::MySqlClient* c,
                              world_editor::app::TransportRow const& r)
{
    auto esc = [&](QString const& v) {
        return QString::fromStdString(c->escapeString(v.toStdString()));
    };
    return QString(
        "INSERT INTO transports "
        "(guid, entry, name, phaseUseFlags, phaseid, phasegroup, ScriptName) "
        "VALUES (%1, %2, '%3', %4, %5, %6, '%7');")
        .arg(qlonglong(r.guid)).arg(r.entry).arg(esc(r.name))
        .arg(int(r.phaseUseFlags)).arg(r.phaseId).arg(r.phaseGroup)
        .arg(esc(r.scriptName));
}

QString formatTransportUpdate(world_editor::db::MySqlClient* c,
                              world_editor::app::TransportRow const& r)
{
    auto esc = [&](QString const& v) {
        return QString::fromStdString(c->escapeString(v.toStdString()));
    };
    // Identity (guid + entry) NOT updated -- the dialog locks them in
    // edit mode.  Phase + name + ScriptName are the editable fields.
    return QString(
        "UPDATE transports SET "
        "name='%1', phaseUseFlags=%2, phaseid=%3, phasegroup=%4, ScriptName='%5' "
        "WHERE guid=%6;")
        .arg(esc(r.name))
        .arg(int(r.phaseUseFlags)).arg(r.phaseId).arg(r.phaseGroup)
        .arg(esc(r.scriptName))
        .arg(qlonglong(r.guid));
}
} // namespace

void MainWindow::onAddTransport()
{
    if (!m_worldDb || !m_worldDb->isConnected())
        return;
    app::TransportRow seed;
    // If the operator has a GO spawn selected, seed guid+entry from it
    // so the common "promote this GO to a transport" workflow is one
    // click + Ok.
    if (m_selectedSpawnIndex >= 0 && m_spawnModel)
    {
        render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
        if (s.kind == render::SpawnKind::GameObject)
        {
            seed.guid  = s.guid;
            seed.entry = s.entry;
        }
    }
    app::TransportEditDialog editor(this);
    editor.setRow(seed);
    editor.setKeyEditable(true);
    if (editor.exec() != QDialog::Accepted)
        return;
    app::TransportRow const r = editor.rowSnapshot();
    if (r.guid == 0 || r.entry == 0)
    {
        QMessageBox::warning(this, tr("Bad transport"),
            tr("guid and entry must both be non-zero."));
        return;
    }
    QString const sql = formatTransportInsert(m_worldDb.get(), r);
    QString const summary = tr("Add transport row (guid=%1, entry=%2)")
        .arg(qlonglong(r.guid)).arg(r.entry);
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied() && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onEditTransport(qlonglong guid)
{
    if (!m_worldDb || !m_worldDb->isConnected()) return;
    app::TransportRow before;
    if (!loadTransportByGuid(m_worldDb.get(), int64_t(guid), before))
    {
        QMessageBox::warning(this, tr("Row not found"),
            tr("Could not re-read transports row guid=%1 -- maybe it was removed externally?").arg(guid));
        return;
    }
    app::TransportEditDialog editor(this);
    editor.setRow(before);
    editor.setKeyEditable(false);
    if (editor.exec() != QDialog::Accepted)
        return;
    app::TransportRow const after = editor.rowSnapshot();
    QString const sql = formatTransportUpdate(m_worldDb.get(), after);
    QString const summary = tr("Update transport row (guid=%1)").arg(qlonglong(after.guid));
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied() && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onRemoveTransport(qlonglong guid)
{
    if (!m_worldDb || !m_worldDb->isConnected()) return;
    QString const sql = QString("DELETE FROM transports WHERE guid=%1;").arg(qlonglong(guid));
    QString const summary = tr("Remove transports row guid=%1").arg(qlonglong(guid));
    app::ConfirmSqlDialog dlg(m_worldDb.get(), summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied() && m_diagDock)
        m_diagDock->refresh();
}

void MainWindow::onEditCreatureAddonForSelectedSpawn()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world database first."));
        return;
    }
    if (m_selectedSpawnIndex < 0 || !m_spawnModel)
        return;
    render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
    if (s.kind != render::SpawnKind::Creature)
    {
        QMessageBox::information(this, tr("creature_addon"),
            tr("creature_addon rows are creature-only."));
        return;
    }

    // Seed with table defaults; overwrite from DB if a row already exists.
    app::CreatureAddonRow row;
    row.guid = s.guid;

    char selectSql[320];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT guid, PathId, mount, StandState, AnimTier, VisFlags, SheathState, "
        "PvPFlags, emote, aiAnimKit, movementAnimKit, meleeAnimKit, "
        "visibilityDistanceType, auras FROM creature_addon WHERE guid = %lld",
        static_cast<long long>(s.guid));
    world_editor::db::QueryResult res;
    auto qErr = m_worldDb->query(selectSql, res);
    if (!qErr.ok())
    {
        QMessageBox::warning(this, tr("Query failed"),
            tr("SELECT creature_addon failed: %1").arg(QString::fromStdString(qErr.message)));
        return;
    }
    bool const existing = (res.rowCount() == 1);
    if (existing)
    {
        row.guid                   = res.asInt64 (0, 0).value_or(s.guid);
        row.pathId                 = static_cast<uint32_t>(res.asUInt64(0, 1).value_or(0));
        row.mount                  = static_cast<uint32_t>(res.asUInt64(0, 2).value_or(0));
        row.standState             = static_cast<uint8_t>(res.asUInt64(0, 3).value_or(0));
        row.animTier               = static_cast<uint8_t>(res.asUInt64(0, 4).value_or(0));
        row.visFlags               = static_cast<uint8_t>(res.asUInt64(0, 5).value_or(0));
        row.sheathState            = static_cast<uint8_t>(res.asUInt64(0, 6).value_or(1));
        row.pvpFlags               = static_cast<uint8_t>(res.asUInt64(0, 7).value_or(0));
        row.emote                  = static_cast<uint32_t>(res.asUInt64(0, 8).value_or(0));
        row.aiAnimKit              = static_cast<uint32_t>(res.asUInt64(0, 9).value_or(0));
        row.movementAnimKit        = static_cast<uint32_t>(res.asUInt64(0, 10).value_or(0));
        row.meleeAnimKit           = static_cast<uint32_t>(res.asUInt64(0, 11).value_or(0));
        row.visibilityDistanceType = static_cast<uint8_t>(res.asUInt64(0, 12).value_or(0));
        row.auras                  = QString::fromStdString(res.cell(0, 13));
    }

    app::CreatureAddonEditDialog dlg(this);
    dlg.setRow(row);
    dlg.setKeyEditable(!existing);  // PK locked once the row exists
    if (dlg.exec() != QDialog::Accepted)
        return;

    app::CreatureAddonRow const after = dlg.row();
    if (after.guid <= 0)
    {
        QMessageBox::warning(this, tr("Bad guid"),
            tr("creature_addon.guid must be > 0."));
        return;
    }

    // INSERT...ON DUPLICATE KEY UPDATE so the same SQL path handles both
    // create + edit.  Wrapped in a transaction; ROLLBACK on any error.
    std::string const aurasEsc = m_worldDb->escapeString(after.auras.toStdString());
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO creature_addon "
        "(guid, PathId, mount, StandState, AnimTier, VisFlags, SheathState, "
        " PvPFlags, emote, aiAnimKit, movementAnimKit, meleeAnimKit, "
        " visibilityDistanceType, auras) "
        "VALUES (%lld, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, '%s') "
        "ON DUPLICATE KEY UPDATE "
        "PathId=VALUES(PathId), mount=VALUES(mount), StandState=VALUES(StandState), "
        "AnimTier=VALUES(AnimTier), VisFlags=VALUES(VisFlags), SheathState=VALUES(SheathState), "
        "PvPFlags=VALUES(PvPFlags), emote=VALUES(emote), aiAnimKit=VALUES(aiAnimKit), "
        "movementAnimKit=VALUES(movementAnimKit), meleeAnimKit=VALUES(meleeAnimKit), "
        "visibilityDistanceType=VALUES(visibilityDistanceType), auras=VALUES(auras)",
        static_cast<long long>(after.guid),
        unsigned(after.pathId), unsigned(after.mount),
        unsigned(after.standState), unsigned(after.animTier), unsigned(after.visFlags),
        unsigned(after.sheathState), unsigned(after.pvpFlags),
        unsigned(after.emote), unsigned(after.aiAnimKit),
        unsigned(after.movementAnimKit), unsigned(after.meleeAnimKit),
        unsigned(after.visibilityDistanceType),
        aurasEsc.c_str());

    auto err = m_worldDb->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Transaction"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_worldDb->exec(sql, &affected);
    if (!err.ok())
    {
        (void)m_worldDb->exec("ROLLBACK");
        QMessageBox::warning(this, tr("creature_addon"),
            tr("UPSERT failed; transaction rolled back.\n\n%1")
                .arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_worldDb->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_worldDb->exec("ROLLBACK");
        QMessageBox::warning(this, tr("creature_addon"),
            tr("COMMIT failed; transaction rolled back.\n\n%1")
                .arg(QString::fromStdString(err.message)));
        return;
    }
    statusBar()->showMessage(
        tr("creature_addon %1 guid=%2 (rows affected=%3)")
            .arg(existing ? tr("updated") : tr("inserted"))
            .arg(qlonglong(after.guid))
            .arg(qulonglong(affected)),
        4000);
}

void MainWindow::onEditGameObjectAddonForSelectedSpawn()
{
    if (!m_worldDb || !m_worldDb->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world database first."));
        return;
    }
    if (m_selectedSpawnIndex < 0 || !m_spawnModel)
        return;
    render::Spawn const& s = m_spawnModel->current()[m_selectedSpawnIndex];
    if (s.kind != render::SpawnKind::GameObject)
    {
        QMessageBox::information(this, tr("gameobject_addon"),
            tr("gameobject_addon rows are gameobject-only."));
        return;
    }

    // Seed with table defaults; overwrite from DB if a row already exists.
    app::GameObjectAddonRow row;
    row.guid = s.guid;

    char selectSql[320];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT guid, parent_rotation0, parent_rotation1, parent_rotation2, parent_rotation3, "
        "invisibilityType, invisibilityValue, WorldEffectID, AIAnimKitID "
        "FROM gameobject_addon WHERE guid = %lld",
        static_cast<long long>(s.guid));
    world_editor::db::QueryResult res;
    auto qErr = m_worldDb->query(selectSql, res);
    if (!qErr.ok())
    {
        QMessageBox::warning(this, tr("Query failed"),
            tr("SELECT gameobject_addon failed: %1").arg(QString::fromStdString(qErr.message)));
        return;
    }
    bool const existing = (res.rowCount() == 1);
    if (existing)
    {
        row.guid              = res.asInt64 (0, 0).value_or(s.guid);
        row.parentRotation0   = static_cast<float>(res.asDouble(0, 1).value_or(0.0));
        row.parentRotation1   = static_cast<float>(res.asDouble(0, 2).value_or(0.0));
        row.parentRotation2   = static_cast<float>(res.asDouble(0, 3).value_or(0.0));
        row.parentRotation3   = static_cast<float>(res.asDouble(0, 4).value_or(1.0));
        row.invisibilityType  = static_cast<uint8_t>(res.asUInt64(0, 5).value_or(0));
        row.invisibilityValue = static_cast<uint32_t>(res.asUInt64(0, 6).value_or(0));
        row.worldEffectId     = static_cast<uint32_t>(res.asUInt64(0, 7).value_or(0));
        row.aiAnimKitId       = static_cast<uint32_t>(res.asUInt64(0, 8).value_or(0));
    }

    app::GameObjectAddonEditDialog dlg(this);
    dlg.setRow(row);
    dlg.setKeyEditable(!existing);  // PK locked once the row exists
    if (dlg.exec() != QDialog::Accepted)
        return;

    app::GameObjectAddonRow const after = dlg.row();
    if (after.guid <= 0)
    {
        QMessageBox::warning(this, tr("Bad guid"),
            tr("gameobject_addon.guid must be > 0."));
        return;
    }

    // INSERT...ON DUPLICATE KEY UPDATE so the same SQL path handles both
    // create + edit.  Wrapped in a transaction; ROLLBACK on any error.
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO gameobject_addon "
        "(guid, parent_rotation0, parent_rotation1, parent_rotation2, parent_rotation3, "
        " invisibilityType, invisibilityValue, WorldEffectID, AIAnimKitID) "
        "VALUES (%lld, %.6f, %.6f, %.6f, %.6f, %u, %u, %u, %u) "
        "ON DUPLICATE KEY UPDATE "
        "parent_rotation0=VALUES(parent_rotation0), parent_rotation1=VALUES(parent_rotation1), "
        "parent_rotation2=VALUES(parent_rotation2), parent_rotation3=VALUES(parent_rotation3), "
        "invisibilityType=VALUES(invisibilityType), invisibilityValue=VALUES(invisibilityValue), "
        "WorldEffectID=VALUES(WorldEffectID), AIAnimKitID=VALUES(AIAnimKitID)",
        static_cast<long long>(after.guid),
        double(after.parentRotation0), double(after.parentRotation1),
        double(after.parentRotation2), double(after.parentRotation3),
        unsigned(after.invisibilityType), unsigned(after.invisibilityValue),
        unsigned(after.worldEffectId), unsigned(after.aiAnimKitId));

    auto err = m_worldDb->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Transaction"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_worldDb->exec(sql, &affected);
    if (!err.ok())
    {
        (void)m_worldDb->exec("ROLLBACK");
        QMessageBox::warning(this, tr("gameobject_addon"),
            tr("UPSERT failed; transaction rolled back.\n\n%1")
                .arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_worldDb->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_worldDb->exec("ROLLBACK");
        QMessageBox::warning(this, tr("gameobject_addon"),
            tr("COMMIT failed; transaction rolled back.\n\n%1")
                .arg(QString::fromStdString(err.message)));
        return;
    }
    statusBar()->showMessage(
        tr("gameobject_addon %1 guid=%2 (rows affected=%3)")
            .arg(existing ? tr("updated") : tr("inserted"))
            .arg(qlonglong(after.guid))
            .arg(qulonglong(affected)),
        4000);
}

// Top-of-window QToolBar with the most-frequently-used actions.  Each
// button uses a Unicode-glyph placeholder icon; full SVGs ship later.
void MainWindow::buildQuickToolbar()
{
    auto* bar = addToolBar(tr("Quick actions"));
    bar->setObjectName(QStringLiteral("quick_toolbar"));
    bar->setMovable(true);
    bar->setIconSize(QSize(20, 20));

    auto add = [&](QString const& glyph, QString const& tip,
                   auto handler) -> QAction* {
        QAction* a = bar->addAction(glyph);
        a->setToolTip(tip);
        connect(a, &QAction::triggered, this, handler);
        return a;
    };

    add(QStringLiteral("\xF0\x9F\x94\x8D"), tr("Spawn search"),
        [this]() { onShowSpawnSearchDialog(); });
    add(QStringLiteral("\xF0\x9F\x93\xA6"), tr("Loot table editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Loot table"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::CreatureLootEditDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xF0\x9F\x90\xBA"), tr("Creature template addon editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Creature template addon"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::CreatureTemplateAddonDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xF0\x9F\x93\xA2"), tr("Broadcast text editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Broadcast text"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::BroadcastTextDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xF0\x9F\x93\x9C"), tr("Quest dialog text editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Quest dialog text"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::QuestDialogTextDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xF0\x9F\x8E\x83"), tr("Game events editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Game events"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::GameEventEditDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xF0\x9F\x92\xB0"), tr("NPC vendor editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("NPC vendor"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::NpcVendorDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xF0\x9F\x97\xA8"), tr("Gossip menu editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Gossip menu"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::GossipMenuEditDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
    add(QStringLiteral("\xE2\x9C\xA8"), tr("Areatrigger teleports editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("Areatrigger teleports"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::AreaTriggerTeleportDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            connect(dlg, &app::AreaTriggerTeleportDialog::jumpRequested, this,
                    [this](uint32_t mapId, float x, float y) {
                onJumpRequested(mapId, x, y, std::nullopt);
            });
            dlg->show();
        });
    add(QStringLiteral("\xE2\x9A\xB0"), tr("World safe locs editor"),
        [this]() {
            if (!m_worldDb || !m_worldDb->isConnected())
            {
                QMessageBox::warning(this, tr("World safe locs"), tr("Connect to the world DB first."));
                return;
            }
            QString const dbName = QString::fromStdString(
                db::ConnectionDialog::loadProfile(QStringLiteral("world")).database);
            auto* dlg = new app::WorldSafeLocsDialog(m_worldDb.get(),
                dbName.isEmpty() ? QStringLiteral("playerbot_world") : dbName, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            connect(dlg, &app::WorldSafeLocsDialog::jumpRequested, this,
                    [this](uint32_t mapId, float x, float y) {
                onJumpRequested(mapId, x, y, std::nullopt);
            });
            dlg->show();
        });

    bar->addSeparator();
    add(QStringLiteral("\xE2\x84\xB9"), tr("Toggle Info Inspector"),
        [this]() { onToggleInfoInspectorDock(); });
    add(QStringLiteral("\xF0\x9F\x9B\xA0"), tr("Toggle Property Inspector"),
        [this]() { onTogglePropertyInspectorDock(); });
    add(QStringLiteral("\xF0\x9F\x9B\xA3"), tr("Handcrafted roads"),
        [this]() { onToggleHandcraftedRoadDock(); });

    bar->addSeparator();
    add(QStringLiteral("\xF0\x9F\x94\x84"), tr("Reload current map"),
        [this]() { onReloadCurrentMap(); });
    add(QStringLiteral("\xF0\x9F\xA9\xBA"), tr("Health report"),
        [this]() { onShowHealthReport(); });
}

void MainWindow::onReloadCurrentMap()
{
    if (!m_currentMapId.has_value())
    {
        statusBar()->showMessage(tr("Reload: no map currently loaded."), 3000);
        return;
    }
    uint32_t const mapId = *m_currentMapId;
    loadAndDisplayMap(mapId);
    statusBar()->showMessage(tr("Reloaded map %1.").arg(mapId), 3000);
}

void MainWindow::onToggleInfoInspectorDock()
{
    auto* dock = findChild<QDockWidget*>(QStringLiteral("info_inspector_dock"));
    if (!dock) return;
    if (dock->isVisible())
        dock->hide();
    else
    {
        dock->setFloating(false);
        dock->show();
        dock->raise();
    }
}

void MainWindow::onTogglePropertyInspectorDock()
{
    auto* dock = findChild<QDockWidget*>(QStringLiteral("property_inspector_dock"));
    if (!dock) return;
    if (dock->isVisible())
        dock->hide();
    else
    {
        dock->setFloating(false);
        dock->show();
        dock->raise();
    }
}

void MainWindow::onShowPoolEditorFromInspector()
{
    onShowGroupsPoolsDialog();
}

void MainWindow::onShowSpawnGroupEditorFromInspector()
{
    // Hand off to the existing groups/pools modal — it carries both
    // tabs (Pool template + Spawn group template) so a single launch
    // surface covers both inspector tabs.
    onShowGroupsPoolsDialog();
}

} // namespace world_editor
