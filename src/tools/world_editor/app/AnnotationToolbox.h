/*
 * AnnotationToolbox - left-dock UI for creating + editing
 * playerbot_v2_world_metadata rows.
 *
 * Two modes:
 *   Browse: the default. Clicks on the viewer hit-test spawns /
 *           annotations; the toolbox shows nothing special.
 *   Place : the operator picked a kind from the kind selector. Clicks
 *           on the viewer drop a new annotation at that point with the
 *           current radius/label/notes.
 *
 * The toolbox also exposes the pending-changes count and a "Commit..."
 * button that opens AnnotationCommitDialog.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QString>
#include <QWidget>

#include <cstdint>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;

namespace world_editor::app
{

class AnnotationToolbox final : public QWidget
{
    Q_OBJECT

public:
    explicit AnnotationToolbox(QWidget* parent = nullptr);

    // True when the operator wants new clicks to place annotations.
    [[nodiscard]] bool isPlacing() const;
    [[nodiscard]] render::AnnotationKind currentKind() const;
    [[nodiscard]] float                  currentRadius() const;
    [[nodiscard]] QString                currentLabel() const;
    [[nodiscard]] QString                currentNotes() const;
    [[nodiscard]] QString                currentCreatedBy() const;

    // Status display from outside.
    void setPendingCount(size_t count);
    void setSelectedRow(int index, render::Annotation const& a);
    void clearSelectedRow();

    // Committed-count badge at the top of the dock.  Always called by
    // MainWindow after the baseline reload + after a successful commit
    // so the header stays in lockstep with the DB.  Greens when N>0, dim
    // grey when N==0.
    void setCommittedCount(size_t count, uint32_t mapId);

    // Brief feedback toast at the bottom of the dock.  kind picks the
    // background palette: "ok" (green, 2.5s), "warn" (orange, 2.5s), or
    // "err" (red, 6s).  Mirrors the HandcraftedRoadDock pattern so the
    // operator gets the same feedback shape across mutation surfaces.
    void showToast(QString const& text, QString const& kind);

signals:
    // Operator changed kind / radius / label / notes for the *selected*
    // row (only emitted when a row is selected).
    void selectedRowRadiusChanged(float newRadius);
    void selectedRowLabelChanged (QString const& newLabel);
    void selectedRowNotesChanged (QString const& newNotes);

    void placeModeChanged(bool placing);
    void deleteSelectedRequested();
    void commitRequested();
    void revertRequested();

private slots:
    void onKindChanged(int index);
    void onPlaceToggled(bool checked);
    void onRadiusEdited(double value);
    void onLabelEdited();
    void onNotesEdited();

private:
    QComboBox*      m_kindCombo     = nullptr;
    QDoubleSpinBox* m_radiusSpin    = nullptr;
    QLineEdit*      m_labelEdit     = nullptr;
    QLineEdit*      m_notesEdit     = nullptr;
    QLineEdit*      m_createdByEdit = nullptr;
    QCheckBox*      m_placeToggle   = nullptr;
    QLabel*         m_pendingLabel  = nullptr;
    QPushButton*    m_commitButton  = nullptr;
    QPushButton*    m_revertButton  = nullptr;
    QPushButton*    m_deleteButton  = nullptr;
    QLabel*         m_selectedHeader = nullptr;
    // Persistent header badge showing committed-row count for current map.
    QLabel*         m_committedBadge = nullptr;
    // Transient toast at dock bottom; auto-hidden by a singleShot timer
    // gated on m_toastEpoch so rapid mutations don't strand stale text.
    QLabel*         m_toastLabel     = nullptr;
    quint64         m_toastEpoch     = 0;

    // -1 when no row is selected.  Used to gate the edit signals so
    // typing in the radius/label/notes fields while NO row is selected
    // is interpreted as "defaults for the next placement", not as edits
    // to a row.
    int             m_selectedIndex = -1;
    bool            m_suppressEdits = false;
};

} // namespace world_editor::app
