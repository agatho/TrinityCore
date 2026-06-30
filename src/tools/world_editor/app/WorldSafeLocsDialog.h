/*
 * WorldSafeLocsDialog - modal editor for `world_safe_locs`.
 *
 * world_safe_locs is the central table of named teleport destinations /
 * graveyard points used by TC. Referenced by:
 *   - areatrigger_teleport.PortLocID  (teleport destination)
 *   - graveyard_zone.ID                (graveyard for ghost resurrection)
 *   - instance_template                (entry/exit anchor)
 *
 * Editing one row here changes EVERY destination that points at this ID.
 *
 * Canonical schema (column tolerance: optional TransportSpawnId on some
 * forks, ignored here):
 *
 *   world_safe_locs(ID         INT UNSIGNED PRIMARY KEY,
 *                   MapID      INT UNSIGNED,
 *                   LocX       FLOAT,
 *                   LocY       FLOAT,
 *                   LocZ       FLOAT,
 *                   Facing     FLOAT,
 *                   Comment    VARCHAR(100))
 *
 * Layout:
 *   - Top: search QLineEdit (ID exact, MapID exact, or Comment substring)
 *          + Refresh button.
 *   - Main: QTableWidget (ID, MapID, LocX, LocY, LocZ, Facing, Comment)
 *          backed by SELECT ... ORDER BY ID LIMIT 5000.
 *   - Buttons: Add loc... / Edit loc / Remove loc / Jump to location /
 *              Show references.
 *
 * Add/Edit modal: QSpinBox ID + QSpinBox MapID + 4x QDoubleSpinBox + QLineEdit Comment.
 * Remove counts areatrigger_teleport.PortLocID references and warns.
 *
 * Jump to location emits jumpRequested(mapId, x, y) so MainWindow can
 * forward to onJumpRequested - same shape used by HealthReportDialog and
 * AreaTriggerTeleportDialog.
 *
 * Show references opens a small modal listing rows in areatrigger_teleport
 * (PortLocID=ID) and graveyard_zone (ID=ID).
 *
 * All DML wraps START TRANSACTION / COMMIT / ROLLBACK so a failure mid-
 * statement never leaves the table in a half-written state.
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

class WorldSafeLocsDialog final : public QDialog
{
    Q_OBJECT

public:
    WorldSafeLocsDialog(db::MySqlClient* dbClient,
                        QString const& worldDbName,
                        QWidget* parent = nullptr);

signals:
    // Operator clicked "Jump to location" on a row.  MainWindow forwards
    // this to onJumpRequested(mapId, x, y, std::nullopt).
    void jumpRequested(uint32_t mapId, float worldX, float worldY);

private slots:
    void onSearchChanged(QString const& text);
    void onRefresh();
    void onAdd();
    void onEdit();
    void onRemove();
    void onJump();
    void onShowRefs();
    void onSelectionChanged();

private:
    void loadRows();

    // Returns the ID stored on the selected row, or false on no selection.
    // Also surfaces the row index for table-cell lookups.
    bool currentRowId(uint32_t& idOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // Rolls back and surfaces a QMessageBox on any error path.  Returns
    // true on success.  Multi-statement is split here because the
    // underlying MySQL connection lacks CLIENT_MULTI_STATEMENTS.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  When editingId != UINT32_MAX, the modal
    // pre-populates from the current row and emits an UPDATE WHERE ID=...;
    // otherwise emits an INSERT with the modal's chosen ID.
    void openModal(uint32_t editingId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    QLineEdit*       m_searchEdit  = nullptr;
    QPushButton*     m_refreshBtn  = nullptr;
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_editBtn     = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QPushButton*     m_jumpBtn     = nullptr;
    QPushButton*     m_refsBtn     = nullptr;
    QLabel*          m_statusLabel = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
