/*
 * VendorInventoryDock - read-only panel showing the npc_vendor inventory
 * of the currently-selected creature spawn (entry-keyed).
 *
 * Auto-populates when MainWindow's onSpawnClicked finds a creature
 * spawn whose template carries any UNIT_NPC_FLAG_VENDOR* bit.  Empty
 * when no vendor is selected.
 *
 * Columns: slot, item (id + name), maxcount, incrtime (s), ExtendedCost,
 * type, BonusListIDs.  Item name comes from item_template; missing item
 * templates render the id alone so the operator notices the gap.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class VendorInventoryDock final : public QWidget
{
    Q_OBJECT

public:
    explicit VendorInventoryDock(db::MySqlClient* dbClient,
                                 QWidget* parent = nullptr);

    // Refresh the table to show npc_vendor rows for this creature
    // template entry.  entry=0 clears.
    void setVendorEntry(uint32_t entry);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

signals:
    // Operator double-clicked a row whose `type` column == 1 (item).
    // MainWindow forwards this to the ItemInfoDock so the full
    // item_template record opens beside the vendor inventory.
    void itemSelected(uint32_t itemId);
    // Operator double-clicked a row whose `type` column == 2 (currency).
    // The id is the same `nv.item` column - it just references
    // CurrencyType.db2 instead of item_template.  MainWindow forwards
    // this to the CurrencyTypeDock.
    void currencySelected(uint32_t currencyId);

private:
    db::MySqlClient* m_db;
    QLabel*          m_header = nullptr;
    QTableWidget*    m_table  = nullptr;
};

} // namespace world_editor::app
