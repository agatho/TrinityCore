#include "BulkTransformDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace world_editor::app
{

BulkTransformDialog::BulkTransformDialog(QVector<render::Spawn> const& selectedSpawns,
                                        QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bulk transform"));
    setModal(true);
    resize(440, 460);

    m_count = selectedSpawns.size();
    // Centroid = mean of (X, Y, Z) across all selected rows.  Used for
    // the preview label AND emitted-via-signal payload only indirectly
    // (the caller recomputes its own centroid from the live model so
    // we don't carry a stale snapshot across the dialog lifetime).
    if (m_count > 0)
    {
        double sx = 0.0, sy = 0.0, sz = 0.0;
        for (render::Spawn const& s : selectedSpawns)
        {
            sx += s.worldX;
            sy += s.worldY;
            sz += s.worldZ;
        }
        m_centroidX = float(sx / double(m_count));
        m_centroidY = float(sy / double(m_count));
        m_centroidZ = float(sz / double(m_count));
    }

    // -------- Translate group --------
    auto* translateGroup = new QGroupBox(tr("Translate (yards)"), this);
    auto* translateForm  = new QFormLayout(translateGroup);
    m_dx = new QDoubleSpinBox(this); m_dx->setRange(-100000.0, 100000.0); m_dx->setDecimals(3); m_dx->setValue(0.0);
    m_dy = new QDoubleSpinBox(this); m_dy->setRange(-100000.0, 100000.0); m_dy->setDecimals(3); m_dy->setValue(0.0);
    m_dz = new QDoubleSpinBox(this); m_dz->setRange(-100000.0, 100000.0); m_dz->setDecimals(3); m_dz->setValue(0.0);
    translateForm->addRow(tr("dX"), m_dx);
    translateForm->addRow(tr("dY"), m_dy);
    translateForm->addRow(tr("dZ"), m_dz);

    // -------- Rotate group --------
    auto* rotateGroup = new QGroupBox(tr("Rotate (degrees)"), this);
    auto* rotateForm  = new QFormLayout(rotateGroup);
    m_rotDeg = new QDoubleSpinBox(this);
    m_rotDeg->setRange(-360.0, 360.0);
    m_rotDeg->setDecimals(3);
    m_rotDeg->setValue(0.0);
    m_rotAroundCentroid = new QCheckBox(tr(
        "Rotate around centroid (else each spawn rotates around its own "
        "origin which is a no-op for facing-only spawns)"), this);
    m_rotAroundCentroid->setChecked(true);
    rotateForm->addRow(tr("Angle"), m_rotDeg);
    rotateForm->addRow(m_rotAroundCentroid);

    // -------- Scale group --------
    auto* scaleGroup = new QGroupBox(tr("Scale"), this);
    auto* scaleForm  = new QFormLayout(scaleGroup);
    m_scaleEn = new QCheckBox(tr("Scale positions around centroid by factor"), this);
    m_scale   = new QDoubleSpinBox(this);
    m_scale->setRange(0.001, 1000.0);
    m_scale->setDecimals(4);
    m_scale->setValue(1.0);
    scaleForm->addRow(m_scaleEn);
    scaleForm->addRow(tr("Factor"), m_scale);

    // -------- Preview --------
    m_preview = new QLabel(this);
    m_preview->setWordWrap(true);
    m_preview->setText(tr("%1 spawn(s) selected, centroid at (%2, %3, %4).")
                       .arg(m_count)
                       .arg(double(m_centroidX), 0, 'f', 2)
                       .arg(double(m_centroidY), 0, 'f', 2)
                       .arg(double(m_centroidZ), 0, 'f', 2));

    // -------- Buttons --------
    auto* buttons = new QDialogButtonBox(this);
    m_applyButton = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(m_applyButton, &QPushButton::clicked, this, &BulkTransformDialog::onApply);
    connect(buttons,       &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_preview);
    outer->addWidget(translateGroup);
    outer->addWidget(rotateGroup);
    outer->addWidget(scaleGroup);
    outer->addStretch(1);
    outer->addWidget(buttons);
}

void BulkTransformDialog::onApply()
{
    if (m_count <= 0)
    {
        reject();
        return;
    }
    float const dx       = float(m_dx->value());
    float const dy       = float(m_dy->value());
    float const dz       = float(m_dz->value());
    float const rotDeg   = float(m_rotDeg->value());
    bool  const rotAroundCent = m_rotAroundCentroid->isChecked();
    float const scaleVal = m_scaleEn->isChecked() ? float(m_scale->value()) : 1.0f;

    emit transformRequested(dx, dy, dz, rotDeg, rotAroundCent, scaleVal);
    accept();
}

} // namespace world_editor::app
