/*
 * BulkEditDialog - apply one or more field changes across N selected spawns.
 *
 * UI shape: one row per editable column = QCheckBox (enables the field
 * for bulk apply) + value widget.  Unchecked rows leave the field
 * untouched on each spawn; checked rows write the value into every
 * selected row via SpawnModel::replaceRow.  Per-spawn no-op writes
 * collapse to None inside the model, so re-running the dialog with
 * the same values is idempotent.
 *
 * Only fields that make sense to bulk-set are exposed - the dialog
 * deliberately omits position/orientation/rotation (move multiple
 * spawns to the SAME spot is rarely what you want) and guid/entry/map
 * (immutable per spawn).  The exposed set covers the common authoring
 * cases the HANDOFF doc lists in section 3.2 as bulk-edit candidates:
 * phase, spawntimesecs, MovementType, ScriptName, plus npcflag /
 * unit_flags{,2,3} for batch faction/flag tagging.
 */

#pragma once

#include "../db/SpawnModel.h"
#include "../render/NavMeshView.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

namespace world_editor::app
{

class UndoManager;

class BulkEditDialog final : public QDialog
{
    Q_OBJECT

public:
    // `selectionIndices` is into `model.current()`.  Dialog edits the
    // model directly on Apply; no separate "applied" accessor is
    // needed - the caller refreshes the viewer from the model after
    // exec() returns Accepted.  `undo` is optional; when non-null the
    // entire apply batch is wrapped in a single recordOn frame so
    // Ctrl+Z reverses every row in one step.
    BulkEditDialog(db::SpawnModel& model,
                   QVector<int> const& selectionIndices,
                   QWidget* parent = nullptr,
                   UndoManager* undo = nullptr);

    [[nodiscard]] int rowsTouched() const noexcept { return m_rowsTouched; }

private slots:
    void onApply();

private:
    [[nodiscard]] uint64_t parseHexOrDec(QString const& s, uint64_t fallback) const;

    db::SpawnModel&    m_model;
    QVector<int> const m_selection;
    UndoManager*       m_undo = nullptr;
    int                m_rowsTouched = 0;

    // Row: enable-checkbox + value widget per field.
    QCheckBox*      m_enSpawntime    = nullptr;  QSpinBox*       m_spawntime    = nullptr;
    QCheckBox*      m_enMovement     = nullptr;  QComboBox*      m_movement     = nullptr;
    QCheckBox*      m_enPhaseUse     = nullptr;  QSpinBox*       m_phaseUse     = nullptr;
    QCheckBox*      m_enPhaseId      = nullptr;  QSpinBox*       m_phaseId      = nullptr;
    QCheckBox*      m_enPhaseGroup   = nullptr;  QSpinBox*       m_phaseGroup   = nullptr;
    QCheckBox*      m_enTerrainSwap  = nullptr;  QSpinBox*       m_terrainSwap  = nullptr;
    QCheckBox*      m_enDifficulties = nullptr;  QLineEdit*      m_difficulties = nullptr;
    QCheckBox*      m_enScriptName   = nullptr;  QLineEdit*      m_scriptName   = nullptr;
    QCheckBox*      m_enStringId     = nullptr;  QLineEdit*      m_stringId     = nullptr;
    QCheckBox*      m_enNpcflag      = nullptr;  QLineEdit*      m_npcflag      = nullptr;
    QCheckBox*      m_enUnitFlags1   = nullptr;  QLineEdit*      m_unitFlags1   = nullptr;
    QCheckBox*      m_enUnitFlags2   = nullptr;  QLineEdit*      m_unitFlags2   = nullptr;
    QCheckBox*      m_enUnitFlags3   = nullptr;  QLineEdit*      m_unitFlags3   = nullptr;
    QCheckBox*      m_enCurHealth    = nullptr;  QSpinBox*       m_curHealth    = nullptr;
    QCheckBox*      m_enGoState      = nullptr;  QSpinBox*       m_goState      = nullptr;
    QCheckBox*      m_enAnimprog     = nullptr;  QSpinBox*       m_animprog     = nullptr;

    // Respawn-time group: three mutually-exclusive ops applied to spawntimesecs.
    // Visual is three checkboxes (so the operator can see the values they'd
    // pick) but checking one auto-clears the other two.
    QCheckBox*      m_enRespawnSet   = nullptr;  QSpinBox*        m_respawnSet   = nullptr;
    QCheckBox*      m_enRespawnMul   = nullptr;  QDoubleSpinBox*  m_respawnMul   = nullptr;
    QCheckBox*      m_enRespawnAdd   = nullptr;  QSpinBox*        m_respawnAdd   = nullptr;

    // Phase group: three independent setters mirroring the columns in
    // creature/gameobject_phase semantics.  Independent (not mutex).
    QCheckBox*      m_enSetPhaseId      = nullptr;  QSpinBox*    m_setPhaseId      = nullptr;
    QCheckBox*      m_enSetPhaseGroup   = nullptr;  QSpinBox*    m_setPhaseGroup   = nullptr;
    QCheckBox*      m_enSetPhaseUse     = nullptr;  QSpinBox*    m_setPhaseUse     = nullptr;

    QLabel*         m_summary      = nullptr;
    QPushButton*    m_applyButton  = nullptr;
};

} // namespace world_editor::app
