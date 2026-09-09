/*
 * CommandPaletteDialog - VS Code-style fuzzy command launcher.
 *
 * Walks the host MainWindow's QMenuBar recursively, collects every leaf
 * QAction (action, menuPath, fullText = "menuPath > action"), then renders
 * a frameless modal overlay near the top of the host with a tall narrow
 * QLineEdit + QListWidget.  Typed text is filtered case-insensitively
 * by substring against fullText; prefix matches rank above midword
 * matches.  Up/Down arrows navigate, Enter triggers the selected
 * QAction, Esc closes.
 *
 * Empty input shows the recent-command history (QSettings:
 * command_palette/recent, capacity 20).  Each successful trigger pushes
 * the command path to the front of the recent list.
 *
 * Display cap: 50 results to keep the list snappy on very large menus.
 */

#pragma once

#include <QDialog>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

class QAction;
class QLineEdit;
class QListWidget;
class QMenu;
class QMenuBar;

namespace world_editor::app
{

class CommandPaletteDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPaletteDialog(QMenuBar* sourceBar, QWidget* parent = nullptr);

    // Position the overlay near the top-center of `host`.  Called by the
    // caller right before show()/exec() so we re-anchor on every open.
    void positionOver(QWidget* host);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onFilterChanged(QString const& text);
    void onItemActivated();

private:
    struct Entry
    {
        QPointer<QAction> action;
        QString           menuPath;     // "View > Bookmarks"
        QString           fullText;     // "View > Bookmarks > Add bookmark"
        QString           lowerFull;    // cached lowercase for matcher
    };

    struct Match
    {
        int entryIndex = -1;
        int rank       = 0;   // 0 = prefix on full, 1 = prefix on action, 2 = midword, 3 = recent
        int position   = 0;   // tiebreaker: earlier match wins
    };

    // Recursive collector: appends one Entry per non-separator, non-submenu
    // action under `menu`, prefixing the path with `path`.
    void collectFromMenu(QMenu* menu, QString const& path);

    // Strip Qt mnemonic ampersands (e.g. "&File" -> "File") so the display
    // stays clean.  Double-ampersand "&&" -> "&".
    static QString stripMnemonics(QString const& text);

    void rebuildList(QString const& needle);
    void renderRecent();
    void pushRecent(QString const& fullText);

    QLineEdit*    m_filter    = nullptr;
    QListWidget*  m_list      = nullptr;
    QVector<Entry> m_entries;
    QStringList   m_recent;          // most-recent first
    static constexpr int kRecentMax  = 20;
    static constexpr int kDisplayCap = 50;
};

} // namespace world_editor::app
