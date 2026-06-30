/*
 * MinimapSetupWizard - first-run walkthrough for the minimap texture layer.
 *
 * Triggered from MainWindow when the operator enables View -> Minimap texture
 * layer for the first time without either paths/casc_client_dir or
 * paths/minimap_dir configured.  Modal QDialog: shows two paths to a working
 * minimap (live CASC read vs pre-extracted PNGs) and routes the operator to
 * the matching File-menu picker via two action buttons.  Pure UI shell; the
 * actual storage open / dir picking lives on MainWindow.
 */

#pragma once

#include <QDialog>

namespace world_editor { class MainWindow; }

namespace world_editor::app
{

class MinimapSetupWizard final : public QDialog
{
    Q_OBJECT

public:
    explicit MinimapSetupWizard(MainWindow* owner, QWidget* parent = nullptr);

private slots:
    // Closes the wizard and triggers MainWindow::onSetCascClientDir so the
    // operator lands on the picker without an extra menu trip.
    void onPickCascDir();
    // Closes the wizard and opens a directory picker for the minimap PNG
    // root.  The picked path is pushed back through MainWindow via a public
    // setter so settings + viewer wiring stay co-located.
    void onPickMinimapDir();

private:
    MainWindow* m_owner = nullptr;
};

} // namespace world_editor::app
