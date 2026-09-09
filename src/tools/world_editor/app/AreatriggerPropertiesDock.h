/*
 * AreatriggerPropertiesDock - editable property panel for a selected
 * `areatrigger` row.  Phase 7b.
 *
 * The shape outline (Sphere/Box/Disk/...) is rendered from the joined
 * areatrigger_create_properties row, which we treat as read-only - the
 * Shape + ShapeData* values are displayed in a small read-only header so
 * the operator can see what they're moving, but they are not editable
 * here.  Pick a different CreatePropertiesId to change the shape.
 *
 * Buttons:
 *   - Delete areatrigger   (marks the row Delete)
 *   - Revert all           (clears the in-memory changelog)
 *   - Commit...            (opens AreatriggerCommitDialog)
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;

namespace world_editor::app
{

class AreatriggerPropertiesDock final : public QWidget
{
    Q_OBJECT

public:
    explicit AreatriggerPropertiesDock(QWidget* parent = nullptr);

    void setAreatrigger(int index, render::Areatrigger const& a);
    void clear();
    [[nodiscard]] int currentIndex() const noexcept { return m_index; }

    void setPendingCount(size_t count);

signals:
    void areatriggerEdited(render::Areatrigger const& proposed);
    void deleteAreatriggerRequested();
    void commitRequested();
    void revertRequested();

private slots:
    void onFormChanged();

private:
    [[nodiscard]] render::Areatrigger snapshotFromForm() const;
    void applyToForm(render::Areatrigger const& a);
    static QString shapeName(uint8_t shape);

    int                 m_index = -1;
    render::Areatrigger m_baseline{};
    bool                m_suppress = false;

    QLabel*         m_headerLabel       = nullptr;
    QLabel*         m_shapeReadOnly     = nullptr;
    QSpinBox*       m_createPropsSpin   = nullptr;
    QSpinBox*       m_isCustomSpin      = nullptr;
    QLineEdit*      m_spawnDiffEdit     = nullptr;
    QDoubleSpinBox* m_posXSpin          = nullptr;
    QDoubleSpinBox* m_posYSpin          = nullptr;
    QDoubleSpinBox* m_posZSpin          = nullptr;
    QDoubleSpinBox* m_orientSpin        = nullptr;
    QSpinBox*       m_phaseUseFlagsSpin = nullptr;
    QSpinBox*       m_phaseIdSpin       = nullptr;
    QSpinBox*       m_phaseGroupSpin    = nullptr;
    QLineEdit*      m_scriptNameEdit    = nullptr;
    QLineEdit*      m_commentEdit       = nullptr;
    QSpinBox*       m_verifiedBuildSpin = nullptr;
    QLabel*         m_pendingLabel      = nullptr;
    QPushButton*    m_deleteButton      = nullptr;
    QPushButton*    m_revertButton      = nullptr;
    QPushButton*    m_commitButton      = nullptr;
};

} // namespace world_editor::app
