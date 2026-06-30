#include "SmartScriptEditDialog.h"

#include "SmartScriptEnumTables.h"
#include "SmartAiParams.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <climits>

namespace world_editor::app
{

namespace
{

QSpinBox* makeUIntSpin(int max = INT_MAX)
{
    auto* s = new QSpinBox;
    s->setRange(0, max);
    s->setSingleStep(1);
    s->setAccelerated(true);
    return s;
}

QDoubleSpinBox* makeDoubleSpin()
{
    auto* s = new QDoubleSpinBox;
    s->setRange(-100000.0, 100000.0);
    s->setDecimals(4);
    s->setSingleStep(0.1);
    s->setAccelerated(true);
    return s;
}

// Format a SMART_* enum row for combo display: 3-wide right-aligned
// numeric followed by two spaces then the short name -- e.g.
// "  1  EVENT_AGGRO".  The "EVENT_"/"ACTION_"/"TARGET_" qualifier is
// re-added here so the operator can distinguish across all three
// columns at a glance even if they later copy a label.
template <std::size_t N>
QComboBox* makeEnumCombo(SmartScriptEnumEntry const (&table)[N], char const* qualifier,
                         char const* (*paramFn)(int) = nullptr)
{
    auto* c = new QComboBox;
    for (auto const& entry : table)
    {
        QString const text = QString::asprintf("%3d  %s_%s",
            entry.value, qualifier, entry.name);
        c->addItem(text, entry.value);
        // Tooltip the numeric-param semantics (from the build-time-generated
        // metadata) so the operator sees what param1..4 mean for this value.
        if (paramFn)
        {
            char const* params = paramFn(entry.value);
            if (params && params[0])
                c->setItemData(c->count() - 1,
                    QStringLiteral("params: ") + QString::fromLatin1(params),
                    Qt::ToolTipRole);
        }
    }
    // Some of the enums have ~160 entries.  A scrollable popup with
    // keyboard search by typing the numeric prefix is the most usable
    // shape; Qt's default already supports type-ahead.
    c->setMaxVisibleItems(20);
    return c;
}

// Select the combo row whose userData matches `value`.  If no row
// matches, append a synthetic "(unknown N)" entry at the end and
// select it -- this preserves operator-pasted future-TC values across
// open/save round-trips.  Idempotent: opening the dialog repeatedly
// on the same exotic value reuses the existing "(unknown N)" row.
void selectEnumValue(QComboBox* c, int value)
{
    int idx = c->findData(value);
    if (idx < 0)
    {
        QString const text = QString::asprintf("%3d  (unknown %d)", value, value);
        // Defence against a previously-appended unknown row that the
        // findData call somehow missed (e.g. mismatched type).  We
        // search the visible text as a safety net before appending.
        for (int i = 0; i < c->count(); ++i)
        {
            if (c->itemText(i) == text)
            {
                idx = i;
                break;
            }
        }
        if (idx < 0)
        {
            c->addItem(text, value);
            idx = c->count() - 1;
        }
    }
    c->setCurrentIndex(idx);
}

} // namespace

SmartScriptEditDialog::SmartScriptEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit smart_scripts row"));
    setModal(true);
    resize(560, 520);
    buildUi();
}

void SmartScriptEditDialog::buildUi()
{
    m_tabs = new QTabWidget(this);

    buildIdentityTab();
    buildEventTab();
    buildActionTab();
    buildTargetTab();
    buildCommentTab();

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_tabs, 1);
    outer->addWidget(buttons);
}

void SmartScriptEditDialog::buildIdentityTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    m_entryorguidEdit = new QLineEdit;
    // Signed bigint: 19 digits + optional leading '-'.
    auto* re = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^-?\\d{1,19}$")), m_entryorguidEdit);
    m_entryorguidEdit->setValidator(re);
    m_entryorguidEdit->setText(QStringLiteral("0"));
    m_entryorguidEdit->setToolTip(tr(
        "bigint signed.  Positive = creature_template.entry (source_type=0) "
        "or gameobject_template.entry (source_type=1) or action_list id "
        "(source_type=9).  Negative = creature.guid (per-spawn SAI override)."));

    m_sourceTypeCombo = new QComboBox;
    m_sourceTypeCombo->addItem(tr("0 - Creature"),    0);
    m_sourceTypeCombo->addItem(tr("1 - GameObject"),  1);
    m_sourceTypeCombo->addItem(tr("9 - Action List"), 9);

    m_idSpin   = makeUIntSpin(65535);
    m_linkSpin = makeUIntSpin(65535);

    m_difficultiesEdit = new QLineEdit;
    m_difficultiesEdit->setMaxLength(100);
    m_difficultiesEdit->setPlaceholderText(tr("(empty = all difficulties)"));

    form->addRow(tr("entryorguid"), m_entryorguidEdit);
    form->addRow(tr("source_type"), m_sourceTypeCombo);
    form->addRow(tr("id"),          m_idSpin);
    form->addRow(tr("link"),        m_linkSpin);
    form->addRow(tr("Difficulties"), m_difficultiesEdit);

    m_tabs->addTab(w, tr("Identity"));
}

void SmartScriptEditDialog::buildEventTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    m_eventTypeCombo     = makeEnumCombo(kSmartEvents,  "EVENT", &smartEventParams);
    m_eventPhaseMaskSpin = makeUIntSpin(65535);
    m_eventChanceSpin    = makeUIntSpin(100);
    m_eventChanceSpin->setValue(100);
    m_eventFlagsSpin     = makeUIntSpin(65535);
    m_eventP1Spin = makeUIntSpin();
    m_eventP2Spin = makeUIntSpin();
    m_eventP3Spin = makeUIntSpin();
    m_eventP4Spin = makeUIntSpin();
    m_eventP5Spin = makeUIntSpin();
    m_eventParamStringEdit = new QLineEdit;
    m_eventParamStringEdit->setMaxLength(255);

    form->addRow(tr("event_type"),        m_eventTypeCombo);
    form->addRow(tr("event_phase_mask"),  m_eventPhaseMaskSpin);
    form->addRow(tr("event_chance"),      m_eventChanceSpin);
    form->addRow(tr("event_flags"),       m_eventFlagsSpin);
    form->addRow(tr("event_param1"),      m_eventP1Spin);
    form->addRow(tr("event_param2"),      m_eventP2Spin);
    form->addRow(tr("event_param3"),      m_eventP3Spin);
    form->addRow(tr("event_param4"),      m_eventP4Spin);
    form->addRow(tr("event_param5"),      m_eventP5Spin);
    form->addRow(tr("event_param_string"), m_eventParamStringEdit);

    m_tabs->addTab(w, tr("Event"));
}

void SmartScriptEditDialog::buildActionTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    m_actionTypeCombo = makeEnumCombo(kSmartActions, "ACTION", &smartActionParams);
    m_actionP1Spin   = makeUIntSpin();
    m_actionP2Spin   = makeUIntSpin();
    m_actionP3Spin   = makeUIntSpin();
    m_actionP4Spin   = makeUIntSpin();
    m_actionP5Spin   = makeUIntSpin();
    m_actionP6Spin   = makeUIntSpin();
    m_actionP7Spin   = makeUIntSpin();

    auto* aStrRow = new QWidget;
    auto* aStrLay = new QHBoxLayout(aStrRow);
    aStrLay->setContentsMargins(0, 0, 0, 0);
    m_actionParamStringEdit = new QLineEdit;
    m_actionParamStringEdit->setMaxLength(255);
    m_actionParamStringNullChk = new QCheckBox(tr("NULL"));
    m_actionParamStringNullChk->setChecked(true);
    aStrLay->addWidget(m_actionParamStringEdit, 1);
    aStrLay->addWidget(m_actionParamStringNullChk);

    connect(m_actionParamStringNullChk, &QCheckBox::toggled, this,
            [this](bool checked) {
                m_actionParamStringEdit->setEnabled(!checked);
                if (checked) m_actionParamStringEdit->clear();
            });
    m_actionParamStringEdit->setEnabled(false);

    form->addRow(tr("action_type"),         m_actionTypeCombo);
    form->addRow(tr("action_param1"),       m_actionP1Spin);
    form->addRow(tr("action_param2"),       m_actionP2Spin);
    form->addRow(tr("action_param3"),       m_actionP3Spin);
    form->addRow(tr("action_param4"),       m_actionP4Spin);
    form->addRow(tr("action_param5"),       m_actionP5Spin);
    form->addRow(tr("action_param6"),       m_actionP6Spin);
    form->addRow(tr("action_param7"),       m_actionP7Spin);
    form->addRow(tr("action_param_string"), aStrRow);

    m_tabs->addTab(w, tr("Action"));
}

void SmartScriptEditDialog::buildTargetTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    m_targetTypeCombo = makeEnumCombo(kSmartTargets, "TARGET", &smartTargetParams);
    m_targetP1Spin   = makeUIntSpin();
    m_targetP2Spin   = makeUIntSpin();
    m_targetP3Spin   = makeUIntSpin();
    m_targetP4Spin   = makeUIntSpin();

    auto* tStrRow = new QWidget;
    auto* tStrLay = new QHBoxLayout(tStrRow);
    tStrLay->setContentsMargins(0, 0, 0, 0);
    m_targetParamStringEdit = new QLineEdit;
    m_targetParamStringEdit->setMaxLength(255);
    m_targetParamStringNullChk = new QCheckBox(tr("NULL"));
    m_targetParamStringNullChk->setChecked(true);
    tStrLay->addWidget(m_targetParamStringEdit, 1);
    tStrLay->addWidget(m_targetParamStringNullChk);

    connect(m_targetParamStringNullChk, &QCheckBox::toggled, this,
            [this](bool checked) {
                m_targetParamStringEdit->setEnabled(!checked);
                if (checked) m_targetParamStringEdit->clear();
            });
    m_targetParamStringEdit->setEnabled(false);

    m_targetXSpin = makeDoubleSpin();
    m_targetYSpin = makeDoubleSpin();
    m_targetZSpin = makeDoubleSpin();
    m_targetOSpin = makeDoubleSpin();

    form->addRow(tr("target_type"),         m_targetTypeCombo);
    form->addRow(tr("target_param1"),       m_targetP1Spin);
    form->addRow(tr("target_param2"),       m_targetP2Spin);
    form->addRow(tr("target_param3"),       m_targetP3Spin);
    form->addRow(tr("target_param4"),       m_targetP4Spin);
    form->addRow(tr("target_param_string"), tStrRow);
    form->addRow(tr("target_x"),            m_targetXSpin);
    form->addRow(tr("target_y"),            m_targetYSpin);
    form->addRow(tr("target_z"),            m_targetZSpin);
    form->addRow(tr("target_o"),            m_targetOSpin);

    m_tabs->addTab(w, tr("Target"));
}

void SmartScriptEditDialog::buildCommentTab()
{
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->addWidget(new QLabel(tr("Comment (mediumtext):")));
    m_commentEdit = new QPlainTextEdit;
    lay->addWidget(m_commentEdit, 1);
    m_tabs->addTab(w, tr("Comment"));
}

void SmartScriptEditDialog::setKeyEditable(bool editable)
{
    m_keyEditable = editable;
    m_entryorguidEdit->setReadOnly(!editable);
    m_sourceTypeCombo->setEnabled(editable);
    m_idSpin  ->setReadOnly(!editable);
    m_linkSpin->setReadOnly(!editable);
    // The Difficulties column is not part of the PK on the live schema
    // (the PK is 4 cols), so it stays editable regardless.
}

void SmartScriptEditDialog::setRow(render::SmartScript const& row)
{
    m_entryorguidEdit->setText(QString::number(row.entryorguid));
    int srcIdx = m_sourceTypeCombo->findData(int(row.sourceType));
    if (srcIdx < 0)
    {
        // Tolerate exotic source_type values by appending a one-off entry.
        m_sourceTypeCombo->addItem(QString::number(int(row.sourceType)),
                                   int(row.sourceType));
        srcIdx = m_sourceTypeCombo->count() - 1;
    }
    m_sourceTypeCombo->setCurrentIndex(srcIdx);
    m_idSpin  ->setValue(int(row.id));
    m_linkSpin->setValue(int(row.link));
    m_difficultiesEdit->setText(row.difficulties);

    selectEnumValue(m_eventTypeCombo, int(row.eventType));
    m_eventPhaseMaskSpin ->setValue(int(row.eventPhaseMask));
    m_eventChanceSpin    ->setValue(int(row.eventChance));
    m_eventFlagsSpin     ->setValue(int(row.eventFlags));
    m_eventP1Spin->setValue(int(row.eventParam1));
    m_eventP2Spin->setValue(int(row.eventParam2));
    m_eventP3Spin->setValue(int(row.eventParam3));
    m_eventP4Spin->setValue(int(row.eventParam4));
    m_eventP5Spin->setValue(int(row.eventParam5));
    m_eventParamStringEdit->setText(row.eventParamString);

    selectEnumValue(m_actionTypeCombo, int(row.actionType));
    m_actionP1Spin->setValue(int(row.actionParam1));
    m_actionP2Spin->setValue(int(row.actionParam2));
    m_actionP3Spin->setValue(int(row.actionParam3));
    m_actionP4Spin->setValue(int(row.actionParam4));
    m_actionP5Spin->setValue(int(row.actionParam5));
    m_actionP6Spin->setValue(int(row.actionParam6));
    m_actionP7Spin->setValue(int(row.actionParam7));
    m_actionParamStringNullChk->setChecked(row.actionParamStringIsNull);
    m_actionParamStringEdit->setEnabled(!row.actionParamStringIsNull);
    m_actionParamStringEdit->setText(
        row.actionParamStringIsNull ? QString{} : row.actionParamString);

    selectEnumValue(m_targetTypeCombo, int(row.targetType));
    m_targetP1Spin->setValue(int(row.targetParam1));
    m_targetP2Spin->setValue(int(row.targetParam2));
    m_targetP3Spin->setValue(int(row.targetParam3));
    m_targetP4Spin->setValue(int(row.targetParam4));
    m_targetParamStringNullChk->setChecked(row.targetParamStringIsNull);
    m_targetParamStringEdit->setEnabled(!row.targetParamStringIsNull);
    m_targetParamStringEdit->setText(
        row.targetParamStringIsNull ? QString{} : row.targetParamString);
    m_targetXSpin->setValue(double(row.targetX));
    m_targetYSpin->setValue(double(row.targetY));
    m_targetZSpin->setValue(double(row.targetZ));
    m_targetOSpin->setValue(double(row.targetO));

    m_commentEdit->setPlainText(row.comment);
}

render::SmartScript SmartScriptEditDialog::rowSnapshot() const
{
    render::SmartScript row;

    bool parsed = false;
    qlonglong const v = m_entryorguidEdit->text().toLongLong(&parsed);
    row.entryorguid = parsed ? int64_t(v) : 0;
    row.sourceType  = uint8_t(m_sourceTypeCombo->currentData().toInt());
    row.id          = uint16_t(m_idSpin->value());
    row.link        = uint16_t(m_linkSpin->value());
    row.difficulties = m_difficultiesEdit->text();

    row.eventType        = uint8_t (m_eventTypeCombo->currentData().toInt());
    row.eventPhaseMask   = uint16_t(m_eventPhaseMaskSpin->value());
    row.eventChance      = uint8_t (m_eventChanceSpin->value());
    row.eventFlags       = uint16_t(m_eventFlagsSpin->value());
    row.eventParam1      = uint32_t(m_eventP1Spin->value());
    row.eventParam2      = uint32_t(m_eventP2Spin->value());
    row.eventParam3      = uint32_t(m_eventP3Spin->value());
    row.eventParam4      = uint32_t(m_eventP4Spin->value());
    row.eventParam5      = uint32_t(m_eventP5Spin->value());
    row.eventParamString = m_eventParamStringEdit->text();

    row.actionType   = uint8_t (m_actionTypeCombo->currentData().toInt());
    row.actionParam1 = uint32_t(m_actionP1Spin->value());
    row.actionParam2 = uint32_t(m_actionP2Spin->value());
    row.actionParam3 = uint32_t(m_actionP3Spin->value());
    row.actionParam4 = uint32_t(m_actionP4Spin->value());
    row.actionParam5 = uint32_t(m_actionP5Spin->value());
    row.actionParam6 = uint32_t(m_actionP6Spin->value());
    row.actionParam7 = uint32_t(m_actionP7Spin->value());
    row.actionParamStringIsNull = m_actionParamStringNullChk->isChecked();
    row.actionParamString = row.actionParamStringIsNull
        ? QString{} : m_actionParamStringEdit->text();

    row.targetType   = uint8_t (m_targetTypeCombo->currentData().toInt());
    row.targetParam1 = uint32_t(m_targetP1Spin->value());
    row.targetParam2 = uint32_t(m_targetP2Spin->value());
    row.targetParam3 = uint32_t(m_targetP3Spin->value());
    row.targetParam4 = uint32_t(m_targetP4Spin->value());
    row.targetParamStringIsNull = m_targetParamStringNullChk->isChecked();
    row.targetParamString = row.targetParamStringIsNull
        ? QString{} : m_targetParamStringEdit->text();
    row.targetX = float(m_targetXSpin->value());
    row.targetY = float(m_targetYSpin->value());
    row.targetZ = float(m_targetZSpin->value());
    row.targetO = float(m_targetOSpin->value());

    row.comment = m_commentEdit->toPlainText();

    return row;
}

} // namespace world_editor::app
