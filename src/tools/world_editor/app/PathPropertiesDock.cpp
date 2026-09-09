#include "PathPropertiesDock.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace world_editor::app
{

PathPropertiesDock::PathPropertiesDock(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);

    m_pathHeaderLabel = new QLabel(tr("(no path selected)"), this);
    m_pathHeaderLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    outer->addWidget(m_pathHeaderLabel);

    auto* form = new QFormLayout;
    m_moveTypeSpin = new QSpinBox(this); m_moveTypeSpin->setRange(0, 255);
    m_flagsSpin    = new QSpinBox(this); m_flagsSpin->setRange(0, 255);
    m_velocitySpin = new QDoubleSpinBox(this);
    m_velocitySpin->setRange(0.0, 50.0); m_velocitySpin->setDecimals(3);
    m_velocitySpin->setSuffix(QStringLiteral(" y/s"));
    m_commentEdit  = new QLineEdit(this);
    m_commentEdit->setMaxLength(255);
    form->addRow(tr("MoveType"), m_moveTypeSpin);
    form->addRow(tr("Flags"),    m_flagsSpin);
    form->addRow(tr("Velocity"), m_velocitySpin);
    form->addRow(tr("Comment"),  m_commentEdit);
    outer->addLayout(form);

    m_nodeTable = new QTableWidget(this);
    m_nodeTable->setColumnCount(6);
    m_nodeTable->setHorizontalHeaderLabels(
        { tr("NodeId"), tr("X"), tr("Y"), tr("Z"), tr("Orientation"), tr("Delay") });
    m_nodeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_nodeTable->verticalHeader()->setVisible(false);
    outer->addWidget(m_nodeTable, 1);

    auto* footer = new QHBoxLayout;
    m_deleteButton = new QPushButton(tr("Delete path"), this);
    m_deleteButton->setEnabled(false);
    m_assignButton = new QPushButton(tr("Assign to selected spawn"), this);
    m_assignButton->setEnabled(false);
    m_revertButton = new QPushButton(tr("Revert all"), this);
    m_commitButton = new QPushButton(tr("Commit..."), this);
    m_commitButton->setEnabled(false);
    footer->addWidget(m_deleteButton);
    footer->addWidget(m_assignButton);
    footer->addStretch(1);
    footer->addWidget(m_revertButton);
    footer->addWidget(m_commitButton);
    outer->addLayout(footer);

    m_pendingLabel = new QLabel(tr("pending: 0"), this);
    outer->addWidget(m_pendingLabel);

    connect(m_moveTypeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PathPropertiesDock::onHeaderChanged);
    connect(m_flagsSpin,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PathPropertiesDock::onHeaderChanged);
    connect(m_velocitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PathPropertiesDock::onHeaderChanged);
    connect(m_commentEdit,  &QLineEdit::editingFinished,
            this, &PathPropertiesDock::onHeaderChanged);
    connect(m_nodeTable,    &QTableWidget::cellChanged,
            this, &PathPropertiesDock::onNodeChanged);

    connect(m_deleteButton, &QPushButton::clicked, this, &PathPropertiesDock::deletePathRequested);
    connect(m_assignButton, &QPushButton::clicked, this, &PathPropertiesDock::assignToSelectedSpawnRequested);
    connect(m_commitButton, &QPushButton::clicked, this, &PathPropertiesDock::commitRequested);
    connect(m_revertButton, &QPushButton::clicked, this, &PathPropertiesDock::revertRequested);
}

void PathPropertiesDock::setPath(int index, render::Path const& p)
{
    m_index    = index;
    m_baseline = p;
    applyToForm(p);
    m_pathHeaderLabel->setText(tr("selected: PathId=%1  nodes=%2")
        .arg(p.pathId).arg(p.nodes.size()));
    m_pathHeaderLabel->setStyleSheet(QStringLiteral("color: #f9b34a;"));
    m_deleteButton->setEnabled(true);
    m_assignButton->setEnabled(true);
}

void PathPropertiesDock::clear()
{
    m_index = -1;
    m_baseline = render::Path{};
    applyToForm(m_baseline);
    m_pathHeaderLabel->setText(tr("(no path selected)"));
    m_pathHeaderLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    m_deleteButton->setEnabled(false);
    m_assignButton->setEnabled(false);
}

void PathPropertiesDock::setPendingCount(size_t count)
{
    m_pendingLabel->setText(tr("pending: %1").arg(count));
    m_commitButton->setEnabled(count > 0);
    m_revertButton->setEnabled(count > 0);
}

void PathPropertiesDock::applyToForm(render::Path const& p)
{
    m_suppress = true;
    m_moveTypeSpin->setValue(int(p.moveType));
    m_flagsSpin->setValue(int(p.flags));
    m_velocitySpin->setValue(p.velocity);
    m_commentEdit->setText(p.comment);
    m_nodeTable->setRowCount(int(p.nodes.size()));
    for (size_t i = 0; i < p.nodes.size(); ++i)
    {
        auto const& n = p.nodes[i];
        auto makeRO = [](QString const& s) {
            auto* it = new QTableWidgetItem(s);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };
        auto makeRW = [](QString const& s) {
            return new QTableWidgetItem(s);
        };
        m_nodeTable->setItem(int(i), 0, makeRO(QString::number(n.nodeId)));
        m_nodeTable->setItem(int(i), 1, makeRO(QString::number(n.x, 'f', 3)));
        m_nodeTable->setItem(int(i), 2, makeRO(QString::number(n.y, 'f', 3)));
        m_nodeTable->setItem(int(i), 3, makeRO(QString::number(n.z, 'f', 3)));
        m_nodeTable->setItem(int(i), 4, makeRW(QString::number(n.orientation, 'f', 4)));
        m_nodeTable->setItem(int(i), 5, makeRW(QString::number(n.delay)));
    }
    m_suppress = false;
}

render::Path PathPropertiesDock::snapshotFromForm() const
{
    render::Path p = m_baseline;
    p.moveType = uint8_t(m_moveTypeSpin->value());
    p.flags    = uint8_t(m_flagsSpin->value());
    p.velocity = float(m_velocitySpin->value());
    p.comment  = m_commentEdit->text();
    int const rowN = std::min(m_nodeTable->rowCount(), int(p.nodes.size()));
    for (int i = 0; i < rowN; ++i)
    {
        bool okO = false, okD = false;
        float const ori = m_nodeTable->item(i, 4)->text().toFloat(&okO);
        uint32_t const dly = m_nodeTable->item(i, 5)->text().toUInt(&okD);
        if (okO) p.nodes[i].orientation = ori;
        if (okD) p.nodes[i].delay       = dly;
    }
    return p;
}

void PathPropertiesDock::onHeaderChanged()
{
    if (m_suppress || m_index < 0) return;
    emit pathEdited(snapshotFromForm());
}

void PathPropertiesDock::onNodeChanged(int /*row*/, int /*col*/)
{
    if (m_suppress || m_index < 0) return;
    emit pathEdited(snapshotFromForm());
}

} // namespace world_editor::app
