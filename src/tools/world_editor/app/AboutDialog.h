/*
 * AboutDialog - polished "About" dialog for the world_editor.
 *
 * Replaces the legacy QMessageBox::about call in MainWindow::onAbout with
 * a structured layout showing:
 *   - Title + version (build date / time captured via __DATE__ __TIME__).
 *   - Runtime Qt version (qVersion()), TC build target string, OpenGL
 *     driver string (captured by SceneView3D at initializeGL), database
 *     connection status, crashlog directory.
 *   - Dependencies, documentation links, credits.
 *   - Footer with "Copy diagnostic info" + Close.
 *
 * The dialog is dependency-light: it takes optional borrowed pointers to
 * the MainWindow's MySqlClient and SceneView3D so it can render live
 * status without spawning new contexts or DB connections.
 */

#pragma once

#include <QDialog>
#include <QString>

class QLabel;

namespace world_editor::db { class MySqlClient; }
namespace world_editor::render { class SceneView3D; }

namespace world_editor::app
{

class AboutDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(db::MySqlClient const* worldDb,
                         render::SceneView3D const* viewer3d,
                         QWidget* parent = nullptr);

private slots:
    void onCopyDiagnostics();

private:
    // Builds the single multiline string used by the clipboard copy and
    // by the in-dialog "version block" label.  Kept in one place so the
    // two surfaces never drift.
    QString buildDiagnosticBlock() const;

    // Builds the small HTML body for the build/runtime info section.
    QString buildHeaderHtml() const;

    // Crashlog directory path - %LOCALAPPDATA%\TrinityCore\world_editor\crashlogs\
    // on Windows, $XDG_DATA_HOME / ~/.local/share fallback elsewhere.
    static QString crashlogPath();

    // Resolves a doc filename to an absolute file:// URL under
    // src/tools/world_editor/docs/.  Returns empty string if the file
    // can't be located so the caller can omit a dead link.
    static QString docFileUrl(QString const& fileName);

    db::MySqlClient const*      m_worldDb = nullptr;
    render::SceneView3D const*  m_viewer3d = nullptr;
};

} // namespace world_editor::app
