/*
 * SmartScriptFlowDialog - tree view of every smart_scripts rule for a
 * given (entryorguid, source_type) and the linked-chain + action-list
 * dependencies that flow from each rule.
 *
 * Layout (top to bottom):
 *
 *   <entryorguid>  source_type=<n>
 *     EVENT  <event_type>(params)  -- chance <c>%, phaseMask <m>
 *       ACTION <action_type>(params)
 *         CALL action_list <id>          // if action is CALL_TIMED_ACTIONLIST
 *           ACTION ...                   // expanded action_list rules
 *       linked id=<L>                    // if `link` is non-zero
 *         EVENT (linked) ...
 *
 * The flow visualizer is read-only.  Double-click an event/action row
 * opens SmartScriptEditDialog via the existing edit-row pipeline.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>
#include <unordered_map>
#include <vector>

class QLineEdit;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

namespace world_editor::db { class MySqlClient; }
namespace world_editor::render { struct SmartScript; }

namespace world_editor::app
{

class SmartScriptFlowDialog final : public QDialog
{
    Q_OBJECT

public:
    SmartScriptFlowDialog(db::MySqlClient* dbClient,
                          int64_t initialEntryOrGuid,
                          uint8_t initialSourceType,
                          QWidget* parent = nullptr);

signals:
    // Operator double-clicked a rule.  The signal carries the rule's
    // composite PK so MainWindow can open SmartScriptEditDialog.
    void editRequested(qlonglong entryOrGuid, int sourceType, int id, int link);
    // Operator selected (single-click) a rule whose action references
    // a spell id (SMART_ACTION_CAST=11, SMART_ACTION_REMOVE_AURASFROMSPELL=28,
    // SMART_ACTION_ADD_AURA=75, SMART_ACTION_CROSS_CAST=86, ...).  The
    // SpellInfoDock subscribes to this and renders the spell summary.
    void spellReferenced(uint32_t spellId);

private slots:
    void onRescan();
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onItemSelectionChanged();

private:
    void buildTree(int64_t entryOrGuid, uint8_t sourceType);
    // Fetch every smart_scripts row matching the PK prefix
    // (entryorguid, source_type).  Returns rows sorted by id then link.
    [[nodiscard]] std::vector<render::SmartScript>
        fetchRulesFor(int64_t entryOrGuid, uint8_t sourceType);
    // Render one rule as a tree row hanging off `parent`.  Recurses
    // into the linked chain and any CALL_TIMED_ACTIONLIST target.
    void addRuleRow(QTreeWidgetItem* parent,
                    render::SmartScript const& rule,
                    int depth);
    // Format a single event / action / target for display.
    [[nodiscard]] QString formatEvent (render::SmartScript const& r) const;
    [[nodiscard]] QString formatAction(render::SmartScript const& r) const;
    [[nodiscard]] QString formatTarget(render::SmartScript const& r) const;

    db::MySqlClient* m_db;
    QSpinBox*        m_entryOrGuidSpin = nullptr;
    QSpinBox*        m_sourceTypeSpin  = nullptr;
    QTreeWidget*     m_tree            = nullptr;

    // Cache the rule set so the recursive renderer can lookup linked
    // rows (link field references an id in the SAME entryorguid).
    std::vector<render::SmartScript> m_rules;
    // For action-list expansion: lookup ruleset for an entryorguid with
    // source_type=9.  Lazy-populated; keyed by entryorguid.
    std::unordered_map<int64_t, std::vector<render::SmartScript>> m_actionListCache;

    // Guard against runaway recursion / cycles (a CALL_TIMED_ACTIONLIST
    // could in theory call itself; the linked field could loop too).
    static constexpr int kMaxDepth = 16;
};

} // namespace world_editor::app
