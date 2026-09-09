#include "SmartScriptFlowDialog.h"

#include "SmartScriptEnumTables.h"
#include "../db/MySqlClient.h"
#include "../render/NavMeshView.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
constexpr int kRoleEntryOrGuid = Qt::UserRole + 0;
constexpr int kRoleSourceType  = Qt::UserRole + 1;
constexpr int kRoleId          = Qt::UserRole + 2;
constexpr int kRoleLink        = Qt::UserRole + 3;
// Spell id this row's ACTION references (0 if the action carries no
// spell).  Stored on the action sub-row so the selection-change handler
// can emit it to the SpellInfoDock without re-walking the rule.
constexpr int kRoleSpellId     = Qt::UserRole + 4;

// SMART_ACTION_* values from TC's SmartScripts.h that carry a spell id
// in actionParam1.  Conservative list -- only the actions whose
// param1 is unambiguously a spell id.
constexpr bool actionParam1IsSpell(uint8_t actionType)
{
    switch (actionType)
    {
        case 11: // SMART_ACTION_CAST
        case 15: // SMART_ACTION_REMOVE_AURASFROMSPELL (older builds; modern is 28 but flow viewer is build-agnostic)
        case 28: // SMART_ACTION_REMOVE_AURASFROMSPELL (modern TC)
        case 75: // SMART_ACTION_ADD_AURA
        case 85: // SMART_ACTION_INVOKER_CAST
        case 86: // SMART_ACTION_CROSS_CAST
        case 134:// SMART_ACTION_CAST_CUSTOM_SPELL
        case 188:// SMART_ACTION_CASTING_FROM_TARGET (param1 = spell)
            return true;
        default:
            return false;
    }
}

// Source-type IDs from TC's SmartScripts.h.  0=creature, 1=GO, 9=action_list.
constexpr uint8_t kSourceTypeCreature   = 0;
constexpr uint8_t kSourceTypeActionList = 9;
// SMART_ACTION_CALL_TIMED_ACTIONLIST value from TC.  Carries the
// action-list id in actionParam1.
constexpr uint8_t kActionCallTimedActionList = 80;

QString enumLookup(SmartScriptEnumEntry const* table, size_t tableSize, int value)
{
    for (size_t i = 0; i < tableSize; ++i)
    {
        if (table[i].value == value)
            return QString::fromLatin1(table[i].name);
    }
    return QStringLiteral("<unknown %1>").arg(value);
}
} // namespace

SmartScriptFlowDialog::SmartScriptFlowDialog(db::MySqlClient* dbClient,
                                             int64_t initialEntryOrGuid,
                                             uint8_t initialSourceType,
                                             QWidget* parent)
    : QDialog(parent)
    , m_db(dbClient)
{
    setWindowTitle(tr("Smart script flow"));
    resize(960, 720);

    auto* root = new QVBoxLayout(this);

    auto* hint = new QLabel(
        tr("Browse the events / actions for a given (entryorguid, source_type) "
           "with linked-chain and CALL_TIMED_ACTIONLIST expansion.  "
           "Double-click any row to open it in the smart-script editor."),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout();
    m_entryOrGuidSpin = new QSpinBox(this);
    m_entryOrGuidSpin->setRange(std::numeric_limits<int>::min(),
                                std::numeric_limits<int>::max());
    m_entryOrGuidSpin->setValue(int(initialEntryOrGuid));
    form->addRow(tr("entryorguid:"), m_entryOrGuidSpin);

    m_sourceTypeSpin = new QSpinBox(this);
    m_sourceTypeSpin->setRange(0, 255);
    m_sourceTypeSpin->setValue(initialSourceType);
    form->addRow(tr("source_type:"), m_sourceTypeSpin);
    root->addLayout(form);

    auto* scanButton = new QPushButton(tr("Scan"), this);
    connect(scanButton, &QPushButton::clicked, this, &SmartScriptFlowDialog::onRescan);
    root->addWidget(scanButton);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({ tr("Rule"), tr("Comment / detail") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::itemActivated,
            this, &SmartScriptFlowDialog::onItemActivated);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &SmartScriptFlowDialog::onItemSelectionChanged);
    root->addWidget(m_tree, /*stretch=*/1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);

    onRescan();
}

void SmartScriptFlowDialog::onRescan()
{
    m_tree->clear();
    m_rules.clear();
    m_actionListCache.clear();
    if (!m_db || !m_db->isConnected())
    {
        auto* err = new QTreeWidgetItem(m_tree);
        err->setText(0, tr("Database not connected."));
        return;
    }
    int64_t const eoid = int64_t(m_entryOrGuidSpin->value());
    uint8_t const st   = uint8_t(m_sourceTypeSpin->value());
    buildTree(eoid, st);
}

void SmartScriptFlowDialog::buildTree(int64_t entryOrGuid, uint8_t sourceType)
{
    m_rules = fetchRulesFor(entryOrGuid, sourceType);
    if (m_rules.empty())
    {
        auto* empty = new QTreeWidgetItem(m_tree);
        empty->setText(0,
            tr("No smart_scripts rows for entryorguid=%1 source_type=%2.")
                .arg(entryOrGuid).arg(sourceType));
        return;
    }
    auto* root = new QTreeWidgetItem(m_tree);
    root->setText(0, QStringLiteral("entryorguid=%1  source_type=%2  (%3 rule(s))")
        .arg(entryOrGuid).arg(sourceType).arg(m_rules.size()));
    QFont rootFont = root->font(0);
    rootFont.setBold(true);
    root->setFont(0, rootFont);
    root->setExpanded(true);

    // Only render top-level rules (link == 0).  Linked rows are reached
    // recursively from the linker via addRuleRow.
    for (render::SmartScript const& r : m_rules)
    {
        if (r.link != 0) continue;
        addRuleRow(root, r, /*depth=*/0);
    }
}

void SmartScriptFlowDialog::addRuleRow(QTreeWidgetItem* parent,
                                       render::SmartScript const& rule,
                                       int depth)
{
    if (depth >= kMaxDepth) return;

    auto* eventIt = new QTreeWidgetItem(parent);
    eventIt->setText(0, formatEvent(rule));
    eventIt->setText(1, rule.comment);
    eventIt->setData(0, kRoleEntryOrGuid, qlonglong(rule.entryorguid));
    eventIt->setData(0, kRoleSourceType,  int(rule.sourceType));
    eventIt->setData(0, kRoleId,          int(rule.id));
    eventIt->setData(0, kRoleLink,        int(rule.link));
    eventIt->setExpanded(depth <= 1);

    auto* actionIt = new QTreeWidgetItem(eventIt);
    actionIt->setText(0, formatAction(rule));
    actionIt->setText(1, formatTarget(rule));
    actionIt->setData(0, kRoleEntryOrGuid, qlonglong(rule.entryorguid));
    actionIt->setData(0, kRoleSourceType,  int(rule.sourceType));
    actionIt->setData(0, kRoleId,          int(rule.id));
    actionIt->setData(0, kRoleLink,        int(rule.link));
    // Stash the spell id for the spell-info dock if this action
    // references one.  Zero = "no spell reference" -> dock clears.
    actionIt->setData(0, kRoleSpellId,
        actionParam1IsSpell(rule.actionType)
            ? uint(rule.actionParam1) : 0u);

    // If the action is CALL_TIMED_ACTIONLIST, drill into the called
    // action_list (source_type=9, entryorguid = actionParam1).
    if (rule.actionType == kActionCallTimedActionList
        && rule.actionParam1 != 0)
    {
        int64_t const calleeEoid = int64_t(rule.actionParam1);
        auto it = m_actionListCache.find(calleeEoid);
        if (it == m_actionListCache.end())
        {
            auto rows = fetchRulesFor(calleeEoid, kSourceTypeActionList);
            it = m_actionListCache.emplace(calleeEoid, std::move(rows)).first;
        }
        auto* callIt = new QTreeWidgetItem(actionIt);
        callIt->setText(0, tr("calls action_list %1  (%2 row(s))")
            .arg(calleeEoid).arg(it->second.size()));
        QFont f = callIt->font(0);
        f.setItalic(true);
        callIt->setFont(0, f);
        for (render::SmartScript const& sub : it->second)
        {
            addRuleRow(callIt, sub, depth + 1);
        }
    }

    // If this rule has a linked successor (link == id of another rule
    // in the same entryorguid), expand it inline.  Note: the linked-id
    // semantics in TC are: rule A with link=L sets up a chain where
    // rule with id=L runs after A.  We resolve this lookup against
    // m_rules.
    if (rule.link != 0)
        return; // shouldn't be reached -- linked rows are reached via parent
    // Walk forward: any rule whose `id` equals rule.id+offset and whose
    // `link` chain originates from this rule is reachable.  The TC
    // schema actually links via: a rule with id=X and link=Y -> the
    // event runs Y's body too.  So find the rule whose id == rule.link
    // ... but rule.link was 0 here.  Iterate: maybe rule A has
    // link=0 and another rule B has id=A.id+something and link=A.id.
    // The simpler model: a follow-up rule B has link=A.id, meaning B
    // runs when A's event fires.  So look for any rule whose link==rule.id.
    for (render::SmartScript const& r : m_rules)
    {
        if (r.link == rule.id && r.id != rule.id)
        {
            auto* linkIt = new QTreeWidgetItem(eventIt);
            linkIt->setText(0, tr("(linked) id=%1 link=%2").arg(r.id).arg(r.link));
            QFont f = linkIt->font(0);
            f.setItalic(true);
            linkIt->setFont(0, f);
            // Recurse with depth+1 to drill into the linked rule's
            // action + further links.
            addRuleRow(linkIt, r, depth + 1);
        }
    }
}

QString SmartScriptFlowDialog::formatEvent(render::SmartScript const& r) const
{
    return tr("id=%1 EVENT %2(p1=%3 p2=%4 p3=%5)  chance=%6%  phase=0x%7")
        .arg(r.id)
        .arg(enumLookup(kSmartEvents, std::size(kSmartEvents), r.eventType))
        .arg(r.eventParam1).arg(r.eventParam2).arg(r.eventParam3)
        .arg(r.eventChance)
        .arg(QString::number(r.eventPhaseMask, 16));
}

QString SmartScriptFlowDialog::formatAction(render::SmartScript const& r) const
{
    return tr("ACTION %1(p1=%2 p2=%3 p3=%4 p4=%5 p5=%6 p6=%7)")
        .arg(enumLookup(kSmartActions, std::size(kSmartActions), r.actionType))
        .arg(r.actionParam1).arg(r.actionParam2).arg(r.actionParam3)
        .arg(r.actionParam4).arg(r.actionParam5).arg(r.actionParam6);
}

QString SmartScriptFlowDialog::formatTarget(render::SmartScript const& r) const
{
    return tr("TARGET %1(p1=%2 p2=%3)")
        .arg(enumLookup(kSmartTargets, std::size(kSmartTargets), r.targetType))
        .arg(r.targetParam1).arg(r.targetParam2);
}

std::vector<render::SmartScript>
SmartScriptFlowDialog::fetchRulesFor(int64_t entryOrGuid, uint8_t sourceType)
{
    std::vector<render::SmartScript> rows;
    if (!m_db || !m_db->isConnected()) return rows;

    QString const sql = QStringLiteral(
        "SELECT entryorguid, source_type, id, link, "
        "       event_type, event_phase_mask, event_chance, event_flags, "
        "       event_param1, event_param2, event_param3, event_param4, event_param5, "
        "       action_type, action_param1, action_param2, action_param3, "
        "       action_param4, action_param5, action_param6, action_param7, "
        "       target_type, target_param1, target_param2, target_param3, target_param4, "
        "       target_x, target_y, target_z, target_o, comment "
        "FROM smart_scripts "
        "WHERE entryorguid = %1 AND source_type = %2 "
        "ORDER BY id, link").arg(entryOrGuid).arg(int(sourceType));

    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
        return rows;

    rows.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::SmartScript s;
        s.entryorguid    = res.asInt64 (r, 0).value_or(0);
        s.sourceType     = uint8_t(res.asUInt64(r, 1).value_or(0));
        s.id             = uint16_t(res.asUInt64(r, 2).value_or(0));
        s.link           = uint16_t(res.asUInt64(r, 3).value_or(0));
        s.eventType      = uint8_t(res.asUInt64(r, 4).value_or(0));
        s.eventPhaseMask = uint16_t(res.asUInt64(r, 5).value_or(0));
        s.eventChance    = uint8_t(res.asUInt64(r, 6).value_or(100));
        s.eventFlags     = uint16_t(res.asUInt64(r, 7).value_or(0));
        s.eventParam1    = uint32_t(res.asUInt64(r, 8).value_or(0));
        s.eventParam2    = uint32_t(res.asUInt64(r, 9).value_or(0));
        s.eventParam3    = uint32_t(res.asUInt64(r, 10).value_or(0));
        s.eventParam4    = uint32_t(res.asUInt64(r, 11).value_or(0));
        s.eventParam5    = uint32_t(res.asUInt64(r, 12).value_or(0));
        s.actionType     = uint8_t(res.asUInt64(r, 13).value_or(0));
        s.actionParam1   = uint32_t(res.asUInt64(r, 14).value_or(0));
        s.actionParam2   = uint32_t(res.asUInt64(r, 15).value_or(0));
        s.actionParam3   = uint32_t(res.asUInt64(r, 16).value_or(0));
        s.actionParam4   = uint32_t(res.asUInt64(r, 17).value_or(0));
        s.actionParam5   = uint32_t(res.asUInt64(r, 18).value_or(0));
        s.actionParam6   = uint32_t(res.asUInt64(r, 19).value_or(0));
        s.actionParam7   = uint32_t(res.asUInt64(r, 20).value_or(0));
        s.targetType     = uint8_t(res.asUInt64(r, 21).value_or(0));
        s.targetParam1   = uint32_t(res.asUInt64(r, 22).value_or(0));
        s.targetParam2   = uint32_t(res.asUInt64(r, 23).value_or(0));
        s.targetParam3   = uint32_t(res.asUInt64(r, 24).value_or(0));
        s.targetParam4   = uint32_t(res.asUInt64(r, 25).value_or(0));
        s.targetX        = float(res.asDouble(r, 26).value_or(0.0));
        s.targetY        = float(res.asDouble(r, 27).value_or(0.0));
        s.targetZ        = float(res.asDouble(r, 28).value_or(0.0));
        s.targetO        = float(res.asDouble(r, 29).value_or(0.0));
        s.comment        = QString::fromStdString(res.cell(r, 30));
        rows.push_back(std::move(s));
    }
    (void)kSourceTypeCreature;
    return rows;
}

void SmartScriptFlowDialog::onItemSelectionChanged()
{
    // Fire spellReferenced(id) for the newly-selected action row if it
    // carries a spell id; fire spellReferenced(0) otherwise so the
    // SpellInfoDock clears.  No-op when nothing is selected.
    auto const selected = m_tree->selectedItems();
    if (selected.isEmpty())
        return;
    QVariant const v = selected.first()->data(0, kRoleSpellId);
    uint32_t const spellId = v.isValid() ? v.toUInt() : 0u;
    emit spellReferenced(spellId);
}

void SmartScriptFlowDialog::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QVariant const eoid = item->data(0, kRoleEntryOrGuid);
    QVariant const st   = item->data(0, kRoleSourceType);
    QVariant const idv  = item->data(0, kRoleId);
    QVariant const lnk  = item->data(0, kRoleLink);
    if (!eoid.isValid() || !st.isValid() || !idv.isValid())
        return;
    emit editRequested(eoid.toLongLong(),
                       st.toInt(),
                       idv.toInt(),
                       lnk.isValid() ? lnk.toInt() : 0);
}

} // namespace world_editor::app
