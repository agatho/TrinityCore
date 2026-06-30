/*
 * SecondaryLootTablesDialog - unified modal editor for the ten loot-template
 * tables that share `creature_loot_template`'s schema:
 *
 *   gameobject_loot_template, item_loot_template, skinning_loot_template,
 *   fishing_loot_template, pickpocketing_loot_template, disenchant_loot_template,
 *   milling_loot_template, prospecting_loot_template, reference_loot_template,
 *   mail_loot_template
 *
 * All ten use the columns (Entry, ItemType, Item, Chance, QuestRequired,
 * LootMode, GroupId, MinCount, MaxCount, Comment) with composite-key index
 * (Entry, ItemType, Item).  The meaning of `Entry` depends on the table:
 *   - gameobject_loot_template          -> gameobject_template.entry
 *   - item_loot_template                -> item_template.entry (container)
 *   - skinning_loot_template            -> creature_template.entry (skinnable)
 *   - fishing_loot_template             -> areaid (zone) entry
 *   - pickpocketing_loot_template       -> creature_template.entry
 *   - disenchant_loot_template          -> disenchant id (item_template column)
 *   - milling_loot_template             -> herb item entry
 *   - prospecting_loot_template         -> ore item entry
 *   - reference_loot_template           -> arbitrary reference id, fan-out by ItemType=1
 *   - mail_loot_template                -> mailLootId (item / quest reward)
 *
 * Layout mirrors CreatureLootEditDialog, with a QComboBox at the top to pick
 * one of the ten tables.  Table-name substitution is locked to the allow-list
 * defined in the .cpp (operator never types a table name).  All DML wraps
 * START TRANSACTION / COMMIT / ROLLBACK.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <cstdint>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class SecondaryLootTablesDialog final : public QDialog
{
    Q_OBJECT

public:
    SecondaryLootTablesDialog(db::MySqlClient* dbClient,
                              QString const& worldDbName,
                              QWidget* parent = nullptr);

private slots:
    void onTableChanged(int idx);
    void onLoad();
    void onAdd();
    void onEdit();
    void onRemove();
    void onSelectionChanged();

private:
    // Returns the currently-selected table name from the allow-listed combo.
    // Never returns an operator-supplied string - the combo is the only source
    // and is initialised once with the fixed ten entries.
    QString currentTable() const;

    // Refresh the drops table for the currently-loaded entry/table.
    void loadDrops();

    // Recover the (Item, ItemType) pair for the selected row.  Both belong
    // to the composite PK alongside Entry.
    bool currentRowItem(uint32_t& itemOut, int& itemTypeOut, int& rowOut) const;

    // Wrap every statement in `sqls` inside START TRANSACTION / COMMIT.
    // ROLLBACKs and pops a QMessageBox on any failure.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal (mirrors CreatureLootEditDialog::openModal).
    // editingItem == UINT32_MAX means INSERT; else pre-populate from the
    // selected row and emit an UPDATE WHERE composite-PK.
    void openModal(uint32_t editingItem);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;
    uint32_t         m_loadedEntry = 0;

    QComboBox*       m_tableCombo   = nullptr;
    QSpinBox*        m_entrySpin    = nullptr;
    QPushButton*     m_loadBtn      = nullptr;
    QLabel*          m_entryLbl     = nullptr;
    QTableWidget*    m_table        = nullptr;
    QPushButton*     m_addBtn       = nullptr;
    QPushButton*     m_editBtn      = nullptr;
    QPushButton*     m_removeBtn    = nullptr;
    QLabel*          m_statusLabel  = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
