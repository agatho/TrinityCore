#include "ConditionEditDialog.h"

#include "ConditionEnumTables.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <climits>
#include <cstddef>

namespace world_editor::app
{

namespace
{

QSpinBox* makeIntSpin(int low, int high)
{
    auto* s = new QSpinBox;
    s->setRange(low, high);
    s->setSingleStep(1);
    s->setAccelerated(true);
    return s;
}

QSpinBox* makeUIntSpin(int max = INT_MAX)
{
    return makeIntSpin(0, max);
}

// Build a QComboBox seeded from a ConditionEnumEntry table.  The combo
// stores the numeric value in userData and renders "VAL  NAME".
QComboBox* makeEnumCombo(ConditionEnumEntry const* table, std::size_t n,
                         char const* qualifier)
{
    auto* c = new QComboBox;
    for (std::size_t i = 0; i < n; ++i)
    {
        QString const text = QString::asprintf("%3d  %s_%s",
            table[i].value, qualifier, table[i].name);
        c->addItem(text, table[i].value);
    }
    c->setMaxVisibleItems(20);
    return c;
}

// Select the combo row whose userData matches `value`.  If absent,
// append a synthetic "(unknown N)" row so exotic / future-TC values
// round-trip safely.  Idempotent across repeated opens.
void selectEnumValue(QComboBox* c, int value)
{
    int idx = c->findData(value);
    if (idx < 0)
    {
        QString const text = QString::asprintf("%3d  (unknown %d)", value, value);
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

ConditionEditDialog::ConditionEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit conditions row"));
    setModal(true);
    resize(560, 520);
    buildUi();
}

void ConditionEditDialog::buildUi()
{
    m_tabs = new QTabWidget(this);

    buildKeyTab();
    buildBodyTab();

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_tabs, 1);
    outer->addWidget(buttons);
}

void ConditionEditDialog::buildKeyTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    m_sourceTypeCombo = makeEnumCombo(kConditionSourceTypes,
        std::size(kConditionSourceTypes), "CONDITION_SOURCE_TYPE");
    m_sourceTypeCombo->setToolTip(tr(
        "ConditionSourceType.  Defines what SourceGroup/SourceEntry/SourceId mean."));

    m_sourceGroupSpin  = makeUIntSpin();
    m_sourceEntrySpin  = makeIntSpin(INT_MIN, INT_MAX);
    m_sourceIdSpin     = makeIntSpin(INT_MIN, INT_MAX);
    m_elseGroupSpin    = makeUIntSpin();

    m_condTypeCombo = makeEnumCombo(kConditionTypes,
        std::size(kConditionTypes), "CONDITION");
    m_condTypeCombo->setToolTip(tr(
        "ConditionTypeOrReference.  Negative = reference id; positive = condition kind."));

    m_condTargetSpin   = makeUIntSpin(255);
    m_condValue1Spin   = makeUIntSpin();
    m_condValue2Spin   = makeUIntSpin();
    m_condValue3Spin   = makeUIntSpin();
    m_condStringValue1Edit = new QLineEdit;
    m_condStringValue1Edit->setMaxLength(255);
    m_condStringValue1Edit->setToolTip(tr(
        "Part of the composite PK.  Mutating this column = delete+insert at commit."));

    form->addRow(tr("SourceTypeOrReferenceId"),  m_sourceTypeCombo);
    form->addRow(tr("SourceGroup"),              m_sourceGroupSpin);
    form->addRow(tr("SourceEntry"),              m_sourceEntrySpin);
    form->addRow(tr("SourceId"),                 m_sourceIdSpin);
    form->addRow(tr("ElseGroup"),                m_elseGroupSpin);
    form->addRow(tr("ConditionTypeOrReference"), m_condTypeCombo);
    form->addRow(tr("ConditionTarget"),          m_condTargetSpin);
    form->addRow(tr("ConditionValue1"),          m_condValue1Spin);
    form->addRow(tr("ConditionValue2"),          m_condValue2Spin);
    form->addRow(tr("ConditionValue3"),          m_condValue3Spin);
    form->addRow(tr("ConditionStringValue1"),    m_condStringValue1Edit);

    m_tabs->addTab(w, tr("Key"));
}

void ConditionEditDialog::buildBodyTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    m_negativeCheck = new QCheckBox(tr("NegativeCondition"));
    m_negativeCheck->setToolTip(tr("Invert the result (!cond)."));

    m_errorTypeSpin   = makeUIntSpin();
    m_errorTextIdSpin = makeUIntSpin();
    m_scriptNameEdit  = new QLineEdit;
    m_scriptNameEdit->setMaxLength(64);

    m_commentEdit = new QPlainTextEdit;
    m_commentEdit->setPlaceholderText(tr("Free-form note (mediumtext)."));

    form->addRow(QString(),               m_negativeCheck);
    form->addRow(tr("ErrorType"),         m_errorTypeSpin);
    form->addRow(tr("ErrorTextId"),       m_errorTextIdSpin);
    form->addRow(tr("ScriptName"),        m_scriptNameEdit);
    form->addRow(tr("Comment"),           m_commentEdit);

    m_tabs->addTab(w, tr("Body"));
}

void ConditionEditDialog::setKeyEditable(bool editable)
{
    m_keyEditable = editable;
    m_sourceTypeCombo     ->setEnabled (editable);
    m_sourceGroupSpin     ->setReadOnly(!editable);
    m_sourceEntrySpin     ->setReadOnly(!editable);
    m_sourceIdSpin        ->setReadOnly(!editable);
    m_elseGroupSpin       ->setReadOnly(!editable);
    m_condTypeCombo       ->setEnabled (editable);
    m_condTargetSpin      ->setReadOnly(!editable);
    m_condValue1Spin      ->setReadOnly(!editable);
    m_condValue2Spin      ->setReadOnly(!editable);
    m_condValue3Spin      ->setReadOnly(!editable);
    m_condStringValue1Edit->setReadOnly(!editable);
}

void ConditionEditDialog::setCondition(render::Condition const& row)
{
    selectEnumValue(m_sourceTypeCombo, int(row.sourceTypeOrReferenceId));
    m_sourceGroupSpin     ->setValue(int(row.sourceGroup));
    m_sourceEntrySpin     ->setValue(int(row.sourceEntry));
    m_sourceIdSpin        ->setValue(int(row.sourceId));
    m_elseGroupSpin       ->setValue(int(row.elseGroup));
    selectEnumValue(m_condTypeCombo, int(row.conditionTypeOrReference));
    m_condTargetSpin      ->setValue(int(row.conditionTarget));
    m_condValue1Spin      ->setValue(int(row.conditionValue1));
    m_condValue2Spin      ->setValue(int(row.conditionValue2));
    m_condValue3Spin      ->setValue(int(row.conditionValue3));
    m_condStringValue1Edit->setText(row.conditionStringValue1);

    m_negativeCheck   ->setChecked(row.negativeCondition != 0);
    m_errorTypeSpin   ->setValue(int(row.errorType));
    m_errorTextIdSpin ->setValue(int(row.errorTextId));
    m_scriptNameEdit  ->setText(row.scriptName);
    m_commentEdit     ->setPlainText(row.comment);
}

render::Condition ConditionEditDialog::condition() const
{
    render::Condition row;

    row.sourceTypeOrReferenceId  = int32_t (m_sourceTypeCombo->currentData().toInt());
    row.sourceGroup              = uint32_t(m_sourceGroupSpin->value());
    row.sourceEntry              = int32_t (m_sourceEntrySpin->value());
    row.sourceId                 = int32_t (m_sourceIdSpin   ->value());
    row.elseGroup                = uint32_t(m_elseGroupSpin  ->value());
    row.conditionTypeOrReference = int32_t (m_condTypeCombo  ->currentData().toInt());
    row.conditionTarget          = uint8_t (m_condTargetSpin ->value());
    row.conditionValue1          = uint32_t(m_condValue1Spin ->value());
    row.conditionValue2          = uint32_t(m_condValue2Spin ->value());
    row.conditionValue3          = uint32_t(m_condValue3Spin ->value());
    row.conditionStringValue1    = m_condStringValue1Edit->text();

    row.negativeCondition        = m_negativeCheck   ->isChecked() ? 1 : 0;
    row.errorType                = uint32_t(m_errorTypeSpin  ->value());
    row.errorTextId              = uint32_t(m_errorTextIdSpin->value());
    row.scriptName               = m_scriptNameEdit  ->text();
    row.comment                  = m_commentEdit     ->toPlainText();

    return row;
}

} // namespace world_editor::app
