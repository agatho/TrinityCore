/*
 * CreatureLootEditDialog - modal editor for `creature_loot_template`.
 *
 * Actual modern TC schema (see sql/base/dev/world_database.sql and
 * src/server/game/Loot/LootMgr.cpp):
 *
 *   creature_loot_template(Entry         INT UNSIGNED,
 *                          ItemType      TINYINT,        -- 0=Item,1=Ref,2=Currency,3=TrackingQuest
 *                          Item          INT UNSIGNED,
 *                          Chance        FLOAT          DEFAULT 100,
 *                          QuestRequired TINYINT        DEFAULT 0,
 *                          LootMode      SMALLINT UNSIGNED DEFAULT 1,
 *                          GroupId       TINYINT UNSIGNED DEFAULT 0,
 *                          MinCount      TINYINT UNSIGNED DEFAULT 1,
 *                          MaxCount      TINYINT UNSIGNED DEFAULT 1,
 *                          Comment       VARCHAR(255))
 *
 * Composite PK index = (Entry, ItemType, Item).  A creature_template.entry
 * can have many drop rows.  ItemType=1 means the row is a reference to a
 * reference_loot_template fan-out (Item then carries the referenced
 * Entry).  The task spec's "Reference INT UNSIGNED" column belongs to an
 * older 3.3.5-era schema; this editor targets the live modern schema.
 *
 * Layout:
 *   - Top filter row: Creature entry QSpinBox + Load button + read-only
 *     "<entry> - <name>" label populated via creature_template.name1
 *     (with `name` fallback for old schemas).
 *   - Main: QTableWidget over the drop rows for that Entry, ordered by
 *     GroupId, Chance DESC, Item.
 *   - Action row: Add drop / Edit drop / Remove drop / Lookup item template.
 *
 * Add/Edit modal carries every column except Entry (taken from the top
 * spinner) and is the same dialog for INSERT and UPDATE - the latter
 * just pre-populates from the selected row and emits an UPDATE WHERE
 * Entry=? AND ItemType=? AND Item=?.
 *
 * Lookup item template prints a qDebug() trace and updates the status
 * label.  MainWindow does NOT today expose a public openItemInfoDock()
 * helper, and the spec forbids introducing one; the LootTableDock's own
 * itemSelected route is wired up separately.
 *
 * All DML wraps START TRANSACTION / COMMIT / ROLLBACK so a failure
 * mid-statement never leaves the table in a half-written state.
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

class CreatureLootEditDialog final : public QDialog
{
    Q_OBJECT

public:
    CreatureLootEditDialog(db::MySqlClient* dbClient,
                           QString const& worldDbName,
                           QWidget* parent = nullptr);

private slots:
    void onLoad();
    void onAdd();
    void onEdit();
    void onRemove();
    void onLookupItem();
    void onSelectionChanged();

private:
    // Resolves the creature_template name (name1 first, name as fallback)
    // and updates the top label.  Empty string for missing rows.
    void refreshCreatureName(uint32_t entry);

    // Reload the drops table for the currently-loaded creature entry.
    void loadDrops();

    // Returns the selected (Item, ItemType) pair on the table; both are
    // needed because the composite PK is (Entry, ItemType, Item).
    // Surfaces the row index too.
    bool currentRowItem(uint32_t& itemOut, int& itemTypeOut, int& rowOut) const;

    // Run every entry in `sqls` inside one START TRANSACTION / COMMIT.
    // ROLLBACKs and surfaces a QMessageBox on any error path.
    bool runInTransaction(QStringList const& sqls, QString const& description);

    // Open the Add/Edit modal.  editingItem == UINT32_MAX means INSERT;
    // otherwise pre-populate from the selected row and UPDATE on Ok.
    void openModal(uint32_t editingItem);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;
    uint32_t         m_loadedEntry = 0;

    QSpinBox*        m_entrySpin    = nullptr;
    QPushButton*     m_loadBtn      = nullptr;
    QLabel*          m_creatureLbl  = nullptr;
    QTableWidget*    m_table        = nullptr;
    QPushButton*     m_addBtn       = nullptr;
    QPushButton*     m_editBtn      = nullptr;
    QPushButton*     m_removeBtn    = nullptr;
    QPushButton*     m_lookupBtn    = nullptr;
    QLabel*          m_statusLabel  = nullptr;

    bool             m_loading = false;
};

} // namespace world_editor::app
