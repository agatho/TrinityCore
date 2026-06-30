/*
 * PropertyInspectorDock - unified right-pane host for the entity-property
 * editors that used to live as individual tabbed QDockWidgets.
 *
 * Path / Areatrigger / Graveyard property panes are full editors; Pool
 * and SpawnGroup tabs are launch surfaces (single-button) that open
 * the corresponding modal editors, since those entities are managed
 * via dialogs rather than dedicated docks.
 *
 * MainWindow drives the active tab to match the selected entity (a
 * path-node click flips to the Path tab; a graveyard click flips to
 * Graveyard).  The dock owns the inner widgets; signals from the
 * inner widgets are forwarded by MainWindow exactly as they were when
 * the docks lived alone.
 */

#pragma once

#include <QWidget>

class QPushButton;
class QTabWidget;

namespace world_editor::app
{

class PathPropertiesDock;
class AreatriggerPropertiesDock;
class GraveyardPropertiesDock;

class PropertyInspectorDock final : public QWidget
{
    Q_OBJECT

public:
    enum class Tab : int
    {
        Path        = 0,
        Areatrigger = 1,
        Graveyard   = 2,
        Pool        = 3,
        SpawnGroup  = 4,
        Count_
    };

    explicit PropertyInspectorDock(QWidget* parent = nullptr);

    PathPropertiesDock*        pathDock()        const noexcept { return m_pathDock; }
    AreatriggerPropertiesDock* areatriggerDock() const noexcept { return m_atrDock; }
    GraveyardPropertiesDock*   graveyardDock()   const noexcept { return m_gyDock; }

    // Switch to the named tab programmatically; called by MainWindow
    // on entity-selection changes so the relevant editor surfaces.
    void showTab(Tab t);

signals:
    // Operator clicked the "Open editor..." button on Pool / SpawnGroup
    // tabs.  MainWindow opens the matching modal dialog.
    void openPoolEditorRequested();
    void openSpawnGroupEditorRequested();

private:
    QTabWidget*                m_tabs    = nullptr;
    PathPropertiesDock*        m_pathDock = nullptr;
    AreatriggerPropertiesDock* m_atrDock  = nullptr;
    GraveyardPropertiesDock*   m_gyDock   = nullptr;
    QPushButton*               m_poolBtn  = nullptr;
    QPushButton*               m_sgBtn    = nullptr;
};

} // namespace world_editor::app
