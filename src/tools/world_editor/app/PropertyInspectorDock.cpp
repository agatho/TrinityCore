#include "PropertyInspectorDock.h"

#include "AreatriggerPropertiesDock.h"
#include "GraveyardPropertiesDock.h"
#include "PathPropertiesDock.h"

#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace world_editor::app
{

namespace
{
// Build a simple "open the modal editor" tab body so Pool / SpawnGroup
// have a discoverable surface even though the actual editor is dialog-
// based.  The button click is wired by the caller.
QWidget* makeLaunchTab(QString const& title, QString const& description,
                       QString const& buttonLabel, QPushButton*& outBtn,
                       QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    auto* hdr = new QLabel(QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped()), w);
    auto* desc = new QLabel(description, w);
    desc->setWordWrap(true);
    outBtn = new QPushButton(buttonLabel, w);
    lay->addWidget(hdr);
    lay->addWidget(desc);
    lay->addWidget(outBtn);
    lay->addStretch(1);
    return w;
}
} // namespace

PropertyInspectorDock::PropertyInspectorDock(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabPosition(QTabWidget::North);
    root->addWidget(m_tabs);

    m_pathDock = new PathPropertiesDock(m_tabs);
    m_atrDock  = new AreatriggerPropertiesDock(m_tabs);
    m_gyDock   = new GraveyardPropertiesDock(m_tabs);

    m_tabs->insertTab(int(Tab::Path),        m_pathDock, tr("Path"));
    m_tabs->insertTab(int(Tab::Areatrigger), m_atrDock,  tr("Areatrigger"));
    m_tabs->insertTab(int(Tab::Graveyard),   m_gyDock,   tr("Graveyard"));

    QWidget* poolTab = makeLaunchTab(
        tr("Pool template"),
        tr("Pool membership is managed through the Groups / Pools modal editor.  "
           "Click to open it.  Switches to this tab automatically when a pooled "
           "spawn is selected."),
        tr("Open Groups / Pools editor..."),
        m_poolBtn, m_tabs);
    m_tabs->insertTab(int(Tab::Pool), poolTab, tr("Pool"));

    QWidget* sgTab = makeLaunchTab(
        tr("Spawn group"),
        tr("Spawn-group membership is managed through the Spawn group template "
           "modal editor.  Click to open it."),
        tr("Open Spawn group templates..."),
        m_sgBtn, m_tabs);
    m_tabs->insertTab(int(Tab::SpawnGroup), sgTab, tr("Spawn group"));

    connect(m_poolBtn, &QPushButton::clicked,
            this, &PropertyInspectorDock::openPoolEditorRequested);
    connect(m_sgBtn, &QPushButton::clicked,
            this, &PropertyInspectorDock::openSpawnGroupEditorRequested);

    m_tabs->setCurrentIndex(int(Tab::Path));
}

void PropertyInspectorDock::showTab(Tab t)
{
    int const idx = int(t);
    if (idx < 0 || idx >= int(Tab::Count_))
        return;
    if (m_tabs) m_tabs->setCurrentIndex(idx);
}

} // namespace world_editor::app
