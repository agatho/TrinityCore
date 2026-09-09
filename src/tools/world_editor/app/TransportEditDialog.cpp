#include "TransportEditDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

namespace world_editor::app
{

TransportEditDialog::TransportEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit transport row"));
    setModal(true);
    resize(520, 480);

    m_guidEdit = new QLineEdit(this);
    // bigint unsigned -- use QLineEdit + 64-bit-digit validator so we
    // don't truncate to INT_MAX.  19 digits = floor(log10(2^63 - 1)).
    m_guidEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,19}$")), this));
    m_guidEdit->setText(QStringLiteral("0"));

    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, 0x7fffffff);

    m_nameEdit = new QPlainTextEdit(this);
    m_nameEdit->setPlaceholderText(tr("display name (mediumtext, nullable)"));

    m_phaseUseFlagsSpin = new QSpinBox(this);
    m_phaseUseFlagsSpin->setRange(0, 255);
    m_phaseIdSpin = new QSpinBox(this);
    m_phaseIdSpin->setRange(0, 0x7fffffff);
    m_phaseGroupSpin = new QSpinBox(this);
    m_phaseGroupSpin->setRange(0, 0x7fffffff);

    m_scriptNameEdit = new QLineEdit(this);
    m_scriptNameEdit->setMaxLength(64);
    m_scriptNameEdit->setPlaceholderText(tr("ScriptName (varchar(64))"));

    auto* form = new QFormLayout;
    form->addRow(tr("guid (PK)"),       m_guidEdit);
    form->addRow(tr("entry (UNIQUE)"),  m_entrySpin);
    form->addRow(tr("name"),            m_nameEdit);
    form->addRow(tr("phaseUseFlags"),   m_phaseUseFlagsSpin);
    form->addRow(tr("phaseid"),         m_phaseIdSpin);
    form->addRow(tr("phasegroup"),      m_phaseGroupSpin);
    form->addRow(tr("ScriptName"),      m_scriptNameEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);
}

void TransportEditDialog::setRow(TransportRow const& r)
{
    m_guidEdit->setText(QString::number(qlonglong(r.guid)));
    m_entrySpin->setValue(int(r.entry));
    m_nameEdit->setPlainText(r.name);
    m_phaseUseFlagsSpin->setValue(int(r.phaseUseFlags));
    m_phaseIdSpin->setValue(int(r.phaseId));
    m_phaseGroupSpin->setValue(int(r.phaseGroup));
    m_scriptNameEdit->setText(r.scriptName);
}

TransportRow TransportEditDialog::rowSnapshot() const
{
    TransportRow r;
    r.guid          = m_guidEdit->text().toLongLong();
    r.entry         = uint32_t(m_entrySpin->value());
    r.name          = m_nameEdit->toPlainText();
    r.phaseUseFlags = uint8_t(m_phaseUseFlagsSpin->value());
    r.phaseId       = int32_t(m_phaseIdSpin->value());
    r.phaseGroup    = int32_t(m_phaseGroupSpin->value());
    r.scriptName    = m_scriptNameEdit->text();
    return r;
}

void TransportEditDialog::setKeyEditable(bool editable)
{
    m_guidEdit->setReadOnly(!editable);
    m_entrySpin->setReadOnly(!editable);
    if (!editable)
        m_guidEdit->setToolTip(tr("guid + entry are part of identity; Remove + Add to re-key"));
}

} // namespace world_editor::app
