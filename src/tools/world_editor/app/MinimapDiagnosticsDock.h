/*
 * MinimapDiagnosticsDock - right-pane status panel for the minimap layer.
 *
 * Surfaces the same pieces the operator used to have to grep out of
 * qDebug: CASC client dir, CASC storage open/closed, Map.db2 entry
 * count, minimap PNG dir, current map (mapId + Map.db2 directory name),
 * heightmap tile count, cached minimap texture count, running
 * successful/failed load counters and the most-recent "gx,gy -> result"
 * blurb.
 *
 * The dock is fully passive - MainWindow polls NavMeshView + its own
 * config strings and calls setMinimapInfo() to refresh the labels.  A
 * 2-second auto-refresh QTimer drives the polling while the dock is
 * visible; the Refresh button forces an immediate poll, and the Force
 * reload minimaps button re-pokes NavMeshView::setMinimapDir(...) with
 * its current value to trigger a rebuild (which clears caches +
 * destroyMinimapTextures + re-attempts loads on the next paint).
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class QTimer;

namespace world_editor::render { class NavMeshView; }

namespace world_editor::app
{

class MinimapDiagnosticsDock final : public QWidget
{
    Q_OBJECT

public:
    explicit MinimapDiagnosticsDock(QWidget* parent = nullptr);

    // Borrowed pointer; MainWindow owns the viewer + outlives the dock.
    // Used by the Force reload button to re-pump setMinimapDir.
    void setViewer(render::NavMeshView* viewer) noexcept { m_viewer = viewer; }

    // Push the latest state.  All strings are taken by value; passing an
    // empty QString triggers the "<not configured>" placeholder where
    // appropriate.  mapId == 0 + empty mapDir => "none loaded".
    //
    // `listfileCsv` is the path of the loaded wow-listfile CSV (empty when
    // none); `listfileEntries` is the parsed mapping count.  `pngDirCount`
    // is the file count under the minimap PNG dir (-1 means "not probed";
    // the dock surfaces it only when the dir is configured).
    void setMinimapInfo(QString cascDir, bool cascOpen, int mapDb2Entries,
                        QString minimapDir, uint32_t mapId, QString mapDir,
                        int heightmapTiles, int cachedTextures,
                        int successfulLoads, int failedLoads,
                        QString lastTried,
                        QString listfileCsv, int listfileEntries,
                        int pngDirCount);

    // Push the latest road-overlay diagnostic counts (vertex-pair counts,
    // i.e. polyline-count = vertices / 2).  Separate setter so the existing
    // setMinimapInfo signature stays stable while the dock surfaces the
    // new layer's health alongside the minimap counters.
    void setRoadOverlayInfo(int autoPolylines, int handcraftedPolylines);

    // Start/stop the 2s auto-refresh.  Wired to the dock's visibility
    // (showEvent/hideEvent); also exposed so MainWindow can pause polling
    // explicitly if needed.
    void startAutoRefresh();
    void stopAutoRefresh();

signals:
    // Operator clicked Refresh.  MainWindow re-reads its state and calls
    // setMinimapInfo with the fresh values.
    void refreshRequested();
    // Operator clicked Force reload.  MainWindow forwards to
    // NavMeshView::setMinimapDir(minimapDir()) to drop GL textures + the
    // cache and re-attempt every probed tile.
    void forceReloadRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    render::NavMeshView* m_viewer = nullptr;
    QTimer*              m_pollTimer = nullptr;

    QLabel* m_cascDirLabel       = nullptr;
    QLabel* m_cascStatusLabel    = nullptr;
    QLabel* m_mapDb2Label        = nullptr;
    QLabel* m_minimapDirLabel    = nullptr;
    QLabel* m_currentMapLabel    = nullptr;
    QLabel* m_heightmapLabel     = nullptr;
    QLabel* m_cachedLabel        = nullptr;
    QLabel* m_successfulLabel    = nullptr;
    QLabel* m_failedLabel        = nullptr;
    QLabel* m_lastTriedLabel     = nullptr;
    QLabel* m_listfileLabel      = nullptr;
    QLabel* m_listfileEntriesLabel = nullptr;
    QLabel* m_pngDirCountLabel   = nullptr;
    QLabel* m_autoRoadLabel      = nullptr;
    QLabel* m_handcraftedRoadLabel = nullptr;
    QLabel* m_hintLabel          = nullptr;

    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_reloadButton  = nullptr;
    QPushButton* m_probeButton   = nullptr;

    // Minimap-transform A/B panel.  10 radio buttons map to the
    // NavMeshView::MinimapTransform enum.  Clicking saves the choice to
    // QSettings ("viewer2d/minimap_transform") and calls
    // NavMeshView::setMinimapTransform, which flushes the texture cache
    // AND forces the heightmap chunked-build queue to re-seed so tiles
    // visibly re-stream.  Replaces the previous F1..F7 keyboard
    // shortcuts (which conflicted with global Find/3D-view bindings and
    // never re-seeded the queue).
    QButtonGroup* m_transformGroup   = nullptr;
    QLabel*       m_transformActive  = nullptr;
    QPushButton*  m_inspectButton    = nullptr;
};

} // namespace world_editor::app
