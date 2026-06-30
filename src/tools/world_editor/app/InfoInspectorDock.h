/*
 * InfoInspectorDock - unified right-pane host for the ~14 read-only
 * info panels that used to live as individual tabbed QDockWidgets.
 *
 * Replaces the visual mess of ~15 docks stacked / cascading on the right
 * side.  Each former dock's inner QWidget is reparented into a single
 * QStackedWidget here; a QComboBox at the top of the dock switches
 * between them.  Operator-initiated openItem(), openLootTable(),
 * openSpellInfo(), etc. methods route the request to the correct page
 * and switch the combo to that page so the panel is visible.
 *
 * The old dock classes (ItemInfoDock, LootTableDock, etc.) remain as
 * plain QWidget subclasses - they were already QWidgets, not
 * QDockWidget subclasses, so the migration is a reparenting move
 * rather than a content extraction.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QComboBox;
class QStackedWidget;

namespace world_editor::db { class MySqlClient; }
namespace world_editor::render { class NavMeshView; }

namespace world_editor::app
{

class ItemInfoDock;
class LootTableDock;
class QuestRewardDock;
class SpellInfoDock;
class FactionTemplateDock;
class AreaInfoDock;
class CurrencyTypeDock;
class PlayerConditionDock;
class GameObjectInfoDock;
class NpcTextDock;
class TrainerSpellDock;
class AreatriggerScriptDock;
class ZoneSummaryDock;
class MinimapDiagnosticsDock;

class InfoInspectorDock final : public QWidget
{
    Q_OBJECT

public:
    // The 14 distinct info panels surfaced through this dock.  Indices
    // line up 1:1 with the QComboBox + QStackedWidget pages.
    enum class Page : int
    {
        Item              = 0,
        Loot              = 1,
        QuestReward       = 2,
        Spell             = 3,
        Faction           = 4,
        Area              = 5,
        Currency          = 6,
        PlayerCondition   = 7,
        GameObjectInfo    = 8,
        NpcText           = 9,
        TrainerSpells     = 10,
        AreatriggerScript = 11,
        ZoneSummary       = 12,
        MinimapDiagnostics = 13,
        Count_
    };

    explicit InfoInspectorDock(db::MySqlClient* dbClient,
                               QWidget* parent = nullptr);

    // Switch every embedded dock's late-bound DB client in one shot.
    // Called from MainWindow after the world DB connects / disconnects.
    void setDbClient(db::MySqlClient* db);

    // Borrow pointer for the MinimapDiagnosticsDock's force-reload path.
    void setMinimapViewer(render::NavMeshView* viewer);

    // Accessors so MainWindow can wire signals on the individual pages
    // (itemSelected, currencySelected, refreshRequested, ...).
    ItemInfoDock*              itemDock()            const noexcept { return m_itemDock; }
    LootTableDock*             lootDock()            const noexcept { return m_lootDock; }
    QuestRewardDock*           questRewardDock()     const noexcept { return m_questRewardDock; }
    SpellInfoDock*             spellDock()           const noexcept { return m_spellDock; }
    FactionTemplateDock*       factionDock()         const noexcept { return m_factionDock; }
    AreaInfoDock*              areaDock()            const noexcept { return m_areaDock; }
    CurrencyTypeDock*          currencyDock()        const noexcept { return m_currencyDock; }
    PlayerConditionDock*       playerCondDock()      const noexcept { return m_playerCondDock; }
    GameObjectInfoDock*        goInfoDock()          const noexcept { return m_goInfoDock; }
    NpcTextDock*               npcTextDock()         const noexcept { return m_npcTextDock; }
    TrainerSpellDock*          trainerDock()         const noexcept { return m_trainerDock; }
    AreatriggerScriptDock*     atrScriptDock()       const noexcept { return m_atrScriptDock; }
    ZoneSummaryDock*           zoneSummaryDock()     const noexcept { return m_zoneSummaryDock; }
    MinimapDiagnosticsDock*    minimapDiagDock()     const noexcept { return m_minimapDiagDock; }

    // Switch to the named page programmatically.  Used by openItemInfo
    // and friends so the operator sees the panel they just populated.
    void showPage(Page page);

    // Convenience: switch + populate.  Each method drives the matching
    // page's set...() method and raises the page.  id=0 clears.
    void openItemInfo         (uint32_t itemId);
    void openCurrency         (uint32_t currencyId);
    void openSpellInfo        (uint32_t spellId);
    void openPlayerCondition  (uint32_t pcId);
    void openNpcText          (uint32_t npcTextId);

private:
    QComboBox*       m_combo  = nullptr;
    QStackedWidget*  m_stack  = nullptr;

    ItemInfoDock*           m_itemDock         = nullptr;
    LootTableDock*          m_lootDock         = nullptr;
    QuestRewardDock*        m_questRewardDock  = nullptr;
    SpellInfoDock*          m_spellDock        = nullptr;
    FactionTemplateDock*    m_factionDock      = nullptr;
    AreaInfoDock*           m_areaDock         = nullptr;
    CurrencyTypeDock*       m_currencyDock     = nullptr;
    PlayerConditionDock*    m_playerCondDock   = nullptr;
    GameObjectInfoDock*     m_goInfoDock       = nullptr;
    NpcTextDock*            m_npcTextDock      = nullptr;
    TrainerSpellDock*       m_trainerDock      = nullptr;
    AreatriggerScriptDock*  m_atrScriptDock    = nullptr;
    ZoneSummaryDock*        m_zoneSummaryDock  = nullptr;
    MinimapDiagnosticsDock* m_minimapDiagDock  = nullptr;
};

} // namespace world_editor::app
