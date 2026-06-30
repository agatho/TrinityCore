#include "SmartScriptDryRunDialog.h"

#include "SmartScriptEnumTables.h"
#include "../db/MySqlClient.h"
#include "../render/NavMeshView.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include <limits>

namespace world_editor::app
{

namespace
{

// SMART_EVENT values that ONLY fire while the unit is in combat
// (UPDATE_IC, AGGRO, KILL, EVADE, SPELLHIT, RANGE, VICTIM_CASTING,
// HAS_AURA targeted on victim, RESET fires once when leaving combat
// but we treat it as combat-bound so a non-combat dry-run skips it).
constexpr bool eventRequiresCombat(uint8_t et)
{
    switch (et)
    {
        case 0:  // UPDATE_IC
        case 4:  // AGGRO
        case 5:  // KILL
        case 7:  // EVADE
        case 8:  // SPELLHIT (when self is the victim being hit)
        case 9:  // RANGE
        case 13: // VICTIM_CASTING
        case 25: // RESET (post-combat)
        case 32: // DAMAGED
        case 33: // DAMAGED_TARGET
            return true;
        default:
            return false;
    }
}

// HP-gated event check.  Returns true if the rule's [param1, param2]
// range covers simulatedHpPct.  If param2 < param1 the rule is malformed;
// we treat it as a single-point match against param1.
bool hpInRange(render::SmartScript const& r, int simulatedHpPct)
{
    int const lo = int(r.eventParam1);
    int const hi = int(r.eventParam2);
    if (hi < lo) return simulatedHpPct == lo;
    return simulatedHpPct >= lo && simulatedHpPct <= hi;
}

QString enumLookup(SmartScriptEnumEntry const* table, size_t tableSize, int value)
{
    for (size_t i = 0; i < tableSize; ++i)
        if (table[i].value == value)
            return QString::fromLatin1(table[i].name);
    return QStringLiteral("<unknown %1>").arg(value);
}

} // namespace

SmartScriptDryRunDialog::SmartScriptDryRunDialog(db::MySqlClient* dbClient,
                                                 int64_t entryOrGuid,
                                                 uint8_t sourceType,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_db(dbClient)
    , m_entryOrGuid(entryOrGuid)
    , m_sourceType(sourceType)
{
    setWindowTitle(tr("Smart script dry-run trace"));
    resize(1000, 760);

    auto* root = new QVBoxLayout(this);

    auto* hint = new QLabel(
        tr("Heuristic SAI simulator: pick an event + condition inputs, click Run, and the "
           "trace lists every smart_scripts rule that would FIRE for this (entryorguid, "
           "source_type) under those inputs.  Skipped rules log WHY they were skipped.  "
           "Anything driven by runtime world-state (LOS, target acquisition, talk events, "
           "passenger transitions, charm, etc.) is assumed present so candidates are not "
           "hidden by false negatives."),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* pkLabel = new QLabel(
        tr("entryorguid = %1    source_type = %2")
            .arg(entryOrGuid).arg(sourceType),
        this);
    QFont pkFont = pkLabel->font();
    pkFont.setBold(true);
    pkLabel->setFont(pkFont);
    root->addWidget(pkLabel);

    auto* form = new QFormLayout();

    m_eventTypeSpin = new QSpinBox(this);
    m_eventTypeSpin->setRange(0, 255);
    m_eventTypeSpin->setValue(0);
    m_eventTypeSpin->setToolTip(tr("SMART_EVENT type to simulate.  0=UPDATE_IC, 2=HEALTH_PCT, "
                                   "4=AGGRO, 6=DEATH, 8=SPELLHIT, 11=RESPAWN, 19=ACCEPTED_QUEST, "
                                   "20=REWARD_QUEST, 22=RECEIVE_EMOTE, 25=RESET, ..."));
    form->addRow(tr("eventType:"), m_eventTypeSpin);

    m_phaseMaskSpin = new QSpinBox(this);
    m_phaseMaskSpin->setRange(0, 11);
    m_phaseMaskSpin->setValue(1);
    m_phaseMaskSpin->setToolTip(tr("Current phase index (0-11).  A rule fires only if its "
                                   "event_phase_mask has the matching bit set, or if "
                                   "event_phase_mask == 0 (treated as wildcard)."));
    form->addRow(tr("phase (0-11):"), m_phaseMaskSpin);

    m_hpPctSpin = new QSpinBox(this);
    m_hpPctSpin->setRange(0, 100);
    m_hpPctSpin->setValue(100);
    m_hpPctSpin->setSuffix(tr(" %"));
    m_hpPctSpin->setToolTip(tr("Simulated HP percent for HEALTH_PCT (event 2) range matching."));
    form->addRow(tr("simulated_hp_pct:"), m_hpPctSpin);

    m_inCombatCheck = new QCheckBox(this);
    m_inCombatCheck->setChecked(false);
    m_inCombatCheck->setToolTip(tr("Gate for combat-only events (UPDATE_IC, AGGRO, KILL, "
                                   "EVADE, SPELLHIT, RANGE, VICTIM_CASTING, RESET, DAMAGED, "
                                   "DAMAGED_TARGET).  Non-combat events ignore this."));
    form->addRow(tr("is_in_combat:"), m_inCombatCheck);

    m_eventParamSpin = new QSpinBox(this);
    m_eventParamSpin->setRange(0, std::numeric_limits<int>::max());
    m_eventParamSpin->setValue(0);
    m_eventParamSpin->setToolTip(tr("A single test value plugged into whatever the chosen "
                                    "event's primary parameter expects: spell id for SPELLHIT, "
                                    "emote id for RECEIVE_EMOTE, quest id for ACCEPTED_QUEST/"
                                    "REWARD_QUEST, summon entry for SUMMONED_UNIT, etc.  "
                                    "Zero is treated as 'match any'."));
    form->addRow(tr("event_param_value:"), m_eventParamSpin);

    root->addLayout(form);

    m_runButton = new QPushButton(tr("Run"), this);
    connect(m_runButton, &QPushButton::clicked, this, &SmartScriptDryRunDialog::onRun);
    root->addWidget(m_runButton);

    m_trace = new QTextEdit(this);
    m_trace->setReadOnly(true);
    m_trace->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_trace->setLineWrapMode(QTextEdit::NoWrap);
    root->addWidget(m_trace, /*stretch=*/1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);
}

void SmartScriptDryRunDialog::onRun()
{
    m_trace->clear();
    if (!m_db || !m_db->isConnected())
    {
        appendLine(0, tr("ERROR: database not connected."));
        return;
    }

    auto const rules = fetchRules(m_entryOrGuid, m_sourceType);
    appendLine(0, tr("=== Dry-run trace for entryorguid=%1 source_type=%2 ===")
        .arg(m_entryOrGuid).arg(m_sourceType));
    appendLine(0, tr("    eventType=%1 (%2)   phase=%3   hp=%4%%   combat=%5   param=%6")
        .arg(m_eventTypeSpin->value())
        .arg(enumLookup(kSmartEvents, std::size(kSmartEvents), m_eventTypeSpin->value()))
        .arg(m_phaseMaskSpin->value())
        .arg(m_hpPctSpin->value())
        .arg(m_inCombatCheck->isChecked() ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(m_eventParamSpin->value()));
    appendLine(0, QStringLiteral(""));

    if (rules.empty())
    {
        appendLine(0, tr("No smart_scripts rows for this (entryorguid, source_type)."));
        return;
    }
    appendLine(0, tr("Found %1 rule(s).  Walking top-level (link == 0) rules:").arg(rules.size()));
    appendLine(0, QStringLiteral(""));

    int fireCount = 0;
    int skipCount = 0;
    for (render::SmartScript const& r : rules)
    {
        // Only walk top-level rules from the outer loop; linked successors are
        // reached via the recursive `simulateRule` chain follow.
        if (r.link != 0) continue;
        bool const fired = simulateRule(r, rules, /*depth=*/0);
        if (fired) ++fireCount; else ++skipCount;
    }
    appendLine(0, QStringLiteral(""));
    appendLine(0, tr("=== Summary: %1 fired, %2 skipped (top-level rules only) ===")
        .arg(fireCount).arg(skipCount));
}

bool SmartScriptDryRunDialog::simulateRule(render::SmartScript const& rule,
                                           std::vector<render::SmartScript> const& allRules,
                                           int depth)
{
    if (depth >= kMaxDepth)
    {
        appendLine(depth, tr("[max-depth %1 reached; truncating link chain]").arg(kMaxDepth));
        return false;
    }

    int const wantEvent = m_eventTypeSpin->value();
    int const wantPhase = m_phaseMaskSpin->value();
    int const simHp     = m_hpPctSpin->value();
    bool const inCombat = m_inCombatCheck->isChecked();
    uint32_t const paramTest = uint32_t(m_eventParamSpin->value());

    QString const head = tr("id=%1 link=%2 event=%3(%4) phase=0x%5 chance=%6%")
        .arg(rule.id).arg(rule.link)
        .arg(rule.eventType)
        .arg(enumLookup(kSmartEvents, std::size(kSmartEvents), rule.eventType))
        .arg(QString::number(rule.eventPhaseMask, 16))
        .arg(rule.eventChance);

    // 1. Event-type gate.
    if (int(rule.eventType) != wantEvent)
    {
        appendLine(depth, tr("SKIP  %1 -- event_type mismatch (need %2)").arg(head).arg(wantEvent));
        return false;
    }

    // 2. Phase-mask gate.  event_phase_mask==0 is treated as wildcard
    // (matches every phase, including PHASE_ALWAYS).
    if (rule.eventPhaseMask != 0)
    {
        uint32_t const wantBit = 1u << wantPhase;
        if ((uint32_t(rule.eventPhaseMask) & wantBit) == 0)
        {
            appendLine(depth, tr("SKIP  %1 -- phase 0x%2 does not include bit 0x%3")
                .arg(head)
                .arg(QString::number(rule.eventPhaseMask, 16))
                .arg(QString::number(wantBit, 16)));
            return false;
        }
    }

    // 3. Chance gate.  Deterministic for trace: chance>=100 always fires,
    // chance<100 is reported as a "would skip when < N%" advisory but we
    // proceed so the operator can see the action chain.
    if (rule.eventChance < 100)
    {
        appendLine(depth, tr("NOTE  %1 -- chance %2% (probabilistic; trace assumes hit)")
            .arg(head).arg(rule.eventChance));
    }

    // 4. Combat-presence gate.  Skip combat-only events when is_in_combat
    // is unchecked.
    if (eventRequiresCombat(rule.eventType) && !inCombat)
    {
        appendLine(depth, tr("SKIP  %1 -- event requires combat but is_in_combat=false").arg(head));
        return false;
    }

    // 5. HP-range gate for HEALTH_PCT (event 2).
    if (rule.eventType == 2 /* HEALTH_PCT */)
    {
        if (!hpInRange(rule, simHp))
        {
            appendLine(depth, tr("SKIP  %1 -- hp %2%% out of [%3,%4]")
                .arg(head).arg(simHp)
                .arg(rule.eventParam1).arg(rule.eventParam2));
            return false;
        }
    }
    // FRIENDLY_HEALTH-style events (kept generic): event 4 is AGGRO, not
    // HP-gated by core, but FRIENDLY_HEALTH (14) and FRIENDLY_HEALTH_PCT
    // variants use param1 as a threshold.  We model the param1-as-threshold
    // case for any event whose param1 looks like an HP percent (1..100).
    else if (rule.eventType == 14 /* FRIENDLY_HEALTH (unused upstream but
              kept for completeness) */)
    {
        if (rule.eventParam1 > 0 && rule.eventParam1 <= 100
            && uint32_t(simHp) > rule.eventParam1)
        {
            appendLine(depth, tr("SKIP  %1 -- friendly hp threshold %2%% not crossed (sim %3%%)")
                .arg(head).arg(rule.eventParam1).arg(simHp));
            return false;
        }
    }

    // 6. Event-param value gate.  For events whose primary param is a
    // discrete id (spell, quest, emote, summon entry, movement type, ...),
    // skip rules whose param1 doesn't match the operator's test value.
    // paramTest == 0 means "match any" so the operator can run a generic
    // dry-run without picking a specific id.
    if (paramTest != 0 && rule.eventParam1 != 0 && rule.eventParam1 != paramTest)
    {
        // Only enforce param1 match for event types where param1 carries
        // an id-like value (SPELLHIT, RECEIVE_EMOTE, ACCEPTED_QUEST, REWARD_QUEST,
        // SUMMONED_UNIT, MOVEMENTINFORM, AREATRIGGER, GO_STATE_CHANGED, GAMEEVENT_*,
        // SPELLHIT_TARGET, RECEIVE_HEAL, INSTANCE_PLAYER_ENTER, HAS_AURA, TARGET_BUFFED).
        switch (rule.eventType)
        {
            case 8: case 17: case 19: case 20: case 22: case 23: case 24:
            case 31: case 34: case 39: case 47: case 68: case 69:
                appendLine(depth, tr("SKIP  %1 -- param1=%2 != test value %3")
                    .arg(head).arg(rule.eventParam1).arg(paramTest));
                return false;
            default:
                break;
        }
    }

    // All gates passed -- FIRE.
    appendLine(depth, tr("FIRE  %1").arg(head));
    appendLine(depth + 1, formatActionDecoded(rule)
        + (rule.comment.isEmpty() ? QString() : QStringLiteral("  // ") + rule.comment));

    // CALL_TIMED_ACTIONLIST (action 80) advisory: surface the callee so the
    // operator can dry-run it separately.  We do NOT recurse into source_type=9
    // here -- a full action-list trace warrants opening the flow viewer.
    if (rule.actionType == 80 && rule.actionParam1 != 0)
    {
        appendLine(depth + 1, tr(">> calls action_list entryorguid=%1 (open the flow viewer "
                                 "for the full chain)")
            .arg(rule.actionParam1));
    }

    // Linked-chain follow: walk every rule whose `link` equals this rule's id.
    // A linked rule's event_type is informational only (the upstream rule's
    // event is what triggered the chain), so we expand the linked rule by
    // logging its ACTION directly rather than re-running the gate cascade.
    for (render::SmartScript const& linked : allRules)
    {
        if (linked.link == rule.id && linked.id != rule.id)
        {
            appendLine(depth + 1, tr("LINK  id=%1 link=%2  (chained off id=%3)")
                .arg(linked.id).arg(linked.link).arg(rule.id));
            appendLine(depth + 2, formatActionDecoded(linked)
                + (linked.comment.isEmpty() ? QString() : QStringLiteral("  // ") + linked.comment));
            // Recurse to expand any further link chain (linked->linked->...).
            for (render::SmartScript const& deeper : allRules)
            {
                if (deeper.link == linked.id && deeper.id != linked.id)
                {
                    appendLine(depth + 2, tr("LINK  id=%1 link=%2  (chained off id=%3)")
                        .arg(deeper.id).arg(deeper.link).arg(linked.id));
                    appendLine(depth + 3, formatActionDecoded(deeper)
                        + (deeper.comment.isEmpty() ? QString()
                                                    : QStringLiteral("  // ") + deeper.comment));
                }
            }
        }
    }
    return true;
}

void SmartScriptDryRunDialog::appendLine(int depth, QString const& line)
{
    QString prefix;
    for (int i = 0; i < depth; ++i) prefix += QStringLiteral("  ");
    m_trace->append(prefix + line);
}

QString SmartScriptDryRunDialog::formatActionDecoded(render::SmartScript const& r) const
{
    return tr("ACTION %1(p1=%2 p2=%3 p3=%4)")
        .arg(enumLookup(kSmartActions, std::size(kSmartActions), r.actionType))
        .arg(r.actionParam1).arg(r.actionParam2).arg(r.actionParam3);
}

std::vector<render::SmartScript>
SmartScriptDryRunDialog::fetchRules(int64_t entryOrGuid, uint8_t sourceType)
{
    std::vector<render::SmartScript> rows;
    if (!m_db || !m_db->isConnected()) return rows;

    QString const sql = QStringLiteral(
        "SELECT entryorguid, source_type, id, link, "
        "       event_type, event_phase_mask, event_chance, event_flags, "
        "       event_param1, event_param2, event_param3, event_param4, event_param5, "
        "       action_type, action_param1, action_param2, action_param3, "
        "       target_type, comment "
        "FROM smart_scripts "
        "WHERE entryorguid = %1 AND source_type = %2 "
        "ORDER BY id, link").arg(entryOrGuid).arg(int(sourceType));

    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok()) return rows;

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
        s.targetType     = uint8_t(res.asUInt64(r, 17).value_or(0));
        s.comment        = QString::fromStdString(res.cell(r, 18));
        rows.push_back(std::move(s));
    }
    return rows;
}

} // namespace world_editor::app
