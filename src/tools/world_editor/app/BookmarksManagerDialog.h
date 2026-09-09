/*
 * BookmarksManagerDialog - bulk editor for the operator's bookmark list.
 *
 * Tree grouped by folder (folder rows expandable), each leaf row
 * inline-edits name / folder / tags / mapId / X / Y / Z.  A tag-filter
 * line at the top narrows the visible leaves to bookmarks whose name OR
 * tags match.  Buttons: Add, Delete selected, Move-to-folder, Apply.
 *
 * "Apply" snapshots the current tree state back into QSettings via
 * Bookmarks::saveAll().  The dialog also writes on every structural
 * mutation (add/delete/move) so an accidental Close-without-Apply doesn't
 * lose the bigger operations.
 *
 * Emits bookmarksChanged() any time the persisted set is rewritten so the
 * View -> Bookmarks submenu in MainWindow can rebuild itself.
 */

#pragma once

#include "Bookmarks.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace world_editor::app
{

class BookmarksManagerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit BookmarksManagerDialog(QWidget* parent = nullptr);

signals:
    void bookmarksChanged();

private slots:
    void onAdd();
    void onDelete();
    void onMoveToFolder();
    void onApply();
    void onFilterChanged(QString const& text);
    void onItemChanged(QTreeWidgetItem* item, int column);

private:
    void rebuildTree();
    void applyFilterToTree();
    // Pull every leaf row out of the tree (across all folder groups) into
    // a fresh QVector<Bookmark>, ready to hand to Bookmarks::saveAll().
    QVector<Bookmark> collectFromTree() const;
    // Insert one row under the appropriate folder group; creates the
    // group on demand.  Returns the new leaf item.
    QTreeWidgetItem* insertLeafFor(Bookmark const& b);

    QLineEdit*   m_filter      = nullptr;
    QTreeWidget* m_tree        = nullptr;
    QPushButton* m_addBtn      = nullptr;
    QPushButton* m_delBtn      = nullptr;
    QPushButton* m_moveBtn     = nullptr;
    QPushButton* m_applyBtn    = nullptr;

    // When true, onItemChanged ignores edits (we're populating the tree
    // and don't want to re-save on every setText).
    bool m_suppressEdits = false;
};

} // namespace world_editor::app
