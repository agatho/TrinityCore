/*
 * GameObjectAddonEditDialog - modal form for one `gameobject_addon` row.
 *
 * Schema (live MySQL 9.4):
 *   guid              int unsigned    PK -> gameobject.guid
 *   parent_rotation0  float           quaternion component (default 0)
 *   parent_rotation1  float           quaternion component (default 0)
 *   parent_rotation2  float           quaternion component (default 0)
 *   parent_rotation3  float           quaternion component (default 1)
 *   invisibilityType  tinyint unsigned  Invisibility kind enum
 *   invisibilityValue int unsigned    magnitude paired with invisibilityType
 *   WorldEffectID     int unsigned    WorldEffect.db2 id (0 = none)
 *   AIAnimKitID       int unsigned    AnimKit.db2 id (0 = none)
 *
 * Mirrors CreatureAddonEditDialog: round-trips a Row struct, caller owns
 * the INSERT...ON DUPLICATE KEY UPDATE SQL with transactional rollback.
 * Not undo-tracked -- direct SQL via the editor's ConfirmSqlDialog path.
 */

#pragma once

#include <QDialog>

#include <cstdint>

class QDoubleSpinBox;
class QSpinBox;

namespace world_editor::app
{

struct GameObjectAddonRow
{
    int64_t  guid              = 0;
    float    parentRotation0   = 0.0f;
    float    parentRotation1   = 0.0f;
    float    parentRotation2   = 0.0f;
    float    parentRotation3   = 1.0f;     // identity quaternion w-component
    uint8_t  invisibilityType  = 0;
    uint32_t invisibilityValue = 0;
    uint32_t worldEffectId     = 0;
    uint32_t aiAnimKitId       = 0;
};

class GameObjectAddonEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GameObjectAddonEditDialog(QWidget* parent = nullptr);

    // Populate fields from `r`.
    void setRow(GameObjectAddonRow const& r);

    // Read the current form back into a row struct.
    [[nodiscard]] GameObjectAddonRow row() const;

    // Lock guid when editing an existing row (PK cannot change).
    void setKeyEditable(bool editable);

private:
    QSpinBox*       m_guidSpin              = nullptr;
    QDoubleSpinBox* m_rot0Spin              = nullptr;
    QDoubleSpinBox* m_rot1Spin              = nullptr;
    QDoubleSpinBox* m_rot2Spin              = nullptr;
    QDoubleSpinBox* m_rot3Spin              = nullptr;
    QSpinBox*       m_invisibilityTypeSpin  = nullptr;
    QSpinBox*       m_invisibilityValueSpin = nullptr;
    QSpinBox*       m_worldEffectIdSpin     = nullptr;
    QSpinBox*       m_aiAnimKitIdSpin       = nullptr;
};

} // namespace world_editor::app
