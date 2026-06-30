/*
 * WaypointPathDialog - modal editor for the `waypoint_path` family.
 *
 * TC stores creature patrol paths as two related tables:
 *   waypoint_path     (PathId, MoveType, Flags, Velocity, Comment)
 *   waypoint_path_node(PathId, NodeId, PositionX, PositionY, PositionZ,
 *                      Orientation, Delay)  -- composite PK (PathId, NodeId)
 *
 * The dialog is a two-pane modal:
 *   Left   - search QLineEdit + QListWidget of every waypoint_path row
 *            rendered as `"<PathId> -- <Comment>"` (just "<PathId>"
 *            when Comment is empty).  Sortable.
 *   Right  - header form (Comment / MoveType / Flags / Velocity + Save)
 *            plus a node QTableWidget for the selected PathId ordered by
 *            NodeId.  Node toolbar: Add / Edit / Remove / Renumber.
 *
 * Top-of-dialog toolbar (under path list): New path / Delete path /
 * Clone path.  Clone duplicates the header and every node into a fresh
 * MAX(PathId)+1.
 *
 * All DML wrapped in START TRANSACTION / COMMIT / ROLLBACK; deletes warn
 * when creature.path_id references the chosen path.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class WaypointPathDialog final : public QDialog
{
    Q_OBJECT

public:
    WaypointPathDialog(db::MySqlClient* dbClient,
                       QString const& worldDbName,
                       QWidget* parent = nullptr);

private slots:
    void onPathSearchChanged(QString const& text);
    void onPathSelectionChanged();
    void onSaveHeader();
    void onNewPath();
    void onDeletePath();
    void onClonePath();
    void onAddNode();
    void onEditNode();
    void onRemoveNode();
    void onRenumberNodes();

private:
    // Reload the left QListWidget from waypoint_path.  Honours the
    // search QLineEdit (substring match on the rendered label).
    void loadPaths();

    // Refresh the right-pane header form + node table for `pathId`.
    // Pass 0 to clear all right-pane widgets.
    void loadPath(uint32_t pathId);

    // Returns the PathId stored on the currently-selected QListWidgetItem,
    // or 0 if no selection.
    uint32_t selectedPathId() const;

    // Returns the NodeId of the currently-selected node table row, or
    // false when no selection / parse failure.
    bool currentNodeId(uint32_t& nodeIdOut) const;

    // Run `sql` inside START TRANSACTION / COMMIT.  Surfaces QMessageBox on
    // any error path and ROLLBACKs.  affectedOut may be null.
    bool runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut = nullptr);

    // Open the Add/Edit-node modal.  When editNodeId != UINT32_MAX the modal
    // pre-populates from the currently-selected node row and emits an UPDATE
    // WHERE (PathId, NodeId); otherwise it emits an INSERT with NodeId set to
    // MAX(NodeId)+1 (0 when the path has no nodes yet).
    void openNodeModal(uint32_t editNodeId);

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;

    // Left pane - search + path list.
    QLineEdit*       m_searchEdit = nullptr;
    QListWidget*     m_pathList   = nullptr;
    QPushButton*     m_newPathBtn    = nullptr;
    QPushButton*     m_deletePathBtn = nullptr;
    QPushButton*     m_clonePathBtn  = nullptr;

    // Right pane - header form.
    QLineEdit*       m_commentEdit   = nullptr;
    QSpinBox*        m_moveTypeSpin  = nullptr;
    QSpinBox*        m_flagsSpin     = nullptr;
    QDoubleSpinBox*  m_velocitySpin  = nullptr;
    QPushButton*     m_saveHeaderBtn = nullptr;

    // Right pane - node table + toolbar.
    QTableWidget*    m_nodeTable     = nullptr;
    QPushButton*     m_addNodeBtn    = nullptr;
    QPushButton*     m_editNodeBtn   = nullptr;
    QPushButton*     m_removeNodeBtn = nullptr;
    QPushButton*     m_renumberBtn   = nullptr;

    QLabel*          m_statusLabel = nullptr;

    // When true, loadPath()/loadPaths() is currently mutating widgets and
    // selectionChanged signals must be ignored to avoid recursion.
    bool             m_loading = false;
};

} // namespace world_editor::app
