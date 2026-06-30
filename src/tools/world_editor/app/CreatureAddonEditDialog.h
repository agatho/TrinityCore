/*
 * CreatureAddonEditDialog - modal form for one `creature_addon` row.
 *
 * Schema (live MySQL 9.4):
 *   guid                    bigint unsigned  PK -> creature.guid
 *   PathId                  int unsigned     waypoint path id (0 = none)
 *   mount                   int unsigned     CreatureDisplayInfo / template entry (0 = none)
 *   StandState              tinyint unsigned 0..9  (Stand/Sit/SitChair/Sleep/SitLowChair/
 *                                                   SitMediumChair/SitHighChair/Dead/Kneel/Submerged)
 *   AnimTier                tinyint unsigned 0..3
 *   VisFlags                tinyint unsigned bitfield
 *   SheathState             tinyint unsigned 0=unarmed 1=melee 2=ranged 3=spell
 *   PvPFlags                tinyint unsigned bitfield
 *   emote                   int unsigned     Emote.db2 id
 *   aiAnimKit               smallint unsigned
 *   movementAnimKit         smallint unsigned
 *   meleeAnimKit            smallint unsigned
 *   visibilityDistanceType  tinyint unsigned 0..4
 *   auras                   mediumtext       space-separated spell ids
 *
 * The dialog round-trips a CreatureAddonRow struct; the caller drives
 * the INSERT...ON DUPLICATE KEY UPDATE SQL with transactional rollback
 * on error.  No Undo wiring -- creature_addon isn't currently modeled
 * in the editor, so this is direct SQL via ConfirmSqlDialog.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QLineEdit;
class QSpinBox;

namespace world_editor::app
{

struct CreatureAddonRow
{
    int64_t  guid                   = 0;
    uint32_t pathId                 = 0;
    uint32_t mount                  = 0;
    uint8_t  standState             = 0;
    uint8_t  animTier               = 0;
    uint8_t  visFlags               = 0;
    uint8_t  sheathState            = 1;     // melee is the table default
    uint8_t  pvpFlags               = 0;
    uint32_t emote                  = 0;
    uint32_t aiAnimKit              = 0;
    uint32_t movementAnimKit        = 0;
    uint32_t meleeAnimKit           = 0;
    uint8_t  visibilityDistanceType = 0;
    QString  auras;                          // space-separated spell ids
};

class CreatureAddonEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CreatureAddonEditDialog(QWidget* parent = nullptr);

    // Populate fields from `r`.
    void setRow(CreatureAddonRow const& r);

    // Read the current form back into a row struct.
    [[nodiscard]] CreatureAddonRow row() const;

    // Lock guid when editing an existing row (PK cannot change).
    void setKeyEditable(bool editable);

private:
    QSpinBox*  m_guidSpin                  = nullptr;
    QSpinBox*  m_pathIdSpin                = nullptr;
    QSpinBox*  m_mountSpin                 = nullptr;
    QSpinBox*  m_standStateSpin            = nullptr;
    QSpinBox*  m_animTierSpin              = nullptr;
    QSpinBox*  m_visFlagsSpin              = nullptr;
    QSpinBox*  m_sheathStateSpin           = nullptr;
    QSpinBox*  m_pvpFlagsSpin              = nullptr;
    QSpinBox*  m_emoteSpin                 = nullptr;
    QSpinBox*  m_aiAnimKitSpin             = nullptr;
    QSpinBox*  m_movementAnimKitSpin       = nullptr;
    QSpinBox*  m_meleeAnimKitSpin          = nullptr;
    QSpinBox*  m_visibilityDistanceTypeSpin = nullptr;
    QLineEdit* m_aurasEdit                 = nullptr;
};

} // namespace world_editor::app
