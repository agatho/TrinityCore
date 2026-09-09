#include "SpawnCloneDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace world_editor::app
{

SpawnCloneDialog::SpawnCloneDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Clone spawn"));
    setModal(true);

    auto* form = new QFormLayout;

    m_count = new QSpinBox(this);
    m_count->setRange(1, 200);
    m_count->setValue(5);
    form->addRow(tr("Count"), m_count);

    m_pattern = new QComboBox(this);
    // Keep order in sync with the patternIdx contract documented in the header.
    m_pattern->addItem(tr("Random scatter"));
    m_pattern->addItem(tr("Ring"));
    m_pattern->addItem(tr("Grid"));
    form->addRow(tr("Pattern"), m_pattern);

    m_radius = new QDoubleSpinBox(this);
    m_radius->setRange(1.0, 200.0);
    m_radius->setDecimals(2);
    m_radius->setSingleStep(1.0);
    m_radius->setValue(15.0);
    m_radius->setSuffix(tr(" yd"));
    form->addRow(tr("Radius / spacing"), m_radius);

    m_snap = new QCheckBox(tr("Snap clones to ground"), this);
    m_snap->setChecked(true);
    form->addRow(QString(), m_snap);

    m_preserveOri = new QCheckBox(tr("Preserve source orientation"), this);
    m_preserveOri->setChecked(true);
    form->addRow(QString(), m_preserveOri);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SpawnCloneDialog::onApply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(buttons);
}

void SpawnCloneDialog::onApply()
{
    emit cloneRequested(m_count->value(),
                        m_pattern->currentIndex(),
                        static_cast<float>(m_radius->value()),
                        m_snap->isChecked(),
                        m_preserveOri->isChecked());
    accept();
}

} // namespace world_editor::app
