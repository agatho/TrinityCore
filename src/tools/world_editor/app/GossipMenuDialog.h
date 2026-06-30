/*
 * GossipMenuDialog - walks the gossip_menu / gossip_menu_option tree
 * starting at a root MenuID and renders the entire reachable chain
 * as a QTreeWidget.
 *
 * Why a dedicated dialog: gossip chains in TC's world DB are graphs of
 * (menu -> options -> child menus) keyed only by integer ids.  Tracking
 * them by hand in MySQL Workbench is painful, especially when a single
 * NPC has dozens of branches.  This dialog probes table existence at
 * runtime so a fork without `gossip_menu_addon` still works, caches
 * visited menus to short-circuit cycles, and caps recursion at depth 8.
 *
 * Tree layout per node:
 *   Menu <id>: TextID <textId>
 *     Option <optId>: <text> (icon=<n>, type=<n>)
 *       (italic) BoxText: ...
 *       (italic) OptionBroadcastTextID: <id>
 *       (subtree) Menu <ActionMenuID> ...
 *
 * Double-click on any node that carries a MenuID emits menuSelected so
 * a caller (eg. MainWindow) can route the user elsewhere (npc_text
 * dock, find-spawn-by-gossip, etc).
 */

#pragma once

#include <QDialog>

#include <cstdint>
#include <unordered_set>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class GossipMenuDialog final : public QDialog
{
    Q_OBJECT

public:
    GossipMenuDialog(db::MySqlClient* db, uint32_t rootMenuId, QWidget* parent = nullptr);

signals:
    // Emitted when the operator double-clicks a tree node that carries a
    // menu id.  Caller can wire this up to jump to another lookup.
    void menuSelected(uint32_t menuId);

private slots:
    void onRefresh();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    // Hard cap on recursion depth so a self-referencing chain or an
    // accidentally-deep tree can't lock the UI.
    static constexpr int kMaxDepth = 8;

    // Run the full walk from m_rootMenuId, repopulating m_tree.
    void walk();

    // Probe-once table presence so a fork that lacks one of the gossip
    // tables fails gracefully instead of spamming "table not found".
    void probeSchema();

    // Recursive helper.  `parent` is where new tree rows attach; `visited`
    // is the per-walk cycle guard; depth limits recursion.
    void walkMenu(uint32_t menuId, QTreeWidgetItem* parent, int depth,
                  std::unordered_set<uint32_t>& visited);

    db::MySqlClient* m_db          = nullptr;
    uint32_t         m_rootMenuId  = 0;

    QTreeWidget*     m_tree        = nullptr;
    QLabel*          m_header      = nullptr;
    QLabel*          m_statusLbl   = nullptr;
    QPushButton*     m_refreshBtn  = nullptr;
    QPushButton*     m_closeBtn    = nullptr;

    // Schema probe results, populated once per dialog instance.
    bool m_haveGossipMenu        = false;
    bool m_haveGossipMenuOption  = false;
    bool m_haveGossipMenuAddon   = false;
    bool m_schemaProbed          = false;
};

} // namespace world_editor::app
