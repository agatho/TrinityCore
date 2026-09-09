/*
 * PropagateFieldsDialog - copy selected fields from one "canonical" spawn
 * to every other spawn that shares its kind + entry + mapId.
 *
 * Use case: an operator tweaks a single template-style spawn (npcflag,
 * spawntimesecs, phase, script, etc.) and wants the same values broadcast
 * to all sibling rows of that creature/gameobject on the same map without
 * touching unrelated fields (position, guid, etc.).
 *
 * UI shape: one QCheckBox per propagatable field with sensible defaults
 * pre-checked.  Footer: "Preview affected rows" (lists the GUIDs/positions
 * of the receivers in a small modal) + Apply + Cancel.  Emits
 * propagateRequested() on Apply; MainWindow consumes the signal and
 * rewrites each receiver row through SpawnModel::replaceRow inside a
 * single UndoManager::recordOn frame.
 *
 * Field categorization mirrors render::Spawn:
 *   - shared        : spawntimesecs, phaseUseFlags, phaseId, phaseGroup,
 *                     spawnDifficulties, scriptName, stringId
 *   - creature-only : npcflag, unitFlags1/2/3, movementType, modelid,
 *                     equipmentId, curHealthPct, wanderDistance
 *   - GO-only       : rotation0/1/2/3, goState, animprogress
 * Creature-only checkboxes are hidden for GO canonicals and vice versa.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QDialog>
#include <QHash>
#include <QSet>
#include <QString>

class QCheckBox;
class QLabel;
class QPushButton;

namespace world_editor::app
{

class PropagateFieldsDialog final : public QDialog
{
    Q_OBJECT

public:
    // Field-name tokens.  These are the canonical strings emitted in the
    // selectedFields set and consumed by MainWindow::onPropagateRequested.
    // Kept as plain strings (not an enum) so the signal payload stays
    // header-light and the set is easy to inspect in the debugger.
    static QString const kSpawntimesecs;
    static QString const kPhaseUseFlags;
    static QString const kPhaseId;
    static QString const kPhaseGroup;
    static QString const kSpawnDifficulties;
    static QString const kNpcflag;
    static QString const kUnitFlags1;
    static QString const kUnitFlags2;
    static QString const kUnitFlags3;
    static QString const kMovementType;
    static QString const kModelid;
    static QString const kEquipmentId;
    static QString const kRotation0;
    static QString const kRotation1;
    static QString const kRotation2;
    static QString const kRotation3;
    static QString const kGoState;
    static QString const kAnimprogress;
    static QString const kScriptName;
    static QString const kStringId;
    static QString const kCurHealthPct;
    static QString const kWanderDistance;

    // `receivers` is the set of spawns that share kind+entry+mapId with
    // `canonical` (excluding the canonical itself).  Caller has already
    // filtered the model; the dialog just renders counts + GUID preview.
    PropagateFieldsDialog(render::Spawn const& canonical,
                          QVector<render::Spawn> const& receivers,
                          QWidget* parent = nullptr);

signals:
    void propagateRequested(render::Spawn const& canonical, QSet<QString> selectedFields);

private slots:
    void onPreviewAffected();
    void onApply();

private:
    QCheckBox* addCheck(QString const& label, QString const& token, bool defaultOn);

    render::Spawn                m_canonical;
    QVector<render::Spawn>       m_receivers;
    QHash<QString, QCheckBox*>   m_checks;       // token -> checkbox
    QLabel*                      m_header     = nullptr;
    QPushButton*                 m_applyButton = nullptr;
};

} // namespace world_editor::app
