/*
 * SmartScriptDryRunDialog - heuristic "what would fire?" simulator for a
 * given (entryorguid, source_type) smart_scripts ruleset.
 *
 * Inputs (operator-supplied via the form at top):
 *   - eventType        : which SMART_EVENT to test
 *   - phaseMask        : zero-based phase index the simulated SAI is in
 *   - simulated_hp_pct : HP percent for HEALTH_PCT (event 2) / FRIENDLY (4)
 *                        range matching
 *   - is_in_combat     : gate for combat-only triggers (UPDATE_IC, AGGRO,
 *                        RESET, DAMAGED ...)
 *   - event_param_value: a single test number plugged into whatever the
 *                        chosen event's primary param expects (e.g.
 *                        spell id for SPELLHIT, emote id for RECEIVE_EMOTE)
 *
 * Output:
 *   Plain-text trace dumped into a read-only QTextEdit.  Each rule that
 *   matches the chosen (eventType, phaseMask, hp, combat, param) gate
 *   produces a "FIRE id=N link=L: ACTION(params)  // comment" line; any
 *   `link` chain originating from a fired rule is recursively expanded.
 *   Rules that miss the gate produce a single-line skip explanation so
 *   the operator can see WHY a rule didn't fire (wrong event type, wrong
 *   phase, out-of-range hp, deterministic-chance-skip, etc.).
 *
 * The simulation is best-effort: true SAI is driven by runtime world
 * state (target acquisition, line-of-sight, talked-to events, charm,
 * passenger transitions, ...) and only the deterministically-checkable
 * pieces are modelled here.  Anything else is reported as "assumed
 * present" so the operator sees the rule as a candidate without a false
 * negative.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>
#include <unordered_map>
#include <vector>

class QCheckBox;
class QPushButton;
class QSpinBox;
class QTextEdit;

namespace world_editor::db { class MySqlClient; }
namespace world_editor::render { struct SmartScript; }

namespace world_editor::app
{

class SmartScriptDryRunDialog final : public QDialog
{
    Q_OBJECT

public:
    SmartScriptDryRunDialog(db::MySqlClient* dbClient,
                            int64_t entryOrGuid,
                            uint8_t sourceType,
                            QWidget* parent = nullptr);

private slots:
    void onRun();

private:
    // Fetch every smart_scripts row matching the PK prefix (entryorguid,
    // source_type), sorted by id then link.
    [[nodiscard]] std::vector<render::SmartScript>
        fetchRules(int64_t entryOrGuid, uint8_t sourceType);

    // Single-rule trace step: returns true if the rule passes its event
    // gate and is logged as FIRE; false otherwise (with the skip reason
    // logged via appendLine).  The "depth" controls indentation in the
    // text trace and bounds recursion through `link`.
    bool simulateRule(render::SmartScript const& rule,
                      std::vector<render::SmartScript> const& allRules,
                      int depth);

    // Append a line to the trace text edit with `depth` levels of
    // 2-space indentation prefixed.
    void appendLine(int depth, QString const& line);

    // Format helper - decodes the action via the SMART_ACTION enum table.
    [[nodiscard]] QString formatActionDecoded(render::SmartScript const& r) const;

    db::MySqlClient* m_db;
    int64_t          m_entryOrGuid;
    uint8_t          m_sourceType;

    QSpinBox*        m_eventTypeSpin    = nullptr;
    QSpinBox*        m_phaseMaskSpin    = nullptr;
    QSpinBox*        m_hpPctSpin        = nullptr;
    QCheckBox*       m_inCombatCheck    = nullptr;
    QSpinBox*        m_eventParamSpin   = nullptr;
    QPushButton*     m_runButton        = nullptr;
    QTextEdit*       m_trace            = nullptr;

    // Cycle guard for `link` chain recursion.
    static constexpr int kMaxDepth = 16;
};

} // namespace world_editor::app
