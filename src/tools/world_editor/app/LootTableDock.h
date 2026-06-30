/*
 * LootTableDock - read-only panel showing the loot table rows for the
 * currently-selected creature or gameobject spawn.
 *
 * Creature lookup: creature_template_difficulty.LootID -> creature_loot_template.Entry.
 * GO lookup:       gameobject_template.Data1 -> gameobject_loot_template.Entry.
 *
 * Reference rows (Reference > 0) are recursively expanded one extra
 * level (cap 3) by reading from reference_loot_template; expanded rows
 * are annotated with `(ref %ref_id%)` in the reference column.
 *
 * Columns: item-id, name, chance (%), count (min..max), groupId,
 * mode (loot-mode bitmask hex), quest-only flag, reference id (or "-").
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class LootTableDock final : public QWidget
{
    Q_OBJECT

public:
    explicit LootTableDock(db::MySqlClient* dbClient,
                           QWidget* parent = nullptr);

    // Refresh the table for the LootID resolved from creature_template_difficulty.
    // entry=0 clears.
    void setCreatureEntry(uint32_t entry);
    // Refresh the table for the LootID resolved from gameobject_template.Data1.
    // entry=0 clears.
    void setGameObjectEntry(uint32_t entry);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

signals:
    // Operator double-clicked a row.  MainWindow forwards this to the
    // ItemInfoDock so the operator sees the full item_template record
    // without leaving the spawn-centric workflow.
    void itemSelected(uint32_t itemId);

private:
    // Common back-end: load and render rows from `lootTable` for the
    // given loot-template Entry id, with `kindLabel` shown in the header.
    void loadFromLootTable(char const* lootTable,
                           char const* refKindLabel,
                           uint32_t entry,
                           uint32_t lootId);

    db::MySqlClient* m_db;
    QLabel*          m_header = nullptr;
    QTableWidget*    m_table  = nullptr;
};

} // namespace world_editor::app
