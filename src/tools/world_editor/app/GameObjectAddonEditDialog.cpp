#include "GameObjectAddonEditDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <climits>

namespace world_editor::app
{

namespace
{
// Quaternion components are normalized so cap range at [-1, 1] with fine
// granularity; same precision as the read-only Position-tab display.
constexpr int    kRotationDecimals = 6;
constexpr double kRotationStep     = 0.000001;
constexpr double kRotationMin      = -1.0;
constexpr double kRotationMax      =  1.0;
} // namespace

GameObjectAddonEditDialog::GameObjectAddonEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit gameobject_addon row"));
    setModal(true);
    resize(560, 420);

    // PK: int unsigned in schema; QSpinBox range matches creature_addon
    // dialog's INT_MAX cap.  GameObject guids fit comfortably.
    m_guidSpin = new QSpinBox(this);
    m_guidSpin->setRange(0, INT_MAX);
    m_guidSpin->setToolTip(tr("gameobject.guid (PK). Locked when editing an existing row."));

    auto makeRotationSpin = [this](double defaultValue) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(kRotationMin, kRotationMax);
        spin->setDecimals(kRotationDecimals);
        spin->setSingleStep(kRotationStep);
        spin->setValue(defaultValue);
        return spin;
    };

    m_rot0Spin = makeRotationSpin(0.0);
    m_rot0Spin->setToolTip(tr("parent_rotation0 (qx). Quaternion component, range [-1, 1]."));
    m_rot1Spin = makeRotationSpin(0.0);
    m_rot1Spin->setToolTip(tr("parent_rotation1 (qy). Quaternion component, range [-1, 1]."));
    m_rot2Spin = makeRotationSpin(0.0);
    m_rot2Spin->setToolTip(tr("parent_rotation2 (qz). Quaternion component, range [-1, 1]."));
    m_rot3Spin = makeRotationSpin(1.0);
    m_rot3Spin->setToolTip(tr("parent_rotation3 (qw). Quaternion component, range [-1, 1]. Identity = 1."));

    m_invisibilityTypeSpin = new QSpinBox(this);
    m_invisibilityTypeSpin->setRange(0, 255);
    m_invisibilityTypeSpin->setToolTip(tr("Invisibility kind enum (uint8). 0 = no invisibility."));

    m_invisibilityValueSpin = new QSpinBox(this);
    m_invisibilityValueSpin->setRange(0, INT_MAX);
    m_invisibilityValueSpin->setToolTip(tr("Magnitude paired with invisibilityType."));

    m_worldEffectIdSpin = new QSpinBox(this);
    m_worldEffectIdSpin->setRange(0, INT_MAX);
    m_worldEffectIdSpin->setToolTip(tr("WorldEffect.db2 id (0 = none)."));

    m_aiAnimKitIdSpin = new QSpinBox(this);
    m_aiAnimKitIdSpin->setRange(0, INT_MAX);
    m_aiAnimKitIdSpin->setToolTip(tr("AnimKit.db2 id (0 = none)."));

    auto* form = new QFormLayout;
    form->addRow(tr("guid (PK)"),         m_guidSpin);
    form->addRow(tr("parent_rotation0"),  m_rot0Spin);
    form->addRow(tr("parent_rotation1"),  m_rot1Spin);
    form->addRow(tr("parent_rotation2"),  m_rot2Spin);
    form->addRow(tr("parent_rotation3"),  m_rot3Spin);
    form->addRow(tr("invisibilityType"),  m_invisibilityTypeSpin);
    form->addRow(tr("invisibilityValue"), m_invisibilityValueSpin);
    form->addRow(tr("WorldEffectID"),     m_worldEffectIdSpin);
    form->addRow(tr("AIAnimKitID"),       m_aiAnimKitIdSpin);

    auto* hint = new QLabel(tr(
        "gameobject_addon is per-spawn (keyed on gameobject.guid). The four\n"
        "parent_rotation floats form the parent-relative quaternion;\n"
        "(0, 0, 0, 1) is the identity orientation."), this);
    hint->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(hint);
    outer->addWidget(buttons);
}

void GameObjectAddonEditDialog::setRow(GameObjectAddonRow const& r)
{
    // Clamp the guid to int32 range -- QSpinBox limit per spec.
    int64_t const clampedGuid = (r.guid > int64_t(INT_MAX)) ? int64_t(INT_MAX) : r.guid;
    m_guidSpin->setValue(int(clampedGuid));
    m_rot0Spin->setValue(double(r.parentRotation0));
    m_rot1Spin->setValue(double(r.parentRotation1));
    m_rot2Spin->setValue(double(r.parentRotation2));
    m_rot3Spin->setValue(double(r.parentRotation3));
    m_invisibilityTypeSpin->setValue(int(r.invisibilityType));
    m_invisibilityValueSpin->setValue(int(r.invisibilityValue));
    m_worldEffectIdSpin->setValue(int(r.worldEffectId));
    m_aiAnimKitIdSpin->setValue(int(r.aiAnimKitId));
}

GameObjectAddonRow GameObjectAddonEditDialog::row() const
{
    GameObjectAddonRow r;
    r.guid              = int64_t(m_guidSpin->value());
    r.parentRotation0   = float(m_rot0Spin->value());
    r.parentRotation1   = float(m_rot1Spin->value());
    r.parentRotation2   = float(m_rot2Spin->value());
    r.parentRotation3   = float(m_rot3Spin->value());
    r.invisibilityType  = uint8_t(m_invisibilityTypeSpin->value());
    r.invisibilityValue = uint32_t(m_invisibilityValueSpin->value());
    r.worldEffectId     = uint32_t(m_worldEffectIdSpin->value());
    r.aiAnimKitId       = uint32_t(m_aiAnimKitIdSpin->value());
    return r;
}

void GameObjectAddonEditDialog::setKeyEditable(bool editable)
{
    m_guidSpin->setReadOnly(!editable);
    m_guidSpin->setButtonSymbols(editable ? QAbstractSpinBox::UpDownArrows
                                          : QAbstractSpinBox::NoButtons);
    if (!editable)
        m_guidSpin->setToolTip(tr("guid is the PK; locked while editing. Use Remove + Add to re-key."));
}

} // namespace world_editor::app
