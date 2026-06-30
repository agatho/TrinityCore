#include "PoolTemplateEditDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <climits>

namespace world_editor::app
{

PoolTemplateEditDialog::PoolTemplateEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit pool_template row"));
    setModal(true);

    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(1, INT_MAX);
    m_entrySpin->setAccelerated(true);

    // max_limit is unsigned; 0 means "no limit" in TC core.
    m_maxLimitSpin = new QSpinBox(this);
    m_maxLimitSpin->setRange(0, INT_MAX);
    m_maxLimitSpin->setAccelerated(true);
    m_maxLimitSpin->setSpecialValueText(tr("0 (no limit)"));

    m_descEdit = new QLineEdit(this);
    m_descEdit->setMaxLength(255); // pool_template.description is varchar(255).

    auto* form = new QFormLayout;
    form->addRow(tr("entry (PK)"),  m_entrySpin);
    form->addRow(tr("max_limit"),   m_maxLimitSpin);
    form->addRow(tr("description"), m_descEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);
}

void PoolTemplateEditDialog::setEntry(uint32_t entry)
{
    m_entrySpin->setValue(int(entry));
}

void PoolTemplateEditDialog::setMaxLimit(uint32_t maxLimit)
{
    m_maxLimitSpin->setValue(int(maxLimit));
}

void PoolTemplateEditDialog::setDescription(QString const& description)
{
    m_descEdit->setText(description);
}

void PoolTemplateEditDialog::setKeyEditable(bool editable)
{
    m_entrySpin->setReadOnly(!editable);
    m_entrySpin->setEnabled(editable);
}

uint32_t PoolTemplateEditDialog::entry() const       { return uint32_t(m_entrySpin->value()); }
uint32_t PoolTemplateEditDialog::maxLimit() const    { return uint32_t(m_maxLimitSpin->value()); }
QString  PoolTemplateEditDialog::description() const { return m_descEdit->text(); }

} // namespace world_editor::app
