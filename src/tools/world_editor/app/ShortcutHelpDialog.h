/*
 * ShortcutHelpDialog - read-only "what does every menu action do?" reference.
 *
 * Walks the parent QMenuBar recursively, collects (menu path, action text,
 * shortcut, tooltip) for every concrete QAction, and renders them in a
 * filterable QTableWidget.  Submenu paths are flattened with " > "
 * separators so the operator can grep visually.
 *
 * Filter line: case-insensitive substring match against menu-path OR action
 * text.  Empty filter shows everything.
 *
 * No mutation of the source menus - we only read action->text() / shortcut() /
 * toolTip().
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QMenu;
class QMenuBar;
class QTableWidget;

namespace world_editor::app
{

class ShortcutHelpDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutHelpDialog(QMenuBar* sourceBar, QWidget* parent = nullptr);

private slots:
    void onFilterChanged(QString const& text);

private:
    struct Row
    {
        QString menuPath;
        QString actionText;
        QString shortcut;
        QString description;
    };

    // Recursive collector: appends one Row per non-separator, non-submenu
    // action under `menu`, prefixing the path with `path`.
    void collectFromMenu(QMenu* menu, QString const& path, QVector<Row>& out) const;

    // Strip Qt mnemonic ampersands (e.g. "&File" -> "File") so the display
    // table stays clean.  Double-ampersand "&&" -> "&".
    static QString stripMnemonics(QString const& text);

    QLineEdit*    m_filter = nullptr;
    QTableWidget* m_table  = nullptr;
    QVector<Row>  m_rows;
};

} // namespace world_editor::app
