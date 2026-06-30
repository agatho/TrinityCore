#include "SpawnGroupTemplateEditDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <climits>

namespace world_editor::app
{

SpawnGroupTemplateEditDialog::SpawnGroupTemplateEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit spawn_group_template row"));
    setModal(true);

    m_groupIdSpin = new QSpinBox(this);
    m_groupIdSpin->setRange(1, INT_MAX);
    m_groupIdSpin->setAccelerated(true);

    // varchar(100) per spawn_group_template schema.
    m_groupNameEdit = new QLineEdit(this);
    m_groupNameEdit->setMaxLength(100);

    // groupFlags is a TC bitmask (currently uses bits 0x01-0x80).
    // Display in hex for legibility; tooltip enumerates the flag values.
    m_groupFlagsSpin = new QSpinBox(this);
    m_groupFlagsSpin->setRange(0, INT_MAX);
    m_groupFlagsSpin->setDisplayIntegerBase(16);
    m_groupFlagsSpin->setPrefix(QStringLiteral("0x"));
    m_groupFlagsSpin->setAccelerated(true);
    m_groupFlagsSpin->setToolTip(tr(
        "SPAWNGROUP_FLAG_* bitmask (TC core):\n"
        "  0x01 System\n"
        "  0x02 CompatibilityMode\n"
        "  0x04 ManualSpawn\n"
        "  0x08 DynamicSpawnRate\n"
        "  0x10 EscortQuestNpc\n"
        "  0x20 Despawn\n"
        "  0x40 NoDb\n"
        "  0x80 DespawnOnInstanceUnload"));

    auto* form = new QFormLayout;
    form->addRow(tr("groupId (PK)"), m_groupIdSpin);
    form->addRow(tr("groupName"),    m_groupNameEdit);
    form->addRow(tr("groupFlags"),   m_groupFlagsSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);
}

void SpawnGroupTemplateEditDialog::setGroupId(uint32_t groupId)
{
    m_groupIdSpin->setValue(int(groupId));
}

void SpawnGroupTemplateEditDialog::setGroupName(QString const& groupName)
{
    m_groupNameEdit->setText(groupName);
}

void SpawnGroupTemplateEditDialog::setGroupFlags(uint32_t groupFlags)
{
    m_groupFlagsSpin->setValue(int(groupFlags));
}

void SpawnGroupTemplateEditDialog::setKeyEditable(bool editable)
{
    m_groupIdSpin->setReadOnly(!editable);
    m_groupIdSpin->setEnabled(editable);
}

uint32_t SpawnGroupTemplateEditDialog::groupId()    const { return uint32_t(m_groupIdSpin->value()); }
QString  SpawnGroupTemplateEditDialog::groupName()  const { return m_groupNameEdit->text(); }
uint32_t SpawnGroupTemplateEditDialog::groupFlags() const { return uint32_t(m_groupFlagsSpin->value()); }

} // namespace world_editor::app
