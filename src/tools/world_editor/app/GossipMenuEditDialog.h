/*
 * GossipMenuEditDialog - modal editor for the `gossip_menu` family.
 *
 * Three TC world tables are touched:
 *   gossip_menu(MenuID, TextID, VerifiedBuild)
 *     - one row per (MenuID, TextID) pair; MenuID alone is NOT unique.
 *   gossip_menu_option(MenuID, OptionID, OptionNpc, OptionText,
 *                      OptionBroadcastTextID, Language, ActionMenuID,
 *                      ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney,
 *                      BoxText, BoxBroadcastTextID, SpellID,
 *                      OverrideIconID, VerifiedBuild)
 *     - option rows under a menu; composite PK (MenuID, OptionID).
 *   npc_text(ID, BroadcastTextID0..7, Probability0..7, VerifiedBuild)
 *     - keyed by gossip_menu.TextID.
 *
 * The existing read-only GossipMenuDialog walks the menu/option graph as
 * a tree.  This editor lives alongside it and concentrates on direct
 * row-level INSERT / UPDATE / DELETE without re-implementing the walker.
 *
 * Layout:
 *   Left   - search QLineEdit + QListWidget of `gossip_menu` rows rendered
 *            as `"<MenuID>/<TextID>"`, sorted.
 *   Right  - QTabWidget with three tabs:
 *              Header   - read-only MenuID/TextID label + VerifiedBuild
 *                         QSpinBox + Save.
 *              Options  - QTableWidget over gossip_menu_option for the
 *                         selected MenuID (OptionID, OptionNpc, OptionText,
 *                         ActionMenuID, ActionPoiID, BoxMoney, SpellID) +
 *                         Add option / Edit option / Remove option.
 *              NPC text - read-only 8-row QTableWidget showing the npc_text
 *                         row keyed by gossip_menu.TextID
 *                         ((BroadcastTextID<i>, Probability<i>)) + an
 *                         "Edit NPC text..." button that opens an inline
 *                         modal updating all 16 fields.
 *
 * Top toolbar (under list): New menu / Delete menu / Refresh.
 *
 * All DML wrapped in START TRANSACTION / COMMIT / ROLLBACK.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class GossipMenuEditDialog final : public QDialog
{
    Q_OBJECT

public:
    GossipMenuEditDialog(db::MySqlClient* dbClient,
                         QString const& worldDbName,
                         QWidget* parent = nullptr);

private slots:
    void onMenuSearchChanged(QString const& text);
    void onMenuSelectionChanged();
    void onSaveHeader();
    void onNewMenu();
    void onDeleteMenu();
    void onRefresh();
    void onAddOption();
    void onEditOption();
    void onRemoveOption();
    void onEditNpcText();

private:
    // Reload left QListWidget from gossip_menu.  Honours the search box.
    void loadMenus();

    // Refresh all three right-pane tabs for the selected (MenuID, TextID).
    // Pass menuId==0 to clear all right-pane widgets.
    void loadMenu(uint32_t menuId, uint32_t textId);

    // Returns true and writes the (MenuID, TextID) of the selected QListWidget
    // item; false when no selection.
    bool selectedKey(uint32_t& menuIdOut, uint32_t& textIdOut) const;

    // Returns the OptionID of the currently-selected options-table row, or
    // false when no selection.
    bool currentOptionId(uint32_t& optionIdOut) const;

    // Run `sql` inside START TRANSACTION / COMMIT.  Surfaces QMessageBox on
    // any error path and ROLLBACKs.  affectedOut may be null.
    bool runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut = nullptr);

    // Open the option Add/Edit modal.  When editOptionId != UINT32_MAX the
    // modal pre-populates from the currently-selected option row and emits
    // an UPDATE WHERE (MenuID, OptionID); otherwise it emits an INSERT.
    void openOptionModal(uint32_t editOptionId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    // Left pane.
    QLineEdit*       m_searchEdit    = nullptr;
    QListWidget*     m_menuList      = nullptr;
    QPushButton*     m_newMenuBtn    = nullptr;
    QPushButton*     m_deleteMenuBtn = nullptr;
    QPushButton*     m_refreshBtn    = nullptr;

    // Right - Header tab.
    QTabWidget*      m_tabs            = nullptr;
    QLabel*          m_headerKeyLabel  = nullptr;
    QSpinBox*        m_verifiedBuildSpin = nullptr;
    QPushButton*     m_saveHeaderBtn   = nullptr;

    // Right - Options tab.
    QTableWidget*    m_optionTable    = nullptr;
    QPushButton*     m_addOptionBtn   = nullptr;
    QPushButton*     m_editOptionBtn  = nullptr;
    QPushButton*     m_removeOptionBtn = nullptr;

    // Right - NPC text tab.
    QTableWidget*    m_npcTextTable   = nullptr;
    QPushButton*     m_editNpcTextBtn = nullptr;
    QLabel*          m_npcTextStatus  = nullptr;

    QLabel*          m_statusLabel    = nullptr;

    // Guard against recursive selection events while we repopulate widgets.
    bool             m_loading = false;
};

} // namespace world_editor::app
