#include "HandcraftedRoadDock.h"

#include "../db/MySqlClient.h"
#include "../io/HandcraftedRoadRepo.h"
#include "../render/NavMeshView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace world_editor::app
{

namespace
{
constexpr int kColId       = 0;
constexpr int kColFromX    = 1;
constexpr int kColFromY    = 2;
constexpr int kColToX      = 3;
constexpr int kColToY      = 4;
constexpr int kColWidth    = 5;
constexpr int kColComment  = 6;
constexpr int kColVerified = 7;
constexpr int kColCount    = 8;
} // namespace

HandcraftedRoadDock::HandcraftedRoadDock(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // ---- Top row: search + map-id filter + refresh ----
    auto* top = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter comments..."));
    m_searchEdit->setClearButtonEnabled(true);
    top->addWidget(m_searchEdit, 2);

    top->addWidget(new QLabel(tr("Filter by mapId:"), this));
    m_filterMapIdSpin = new QSpinBox(this);
    m_filterMapIdSpin->setRange(0, 9999);
    m_filterMapIdSpin->setValue(0);
    m_filterMapIdSpin->setToolTip(tr(
        "0 = use the currently loaded viewer map.  Any non-zero value "
        "overrides and filters the table to that mapId."));
    top->addWidget(m_filterMapIdSpin);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    top->addWidget(m_refreshButton);

    // Toolbar '?' workflow-help button.  Pops a QMessageBox explaining
    // the whole author -> preview -> apply -> live-worldserver flow so
    // the operator doesn't have to guess what each action button does.
    m_helpButton = new QPushButton(QStringLiteral("?"), this);
    m_helpButton->setFixedWidth(28);
    m_helpButton->setToolTip(tr(
        "Show the handcrafted-road authoring walkthrough: how to draw "
        "chains, what each test mode does (Preview impact / Apply to "
        "local navmesh / live worldserver), and the practical iteration "
        "loop."));
    top->addWidget(m_helpButton);
    root->addLayout(top);

    // ---- Committed-count badge (prominent header, persistent) ----
    //
    // Above every other status label so the operator gets a constant,
    // high-signal "did my writes land?" cue.  Green when N>0, dim grey
    // when N==0.  Updated after every refreshFromDb() via the helper
    // refreshCommittedBadge() so each click+edit+delete reflects here.
    m_committedBadge = new QLabel(tr("No segments yet for map 0"), this);
    {
        QFont f = m_committedBadge->font();
        f.setBold(true);
        f.setPointSize(14);
        m_committedBadge->setFont(f);
    }
    m_committedBadge->setStyleSheet(QStringLiteral("color: #888;"));
    root->addWidget(m_committedBadge);

    // ---- Visibility status row (just below the toolbar) ----
    //
    // Three small labels make it obvious why a freshly-inserted segment
    // would NOT show up on the map even though the dock claims it was
    // committed: either the dock's segment list is empty (DB issue) or
    // the viewer pointer is unset (wiring issue) or the Roads layer
    // toggle is off (operator visibility).  Each case lights up the
    // appropriate label so the operator can self-diagnose in seconds.
    auto* statusRow = new QHBoxLayout();
    m_loadedLabel = new QLabel(tr("Loaded from DB: 0 segments"), this);
    m_loadedLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusRow->addWidget(m_loadedLabel);
    statusRow->addSpacing(12);
    m_visibleLabel = new QLabel(tr("Visible on map: 0 segments"), this);
    m_visibleLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusRow->addWidget(m_visibleLabel);
    statusRow->addStretch(1);
    root->addLayout(statusRow);

    m_visibilityHint = new QLabel(tr(
        "Toggle visibility: View menu -> Show road network"), this);
    m_visibilityHint->setStyleSheet(QStringLiteral("color: #c66; font-style: italic;"));
    m_visibilityHint->setVisible(false);
    root->addWidget(m_visibilityHint);

    // ---- Table ----
    m_table = new QTableWidget(0, kColCount, this);
    QStringList headers;
    headers << tr("id") << tr("fromX") << tr("fromY")
            << tr("toX") << tr("toY") << tr("width")
            << tr("comment") << tr("verified");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // ---- Action buttons ----
    auto* btnRow = new QHBoxLayout();
    m_addButton     = new QPushButton(tr("Add chain..."), this);
    m_editButton    = new QPushButton(tr("Edit segment"), this);
    m_deleteButton  = new QPushButton(tr("Delete segment"), this);
    m_previewButton = new QPushButton(tr("Preview impact"), this);
    m_applyButton   = new QPushButton(tr("Apply to local navmesh (dry preview)"), this);
    m_addButton->setToolTip(tr(
        "Switches the 2D viewer into 'place chain' mode.  Each click drops "
        "an endpoint; consecutive clicks rope segments together (A->B, B->C, "
        "C->D, ...).  Esc or right-click finishes the chain.  Each segment "
        "INSERTs through HandcraftedRoadRepo as soon as it's dropped; clicks "
        "within 6 yards of an existing road endpoint snap to that exact "
        "endpoint so the operator can extend / close roads cleanly."));
    m_editButton->setToolTip(tr(
        "Edit width / comment / verified flag for the selected row.  Saves "
        "via HandcraftedRoadRepo::update (transactional + rollback-safe)."));
    m_deleteButton->setToolTip(tr(
        "Delete the selected row.  Confirms then runs HandcraftedRoadRepo::remove."));
    m_previewButton->setToolTip(tr(
        "Run RoadCorridor::ScanCorridor for the selected row and show the "
        "polygon count + tile/poly diagnostics.  Affected polygons are "
        "highlighted in translucent yellow on the viewer for 10 seconds."));
    m_applyButton->setToolTip(tr(
        "Flip every polygon the selected segment intersects to "
        "NAV_AREA_ROAD on the IN-MEMORY navmesh.  Auto-rebuilds the gold "
        "road overlay so the operator sees the result immediately.  Does "
        "NOT persist anything; the worldserver re-applies handcrafted-road "
        "tagging from the SQL table at map load."));
    btnRow->addWidget(m_addButton);
    btnRow->addWidget(m_editButton);
    btnRow->addWidget(m_deleteButton);
    btnRow->addWidget(m_previewButton);
    btnRow->addWidget(m_applyButton);

    // Bulk operations + selection helpers.  These light up only when 2+
    // rows are selected (or there are visible rows for the helpers).
    m_bulkEditButton         = new QPushButton(tr("Bulk edit..."), this);
    m_bulkDeleteButton       = new QPushButton(tr("Bulk delete"), this);
    m_selectAllVisibleButton = new QPushButton(tr("Select all visible"), this);
    m_invertSelectionButton  = new QPushButton(tr("Invert selection"), this);
    m_bulkEditButton->setToolTip(tr(
        "Apply common values (comment / width / verified) to every selected "
        "row in one transactional batch.  Each field has a checkbox in the "
        "dialog -- only the checked ones get applied; unchecked = leave as-is."));
    m_bulkDeleteButton->setToolTip(tr(
        "DELETE every selected row in a single transaction.  Asks for "
        "confirmation first."));
    m_selectAllVisibleButton->setToolTip(tr(
        "Select every currently-visible row (the comment-substring filter "
        "above hides non-matching rows; this command honours that filter)."));
    m_invertSelectionButton->setToolTip(tr(
        "Toggle the selection state of every visible row."));
    btnRow->addWidget(m_bulkEditButton);
    btnRow->addWidget(m_bulkDeleteButton);
    btnRow->addWidget(m_selectAllVisibleButton);
    btnRow->addWidget(m_invertSelectionButton);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("(no map loaded)"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888;"));
    root->addWidget(m_statusLabel);

    // Transient mutation feedback toast.  Lives at the very bottom of
    // the dock, hidden by default; showToast() pops it after every
    // INSERT/UPDATE/DELETE/bulk-* and an internal QTimer::singleShot
    // hides it again (2.5s for success, 6s for failures).
    m_toastLabel = new QLabel(this);
    m_toastLabel->setVisible(false);
    m_toastLabel->setWordWrap(true);
    m_toastLabel->setMargin(6);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_toastLabel);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onRefreshClicked);
    connect(m_addButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onAddClicked);
    connect(m_editButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onEditClicked);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onDeleteClicked);
    connect(m_previewButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onPreviewImpactClicked);
    connect(m_applyButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onApplyToLocalNavmeshClicked);
    connect(m_bulkEditButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onBulkEditClicked);
    connect(m_bulkDeleteButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onBulkDeleteClicked);
    connect(m_selectAllVisibleButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onSelectAllVisibleClicked);
    connect(m_invertSelectionButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::onInvertSelectionClicked);
    connect(m_filterMapIdSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &HandcraftedRoadDock::onFilterMapIdChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &HandcraftedRoadDock::onSearchTextChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &HandcraftedRoadDock::onSelectionChanged);
    connect(m_helpButton, &QPushButton::clicked,
            this, &HandcraftedRoadDock::showWorkflowHelp);

    onSelectionChanged();
}

void HandcraftedRoadDock::setMySqlClient(db::MySqlClient* client)
{
    m_client = client;
    refreshFromDb();
}

void HandcraftedRoadDock::setNavMeshView(render::NavMeshView* viewer)
{
    m_viewer = viewer;
    // Re-push polylines so the freshly attached viewer surfaces them
    // even if the dock was loaded before the viewer was constructed.
    pushPolylinesToViewer();
}

void HandcraftedRoadDock::setCurrentMapId(uint32_t mapId)
{
    if (m_currentMapId == mapId)
        return;
    m_currentMapId = mapId;
    refreshFromDb();
}

uint32_t HandcraftedRoadDock::filterMapId() const noexcept
{
    int const explicitId = m_filterMapIdSpin ? m_filterMapIdSpin->value() : 0;
    if (explicitId > 0)
        return uint32_t(explicitId);
    return m_currentMapId;
}

void HandcraftedRoadDock::onRefreshClicked()
{
    refreshFromDb();
}

void HandcraftedRoadDock::onFilterMapIdChanged(int /*v*/)
{
    refreshFromDb();
}

void HandcraftedRoadDock::onSearchTextChanged()
{
    // Hide rows whose comment does not contain the typed text (case-
    // insensitive).  Simple substring match; the table is bounded in size
    // (operator-curated, per-map) so a linear scan is fine.
    QString const needle = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    if (!m_table)
        return;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        bool visible = true;
        if (!needle.isEmpty())
        {
            QTableWidgetItem* commentItem = m_table->item(row, kColComment);
            QString const haystack = commentItem ? commentItem->text() : QString();
            visible = haystack.contains(needle, Qt::CaseInsensitive);
        }
        m_table->setRowHidden(row, !visible);
    }
}

void HandcraftedRoadDock::onSelectionChanged()
{
    std::vector<uint32_t> const sel = selectedSegmentIds();
    bool const hasSel    = !sel.empty();
    bool const hasBulk   = sel.size() >= 2;
    if (m_editButton)    m_editButton->setEnabled(hasSel);
    if (m_deleteButton)  m_deleteButton->setEnabled(hasSel);
    if (m_previewButton) m_previewButton->setEnabled(hasSel && m_viewer != nullptr);
    if (m_applyButton)   m_applyButton->setEnabled(hasSel && m_viewer != nullptr);
    if (m_bulkEditButton)   m_bulkEditButton->setEnabled(hasBulk);
    if (m_bulkDeleteButton) m_bulkDeleteButton->setEnabled(hasBulk);
}

uint32_t HandcraftedRoadDock::selectedSegmentId() const
{
    std::vector<uint32_t> const ids = selectedSegmentIds();
    return ids.empty() ? 0u : ids.front();
}

std::vector<uint32_t> HandcraftedRoadDock::selectedSegmentIds() const
{
    std::vector<uint32_t> out;
    if (!m_table) return out;
    // selectionModel()->selectedRows() returns one QModelIndex per
    // selected row (with column 0), even in ExtendedSelection mode.
    // We additionally skip hidden rows so the bulk operations operate
    // strictly on what the operator can see post-filter.
    QModelIndexList const rows = m_table->selectionModel() != nullptr
        ? m_table->selectionModel()->selectedRows()
        : QModelIndexList{};
    out.reserve(size_t(rows.size()));
    for (QModelIndex const& mi : rows)
    {
        int const row = mi.row();
        if (m_table->isRowHidden(row))
            continue;
        QTableWidgetItem* idItem = m_table->item(row, kColId);
        if (!idItem) continue;
        bool ok = false;
        uint32_t const id = idItem->text().toUInt(&ok);
        if (ok && id != 0)
            out.push_back(id);
    }
    return out;
}

void HandcraftedRoadDock::selectSegmentsNear(float worldX, float worldY)
{
    if (!m_table)
        return;

    // A touch over the server's 3y graph merge epsilon so every segment whose
    // endpoint clustered into the clicked node is caught.
    constexpr float kTol2 = 4.0f * 4.0f;

    std::vector<int>      matchRows;
    std::vector<uint32_t> matchIds;
    for (size_t i = 0; i < m_rowsBackingStorage.size(); ++i)
    {
        io::RoadSegment const& s = m_rowsBackingStorage[i].seg;
        float const dfx = s.fromX - worldX, dfy = s.fromY - worldY;
        float const dtx = s.toX   - worldX, dty = s.toY   - worldY;
        if ((dfx * dfx + dfy * dfy) > kTol2 && (dtx * dtx + dty * dty) > kTol2)
            continue;
        int const row = int(i);
        if (m_table->isRowHidden(row))
            continue;
        matchRows.push_back(row);
        matchIds.push_back(s.id);
    }

    if (matchRows.empty())
    {
        showToast(tr("No segment at that node (click nearer the ring centre)."), "warn");
        return;
    }

    m_table->clearSelection();
    if (QItemSelectionModel* sel = m_table->selectionModel())
    {
        QAbstractItemModel* model = m_table->model();
        int const lastCol = m_table->columnCount() - 1;
        for (int row : matchRows)
            sel->select(QItemSelection(model->index(row, 0), model->index(row, lastCol)),
                        QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    if (QTableWidgetItem* first = m_table->item(matchRows.front(), kColId))
        m_table->scrollToItem(first, QAbstractItemView::PositionAtCenter);
    flashRowForSegmentId(matchIds.front());
    onSelectionChanged(); // refresh Edit / Delete button enablement

    QStringList idStrs;
    idStrs.reserve(int(matchIds.size()));
    for (uint32_t id : matchIds)
        idStrs << QString::number(id);
    showToast(tr("Selected segment(s) %1 at this node — Delete to remove, or redraw to reconnect.")
                  .arg(idStrs.join(", ")),
              "ok");
}

void HandcraftedRoadDock::refreshFromDb()
{
    m_rowsBackingStorage.clear();
    if (!m_table)
        return;
    m_table->setRowCount(0);

    if (!m_client || !m_client->isConnected())
    {
        if (m_statusLabel)
            m_statusLabel->setText(tr("DB not connected -- connect via Database -> Connect..."));
        qDebug() << "[handcrafted-road] dock load: skipped (DB not connected) mapId="
                 << filterMapId();
        pushPolylinesToViewer();
        onSelectionChanged();
        return;
    }
    uint32_t const mapId = filterMapId();
    io::HandcraftedRoadRepo repo(m_client);
    std::vector<io::RoadSegment> const segs = repo.loadForMap(mapId);
    qDebug() << "[handcrafted-road] dock load:" << segs.size()
             << "segments from DB for mapId=" << mapId;

    m_rowsBackingStorage.reserve(segs.size());
    m_table->setRowCount(int(segs.size()));
    for (size_t i = 0; i < segs.size(); ++i)
    {
        io::RoadSegment const& s = segs[i];
        m_rowsBackingStorage.push_back(HandcraftedRoadRow{ s });

        auto addCell = [&](int col, QString const& text, bool numeric) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if (numeric)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(int(i), col, item);
        };
        addCell(kColId,       QString::number(s.id), true);
        addCell(kColFromX,    QString::number(s.fromX, 'f', 2), true);
        addCell(kColFromY,    QString::number(s.fromY, 'f', 2), true);
        addCell(kColToX,      QString::number(s.toX,   'f', 2), true);
        addCell(kColToY,      QString::number(s.toY,   'f', 2), true);
        addCell(kColWidth,    QString::number(s.width, 'f', 2), true);
        addCell(kColComment,  s.comment, false);
        addCell(kColVerified, s.verified ? tr("yes") : tr("no"), false);
    }

    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);

    if (m_statusLabel)
        m_statusLabel->setText(tr("%1 segment(s) on mapId=%2")
                               .arg(segs.size())
                               .arg(mapId));

    onSearchTextChanged();
    pushPolylinesToViewer();
    onSelectionChanged();

    // Notify listeners (MainWindow updates the parent QDockWidget title bar
    // so the count is visible even when the dock is tabbed or minimised).
    emit segmentCountChanged(int(m_rowsBackingStorage.size()));
}

void HandcraftedRoadDock::pushPolylinesToViewer()
{
    size_t const loaded = m_rowsBackingStorage.size();
    // Always refresh the visibility-status labels even when the viewer is
    // unset -- the labels exist to surface exactly that failure mode.
    refreshStatusLabels(loaded);

    if (!m_viewer)
    {
        qDebug() << "[handcrafted-road] dock push: viewer pointer unset --"
                 << loaded << "segment(s) NOT visible (mapId=" << m_currentMapId << ")";
        return;
    }
    std::vector<QVector2D> verts;
    verts.reserve(loaded * 2);
    for (HandcraftedRoadRow const& r : m_rowsBackingStorage)
    {
        verts.emplace_back(r.seg.fromX, r.seg.fromY);
        verts.emplace_back(r.seg.toX,   r.seg.toY);
    }
    qDebug() << "[handcrafted-road] dock push:" << loaded << "segments ("
             << verts.size() << "vertices) sent to viewer (mapId="
             << m_currentMapId << ")";
    m_viewer->setHandcraftedRoadPolylines(verts);
}

void HandcraftedRoadDock::refreshStatusLabels(size_t loadedSegments)
{
    refreshCommittedBadge(loadedSegments);
    if (m_loadedLabel)
        m_loadedLabel->setText(tr("Loaded from DB: %1 segment(s)").arg(loadedSegments));

    // "Visible on map" is loadedSegments IFF the viewer pointer is set AND
    // the Roads layer is currently on.  Either failing zeros the count.
    bool const viewerOk     = (m_viewer != nullptr);
    bool const roadsLayerOn = viewerOk
        ? m_viewer->isLayerVisible(render::Layer::Roads)
        : false;
    size_t const visible = (viewerOk && roadsLayerOn) ? loadedSegments : 0;

    if (m_visibleLabel)
    {
        m_visibleLabel->setText(tr("Visible on map: %1 segment(s)").arg(visible));
        // Red text when fewer are visible than loaded -- this is the
        // "operator drew a road but doesn't see it" failure mode.
        bool const mismatch = (loadedSegments > 0) && (visible < loadedSegments);
        m_visibleLabel->setStyleSheet(mismatch
            ? QStringLiteral("color: #c44; font-weight: bold;")
            : QStringLiteral("color: #888;"));
    }
    if (m_visibilityHint)
    {
        bool const mismatch = (loadedSegments > 0) && (visible < loadedSegments);
        // Compose a specific diagnostic so the operator knows EXACTLY why
        // the count dropped: viewer unset is a wiring bug, layer off is a
        // user-visible toggle.
        QString reason;
        if (!viewerOk)
            reason = tr("(viewer pointer is unset)");
        else if (!roadsLayerOn)
            reason = tr("(Roads layer toggle is off)");
        m_visibilityHint->setText(tr(
            "Some segments are not visible %1. "
            "Toggle visibility: View menu -> Show road network.").arg(reason));
        m_visibilityHint->setVisible(mismatch);
    }
}

void HandcraftedRoadDock::refreshCommittedBadge(size_t loadedSegments)
{
    if (!m_committedBadge)
        return;
    uint32_t const mapId = filterMapId();
    if (loadedSegments == 0)
    {
        m_committedBadge->setText(tr("No segments yet for map %1").arg(mapId));
        m_committedBadge->setStyleSheet(QStringLiteral("color: #888;"));
    }
    else
    {
        m_committedBadge->setText(tr("\xE2\x9C\x93 %1 segment(s) committed (map %2)")
                                  .arg(loadedSegments).arg(mapId));
        m_committedBadge->setStyleSheet(QStringLiteral("color: #3da85a;"));
    }
}

void HandcraftedRoadDock::showToast(QString const& text, QString const& kind)
{
    if (!m_toastLabel)
        return;
    // Pick palette by kind: green ok, orange warn, red err.  Red toasts
    // linger 6s; everything else 2.5s.  We use bold white text on a
    // saturated background so the toast pops clearly above the regular
    // dim status labels.
    QString bg;
    int holdMs = 2500;
    if (kind == QStringLiteral("ok"))
        bg = QStringLiteral("#3da85a");
    else if (kind == QStringLiteral("warn"))
        bg = QStringLiteral("#d98a3d");
    else
    {
        bg = QStringLiteral("#c0392b");
        holdMs = 6000;
    }
    m_toastLabel->setText(text);
    m_toastLabel->setStyleSheet(QString(
        QStringLiteral("background-color: %1; color: white; "
                       "font-weight: bold; border-radius: 4px;")).arg(bg));
    m_toastLabel->setVisible(true);

    quint64 const epoch = ++m_toastEpoch;
    QTimer::singleShot(holdMs, this, [this, epoch]() {
        if (!m_toastLabel)
            return;
        // A newer toast may have superseded us; only clear if our epoch
        // is still the latest one.
        if (epoch != m_toastEpoch)
            return;
        m_toastLabel->setVisible(false);
        m_toastLabel->clear();
        m_toastLabel->setStyleSheet(QString());
    });
}

void HandcraftedRoadDock::flashRowForSegmentId(uint32_t segmentId)
{
    if (!m_table || segmentId == 0)
        return;
    int targetRow = -1;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        QTableWidgetItem* idItem = m_table->item(row, kColId);
        if (!idItem) continue;
        bool ok = false;
        uint32_t const id = idItem->text().toUInt(&ok);
        if (ok && id == segmentId)
        {
            targetRow = row;
            break;
        }
    }
    if (targetRow < 0)
        return;
    // Apply soft-green background to every cell on the row.  We remember
    // each cell's prior brush so the fade-out restores the original look
    // (alternating-row-colors stripe etc.) instead of forcing default.
    QColor const flash(0x9d, 0xe8, 0xb0);
    QVector<QBrush> previousBrushes;
    previousBrushes.reserve(m_table->columnCount());
    for (int col = 0; col < m_table->columnCount(); ++col)
    {
        QTableWidgetItem* item = m_table->item(targetRow, col);
        if (!item)
        {
            previousBrushes.push_back(QBrush());
            continue;
        }
        previousBrushes.push_back(item->background());
        item->setBackground(flash);
    }
    QPointer<HandcraftedRoadDock> guard(this);
    QTimer::singleShot(1500, this, [guard, targetRow, previousBrushes]() {
        if (!guard || !guard->m_table)
            return;
        if (targetRow >= guard->m_table->rowCount())
            return;
        for (int col = 0; col < guard->m_table->columnCount() && col < previousBrushes.size(); ++col)
        {
            QTableWidgetItem* item = guard->m_table->item(targetRow, col);
            if (!item) continue;
            item->setBackground(previousBrushes[col]);
        }
    });
}

void HandcraftedRoadDock::showWorkflowHelp()
{
    // Verbatim walkthrough text.  Surfaced via the toolbar '?' button so
    // a fresh operator can self-serve without grepping the dock source.
    // The same text is mirrored in sql/playerbot/14_handcrafted_road.sql
    // as a developer-facing comment block.
    QMessageBox box(this);
    box.setWindowTitle(tr("How to author handcrafted roads"));
    box.setIcon(QMessageBox::Information);
    box.setTextFormat(Qt::PlainText);
    box.setText(QStringLiteral(
        "HOW TO AUTHOR ROADS\n\n"
        "1. Click \"Add chain...\" to start drawing.\n"
        "2. Click two points on the map for the first segment. A dialog asks for\n"
        "   width (yards) and a comment. These values are reused for the entire\n"
        "   chain.\n"
        "3. Each subsequent click extends the chain with another segment, sharing\n"
        "   the previous endpoint.\n"
        "4. Cyan squares show existing road endpoints nearby. Within 6 yards, the\n"
        "   endpoint glows yellow -- your next click snaps to it. Use this to\n"
        "   continue an existing road or close a loop.\n"
        "5. White diamonds mark crossroads (3+ segments meeting).\n"
        "6. Press Esc or right-click to finish the chain.\n\n"
        "INSPECT AND EDIT\n\n"
        "- The table shows every segment in the current map's DB row.\n"
        "- Edit segment changes width/comment/verified flag.\n"
        "- Delete segment removes the row from DB.\n"
        "- Preview impact shows which navmesh polygons would get retagged\n"
        "  (highlight in translucent yellow). Read-only.\n\n"
        "TEST CHANGES\n\n"
        "There are three test paths in increasing levels of commitment:\n\n"
        "A) PREVIEW IMPACT (no commit, no navmesh change):\n"
        "   Click \"Preview impact\" for the selected segment. The viewer highlights\n"
        "   the affected navmesh polygons in yellow. Useful for sanity-checking\n"
        "   that the corridor covers the road you want.\n\n"
        "B) APPLY TO LOCAL NAVMESH (in-memory only, no persistence):\n"
        "   Click \"Apply to local navmesh (dry preview)\" to flip the selected\n"
        "   segment's polygons to NAV_AREA_ROAD in the world_editor's in-memory\n"
        "   navmesh. The GOLD auto-road overlay refreshes immediately to include\n"
        "   the new corridor. This is the best visual confirmation, but it does\n"
        "   NOT change the worldserver -- on next map reload in the editor, the\n"
        "   change vanishes (the DB row stays).\n\n"
        "C) APPLY TO LIVE WORLDSERVER (real change, persisted):\n"
        "   - Restart the worldserver OR\n"
        "   - As a GM in-game: \".reload handcrafted_road apply <mapId>\"\n"
        "   The worldserver applies all segments from the DB to the live\n"
        "   navmesh. Bots will now path along these corridors. To revert, delete\n"
        "   the DB row + restart the worldserver (Detour can't un-tag a poly\n"
        "   without a full navmesh reload).\n\n"
        "PRACTICAL FLOW\n\n"
        "1. Draw 5-10 chain segments for a road.\n"
        "2. Click \"Apply to local navmesh\" to confirm visually.\n"
        "3. If happy: \".reload handcrafted_road apply <mapId>\" in-game.\n"
        "4. If not happy: edit/delete in the dock, repeat from step 2.\n"));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void HandcraftedRoadDock::onAddClicked()
{
    if (!m_client || !m_client->isConnected())
    {
        QMessageBox::warning(this, tr("Add handcrafted road"),
            tr("Connect to the world DB first (Database -> Connect...)."));
        return;
    }
    if (!m_viewer)
    {
        QMessageBox::warning(this, tr("Add handcrafted road"),
            tr("2D viewer is not available; cannot enter placement mode."));
        return;
    }
    // Fresh chain: clear cached width/comment so the first segment of
    // the chain re-prompts.  Subsequent segments inside the same chain
    // skip the prompt and reuse the cached values.
    m_chainPrompted = false;
    m_chainWidth    = 8.0f;
    m_chainComment.clear();
    m_chainVerified = false;
    emit addSegmentRequested();
    if (m_statusLabel)
        m_statusLabel->setText(tr("Click first point for road chain "
                                   "(Esc or right-click to finish)."));
}

void HandcraftedRoadDock::handleSegmentPlaced(float fromX, float fromY, float toX, float toY)
{
    if (!m_client || !m_client->isConnected())
        return;

    // First segment of a chain: prompt for width / comment / verified
    // flag and cache the values so subsequent segments in the same chain
    // skip the modal (otherwise chain mode is no faster than the old
    // re-arm-per-click flow).  A new chain restarts the prompt via
    // onAddClicked which resets m_chainPrompted.
    if (!m_chainPrompted)
    {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("New handcrafted road chain"));
        auto* form = new QFormLayout(&dlg);

        auto* widthSpin = new QDoubleSpinBox(&dlg);
        widthSpin->setRange(0.5, 200.0);
        widthSpin->setSingleStep(0.5);
        widthSpin->setDecimals(2);
        widthSpin->setValue(8.0);
        widthSpin->setSuffix(tr(" yards"));
        form->addRow(tr("Width:"), widthSpin);

        auto* commentEdit = new QLineEdit(&dlg);
        commentEdit->setPlaceholderText(tr("Optional comment (shared by every segment in this chain)"));
        form->addRow(tr("Comment:"), commentEdit);

        auto* verifiedCheck = new QCheckBox(tr("Mark verified"), &dlg);
        form->addRow(QString{}, verifiedCheck);

        auto* coordsLabel = new QLabel(
            tr("First segment: (%1, %2) -> (%3, %4)  mapId=%5")
                .arg(fromX, 0, 'f', 2)
                .arg(fromY, 0, 'f', 2)
                .arg(toX,   0, 'f', 2)
                .arg(toY,   0, 'f', 2)
                .arg(filterMapId()), &dlg);
        coordsLabel->setStyleSheet(QStringLiteral("color: #888;"));
        form->addRow(coordsLabel);

        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        form->addRow(bb);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted)
        {
            if (m_statusLabel)
                m_statusLabel->setText(tr("Add chain cancelled."));
            // Stop the chain so the operator doesn't get a stuck FSM.
            if (m_viewer)
                m_viewer->cancelSegmentPlacement();
            return;
        }
        m_chainWidth    = float(widthSpin->value());
        m_chainComment  = commentEdit->text();
        m_chainVerified = verifiedCheck->isChecked();
        m_chainPrompted = true;
    }

    io::RoadSegment seg;
    seg.mapId    = filterMapId();
    seg.fromX    = fromX;
    seg.fromY    = fromY;
    seg.toX      = toX;
    seg.toY      = toY;
    seg.width    = m_chainWidth;
    seg.comment  = m_chainComment;
    seg.verified = m_chainVerified;

    io::HandcraftedRoadRepo repo(m_client);
    auto const newId = repo.insert(seg);
    if (!newId.has_value())
    {
        qDebug().noquote() << QStringLiteral(
            "[handcrafted-road] commit \xE2\x9C\x97: action=insert, ids=[], rowsAffected=0 (FAILED)");
        showToast(tr("\xE2\x9C\x97 Save failed: INSERT returned no id"),
                  QStringLiteral("err"));
        QMessageBox::critical(this, tr("Add handcrafted road"),
            tr("INSERT failed.  Check the world DB log for details."));
        return;
    }
    int const chainCount = m_viewer ? m_viewer->chainSegmentCount() : 0;
    if (m_statusLabel)
        m_statusLabel->setText(tr("Chain mode: %1 segment(s) placed.  "
                                   "Click to extend, Esc/right-click to finish.  "
                                   "Last id=%2.")
                               .arg(chainCount).arg(*newId));
    refreshFromDb();
    size_t const total = m_rowsBackingStorage.size();
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] commit \xE2\x9C\x93: action=insert, ids=[%1], rowsAffected=1")
            .arg(*newId);
    showToast(tr("\xE2\x9C\x93 Saved (id %1, total %2)").arg(*newId).arg(total),
              QStringLiteral("ok"));
    flashRowForSegmentId(*newId);
}

void HandcraftedRoadDock::onEditClicked()
{
    uint32_t const id = selectedSegmentId();
    if (id == 0)
        return;
    // Find the backing-store row whose id matches.
    auto it = std::find_if(m_rowsBackingStorage.begin(), m_rowsBackingStorage.end(),
        [id](HandcraftedRoadRow const& r) { return r.seg.id == id; });
    if (it == m_rowsBackingStorage.end())
        return;
    io::RoadSegment seg = it->seg;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit handcrafted road segment #%1").arg(id));
    auto* form = new QFormLayout(&dlg);

    auto* widthSpin = new QDoubleSpinBox(&dlg);
    widthSpin->setRange(0.5, 200.0);
    widthSpin->setSingleStep(0.5);
    widthSpin->setDecimals(2);
    widthSpin->setValue(seg.width);
    widthSpin->setSuffix(tr(" yards"));
    form->addRow(tr("Width:"), widthSpin);

    auto* commentEdit = new QLineEdit(&dlg);
    commentEdit->setText(seg.comment);
    form->addRow(tr("Comment:"), commentEdit);

    auto* verifiedCheck = new QCheckBox(tr("Mark verified"), &dlg);
    verifiedCheck->setChecked(seg.verified);
    form->addRow(QString{}, verifiedCheck);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    seg.width    = float(widthSpin->value());
    seg.comment  = commentEdit->text();
    seg.verified = verifiedCheck->isChecked();

    io::HandcraftedRoadRepo repo(m_client);
    if (!repo.update(seg))
    {
        qDebug().noquote() << QStringLiteral(
            "[handcrafted-road] commit \xE2\x9C\x97: action=update, ids=[%1], rowsAffected=0 (FAILED)")
                .arg(id);
        showToast(tr("\xE2\x9C\x97 Save failed: UPDATE id %1 rolled back").arg(id),
                  QStringLiteral("err"));
        QMessageBox::critical(this, tr("Edit handcrafted road"),
            tr("UPDATE failed.  Check the world DB log for details."));
        return;
    }
    if (m_statusLabel)
        m_statusLabel->setText(tr("Updated segment id=%1.").arg(id));
    refreshFromDb();
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] commit \xE2\x9C\x93: action=update, ids=[%1], rowsAffected=1").arg(id);
    showToast(tr("\xE2\x9C\x93 Updated id %1").arg(id), QStringLiteral("ok"));
}

void HandcraftedRoadDock::onDeleteClicked()
{
    uint32_t const id = selectedSegmentId();
    if (id == 0)
        return;
    auto const reply = QMessageBox::question(this,
        tr("Delete handcrafted road"),
        tr("Delete handcrafted_road row id=%1?  This cannot be undone "
           "from the editor; you would have to re-INSERT manually.").arg(id),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Yes)
        return;
    io::HandcraftedRoadRepo repo(m_client);
    if (!repo.remove(id))
    {
        qDebug().noquote() << QStringLiteral(
            "[handcrafted-road] commit \xE2\x9C\x97: action=delete, ids=[%1], rowsAffected=0 (FAILED)")
                .arg(id);
        showToast(tr("\xE2\x9C\x97 Save failed: DELETE id %1 rolled back").arg(id),
                  QStringLiteral("err"));
        QMessageBox::critical(this, tr("Delete handcrafted road"),
            tr("DELETE failed.  Check the world DB log for details."));
        return;
    }
    if (m_statusLabel)
        m_statusLabel->setText(tr("Deleted segment id=%1.").arg(id));
    refreshFromDb();
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] commit \xE2\x9C\x93: action=delete, ids=[%1], rowsAffected=1").arg(id);
    showToast(tr("\xE2\x9C\x93 Deleted id %1").arg(id), QStringLiteral("warn"));
}

void HandcraftedRoadDock::onPreviewImpactClicked()
{
    if (!m_viewer)
        return;
    uint32_t const id = selectedSegmentId();
    if (id == 0)
        return;
    auto it = std::find_if(m_rowsBackingStorage.begin(), m_rowsBackingStorage.end(),
        [id](HandcraftedRoadRow const& r) { return r.seg.id == id; });
    if (it == m_rowsBackingStorage.end())
        return;
    io::RoadSegment const& seg = it->seg;
    auto const result = m_viewer->previewSegmentImpact(seg.fromX, seg.fromY,
                                                       seg.toX,   seg.toY,
                                                       seg.width);
    QMessageBox box(this);
    box.setWindowTitle(tr("Handcrafted road impact preview"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr(
        "Segment #%1 corridor scan:\n"
        "  Polygons to retag: %2\n"
        "  Tiles scanned:     %3\n"
        "  Polygons examined: %4\n"
        "\n"
        "Affected polygons are highlighted in translucent yellow on the "
        "viewer for ~10 seconds.")
        .arg(id)
        .arg(result.polyRefs.size())
        .arg(result.tilesScanned)
        .arg(result.polysExamined));
    if (m_statusLabel)
        m_statusLabel->setText(tr(
            "Preview impact for #%1: %2 polygons / %3 tiles scanned.")
            .arg(id).arg(result.polyRefs.size()).arg(result.tilesScanned));
    box.exec();
}

void HandcraftedRoadDock::onApplyToLocalNavmeshClicked()
{
    if (!m_viewer)
        return;
    uint32_t const id = selectedSegmentId();
    if (id == 0)
        return;
    auto it = std::find_if(m_rowsBackingStorage.begin(), m_rowsBackingStorage.end(),
        [id](HandcraftedRoadRow const& r) { return r.seg.id == id; });
    if (it == m_rowsBackingStorage.end())
        return;
    io::RoadSegment const& seg = it->seg;
    size_t const tagged = m_viewer->applyHandcraftedSegmentToLocalNavmesh(
        seg.fromX, seg.fromY, seg.toX, seg.toY, seg.width);

    if (m_statusLabel)
        m_statusLabel->setText(tr(
            "Applied #%1 to local navmesh: %2 polygon(s) retagged "
            "NAV_AREA_ROAD (in-memory only).")
            .arg(id).arg(tagged));
    QMessageBox::information(this, tr("Apply to local navmesh"),
        tr("Segment #%1 retagged %2 polygon(s) NAV_AREA_ROAD on the in-"
           "memory navmesh.\n\n"
           "The gold auto-road overlay should now include this corridor.  "
           "The DB row is untouched; the worldserver applies handcrafted-"
           "road tagging at map load from the SQL table.")
        .arg(id).arg(tagged));
}

// ---------------------------------------------------------------------------
// Bulk operations: edit-many / delete-many.  The repo only exposes single-row
// update/remove (each wrapped in its own START TRANSACTION / COMMIT), so the
// bulk paths talk to the MySqlClient directly to keep the whole batch atomic.
// Selection helpers (select-all-visible / invert) honour the comment filter
// so an operator can type "Elwynn" and bulk-tag only those rows.
// ---------------------------------------------------------------------------

void HandcraftedRoadDock::onBulkEditClicked()
{
    if (!m_client || !m_client->isConnected())
    {
        QMessageBox::warning(this, tr("Bulk edit handcrafted roads"),
            tr("Connect to the world DB first (Database -> Connect...)."));
        return;
    }
    std::vector<uint32_t> const ids = selectedSegmentIds();
    if (ids.size() < 2)
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Bulk edit %1 segments").arg(int(ids.size())));
    auto* form = new QFormLayout(&dlg);

    // Comment row.
    auto* setCommentCheck = new QCheckBox(tr("Set comment"), &dlg);
    auto* commentEdit     = new QLineEdit(&dlg);
    commentEdit->setEnabled(false);
    commentEdit->setPlaceholderText(tr("e.g. Elwynn road"));
    QObject::connect(setCommentCheck, &QCheckBox::toggled,
                     commentEdit, &QLineEdit::setEnabled);
    form->addRow(setCommentCheck, commentEdit);

    // Width row.
    auto* setWidthCheck = new QCheckBox(tr("Set width"), &dlg);
    auto* widthSpin     = new QDoubleSpinBox(&dlg);
    widthSpin->setRange(1.0, 64.0);
    widthSpin->setSingleStep(0.5);
    widthSpin->setDecimals(2);
    widthSpin->setValue(8.0);
    widthSpin->setSuffix(tr(" yards"));
    widthSpin->setEnabled(false);
    QObject::connect(setWidthCheck, &QCheckBox::toggled,
                     widthSpin, &QDoubleSpinBox::setEnabled);
    form->addRow(setWidthCheck, widthSpin);

    // Verified row.
    auto* setVerifiedCheck = new QCheckBox(tr("Set verified"), &dlg);
    auto* verifiedCombo    = new QComboBox(&dlg);
    verifiedCombo->addItem(tr("0 = no"),  int(0));
    verifiedCombo->addItem(tr("1 = yes"), int(1));
    verifiedCombo->setEnabled(false);
    QObject::connect(setVerifiedCheck, &QCheckBox::toggled,
                     verifiedCombo, &QComboBox::setEnabled);
    form->addRow(setVerifiedCheck, verifiedCombo);

    auto* infoLabel = new QLabel(
        tr("Each checked field is applied to all %1 selected segments in a "
           "single transaction.  Unchecked fields are left untouched.")
            .arg(int(ids.size())), &dlg);
    infoLabel->setStyleSheet(QStringLiteral("color: #888;"));
    infoLabel->setWordWrap(true);
    form->addRow(infoLabel);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    bool const wantComment  = setCommentCheck->isChecked();
    bool const wantWidth    = setWidthCheck->isChecked();
    bool const wantVerified = setVerifiedCheck->isChecked();
    if (!wantComment && !wantWidth && !wantVerified)
    {
        QMessageBox::information(this, tr("Bulk edit handcrafted roads"),
            tr("No fields were checked.  Nothing to apply."));
        return;
    }

    QString const newComment = commentEdit->text();
    double const newWidth    = widthSpin->value();
    int const newVerified    = verifiedCombo->currentData().toInt();

    // Build the SET clause once; reuse it for every row in the batch.
    std::string setClause;
    auto appendSet = [&](std::string const& expr) {
        if (!setClause.empty()) setClause += ", ";
        setClause += expr;
    };
    if (wantComment)
    {
        std::string const esc = m_client->escapeString(newComment.toStdString());
        appendSet(std::string("comment='") + esc + "'");
    }
    if (wantWidth)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "width=%.4f", newWidth);
        appendSet(buf);
    }
    if (wantVerified)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "verified=%d", newVerified ? 1 : 0);
        appendSet(buf);
    }

    auto err = m_client->exec("START TRANSACTION");
    if (!err.ok())
    {
        showToast(tr("\xE2\x9C\x97 Save failed: BEGIN: %1")
                  .arg(QString::fromStdString(err.message)), QStringLiteral("err"));
        QMessageBox::critical(this, tr("Bulk edit handcrafted roads"),
            tr("Could not begin transaction: %1")
                .arg(QString::fromStdString(err.message)));
        return;
    }

    uint64_t totalAffected = 0;
    bool sqlOk = true;
    for (uint32_t id : ids)
    {
        std::string const sql = "UPDATE handcrafted_road SET " + setClause
                              + " WHERE id=" + std::to_string(id);
        uint64_t affected = 0;
        auto const ue = m_client->exec(sql, &affected);
        if (!ue.ok())
        {
            (void)m_client->exec("ROLLBACK");
            showToast(tr("\xE2\x9C\x97 Save failed: UPDATE id %1 rolled back").arg(id),
                      QStringLiteral("err"));
            QMessageBox::critical(this, tr("Bulk edit handcrafted roads"),
                tr("UPDATE failed for id=%1 (rolled back): %2")
                    .arg(id).arg(QString::fromStdString(ue.message)));
            sqlOk = false;
            break;
        }
        totalAffected += affected;
    }

    if (!sqlOk)
        return;

    err = m_client->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_client->exec("ROLLBACK");
        showToast(tr("\xE2\x9C\x97 Save failed: COMMIT rolled back"), QStringLiteral("err"));
        QMessageBox::critical(this, tr("Bulk edit handcrafted roads"),
            tr("COMMIT failed (rolled back): %1")
                .arg(QString::fromStdString(err.message)));
        return;
    }

    refreshFromDb();
    pushPolylinesToViewer();
    if (m_statusLabel)
        m_statusLabel->setText(tr("Bulk edit: %1 row(s) updated.")
                               .arg(int(totalAffected)));
    QString const commentTag  = wantComment  ? QStringLiteral("'") + newComment + QStringLiteral("'")
                                              : QStringLiteral("<unchanged>");
    QString const widthTag    = wantWidth    ? QString::number(newWidth, 'f', 2)
                                              : QStringLiteral("<unchanged>");
    QString const verifiedTag = wantVerified ? QString::number(newVerified)
                                              : QStringLiteral("<unchanged>");
    // Build a compact id list for the commit diag line so a grep tells
    // the operator exactly which rows the bulk batch touched.
    QString idListStr;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i) idListStr += QStringLiteral(", ");
        idListStr += QString::number(ids[i]);
    }
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] commit \xE2\x9C\x93: action=bulk_update, ids=[%1], rowsAffected=%2")
            .arg(idListStr).arg(int(totalAffected));
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] bulk edit: %1 rows updated (comment=%2 width=%3 verified=%4)")
            .arg(int(totalAffected))
            .arg(commentTag)
            .arg(widthTag)
            .arg(verifiedTag);
    showToast(tr("\xE2\x9C\x93 Updated %1 segment(s)").arg(int(totalAffected)),
              QStringLiteral("ok"));
}

void HandcraftedRoadDock::onBulkDeleteClicked()
{
    if (!m_client || !m_client->isConnected())
    {
        QMessageBox::warning(this, tr("Bulk delete handcrafted roads"),
            tr("Connect to the world DB first (Database -> Connect...)."));
        return;
    }
    std::vector<uint32_t> const ids = selectedSegmentIds();
    if (ids.size() < 2)
        return;

    auto const reply = QMessageBox::question(this,
        tr("Bulk delete handcrafted roads"),
        tr("Delete %1 segments?  This cannot be undone.").arg(int(ids.size())),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Yes)
        return;

    // Build the "id IN (a, b, c, ...)" clause from the operator's selection.
    std::string inList;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i) inList += ", ";
        inList += std::to_string(ids[i]);
    }

    auto err = m_client->exec("START TRANSACTION");
    if (!err.ok())
    {
        showToast(tr("\xE2\x9C\x97 Save failed: BEGIN: %1")
                  .arg(QString::fromStdString(err.message)), QStringLiteral("err"));
        QMessageBox::critical(this, tr("Bulk delete handcrafted roads"),
            tr("Could not begin transaction: %1")
                .arg(QString::fromStdString(err.message)));
        return;
    }
    std::string const sql = "DELETE FROM handcrafted_road WHERE id IN (" + inList + ")";
    uint64_t affected = 0;
    auto de = m_client->exec(sql, &affected);
    if (!de.ok())
    {
        (void)m_client->exec("ROLLBACK");
        showToast(tr("\xE2\x9C\x97 Save failed: bulk DELETE rolled back"),
                  QStringLiteral("err"));
        QMessageBox::critical(this, tr("Bulk delete handcrafted roads"),
            tr("DELETE failed (rolled back): %1")
                .arg(QString::fromStdString(de.message)));
        return;
    }
    err = m_client->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_client->exec("ROLLBACK");
        showToast(tr("\xE2\x9C\x97 Save failed: COMMIT rolled back"), QStringLiteral("err"));
        QMessageBox::critical(this, tr("Bulk delete handcrafted roads"),
            tr("COMMIT failed (rolled back): %1")
                .arg(QString::fromStdString(err.message)));
        return;
    }

    refreshFromDb();
    pushPolylinesToViewer();
    if (m_statusLabel)
        m_statusLabel->setText(tr("Bulk delete: %1 row(s) removed.")
                               .arg(int(affected)));
    QString idListStr;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i) idListStr += QStringLiteral(", ");
        idListStr += QString::number(ids[i]);
    }
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] commit \xE2\x9C\x93: action=bulk_delete, ids=[%1], rowsAffected=%2")
            .arg(idListStr).arg(int(affected));
    qDebug().noquote() << QStringLiteral(
        "[handcrafted-road] bulk delete: %1 rows removed (ids requested=%2)")
            .arg(int(affected))
            .arg(int(ids.size()));
    showToast(tr("\xE2\x9C\x93 Deleted %1 segment(s)").arg(int(affected)),
              QStringLiteral("warn"));
}

void HandcraftedRoadDock::onSelectAllVisibleClicked()
{
    if (!m_table) return;
    QItemSelectionModel* sm = m_table->selectionModel();
    if (!sm) return;
    int const cols = m_table->columnCount();
    if (cols <= 0) return;
    QItemSelection sel;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        if (m_table->isRowHidden(row))
            continue;
        QModelIndex const topLeft     = m_table->model()->index(row, 0);
        QModelIndex const bottomRight = m_table->model()->index(row, cols - 1);
        sel.select(topLeft, bottomRight);
    }
    sm->clear();
    sm->select(sel, QItemSelectionModel::Select);
    onSelectionChanged();
}

void HandcraftedRoadDock::onInvertSelectionClicked()
{
    if (!m_table) return;
    QItemSelectionModel* sm = m_table->selectionModel();
    if (!sm) return;
    int const cols = m_table->columnCount();
    if (cols <= 0) return;
    QItemSelection toggle;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        if (m_table->isRowHidden(row))
            continue;
        QModelIndex const topLeft     = m_table->model()->index(row, 0);
        QModelIndex const bottomRight = m_table->model()->index(row, cols - 1);
        toggle.select(topLeft, bottomRight);
    }
    sm->select(toggle, QItemSelectionModel::Toggle);
    onSelectionChanged();
}

} // namespace world_editor::app
