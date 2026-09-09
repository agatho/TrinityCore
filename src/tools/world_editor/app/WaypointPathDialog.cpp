#include "WaypointPathDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Render the left-pane list label for a waypoint_path row.  Comment-empty
// rows show just the PathId so operators can still pick them.
QString pathLabel(uint32_t pathId, QString const& comment)
{
    if (comment.isEmpty())
        return QString::number(pathId);
    return QStringLiteral("%1 -- %2").arg(pathId).arg(comment);
}

} // namespace

WaypointPathDialog::WaypointPathDialog(db::MySqlClient* dbClient,
                                       QString const& worldDbName,
                                       QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Waypoint paths"));
    setModal(true);
    resize(1100, 680);

    auto* outer = new QVBoxLayout(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter, 1);

    // -- Left pane: search + list + path-level toolbar -----------------
    auto* leftWrap = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(new QLabel(tr("Search:"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("substring on \"<PathId> -- <Comment>\""));
    searchRow->addWidget(m_searchEdit, 1);
    leftLayout->addLayout(searchRow);

    m_pathList = new QListWidget(this);
    m_pathList->setSortingEnabled(true);
    m_pathList->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_pathList, 1);

    auto* pathToolbar = new QHBoxLayout;
    m_newPathBtn    = new QPushButton(tr("New path"), this);
    m_deletePathBtn = new QPushButton(tr("Delete path"), this);
    m_clonePathBtn  = new QPushButton(tr("Clone path"), this);
    m_deletePathBtn->setEnabled(false);
    m_clonePathBtn ->setEnabled(false);
    pathToolbar->addWidget(m_newPathBtn);
    pathToolbar->addWidget(m_deletePathBtn);
    pathToolbar->addWidget(m_clonePathBtn);
    pathToolbar->addStretch(1);
    leftLayout->addLayout(pathToolbar);

    splitter->addWidget(leftWrap);

    // -- Right pane: header form + node table + node toolbar ----------
    auto* rightWrap = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* form = new QFormLayout;
    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setMaxLength(255);
    form->addRow(tr("Comment:"), m_commentEdit);

    m_moveTypeSpin = new QSpinBox(this);
    m_moveTypeSpin->setRange(0, 255);
    form->addRow(tr("MoveType:"), m_moveTypeSpin);

    m_flagsSpin = new QSpinBox(this);
    m_flagsSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("Flags:"), m_flagsSpin);

    m_velocitySpin = new QDoubleSpinBox(this);
    m_velocitySpin->setRange(0.0, 100.0);
    m_velocitySpin->setDecimals(3);
    m_velocitySpin->setSingleStep(0.1);
    form->addRow(tr("Velocity:"), m_velocitySpin);

    rightLayout->addLayout(form);

    auto* saveRow = new QHBoxLayout;
    m_saveHeaderBtn = new QPushButton(tr("Save header"), this);
    m_saveHeaderBtn->setEnabled(false);
    saveRow->addWidget(m_saveHeaderBtn);
    saveRow->addStretch(1);
    rightLayout->addLayout(saveRow);

    m_nodeTable = new QTableWidget(this);
    m_nodeTable->setColumnCount(6);
    m_nodeTable->setHorizontalHeaderLabels({
        tr("NodeId"), tr("X"), tr("Y"), tr("Z"),
        tr("Orientation"), tr("Delay") });
    m_nodeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_nodeTable->horizontalHeader()->setStretchLastSection(true);
    m_nodeTable->verticalHeader()->setVisible(false);
    m_nodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_nodeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_nodeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_nodeTable->setSortingEnabled(false);  // NodeId order matters; sort would mislead.
    rightLayout->addWidget(m_nodeTable, 1);

    auto* nodeToolbar = new QHBoxLayout;
    m_addNodeBtn    = new QPushButton(tr("Add node"), this);
    m_editNodeBtn   = new QPushButton(tr("Edit node"), this);
    m_removeNodeBtn = new QPushButton(tr("Remove node"), this);
    m_renumberBtn   = new QPushButton(tr("Renumber nodes"), this);
    m_addNodeBtn   ->setEnabled(false);
    m_editNodeBtn  ->setEnabled(false);
    m_removeNodeBtn->setEnabled(false);
    m_renumberBtn  ->setEnabled(false);
    nodeToolbar->addWidget(m_addNodeBtn);
    nodeToolbar->addWidget(m_editNodeBtn);
    nodeToolbar->addWidget(m_removeNodeBtn);
    nodeToolbar->addWidget(m_renumberBtn);
    nodeToolbar->addStretch(1);
    rightLayout->addLayout(nodeToolbar);

    splitter->addWidget(rightWrap);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    m_statusLabel = new QLabel(tr("Loading..."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_searchEdit,    &QLineEdit::textChanged,
            this, &WaypointPathDialog::onPathSearchChanged);
    connect(m_pathList,      &QListWidget::itemSelectionChanged,
            this, &WaypointPathDialog::onPathSelectionChanged);
    connect(m_saveHeaderBtn, &QPushButton::clicked, this, &WaypointPathDialog::onSaveHeader);
    connect(m_newPathBtn,    &QPushButton::clicked, this, &WaypointPathDialog::onNewPath);
    connect(m_deletePathBtn, &QPushButton::clicked, this, &WaypointPathDialog::onDeletePath);
    connect(m_clonePathBtn,  &QPushButton::clicked, this, &WaypointPathDialog::onClonePath);
    connect(m_addNodeBtn,    &QPushButton::clicked, this, &WaypointPathDialog::onAddNode);
    connect(m_editNodeBtn,   &QPushButton::clicked, this, &WaypointPathDialog::onEditNode);
    connect(m_removeNodeBtn, &QPushButton::clicked, this, &WaypointPathDialog::onRemoveNode);
    connect(m_renumberBtn,   &QPushButton::clicked, this, &WaypointPathDialog::onRenumberNodes);
    connect(m_nodeTable,     &QTableWidget::itemSelectionChanged,
            this, [this]() {
                bool const has = m_nodeTable->currentRow() >= 0;
                m_editNodeBtn  ->setEnabled(has);
                m_removeNodeBtn->setEnabled(has);
            });

    loadPaths();
}

void WaypointPathDialog::onPathSearchChanged(QString const& text)
{
    // Substring filter on the rendered label.  Items are hidden rather
    // than removed so the underlying PathId data survives the filter.
    QString const needle = text.trimmed();
    for (int i = 0; i < m_pathList->count(); ++i)
    {
        auto* item = m_pathList->item(i);
        if (needle.isEmpty())
            item->setHidden(false);
        else
            item->setHidden(!item->text().contains(needle, Qt::CaseInsensitive));
    }
}

void WaypointPathDialog::onPathSelectionChanged()
{
    if (m_loading) return;
    uint32_t const pathId = selectedPathId();
    loadPath(pathId);
    bool const has = pathId != 0;
    m_saveHeaderBtn->setEnabled(has);
    m_deletePathBtn->setEnabled(has);
    m_clonePathBtn ->setEnabled(has);
    m_addNodeBtn   ->setEnabled(has);
    m_renumberBtn  ->setEnabled(has);
}

uint32_t WaypointPathDialog::selectedPathId() const
{
    auto* item = m_pathList->currentItem();
    if (!item) return 0;
    return static_cast<uint32_t>(item->data(Qt::UserRole).toULongLong());
}

bool WaypointPathDialog::currentNodeId(uint32_t& nodeIdOut) const
{
    int const row = m_nodeTable->currentRow();
    if (row < 0) return false;
    auto* cell = m_nodeTable->item(row, 0);
    if (!cell) return false;
    nodeIdOut = static_cast<uint32_t>(cell->data(Qt::DisplayRole).toULongLong());
    return true;
}

void WaypointPathDialog::loadPaths()
{
    m_loading = true;
    uint32_t const previouslySelected = selectedPathId();
    m_pathList->clear();

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }

    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT PathId, COALESCE(Comment, '') FROM %s.waypoint_path ORDER BY PathId",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("waypoint_path query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    QListWidgetItem* restoreSelect = nullptr;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const pid = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        QString const cmt = QString::fromStdString(res.cell(r, 1));
        auto* item = new QListWidgetItem(pathLabel(pid, cmt), m_pathList);
        item->setData(Qt::UserRole, qulonglong(pid));
        if (pid == previouslySelected)
            restoreSelect = item;
    }

    m_statusLabel->setText(tr("paths=%1").arg(res.rowCount()));
    onPathSearchChanged(m_searchEdit->text());
    m_loading = false;

    if (restoreSelect)
        m_pathList->setCurrentItem(restoreSelect);
    else
        loadPath(0);
}

void WaypointPathDialog::loadPath(uint32_t pathId)
{
    m_loading = true;
    m_nodeTable->setRowCount(0);
    m_commentEdit->clear();
    m_moveTypeSpin->setValue(0);
    m_flagsSpin   ->setValue(0);
    m_velocitySpin->setValue(0.0);

    if (pathId == 0 || !m_db || !m_db->isConnected())
    {
        m_loading = false;
        return;
    }

    // Header.
    char hsql[512];
    std::snprintf(hsql, sizeof(hsql),
        "SELECT MoveType, Flags, COALESCE(Velocity, 0), COALESCE(Comment, '') "
        "FROM %s.waypoint_path WHERE PathId=%u",
        m_worldDb.toStdString().c_str(), pathId);
    db::QueryResult hRes;
    auto err = m_db->query(hsql, hRes);
    if (err.ok() && hRes.rowCount() > 0)
    {
        m_moveTypeSpin->setValue(static_cast<int>(hRes.asUInt64(0, 0).value_or(0)));
        m_flagsSpin   ->setValue(static_cast<int>(hRes.asUInt64(0, 1).value_or(0)));
        m_velocitySpin->setValue(hRes.asDouble(0, 2).value_or(0.0));
        m_commentEdit ->setText(QString::fromStdString(hRes.cell(0, 3)));
    }

    // Nodes.
    char nsql[512];
    std::snprintf(nsql, sizeof(nsql),
        "SELECT NodeId, PositionX, PositionY, PositionZ, "
        "       COALESCE(Orientation, 0), Delay "
        "FROM %s.waypoint_path_node WHERE PathId=%u ORDER BY NodeId",
        m_worldDb.toStdString().c_str(), pathId);
    db::QueryResult nRes;
    err = m_db->query(nsql, nRes);
    if (!err.ok())
    {
        m_statusLabel->setText(tr("nodes query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_nodeTable->setRowCount(static_cast<int>(nRes.rowCount()));
    for (size_t r = 0; r < nRes.rowCount(); ++r)
    {
        auto* nidCell = new QTableWidgetItem;
        nidCell->setData(Qt::DisplayRole, qulonglong(nRes.asUInt64(r, 0).value_or(0)));
        m_nodeTable->setItem(int(r), 0, nidCell);

        auto setDouble = [&](int col, double v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, v);
            m_nodeTable->setItem(int(r), col, c);
        };
        setDouble(1, nRes.asDouble(r, 1).value_or(0.0));
        setDouble(2, nRes.asDouble(r, 2).value_or(0.0));
        setDouble(3, nRes.asDouble(r, 3).value_or(0.0));
        setDouble(4, nRes.asDouble(r, 4).value_or(0.0));

        auto* delayCell = new QTableWidgetItem;
        delayCell->setData(Qt::DisplayRole, qulonglong(nRes.asUInt64(r, 5).value_or(0)));
        m_nodeTable->setItem(int(r), 5, delayCell);
    }

    m_statusLabel->setText(tr("PathId=%1  nodes=%2").arg(pathId).arg(nRes.rowCount()));
    m_loading = false;
}

bool WaypointPathDialog::runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut)
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return false;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return false;
    }
    uint64_t affected = 0;
    err = m_db->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("DML failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    if (affectedOut) *affectedOut = affected;
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(affected)));
    return true;
}

void WaypointPathDialog::onSaveHeader()
{
    uint32_t const pid = selectedPathId();
    if (pid == 0) return;

    QString const escCmt = QString::fromStdString(
        m_db->escapeString(m_commentEdit->text().toStdString()));

    QString const upd = QStringLiteral(
        "UPDATE %1.waypoint_path SET MoveType=%2, Flags=%3, Velocity=%4, Comment='%5' "
        "WHERE PathId=%6")
        .arg(m_worldDb)
        .arg(m_moveTypeSpin->value())
        .arg(m_flagsSpin   ->value())
        .arg(QString::number(m_velocitySpin->value(), 'f', 4))
        .arg(escCmt)
        .arg(pid);

    if (runInTransaction(upd, tr("UPDATE waypoint_path (PathId=%1)").arg(pid)))
        loadPaths();
}

void WaypointPathDialog::onNewPath()
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }
    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(PathId), 0)+1 FROM %s.waypoint_path",
        m_worldDb.toStdString().c_str());
    db::QueryResult maxRes;
    auto err = m_db->query(maxSql, maxRes);
    if (!err.ok() || maxRes.rowCount() == 0)
    {
        QMessageBox::critical(this, tr("New path"),
            tr("Could not reserve next PathId: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t const newId = static_cast<uint32_t>(maxRes.asUInt64(0, 0).value_or(0));

    QString const ins = QStringLiteral(
        "INSERT INTO %1.waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%2, 0, 0, 0.0, '')")
        .arg(m_worldDb).arg(newId);

    if (runInTransaction(ins, tr("INSERT waypoint_path (PathId=%1)").arg(newId)))
    {
        loadPaths();
        // Select the new row so the operator can immediately add nodes.
        for (int i = 0; i < m_pathList->count(); ++i)
        {
            auto* item = m_pathList->item(i);
            if (item->data(Qt::UserRole).toULongLong() == newId)
            {
                m_pathList->setCurrentItem(item);
                break;
            }
        }
    }
}

void WaypointPathDialog::onDeletePath()
{
    uint32_t const pid = selectedPathId();
    if (pid == 0) return;

    // Warn when creature.path_id still references this path.  We don't
    // block; some servers wire patrols via creature_addon.PathId instead.
    char refSql[384];
    std::snprintf(refSql, sizeof(refSql),
        "SELECT COUNT(*) FROM %s.creature WHERE path_id=%u",
        m_worldDb.toStdString().c_str(), pid);
    db::QueryResult refRes;
    uint64_t refs = 0;
    if (m_db->query(refSql, refRes).ok() && refRes.rowCount() > 0)
        refs = refRes.asUInt64(0, 0).value_or(0);

    QString msg = tr("Delete waypoint_path PathId=%1 and all its nodes?").arg(pid);
    if (refs > 0)
        msg += tr("\n\nWarning: %1 creature row(s) still reference path_id=%2.")
            .arg(refs).arg(pid);
    if (QMessageBox::question(this, tr("Delete path"), msg,
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Two statements wrapped manually so both DELETEs land in one tx; the
    // single-statement runInTransaction helper isn't enough here.
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    char delNodes[256], delPath[256];
    std::snprintf(delNodes, sizeof(delNodes),
        "DELETE FROM %s.waypoint_path_node WHERE PathId=%u",
        m_worldDb.toStdString().c_str(), pid);
    std::snprintf(delPath, sizeof(delPath),
        "DELETE FROM %s.waypoint_path WHERE PathId=%u",
        m_worldDb.toStdString().c_str(), pid);
    err = m_db->exec(delNodes);
    if (err.ok()) err = m_db->exec(delPath);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Delete failed"),
            tr("DELETE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    m_statusLabel->setText(tr("Deleted PathId=%1 + nodes").arg(pid));
    loadPaths();
}

void WaypointPathDialog::onClonePath()
{
    uint32_t const srcId = selectedPathId();
    if (srcId == 0) return;
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(PathId), 0)+1 FROM %s.waypoint_path",
        m_worldDb.toStdString().c_str());
    db::QueryResult maxRes;
    auto err = m_db->query(maxSql, maxRes);
    if (!err.ok() || maxRes.rowCount() == 0)
    {
        QMessageBox::critical(this, tr("Clone path"),
            tr("Could not reserve next PathId: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t const newId = static_cast<uint32_t>(maxRes.asUInt64(0, 0).value_or(0));

    err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    // Copy header via INSERT...SELECT so we don't have to roundtrip the
    // existing column values back to the client.
    char insHeader[512];
    std::snprintf(insHeader, sizeof(insHeader),
        "INSERT INTO %s.waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "SELECT %u, MoveType, Flags, Velocity, Comment "
        "FROM %s.waypoint_path WHERE PathId=%u",
        m_worldDb.toStdString().c_str(), newId,
        m_worldDb.toStdString().c_str(), srcId);
    err = m_db->exec(insHeader);

    if (err.ok())
    {
        // Same trick for nodes - preserve original NodeId ordering.
        char insNodes[768];
        std::snprintf(insNodes, sizeof(insNodes),
            "INSERT INTO %s.waypoint_path_node "
            "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
            "SELECT %u, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay "
            "FROM %s.waypoint_path_node WHERE PathId=%u",
            m_worldDb.toStdString().c_str(), newId,
            m_worldDb.toStdString().c_str(), srcId);
        err = m_db->exec(insNodes);
    }

    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Clone failed"),
            tr("INSERT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    m_statusLabel->setText(tr("Cloned PathId=%1 -> %2").arg(srcId).arg(newId));
    loadPaths();
    for (int i = 0; i < m_pathList->count(); ++i)
    {
        auto* item = m_pathList->item(i);
        if (item->data(Qt::UserRole).toULongLong() == newId)
        {
            m_pathList->setCurrentItem(item);
            break;
        }
    }
}

void WaypointPathDialog::onAddNode()
{
    openNodeModal(/*editNodeId=*/std::numeric_limits<uint32_t>::max());
}

void WaypointPathDialog::onEditNode()
{
    uint32_t nid = 0;
    if (!currentNodeId(nid)) return;
    openNodeModal(nid);
}

void WaypointPathDialog::onRemoveNode()
{
    uint32_t const pid = selectedPathId();
    uint32_t nid = 0;
    if (pid == 0 || !currentNodeId(nid)) return;

    if (QMessageBox::question(this, tr("Remove node"),
            tr("Delete node NodeId=%1 from PathId=%2?").arg(nid).arg(pid),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QString const del = QStringLiteral(
        "DELETE FROM %1.waypoint_path_node WHERE PathId=%2 AND NodeId=%3")
        .arg(m_worldDb).arg(pid).arg(nid);
    if (runInTransaction(del,
            tr("DELETE waypoint_path_node (PathId=%1, NodeId=%2)").arg(pid).arg(nid)))
        loadPath(pid);
}

void WaypointPathDialog::onRenumberNodes()
{
    uint32_t const pid = selectedPathId();
    if (pid == 0) return;

    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }

    // Fetch existing NodeIds in order; rewrite to a dense 0..N-1 sequence
    // inside a single transaction.  Two-pass: first move all rows to a
    // disjoint range (NodeId += 100000) to avoid PK clashes mid-renumber,
    // then map back to dense 0..N-1.
    char selSql[256];
    std::snprintf(selSql, sizeof(selSql),
        "SELECT NodeId FROM %s.waypoint_path_node WHERE PathId=%u ORDER BY NodeId",
        m_worldDb.toStdString().c_str(), pid);
    db::QueryResult res;
    auto err = m_db->query(selSql, res);
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Renumber failed"),
            tr("SELECT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_statusLabel->setText(tr("Renumber: PathId=%1 has no nodes").arg(pid));
        return;
    }

    err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    // Phase 1: shift every row out of the way.
    char shiftSql[256];
    std::snprintf(shiftSql, sizeof(shiftSql),
        "UPDATE %s.waypoint_path_node SET NodeId = NodeId + 100000 "
        "WHERE PathId=%u",
        m_worldDb.toStdString().c_str(), pid);
    err = m_db->exec(shiftSql);

    // Phase 2: assign dense ids 0..N-1 in the original order.
    for (size_t i = 0; err.ok() && i < res.rowCount(); ++i)
    {
        uint64_t const oldNid = res.asUInt64(i, 0).value_or(0) + 100000;
        char upd[384];
        std::snprintf(upd, sizeof(upd),
            "UPDATE %s.waypoint_path_node SET NodeId=%zu "
            "WHERE PathId=%u AND NodeId=%llu",
            m_worldDb.toStdString().c_str(), i, pid,
            static_cast<unsigned long long>(oldNid));
        err = m_db->exec(upd);
    }

    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Renumber failed"),
            tr("UPDATE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    m_statusLabel->setText(tr("Renumbered PathId=%1 (%2 nodes -> 0..%3)")
        .arg(pid).arg(res.rowCount()).arg(res.rowCount() - 1));
    loadPath(pid);
}

void WaypointPathDialog::openNodeModal(uint32_t editNodeId)
{
    uint32_t const pid = selectedPathId();
    if (pid == 0) return;

    QDialog dlg(this);
    bool const isEdit = (editNodeId != std::numeric_limits<uint32_t>::max());
    dlg.setWindowTitle(isEdit ? tr("Edit waypoint node") : tr("Add waypoint node"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* xSpin = new QDoubleSpinBox(&dlg);
    xSpin->setRange(-1e7, 1e7); xSpin->setDecimals(4); xSpin->setSingleStep(0.1);
    auto* ySpin = new QDoubleSpinBox(&dlg);
    ySpin->setRange(-1e7, 1e7); ySpin->setDecimals(4); ySpin->setSingleStep(0.1);
    auto* zSpin = new QDoubleSpinBox(&dlg);
    zSpin->setRange(-1e7, 1e7); zSpin->setDecimals(4); zSpin->setSingleStep(0.1);
    auto* oSpin = new QDoubleSpinBox(&dlg);
    oSpin->setRange(-12.6, 12.6); oSpin->setDecimals(4); oSpin->setSingleStep(0.1);
    auto* delaySpin = new QSpinBox(&dlg);
    delaySpin->setRange(0, std::numeric_limits<int>::max());

    form->addRow(tr("X:"), xSpin);
    form->addRow(tr("Y:"), ySpin);
    form->addRow(tr("Z:"), zSpin);
    form->addRow(tr("Orientation:"), oSpin);
    form->addRow(tr("Delay (ms):"), delaySpin);

    if (isEdit)
    {
        int const row = m_nodeTable->currentRow();
        if (row >= 0)
        {
            auto getDouble = [&](int col) -> double {
                auto* c = m_nodeTable->item(row, col);
                return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
            };
            xSpin    ->setValue(getDouble(1));
            ySpin    ->setValue(getDouble(2));
            zSpin    ->setValue(getDouble(3));
            oSpin    ->setValue(getDouble(4));
            delaySpin->setValue(static_cast<int>(m_nodeTable->item(row, 5)
                ? m_nodeTable->item(row, 5)->data(Qt::DisplayRole).toLongLong() : 0));
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString const xs = QString::number(xSpin->value(), 'f', 4);
    QString const ys = QString::number(ySpin->value(), 'f', 4);
    QString const zs = QString::number(zSpin->value(), 'f', 4);
    QString const os = QString::number(oSpin->value(), 'f', 4);
    int const delay = delaySpin->value();

    if (isEdit)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.waypoint_path_node "
            "SET PositionX=%2, PositionY=%3, PositionZ=%4, Orientation=%5, Delay=%6 "
            "WHERE PathId=%7 AND NodeId=%8")
            .arg(m_worldDb).arg(xs).arg(ys).arg(zs).arg(os)
            .arg(delay).arg(pid).arg(editNodeId);
        if (runInTransaction(upd,
                tr("UPDATE waypoint_path_node (PathId=%1, NodeId=%2)").arg(pid).arg(editNodeId)))
            loadPath(pid);
    }
    else
    {
        // NodeId = MAX(NodeId)+1, or 0 when the path has no nodes yet.
        char maxSql[256];
        std::snprintf(maxSql, sizeof(maxSql),
            "SELECT COALESCE(MAX(NodeId)+1, 0) FROM %s.waypoint_path_node WHERE PathId=%u",
            m_worldDb.toStdString().c_str(), pid);
        db::QueryResult maxRes;
        auto err = m_db->query(maxSql, maxRes);
        if (!err.ok() || maxRes.rowCount() == 0)
        {
            QMessageBox::critical(this, tr("Add node"),
                tr("Could not reserve NodeId: %1").arg(QString::fromStdString(err.message)));
            return;
        }
        uint32_t const newNid = static_cast<uint32_t>(maxRes.asUInt64(0, 0).value_or(0));

        QString const ins = QStringLiteral(
            "INSERT INTO %1.waypoint_path_node "
            "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
            "VALUES (%2, %3, %4, %5, %6, %7, %8)")
            .arg(m_worldDb).arg(pid).arg(newNid).arg(xs).arg(ys).arg(zs).arg(os).arg(delay);
        if (runInTransaction(ins,
                tr("INSERT waypoint_path_node (PathId=%1, NodeId=%2)").arg(pid).arg(newNid)))
            loadPath(pid);
    }
}

} // namespace world_editor::app
