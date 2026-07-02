/*
 * MainWindow - shell for the TrinityCore world editor.
 *
 * Phase 0: empty window + menu skeleton (File / View / Help) so we can
 * confirm the build links cleanly against Qt 6 and the editor process
 * launches. The 2D viewer, DB connection panel and property inspector
 * are added in subsequent phases - see src/tools/world_editor/
 * CMakeLists.txt comment for the phase plan and ../../HANDOFF_NATIVE_EDITOR.md
 * for the full design doc.
 */

#pragma once

#include "../io/VmapHeightProbe.h"
#include "../render/NavMeshView.h"   // brings render::Path full definition

#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QVector>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QAction;
class QLabel;
class QLineEdit;
class QPlainTextEdit; // used by the annotation dock (text-only fallback)
class QMenu;
class QStackedWidget;

namespace world_editor
{

namespace render { class NavMeshView; class SceneView3D; struct Spawn; struct Path; struct Areatrigger; struct Graveyard; }
namespace db     { class MySqlClient; class AnnotationModel; class SpawnModel; class WaypointModel;
                   class AreatriggerModel; class GraveyardModel; class ConditionsModel;
                   struct ConnectionParams; }
namespace io     { class MapTileCache; class CascClient; class MapDb2Lookup; class ListfileLookup; }
namespace app    { class AnnotationToolbox; class SpawnPropertiesEditor; class PathPropertiesDock;
                   class AreatriggerPropertiesDock; class GraveyardPropertiesDock;
                   class AnnotationPropertiesDock; class UndoManager;
                   class VendorInventoryDock; class TrainerSpellDock; class ConditionsDock;
                   class LootTableDock; class QuestRewardDock;
                   class SpellInfoDock; class ItemInfoDock;
                   class GameObjectInfoDock;
                   class CurrencyTypeDock;
                   class AreaInfoDock;
                   class ZoneSummaryDock;
                   class FactionTemplateDock;
                   class PlayerConditionDock;
                   class NpcTextDock;
                   class AreatriggerScriptDock;
                   class LogTailDock;
                   class SpawnDiagnosticsDock; class MinimapDiagnosticsDock;
                   class InfoInspectorDock; class PropertyInspectorDock;
                   class HandcraftedRoadDock;
                   struct PickedTemplate; }

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Configuration injected from main() based on CLI args. Used during
    // showEvent() if a startup map was requested via --open.
    void setMMapsDir(QString const& dir);
    void setMapsDir(QString const& dir);
    void setVmapsDir(QString const& dir);
    void requestOpenMap(uint32_t mapId);

    // Headless visual-oracle: open `mapId`, point the 3D camera at
    // (camX,camY,camZ,yaw,pitch radians), force realistic textures, let async
    // tiles stream for `waitMs`, then grab the 3D framebuffer to `outPath` PNG.
    // Drives the REAL SceneView3D render so the saved image is what an operator
    // would see. Returns true on a successful PNG write. Used by --screenshot.
    bool renderToPng(uint32_t mapId, float camX, float camY, float camZ,
                     float yaw, float pitch, bool realistic,
                     int w, int h, int waitMs, QString const& outPath);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onOpenMmapsDir();
    void onOpenMapsDir();
    void onOpenMap();
    // File -> Recent maps submenu rebuild + clear handlers.  Submenu
    // rebuilds itself on aboutToShow so newly-opened maps appear without
    // an editor restart.  Persisted in QSettings under "editor/recent_maps"
    // as a semicolon-separated "<mapId>|<dirName>|<unix-secs>" list, capped
    // at 15 entries with dedupe on mapId.
    void onRebuildRecentMapsMenu();
    void onClearRecentMaps();
    void onAbout();
    void onShowShortcuts();
    void onShowCommandPalette();
    void onFrameView();
    void onToggleNavLayer(bool checked);
    void onToggleSpawnLayer(bool checked);
    void onToggleHeightmapLayer(bool checked);
    void onTogglePathLayer(bool checked);
    void onSwitchTo2D();
    void onSwitchTo3D();
    void onViewerHover(float worldX, float worldY);
    void onViewerClick(float worldX, float worldY);
    void onSpawnClicked(int spawnIndex);
    void onSpawnHovered(int spawnIndex);
    void onAnnotationClicked(int annotationIndex);
    void onAnnotationHovered(int annotationIndex);
    void onToggleAnnotationLayer(bool checked);
    void onPlaceModeChanged(bool placing);
    void onPlacementRequested(float worldX, float worldY);
    // 3D-viewer overload: SceneView3D already resolved the exact surface Z by
    // depth-unprojecting the click, so we place at the authored (x, y, z)
    // verbatim instead of snapping to ground at runtime.  Threads the Z
    // through m_authoredZ and delegates to onPlacementRequested.
    void onPlacementRequested3D(float worldX, float worldY, float worldZ);
    // 3D placement pick: march the click ray against terrain/WMO height
    // (snapToGround) to find the surface, then place there with authored Z.
    void onPlacementRay(float ox, float oy, float oz, float dx, float dy, float dz);
    void onSelectedRadiusChanged(float newRadius);
    void onSelectedLabelChanged(QString const& newLabel);
    void onSelectedNotesChanged(QString const& newNotes);
    void onAnnotationRowEdited(int index, render::Annotation const& proposed);
    void onDeleteSelectedAnnotation();
    void onCommitAnnotations();
    void onRevertAnnotations();
    void onSpawnRowEdited(render::Spawn const& proposed);
    void onSpawnMoved(int spawnIndex, float worldX, float worldY);
    // 3D drag-to-move: carries the drag-plane Z so altitude is preserved for
    // flying mobs (snap off) or re-grounded (snap on).  Both slots funnel
    // into applySpawnMove.
    void onSpawnMoved3D(int spawnIndex, float worldX, float worldY, float worldZ);
    void onSpawnSelectionChanged(QVector<int> const& indices);
    void onDeleteSelectedSpawn();
    void onBulkEditSpawns();
    void onBulkTransformSpawns();
    // Spawn -> Diff selected pair... Modal field-by-field comparison.
    // Enabled only when m_spawnSelection.size() == 2.
    void onDiffSelectedSpawnPair();
    // Spawn -> Propagate fields from selection... copies selected fields
    // from the single selected canonical spawn to every other spawn that
    // shares its kind + entry + mapId.  Enabled only when exactly one
    // spawn is selected.
    void onPropagateFieldsFromSelection();
    void onPropagateRequested(render::Spawn const& canonical, QSet<QString> selectedFields);
    void onTransformRequested(float dx, float dy, float dz,
                              float rotDegrees, bool rotateAroundCentroid,
                              float scale);
    void onCommitSpawns();
    void onRevertSpawns();
    void onNewSpawnFromTemplate();   // opens TemplatePickerDialog, arms placement mode
    void onImportSpawnsFromCsv();    // opens CsvImportDialog, batch-inserts into m_spawnModel
    // File -> Export spawns to CSV...  Writes selection (if any) or every spawn on the
    // currently loaded map to a UTF-8 CSV using the same header CsvImportDialog accepts,
    // so an operator can round-trip export -> edit -> re-import.  Floats use 3 decimals
    // for x/y/z/orientation and 1 decimal for wanderDistance; strings are quoted only
    // when they contain comma, newline or quote (with internal quotes doubled).
    void onExportSpawnsToCsv();
    void onNewPath();
    void onFinishPath();
    void onCancelPath();
    void onArmAutoRoute();
    void onStartRoadDraw();
    void onFinishRoadDraw();
    void onPathClicked(int pathIndex);
    void onPathNodeClicked(int pathIndex, int nodeIndex);
    void onPathNodeMoved(int pathIndex, int nodeIndex, float worldX, float worldY);
    // 3D variants: carry the authored Z (drag-plane / segment-lerped) and
    // only ground-snap when the ground is near it (multi-floor guard).
    void onPathNodeMoved3D(int pathIndex, int nodeIndex,
                           float worldX, float worldY, float worldZ);
    void onPathSegmentContextMenu3D(int pathIndex, int afterNodeIndex,
                                    float worldX, float worldY, float worldZ,
                                    QPoint globalPos);
    // ---- Bot dungeon-route chain (shared-schema playerbot_dungeon_routes).
    // Loaded per map, edited in the 3D view, committed as a full-map rewrite.
    // NOTE: dungeon scripts that hand-author route_waypoints in C++ (currently
    // Deadmines) override the DB rows at runtime -- edits there only take
    // effect once the script's authored chain is removed.
    void onRouteNodeMoved3D(int routeIndex, int nodeIndex,
                            float worldX, float worldY, float worldZ);
    void onRouteNodeContextMenu3D(int routeIndex, int nodeIndex, QPoint globalPos);
    void onRouteSegmentContextMenu3D(int routeIndex, int afterNodeIndex,
                                     float worldX, float worldY, float worldZ,
                                     QPoint globalPos);
    void onReloadDungeonRoutes();
    void onCommitDungeonRoutes();
    void onNewDungeonRouteAtCamera();
    void onPathNodeContextMenu(int pathIndex, int nodeIndex, QPoint globalPos);
    void onPathSegmentContextMenu(int pathIndex, int afterNodeIndex,
                                   float worldX, float worldY, QPoint globalPos);
    // Right-click on a spawn icon: open context menu with "Clone..." entry.
    void onSpawnContextMenu(int spawnIndex, QPoint globalPos);
    // Apply slot for SpawnCloneDialog::cloneRequested: produces `count` copies
    // of m_cloneSourceSpawnIndex with offsets derived from `patternIdx`/`radius`.
    void onCloneRequested(int count, int patternIdx, float radius, bool snap, bool preserveOri);
    void onPathEdited(render::Path const& proposed);
    void onDeleteSelectedPath();
    void onCommitPaths();
    void onRevertPaths();
    void onAssignPathToSelectedSpawn();
    void onShowGroupsPoolsDialog();
    void onShowGraveyardZoneDialog();
    void onShowFindJumpDialog();
    // View -> Bookmarks: rebuild the submenu from the persisted list
    // (called whenever the list might have changed) and the dialog
    // handlers for Manage... + Add current.
    void onRebuildBookmarksMenu();
    void onShowBookmarksManager();
    void onAddBookmarkAtCurrentView();
    void onShowSpawnSearchDialog();
    // Spawn -> Find similar spawns... Modal "find another like this one"
    // search keyed off the currently selected spawn.  Enabled only when
    // exactly one spawn is selected.
    void onShowFindSimilarDialog();
    void onResetWindowLayout();
    // MinimapDiagnosticsDock plumbing.  onShowMinimapDiagnostics raises the
    // dock + forces an immediate refresh; onRefreshMinimapDiagnostics pulls
    // fresh state out of NavMeshView + MainWindow config and pushes it via
    // MinimapDiagnosticsDock::setMinimapInfo().
    void onShowMinimapDiagnostics();
    void onRefreshMinimapDiagnostics();
    // Quick toolbar handlers (UI declutter).  Reload re-issues the
    // current loadAndDisplayMap; ToggleInfo/ToggleProperty flip the
    // visibility of the unified right-side docks.
    void onReloadCurrentMap();
    void onToggleInfoInspectorDock();
    void onTogglePropertyInspectorDock();
    void onShowPoolEditorFromInspector();
    void onShowSpawnGroupEditorFromInspector();
    // Tools -> Handcrafted roads...: toggle dock visibility + raise.
    void onToggleHandcraftedRoadDock();
    // HandcraftedRoadDock::addSegmentRequested handler.  Flips the 2D
    // viewer into segment-placement mode; the second click forwards back
    // to the dock via onHandcraftedSegmentPlaced.
    void onHandcraftedAddSegmentRequested();
    void onHandcraftedCancelAddSegment();
    void onHandcraftedSegmentPlaced(float fromX, float fromY, float toX, float toY);
    // View -> Theme -> {System,Light,Dark}: switch the application palette
    // + persist in QSettings under "ui/theme".  Applied at startup from
    // restoreSettings() so the chosen theme survives across editor runs.
    void onSelectTheme(QString const& name);
    void onUndo();
    void onRedo();
    void onUndoStateChanged(QString const& label);
    void refreshAllViewers();
    void onAutoTagNpcs();
    void onShowHealthReport();
    void onShowSmartScriptFlow();
    void onShowSmartScriptDryRun();
    void onPlayPathIn3D();
    void loadSpawnGroupColors();
    // Build the entry->faction-group map for the currently-loaded map
    // and push it to the viewer.  Tries faction_template_dbc first; if
    // that table isn't present, falls back to a hard-coded classifier
    // over the most common creature_template.faction values.
    void loadFactionTintMap();
    // Build the entry->(minLevel, maxLevel) map for the currently-loaded
    // map and push it to the viewer.  Used by the Level heatmap overlay
    // to color spawns by content-level bracket.  TC's int8 -1 sentinel
    // for boss/world-boss levels is normalized to 0 here so the viewer
    // can flag it as magenta.
    void loadLevelMap();
    void loadQuestMarkers();
    // Probe taxi_nodes / taxi_path for nodes + edges on the current
    // map and push them to the viewer.  Silently no-ops when the tables
    // are absent (DB2-only deployments); a one-time status-bar note
    // makes the absence visible to the operator.
    void loadFlightGraph();
    // Probe transports + transport_animation (or transport_template +
    // transport_keyframes on schema variants) for keyframe polylines on
    // the current map.  Schema-tolerant: falls back to raw keyframes
    // as world positions when no spawn-offset lookup is wired.  Silent
    // empty layer when neither schema is present.
    void loadTransportRoutes();
    // Classify gameobject spawns on the current map as mining vein /
    // herb node / fishing pool / treasure goober by joining gameobject
    // with gameobject_template (type + name).  Pushes the resulting
    // guid -> kind map to the viewer.  No-op when no map is loaded.
    void loadGatheringNodes();
    // Probe areatrigger_teleport / areatrigger_teleport_dbc + join with
    // areatrigger spawn rows on the current map to extract entrance
    // positions + target map ids.  Schema-tolerant: tries the modern
    // areatrigger_teleport keyed on SpawnId first, then falls back to
    // the legacy areatrigger_teleport_dbc keyed on areatrigger ID.
    // Pushes the resulting list to the viewer; empty when no DB schema
    // matches.
    void loadInstanceEntrances();
    // Probe world.linked_respawn for every dependency row touching a
    // creature spawn on the current map and push the (from, to) pairs to
    // the viewer.  Silently empty on schema mismatch (some forks rename
    // the columns or use BIGINT signed).  Status bar reports the loaded
    // count on success.
    void loadSpawnLinks();
    // Push cached WMO footprints to the 2D viewer.  If the cache is empty
    // and the operator hasn't loaded vmaps yet, surface a one-shot status
    // bar tip pointing at File -> Set vmaps directory.  Safe to call when
    // the layer is OFF; it's a no-op in that case.
    void pushWmoFootprintsToViewer(bool layerOn);
    // Apply the currently-saved phase filter to m_viewer + refresh the
    // spawn buffers + update the status bar feedback line.  Called from
    // the View -> Phase filter menu actions and from restoreSettings()
    // so the filter survives across editor restarts.
    void applyPhaseFilter();
    // Workspace preset menu: flip a named combo of layer toggles in one
    // click.  Walks m_layerToggles to set each layer's check-state (which
    // fires the existing toggled() slot, so persistence + viewer push
    // happen for free), handles path-debug + phase filter explicitly,
    // and persists the last-applied preset in QSettings under
    // "viewer/last_preset" so the editor restores to it at startup.
    void applyWorkspacePreset(QString const& name);
    void onJumpRequested(uint32_t mapId, float worldX, float worldY,
                         std::optional<int64_t> guid);
    // Status-bar goto line edit returnPressed handler.  Parses the
    // operator-typed expression and either pans the viewer via
    // onJumpRequested or surfaces a 3-second status-bar error.
    void onGotoLineEditReturn();
    void onAddSmartScript();
    void onEditSmartScript(qlonglong entryorguid, int sourceType, int id, int link);
    void onRemoveSmartScript(qlonglong entryorguid, int sourceType, int id, int link);
    void onAddTransport();
    void onEditTransport(qlonglong guid);
    void onRemoveTransport(qlonglong guid);
    // Open the creature_addon editor for the currently-selected creature spawn.
    // Reads the existing row (if any) and runs INSERT...ON DUPLICATE KEY UPDATE
    // inside a transaction with rollback on error.  Not undo-tracked.
    void onEditCreatureAddonForSelectedSpawn();
    // Open the gameobject_addon editor for the currently-selected gameobject
    // spawn.  Same INSERT...ON DUPLICATE KEY UPDATE transactional path as the
    // creature variant.  Not undo-tracked.
    void onEditGameObjectAddonForSelectedSpawn();
    void onHighlightSpawnGuids(QVector<qlonglong> const& guids);
    void onAddSelectedSpawnToGroup();
    void onToggleAreatriggerLayer(bool checked);
    void onToggleGraveyardLayer(bool checked);
    void onAreatriggerClicked(int idx);
    void onGraveyardClicked(int idx);
    void onAreatriggerEdited(render::Areatrigger const& proposed);
    void onDeleteSelectedAreatrigger();
    void onCommitAreatriggers();
    void onRevertAreatriggers();
    void onNewAreatriggerFromCreateProps();
    void onGraveyardEdited(render::Graveyard const& proposed);
    void onDeleteSelectedGraveyard();
    void onCommitGraveyards();
    void onRevertGraveyards();
    void onNewGraveyard();
    void onAddLinkedRespawn(qlonglong fromGuid, qlonglong toGuid, int linkType);
    void onRemoveLinkedRespawn(qlonglong guid, int linkType);
    void onAddGameEvent(int eventEntry);
    void onRemoveGameEvent(int eventEntry);
    void onToggleWmoLayer(bool checked);
    // Toggle the SceneView3D realistic (textured + lit) terrain + WMO
    // pass.  When off, the legacy per-vertex coloured terrain + flat
    // translucent WMO overlay are drawn instead.  Persisted in QSettings
    // under "viewer3d/realistic".
    void onToggleRealistic3D(bool checked);
    void onOpenVmapsDir();
    void onSetCascClientDir();
    // File -> Set listfile CSV... picker + setter.  Loads a wow-listfile
    // CSV into m_listfile + persists the path in QSettings so the editor
    // re-loads it on next startup.  Required for live CASC minimap reads
    // on modern (TWW+) client data where many BLPs are FDID-only.
    void onSetListfileCsv();
    // File -> Set minimap directory... picker + setter.  Extracted from the
    // original inline lambda in buildMenus() so MinimapSetupWizard can drive
    // the same code path.
    void onSetMinimapDir();
    void onExportMinimapCache();
    // View -> Minimap texture layer toggled().  Pushes the visibility into
    // the viewer and, on first enable without any source configured, surfaces
    // the MinimapSetupWizard so the operator knows how to fix the empty
    // overlay.
    void onToggleMinimapLayer(bool on);
    // File -> Export view as PNG... handlers.  Grab the currently visible
    // central widget (2D NavMeshView or 3D SceneView3D, both QOpenGLWidget)
    // via grabFramebuffer() and save to a user-picked path.  The 4x variant
    // temporarily upscales the widget for printable / documentation use and
    // restores the original size + visibility in all paths (incl. save fail).
    void onExportViewAsPng();
    void onExportViewAsPngHighRes();
    void onDbConnect();
    void onDbDisconnect();
    // File -> Export pending changes... handler.  Writes one INSERT/
    // UPDATE/DELETE per pending row across every model into a single
    // .sql file (no DB writes).  Enabled only when at least one model
    // has pending changes.
    void onExportPendingChanges();
    // Recompute the enabled state of m_exportPendingAction by polling
    // each model's pendingCount().  Called from buildMenus() at startup
    // and from refreshAllViewers() after any commit/revert/edit.
    void updateExportPendingActionEnabled();

private:
    void buildMenus();
    void buildStatusBar();
    void buildCentralWidget();
    void buildSpawnDock();
    void buildAnnotationToolbox();
    // Unified right-side docks (UI declutter).  Replace the prior fan-out
    // of buildPathDock + buildAreatriggerDock + buildGraveyardDock + the
    // ~14 info docks built inline in buildSpawnDock with two single-dock
    // hosts: InfoInspectorDock + PropertyInspectorDock.
    void buildInfoInspectorDock();
    void buildPropertyInspectorDock();
    // Handcrafted-road CRUD dock (Tools menu + toolbar entry point).
    // Stand-alone QDockWidget on the right side; hidden by default so
    // the operator only sees it after Tools -> Handcrafted roads...
    void buildHandcraftedRoadDock();
    // Top-of-window icon toolbar with the most-frequently-used actions.
    void buildQuickToolbar();
    void pushAnnotationsToViewer();
    void pushSpawnsToViewer();
    void pushPathsToViewer();
    void pushAreatriggersToViewer();
    void pushGraveyardsToViewer();
    // Re-query world DB for MAX(SpawnId) in `areatrigger`; reserve a
    // local block starting at MAX+1 (mirrors refreshGuidReservation).
    void refreshAreatriggerSpawnIdReservation();
    // Re-query world DB for MAX(ID) in `world_safe_locs`; reserve from
    // MAX+1 for new graveyard placements.
    void refreshGraveyardIdReservation();
    // Snap to the highest of ADT terrain Z and any WMO floor below
    // `probeZ`.  Returns `fallbackZ` (typically the caller's current Z)
    // when neither layer has a hit at (worldX, worldY).  Used by every
    // placement path (spawn / areatrigger / graveyard / path node).
    [[nodiscard]] float snapToGround(uint32_t mapId,
                                     float worldX, float worldY,
                                     float probeZ, float fallbackZ) const;
    // Shared post-connect setup: status label, hook diag dock,
    // GUID/SpawnId reservations, and reload-for-current-map if a map is
    // already open.  Returns true on success; on failure outErrorMessage
    // is set and m_worldDb is reset.  Both onDbConnect (interactive) and
    // tryAutoConnectWorldDb (silent) call this.
    [[nodiscard]] bool finishWorldDbConnect(db::ConnectionParams const& params,
                                            QString& outErrorMessage);
    // Silent startup auto-connect from the saved "world" profile.  No
    // popup on failure -- the operator sees "DB: disconnected" and can
    // recover via Database -> Connect...
    void tryAutoConnectWorldDb();
    // Query world DB for the current MAX(guid) on creature and
    // gameobject; reserve a local block starting at MAX+1. Called on
    // connect and after every successful spawn commit.
    void refreshGuidReservation();
    void restoreSettings();
    void saveSettings();
    // Apply one of "system" / "light" / "dark" to QApplication: sets the
    // Fusion style + the matching QPalette.  "system" resets to the
    // default palette built from the active style.  Pure UI plumbing;
    // does not touch QSettings.  Static so main.cpp can also pre-apply
    // the persisted theme before the MainWindow is constructed.
    static void applyTheme(QString const& name);
    void loadAndDisplayMap(uint32_t mapId);
    // Re-query world DB for creatures/gameobjects on the active map.
    void reloadSpawnsForMap(uint32_t mapId);
    // Re-query characters.playerbot_v2_world_metadata for the active map.
    void reloadAnnotationsForMap(uint32_t mapId);
    void reloadPathsForMap(uint32_t mapId);
    void reloadAreatriggersForMap(uint32_t mapId);
    void reloadGraveyardsForMap(uint32_t mapId);
    void closeEvent(QCloseEvent* event) override;

    QString m_mmapsDir;
    QString m_mapsDir;
    QString m_vmapsDir;
    QString m_minimapDir;
    QString m_cascClientDir;
    // CASC + Map.db2 reader.  Owned by MainWindow; passed by borrowed
    // pointer to the NavMeshView so the minimap loader can fall back to
    // live CASC reads when PNGs aren't on disk.  Both are reset on path
    // change and re-opened lazily.
    std::unique_ptr<io::CascClient>   m_cascClient;
    std::unique_ptr<io::MapDb2Lookup> m_mapDb2;
    // wow-listfile FDID resolver.  Loaded from a CSV the operator picks
    // via File -> Set listfile CSV; persisted under paths/listfile_csv.
    // Borrowed pointer is handed to NavMeshView so its minimap loader can
    // resolve "world/minimaps/<dir>/map_X_Y.blp" paths to FDIDs.
    std::unique_ptr<io::ListfileLookup> m_listfile;
    QString                             m_listfileCsvPath;
    // Opens m_cascClient + loads m_mapDb2 for m_cascClientDir.  Pushes
    // both into the viewer on success.  Returns false on failure and
    // fills outError.
    [[nodiscard]] bool openCascAndMapDb2(QString& outError);
    std::optional<uint32_t> m_pendingOpenMapId;
    bool                    m_autoConnectAttempted = false;
    // The 3D viewer's mesh is loaded lazily on first 3D activation so a map
    // open doesn't pay the cost of a second io::loadMap (which iterates the
    // whole mmaps directory and parses 1000+ .mmtile files).  Holds the
    // mapId queued for the 3D viewer; the value is consumed by
    // ensureViewer3dMeshLoaded() when the user switches to 3D view.
    std::optional<uint32_t> m_pendingViewer3dMapId;
    // The mapId for which the 3D viewer's mesh is currently loaded.  When
    // the user switches map, m_pendingViewer3dMapId is set and this stays
    // pointing at the prior map until the deferred load fires.
    std::optional<uint32_t> m_viewer3dMapLoaded;
    void ensureViewer3dMeshLoaded();

    render::NavMeshView*             m_viewer            = nullptr;
    render::SceneView3D*             m_viewer3d          = nullptr;
    ::QStackedWidget*                m_centralStack      = nullptr;
    QLabel*                          m_coordsLabel       = nullptr;
    // Permanent editing-mode badge (leftmost status-bar widget).  Updated
    // exclusively by updateModeBadge() via setPlacementKind().
    QLabel*                          m_modeBadge         = nullptr;
    QLabel*                          m_meshStatsLabel    = nullptr;
    QLabel*                          m_spawnStatsLabel   = nullptr;
    QLabel*                          m_annotStatsLabel   = nullptr;
    QLabel*                          m_dbStatusLabel     = nullptr;
    // Status-bar coordinate-jump quick-input.  Accepts "X Y", "X Y mapId",
    // "/<entry>" (creature_template.entry on current map), "#<guid>"
    // (creature.guid lookup) or "?<text>" (creature_template.name LIKE
    // match).  Parse failures + lookup misses surface on the status bar.
    QLineEdit*                       m_gotoEdit          = nullptr;
    app::AnnotationPropertiesDock*   m_annotPropertyDock = nullptr;
    app::UndoManager*                m_undo              = nullptr;
    app::VendorInventoryDock*        m_vendorDock        = nullptr;
    app::TrainerSpellDock*           m_trainerDock       = nullptr;
    app::ConditionsDock*             m_conditionsDock    = nullptr;
    app::LootTableDock*              m_lootDock          = nullptr;
    app::QuestRewardDock*            m_questRewardDock   = nullptr;
    app::SpellInfoDock*              m_spellDock         = nullptr;
    app::ItemInfoDock*               m_itemDock          = nullptr;
    app::GameObjectInfoDock*         m_goInfoDock        = nullptr;
    app::CurrencyTypeDock*           m_currencyDock      = nullptr;
    app::AreaInfoDock*               m_areaDock          = nullptr;
    app::ZoneSummaryDock*            m_zoneSummaryDock   = nullptr;
    app::FactionTemplateDock*        m_factionDock       = nullptr;
    app::PlayerConditionDock*        m_playerCondDock    = nullptr;
    app::NpcTextDock*                m_npcTextDock       = nullptr;
    app::AreatriggerScriptDock*      m_atrScriptDock     = nullptr;
    app::LogTailDock*                m_logTailDock       = nullptr;
    // Unified right-side hosts (UI declutter).  Own the QStackedWidget
    // / QTabWidget that hold the inner inspector widgets; the legacy
    // dock-typed members above still point at the same widgets so the
    // rest of MainWindow's call sites are unchanged.
    app::InfoInspectorDock*          m_infoInspector     = nullptr;
    app::PropertyInspectorDock*      m_propertyInspector = nullptr;
    // Tools -> Handcrafted roads...  Standalone QDockWidget hosting a
    // HandcraftedRoadDock CRUD widget.  Constructed in buildHandcraftedRoadDock();
    // hidden until the operator opens it via Tools menu / toolbar.
    app::HandcraftedRoadDock*        m_handcraftedRoadDock = nullptr;
    std::unique_ptr<db::MySqlClient>          m_worldDb;
    std::unique_ptr<db::AnnotationModel>      m_annotationModel;
    std::unique_ptr<db::SpawnModel>           m_spawnModel;
    std::unique_ptr<db::WaypointModel>        m_waypointModel;
    std::unique_ptr<db::AreatriggerModel>     m_areatriggerModel;
    std::unique_ptr<db::GraveyardModel>       m_graveyardModel;
    std::unique_ptr<db::ConditionsModel>      m_conditionsModel;
    std::unique_ptr<io::MapTileCache>         m_mapTileCache;
    // WMO ray-down probe.  Built from the same LoadedVmap that the 3D
    // viewer renders; used to bias snap-to-ground inside buildings so a
    // spawn dropped on a Stormwind upper-floor sticks to the floor, not
    // the terrain below.  Rebuilt with the rest of the map switch.
    io::VmapHeightProbe                       m_vmapProbe;
    // Per-WMO-instance world-XY AABBs captured during the last vmap load.
    // Pushed to the 2D viewer when the operator enables the WMO footprint
    // layer; cleared on map switch.  Always populated alongside m_vmapProbe
    // so the "lazily compute on first toggle" contract is just a flag flip.
    std::vector<io::WmoInstanceAabb>          m_wmoFootprintsCache;
    bool                                      m_wmoFootprintsPushed = false;
    bool                                      m_wmoFootprintsEmptyNoticeShown = false;
    app::PathPropertiesDock*                  m_pathDock = nullptr;
    app::AreatriggerPropertiesDock*           m_areatriggerDock = nullptr;
    app::GraveyardPropertiesDock*             m_graveyardDock = nullptr;
    app::SpawnDiagnosticsDock*                m_diagDock = nullptr;
    app::MinimapDiagnosticsDock*              m_minimapDiagDock = nullptr;
    int                                       m_selectedAreatriggerIndex = -1;
    int                                       m_selectedGraveyardIndex   = -1;
    uint64_t                                  m_nextAreatriggerSpawnId   = 0;
    uint64_t                                  m_nextGraveyardId          = 0;
    int                                    m_selectedPathIndex = -1;
    uint32_t                               m_nextPathId = 0;
    // Path-draw in-progress state.
    bool                                   m_drawingPath = false;
    render::Path                           m_drawingPathBuf;
    // When true, the next click during path-draw OR road-draw runs
    // Detour findPath from the last node/road-point to the click and
    // inserts every intermediate waypoint instead of just a single
    // node/point.  Cleared after one use.
    bool                                   m_autoRouteNextClick = false;
    // Road-network draw mode: each click drops an AnnotationKind::Road
    // marker, optionally auto-routed via Detour between clicks.  Anchor
    // is the last placed Road annotation in THIS session (so the
    // operator can chain segments without reselecting).
    bool                                   m_drawingRoad        = false;
    bool                                   m_hasRoadAnchor      = false;
    float                                  m_roadAnchorX        = 0.0f;
    float                                  m_roadAnchorY        = 0.0f;
    float                                  m_roadAnchorZ        = 0.0f;
    app::AnnotationToolbox*                m_annotationToolbox = nullptr;
    app::SpawnPropertiesEditor*            m_spawnEditor       = nullptr;
    int                                    m_selectedAnnotationIndex = -1;
    int                                    m_selectedSpawnIndex      = -1;
    // Source spawn for the active Clone... dialog, captured at the moment of
    // right-click so a later selection change doesn't repoint the clone op.
    int                                    m_cloneSourceSpawnIndex   = -1;
    // Number of receivers actually mutated by the last Propagate apply
    // (no-op writes are dropped by SpawnModel::replaceRow).  Captured
    // inside the undo lambda then read by the status-bar message.
    int                                    m_propagateLastTouched    = 0;
    QVector<int>                           m_spawnSelection;
    std::optional<uint32_t>                m_currentMapId;

    // GUID reservation (HANDOFF section 10.3).
    uint64_t m_nextCreatureGuid = 0;
    uint64_t m_nextGoGuid       = 0;

    // Placement-mode dispatch: when the viewer reports
    // placementRequested(x, y), we look at this to decide which model
    // gets the new row.
    // Bot dungeon-route working copy (one render::Path per difficulty;
    // pathId carries the difficulty).  Dirty until committed or reloaded.
    std::vector<render::Path>              m_dungeonRoutes;
    bool                                   m_dungeonRoutesDirty = false;
    [[nodiscard]] QString sharedDbSchema() const;
    void reloadDungeonRoutesForMap(uint32_t mapId);
    void pushDungeonRoutesToViewer();

    enum class PlacementKind { None, Annotation, Spawn, PathDraw, Areatrigger, Graveyard, RoadDraw, PathProbe, OffmeshDraw };
    // Pathfinding-probe two-click state: first click = start, second = run.
    bool  m_probeHaveStart = false;
    float m_probeStartX = 0.0f, m_probeStartY = 0.0f, m_probeStartZ = 0.0f;
    void onStartPathProbe();
    void onClearPathProbe();
    // Off-mesh authoring two-click state + offmesh.txt append.
    bool  m_offmeshHaveStart = false;
    float m_offmeshStartX = 0.0f, m_offmeshStartY = 0.0f, m_offmeshStartZ = 0.0f;
    [[nodiscard]] QString offmeshFilePath() const;
    void onStartOffmeshDraw();
    void onSetOffmeshFile();
    bool appendOffmeshConnection(float fx, float fy, float fz,
                                 float tx, float ty, float tz);
    PlacementKind                              m_placementKind = PlacementKind::None;
    // Central mode setter: every entry/exit goes through here so the
    // permanent status-bar badge always tells the operator what a click
    // in the viewport will do and how to leave the mode.
    void setPlacementKind(PlacementKind kind);
    void updateModeBadge();
    // Escape fallback: exits whichever one-shot placement mode is armed
    // (spawn / areatrigger / graveyard / annotation / road draw).  Path
    // drawing keeps its dedicated cancel (same Escape action, checked first).
    void cancelActivePlacement();
    std::unique_ptr<app::PickedTemplate>       m_pickedTemplate;
    // When a 3D click supplied an authored surface Z (depth-unprojected),
    // onPlacementRequested uses it verbatim instead of snapping to ground.
    // Set/cleared around the delegated call by onPlacementRequested3D so the
    // shared placement body (Spawn / Areatrigger / Path / Road / Annotation)
    // stores the real clicked height rather than a recomputed approximation.
    bool                                       m_haveAuthoredZ = false;
    float                                      m_authoredZ     = 0.0f;
    // Resolve the Z for a clicked placement: the authored 3D-pick Z when
    // present, else snapToGround(mapId, x, y) as the 2D top-down fallback.
    [[nodiscard]] float resolvePlacementZ(uint32_t mapId, float worldX, float worldY,
                                          float fallback = 0.0f);
    // Shared spawn-move core for both viewers.  draggedZ carries the 3D
    // drag-plane altitude (nullopt for a 2D top-down drag).
    void applySpawnMove(int spawnIndex, float worldX, float worldY,
                        std::optional<float> draggedZ);
    // Phase 7b: when armed by "New areatrigger...", the picked
    // create-properties row + initial-row template waits here until the
    // operator clicks on the map.
    struct AreatriggerSpawnTemplate
    {
        uint32_t createPropsId = 0;
        uint8_t  isCustom      = 0;
        uint8_t  shape         = 0;
        float    shapeData[8]  = {0,0,0,0,0,0,0,0};
        QString  scriptName;
    };
    std::unique_ptr<AreatriggerSpawnTemplate>  m_pickedAreatriggerProps;

    // Workspace preset wiring.  buildMenus() populates m_layerToggles
    // with one entry per checkable View-menu layer action; presets walk
    // this vector to flip combos in one click.  The two non-layer
    // toggles (path-debug, phase-filter enable) are stored separately so
    // presets can drive them explicitly.
    struct LayerToggle
    {
        QAction*       action     = nullptr;
        render::Layer  layer      = render::Layer::NavMesh;
        bool           defaultOn  = false;
        char const*    settingsKey = nullptr;
    };
    std::vector<LayerToggle> m_layerToggles;
    QAction*                 m_pathDebugAction  = nullptr;
    QAction*                 m_phaseEnableAction = nullptr;
    QAction*                 m_exportPendingAction = nullptr;
    // View -> Bookmarks submenu.  The first two actions (Manage..., Add
    // current view) are fixed; the rest are rebuilt from QSettings on
    // every menu open + every Manage/Add invocation.
    QMenu*                   m_bookmarksMenu       = nullptr;
    // File -> Recent maps submenu.  Populated dynamically on aboutToShow
    // from the QSettings-backed recents list (see onRebuildRecentMapsMenu).
    QMenu*                   m_recentMapsMenu      = nullptr;
    // Push the freshly-opened mapId onto the persisted recents list,
    // capped at 15, deduped by mapId.  Called from loadAndDisplayMap().
    void recordRecentMap(uint32_t mapId);
    // Flight-path graph: latched to true once the operator has been
    // notified that taxi_nodes is missing on the current world DB, so
    // the diagnostic doesn't replay on every map switch.
    bool                     m_flightTablesMissingNoticeShown = false;
};

} // namespace world_editor
