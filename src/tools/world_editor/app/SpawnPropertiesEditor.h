/*
 * SpawnPropertiesEditor - editable form for a single creature/gameobject row.
 *
 * Replaces the read-only QPlainTextEdit dock content shipped in Phase 1.
 * Mirrors every column in the playerbot_world.creature / .gameobject
 * tables so the operator can edit any field without dropping to SQL.
 * Position / orientation / rotation are display-only in Phase 3a; drag-
 * to-move lands in Phase 3b.
 *
 * The editor emits a single signal `rowEdited(render::Spawn const&)`
 * every time any field changes.  MainWindow forwards the new row into
 * SpawnModel which decides whether it's a real change or a no-op
 * collapsing back to the baseline.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QWidget>

class QTabWidget;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

namespace world_editor::app
{

class SpawnPropertiesEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit SpawnPropertiesEditor(QWidget* parent = nullptr);

    // Load fields from `s` and reset dirty state. index < 0 + a default
    // Spawn() clears the form.
    void setRow(int index, render::Spawn const& s);
    void clear();
    // Phase 3d: switch the dock to "N selected - use bulk edit" UI.
    // Pass count > 1 to enter bulk mode; 0 returns to the cleared/empty
    // state.  Bulk mode disables the per-field tabs and shows the
    // Bulk Edit button.
    void setBulkMode(int count);

    [[nodiscard]] int currentIndex() const noexcept { return m_index; }

    // Snap-to-ground toggle (read by MainWindow on drop).  Persists in
    // QSettings ("editor/snap_to_ground").
    [[nodiscard]] bool snapToGroundEnabled() const;

signals:
    // Operator changed a field. `proposed` carries the full row with the
    // new value applied (position/orientation untouched in 3a).
    void rowEdited(render::Spawn const& proposed);
    void deleteRequested();
    void commitRequested();
    void revertRequested();
    // Operator clicked "Bulk Edit..." while in bulk-mode.
    void bulkEditRequested();
    // Operator clicked "Edit addon..." with a creature spawn selected.
    // MainWindow opens CreatureAddonEditDialog keyed on the spawn's guid.
    void editAddonRequested();
    // Operator clicked "Edit GO addon..." with a gameobject spawn selected.
    // MainWindow opens GameObjectAddonEditDialog keyed on the spawn's guid.
    void editGameObjectAddonRequested();
    // Operator clicked "SmartAI..." -- MainWindow seeds a smart_scripts row for
    // this spawn's entry (source_type by kind) and opens the SmartAI editor.
    void editSmartAiRequested();
    // Operator clicked "Spawn pool..." -- MainWindow opens the Groups & Pools
    // dialog so this spawn can be added to / removed from a pool.
    void editPoolRequested();

    // Status updates from outside.
public:
    void setPendingCount(size_t count);
    // spawn_group membership label fed by MainWindow's onSpawnClicked DB probe.
    void setGroupMembershipText(QString const& text);

private slots:
    void onFieldChanged();

private:
    void buildIdentityTab();
    void buildPositionTab();
    void buildBehaviorTab();
    void buildPhaseTab();
    void buildFlagsTab();
    void buildScriptTab();

    [[nodiscard]] render::Spawn snapshotFromForm() const;
    void applyToForm(render::Spawn const& s);

    int           m_index   = -1;
    render::Spawn m_baseline{}; // last setRow() value; for diffing
    bool          m_suppress = false; // re-entry guard during applyToForm

    QTabWidget*   m_tabs = nullptr;

    // Identity tab (mostly read-only).
    QLabel*       m_kindLabel  = nullptr;
    QLineEdit*    m_guidEdit   = nullptr;       // read-only
    QSpinBox*     m_entrySpin  = nullptr;       // read-only in 3a
    QLineEdit*    m_mapEdit    = nullptr;       // read-only
    QLineEdit*    m_zoneEdit   = nullptr;       // read-only
    QLineEdit*    m_areaEdit   = nullptr;       // read-only

    // Position tab (editable).  position_x/y/z + a degrees-based "Facing"
    // control; for gameobjects the facing drives the rotation quaternion.
    // The raw quaternion spinboxes stay available for advanced GO tilt.
    QDoubleSpinBox* m_xSpin = nullptr;
    QDoubleSpinBox* m_ySpin = nullptr;
    QDoubleSpinBox* m_zSpin = nullptr;
    QDoubleSpinBox* m_facingSpin = nullptr;  // yaw in DEGREES (user-friendly)
    QDoubleSpinBox* m_r0Spin = nullptr;      // quaternion x
    QDoubleSpinBox* m_r1Spin = nullptr;      // quaternion y
    QDoubleSpinBox* m_r2Spin = nullptr;      // quaternion z
    QDoubleSpinBox* m_r3Spin = nullptr;      // quaternion w

    // Behavior tab (editable).
    QSpinBox*     m_spawntimeSpin       = nullptr;
    QDoubleSpinBox* m_wanderSpin        = nullptr;
    QSpinBox*     m_curHealthSpin       = nullptr;
    QComboBox*    m_movementCombo       = nullptr;
    QSpinBox*     m_currentwaypointSpin = nullptr;
    QSpinBox*     m_animprogressSpin    = nullptr;
    QSpinBox*     m_stateSpin           = nullptr;

    // Phase tab (editable).
    QSpinBox*     m_phaseUseFlagsSpin   = nullptr;
    QSpinBox*     m_phaseIdSpin         = nullptr;
    QSpinBox*     m_phaseGroupSpin      = nullptr;
    QSpinBox*     m_terrainSwapSpin     = nullptr;
    QLineEdit*    m_difficultiesEdit    = nullptr;

    // Flags tab (creature only).
    QLineEdit*    m_npcflagEdit  = nullptr;  // hex
    QLineEdit*    m_unitFlags1Edit = nullptr;
    QLineEdit*    m_unitFlags2Edit = nullptr;
    QLineEdit*    m_unitFlags3Edit = nullptr;
    QSpinBox*     m_modelidSpin  = nullptr;
    QSpinBox*     m_equipmentSpin = nullptr;

    // Script tab.
    QLineEdit*    m_scriptNameEdit = nullptr;
    QLineEdit*    m_stringIdEdit   = nullptr;

    // Footer.
    QLabel*       m_pendingLabel = nullptr;
    QLabel*       m_groupMembershipLabel = nullptr;  // spawn_group membership readout
    QPushButton*  m_deleteButton = nullptr;
    QPushButton*  m_commitButton = nullptr;
    QPushButton*  m_revertButton = nullptr;
    QCheckBox*    m_snapCheckbox = nullptr;
    QPushButton*  m_bulkButton   = nullptr;
    QPushButton*  m_addonButton  = nullptr;  // visible only when a creature row is selected
    QPushButton*  m_goAddonButton = nullptr; // visible only when a gameobject row is selected
    QPushButton*  m_smartAiButton = nullptr; // SmartAI authoring for the selected spawn
    QPushButton*  m_poolButton    = nullptr; // add/remove this spawn in a spawn pool
};

} // namespace world_editor::app
