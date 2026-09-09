#include "WorldSafeLocsDialog.h"

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

// Column indices for the main table.  Kept centralized so Jump can read
// MapID / LocX / LocY straight off the rendered cells without re-parsing.
enum Col : int
{
    COL_ID      = 0,
    COL_MAP     = 1,
    COL_X       = 2,
    COL_Y       = 3,
    COL_Z       = 4,
    COL_FACING  = 5,
    COL_COMMENT = 6,
    COL_COUNT   = 7,
};

} // namespace

WorldSafeLocsDialog::WorldSafeLocsDialog(db::MySqlClient* dbClient,
                                        QString const& worldDbName,
                                        QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("World safe locs (graveyards / portals)"));
    setModal(true);
    resize(1100, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Top search row + Refresh ----------------------------------------
    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(new QLabel(tr("Search:"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr(
        "ID exact, MapID exact (use 'map:N'), or Comment substring"));
    searchRow->addWidget(m_searchEdit, 1);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    searchRow->addWidget(m_refreshBtn);
    outer->addLayout(searchRow);

    // -- Main table ------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("ID"), tr("MapID"), tr("LocX"), tr("LocY"),
        tr("LocZ"), tr("Facing"), tr("Comment") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // ORDER BY ID is canonical; UI sort would mislead.
    outer->addWidget(m_table, 1);

    // -- Action buttons --------------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add loc..."),         this);
    m_editBtn   = new QPushButton(tr("Edit loc"),           this);
    m_removeBtn = new QPushButton(tr("Remove loc"),         this);
    m_jumpBtn   = new QPushButton(tr("Jump to location"),   this);
    m_refsBtn   = new QPushButton(tr("Show references"),    this);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_jumpBtn  ->setEnabled(false);
    m_refsBtn  ->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_jumpBtn);
    btnRow->addWidget(m_refsBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Loading..."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &WorldSafeLocsDialog::onSearchChanged);
    connect(m_refreshBtn, &QPushButton::clicked, this, &WorldSafeLocsDialog::onRefresh);
    connect(m_addBtn,     &QPushButton::clicked, this, &WorldSafeLocsDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &WorldSafeLocsDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &WorldSafeLocsDialog::onRemove);
    connect(m_jumpBtn,    &QPushButton::clicked, this, &WorldSafeLocsDialog::onJump);
    connect(m_refsBtn,    &QPushButton::clicked, this, &WorldSafeLocsDialog::onShowRefs);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &WorldSafeLocsDialog::onSelectionChanged);

    loadRows();
}

void WorldSafeLocsDialog::onSearchChanged(QString const& text)
{
    // Three filter modes (all anchored at the row level):
    //   ID exact          - needle is all-digits, matches COL_ID exactly.
    //   MapID exact       - needle of shape "map:N" or "m:N", matches COL_MAP.
    //   Comment substring - everything else, case-insensitive on COL_COMMENT.
    QString const needle = text.trimmed();

    auto isAllDigits = [](QString const& s) {
        if (s.isEmpty()) return false;
        for (QChar c : s) if (!c.isDigit()) return false;
        return true;
    };

    enum Mode { ModeAll, ModeIdExact, ModeMapExact, ModeCommentSub };
    Mode mode = ModeAll;
    QString payload;
    if (needle.isEmpty())
    {
        mode = ModeAll;
    }
    else if (needle.startsWith(QStringLiteral("map:"), Qt::CaseInsensitive)
          || needle.startsWith(QStringLiteral("m:"),   Qt::CaseInsensitive))
    {
        int const colon = needle.indexOf(QLatin1Char(':'));
        payload = needle.mid(colon + 1).trimmed();
        if (isAllDigits(payload)) mode = ModeMapExact;
        else                      mode = ModeCommentSub;  // fall back gracefully
    }
    else if (isAllDigits(needle))
    {
        mode = ModeIdExact;
        payload = needle;
    }
    else
    {
        mode = ModeCommentSub;
        payload = needle;
    }

    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        if (mode == ModeAll)
        {
            m_table->setRowHidden(r, false);
            continue;
        }
        bool match = false;
        if (mode == ModeIdExact)
        {
            auto* idCell = m_table->item(r, COL_ID);
            QString const idText = idCell ? idCell->text() : QString();
            match = (idText == payload);
        }
        else if (mode == ModeMapExact)
        {
            auto* mapCell = m_table->item(r, COL_MAP);
            QString const mapText = mapCell ? mapCell->text() : QString();
            match = (mapText == payload);
        }
        else // ModeCommentSub
        {
            auto* cmt = m_table->item(r, COL_COMMENT);
            QString const cmtText = cmt ? cmt->text() : QString();
            match = cmtText.contains(payload, Qt::CaseInsensitive);
        }
        m_table->setRowHidden(r, !match);
    }
}

void WorldSafeLocsDialog::onRefresh()
{
    loadRows();
}

void WorldSafeLocsDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_jumpBtn  ->setEnabled(has);
    m_refsBtn  ->setEnabled(has);
}

bool WorldSafeLocsDialog::currentRowId(uint32_t& idOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* cell = m_table->item(row, COL_ID);
    if (!cell) return false;
    idOut = static_cast<uint32_t>(cell->data(Qt::DisplayRole).toULongLong());
    rowOut = row;
    return true;
}

void WorldSafeLocsDialog::loadRows()
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

    // LIMIT 5000 because retail builds have thousands of rows; the dialog
    // is meant for spot-edits, not bulk browsing.  The search box narrows
    // further locally once loaded.
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT ID, MapID, LocX, LocY, LocZ, Facing, COALESCE(Comment, '') "
        "FROM %s.world_safe_locs ORDER BY ID LIMIT 5000",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("world_safe_locs query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    int restoreRow = -1;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const id    = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        uint32_t const mapId = static_cast<uint32_t>(res.asUInt64(r, 1).value_or(0));

        auto* idCell = new QTableWidgetItem;
        idCell->setData(Qt::DisplayRole, qulonglong(id));
        m_table->setItem(int(r), COL_ID, idCell);

        auto* mapCell = new QTableWidgetItem;
        mapCell->setData(Qt::DisplayRole, qulonglong(mapId));
        m_table->setItem(int(r), COL_MAP, mapCell);

        auto setDouble = [&](int col, double v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, v);
            m_table->setItem(int(r), col, c);
        };
        setDouble(COL_X,      res.asDouble(r, 2).value_or(0.0));
        setDouble(COL_Y,      res.asDouble(r, 3).value_or(0.0));
        setDouble(COL_Z,      res.asDouble(r, 4).value_or(0.0));
        setDouble(COL_FACING, res.asDouble(r, 5).value_or(0.0));

        auto* cmtCell = new QTableWidgetItem(QString::fromStdString(res.cell(r, 6)));
        m_table->setItem(int(r), COL_COMMENT, cmtCell);

        if (id == prevId && prevId != 0)
            restoreRow = int(r);
    }

    m_statusLabel->setText(tr("rows=%1 (capped at 5000)").arg(res.rowCount()));
    m_loading = false;
    onSearchChanged(m_searchEdit->text());
    if (restoreRow >= 0)
        m_table->selectRow(restoreRow);
    else
        onSelectionChanged();
}

bool WorldSafeLocsDialog::runInTransaction(QStringList const& sqls, QString const& description)
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

void WorldSafeLocsDialog::onAdd()
{
    openModal(std::numeric_limits<uint32_t>::max());
}

void WorldSafeLocsDialog::onEdit()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;
    openModal(id);
}

void WorldSafeLocsDialog::onRemove()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    // Count inbound references so the operator sees what they'd break.
    // areatrigger_teleport.PortLocID is the high-traffic FK; graveyard_zone.ID
    // is the secondary one (PK in graveyard_zone IS the world_safe_locs.ID).
    char atrSql[256];
    std::snprintf(atrSql, sizeof(atrSql),
        "SELECT COUNT(*) FROM %s.areatrigger_teleport WHERE PortLocID=%u",
        m_worldDb.toStdString().c_str(), id);
    db::QueryResult atrRes;
    (void)m_db->query(atrSql, atrRes);
    uint64_t const atrRefs = atrRes.rowCount() > 0 ? atrRes.asUInt64(0, 0).value_or(0) : 0;

    char gyzSql[256];
    std::snprintf(gyzSql, sizeof(gyzSql),
        "SELECT COUNT(*) FROM %s.graveyard_zone WHERE ID=%u",
        m_worldDb.toStdString().c_str(), id);
    db::QueryResult gyzRes;
    (void)m_db->query(gyzSql, gyzRes);
    uint64_t const gyzRefs = gyzRes.rowCount() > 0 ? gyzRes.asUInt64(0, 0).value_or(0) : 0;

    QString warning;
    if (atrRefs > 0 || gyzRefs > 0)
    {
        warning = tr("\n\nWARNING: this row is referenced by:\n"
                     "  - areatrigger_teleport.PortLocID = %1 row(s)\n"
                     "  - graveyard_zone.ID             = %2 row(s)\n\n"
                     "Deleting it will break those teleports / graveyards.")
            .arg(qulonglong(atrRefs)).arg(qulonglong(gyzRefs));
    }

    auto const choice = QMessageBox::question(this, tr("Remove world safe loc"),
        tr("Delete world_safe_locs.ID=%1?%2").arg(id).arg(warning),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.world_safe_locs WHERE ID=%2")
        .arg(m_worldDb).arg(id);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE world_safe_locs (ID=%1)").arg(id)))
        loadRows();
}

void WorldSafeLocsDialog::onJump()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    auto getDouble = [&](int col) -> double {
        auto* c = m_table->item(row, col);
        return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
    };
    auto* mapCell = m_table->item(row, COL_MAP);
    uint32_t const mapId = mapCell
        ? static_cast<uint32_t>(mapCell->data(Qt::DisplayRole).toULongLong()) : 0;
    float const wx = static_cast<float>(getDouble(COL_X));
    float const wy = static_cast<float>(getDouble(COL_Y));
    emit jumpRequested(mapId, wx, wy);
}

void WorldSafeLocsDialog::onShowRefs()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    // areatrigger_teleport rows pointing at this PortLocID.
    char atrSql[384];
    std::snprintf(atrSql, sizeof(atrSql),
        "SELECT ID, COALESCE(Name, '') FROM %s.areatrigger_teleport "
        "WHERE PortLocID=%u ORDER BY ID LIMIT 500",
        m_worldDb.toStdString().c_str(), id);
    db::QueryResult atrRes;
    auto err = m_db->query(atrSql, atrRes);
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Show references"),
            tr("areatrigger_teleport query failed: %1")
                .arg(QString::fromStdString(err.message)));
        return;
    }

    // graveyard_zone rows pointing at this ID (same PK by convention).
    char gyzSql[384];
    std::snprintf(gyzSql, sizeof(gyzSql),
        "SELECT ID, GhostZone FROM %s.graveyard_zone "
        "WHERE ID=%u ORDER BY GhostZone LIMIT 500",
        m_worldDb.toStdString().c_str(), id);
    db::QueryResult gyzRes;
    (void)m_db->query(gyzSql, gyzRes);

    // Build a compact two-section modal: top = areatrigger_teleport rows,
    // bottom = graveyard_zone rows.  Read-only summary.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("References for world_safe_locs.ID=%1").arg(id));
    dlg.setModal(true);
    dlg.resize(700, 500);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(
        tr("areatrigger_teleport rows (PortLocID = %1): %2")
            .arg(id).arg(qulonglong(atrRes.rowCount())), &dlg));

    auto* atrTable = new QTableWidget(&dlg);
    atrTable->setColumnCount(2);
    atrTable->setHorizontalHeaderLabels({ tr("ID"), tr("Name") });
    atrTable->horizontalHeader()->setStretchLastSection(true);
    atrTable->verticalHeader()->setVisible(false);
    atrTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    atrTable->setRowCount(static_cast<int>(atrRes.rowCount()));
    for (size_t r = 0; r < atrRes.rowCount(); ++r)
    {
        auto* idCell = new QTableWidgetItem;
        idCell->setData(Qt::DisplayRole,
            qulonglong(atrRes.asUInt64(r, 0).value_or(0)));
        atrTable->setItem(int(r), 0, idCell);
        atrTable->setItem(int(r), 1,
            new QTableWidgetItem(QString::fromStdString(atrRes.cell(r, 1))));
    }
    layout->addWidget(atrTable, 1);

    layout->addWidget(new QLabel(
        tr("graveyard_zone rows (ID = %1): %2")
            .arg(id).arg(qulonglong(gyzRes.rowCount())), &dlg));

    auto* gyzTable = new QTableWidget(&dlg);
    gyzTable->setColumnCount(2);
    gyzTable->setHorizontalHeaderLabels({ tr("ID"), tr("GhostZone") });
    gyzTable->horizontalHeader()->setStretchLastSection(true);
    gyzTable->verticalHeader()->setVisible(false);
    gyzTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gyzTable->setRowCount(static_cast<int>(gyzRes.rowCount()));
    for (size_t r = 0; r < gyzRes.rowCount(); ++r)
    {
        auto* idCell = new QTableWidgetItem;
        idCell->setData(Qt::DisplayRole,
            qulonglong(gyzRes.asUInt64(r, 0).value_or(0)));
        gyzTable->setItem(int(r), 0, idCell);
        auto* zCell = new QTableWidgetItem;
        zCell->setData(Qt::DisplayRole,
            qulonglong(gyzRes.asUInt64(r, 1).value_or(0)));
        gyzTable->setItem(int(r), 1, zCell);
    }
    layout->addWidget(gyzTable, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(bb);

    dlg.exec();
}

void WorldSafeLocsDialog::openModal(uint32_t editingId)
{
    bool const isEdit = (editingId != std::numeric_limits<uint32_t>::max());

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? tr("Edit world safe loc") : tr("Add world safe loc"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, std::numeric_limits<int>::max());
    idSpin->setEnabled(!isEdit);  // PK is immutable.
    form->addRow(tr("ID:"), idSpin);

    auto* mapSpin = new QSpinBox(&dlg);
    mapSpin->setRange(0, std::numeric_limits<int>::max());
    form->addRow(tr("MapID:"), mapSpin);

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
    auto* fSpin = mkDouble(-12.6, 12.6, 4, 0.1);
    form->addRow(tr("LocX:"),   xSpin);
    form->addRow(tr("LocY:"),   ySpin);
    form->addRow(tr("LocZ:"),   zSpin);
    form->addRow(tr("Facing:"), fSpin);

    auto* commentEdit = new QLineEdit(&dlg);
    commentEdit->setMaxLength(100);
    form->addRow(tr("Comment:"), commentEdit);

    if (isEdit)
    {
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            auto getDouble = [&](int col) -> double {
                auto* c = m_table->item(row, col);
                return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
            };
            idSpin->setValue(static_cast<int>(editingId));
            auto* mapCell = m_table->item(row, COL_MAP);
            mapSpin->setValue(mapCell
                ? static_cast<int>(mapCell->data(Qt::DisplayRole).toULongLong()) : 0);
            xSpin->setValue(getDouble(COL_X));
            ySpin->setValue(getDouble(COL_Y));
            zSpin->setValue(getDouble(COL_Z));
            fSpin->setValue(getDouble(COL_FACING));
            auto* cmt = m_table->item(row, COL_COMMENT);
            commentEdit->setText(cmt ? cmt->text() : QString());
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString const escComment = QString::fromStdString(
        m_db->escapeString(commentEdit->text().toStdString()));
    QString const xs = QString::number(xSpin->value(), 'f', 4);
    QString const ys = QString::number(ySpin->value(), 'f', 4);
    QString const zs = QString::number(zSpin->value(), 'f', 4);
    QString const fs = QString::number(fSpin->value(), 'f', 4);
    uint32_t const mapId = static_cast<uint32_t>(mapSpin->value());

    if (isEdit)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.world_safe_locs SET MapID=%2, LocX=%3, LocY=%4, LocZ=%5, "
            "Facing=%6, Comment='%7' WHERE ID=%8")
            .arg(m_worldDb).arg(mapId).arg(xs).arg(ys).arg(zs).arg(fs)
            .arg(escComment).arg(editingId);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE world_safe_locs (ID=%1)").arg(editingId)))
            loadRows();
    }
    else
    {
        uint32_t const newId = static_cast<uint32_t>(idSpin->value());

        // PK collision check.  INSERT would already fail server-side but a
        // friendly QMessageBox beats a raw MySQL duplicate-key error.
        char checkSql[256];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.world_safe_locs WHERE ID=%u",
            m_worldDb.toStdString().c_str(), newId);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add world safe loc"),
                tr("world_safe_locs.ID=%1 already exists.").arg(newId));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.world_safe_locs (ID, MapID, LocX, LocY, LocZ, Facing, Comment) "
            "VALUES (%2, %3, %4, %5, %6, %7, '%8')")
            .arg(m_worldDb).arg(newId).arg(mapId).arg(xs).arg(ys).arg(zs)
            .arg(fs).arg(escComment);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT world_safe_locs (ID=%1)").arg(newId)))
            loadRows();
    }
}

} // namespace world_editor::app
