#include "AccessRequirementDialog.h"

#include "ItemInfoDock.h"
#include "MainWindow.h"
#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
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

// Column indices for the main table.  Kept centralized so onLookupItem can
// read the `item` cell off the rendered row without re-querying the DB.
enum Col : int
{
    COL_MAP_ID      = 0,
    COL_DIFFICULTY  = 1,
    COL_LEVEL_MIN   = 2,
    COL_LEVEL_MAX   = 3,
    COL_ITEM        = 4,
    COL_ITEM2       = 5,
    COL_QUEST_A     = 6,
    COL_QUEST_H     = 7,
    COL_ACHIEVEMENT = 8,
    COL_COMMENT     = 9,
    COL_COUNT       = 10,
};

} // namespace

AccessRequirementDialog::AccessRequirementDialog(db::MySqlClient* dbClient,
                                                 QString const& worldDbName,
                                                 ::world_editor::MainWindow* mainWindow,
                                                 QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_mainWindow(mainWindow), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Instance access requirements"));
    setModal(true);
    resize(1200, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row --------------------------------------------------
    // Both filters default to 0 = "show all".  Either filter narrows the
    // SELECT WHERE clause server-side so we never round-trip the entire
    // table back when the operator only cares about one map.
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("mapId:"), this));
    m_mapFilter = new QSpinBox(this);
    m_mapFilter->setRange(0, std::numeric_limits<int>::max());
    m_mapFilter->setSpecialValueText(tr("(any)"));
    filterRow->addWidget(m_mapFilter);
    filterRow->addWidget(new QLabel(tr("difficulty:"), this));
    m_difficultyFilter = new QSpinBox(this);
    m_difficultyFilter->setRange(0, 255);
    m_difficultyFilter->setSpecialValueText(tr("(any)"));
    filterRow->addWidget(m_difficultyFilter);
    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);
    filterRow->addStretch(1);
    outer->addLayout(filterRow);

    // -- Main table ------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("mapId"), tr("difficulty"), tr("level_min"), tr("level_max"),
        tr("item"), tr("item2"), tr("quest_done_A"), tr("quest_done_H"),
        tr("completed_achievement"), tr("comment") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // canonical ORDER BY (mapId, difficulty).
    outer->addWidget(m_table, 1);

    // -- Action buttons --------------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn        = new QPushButton(tr("Add gate..."),         this);
    m_editBtn       = new QPushButton(tr("Edit gate"),           this);
    m_removeBtn     = new QPushButton(tr("Remove gate"),         this);
    m_lookupItemBtn = new QPushButton(tr("Lookup item template"), this);
    m_editBtn      ->setEnabled(false);
    m_removeBtn    ->setEnabled(false);
    m_lookupItemBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_lookupItemBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Press Load to populate."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_loadBtn,        &QPushButton::clicked, this, &AccessRequirementDialog::onLoadClicked);
    connect(m_addBtn,         &QPushButton::clicked, this, &AccessRequirementDialog::onAddClicked);
    connect(m_editBtn,        &QPushButton::clicked, this, &AccessRequirementDialog::onEditClicked);
    connect(m_removeBtn,      &QPushButton::clicked, this, &AccessRequirementDialog::onRemoveClicked);
    connect(m_lookupItemBtn,  &QPushButton::clicked, this, &AccessRequirementDialog::onLookupItemClicked);
    connect(m_table,          &QTableWidget::itemSelectionChanged,
            this, &AccessRequirementDialog::onSelectionChanged);

    loadRows();
}

void AccessRequirementDialog::onLoadClicked()
{
    loadRows();
}

void AccessRequirementDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0;
    m_editBtn      ->setEnabled(has);
    m_removeBtn    ->setEnabled(has);
    m_lookupItemBtn->setEnabled(has);
}

bool AccessRequirementDialog::currentRowPk(uint32_t& mapIdOut, uint32_t& difficultyOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* mapCell  = m_table->item(row, COL_MAP_ID);
    auto* diffCell = m_table->item(row, COL_DIFFICULTY);
    if (!mapCell || !diffCell) return false;
    mapIdOut      = static_cast<uint32_t>(mapCell ->data(Qt::DisplayRole).toULongLong());
    difficultyOut = static_cast<uint32_t>(diffCell->data(Qt::DisplayRole).toULongLong());
    rowOut = row;
    return true;
}

void AccessRequirementDialog::loadRows()
{
    m_loading = true;
    uint32_t prevMap = 0, prevDiff = 0;
    {
        int dummyRow = 0;
        (void)currentRowPk(prevMap, prevDiff, dummyRow);
    }
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }

    // Build the WHERE clause from the optional filters.  0 = "(any)" so we
    // skip the predicate; >0 narrows server-side.
    int const mapFilter  = m_mapFilter->value();
    int const diffFilter = m_difficultyFilter->value();
    std::string whereClause;
    if (mapFilter > 0 || diffFilter > 0)
    {
        whereClause = "WHERE ";
        bool prev = false;
        char buf[128];
        if (mapFilter > 0)
        {
            std::snprintf(buf, sizeof(buf), "mapId=%d", mapFilter);
            whereClause += buf;
            prev = true;
        }
        if (diffFilter > 0)
        {
            if (prev) whereClause += " AND ";
            std::snprintf(buf, sizeof(buf), "difficulty=%d", diffFilter);
            whereClause += buf;
        }
        whereClause += " ";
    }

    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT mapId, difficulty, level_min, level_max, item, item2, "
        "quest_done_A, quest_done_H, completed_achievement, COALESCE(comment, '') "
        "FROM %s.access_requirement %s"
        "ORDER BY mapId, difficulty LIMIT 5000",
        m_worldDb.toStdString().c_str(), whereClause.c_str());

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("access_requirement query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    int restoreRow = -1;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        auto setUint = [&](int col, uint64_t v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, qulonglong(v));
            m_table->setItem(int(r), col, c);
        };
        uint64_t const mapId      = res.asUInt64(r, 0).value_or(0);
        uint64_t const difficulty = res.asUInt64(r, 1).value_or(0);
        setUint(COL_MAP_ID,      mapId);
        setUint(COL_DIFFICULTY,  difficulty);
        setUint(COL_LEVEL_MIN,   res.asUInt64(r, 2).value_or(0));
        setUint(COL_LEVEL_MAX,   res.asUInt64(r, 3).value_or(0));
        setUint(COL_ITEM,        res.asUInt64(r, 4).value_or(0));
        setUint(COL_ITEM2,       res.asUInt64(r, 5).value_or(0));
        setUint(COL_QUEST_A,     res.asUInt64(r, 6).value_or(0));
        setUint(COL_QUEST_H,     res.asUInt64(r, 7).value_or(0));
        setUint(COL_ACHIEVEMENT, res.asUInt64(r, 8).value_or(0));

        auto* cmtCell = new QTableWidgetItem(QString::fromStdString(res.cell(r, 9)));
        m_table->setItem(int(r), COL_COMMENT, cmtCell);

        if (uint32_t(mapId) == prevMap && uint32_t(difficulty) == prevDiff
            && (prevMap != 0 || prevDiff != 0))
            restoreRow = int(r);
    }

    m_statusLabel->setText(tr("rows=%1 (capped at 5000)").arg(res.rowCount()));
    m_loading = false;
    if (restoreRow >= 0)
        m_table->selectRow(restoreRow);
    else
        onSelectionChanged();
}

bool AccessRequirementDialog::runInTransaction(QStringList const& sqls, QString const& description)
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

void AccessRequirementDialog::onAddClicked()
{
    openModal(false);
}

void AccessRequirementDialog::onEditClicked()
{
    uint32_t map = 0, diff = 0;
    int row = -1;
    if (!currentRowPk(map, diff, row)) return;
    openModal(true);
}

void AccessRequirementDialog::onRemoveClicked()
{
    uint32_t map = 0, diff = 0;
    int row = -1;
    if (!currentRowPk(map, diff, row)) return;

    auto const choice = QMessageBox::question(this, tr("Remove access requirement"),
        tr("Delete access_requirement WHERE mapId=%1 AND difficulty=%2?").arg(map).arg(diff),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.access_requirement WHERE mapId=%2 AND difficulty=%3")
        .arg(m_worldDb).arg(map).arg(diff);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE access_requirement (mapId=%1, difficulty=%2)").arg(map).arg(diff)))
        loadRows();
}

void AccessRequirementDialog::onLookupItemClicked()
{
    uint32_t map = 0, diff = 0;
    int row = -1;
    if (!currentRowPk(map, diff, row)) return;
    auto* itemCell = m_table->item(row, COL_ITEM);
    if (!itemCell)
    {
        m_statusLabel->setText(tr("No item cell on selected row."));
        return;
    }
    uint32_t const itemId = static_cast<uint32_t>(itemCell->data(Qt::DisplayRole).toULongLong());
    if (itemId == 0)
    {
        m_statusLabel->setText(tr("Selected row has item=0; nothing to look up."));
        return;
    }

    // MainWindow exposes m_itemDock privately (no public openItemInfoDock).
    // Best-effort: qDebug + status surface so the operator can copy/paste
    // the ID into the existing item search.  Matches the same pattern used
    // by NpcVendorDialog::onLookupItem.
    qDebug() << "AccessRequirementDialog::onLookupItem requested itemId=" << itemId
             << " (mapId=" << map << " difficulty=" << diff << ")";
    (void)m_mainWindow;
    m_statusLabel->setText(tr("Lookup requested for item %1 (see qDebug; no public dock API yet).")
        .arg(itemId));
}

void AccessRequirementDialog::openModal(bool editing)
{
    QDialog dlg(this);
    dlg.setWindowTitle(editing ? tr("Edit access requirement") : tr("Add access requirement"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto mkUSpin = [&](int hi) {
        auto* sp = new QSpinBox(&dlg);
        sp->setRange(0, hi);
        return sp;
    };
    auto* mapSpin   = mkUSpin(std::numeric_limits<int>::max());
    auto* diffSpin  = mkUSpin(255);
    auto* lminSpin  = mkUSpin(255);
    auto* lmaxSpin  = mkUSpin(255);
    auto* itemSpin  = mkUSpin(std::numeric_limits<int>::max());
    auto* item2Spin = mkUSpin(std::numeric_limits<int>::max());
    auto* qASpin    = mkUSpin(std::numeric_limits<int>::max());
    auto* qHSpin    = mkUSpin(std::numeric_limits<int>::max());
    auto* achvSpin  = mkUSpin(std::numeric_limits<int>::max());

    // PK fields are immutable on edit; the DELETE/INSERT pattern is left
    // to the operator (Remove + Add) when they need to rekey a gate.
    mapSpin ->setEnabled(!editing);
    diffSpin->setEnabled(!editing);

    auto* failTextEdit = new QLineEdit(&dlg);
    failTextEdit->setMaxLength(255);
    auto* commentEdit  = new QLineEdit(&dlg);

    form->addRow(tr("mapId:"),                 mapSpin);
    form->addRow(tr("difficulty:"),            diffSpin);
    form->addRow(tr("level_min:"),             lminSpin);
    form->addRow(tr("level_max:"),             lmaxSpin);
    form->addRow(tr("item:"),                  itemSpin);
    form->addRow(tr("item2:"),                 item2Spin);
    form->addRow(tr("quest_done_A:"),          qASpin);
    form->addRow(tr("quest_done_H:"),          qHSpin);
    form->addRow(tr("completed_achievement:"), achvSpin);
    form->addRow(tr("quest_failed_text:"),     failTextEdit);
    form->addRow(tr("comment:"),               commentEdit);

    uint32_t editingMap = 0, editingDiff = 0;
    if (editing)
    {
        int row = -1;
        if (currentRowPk(editingMap, editingDiff, row) && row >= 0)
        {
            auto readUint = [&](int col) -> uint32_t {
                auto* c = m_table->item(row, col);
                return c ? static_cast<uint32_t>(c->data(Qt::DisplayRole).toULongLong()) : 0;
            };
            mapSpin  ->setValue(int(editingMap));
            diffSpin ->setValue(int(editingDiff));
            lminSpin ->setValue(int(readUint(COL_LEVEL_MIN)));
            lmaxSpin ->setValue(int(readUint(COL_LEVEL_MAX)));
            itemSpin ->setValue(int(readUint(COL_ITEM)));
            item2Spin->setValue(int(readUint(COL_ITEM2)));
            qASpin   ->setValue(int(readUint(COL_QUEST_A)));
            qHSpin   ->setValue(int(readUint(COL_QUEST_H)));
            achvSpin ->setValue(int(readUint(COL_ACHIEVEMENT)));
            auto* cmt = m_table->item(row, COL_COMMENT);
            commentEdit->setText(cmt ? cmt->text() : QString());
            // quest_failed_text is not in the main grid; fetch it on demand.
            char qftSql[256];
            std::snprintf(qftSql, sizeof(qftSql),
                "SELECT COALESCE(quest_failed_text, '') FROM %s.access_requirement "
                "WHERE mapId=%u AND difficulty=%u",
                m_worldDb.toStdString().c_str(), editingMap, editingDiff);
            db::QueryResult qftRes;
            (void)m_db->query(qftSql, qftRes);
            if (qftRes.rowCount() > 0)
                failTextEdit->setText(QString::fromStdString(qftRes.cell(0, 0)));
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString const escFailText = QString::fromStdString(
        m_db->escapeString(failTextEdit->text().toStdString()));
    QString const escComment  = QString::fromStdString(
        m_db->escapeString(commentEdit ->text().toStdString()));
    uint32_t const mapId      = static_cast<uint32_t>(mapSpin  ->value());
    uint32_t const difficulty = static_cast<uint32_t>(diffSpin ->value());
    uint32_t const lmin       = static_cast<uint32_t>(lminSpin ->value());
    uint32_t const lmax       = static_cast<uint32_t>(lmaxSpin ->value());
    uint32_t const item       = static_cast<uint32_t>(itemSpin ->value());
    uint32_t const item2      = static_cast<uint32_t>(item2Spin->value());
    uint32_t const qA         = static_cast<uint32_t>(qASpin   ->value());
    uint32_t const qH         = static_cast<uint32_t>(qHSpin   ->value());
    uint32_t const achv       = static_cast<uint32_t>(achvSpin ->value());

    if (editing)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.access_requirement SET "
            "level_min=%2, level_max=%3, item=%4, item2=%5, "
            "quest_done_A=%6, quest_done_H=%7, completed_achievement=%8, "
            "quest_failed_text='%9', comment='%10' "
            "WHERE mapId=%11 AND difficulty=%12")
            .arg(m_worldDb)
            .arg(lmin).arg(lmax).arg(item).arg(item2)
            .arg(qA).arg(qH).arg(achv)
            .arg(escFailText).arg(escComment)
            .arg(editingMap).arg(editingDiff);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE access_requirement (mapId=%1, difficulty=%2)")
                    .arg(editingMap).arg(editingDiff)))
            loadRows();
    }
    else
    {
        // PK collision check: friendly QMessageBox beats a raw duplicate-key
        // error.  The INSERT would already fail server-side but the operator
        // wouldn't necessarily see why.
        char checkSql[256];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.access_requirement WHERE mapId=%u AND difficulty=%u",
            m_worldDb.toStdString().c_str(), mapId, difficulty);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add access requirement"),
                tr("access_requirement (mapId=%1, difficulty=%2) already exists.")
                    .arg(mapId).arg(difficulty));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.access_requirement "
            "(mapId, difficulty, level_min, level_max, item, item2, "
            " quest_done_A, quest_done_H, completed_achievement, "
            " quest_failed_text, comment) "
            "VALUES (%2, %3, %4, %5, %6, %7, %8, %9, %10, '%11', '%12')")
            .arg(m_worldDb)
            .arg(mapId).arg(difficulty).arg(lmin).arg(lmax)
            .arg(item).arg(item2).arg(qA).arg(qH).arg(achv)
            .arg(escFailText).arg(escComment);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT access_requirement (mapId=%1, difficulty=%2)")
                    .arg(mapId).arg(difficulty)))
            loadRows();
    }
}

} // namespace world_editor::app
