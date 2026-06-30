/*
 * BulkTransformDialog - relative translate / rotate / scale for the
 * currently selected spawns.
 *
 * Complements BulkEditDialog (which handles field changes).  Unlike
 * bulk-edit, every value here is a DELTA: translate adds (dX, dY, dZ);
 * rotate spins around the selection centroid (XY only) or around each
 * spawn's own origin (orientation-only); scale multiplies offsets from
 * the centroid by a factor.
 *
 * The dialog itself owns no model state - it merely collects parameters
 * and fires `transformRequested(...)` which the MainWindow applies via
 * UndoManager::recordOn so one Ctrl+Z reverses the whole batch.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace world_editor::app
{

class BulkTransformDialog final : public QDialog
{
    Q_OBJECT

public:
    // `selectedSpawns` is a snapshot of the rows the dialog should
    // summarise (centroid preview); the dialog never mutates them.
    BulkTransformDialog(QVector<render::Spawn> const& selectedSpawns,
                        QWidget* parent = nullptr);

signals:
    // Emitted on Apply.  Caller is responsible for the actual mutation
    // + undo recording.  Angles are degrees; `rotateAroundCentroid`
    // controls whether (worldX, worldY) is rotated about the centroid
    // (true) or only orientation is adjusted (false).  `scale` is
    // applied to each spawn's offset from the centroid (1.0 = no-op).
    void transformRequested(float dx, float dy, float dz,
                            float rotDegrees, bool rotateAroundCentroid,
                            float scale);

private slots:
    void onApply();

private:
    int          m_count = 0;
    float        m_centroidX = 0.0f;
    float        m_centroidY = 0.0f;
    float        m_centroidZ = 0.0f;

    QDoubleSpinBox* m_dx      = nullptr;
    QDoubleSpinBox* m_dy      = nullptr;
    QDoubleSpinBox* m_dz      = nullptr;
    QDoubleSpinBox* m_rotDeg  = nullptr;
    QCheckBox*      m_rotAroundCentroid = nullptr;
    QCheckBox*      m_scaleEn = nullptr;
    QDoubleSpinBox* m_scale   = nullptr;
    QLabel*         m_preview = nullptr;
    QPushButton*    m_applyButton = nullptr;
};

} // namespace world_editor::app
