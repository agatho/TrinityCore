/*
 * QuestRewardDock - read-only panel showing the configured rewards for
 * a single quest_template row.  Joins item_template for item names when
 * the schema carries it.
 *
 * Auto-populates when MainWindow's onSpawnClicked finds a creature
 * spawn that appears in creature_queststarter or creature_questender;
 * the first matching quest id is displayed by default.  Empty state
 * when no quest-bearing NPC is selected.
 *
 * Schema tolerance: the TC trunk schema has every reward cluster
 * (items / choice items / faction / spell / mail).  Older world DBs
 * drop some columns.  Each cluster is queried independently and renders
 * "(column not present in this schema)" if MySQL returns 1054
 * ER_BAD_FIELD_ERROR.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class QuestRewardDock final : public QWidget
{
    Q_OBJECT

public:
    explicit QuestRewardDock(db::MySqlClient* dbClient,
                             QWidget* parent = nullptr);

    // Refresh the panel to show rewards for this quest_template.ID.
    // questId == 0 clears.
    void setQuest(uint32_t questId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Wipe the body layout (header is preserved).
    void resetBody();
    // Add a bold section header label to the body.
    void addSectionHeader(QString const& text);
    // Add a single text line to the body.
    void addBodyLine(QString const& text);
    // Render an item-id + count row, looking up the item name when
    // item_template is present.  itemName may be empty (id only).
    void addItemLine(uint64_t itemId, uint64_t count, QString const& itemName);

    db::MySqlClient* m_db          = nullptr;
    QLabel*          m_header      = nullptr;
    QScrollArea*     m_scroll      = nullptr;
    QWidget*         m_body        = nullptr;
    QVBoxLayout*     m_bodyLayout  = nullptr;
};

} // namespace world_editor::app
