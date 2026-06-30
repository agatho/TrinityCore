#include "NpcVendorDialog.h"

#include "MainWindow.h"
#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
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

// Pretty-print the type column: 1=item, 2=currency, anything else as the raw
// integer.  Matches VendorInventoryDock prettyType() so the operator sees the
// same vocabulary in both panels.
QString prettyType(uint64_t t)
{
    switch (t)
    {
        case 1: return QStringLiteral("item");
        case 2: return QStringLiteral("currency");
        default: return QString::number(t);
    }
}

} // namespace

NpcVendorDialog::NpcVendorDialog(db::MySqlClient* dbClient,
                                 QString const& worldDbName,
                                 world_editor::MainWindow* mainWindow,
                                 QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName), m_mainWindow(mainWindow)
{
    setWindowTitle(tr("NPC vendor editor"));
    setModal(true);
    resize(1000, 620);

    auto* outer = new QVBoxLayout(this);

    // --- Top: filter row + header status label ------------------------
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Vendor (creature_template entry):"), this));
    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, std::numeric_limits<int>::max());
    m_entrySpin->setValue(0);
    filterRow->addWidget(m_entrySpin);
    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);
    filterRow->addStretch(1);
    outer->addLayout(filterRow);

    m_headerLabel = new QLabel(tr("(no vendor loaded)"), this);
    m_headerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(m_headerLabel);

    // --- Bottom: vendor rows + toolbar --------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        tr("slot"), tr("item"), tr("maxcount"), tr("incrtime"),
        tr("ExtendedCost"), tr("type"), tr("BonusListIDs"),
        tr("PlayerConditionID"), tr("IgnoreFiltering"), tr("VerifiedBuild") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    outer->addWidget(m_table, 1);

    auto* toolbar = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add item..."), this);
    m_removeBtn = new QPushButton(tr("Remove item"), this);
    m_editBtn   = new QPushButton(tr("Edit row"), this);
    m_lookupBtn = new QPushButton(tr("Lookup item template"), this);
    m_addBtn   ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_editBtn  ->setEnabled(false);
    m_lookupBtn->setEnabled(false);
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_removeBtn);
    toolbar->addWidget(m_editBtn);
    toolbar->addWidget(m_lookupBtn);
    toolbar->addStretch(1);
    outer->addLayout(toolbar);

    m_statusLabel = new QLabel(tr("Enter a creature_template entry and press Load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_loadBtn,   &QPushButton::clicked, this, &NpcVendorDialog::onLoadVendor);
    connect(m_addBtn,    &QPushButton::clicked, this, &NpcVendorDialog::onAddItem);
    connect(m_removeBtn, &QPushButton::clicked, this, &NpcVendorDialog::onRemoveItem);
    connect(m_editBtn,   &QPushButton::clicked, this, &NpcVendorDialog::onEditRow);
    connect(m_lookupBtn, &QPushButton::clicked, this, &NpcVendorDialog::onLookupItem);
    connect(m_table,     &QTableWidget::itemSelectionChanged,
            this, &NpcVendorDialog::onSelectionChanged);
}

void NpcVendorDialog::onLoadVendor()
{
    uint32_t const entry = uint32_t(m_entrySpin->value());
    if (entry == 0)
    {
        m_headerLabel->setText(tr("(no vendor loaded)"));
        m_table->setRowCount(0);
        m_loadedEntry = 0;
        m_addBtn->setEnabled(false);
        m_statusLabel->setText(tr("Enter a non-zero entry."));
        return;
    }
    m_loadedEntry = entry;
    refreshHeader();
    loadVendorRows();
    m_addBtn->setEnabled(m_db && m_db->isConnected());
}

void NpcVendorDialog::refreshHeader()
{
    if (!m_db || !m_db->isConnected())
    {
        m_headerLabel->setText(tr("DB not connected."));
        return;
    }
    // creature_template.name1 is the english name field on the modern TC
    // schema; older schemas used `name`.  Try name1 first, fall back to
    // name when the column is absent (older 3.3.5-style world DB).
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COALESCE(name1, '') FROM %s.creature_template WHERE entry=%u",
        m_worldDb.toStdString().c_str(), m_loadedEntry);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok())
    {
        // Retry with `name` column for older schemas.
        std::snprintf(sql, sizeof(sql),
            "SELECT COALESCE(name, '') FROM %s.creature_template WHERE entry=%u",
            m_worldDb.toStdString().c_str(), m_loadedEntry);
        err = m_db->query(sql, res);
    }
    if (!err.ok() || res.rowCount() == 0)
    {
        m_headerLabel->setText(tr("%1 - (unknown / no creature_template row)").arg(m_loadedEntry));
        return;
    }
    QString const name = QString::fromStdString(res.cell(0, 0));
    m_headerLabel->setText(tr("%1 - %2").arg(m_loadedEntry).arg(name));
}

void NpcVendorDialog::loadVendorRows()
{
    m_table->setRowCount(0);
    if (!m_db || !m_db->isConnected() || m_loadedEntry == 0)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT slot, item, maxcount, incrtime, ExtendedCost, type, "
        "       COALESCE(BonusListIDs, ''), PlayerConditionID, IgnoreFiltering, VerifiedBuild "
        "FROM %s.npc_vendor WHERE entry=%u ORDER BY slot, item",
        m_worldDb.toStdString().c_str(), m_loadedEntry);
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    m_table->setSortingEnabled(false);
    m_table->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int64_t  const slot         = res.asInt64 (r, 0).value_or(0);
        uint64_t const item         = res.asUInt64(r, 1).value_or(0);
        uint64_t const maxcount     = res.asUInt64(r, 2).value_or(0);
        uint64_t const incrtime     = res.asUInt64(r, 3).value_or(0);
        uint64_t const extendedCost = res.asUInt64(r, 4).value_or(0);
        uint64_t const type         = res.asUInt64(r, 5).value_or(0);
        QString  const bonusIds     = QString::fromStdString(res.cell(r, 6));
        uint64_t const playerCond   = res.asUInt64(r, 7).value_or(0);
        uint64_t const ignoreFilter = res.asUInt64(r, 8).value_or(0);
        uint64_t const verifiedBld  = res.asUInt64(r, 9).value_or(0);

        auto setNumeric = [&](int col, int64_t v) {
            auto* it = new QTableWidgetItem;
            it->setData(Qt::DisplayRole, qlonglong(v));
            m_table->setItem(int(r), col, it);
        };
        setNumeric(0, slot);
        setNumeric(1, int64_t(item));
        setNumeric(2, int64_t(maxcount));
        setNumeric(3, int64_t(incrtime));
        setNumeric(4, int64_t(extendedCost));
        m_table->setItem(int(r), 5, new QTableWidgetItem(prettyType(type)));
        m_table->setItem(int(r), 6, new QTableWidgetItem(bonusIds));
        setNumeric(7, int64_t(playerCond));
        setNumeric(8, int64_t(ignoreFilter));
        setNumeric(9, int64_t(verifiedBld));

        // Stash the raw type integer on column 5 (Qt::UserRole) so the
        // composite-PK lookup doesn't have to reverse-parse prettyType().
        m_table->item(int(r), 5)->setData(Qt::UserRole, qulonglong(type));
    }
    m_table->setSortingEnabled(true);
    m_statusLabel->setText(tr("rows=%1").arg(res.rowCount()));
}

bool NpcVendorDialog::currentRowKey(uint32_t& itemOut, uint32_t& extendedCostOut, uint32_t& typeOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* itemCell = m_table->item(row, 1);
    auto* extCell  = m_table->item(row, 4);
    auto* typeCell = m_table->item(row, 5);
    if (!itemCell || !extCell || !typeCell) return false;
    itemOut         = uint32_t(itemCell->data(Qt::DisplayRole).toLongLong());
    extendedCostOut = uint32_t(extCell ->data(Qt::DisplayRole).toLongLong());
    typeOut         = uint32_t(typeCell->data(Qt::UserRole).toULongLong());
    return true;
}

void NpcVendorDialog::onSelectionChanged()
{
    bool const hasSelection = m_table->currentRow() >= 0;
    bool const connected    = m_db && m_db->isConnected();
    m_removeBtn->setEnabled(hasSelection && connected);
    m_editBtn  ->setEnabled(hasSelection && connected);
    m_lookupBtn->setEnabled(hasSelection);
}

bool NpcVendorDialog::runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut)
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

void NpcVendorDialog::onAddItem()
{
    if (m_loadedEntry == 0)
    {
        QMessageBox::information(this, tr("Add item"), tr("Load a vendor first."));
        return;
    }
    openItemModal(/*editKey=*/nullptr);
}

void NpcVendorDialog::onEditRow()
{
    RowKey key{};
    if (!currentRowKey(key.item, key.extendedCost, key.type))
        return;
    openItemModal(&key);
}

void NpcVendorDialog::onRemoveItem()
{
    if (m_loadedEntry == 0) return;
    uint32_t item = 0, extCost = 0, type = 0;
    if (!currentRowKey(item, extCost, type))
        return;
    if (QMessageBox::question(this, tr("Remove vendor item"),
            tr("Delete npc_vendor row entry=%1 item=%2 ExtendedCost=%3 type=%4?")
                .arg(m_loadedEntry).arg(item).arg(extCost).arg(type),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Composite PK: entry+item+ExtendedCost+type uniquely identifies the row.
    char del[384];
    std::snprintf(del, sizeof(del),
        "DELETE FROM %s.npc_vendor "
        "WHERE entry=%u AND item=%u AND ExtendedCost=%u AND type=%u",
        m_worldDb.toStdString().c_str(), m_loadedEntry, item, extCost, type);
    if (runInTransaction(QString::fromUtf8(del),
            tr("DELETE npc_vendor (entry=%1, item=%2)").arg(m_loadedEntry).arg(item)))
        loadVendorRows();
}

void NpcVendorDialog::onLookupItem()
{
    int const row = m_table->currentRow();
    if (row < 0) return;
    auto* itemCell = m_table->item(row, 1);
    if (!itemCell) return;
    uint32_t const itemId = uint32_t(itemCell->data(Qt::DisplayRole).toLongLong());
    if (itemId == 0)
    {
        m_statusLabel->setText(tr("No item id on selected row."));
        return;
    }
    // MainWindow does not expose a public openItemInfoDock() helper today.
    // Surface the request via qDebug + status bar so the operator can
    // copy/paste into the existing item search.  Adding a new API was
    // explicitly out of scope.
    qDebug() << "NpcVendorDialog::onLookupItem requested itemId=" << itemId
             << " (vendor entry=" << m_loadedEntry << ")";
    (void)m_mainWindow; // reserved for future direct dock routing
    m_statusLabel->setText(tr("Lookup requested for item %1 (see qDebug; no public dock API yet).").arg(itemId));
}

void NpcVendorDialog::openItemModal(RowKey const* editKey)
{
    // Local modal for INSERT / UPDATE.  Kept inline because the field set
    // is small (5 fields) and dedicating a separate file/class would just
    // be ceremony.
    QDialog dlg(this);
    dlg.setWindowTitle(editKey ? tr("Edit vendor item") : tr("Add vendor item"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* itemSpin = new QSpinBox(&dlg);
    itemSpin->setRange(0, std::numeric_limits<int>::max());
    auto* slotSpin = new QSpinBox(&dlg);
    slotSpin->setRange(0, std::numeric_limits<int>::max());
    auto* maxcountSpin = new QSpinBox(&dlg);
    maxcountSpin->setRange(0, std::numeric_limits<int>::max());
    auto* incrtimeSpin = new QSpinBox(&dlg);
    incrtimeSpin->setRange(0, std::numeric_limits<int>::max());
    auto* extCostSpin = new QSpinBox(&dlg);
    extCostSpin->setRange(0, std::numeric_limits<int>::max());
    auto* typeSpin = new QSpinBox(&dlg);
    typeSpin->setRange(0, 255);
    typeSpin->setValue(1); // ITEM_VENDOR_TYPE_ITEM by default

    form->addRow(tr("item entry:"), itemSpin);
    form->addRow(tr("slot:"), slotSpin);
    form->addRow(tr("maxcount (0=unlimited):"), maxcountSpin);
    form->addRow(tr("incrtime (s, 0=no restock):"), incrtimeSpin);
    form->addRow(tr("ExtendedCost:"), extCostSpin);
    form->addRow(tr("type (1=item, 2=currency):"), typeSpin);

    if (editKey)
    {
        // Pre-populate from selected row's table cells (read-only key
        // fields are still editable in the modal, but the WHERE clause
        // below uses the ORIGINAL editKey so a key-field edit becomes a
        // delete-and-recreate UPDATE-friendly update).
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            itemSpin    ->setValue(int(m_table->item(row, 1)->data(Qt::DisplayRole).toLongLong()));
            slotSpin    ->setValue(int(m_table->item(row, 0)->data(Qt::DisplayRole).toLongLong()));
            maxcountSpin->setValue(int(m_table->item(row, 2)->data(Qt::DisplayRole).toLongLong()));
            incrtimeSpin->setValue(int(m_table->item(row, 3)->data(Qt::DisplayRole).toLongLong()));
            extCostSpin ->setValue(int(m_table->item(row, 4)->data(Qt::DisplayRole).toLongLong()));
            typeSpin    ->setValue(int(m_table->item(row, 5)->data(Qt::UserRole).toULongLong()));
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const itemId   = uint32_t(itemSpin->value());
    int64_t  const slot     = int64_t(slotSpin->value());
    uint64_t const maxcount = uint64_t(maxcountSpin->value());
    uint64_t const incrtime = uint64_t(incrtimeSpin->value());
    uint32_t const extCost  = uint32_t(extCostSpin->value());
    uint32_t const type     = uint32_t(typeSpin->value());

    if (itemId == 0)
    {
        QMessageBox::warning(this, tr("Invalid item"), tr("item entry must be non-zero."));
        return;
    }

    if (editKey)
    {
        char upd[768];
        std::snprintf(upd, sizeof(upd),
            "UPDATE %s.npc_vendor SET slot=%lld, item=%u, maxcount=%llu, incrtime=%llu, "
            "ExtendedCost=%u, type=%u "
            "WHERE entry=%u AND item=%u AND ExtendedCost=%u AND type=%u",
            m_worldDb.toStdString().c_str(),
            (long long)slot, itemId, (unsigned long long)maxcount, (unsigned long long)incrtime,
            extCost, type,
            m_loadedEntry, editKey->item, editKey->extendedCost, editKey->type);
        if (runInTransaction(QString::fromUtf8(upd),
                tr("UPDATE npc_vendor (entry=%1, item=%2)").arg(m_loadedEntry).arg(editKey->item)))
            loadVendorRows();
    }
    else
    {
        char ins[768];
        std::snprintf(ins, sizeof(ins),
            "INSERT INTO %s.npc_vendor "
            "(entry, slot, item, maxcount, incrtime, ExtendedCost, type) "
            "VALUES (%u, %lld, %u, %llu, %llu, %u, %u)",
            m_worldDb.toStdString().c_str(),
            m_loadedEntry, (long long)slot, itemId,
            (unsigned long long)maxcount, (unsigned long long)incrtime,
            extCost, type);
        if (runInTransaction(QString::fromUtf8(ins),
                tr("INSERT npc_vendor (entry=%1, item=%2)").arg(m_loadedEntry).arg(itemId)))
            loadVendorRows();
    }
}

} // namespace world_editor::app
