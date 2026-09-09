/*
 * CreatureTextEditDialog - modal editor for `creature_text`.
 *
 * TC schema:
 *
 *   creature_text(CreatureID      INT UNSIGNED,
 *                 GroupID         TINYINT UNSIGNED,
 *                 ID              TINYINT UNSIGNED,
 *                 Text            TEXT,
 *                 Type            TINYINT UNSIGNED,
 *                 Language        TINYINT UNSIGNED,
 *                 Probability     FLOAT,
 *                 Emote           INT UNSIGNED,
 *                 Duration        INT UNSIGNED,
 *                 Sound           INT UNSIGNED,
 *                 BroadcastTextId INT UNSIGNED DEFAULT 0,
 *                 TextRange       TINYINT UNSIGNED DEFAULT 0,
 *                 comment         TEXT)
 *
 * Composite PK = (CreatureID, GroupID, ID).  Multiple texts with the same
 * (CreatureID, GroupID) form a random-selection bucket (TC picks one based
 * on Probability when the script fires that GroupID).
 *
 * Layout:
 *   - Top filter row: Creature entry QSpinBox + Load + read-only
 *     "<entry> - <name>" label (creature_template.name1 / name fallback).
 *   - Main: QTableWidget over the text rows for that CreatureID, ordered
 *     by GroupID, ID.  Type column shows the friendly label (Say/Yell/
 *     TextEmote/Whisper/BossEmote/Unknown(N)).
 *   - Action row: Add text / Edit text / Remove text / New group.
 *     "New group" is a convenience: scans MAX(GroupID) for the loaded
 *     creature and INSERTs an empty probe row at GroupID = max+1, ID=0
 *     to seed a new random-selection bucket.
 *
 * The Add/Edit modal carries every column except CreatureID (the loaded
 * entry) and is the same dialog for INSERT and UPDATE - the latter just
 * pre-populates from the selected row and emits an UPDATE WHERE
 * CreatureID=? AND GroupID=? AND ID=?.  All DML wraps START TRANSACTION /
 * COMMIT / ROLLBACK so a failure mid-statement never leaves the table in
 * a half-written state.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class CreatureTextEditDialog final : public QDialog
{
    Q_OBJECT

public:
    CreatureTextEditDialog(db::MySqlClient* dbClient,
                           QString const& worldDbName,
                           QWidget* parent = nullptr);

private slots:
    void onLoad();
    void onAdd();
    void onEdit();
    void onRemove();
    void onNewGroup();
    void onSelectionChanged();

private:
    // Resolves the creature_template name (name1 first, then name) and
    // updates the top label.
    void refreshCreatureName(uint32_t entry);

    // Reload the texts table for the currently-loaded creature.
    void loadTexts();

    // Returns the selected (GroupID, ID) pair on the table.
    bool currentRowKey(uint32_t& groupOut, uint32_t& idOut, int& rowOut) const;

    // Returns the next free GroupID = MAX(GroupID)+1 for this creature,
    // or 0 if the creature currently has no rows.
    uint32_t nextFreeGroupId(uint32_t creatureId);

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // ROLLBACKs and surfaces a QMessageBox on any error path.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  When `editing` is true, pre-populate from
    // the supplied (group, id) row and UPDATE on Ok; otherwise INSERT.
    void openModal(bool editing, uint32_t editingGroup, uint32_t editingId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;
    uint32_t         m_loadedEntry = 0;

    QSpinBox*        m_entrySpin   = nullptr;
    QPushButton*     m_loadBtn     = nullptr;
    QLabel*          m_creatureLbl = nullptr;
    QTableWidget*    m_table       = nullptr;
    QPushButton*     m_addBtn      = nullptr;
    QPushButton*     m_editBtn     = nullptr;
    QPushButton*     m_removeBtn   = nullptr;
    QPushButton*     m_newGroupBtn = nullptr;
    QLabel*          m_statusLabel = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
