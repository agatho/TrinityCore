/*
 * AreatriggerScriptDock - read-only panel that surfaces which C++
 * areatrigger scripts (`areatrigger.ScriptName` / `areatrigger_template.
 * ScriptName`) are referenced from the data layer and where each one is
 * spawned.
 *
 * Two modes:
 *   - scope mode (non-empty name): list every `areatrigger` row whose
 *     ScriptName matches, with a footer row per matching
 *     `areatrigger_template` (the template lookup is unioned in so a
 *     script registered ONLY on the template still shows up).  Double-
 *     click a spawn row -> jumpRequested(mapId, x, y, spawnId).
 *   - summary mode (empty name): list every distinct ScriptName seen in
 *     `areatrigger` with its spawn count.  Double-click a row narrows
 *     the scope to that script.
 *
 * The dock is late-bound: it's built before the world DB connection is
 * up, and setDbClient() is called once the operator connects.  Both
 * setScriptName() and the summary path no-op gracefully when there's no
 * connection or the table is missing.
 */

#pragma once

#include <QWidget>

#include <cstdint>
#include <optional>

class QLabel;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class AreatriggerScriptDock final : public QWidget
{
    Q_OBJECT

public:
    explicit AreatriggerScriptDock(db::MySqlClient* dbClient,
                                   QWidget* parent = nullptr);

    // Look up `name` in areatrigger + areatrigger_template and render
    // matching rows.  Empty name switches to summary mode.
    void setScriptName(QString const& name);
    void clear();

    // Late-bind the DB client (dock is built before the connection).
    void setDbClient(db::MySqlClient* db) { m_db = db; }

signals:
    // Emitted when the operator double-clicks a row carrying a SpawnId.
    // MainWindow's onJumpRequested pans the viewer to (mapId, x, y).
    void jumpRequested(uint32_t mapId, float worldX, float worldY,
                       std::optional<int64_t> spawnId);

private slots:
    void onCellDoubleClicked(int row, int col);

private:
    // Populate the table with the per-script spawn list (scope mode).
    void renderScoped(QString const& scriptName);
    // Populate the table with the global script summary (summary mode).
    void renderSummary();

    db::MySqlClient* m_db    = nullptr;
    QLabel*          m_header = nullptr;
    QTableWidget*    m_table  = nullptr;
    // Tracks the active mode so onCellDoubleClicked knows whether to
    // emit a jump or switch scope.
    bool             m_summaryMode = true;
};

} // namespace world_editor::app
