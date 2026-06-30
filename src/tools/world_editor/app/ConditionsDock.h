/*
 * ConditionsDock - editable panel showing `conditions` table rows that
 * apply to the currently-selected entity.
 *
 * Originally read-only; now backed by a persistent ConditionsModel that
 * is the source of truth for the active scope.  Each setXxxScope() call
 * re-queries the live DB into the model's baseline.  Operator edits
 * (Insert / Edit / Delete) mutate the model in place; "Commit..."
 * opens a ConditionCommitDialog that diffs the model against baseline
 * and writes the changeset inside a transaction.
 *
 * Source-type IDs come from TC's ConditionMgr (ConditionSourceType
 * enum).  The dock auto-scopes on:
 *
 *   18 CONDITION_SOURCE_TYPE_SMART_EVENT
 *      SourceGroup=event id, SourceEntry=entryorguid, SourceId=source_type
 *   19 CONDITION_SOURCE_TYPE_NPC_VENDOR
 *      SourceGroup=creature_entry, SourceEntry=item_entry
 *   22 CONDITION_SOURCE_TYPE_AREATRIGGER
 *      SourceGroup=create_properties_id
 *   23 CONDITION_SOURCE_TYPE_AREATRIGGER_CLIENT_TRIGGERED
 *      SourceGroup=areatrigger_id
 *
 * Operators can extend the matched sources later; the dock's setScope
 * API is generic.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QString>
#include <QWidget>

#include <cstdint>

class QLabel;
class QPushButton;
class QTableWidget;

namespace world_editor::db { class ConditionsModel; class MySqlClient; }

namespace world_editor::app
{

class UndoManager;

class ConditionsDock final : public QWidget
{
    Q_OBJECT

public:
    explicit ConditionsDock(QWidget* parent = nullptr);

    void setDbClient(db::MySqlClient* db) { m_db = db; }
    void setModel(db::ConditionsModel* model) { m_model = model; }
    void setUndoManager(UndoManager* undo)    { m_undo  = undo; }

    // Show every conditions row matching a creature/GO spawn template.
    // entry > 0, kind = 0 (creature) or 1 (gameobject).  Pulls
    // conditions where (SourceEntry == entry) OR (SourceGroup == entry)
    // across the source types that mention NPC entries (gossip, vendor,
    // spell click, areatrigger).
    void setSpawnScope(uint32_t entry, uint8_t kind);

    // Show conditions targeting a specific smart_scripts rule.
    void setSmartScriptScope(int64_t entryOrGuid, uint16_t id, uint8_t sourceType);

    // Show conditions targeting an areatrigger create_properties row.
    void setAreatriggerScope(uint32_t createPropsId);

    // Clear and reset the scope label.
    void clear();

    // Re-read the current scope from the DB into the model baseline.
    // Called by MainWindow after commit / external schema reload.
    void refreshFromDb();

signals:
    // Emitted whenever the model changes shape (insert/edit/delete or
    // post-commit refresh).  MainWindow uses this to nudge the
    // diagnostics dock.
    void modelChanged();

    // Emitted when the operator double-clicks a row whose
    // ConditionTypeOrReference == 56 (CONDITION_PLAYER_CONDITION); the
    // payload is ConditionValue1 (the PlayerConditionID).  MainWindow
    // routes this into the PlayerConditionDock.
    void playerConditionSelected(uint32_t pcId);

private slots:
    void onInsertClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onCommitClicked();
    void onRevertClicked();
    void onSelectionChanged();
    // Row double-click hook; fires playerConditionSelected when the
    // clicked row's ConditionType == 56.
    void onRowDoubleClicked(int row, int column);

private:
    // Default-construct a Condition pre-filled with whatever PK columns
    // the active scope pins.  Used to seed the Insert dialog.
    [[nodiscard]] render::Condition scopeDefaults() const;

    void runQuery();
    void rebuildTable();
    void updateFooter();

    enum class ScopeKind : uint8_t
    {
        None, Spawn, SmartScript, Areatrigger
    };

    ScopeKind m_scopeKind = ScopeKind::None;
    // Captured per-scope key columns.  Used both for the live SELECT
    // and as defaults for Insert.
    uint32_t  m_spawnEntry        = 0;
    uint8_t   m_spawnKind         = 0;
    int64_t   m_saiEntryOrGuid    = 0;
    uint16_t  m_saiId             = 0;
    uint8_t   m_saiSourceType     = 0;
    uint32_t  m_areatriggerProps  = 0;

    db::MySqlClient*       m_db    = nullptr;
    db::ConditionsModel*   m_model = nullptr;
    UndoManager*           m_undo  = nullptr;

    QLabel*          m_header = nullptr;
    QTableWidget*    m_table  = nullptr;
    QLabel*          m_pendingLabel = nullptr;
    QPushButton*     m_insertButton = nullptr;
    QPushButton*     m_editButton   = nullptr;
    QPushButton*     m_deleteButton = nullptr;
    QPushButton*     m_revertButton = nullptr;
    QPushButton*     m_commitButton = nullptr;
};

} // namespace world_editor::app
