#include "VendorInventoryDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace world_editor::app
{

VendorInventoryDock::VendorInventoryDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a vendor NPC to see its inventory."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        tr("slot"), tr("item"), tr("name"),
        tr("max"), tr("incr (s)"), tr("ExtCost"), tr("type") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // Column 1 holds the id (item or currency depending on type column 6).
    // Column 6 renders as "item" / "currency" / "<n>" (see prettyType()
    // in setVendorEntry).  Double-click routes to itemSelected for items
    // and currencySelected for currency so the right dock receives the
    // id - they reference different DBs (item_template vs CurrencyType.db2).
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int /*col*/) {
        if (row < 0) return;
        auto* idItem = m_table->item(row, 1);
        if (!idItem) return;
        bool okParse = false;
        uint32_t const id = idItem->text().toUInt(&okParse);
        if (!okParse || id == 0) return;
        auto* typeCell = m_table->item(row, 6);
        QString const typeText = typeCell ? typeCell->text() : QString();
        if (typeText == QStringLiteral("currency"))
            emit currencySelected(id);
        else
            emit itemSelected(id);
    });
}

void VendorInventoryDock::clear()
{
    m_table->setRowCount(0);
    m_header->setText(tr("Click a vendor NPC to see its inventory."));
}

void VendorInventoryDock::setVendorEntry(uint32_t entry)
{
    m_table->setRowCount(0);
    if (entry == 0)
    {
        m_header->setText(tr("Click a vendor NPC to see its inventory."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected."));
        return;
    }

    // Join against item_template via UNION of the common base + sparse
    // tables; many TC schemas split into item_template + item_sparse.
    // We just project the name field; missing rows render the id alone.
    QString const sql = QStringLiteral(
        "SELECT nv.slot, nv.item, nv.maxcount, nv.incrtime, nv.ExtendedCost, "
        "       nv.type, COALESCE(it.name, '') AS item_name "
        "FROM npc_vendor nv "
        "LEFT JOIN item_template it ON it.entry = nv.item "
        "WHERE nv.entry = %1 "
        "ORDER BY nv.slot, nv.item "
        "LIMIT 2000").arg(entry);

    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_header->setText(tr("Query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_header->setText(tr("Entry %1 has no npc_vendor rows.").arg(entry));
        return;
    }
    m_header->setText(tr("Inventory for entry %1 — %2 row(s)")
        .arg(entry).arg(res.rowCount()));

    // Helper: prettify the npc_vendor.incrtime column.  Stored as
    // seconds between restocks; 0 means "no restock".  We render it as
    // d/h/m so the human reader doesn't have to divide by 3600 in
    // their head.
    auto prettyDuration = [](uint64_t seconds) -> QString {
        if (seconds == 0) return QStringLiteral("never");
        if (seconds % 86400 == 0) return QStringLiteral("%1d").arg(seconds / 86400);
        if (seconds %  3600 == 0) return QStringLiteral("%1h").arg(seconds /  3600);
        if (seconds %    60 == 0) return QStringLiteral("%1m").arg(seconds /    60);
        return QStringLiteral("%1s").arg(seconds);
    };
    // npc_vendor.type values (TC ItemVendorType enum):
    //   1 = ITEM_VENDOR_TYPE_ITEM
    //   2 = ITEM_VENDOR_TYPE_CURRENCY
    auto prettyType = [](uint64_t t) -> QString {
        switch (t)
        {
            case 1: return QStringLiteral("item");
            case 2: return QStringLiteral("currency");
            default: return QStringLiteral("%1").arg(t);
        }
    };

    m_table->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        auto setCell = [this, r](int col, QString const& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(int(r), col, item);
        };
        uint64_t const maxcount     = res.asUInt64(r, 2).value_or(0);
        uint64_t const incrtime     = res.asUInt64(r, 3).value_or(0);
        uint64_t const extendedCost = res.asUInt64(r, 4).value_or(0);
        uint64_t const type         = res.asUInt64(r, 5).value_or(0);

        setCell(0, QString::number(res.asInt64 (r, 0).value_or(0)));
        setCell(1, QString::number(res.asUInt64(r, 1).value_or(0)));
        setCell(2, QString::fromStdString(res.cell(r, 6)));
        setCell(3, maxcount == 0
            ? QStringLiteral("unlimited")
            : QString::number(maxcount));
        setCell(4, prettyDuration(incrtime));
        // ExtendedCost references ItemExtendedCost.db2 which lives in
        // hotfixes, not the world DB.  We surface the raw id and a
        // hint when it's non-zero so the operator knows there's a
        // gating cost to look up.
        setCell(5, extendedCost == 0
            ? QStringLiteral("-")
            : QStringLiteral("#%1 (DB2 ItemExtendedCost)").arg(extendedCost));
        setCell(6, prettyType(type));
    }
}

} // namespace world_editor::app
