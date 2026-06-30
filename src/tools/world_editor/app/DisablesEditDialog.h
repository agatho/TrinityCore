/*
 * DisablesEditDialog - modal editor for the `disables` table.
 *
 * The `disables` table is TC's runtime kill-switch table - one row per
 * disabled object identified by (sourceType, entry).  Server consults it
 * on load to suppress spells, quests, maps, BGs, achievement criteria,
 * outdoor PvP zones, vmap LOS / heights, game events, loot templates and
 * MMAP tiles.
 *
 * Composite PK is (sourceType, entry).
 *
 * Columns rendered (mapped to TC's Disables::DisableType enum):
 *   sourceType  - 0..9 with friendly labels (Spell / Quest / Map / ...).
 *   entry       - target id within the chosen sourceType.
 *   flags       - per-type bitmask (e.g. PVP_ONLY, HEROIC_ONLY for maps).
 *   params_0    - free-form auxiliary string (subset gating for criteria).
 *   params_1    - same.
 *   comment     - operator note.
 *
 * Older schemas may name the second column `entryID` instead of `entry`;
 * we probe INFORMATION_SCHEMA.COLUMNS once and adapt the SELECT / DML to
 * whichever name is present.
 *
 * Two-pane layout:
 *   Top    - filter row: sourceType QComboBox + Apply + Refresh.
 *   Bottom - QTableWidget of disable rows ordered by (sourceType, entry)
 *            and a toolbar: Add disable... / Edit row / Remove disable.
 *
 * All DML wrapped in START TRANSACTION / COMMIT, with ROLLBACK on any
 * error path.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class DisablesEditDialog final : public QDialog
{
    Q_OBJECT

public:
    DisablesEditDialog(db::MySqlClient* dbClient,
                       QString const& worldDbName,
                       QWidget* parent = nullptr);

private slots:
    void onApplyFilter();
    void onRefresh();
    void onAddDisable();
    void onEditRow();
    void onRemoveDisable();
    void onSelectionChanged();

private:
    // Re-run the SELECT (optionally filtered to a single sourceType when
    // the combo's current data is >= 0) and repopulate the table.
    void loadRows();

    // Returns the (sourceType, entry) tuple for the selected row, or false
    // if no row is selected / cell parse fails.
    bool currentRowKey(uint32_t& sourceTypeOut, uint32_t& entryOut) const;

    // Run `sql` inside START TRANSACTION / COMMIT.  Surfaces QMessageBox on
    // any error path and ROLLBACKs.  affectedOut may be null.
    bool runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut = nullptr);

    // Open the add/edit modal.  When editKey != nullptr the modal
    // pre-populates from the selected row and emits an UPDATE WHERE
    // (sourceType, entry); otherwise it emits an INSERT.
    struct RowKey { uint32_t sourceType; uint32_t entry; };
    void openRowModal(RowKey const* editKey);

    // Resolve "entry" vs "entryID" once per dialog instance.  Empty on
    // probe failure (table missing).
    QString resolveEntryColumn() const;

    db::MySqlClient* m_db          = nullptr;
    QString          m_worldDb;

    // Top filter row.
    QComboBox*       m_sourceTypeFilter = nullptr;
    QPushButton*     m_applyBtn         = nullptr;
    QPushButton*     m_refreshBtn       = nullptr;

    // Bottom pane.
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_editBtn     = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QLabel*          m_statusLabel = nullptr;

    // Cached real column name for the entry column ("entry" or "entryID").
    // Resolved lazily on first SELECT.
    mutable QString  m_entryCol;
};

} // namespace world_editor::app
