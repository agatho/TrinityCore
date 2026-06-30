/*
 * FindSimilarDialog - "find spawns similar to selected" modal.
 *
 * Given one reference spawn (the user's current selection in the
 * viewer / spawn dock), build a creature SELECT dynamically from a set
 * of similarity checkboxes:
 *   - same entry (auto-on; the canonical "another of these")
 *   - same map
 *   - same zoneId  (creature.zoneId - denormalized on the spawn row)
 *   - same areaId  (creature.areaId)
 *   - same PhaseId
 *   - same PhaseGroup
 *   - same npcflag template field (joins creature_template)
 *   - same faction template field (joins creature_template)
 *   - within radius yards in XY plane (default 50)
 *
 * Checkboxes left off simply omit their AND clause.  The dialog only
 * handles creatures: GO spawns don't carry npcflag/faction template
 * fields and SpawnSearchDialog already covers cross-kind name/loot
 * filtering, so "find similar" is creature-flavored.
 *
 * Double-clicking a result row (or pressing Jump) emits
 * jumpRequested(mapId, x, y, optional<guid>) - same signature as
 * SpawnSearchDialog / FindJumpDialog so MainWindow's existing
 * onJumpRequested slot wires in directly.
 *
 * Triggered via Spawn -> Find similar spawns... (enabled when exactly
 * one spawn is selected).
 */

#pragma once

#include "../db/MySqlClient.h"
#include "../render/NavMeshView.h"  // render::Spawn

#include <QDialog>

#include <cstdint>
#include <optional>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QTableWidget;
class QLabel;

namespace world_editor::app
{

class FindSimilarDialog final : public QDialog
{
    Q_OBJECT

public:
    FindSimilarDialog(db::MySqlClient* db, render::Spawn const& reference, QWidget* parent = nullptr);

signals:
    void jumpRequested(uint32_t mapId, float worldX, float worldY, std::optional<int64_t> guid);

private slots:
    void onRun();
    void onJumpSelected();

private:
    void emitJumpFromRow(int row);

    db::MySqlClient* m_db = nullptr;
    render::Spawn    m_ref;            // value copy: dialog can outlive selection clear

    // Template-derived fields for the reference spawn, fetched lazily
    // from creature_template at construction so npcflag/faction
    // checkboxes have authoritative values to filter against.
    bool      m_refTemplateLoaded = false;
    uint64_t  m_refNpcFlag        = 0;
    uint32_t  m_refFaction        = 0;

    QCheckBox*       m_cbEntry       = nullptr;
    QCheckBox*       m_cbMap         = nullptr;
    QCheckBox*       m_cbZone        = nullptr;
    QCheckBox*       m_cbArea        = nullptr;
    QCheckBox*       m_cbPhaseId     = nullptr;
    QCheckBox*       m_cbPhaseGroup  = nullptr;
    QCheckBox*       m_cbNpcFlag     = nullptr;
    QCheckBox*       m_cbFaction     = nullptr;
    QCheckBox*       m_cbRadius      = nullptr;
    QDoubleSpinBox*  m_radiusYards   = nullptr;

    QPushButton*  m_runBtn   = nullptr;
    QPushButton*  m_jumpBtn  = nullptr;
    QTableWidget* m_results  = nullptr;
    QLabel*       m_statusLbl = nullptr;
};

} // namespace world_editor::app
