#include "AreatriggerPropertiesDock.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace world_editor::app
{

AreatriggerPropertiesDock::AreatriggerPropertiesDock(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);

    m_headerLabel = new QLabel(tr("(no areatrigger selected)"), this);
    m_headerLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    outer->addWidget(m_headerLabel);

    m_shapeReadOnly = new QLabel(QString{}, this);
    m_shapeReadOnly->setStyleSheet(QStringLiteral("color: #8af; font-family: monospace;"));
    m_shapeReadOnly->setWordWrap(true);
    outer->addWidget(m_shapeReadOnly);

    auto* form = new QFormLayout;

    m_createPropsSpin = new QSpinBox(this);
    m_createPropsSpin->setRange(0, 0x7fffffff);
    form->addRow(tr("CreatePropertiesId"), m_createPropsSpin);

    m_isCustomSpin = new QSpinBox(this);
    m_isCustomSpin->setRange(0, 1);
    form->addRow(tr("IsCustom"), m_isCustomSpin);

    m_spawnDiffEdit = new QLineEdit(this);
    m_spawnDiffEdit->setMaxLength(100);
    form->addRow(tr("SpawnDifficulties"), m_spawnDiffEdit);

    auto makePos = [&](char const* suffix) {
        auto* sp = new QDoubleSpinBox(this);
        sp->setRange(-20000.0, 20000.0);
        sp->setDecimals(3);
        sp->setSuffix(QString::fromLatin1(suffix));
        return sp;
    };
    m_posXSpin = makePos(" X");
    m_posYSpin = makePos(" Y");
    m_posZSpin = makePos(" Z");
    form->addRow(tr("PosX (north)"), m_posXSpin);
    form->addRow(tr("PosY (west)"),  m_posYSpin);
    form->addRow(tr("PosZ"),         m_posZSpin);

    m_orientSpin = new QDoubleSpinBox(this);
    m_orientSpin->setRange(-7.0, 7.0);
    m_orientSpin->setDecimals(4);
    m_orientSpin->setSuffix(QStringLiteral(" rad"));
    form->addRow(tr("Orientation"), m_orientSpin);

    m_phaseUseFlagsSpin = new QSpinBox(this); m_phaseUseFlagsSpin->setRange(0, 255);
    m_phaseIdSpin       = new QSpinBox(this); m_phaseIdSpin->setRange(0, 0x7fffffff);
    m_phaseGroupSpin    = new QSpinBox(this); m_phaseGroupSpin->setRange(0, 0x7fffffff);
    form->addRow(tr("PhaseUseFlags"), m_phaseUseFlagsSpin);
    form->addRow(tr("PhaseId"),       m_phaseIdSpin);
    form->addRow(tr("PhaseGroup"),    m_phaseGroupSpin);

    m_scriptNameEdit = new QLineEdit(this);
    m_scriptNameEdit->setMaxLength(64);
    form->addRow(tr("ScriptName"), m_scriptNameEdit);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setMaxLength(255);
    form->addRow(tr("Comment"), m_commentEdit);

    m_verifiedBuildSpin = new QSpinBox(this);
    m_verifiedBuildSpin->setRange(0, 0x7fffffff);
    form->addRow(tr("VerifiedBuild"), m_verifiedBuildSpin);

    outer->addLayout(form);

    auto* footer = new QHBoxLayout;
    m_deleteButton = new QPushButton(tr("Delete areatrigger"), this);
    m_deleteButton->setEnabled(false);
    m_revertButton = new QPushButton(tr("Revert all"), this);
    m_revertButton->setEnabled(false);
    m_commitButton = new QPushButton(tr("Commit..."), this);
    m_commitButton->setEnabled(false);
    footer->addWidget(m_deleteButton);
    footer->addStretch(1);
    footer->addWidget(m_revertButton);
    footer->addWidget(m_commitButton);
    outer->addLayout(footer);

    m_pendingLabel = new QLabel(tr("pending: 0"), this);
    outer->addWidget(m_pendingLabel);
    outer->addStretch(1);

    connect(m_createPropsSpin,   QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_isCustomSpin,      QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_spawnDiffEdit,     &QLineEdit::editingFinished,
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_posXSpin,          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_posYSpin,          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_posZSpin,          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_orientSpin,        QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_phaseUseFlagsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_phaseIdSpin,       QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_phaseGroupSpin,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_scriptNameEdit,    &QLineEdit::editingFinished,
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_commentEdit,       &QLineEdit::editingFinished,
            this, &AreatriggerPropertiesDock::onFormChanged);
    connect(m_verifiedBuildSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AreatriggerPropertiesDock::onFormChanged);

    connect(m_deleteButton, &QPushButton::clicked,
            this, &AreatriggerPropertiesDock::deleteAreatriggerRequested);
    connect(m_revertButton, &QPushButton::clicked,
            this, &AreatriggerPropertiesDock::revertRequested);
    connect(m_commitButton, &QPushButton::clicked,
            this, &AreatriggerPropertiesDock::commitRequested);
}

QString AreatriggerPropertiesDock::shapeName(uint8_t shape)
{
    switch (shape)
    {
    case 0: return QStringLiteral("Sphere");
    case 1: return QStringLiteral("Box");
    case 2: return QStringLiteral("Polygon");
    case 3: return QStringLiteral("Cylinder");
    case 4: return QStringLiteral("Disk");
    default: return QStringLiteral("Unknown");
    }
}

void AreatriggerPropertiesDock::setAreatrigger(int index, render::Areatrigger const& a)
{
    m_index    = index;
    m_baseline = a;
    applyToForm(a);
    m_headerLabel->setText(tr("selected: SpawnId=%1  map=%2")
        .arg(a.spawnId).arg(a.mapId));
    m_headerLabel->setStyleSheet(QStringLiteral("color: #f9b34a;"));
    m_shapeReadOnly->setText(tr("shape=%1 (%2)  data=[%3 %4 %5 %6 %7 %8 %9 %10]")
        .arg(shapeName(a.shape)).arg(int(a.shape))
        .arg(a.shapeData[0], 0, 'f', 2).arg(a.shapeData[1], 0, 'f', 2)
        .arg(a.shapeData[2], 0, 'f', 2).arg(a.shapeData[3], 0, 'f', 2)
        .arg(a.shapeData[4], 0, 'f', 2).arg(a.shapeData[5], 0, 'f', 2)
        .arg(a.shapeData[6], 0, 'f', 2).arg(a.shapeData[7], 0, 'f', 2));
    m_deleteButton->setEnabled(true);
}

void AreatriggerPropertiesDock::clear()
{
    m_index    = -1;
    m_baseline = render::Areatrigger{};
    applyToForm(m_baseline);
    m_headerLabel->setText(tr("(no areatrigger selected)"));
    m_headerLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    m_shapeReadOnly->clear();
    m_deleteButton->setEnabled(false);
}

void AreatriggerPropertiesDock::setPendingCount(size_t count)
{
    m_pendingLabel->setText(tr("pending: %1").arg(count));
    m_commitButton->setEnabled(count > 0);
    m_revertButton->setEnabled(count > 0);
}

void AreatriggerPropertiesDock::applyToForm(render::Areatrigger const& a)
{
    m_suppress = true;
    m_createPropsSpin->setValue(int(a.createPropsId));
    m_isCustomSpin->setValue(int(a.isCustom));
    m_spawnDiffEdit->setText(a.spawnDifficulties);
    m_posXSpin->setValue(a.x);
    m_posYSpin->setValue(a.y);
    m_posZSpin->setValue(a.z);
    m_orientSpin->setValue(a.orientation);
    m_phaseUseFlagsSpin->setValue(int(a.phaseUseFlags));
    m_phaseIdSpin->setValue(int(a.phaseId));
    m_phaseGroupSpin->setValue(int(a.phaseGroup));
    m_scriptNameEdit->setText(a.scriptName);
    m_commentEdit->setText(a.comment);
    m_verifiedBuildSpin->setValue(int(a.verifiedBuild));
    m_suppress = false;
}

render::Areatrigger AreatriggerPropertiesDock::snapshotFromForm() const
{
    render::Areatrigger a = m_baseline;
    a.createPropsId     = uint32_t(m_createPropsSpin->value());
    a.isCustom          = uint8_t(m_isCustomSpin->value());
    a.spawnDifficulties = m_spawnDiffEdit->text();
    a.x                 = float(m_posXSpin->value());
    a.y                 = float(m_posYSpin->value());
    a.z                 = float(m_posZSpin->value());
    a.orientation       = float(m_orientSpin->value());
    a.phaseUseFlags     = uint8_t(m_phaseUseFlagsSpin->value());
    a.phaseId           = uint32_t(m_phaseIdSpin->value());
    a.phaseGroup        = uint32_t(m_phaseGroupSpin->value());
    a.scriptName        = m_scriptNameEdit->text();
    a.comment           = m_commentEdit->text();
    a.verifiedBuild     = uint32_t(m_verifiedBuildSpin->value());
    return a;
}

void AreatriggerPropertiesDock::onFormChanged()
{
    if (m_suppress || m_index < 0) return;
    emit areatriggerEdited(snapshotFromForm());
}

} // namespace world_editor::app
