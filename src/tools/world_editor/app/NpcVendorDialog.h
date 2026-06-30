/*
 * NpcVendorDialog - modal editor for the `npc_vendor` table.
 *
 * Associates creature_template entries (vendor NPCs) with sellable items.
 * Each row optionally carries an ExtendedCost reference (DB2), per-slot
 * positioning, max-count + incrtime restock, BonusListIDs, PlayerConditionID
 * and IgnoreFiltering gating.
 *
 * Two-pane layout:
 *   Top    - filter row: creature_template.entry QSpinBox + Load button +
 *            read-only "<entry> - <name>" status label.
 *   Bottom - QTableWidget of vendor rows ordered by (slot, item) and a
 *            toolbar: Add item... / Remove item / Edit row / Lookup item
 *            template.
 *
 * Composite primary key on (entry, item, ExtendedCost, type) - all four
 * fields are part of every WHERE on Remove/Edit so a vendor can stock the
 * same item twice with different ExtendedCost or type values without the
 * editor confusing them.
 *
 * All DML goes through runInTransaction() (START TRANSACTION / COMMIT,
 * ROLLBACK on any error path).
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QLabel;
class QSpinBox;
class QPushButton;
class QTableWidget;

namespace world_editor { class MainWindow; }
namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class NpcVendorDialog final : public QDialog
{
    Q_OBJECT

public:
    NpcVendorDialog(db::MySqlClient* dbClient,
                    QString const& worldDbName,
                    world_editor::MainWindow* mainWindow,
                    QWidget* parent = nullptr);

private slots:
    void onLoadVendor();
    void onAddItem();
    void onRemoveItem();
    void onEditRow();
    void onLookupItem();
    void onSelectionChanged();

private:
    // Populate the vendor table for the currently set entry.  No-op when
    // the entry spin is 0 or the DB client is offline.
    void loadVendorRows();

    // Resolve creature_template.name1 for the current entry into m_headerLabel.
    void refreshHeader();

    // Returns the (item, ExtendedCost, type) tuple for the selected row, or
    // false when no row is selected / item id parse fails.
    bool currentRowKey(uint32_t& itemOut, uint32_t& extendedCostOut, uint32_t& typeOut) const;

    // Run `sql` inside START TRANSACTION / COMMIT.  Surfaces QMessageBox on
    // any error path and ROLLBACKs.  affectedOut may be null.
    bool runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut = nullptr);

    // Open the add/edit modal.  When editKey != nullptr the modal pre-populates
    // from the selected row and emits an UPDATE WHERE (entry,item,ExtCost,type);
    // when null it emits an INSERT.
    struct RowKey { uint32_t item; uint32_t extendedCost; uint32_t type; };
    void openItemModal(RowKey const* editKey);

    db::MySqlClient* m_db          = nullptr;
    QString          m_worldDb;
    world_editor::MainWindow* m_mainWindow = nullptr;

    // Top filter row.
    QSpinBox*        m_entrySpin   = nullptr;
    QPushButton*     m_loadBtn     = nullptr;
    QLabel*          m_headerLabel = nullptr;

    // Bottom pane.
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QPushButton*     m_editBtn     = nullptr;
    QPushButton*     m_lookupBtn   = nullptr;
    QLabel*          m_statusLabel = nullptr;

    uint32_t         m_loadedEntry = 0;
};

} // namespace world_editor::app
