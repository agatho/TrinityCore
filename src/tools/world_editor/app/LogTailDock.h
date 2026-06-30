/*
 * LogTailDock - read-only right-pane panel that auto-tails a worldserver
 * log file.  Operator points it at a file via the "Set..." button or by
 * typing into the path field; the dock seeks to the last known byte
 * offset every 2 seconds, appends any newly written bytes, caps the
 * visible buffer at 500 lines, and color-tints each line based on
 * ERROR / WARN / DEBUG markers.
 *
 * Persistence: the path is mirrored into QSettings under
 * `paths/log_tail_file` so the next session can resume polling
 * automatically.  File rotation (size shrank) resets the offset to 0
 * and re-tails from scratch.
 */

#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QTimer;

namespace world_editor::app
{

class LogTailDock final : public QWidget
{
    Q_OBJECT

public:
    explicit LogTailDock(QWidget* parent = nullptr);

    // Point the dock at `path` and (re)start polling.  Empty path clears
    // the current file binding and stops the timer.
    void setLogPath(QString const& path);

    // Pause/resume the auto-poll timer.  Visible buffer is preserved.
    void setPaused(bool paused);

    // Public so MainWindow can focus the path edit when opening via the
    // Tools menu entry.
    QLineEdit* pathEdit() const { return m_pathEdit; }

    // Current file path the dock is bound to (may be empty).
    QString currentPath() const;

private slots:
    void onSetClicked();
    void onTogglePause();
    void onPathEdited();
    void onPoll();

private:
    // Drain newly-appended bytes from the bound file, append color-tinted
    // lines to the text edit, and cap the buffer at 500 lines.  Handles
    // rotation (file shrank below cached offset) by resetting to 0.
    void pollOnce();

    // Append `line` to the text edit, picking a color based on whether
    // the line contains ERROR / WARN / DEBUG substrings.
    void appendColoredLine(QString const& line);

    // Trim the visible buffer from the top until it carries at most
    // `kMaxLines` lines.  Cheap when already in-range.
    void enforceLineCap();

    QLineEdit*      m_pathEdit  = nullptr;
    QPushButton*    m_setButton = nullptr;
    QPushButton*    m_pauseButton = nullptr;
    QPlainTextEdit* m_text      = nullptr;
    QTimer*         m_timer     = nullptr;

    QString  m_path;
    qint64   m_offset = 0;
    bool     m_paused = false;
    // Carry partial trailing line (no \n yet) across polls so we color
    // the line as a whole once it's complete.
    QString  m_pending;

    static constexpr int kMaxLines = 500;
    static constexpr int kPollMs   = 2000;
};

} // namespace world_editor::app
