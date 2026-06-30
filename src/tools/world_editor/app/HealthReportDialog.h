/*
 * HealthReportDialog - read-only validation sweep of the world DB.
 *
 * Walks several "should never happen" data invariants and surfaces the
 * offenders in a categorized tree.  Each finding row carries a
 * (mapId, x, y) tuple where applicable; double-clicking it asks
 * MainWindow to pan the viewer to that position.
 *
 * Checks performed (all keyed to the currently-loaded map when one is
 * loaded; otherwise the whole DB):
 *
 *   1. Orphaned smart_scripts - entryorguid < 0 references a non-
 *      existing creature.guid (or > 0 references a non-existing
 *      creature_template.entry / gameobject_template.entry depending on
 *      source_type).
 *   2. Paths with < 2 nodes  - waypoint_path rows whose waypoint_path_node
 *      child list is empty or singular.
 *   3. Areatriggers without create-properties row in
 *      areatrigger_create_properties.
 *   4. Graveyards whose (map, x, y) does not fall inside any
 *      graveyard_zone link AND has no zone reference.
 *   5. Transports referenced by gameobject row whose entry has
 *      no gameobject_template match.
 *   6. smart_scripts rows with action_type=80 (CALL_TIMED_ACTIONLIST)
 *      whose action_param1 doesn't resolve to a source_type=9
 *      action_list entryorguid.
 *   7. smart_scripts rows with link != 0 whose link doesn't resolve
 *      to a peer rule (same entryorguid + source_type, id == link).
 *   8. source_type=9 action_list rows that no action_type=80 caller
 *      references anywhere (dead SAI lists).
 *   9. Quests whose NextQuestInChain points at a missing quest_template row.
 *  10. Quests whose PrevQuestID (signed) resolves to a missing quest_template row.
 *  11. Quests with a creature_queststarter row but no creature_questender or
 *      gameobject_questender row (excluding auto-complete Method=1 quests).
 *
 * The dialog is intentionally read-only - committing fixes (delete the
 * orphan, repair the missing parent, etc.) is left to the operator; we
 * just surface what's broken.  This keeps the safety surface small.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>
#include <optional>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class HealthReportDialog final : public QDialog
{
    Q_OBJECT

public:
    HealthReportDialog(db::MySqlClient* dbClient,
                       std::optional<uint32_t> currentMapId,
                       QWidget* parent = nullptr);

signals:
    // Operator double-clicked a finding that carries a world location.
    // MainWindow connects this to its pan-to-XY handler.
    void jumpRequested(uint32_t mapId, float worldX, float worldY);

private slots:
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onRefresh();

private:
    void runScan();
    // Each scan category bundles its scan SQL with the SQL needed to
    // remove every offender it found.  The dialog renders a per-category
    // "Fix" button mounted on the category row that runs the delete
    // SQL inside a single transaction.
    struct Category
    {
        QString          title;
        QString          deleteSql;   // empty = not auto-fixable
        int              findingCount = 0;
        QTreeWidgetItem* item         = nullptr;
    };
    QTreeWidgetItem* addCategory(Category& cat);
    void addFinding(QTreeWidgetItem* category,
                    QString const& summary,
                    std::optional<uint32_t> mapId  = std::nullopt,
                    std::optional<float>    worldX = std::nullopt,
                    std::optional<float>    worldY = std::nullopt);
    // Attach a "Fix N rows" button to a category's row.  Clicked when
    // the operator confirms; runs the delete SQL in a transaction and
    // re-scans.  No-op when deleteSql is empty.
    void attachFixButton(Category& cat);
    void runFix(Category const& cat);

    db::MySqlClient*         m_db;
    std::optional<uint32_t>  m_currentMapId;
    QTreeWidget*             m_tree    = nullptr;
    QPushButton*             m_refresh = nullptr;
    QPushButton*             m_close   = nullptr;
};

} // namespace world_editor::app
