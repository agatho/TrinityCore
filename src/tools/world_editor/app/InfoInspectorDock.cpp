#include "InfoInspectorDock.h"

#include "AreaInfoDock.h"
#include "AreatriggerScriptDock.h"
#include "CurrencyTypeDock.h"
#include "FactionTemplateDock.h"
#include "GameObjectInfoDock.h"
#include "ItemInfoDock.h"
#include "LootTableDock.h"
#include "MinimapDiagnosticsDock.h"
#include "NpcTextDock.h"
#include "PlayerConditionDock.h"
#include "QuestRewardDock.h"
#include "SpellInfoDock.h"
#include "TrainerSpellDock.h"
#include "ZoneSummaryDock.h"

#include <QComboBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace world_editor::app
{

InfoInspectorDock::InfoInspectorDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    m_combo = new QComboBox(this);
    m_combo->setToolTip(tr("Switch between read-only info panels."));
    // Order MUST match the InfoInspectorDock::Page enum below.
    m_combo->addItem(tr("Item template"),       int(Page::Item));
    m_combo->addItem(tr("Loot table"),          int(Page::Loot));
    m_combo->addItem(tr("Quest reward"),        int(Page::QuestReward));
    m_combo->addItem(tr("Spell info"),          int(Page::Spell));
    m_combo->addItem(tr("Faction template"),    int(Page::Faction));
    m_combo->addItem(tr("Area info"),           int(Page::Area));
    m_combo->addItem(tr("Currency type"),       int(Page::Currency));
    m_combo->addItem(tr("Player condition"),    int(Page::PlayerCondition));
    m_combo->addItem(tr("GameObject info"),     int(Page::GameObjectInfo));
    m_combo->addItem(tr("NPC text"),            int(Page::NpcText));
    m_combo->addItem(tr("Trainer spells"),      int(Page::TrainerSpells));
    m_combo->addItem(tr("Areatrigger script"),  int(Page::AreatriggerScript));
    m_combo->addItem(tr("Zone summary"),        int(Page::ZoneSummary));
    m_combo->addItem(tr("Minimap diagnostics"), int(Page::MinimapDiagnostics));
    root->addWidget(m_combo);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, /*stretch*/ 1);

    // Construct each page widget with the same late-bound DB client
    // pattern the standalone docks used; MainWindow re-pumps it via
    // setDbClient() once the connection is live.
    m_itemDock        = new ItemInfoDock         (dbClient, m_stack);
    m_lootDock        = new LootTableDock        (dbClient, m_stack);
    m_questRewardDock = new QuestRewardDock      (dbClient, m_stack);
    m_spellDock       = new SpellInfoDock        (dbClient, m_stack);
    m_factionDock     = new FactionTemplateDock  (dbClient, m_stack);
    m_areaDock        = new AreaInfoDock         (dbClient, m_stack);
    m_currencyDock    = new CurrencyTypeDock     (dbClient, m_stack);
    m_playerCondDock  = new PlayerConditionDock  (dbClient, m_stack);
    m_goInfoDock      = new GameObjectInfoDock   (dbClient, m_stack);
    m_npcTextDock     = new NpcTextDock          (dbClient, m_stack);
    m_trainerDock     = new TrainerSpellDock     (dbClient, m_stack);
    m_atrScriptDock   = new AreatriggerScriptDock(dbClient, m_stack);
    m_zoneSummaryDock = new ZoneSummaryDock      (dbClient, m_stack);
    m_minimapDiagDock = new MinimapDiagnosticsDock(m_stack);

    // Insertion order MUST match the Page enum / combo entries above so
    // currentIndex() == int(Page::Xxx) lines up.
    m_stack->insertWidget(int(Page::Item),               m_itemDock);
    m_stack->insertWidget(int(Page::Loot),               m_lootDock);
    m_stack->insertWidget(int(Page::QuestReward),        m_questRewardDock);
    m_stack->insertWidget(int(Page::Spell),              m_spellDock);
    m_stack->insertWidget(int(Page::Faction),            m_factionDock);
    m_stack->insertWidget(int(Page::Area),               m_areaDock);
    m_stack->insertWidget(int(Page::Currency),           m_currencyDock);
    m_stack->insertWidget(int(Page::PlayerCondition),    m_playerCondDock);
    m_stack->insertWidget(int(Page::GameObjectInfo),     m_goInfoDock);
    m_stack->insertWidget(int(Page::NpcText),            m_npcTextDock);
    m_stack->insertWidget(int(Page::TrainerSpells),      m_trainerDock);
    m_stack->insertWidget(int(Page::AreatriggerScript),  m_atrScriptDock);
    m_stack->insertWidget(int(Page::ZoneSummary),        m_zoneSummaryDock);
    m_stack->insertWidget(int(Page::MinimapDiagnostics), m_minimapDiagDock);

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx >= 0 && idx < m_stack->count())
            m_stack->setCurrentIndex(idx);
    });

    m_combo->setCurrentIndex(int(Page::Item));
    m_stack->setCurrentIndex(int(Page::Item));
}

void InfoInspectorDock::setDbClient(db::MySqlClient* db)
{
    if (m_itemDock)        m_itemDock->setDbClient(db);
    if (m_lootDock)        m_lootDock->setDbClient(db);
    if (m_questRewardDock) m_questRewardDock->setDbClient(db);
    if (m_spellDock)       m_spellDock->setDbClient(db);
    if (m_factionDock)     m_factionDock->setDbClient(db);
    if (m_areaDock)        m_areaDock->setDbClient(db);
    if (m_currencyDock)    m_currencyDock->setDbClient(db);
    if (m_playerCondDock)  m_playerCondDock->setDbClient(db);
    if (m_goInfoDock)      m_goInfoDock->setDbClient(db);
    if (m_npcTextDock)     m_npcTextDock->setDbClient(db);
    if (m_trainerDock)     m_trainerDock->setDbClient(db);
    if (m_atrScriptDock)   m_atrScriptDock->setDbClient(db);
    if (m_zoneSummaryDock) m_zoneSummaryDock->setDbClient(db);
    // MinimapDiagnosticsDock is DB-free.
}

void InfoInspectorDock::setMinimapViewer(render::NavMeshView* viewer)
{
    if (m_minimapDiagDock)
        m_minimapDiagDock->setViewer(viewer);
}

void InfoInspectorDock::showPage(Page page)
{
    int const idx = int(page);
    if (idx < 0 || idx >= int(Page::Count_))
        return;
    if (m_combo) m_combo->setCurrentIndex(idx);
    if (m_stack) m_stack->setCurrentIndex(idx);
}

void InfoInspectorDock::openItemInfo(uint32_t itemId)
{
    if (!m_itemDock) return;
    if (itemId == 0) m_itemDock->clear();
    else             m_itemDock->setItem(itemId);
    showPage(Page::Item);
}

void InfoInspectorDock::openCurrency(uint32_t currencyId)
{
    if (!m_currencyDock) return;
    if (currencyId == 0) m_currencyDock->clear();
    else                 m_currencyDock->setCurrency(currencyId);
    showPage(Page::Currency);
}

void InfoInspectorDock::openSpellInfo(uint32_t spellId)
{
    if (!m_spellDock) return;
    if (spellId == 0) m_spellDock->clear();
    else              m_spellDock->setSpell(spellId);
    showPage(Page::Spell);
}

void InfoInspectorDock::openPlayerCondition(uint32_t pcId)
{
    if (!m_playerCondDock) return;
    if (pcId == 0) m_playerCondDock->clear();
    else           m_playerCondDock->setPlayerConditionId(pcId);
    showPage(Page::PlayerCondition);
}

void InfoInspectorDock::openNpcText(uint32_t npcTextId)
{
    if (!m_npcTextDock) return;
    if (npcTextId == 0) m_npcTextDock->clear();
    else                m_npcTextDock->setNpcTextId(npcTextId);
    showPage(Page::NpcText);
}

} // namespace world_editor::app
