/*
 * QuestGiverLinkageDialog - unified editor for the 4 quest-giver linkage
 * tables that all share the same (id, quest) composite-PK schema:
 *
 *   creature_queststarter   (id -> creature_template.entry,   quest -> quest_template.ID)
 *   creature_questender     (id -> creature_template.entry,   quest -> quest_template.ID)
 *   gameobject_queststarter (id -> gameobject_template.entry, quest -> quest_template.ID)
 *   gameobject_questender   (id -> gameobject_template.entry, quest -> quest_template.ID)
 *
 * One dialog covers all four because the schema + UX is identical; the
 * operator picks the table via a QComboBox.  The dropdown is the ONLY
 * source of the table-name token used in any SQL substitution -- there
 * is no user-typed free-form table identifier, so SQL injection through
 * the table-name slot is prevented by an explicit allowlist.
 *
 * Layout:
 *   - Top filter row: table-name combo + Filter-by-quest-ID spinbox
 *     (0 = ignore) + Filter-by-id spinbox (0 = ignore) + Refresh.
 *   - Main: QTableWidget (id, quest, friendly "id name"), ORDER BY id,
 *     quest LIMIT 5000.  The friendly-name column JOINs on
 *     creature_template.name or gameobject_template.name depending on
 *     which side the selected table is on.
 *   - Actions: Add link... / Remove link / Show quest info /
 *              Lookup quest-giver template.
 *
 * All DML wraps START TRANSACTION / COMMIT / ROLLBACK so a failure
 * mid-statement never leaves the table in a half-written state.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class QuestGiverLinkageDialog final : public QDialog
{
    Q_OBJECT

public:
    QuestGiverLinkageDialog(db::MySqlClient* dbClient,
                            QString const& worldDbName,
                            QWidget* parent = nullptr);

private slots:
    void onTableChanged(int index);
    void onRefresh();
    void onAdd();
    void onRemove();
    void onShowQuestInfo();
    void onLookupTemplate();
    void onSelectionChanged();

private:
    void loadRows();

    // Returns the (id, quest) of the currently selected row.  False on
    // no selection.
    bool currentRowKey(uint32_t& idOut, uint32_t& questOut) const;

    // Returns the dropdown's currently active table name.  This goes
    // through an internal allowlist so the value is always one of the
    // four canonical TC table names; never a user-typed string.
    QString selectedTable() const;

    // True when the selected table targets creature_template (false ->
    // gameobject_template).  Drives the join-target + lookup popups.
    bool isCreatureSide() const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // Rolls back and surfaces a QMessageBox on any error path.  Returns
    // true on success.  Multi-statement is split here because the
    // underlying MySQL connection lacks CLIENT_MULTI_STATEMENTS.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    QComboBox*       m_tableCombo  = nullptr;
    QSpinBox*        m_questFilter = nullptr;
    QSpinBox*        m_idFilter    = nullptr;
    QPushButton*     m_refreshBtn  = nullptr;
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QPushButton*     m_questInfoBtn = nullptr;
    QPushButton*     m_lookupBtn   = nullptr;
    QLabel*          m_statusLabel = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
