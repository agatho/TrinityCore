#include "CreatureEquipTemplateDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
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

// Column indices for the main equip-set table.  Single source of truth so
// the Add/Edit modal can read the selected row's cells off the rendered
// view without re-querying MySQL.
enum Col : int
{
    COL_ID                = 0,
    COL_ITEMID1           = 1,
    COL_APPEARANCEMODID1  = 2,
    COL_ITEMVISUAL1       = 3,
    COL_ITEMID2           = 4,
    COL_APPEARANCEMODID2  = 5,
    COL_ITEMVISUAL2       = 6,
    COL_ITEMID3           = 7,
    COL_APPEARANCEMODID3  = 8,
    COL_ITEMVISUAL3       = 9,
    COL_COUNT             = 10,
};

} // namespace

CreatureEquipTemplateDialog::CreatureEquipTemplateDialog(db::MySqlClient* dbClient,
                                                         QString const& worldDbName,
                                                         QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Creature equipment editor"));
    setModal(true);
    resize(1100, 560);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row: creature entry spinner + Load + name label ----
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Creature entry:"), this));
    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, std::numeric_limits<int>::max());
    m_entrySpin->setValue(0);
    filterRow->addWidget(m_entrySpin);
    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);
    m_creatureLbl = new QLabel(tr("(no creature loaded)"), this);
    m_creatureLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filterRow->addWidget(m_creatureLbl, 1);
    outer->addLayout(filterRow);

    // -- Main equip-sets table ---------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("ID"),
        tr("ItemID1 (MH)"), tr("AppearanceModID1"), tr("ItemVisual1"),
        tr("ItemID2 (OH)"), tr("AppearanceModID2"), tr("ItemVisual2"),
        tr("ItemID3 (Rng)"), tr("AppearanceModID3"), tr("ItemVisual3") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // ORDER BY ID is the canonical view.
    outer->addWidget(m_table, 1);

    // -- Action buttons ----------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add equip set..."), this);
    m_editBtn   = new QPushButton(tr("Edit equip set"), this);
    m_removeBtn = new QPushButton(tr("Remove equip set"), this);
    m_lookupBtn = new QPushButton(tr("Lookup item template"), this);
    m_addBtn   ->setEnabled(false);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_lookupBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_lookupBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Enter a creature entry and press Load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_loadBtn,   &QPushButton::clicked, this, &CreatureEquipTemplateDialog::onLoad);
    connect(m_addBtn,    &QPushButton::clicked, this, &CreatureEquipTemplateDialog::onAdd);
    connect(m_editBtn,   &QPushButton::clicked, this, &CreatureEquipTemplateDialog::onEdit);
    connect(m_removeBtn, &QPushButton::clicked, this, &CreatureEquipTemplateDialog::onRemove);
    connect(m_lookupBtn, &QPushButton::clicked, this, &CreatureEquipTemplateDialog::onLookupItem);
    connect(m_table,     &QTableWidget::itemSelectionChanged,
            this, &CreatureEquipTemplateDialog::onSelectionChanged);
}

void CreatureEquipTemplateDialog::detectCreatureIdColumn()
{
    if (m_schemaDetected) return;
    m_schemaDetected = true;
    if (!m_db || !m_db->isConnected()) return;

    // Probe INFORMATION_SCHEMA for the actual creature-id column name in
    // this schema.  Modern TC ships `CreatureID`; legacy 3.3.5 forks ship
    // `entry`.  We pick whichever exists; if both are present we prefer
    // the modern name.
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='creature_equip_template'",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0) return;
    bool hasCreatureId = false;
    bool hasEntry      = false;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        QString const c = QString::fromStdString(res.cell(r, 0));
        if (c.compare("CreatureID", Qt::CaseInsensitive) == 0) hasCreatureId = true;
        if (c.compare("entry",      Qt::CaseInsensitive) == 0) hasEntry      = true;
    }
    if (hasCreatureId)      m_creatureIdCol = QStringLiteral("CreatureID");
    else if (hasEntry)      m_creatureIdCol = QStringLiteral("entry");
}

void CreatureEquipTemplateDialog::onLoad()
{
    detectCreatureIdColumn();
    uint32_t const entry = static_cast<uint32_t>(m_entrySpin->value());
    m_loadedEntry = entry;
    refreshCreatureName(entry);
    loadEquipSets();
    bool const hasCreature = (entry != 0);
    m_addBtn->setEnabled(hasCreature);
    onSelectionChanged();
}

void CreatureEquipTemplateDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0 && m_loadedEntry != 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_lookupBtn->setEnabled(has);
}

bool CreatureEquipTemplateDialog::currentRowKey(uint32_t& idOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* idCell = m_table->item(row, COL_ID);
    if (!idCell) return false;
    idOut  = static_cast<uint32_t>(idCell->data(Qt::DisplayRole).toULongLong());
    rowOut = row;
    return true;
}

void CreatureEquipTemplateDialog::refreshCreatureName(uint32_t entry)
{
    if (!m_db || !m_db->isConnected())
    {
        m_creatureLbl->setText(tr("DB not connected."));
        return;
    }
    if (entry == 0)
    {
        m_creatureLbl->setText(tr("(no creature loaded)"));
        return;
    }

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COALESCE(name1, '') FROM %s.creature_template WHERE entry=%u",
        m_worldDb.toStdString().c_str(), entry);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok())
    {
        std::snprintf(sql, sizeof(sql),
            "SELECT COALESCE(name, '') FROM %s.creature_template WHERE entry=%u",
            m_worldDb.toStdString().c_str(), entry);
        err = m_db->query(sql, res);
    }
    if (!err.ok() || res.rowCount() == 0)
    {
        m_creatureLbl->setText(tr("%1 - (unknown / no creature_template row)").arg(entry));
        return;
    }
    QString const name = QString::fromStdString(res.cell(0, 0));
    m_creatureLbl->setText(tr("%1 - %2").arg(entry).arg(name));
}

void CreatureEquipTemplateDialog::loadEquipSets()
{
    m_loading = true;
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }
    if (m_loadedEntry == 0)
    {
        m_statusLabel->setText(tr("No creature loaded."));
        m_loading = false;
        return;
    }

    QString const sql = QStringLiteral(
        "SELECT ID, ItemID1, AppearanceModID1, ItemVisual1, "
        "       ItemID2, AppearanceModID2, ItemVisual2, "
        "       ItemID3, AppearanceModID3, ItemVisual3 "
        "FROM %1.creature_equip_template "
        "WHERE %2=%3 "
        "ORDER BY ID")
        .arg(m_worldDb).arg(m_creatureIdCol).arg(m_loadedEntry);

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql.toStdString(), res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("creature_equip_template query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        auto setU = [&](int col, uint64_t v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, qulonglong(v));
            m_table->setItem(int(r), col, c);
        };
        setU(COL_ID,               res.asUInt64(r, 0).value_or(0));
        setU(COL_ITEMID1,          res.asUInt64(r, 1).value_or(0));
        setU(COL_APPEARANCEMODID1, res.asUInt64(r, 2).value_or(0));
        setU(COL_ITEMVISUAL1,      res.asUInt64(r, 3).value_or(0));
        setU(COL_ITEMID2,          res.asUInt64(r, 4).value_or(0));
        setU(COL_APPEARANCEMODID2, res.asUInt64(r, 5).value_or(0));
        setU(COL_ITEMVISUAL2,      res.asUInt64(r, 6).value_or(0));
        setU(COL_ITEMID3,          res.asUInt64(r, 7).value_or(0));
        setU(COL_APPEARANCEMODID3, res.asUInt64(r, 8).value_or(0));
        setU(COL_ITEMVISUAL3,      res.asUInt64(r, 9).value_or(0));
    }

    m_statusLabel->setText(tr("CreatureID=%1 rows=%2").arg(m_loadedEntry).arg(res.rowCount()));
    m_loading = false;
    onSelectionChanged();
}

bool CreatureEquipTemplateDialog::runInTransaction(QStringList const& sqls, QString const& description)
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

void CreatureEquipTemplateDialog::onAdd()
{
    openModal(false, 0);
}

void CreatureEquipTemplateDialog::onEdit()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowKey(id, row)) return;
    openModal(true, id);
}

void CreatureEquipTemplateDialog::onRemove()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowKey(id, row)) return;

    auto const choice = QMessageBox::question(this, tr("Remove equip set"),
        tr("Delete creature_equip_template row %1=%2, ID=%3?")
            .arg(m_creatureIdCol).arg(m_loadedEntry).arg(id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.creature_equip_template WHERE %2=%3 AND ID=%4")
        .arg(m_worldDb).arg(m_creatureIdCol).arg(m_loadedEntry).arg(id);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE creature_equip_template (%1=%2, ID=%3)")
                .arg(m_creatureIdCol).arg(m_loadedEntry).arg(id)))
        loadEquipSets();
}

void CreatureEquipTemplateDialog::onLookupItem()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowKey(id, row)) return;
    auto* mhCell = m_table->item(row, COL_ITEMID1);
    if (!mhCell) return;
    uint32_t const itemId = static_cast<uint32_t>(mhCell->data(Qt::DisplayRole).toULongLong());
    if (itemId == 0)
    {
        m_statusLabel->setText(tr("Selected equip set has no ItemID1 to look up."));
        return;
    }
    // Best-effort: MainWindow does not expose a public openItemInfoDock().
    // Surface the request via qDebug + status label so the operator knows
    // which ItemID to plug into the existing ItemInfoDock manually.
    qDebug() << "CreatureEquipTemplateDialog: lookup ItemID1=" << itemId;
    m_statusLabel->setText(tr("Lookup requested for ItemID1=%1 (paste into ItemInfoDock).").arg(itemId));
}

void CreatureEquipTemplateDialog::openModal(bool editing, uint32_t editingId)
{
    QDialog dlg(this);
    dlg.setWindowTitle(editing ? tr("Edit equip set") : tr("Add equip set"));
    dlg.setModal(true);
    dlg.resize(520, 460);

    auto* outerL = new QVBoxLayout(&dlg);

    auto* idForm = new QFormLayout;
    auto* idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, 255);
    idSpin->setEnabled(!editing);          // composite PK ID is immutable post-INSERT.
    idForm->addRow(tr("ID:"), idSpin);
    outerL->addLayout(idForm);

    // Three grouped slot sections (Main Hand / Off Hand / Ranged) - each
    // carries ItemID + AppearanceModID + ItemVisual.  Layout mirrors the
    // SQL column order so the wire-up below is straight-line.
    struct SlotInputs { QSpinBox* item; QSpinBox* appear; QSpinBox* visual; };
    auto makeSlot = [&](char const* title) -> SlotInputs {
        auto* box  = new QGroupBox(tr(title), &dlg);
        auto* form = new QFormLayout(box);
        auto* itemSpin   = new QSpinBox(box);
        auto* appearSpin = new QSpinBox(box);
        auto* visSpin    = new QSpinBox(box);
        itemSpin  ->setRange(0, std::numeric_limits<int>::max());
        appearSpin->setRange(0, 65535);    // SMALLINT UNSIGNED
        visSpin   ->setRange(0, 65535);    // SMALLINT UNSIGNED
        form->addRow(tr("ItemID:"),          itemSpin);
        form->addRow(tr("AppearanceModID:"), appearSpin);
        form->addRow(tr("ItemVisual:"),      visSpin);
        outerL->addWidget(box);
        return { itemSpin, appearSpin, visSpin };
    };
    SlotInputs mh  = makeSlot("Main Hand (slot 1)");
    SlotInputs oh  = makeSlot("Off Hand (slot 2)");
    SlotInputs rng = makeSlot("Ranged (slot 3)");

    if (editing)
    {
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            auto getU = [&](int col) -> uint32_t {
                auto* c = m_table->item(row, col);
                return c ? static_cast<uint32_t>(c->data(Qt::DisplayRole).toULongLong()) : 0u;
            };
            idSpin    ->setValue(static_cast<int>(editingId));
            mh .item  ->setValue(static_cast<int>(getU(COL_ITEMID1)));
            mh .appear->setValue(static_cast<int>(getU(COL_APPEARANCEMODID1)));
            mh .visual->setValue(static_cast<int>(getU(COL_ITEMVISUAL1)));
            oh .item  ->setValue(static_cast<int>(getU(COL_ITEMID2)));
            oh .appear->setValue(static_cast<int>(getU(COL_APPEARANCEMODID2)));
            oh .visual->setValue(static_cast<int>(getU(COL_ITEMVISUAL2)));
            rng.item  ->setValue(static_cast<int>(getU(COL_ITEMID3)));
            rng.appear->setValue(static_cast<int>(getU(COL_APPEARANCEMODID3)));
            rng.visual->setValue(static_cast<int>(getU(COL_ITEMVISUAL3)));
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    outerL->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const rowId     = static_cast<uint32_t>(idSpin->value());
    uint32_t const itemId1   = static_cast<uint32_t>(mh .item  ->value());
    uint32_t const appear1   = static_cast<uint32_t>(mh .appear->value());
    uint32_t const visual1   = static_cast<uint32_t>(mh .visual->value());
    uint32_t const itemId2   = static_cast<uint32_t>(oh .item  ->value());
    uint32_t const appear2   = static_cast<uint32_t>(oh .appear->value());
    uint32_t const visual2   = static_cast<uint32_t>(oh .visual->value());
    uint32_t const itemId3   = static_cast<uint32_t>(rng.item  ->value());
    uint32_t const appear3   = static_cast<uint32_t>(rng.appear->value());
    uint32_t const visual3   = static_cast<uint32_t>(rng.visual->value());

    if (editing)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.creature_equip_template SET "
            "ItemID1=%2, AppearanceModID1=%3, ItemVisual1=%4, "
            "ItemID2=%5, AppearanceModID2=%6, ItemVisual2=%7, "
            "ItemID3=%8, AppearanceModID3=%9, ItemVisual3=%10 "
            "WHERE %11=%12 AND ID=%13")
            .arg(m_worldDb)
            .arg(itemId1).arg(appear1).arg(visual1)
            .arg(itemId2).arg(appear2).arg(visual2)
            .arg(itemId3).arg(appear3).arg(visual3)
            .arg(m_creatureIdCol).arg(m_loadedEntry).arg(editingId);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE creature_equip_template (%1=%2, ID=%3)")
                    .arg(m_creatureIdCol).arg(m_loadedEntry).arg(editingId)))
            loadEquipSets();
    }
    else
    {
        // PK collision check: friendly QMessageBox beats a raw duplicate-key
        // error from MySQL.
        QString const checkSql = QStringLiteral(
            "SELECT COUNT(*) FROM %1.creature_equip_template WHERE %2=%3 AND ID=%4")
            .arg(m_worldDb).arg(m_creatureIdCol).arg(m_loadedEntry).arg(rowId);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql.toStdString(), cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add equip set"),
                tr("creature_equip_template row already exists for %1=%2, ID=%3.")
                    .arg(m_creatureIdCol).arg(m_loadedEntry).arg(rowId));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.creature_equip_template "
            "(%2, ID, ItemID1, AppearanceModID1, ItemVisual1, "
            " ItemID2, AppearanceModID2, ItemVisual2, "
            " ItemID3, AppearanceModID3, ItemVisual3) "
            "VALUES (%3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13)")
            .arg(m_worldDb).arg(m_creatureIdCol)
            .arg(m_loadedEntry).arg(rowId)
            .arg(itemId1).arg(appear1).arg(visual1)
            .arg(itemId2).arg(appear2).arg(visual2)
            .arg(itemId3).arg(appear3).arg(visual3);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT creature_equip_template (%1=%2, ID=%3)")
                    .arg(m_creatureIdCol).arg(m_loadedEntry).arg(rowId)))
            loadEquipSets();
    }
}

} // namespace world_editor::app
