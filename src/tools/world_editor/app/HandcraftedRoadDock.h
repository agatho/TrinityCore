/*
 * HandcraftedRoadDock - CRUD panel for the `handcrafted_road` world-DB
 * table that drives the worldserver's curated road-tagging pass.
 *
 * Hosts a QTableWidget of every segment for the current viewer map plus
 * the action buttons: Add (drives the click-to-place mode on
 * NavMeshView), Edit, Delete, Preview impact (ScanCorridor wrapper +
 * temporary yellow polygon highlight in the 2D viewer), Apply to local
 * navmesh (in-memory NAV_AREA_ROAD retag so the gold auto-extracted
 * overlay surfaces the freshly applied corridor immediately, with no DB
 * write), and Refresh.
 *
 * Inserts/updates/deletes go straight through HandcraftedRoadRepo (whose
 * INSERT / UPDATE / DELETE are individually transactional + rollback-safe).
 * After each mutation the dock reloads the current map's segments and
 * pushes the polyline endpoint pairs to NavMeshView::setHandcraftedRoad
 * Polylines so the coral overlay stays in sync.
 *
 * Lifetime: owned by MainWindow.  The repo is constructed on demand from
 * MainWindow's MySqlClient; the dock holds a borrowed pointer to that
 * client (nullable when the operator hasn't connected the DB yet).
 */

#pragma once

#include "../io/HandcraftedRoadRepo.h"

#include <QWidget>

#include <cstdint>
#include <optional>
#include <vector>

class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QLabel;
class QCheckBox;
class QDoubleSpinBox;

namespace world_editor
{
    namespace db    { class MySqlClient; }
    namespace render { class NavMeshView; }
}

namespace world_editor::app
{

// Local backing row for the QTableWidget.  Defined in the header so
// std::vector<HandcraftedRoadRow> as a member is sized at MOC time.
struct HandcraftedRoadRow
{
    world_editor::io::RoadSegment seg;
};

class HandcraftedRoadDock final : public QWidget
{
    Q_OBJECT

public:
    explicit HandcraftedRoadDock(QWidget* parent = nullptr);

    // Borrowed pointer to MainWindow's world-DB client.  Nullable; the
    // dock surfaces "DB not connected" hints when it's null.
    void setMySqlClient(db::MySqlClient* client);
    // Borrowed pointer to the 2D viewer.  Used by Add / Preview impact /
    // Apply-to-local-navmesh to drive placement mode + impact overlay
    // + handcrafted polyline push.
    void setNavMeshView(render::NavMeshView* viewer);
    // Active viewer map id.  Filters the table view; 0 = no map loaded.
    // Triggers an immediate refreshFromDb() so the table tracks the
    // viewer.
    void setCurrentMapId(uint32_t mapId);
    [[nodiscard]] uint32_t currentMapId() const noexcept { return m_currentMapId; }

    // Hand-off slot: when the operator chose Add Segment + clicked twice
    // on the viewer, MainWindow calls this with the captured world coords
    // and the dock pops the width/comment prompt + executes the INSERT.
    void handleSegmentPlaced(float fromX, float fromY, float toX, float toY);

    // Select every loaded segment whose from/to endpoint sits at (worldX,
    // worldY) (within a small tolerance).  Driven by NavMeshView::road
    // DiagnosticClicked when the operator clicks a gap / dead-end marker, so
    // the offending segment rows highlight in the table and become the target
    // of Edit / Delete.  Pans the table to the first match + shows a toast.
    void selectSegmentsNear(float worldX, float worldY);

signals:
    // Emitted when the operator clicks Add segment...  MainWindow uses
    // this to flip the viewer into placement mode (it already has the
    // viewer pointer; the dock signals intent rather than wiring the
    // viewer transitions directly so the test surface is decoupled).
    void addSegmentRequested();
    // Emitted when the operator cancels an add (e.g. Esc).  MainWindow
    // mirrors back into NavMeshView::cancelSegmentPlacement().
    void cancelAddSegmentRequested();
    // Emitted after refreshFromDb() finishes.  MainWindow uses this to
    // update the parent QDockWidget's title bar so the count stays
    // visible even when the dock is tabbed or minimised.
    void segmentCountChanged(int count);

private slots:
    void onRefreshClicked();
    void onAddClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onPreviewImpactClicked();
    void onApplyToLocalNavmeshClicked();
    void onFilterMapIdChanged(int v);
    void onSearchTextChanged();
    void onSelectionChanged();
    void onBulkEditClicked();
    void onBulkDeleteClicked();
    void onSelectAllVisibleClicked();
    void onInvertSelectionClicked();

private:
    // Reload `m_segments` via repo.loadForMap(filterMapId) and rebuild
    // the table + push the polyline endpoints to the viewer.
    void refreshFromDb();
    // Translate `m_segments` into a QVector2D pair-per-segment buffer +
    // call viewer->setHandcraftedRoadPolylines.
    void pushPolylinesToViewer();
    // Current map under inspection: 0 means the spinbox is at 0 so we
    // mirror m_currentMapId; otherwise we honour the operator-typed
    // filter map id.
    [[nodiscard]] uint32_t filterMapId() const noexcept;
    // Returns the currently selected segment id or 0 if no row is
    // selected.
    [[nodiscard]] uint32_t selectedSegmentId() const;
    // Returns the ids of every currently-selected row in the table
    // (multi-select aware).  Used by the bulk-edit + bulk-delete flows.
    // Hidden (filtered-out) rows are skipped even if Qt still reports
    // them as selected from a prior selection.
    [[nodiscard]] std::vector<uint32_t> selectedSegmentIds() const;

    db::MySqlClient*    m_client  = nullptr;   // borrowed
    render::NavMeshView* m_viewer = nullptr;   // borrowed

    uint32_t                                 m_currentMapId = 0;
    std::vector<HandcraftedRoadRow>          m_rowsBackingStorage;

    // Chain placement state: the first click-pair in a chain pops the
    // width/comment dialog; subsequent pairs reuse the same values so the
    // operator can rope segments together without an interrupting modal
    // between each pair.  m_chainPrompted flips to true after the first
    // accepted prompt and resets when a new chain starts.
    bool   m_chainPrompted   = false;
    float  m_chainWidth      = 8.0f;
    QString m_chainComment;
    bool   m_chainVerified   = false;

    QLineEdit*    m_searchEdit    = nullptr;
    QPushButton*  m_refreshButton = nullptr;
    QSpinBox*     m_filterMapIdSpin = nullptr;
    QTableWidget* m_table         = nullptr;
    QPushButton*  m_addButton     = nullptr;
    QPushButton*  m_editButton    = nullptr;
    QPushButton*  m_deleteButton  = nullptr;
    QPushButton*  m_previewButton = nullptr;
    QPushButton*  m_applyButton   = nullptr;
    QPushButton*  m_bulkEditButton    = nullptr;
    QPushButton*  m_bulkDeleteButton  = nullptr;
    QPushButton*  m_selectAllVisibleButton = nullptr;
    QPushButton*  m_invertSelectionButton  = nullptr;
    QPushButton*  m_helpButton    = nullptr;
    QLabel*       m_statusLabel   = nullptr;
    // ---- Visibility status row (top of the dock) ----
    // m_committedBadge: persistent "OK N segments committed (map M)" header
    //   above the loaded/visible labels.  Always green when N>0, dim grey
    //   when N==0.  Mirrors the dock's loaded-count semantics but pops
    //   visually so the operator gets a constant "did my writes land?" cue.
    // m_loadedLabel: "Loaded from DB: N segments"
    // m_visibleLabel: "Visible on map: M segments" (red when M < N)
    // m_visibilityHint: shown only when M < N, explains how to flip the
    // Roads layer back on.
    // m_toastLabel: transient feedback label (bottom of dock).  Shown
    // after every mutation (INSERT / UPDATE / DELETE / bulk-*) with a
    // 2.5s auto-hide via QTimer::singleShot; 6s for failures (red).
    QLabel*       m_committedBadge = nullptr;
    QLabel*       m_loadedLabel    = nullptr;
    QLabel*       m_visibleLabel   = nullptr;
    QLabel*       m_visibilityHint = nullptr;
    QLabel*       m_toastLabel     = nullptr;
    // Monotonic counter so a late-firing fade-out timer never clears a
    // toast that has since been replaced by a newer one.  We capture the
    // counter when we show, then ignore the clear callback if it doesn't
    // match.  Avoids a race where two rapid INSERTs collapse to one toast.
    quint64       m_toastEpoch     = 0;

    // Build + show the "HOW TO AUTHOR ROADS" walkthrough.  Hooked to the
    // toolbar '?' button.  Pure UI, no DB / viewer side-effects.
    void showWorkflowHelp();
    // Refresh the three top status labels (loaded count, visible count,
    // visibility hint) based on the dock's segment list + m_viewer state.
    // Called from pushPolylinesToViewer + setNavMeshView so the counts
    // stay accurate across every dock-state transition.
    void refreshStatusLabels(size_t loadedSegments);
    // Update the prominent green committed-count badge at the top of the
    // dock.  Always called from refreshStatusLabels so the badge tracks
    // the loaded-from-DB count for the currently-filtered map.
    void refreshCommittedBadge(size_t loadedSegments);
    // Show a brief coloured toast at the bottom of the dock.  kind picks
    // the background colour: "ok" = green, "warn" = orange, "err" = red
    // (red toasts linger for 6s instead of the default 2.5s).
    void showToast(QString const& text, QString const& kind);
    // Briefly highlight a row in the table so the operator sees which
    // segment just got inserted.  Background fades back after 1.5s.
    // Looks up the row by segment id; no-op if the id is no longer in
    // the table (e.g. a filter just hid it).
    void flashRowForSegmentId(uint32_t segmentId);
};

} // namespace world_editor::app
