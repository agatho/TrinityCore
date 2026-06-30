/*
 * PathPropertiesDock - editable property panel for a selected waypoint_path.
 *
 * Header fields: MoveType, Flags, Velocity, Comment (all editable in
 * Phase 4c).  Node table: PositionX/Y/Z (read-only in 4c - spatial
 * drag deferred), Orientation, Delay (editable).
 *
 * Footer: "Assign to selected spawn" button (Phase 4d) writes
 * creature_addon.PathId for the spawn the MainWindow has selected.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;

namespace world_editor::app
{

class PathPropertiesDock final : public QWidget
{
    Q_OBJECT

public:
    explicit PathPropertiesDock(QWidget* parent = nullptr);

    void setPath(int index, render::Path const& p);
    void clear();
    [[nodiscard]] int currentIndex() const noexcept { return m_index; }

    void setPendingCount(size_t count);

signals:
    void pathEdited(render::Path const& proposed);
    void deletePathRequested();
    void commitRequested();
    void revertRequested();
    // Operator wants the currently selected spawn's creature_addon.PathId
    // to point at this path.
    void assignToSelectedSpawnRequested();

private slots:
    void onHeaderChanged();
    void onNodeChanged(int row, int col);

private:
    [[nodiscard]] render::Path snapshotFromForm() const;
    void applyToForm(render::Path const& p);

    int            m_index = -1;
    render::Path   m_baseline{};
    bool           m_suppress = false;

    QLabel*          m_pathHeaderLabel = nullptr;
    QSpinBox*        m_moveTypeSpin   = nullptr;
    QSpinBox*        m_flagsSpin      = nullptr;
    QDoubleSpinBox*  m_velocitySpin   = nullptr;
    QLineEdit*       m_commentEdit    = nullptr;
    QTableWidget*    m_nodeTable      = nullptr;
    QLabel*          m_pendingLabel   = nullptr;
    QPushButton*     m_deleteButton   = nullptr;
    QPushButton*     m_assignButton   = nullptr;
    QPushButton*     m_commitButton   = nullptr;
    QPushButton*     m_revertButton   = nullptr;
};

} // namespace world_editor::app
