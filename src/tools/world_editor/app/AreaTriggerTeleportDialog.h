/*
 * AreaTriggerTeleportDialog - modal editor for `areatrigger_teleport`.
 *
 * TC's `areatrigger_teleport` table is a thin server-side join row:
 *
 *   areatrigger_teleport(ID         INT UNSIGNED PK,
 *                        PortLocID  INT UNSIGNED,
 *                        Name       MEDIUMTEXT)
 *
 * `PortLocID` references `world_safe_locs.ID` which carries the actual
 * (MapID, LocX, LocY, LocZ, Facing) destination tuple.  The client-side
 * trigger geometry lives in `AreaTrigger.db2` and is read-only here;
 * this dialog only edits the server-side teleport-destination join.
 *
 *   world_safe_locs(ID        INT UNSIGNED PK,
 *                   MapID     INT UNSIGNED,
 *                   LocX      FLOAT,
 *                   LocY      FLOAT,
 *                   LocZ      FLOAT,
 *                   Facing    FLOAT,
 *                   ...)
 *
 * Layout:
 *   - Top: QLineEdit search (substring on ID, Name, or PortLocID) +
 *          Refresh.
 *   - Main: QTableWidget of (ID, Name, PortLocID, target_map, X, Y, Z,
 *           Orientation), the last five JOINed in from world_safe_locs.
 *   - Buttons: Add / Edit / Remove / Jump to destination.
 *
 * Add/Edit modal: ID + Name + PortLocID + target_map + X/Y/Z +
 * Orientation.  The map+position fields write to world_safe_locs (the
 * teleport target row), with an in-tx INSERT-or-UPDATE upsert so that
 * editing a teleport stays atomic across both tables.
 *
 * Jump to destination emits `jumpRequested(mapId, x, y)` - MainWindow
 * connects this to its existing pan-to-XY handler (same shape used by
 * HealthReportDialog / FindJumpDialog).
 *
 * All DML wraps START TRANSACTION / COMMIT / ROLLBACK so a failure mid-
 * statement never leaves either table in a half-written state.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class AreaTriggerTeleportDialog final : public QDialog
{
    Q_OBJECT

public:
    AreaTriggerTeleportDialog(db::MySqlClient* dbClient,
                              QString const& worldDbName,
                              QWidget* parent = nullptr);

signals:
    // Operator clicked "Jump to destination" on a row.  MainWindow
    // forwards this to onJumpRequested(mapId, x, y, std::nullopt).
    void jumpRequested(uint32_t mapId, float worldX, float worldY);

private slots:
    void onSearchChanged(QString const& text);
    void onRefresh();
    void onAdd();
    void onEdit();
    void onRemove();
    void onJump();
    void onSelectionChanged();

private:
    void loadRows();

    // Returns the ID stored on the selected row, or false on no
    // selection.  Also surfaces the row index for table-cell lookups
    // (e.g. the Jump button reads target_map / X / Y straight from the
    // already-rendered cells).
    bool currentRowId(uint32_t& idOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // ROLLBACKs and surfaces a QMessageBox on any error path.  Returns
    // true on success.  Multi-statement is split here because the
    // underlying MySQL connection lacks CLIENT_MULTI_STATEMENTS, so
    // joining with `;` would fail on the first call.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  When editingId != UINT32_MAX, the modal
    // pre-populates from the current row and emits an UPDATE WHERE
    // ID=editingId; otherwise emits an INSERT with the modal's chosen
    // ID (uniqueness enforced server-side via PK).
    void openModal(uint32_t editingId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    QLineEdit*       m_searchEdit   = nullptr;
    QPushButton*     m_refreshBtn   = nullptr;
    QTableWidget*    m_table        = nullptr;
    QPushButton*     m_addBtn       = nullptr;
    QPushButton*     m_editBtn      = nullptr;
    QPushButton*     m_removeBtn    = nullptr;
    QPushButton*     m_jumpBtn      = nullptr;
    QLabel*          m_statusLabel  = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
