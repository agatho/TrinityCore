/*
 * LinkedRespawnDialog - modal editor for the `linked_respawn` table.
 *
 * Schema (live MySQL 9.4):
 *
 *   linked_respawn(guid       BIGINT UNSIGNED,    -- dependent spawn (creature/GO guid)
 *                  linkedGuid BIGINT UNSIGNED,    -- spawn that triggers it
 *                  linkType   TINYINT UNSIGNED)   -- 0=C->C 1=C->GO 2=GO->GO 3=GO->C
 *
 * Composite PK = (guid, linkType).  The same dependent guid can carry one
 * link per linkType, but never two with the same linkType.  TC interprets
 * each link as "when linkedGuid is killed, guid respawns" (or vice-versa
 * depending on the boss-script flavor wiring it up).
 *
 * The dialog presents the full table (LIMIT 5000) as a filterable
 * QTableWidget plus Add/Edit/Remove/Jump-to-spawn buttons.  All writes are
 * START TRANSACTION / COMMIT / ROLLBACK wrapped via the helper below.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class LinkedRespawnDialog final : public QDialog
{
    Q_OBJECT

public:
    LinkedRespawnDialog(db::MySqlClient* dbClient,
                        QString const& worldDbName,
                        QWidget* parent = nullptr);

signals:
    // Operator clicked "Jump to dependent spawn" with a row selected.
    // MainWindow connects this to its pan-to-XY handler.
    void jumpRequested(uint32_t mapId, float worldX, float worldY);

private slots:
    void onRefresh();
    void onFilterChanged();
    void onAdd();
    void onEdit();
    void onRemove();
    void onJump();
    void onSelectionChanged();

private:
    // Reload the table from MySQL, then apply the current filter substring.
    void loadRows();
    void applyFilter();

    // Read the selected row's PK pair (guid, linkType).  Returns false when
    // nothing is selected.
    bool currentRowKey(uint64_t& guidOut, uint32_t& linkTypeOut, uint64_t& linkedGuidOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // ROLLBACKs and surfaces a QMessageBox on any error path.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  When `editing` is true, pre-populate from
    // the supplied PK and UPDATE on Ok; otherwise INSERT.  The composite PK
    // pair (guid, linkType) is immutable on edit -- only linkedGuid moves.
    void openModal(bool editing,
                   uint64_t editingGuid,
                   uint32_t editingLinkType,
                   uint64_t editingLinkedGuid);

    // Friendly label for the linkType byte: "Creature -> Creature" etc.
    static QString linkTypeLabel(uint32_t linkType);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    QLineEdit*    m_filterEdit  = nullptr;
    QPushButton*  m_refreshBtn  = nullptr;
    QTableWidget* m_table       = nullptr;
    QPushButton*  m_addBtn      = nullptr;
    QPushButton*  m_editBtn     = nullptr;
    QPushButton*  m_removeBtn   = nullptr;
    QPushButton*  m_jumpBtn     = nullptr;
    QLabel*       m_statusLabel = nullptr;
};

} // namespace world_editor::app
