#include "GraveyardPropertiesDock.h"

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

GraveyardPropertiesDock::GraveyardPropertiesDock(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);

    m_headerLabel = new QLabel(tr("(no graveyard selected)"), this);
    m_headerLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    outer->addWidget(m_headerLabel);

    auto* form = new QFormLayout;

    m_mapIdSpin = new QSpinBox(this);
    m_mapIdSpin->setRange(0, 0x7fffffff);
    form->addRow(tr("MapID"), m_mapIdSpin);

    auto makePos = [&](char const* suffix) {
        auto* sp = new QDoubleSpinBox(this);
        sp->setRange(-20000.0, 20000.0);
        sp->setDecimals(3);
        sp->setSuffix(QString::fromLatin1(suffix));
        return sp;
    };
    m_locXSpin = makePos(" X");
    m_locYSpin = makePos(" Y");
    m_locZSpin = makePos(" Z");
    form->addRow(tr("LocX (north)"), m_locXSpin);
    form->addRow(tr("LocY (west)"),  m_locYSpin);
    form->addRow(tr("LocZ"),         m_locZSpin);

    m_facingSpin = new QDoubleSpinBox(this);
    m_facingSpin->setRange(-7.0, 7.0);
    m_facingSpin->setDecimals(4);
    m_facingSpin->setSuffix(QStringLiteral(" rad"));
    form->addRow(tr("Facing"), m_facingSpin);

    m_transportSpin = new QSpinBox(this);
    m_transportSpin->setRange(0, 0x7fffffff);
    m_transportSpin->setToolTip(tr("0 = NULL (no transport)"));
    form->addRow(tr("TransportSpawnId"), m_transportSpin);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setMaxLength(255);
    form->addRow(tr("Comment"), m_commentEdit);

    outer->addLayout(form);

    auto* footer = new QHBoxLayout;
    m_deleteButton = new QPushButton(tr("Delete graveyard"), this);
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

    connect(m_mapIdSpin,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &GraveyardPropertiesDock::onFormChanged);
    connect(m_locXSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraveyardPropertiesDock::onFormChanged);
    connect(m_locYSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraveyardPropertiesDock::onFormChanged);
    connect(m_locZSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraveyardPropertiesDock::onFormChanged);
    connect(m_facingSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraveyardPropertiesDock::onFormChanged);
    connect(m_transportSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &GraveyardPropertiesDock::onFormChanged);
    connect(m_commentEdit,  &QLineEdit::editingFinished,
            this, &GraveyardPropertiesDock::onFormChanged);

    connect(m_deleteButton, &QPushButton::clicked,
            this, &GraveyardPropertiesDock::deleteGraveyardRequested);
    connect(m_revertButton, &QPushButton::clicked,
            this, &GraveyardPropertiesDock::revertRequested);
    connect(m_commitButton, &QPushButton::clicked,
            this, &GraveyardPropertiesDock::commitRequested);
}

void GraveyardPropertiesDock::setGraveyard(int index, render::Graveyard const& g)
{
    m_index    = index;
    m_baseline = g;
    applyToForm(g);
    m_headerLabel->setText(tr("selected: ID=%1  map=%2").arg(g.id).arg(g.mapId));
    m_headerLabel->setStyleSheet(QStringLiteral("color: #f9b34a;"));
    m_deleteButton->setEnabled(true);
}

void GraveyardPropertiesDock::clear()
{
    m_index = -1;
    m_baseline = render::Graveyard{};
    applyToForm(m_baseline);
    m_headerLabel->setText(tr("(no graveyard selected)"));
    m_headerLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    m_deleteButton->setEnabled(false);
}

void GraveyardPropertiesDock::setPendingCount(size_t count)
{
    m_pendingLabel->setText(tr("pending: %1").arg(count));
    m_commitButton->setEnabled(count > 0);
    m_revertButton->setEnabled(count > 0);
}

void GraveyardPropertiesDock::applyToForm(render::Graveyard const& g)
{
    m_suppress = true;
    m_mapIdSpin->setValue(int(g.mapId));
    m_locXSpin->setValue(g.x);
    m_locYSpin->setValue(g.y);
    m_locZSpin->setValue(g.z);
    m_facingSpin->setValue(g.facing);
    // QSpinBox is signed; transports rarely exceed 2^31 for live data.
    m_transportSpin->setValue(int(std::min<uint64_t>(g.transportSpawnId, 0x7fffffff)));
    m_commentEdit->setText(g.comment);
    m_suppress = false;
}

render::Graveyard GraveyardPropertiesDock::snapshotFromForm() const
{
    render::Graveyard g = m_baseline;
    g.mapId            = uint32_t(m_mapIdSpin->value());
    g.x                = float(m_locXSpin->value());
    g.y                = float(m_locYSpin->value());
    g.z                = float(m_locZSpin->value());
    g.facing           = float(m_facingSpin->value());
    g.transportSpawnId = uint64_t(m_transportSpin->value());
    g.comment          = m_commentEdit->text();
    return g;
}

void GraveyardPropertiesDock::onFormChanged()
{
    if (m_suppress || m_index < 0) return;
    emit graveyardEdited(snapshotFromForm());
}

} // namespace world_editor::app
