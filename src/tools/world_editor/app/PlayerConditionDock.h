/*
 * PlayerConditionDock - read-only panel showing core attributes of a
 * PlayerCondition.db2 row.
 *
 * PlayerCondition is referenced from many world-DB tables (conditions
 * ConditionType=56, npc_vendor PlayerConditionID, gossip_menu_option
 * PlayerConditionID, spawn_group, etc.) but the canonical data lives in
 * hotfix DBs.  Some forks / snapshots mirror it in the world DB as
 * `player_condition`, `player_condition_dbc`, or `playercondition`; we
 * probe each in priority order and surface whichever schema first yields
 * a row.  Column presence is checked against INFORMATION_SCHEMA.COLUMNS
 * so we can render whatever subset the local schema carries without
 * baking in a single fixed layout.
 *
 * Trigger path: the ConditionsDock emits playerConditionSelected(pcId)
 * whenever the operator double-clicks a row whose
 * ConditionTypeOrReference == 56 (CONDITION_PLAYER_CONDITION); ConditionValue1
 * carries the PlayerConditionID.  MainWindow forwards that id here.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QVBoxLayout;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class PlayerConditionDock final : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerConditionDock(db::MySqlClient* dbClient,
                                 QWidget* parent = nullptr);

    // Look up `pcId` and render its restriction sections.  id=0 clears.
    void setPlayerConditionId(uint32_t pcId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Try a SELECT against `table` projecting whichever PlayerCondition
    // columns are present in the local schema.  Returns true if the row
    // was found AND the dock has been populated.  Returns false on
    // no-row OR missing table (1146) / missing column (1054) so the
    // caller can fall through to the next probe.
    bool tryPopulateFromTable(uint32_t pcId,
                              char const* table,
                              QString& outNoteMissing);

    db::MySqlClient* m_db;
    QLabel*          m_header        = nullptr;
    // Each section renders independently; missing data leaves the
    // section empty rather than collapsing the whole panel.
    QLabel*          m_identity      = nullptr;
    QLabel*          m_restrictions  = nullptr;   // level / race / class / gender
    QLabel*          m_questGates    = nullptr;
    QLabel*          m_repGates      = nullptr;
    QLabel*          m_itemSpellAura = nullptr;
    QLabel*          m_achievements  = nullptr;
    QLabel*          m_areaZone      = nullptr;
    QLabel*          m_wseExpr       = nullptr;
    QLabel*          m_flags         = nullptr;
};

} // namespace world_editor::app
