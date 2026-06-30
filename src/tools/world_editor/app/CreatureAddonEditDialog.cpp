#include "CreatureAddonEditDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <climits>

namespace world_editor::app
{

CreatureAddonEditDialog::CreatureAddonEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit creature_addon row"));
    setModal(true);
    resize(560, 560);

    // PK: bigint unsigned in schema, but spec requires QSpinBox.
    // Range capped to INT_MAX -- adequate for typical TC GUIDs; 64-bit
    // PKs outside this range need direct SQL anyway.
    m_guidSpin = new QSpinBox(this);
    m_guidSpin->setRange(0, INT_MAX);
    m_guidSpin->setToolTip(tr("creature.guid (PK). Locked when editing an existing row."));

    m_pathIdSpin = new QSpinBox(this);
    m_pathIdSpin->setRange(0, INT_MAX);
    m_pathIdSpin->setToolTip(tr("waypoint_data path id (0 = no path)"));

    m_mountSpin = new QSpinBox(this);
    m_mountSpin->setRange(0, INT_MAX);
    m_mountSpin->setToolTip(tr("CreatureDisplayInfo mount id (or 0 = unmounted)"));

    m_standStateSpin = new QSpinBox(this);
    m_standStateSpin->setRange(0, 9);
    m_standStateSpin->setToolTip(tr(
        "StandState:\n"
        "  0 = Stand\n"
        "  1 = Sit\n"
        "  2 = SitChair\n"
        "  3 = Sleep\n"
        "  4 = SitLowChair\n"
        "  5 = SitMediumChair\n"
        "  6 = SitHighChair\n"
        "  7 = Dead\n"
        "  8 = Kneel\n"
        "  9 = Submerged"));

    m_animTierSpin = new QSpinBox(this);
    m_animTierSpin->setRange(0, 3);
    m_animTierSpin->setToolTip(tr("AnimTier 0..3"));

    m_visFlagsSpin = new QSpinBox(this);
    m_visFlagsSpin->setRange(0, 255);
    m_visFlagsSpin->setDisplayIntegerBase(16);
    m_visFlagsSpin->setPrefix(QStringLiteral("0x"));
    m_visFlagsSpin->setToolTip(tr("UnitVisibilityFlags bitfield (uint8, hex display)"));

    m_sheathStateSpin = new QSpinBox(this);
    m_sheathStateSpin->setRange(0, 3);
    m_sheathStateSpin->setToolTip(tr(
        "SheathState:\n"
        "  0 = unarmed\n"
        "  1 = melee\n"
        "  2 = ranged\n"
        "  3 = spell"));

    m_pvpFlagsSpin = new QSpinBox(this);
    m_pvpFlagsSpin->setRange(0, 255);
    m_pvpFlagsSpin->setDisplayIntegerBase(16);
    m_pvpFlagsSpin->setPrefix(QStringLiteral("0x"));
    m_pvpFlagsSpin->setToolTip(tr("UnitPvPStateFlags bitfield (uint8, hex display)"));

    m_emoteSpin = new QSpinBox(this);
    m_emoteSpin->setRange(0, INT_MAX);
    m_emoteSpin->setToolTip(tr("Emote.db2 id (one-shot)"));

    m_aiAnimKitSpin = new QSpinBox(this);
    m_aiAnimKitSpin->setRange(0, 65535);

    m_movementAnimKitSpin = new QSpinBox(this);
    m_movementAnimKitSpin->setRange(0, 65535);

    m_meleeAnimKitSpin = new QSpinBox(this);
    m_meleeAnimKitSpin->setRange(0, 65535);

    m_visibilityDistanceTypeSpin = new QSpinBox(this);
    m_visibilityDistanceTypeSpin->setRange(0, 4);
    m_visibilityDistanceTypeSpin->setToolTip(tr("0=Normal 1=Tiny 2=Small 3=Large 4=Gigantic"));

    m_aurasEdit = new QLineEdit(this);
    m_aurasEdit->setPlaceholderText(tr("space-separated spell ids, e.g. \"12345 67890\""));

    auto* form = new QFormLayout;
    form->addRow(tr("guid (PK)"),               m_guidSpin);
    form->addRow(tr("PathId"),                  m_pathIdSpin);
    form->addRow(tr("mount"),                   m_mountSpin);
    form->addRow(tr("StandState"),              m_standStateSpin);
    form->addRow(tr("AnimTier"),                m_animTierSpin);
    form->addRow(tr("VisFlags"),                m_visFlagsSpin);
    form->addRow(tr("SheathState"),             m_sheathStateSpin);
    form->addRow(tr("PvPFlags"),                m_pvpFlagsSpin);
    form->addRow(tr("emote"),                   m_emoteSpin);
    form->addRow(tr("aiAnimKit"),               m_aiAnimKitSpin);
    form->addRow(tr("movementAnimKit"),         m_movementAnimKitSpin);
    form->addRow(tr("meleeAnimKit"),            m_meleeAnimKitSpin);
    form->addRow(tr("visibilityDistanceType"),  m_visibilityDistanceTypeSpin);
    form->addRow(tr("auras"),                   m_aurasEdit);

    auto* hint = new QLabel(tr(
        "creature_addon is per-spawn (keyed on creature.guid). MountCreatureID\n"
        "is preserved on UPDATE (not exposed here)."), this);
    hint->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(hint);
    outer->addWidget(buttons);
}

void CreatureAddonEditDialog::setRow(CreatureAddonRow const& r)
{
    // Clamp the bigint guid to int32 range -- QSpinBox limit per spec.
    int64_t const clampedGuid = (r.guid > int64_t(INT_MAX)) ? int64_t(INT_MAX) : r.guid;
    m_guidSpin->setValue(int(clampedGuid));
    m_pathIdSpin->setValue(int(r.pathId));
    m_mountSpin->setValue(int(r.mount));
    m_standStateSpin->setValue(int(r.standState));
    m_animTierSpin->setValue(int(r.animTier));
    m_visFlagsSpin->setValue(int(r.visFlags));
    m_sheathStateSpin->setValue(int(r.sheathState));
    m_pvpFlagsSpin->setValue(int(r.pvpFlags));
    m_emoteSpin->setValue(int(r.emote));
    m_aiAnimKitSpin->setValue(int(r.aiAnimKit));
    m_movementAnimKitSpin->setValue(int(r.movementAnimKit));
    m_meleeAnimKitSpin->setValue(int(r.meleeAnimKit));
    m_visibilityDistanceTypeSpin->setValue(int(r.visibilityDistanceType));
    m_aurasEdit->setText(r.auras);
}

CreatureAddonRow CreatureAddonEditDialog::row() const
{
    CreatureAddonRow r;
    r.guid                   = int64_t(m_guidSpin->value());
    r.pathId                 = uint32_t(m_pathIdSpin->value());
    r.mount                  = uint32_t(m_mountSpin->value());
    r.standState             = uint8_t(m_standStateSpin->value());
    r.animTier               = uint8_t(m_animTierSpin->value());
    r.visFlags               = uint8_t(m_visFlagsSpin->value());
    r.sheathState            = uint8_t(m_sheathStateSpin->value());
    r.pvpFlags               = uint8_t(m_pvpFlagsSpin->value());
    r.emote                  = uint32_t(m_emoteSpin->value());
    r.aiAnimKit              = uint32_t(m_aiAnimKitSpin->value());
    r.movementAnimKit        = uint32_t(m_movementAnimKitSpin->value());
    r.meleeAnimKit           = uint32_t(m_meleeAnimKitSpin->value());
    r.visibilityDistanceType = uint8_t(m_visibilityDistanceTypeSpin->value());
    r.auras                  = m_aurasEdit->text().trimmed();
    return r;
}

void CreatureAddonEditDialog::setKeyEditable(bool editable)
{
    m_guidSpin->setReadOnly(!editable);
    m_guidSpin->setButtonSymbols(editable ? QAbstractSpinBox::UpDownArrows
                                          : QAbstractSpinBox::NoButtons);
    if (!editable)
        m_guidSpin->setToolTip(tr("guid is the PK; locked while editing. Use Remove + Add to re-key."));
}

} // namespace world_editor::app
