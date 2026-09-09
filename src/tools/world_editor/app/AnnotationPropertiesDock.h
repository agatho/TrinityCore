/*
 * AnnotationPropertiesDock - editable form for a single annotation row.
 *
 * Replaces the read-only QPlainTextEdit dock content shipped originally.
 * Mirrors every column the AnnotationModel mutator API supports: radius,
 * label, notes are editable; kind + position + created_by are display-
 * only (the upstream WorldMetadata design treats kind/position as
 * immutable — "delete + readd" for those edits).
 *
 * The dock emits `rowEdited(int index, render::Annotation const&)`
 * whenever an editable field changes.  MainWindow funnels that into the
 * AnnotationModel editRadius / editLabel / editNotes API.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QWidget>

class QLineEdit;
class QDoubleSpinBox;
class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace world_editor::app
{

class AnnotationPropertiesDock final : public QWidget
{
    Q_OBJECT

public:
    explicit AnnotationPropertiesDock(QWidget* parent = nullptr);

    // Load fields from `a` and reset dirty state.  index < 0 clears the
    // form back to the prompt state.
    void setRow(int index, render::Annotation const& a);
    void clear();

    [[nodiscard]] int currentIndex() const noexcept { return m_index; }

    // Status updates from outside.
    void setPendingCount(size_t count);

signals:
    void rowEdited(int index, render::Annotation const& proposed);
    void deleteRequested();
    void commitRequested();
    void revertRequested();

private slots:
    void onFieldChanged();

private:
    [[nodiscard]] render::Annotation snapshotFromForm() const;
    void applyToForm(render::Annotation const& a);

    int                m_index    = -1;
    render::Annotation m_baseline{};
    bool               m_suppress = false;

    QLabel*            m_promptLabel = nullptr;

    // Identity (read-only).
    QLineEdit*         m_idEdit       = nullptr;
    QLineEdit*         m_mapIdEdit    = nullptr;
    QLineEdit*         m_zoneIdEdit   = nullptr;
    QLineEdit*         m_kindEdit     = nullptr;
    QLineEdit*         m_xEdit        = nullptr;
    QLineEdit*         m_yEdit        = nullptr;
    QLineEdit*         m_zEdit        = nullptr;
    QLineEdit*         m_createdByEdit = nullptr;

    // Editable.
    QDoubleSpinBox*    m_radiusSpin   = nullptr;
    QLineEdit*         m_labelEdit    = nullptr;
    QPlainTextEdit*    m_notesEdit    = nullptr;

    // Footer.
    QLabel*            m_pendingLabel = nullptr;
    QPushButton*       m_deleteButton = nullptr;
    QPushButton*       m_commitButton = nullptr;
    QPushButton*       m_revertButton = nullptr;
};

} // namespace world_editor::app
