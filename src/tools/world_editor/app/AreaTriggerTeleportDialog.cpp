#include "AreaTriggerTeleportDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Column indices for the main table.  Kept as a single source of truth
// so Jump-to-destination can read map / X / Y straight off the rendered
// cells without re-parsing.
enum Col : int
{
    COL_ID          = 0,
    COL_NAME        = 1,
    COL_PORT_LOC_ID = 2,
    COL_TARGET_MAP  = 3,
    COL_X           = 4,
    COL_Y           = 5,
    COL_Z           = 6,
    COL_O           = 7,
    COL_COUNT       = 8,
};

} // namespace

AreaTriggerTeleportDialog::AreaTriggerTeleportDialog(db::MySqlClient* dbClient,
                                                    QString const& worldDbName,
                                                    QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Areatrigger teleports"));
    setModal(true);
    resize(1100, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Top search row + Refresh ------------------------------------
    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(new QLabel(tr("Search:"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("substring on ID, Name, or PortLocID"));
    searchRow->addWidget(m_searchEdit, 1);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    searchRow->addWidget(m_refreshBtn);
    outer->addLayout(searchRow);

    // -- Main table --------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("ID"), tr("Name"), tr("PortLocID"), tr("target_map"),
        tr("X"), tr("Y"), tr("Z"), tr("Orientation") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // ORDER BY ID is the canonical view; sort would mislead.
    outer->addWidget(m_table, 1);

    // -- Action buttons ---------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add teleport..."), this);
    m_editBtn   = new QPushButton(tr("Edit teleport"), this);
    m_removeBtn = new QPushButton(tr("Remove teleport"), this);
    m_jumpBtn   = new QPushButton(tr("Jump to destination"), this);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_jumpBtn  ->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_jumpBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Loading..."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &AreaTriggerTeleportDialog::onSearchChanged);
    connect(m_refreshBtn, &QPushButton::clicked, this, &AreaTriggerTeleportDialog::onRefresh);
    connect(m_addBtn,     &QPushButton::clicked, this, &AreaTriggerTeleportDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &AreaTriggerTeleportDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &AreaTriggerTeleportDialog::onRemove);
    connect(m_jumpBtn,    &QPushButton::clicked, this, &AreaTriggerTeleportDialog::onJump);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &AreaTriggerTeleportDialog::onSelectionChanged);

    loadRows();
}

void AreaTriggerTeleportDialog::onSearchChanged(QString const& text)
{
    // Substring filter on ID, Name, and PortLocID columns.  Items are
    // hidden rather than removed so the underlying data survives the
    // filter and a cleared search restores the full list.
    QString const needle = text.trimmed();
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        if (needle.isEmpty())
        {
            m_table->setRowHidden(r, false);
            continue;
        }
        auto* idCell   = m_table->item(r, COL_ID);
        auto* nameCell = m_table->item(r, COL_NAME);
        auto* portCell = m_table->item(r, COL_PORT_LOC_ID);
        QString const idText   = idCell   ? idCell  ->text() : QString();
        QString const nameText = nameCell ? nameCell->text() : QString();
        QString const portText = portCell ? portCell->text() : QString();
        bool const match = idText  .contains(needle, Qt::CaseInsensitive)
                        || nameText.contains(needle, Qt::CaseInsensitive)
                        || portText.contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(r, !match);
    }
}

void AreaTriggerTeleportDialog::onRefresh()
{
    loadRows();
}

void AreaTriggerTeleportDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_jumpBtn  ->setEnabled(has);
}

bool AreaTriggerTeleportDialog::currentRowId(uint32_t& idOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* cell = m_table->item(row, COL_ID);
    if (!cell) return false;
    idOut = static_cast<uint32_t>(cell->data(Qt::DisplayRole).toULongLong());
    rowOut = row;
    return true;
}

void AreaTriggerTeleportDialog::loadRows()
{
    m_loading = true;
    uint32_t prevId = 0;
    {
        int dummyRow = 0;
        (void)currentRowId(prevId, dummyRow);
    }
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }

    // LEFT JOIN world_safe_locs because some areatrigger_teleport rows
    // may reference an unresolved PortLocID; we still want to show the
    // join row and surface the missing destination as blank cells.
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT at.ID, COALESCE(at.Name, ''), at.PortLocID, "
        "       COALESCE(wsl.MapID, 0), "
        "       COALESCE(wsl.LocX, 0), COALESCE(wsl.LocY, 0), "
        "       COALESCE(wsl.LocZ, 0), COALESCE(wsl.Facing, 0) "
        "FROM %s.areatrigger_teleport at "
        "LEFT JOIN %s.world_safe_locs wsl ON wsl.ID = at.PortLocID "
        "ORDER BY at.ID",
        m_worldDb.toStdString().c_str(), m_worldDb.toStdString().c_str());
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("areatrigger_teleport query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    int restoreRow = -1;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const id     = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        QString const name    = QString::fromStdString(res.cell(r, 1));
        uint32_t const portId = static_cast<uint32_t>(res.asUInt64(r, 2).value_or(0));
        uint32_t const tmap   = static_cast<uint32_t>(res.asUInt64(r, 3).value_or(0));

        auto* idCell = new QTableWidgetItem;
        idCell->setData(Qt::DisplayRole, qulonglong(id));
        m_table->setItem(int(r), COL_ID, idCell);

        auto* nameCell = new QTableWidgetItem(name);
        m_table->setItem(int(r), COL_NAME, nameCell);

        auto* portCell = new QTableWidgetItem;
        portCell->setData(Qt::DisplayRole, qulonglong(portId));
        m_table->setItem(int(r), COL_PORT_LOC_ID, portCell);

        auto* mapCell = new QTableWidgetItem;
        mapCell->setData(Qt::DisplayRole, qulonglong(tmap));
        m_table->setItem(int(r), COL_TARGET_MAP, mapCell);

        auto setDouble = [&](int col, double v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, v);
            m_table->setItem(int(r), col, c);
        };
        setDouble(COL_X, res.asDouble(r, 4).value_or(0.0));
        setDouble(COL_Y, res.asDouble(r, 5).value_or(0.0));
        setDouble(COL_Z, res.asDouble(r, 6).value_or(0.0));
        setDouble(COL_O, res.asDouble(r, 7).value_or(0.0));

        if (id == prevId && prevId != 0)
            restoreRow = int(r);
    }

    m_statusLabel->setText(tr("rows=%1").arg(res.rowCount()));
    m_loading = false;
    onSearchChanged(m_searchEdit->text());
    if (restoreRow >= 0)
        m_table->selectRow(restoreRow);
    else
        onSelectionChanged();
}

bool AreaTriggerTeleportDialog::runInTransaction(QStringList const& sqls, QString const& description)
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
    uint64_t totalAffected = 0;
    for (QString const& sql : sqls)
    {
        uint64_t affected = 0;
        err = m_db->exec(sql.toStdString(), &affected);
        if (!err.ok())
        {
            (void)m_db->exec("ROLLBACK");
            QMessageBox::critical(this, tr("DML failed"),
                tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
            return false;
        }
        totalAffected += affected;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(totalAffected)));
    return true;
}

void AreaTriggerTeleportDialog::onAdd()
{
    openModal(std::numeric_limits<uint32_t>::max());
}

void AreaTriggerTeleportDialog::onEdit()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;
    openModal(id);
}

void AreaTriggerTeleportDialog::onRemove()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    auto const choice = QMessageBox::question(this, tr("Remove teleport"),
        tr("Delete areatrigger_teleport.ID=%1?\n\nNote: this leaves the world_safe_locs "
           "destination row intact (it may be referenced by other teleports or "
           "graveyards).").arg(id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.areatrigger_teleport WHERE ID=%2")
        .arg(m_worldDb).arg(id);
    if (runInTransaction(QStringList{ sql }, tr("DELETE areatrigger_teleport (ID=%1)").arg(id)))
        loadRows();
}

void AreaTriggerTeleportDialog::onJump()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    auto getDouble = [&](int col) -> double {
        auto* c = m_table->item(row, col);
        return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
    };
    auto* mapCell = m_table->item(row, COL_TARGET_MAP);
    uint32_t const mapId = mapCell
        ? static_cast<uint32_t>(mapCell->data(Qt::DisplayRole).toULongLong()) : 0;
    float const wx = static_cast<float>(getDouble(COL_X));
    float const wy = static_cast<float>(getDouble(COL_Y));
    emit jumpRequested(mapId, wx, wy);
}

void AreaTriggerTeleportDialog::openModal(uint32_t editingId)
{
    bool const isEdit = (editingId != std::numeric_limits<uint32_t>::max());

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? tr("Edit areatrigger teleport") : tr("Add areatrigger teleport"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, std::numeric_limits<int>::max());
    idSpin->setEnabled(!isEdit);  // PK is immutable.
    form->addRow(tr("ID:"), idSpin);

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setMaxLength(255);
    form->addRow(tr("Name:"), nameEdit);

    auto* portSpin = new QSpinBox(&dlg);
    portSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("PortLocID:"), portSpin);

    auto* mapSpin = new QSpinBox(&dlg);
    mapSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("target_map:"), mapSpin);

    auto mkDouble = [&](double lo, double hi, int decimals, double step) {
        auto* sp = new QDoubleSpinBox(&dlg);
        sp->setRange(lo, hi);
        sp->setDecimals(decimals);
        sp->setSingleStep(step);
        return sp;
    };
    auto* xSpin = mkDouble(-1e7, 1e7, 4, 0.1);
    auto* ySpin = mkDouble(-1e7, 1e7, 4, 0.1);
    auto* zSpin = mkDouble(-1e7, 1e7, 4, 0.1);
    auto* oSpin = mkDouble(-12.6, 12.6, 4, 0.1);
    form->addRow(tr("target_position_x:"), xSpin);
    form->addRow(tr("target_position_y:"), ySpin);
    form->addRow(tr("target_position_z:"), zSpin);
    form->addRow(tr("target_orientation:"), oSpin);

    if (isEdit)
    {
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            auto getDouble = [&](int col) -> double {
                auto* c = m_table->item(row, col);
                return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
            };
            idSpin   ->setValue(static_cast<int>(editingId));
            auto* nameCell = m_table->item(row, COL_NAME);
            nameEdit ->setText(nameCell ? nameCell->text() : QString());
            auto* portCell = m_table->item(row, COL_PORT_LOC_ID);
            portSpin ->setValue(portCell
                ? static_cast<int>(portCell->data(Qt::DisplayRole).toULongLong()) : 0);
            auto* mapCell = m_table->item(row, COL_TARGET_MAP);
            mapSpin  ->setValue(mapCell
                ? static_cast<int>(mapCell->data(Qt::DisplayRole).toULongLong()) : 0);
            xSpin    ->setValue(getDouble(COL_X));
            ySpin    ->setValue(getDouble(COL_Y));
            zSpin    ->setValue(getDouble(COL_Z));
            oSpin    ->setValue(getDouble(COL_O));
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString const escName = QString::fromStdString(
        m_db->escapeString(nameEdit->text().toStdString()));
    QString const xs = QString::number(xSpin->value(), 'f', 4);
    QString const ys = QString::number(ySpin->value(), 'f', 4);
    QString const zs = QString::number(zSpin->value(), 'f', 4);
    QString const os = QString::number(oSpin->value(), 'f', 4);
    uint32_t const portLocId = static_cast<uint32_t>(portSpin->value());
    int const targetMap = mapSpin->value();

    // Build one compound multi-statement SQL string that touches both
    // tables; m_db->exec() runs it inside a single START TRANSACTION /
    // COMMIT envelope.  This keeps the join row and the destination row
    // atomic across failure.
    //
    // world_safe_locs upsert: INSERT ON DUPLICATE KEY UPDATE.  TC's
    // destination data is normalized per-PortLocID, so any existing row
    // gets its map/x/y/z/facing rewritten; otherwise a fresh row is
    // created (with Comment derived from Name).
    QString wslUpsert = QStringLiteral(
        "INSERT INTO %1.world_safe_locs (ID, MapID, LocX, LocY, LocZ, Facing, Comment) "
        "VALUES (%2, %3, %4, %5, %6, %7, '%8') "
        "ON DUPLICATE KEY UPDATE "
        "MapID=VALUES(MapID), LocX=VALUES(LocX), LocY=VALUES(LocY), "
        "LocZ=VALUES(LocZ), Facing=VALUES(Facing)")
        .arg(m_worldDb).arg(portLocId).arg(targetMap)
        .arg(xs).arg(ys).arg(zs).arg(os).arg(escName);

    if (isEdit)
    {
        QString const atrUpd = QStringLiteral(
            "UPDATE %1.areatrigger_teleport SET Name='%2', PortLocID=%3 WHERE ID=%4")
            .arg(m_worldDb).arg(escName).arg(portLocId).arg(editingId);
        if (runInTransaction(QStringList{ wslUpsert, atrUpd },
                tr("UPDATE areatrigger_teleport+world_safe_locs (ID=%1, PortLocID=%2)")
                    .arg(editingId).arg(portLocId)))
            loadRows();
    }
    else
    {
        uint32_t const newId = static_cast<uint32_t>(idSpin->value());

        // PK collision check.  INSERT would already fail server-side but
        // a friendly QMessageBox beats a raw MySQL duplicate-key error.
        char checkSql[256];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.areatrigger_teleport WHERE ID=%u",
            m_worldDb.toStdString().c_str(), newId);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add teleport"),
                tr("areatrigger_teleport.ID=%1 already exists.").arg(newId));
            return;
        }

        QString const atrIns = QStringLiteral(
            "INSERT INTO %1.areatrigger_teleport (ID, PortLocID, Name) "
            "VALUES (%2, %3, '%4')")
            .arg(m_worldDb).arg(newId).arg(portLocId).arg(escName);
        if (runInTransaction(QStringList{ wslUpsert, atrIns },
                tr("INSERT areatrigger_teleport (ID=%1, PortLocID=%2)").arg(newId).arg(portLocId)))
            loadRows();
    }
}

} // namespace world_editor::app
