/*
 * SpawnDiagnosticsDock - read-only diagnostic panels attached to the
 * currently selected spawn (creature or gameobject).  Phase 8 of the
 * world-editor handoff: surfaces the long tail of related world tables
 * the operator usually has to grep for in mysql client.
 *
 * Tabs:
 *   - SmartScripts     (smart_scripts:  source_type 0=creature, 1=GO,
 *                       9=action_list. Matched by entryorguid=entry OR
 *                       entryorguid=-guid for per-spawn overrides.)
 *   - LinkedRespawn    (linked_respawn rows where this spawn appears as
 *                       either guid or linkedGuid)
 *   - GameEvent        (game_event_creature / game_event_gameobject:
 *                       conditional spawns gated on a game event)
 *   - Transport        (creature_template/gameobject_template-flagged
 *                       transport entries, plus the `transports` row.
 *                       Only meaningful for gameobjects.)
 *
 * The dock owns a borrowed MySqlClient pointer.  Each setSelection()
 * call re-fires the four queries; if the client is null/disconnected we
 * show a placeholder string and don't query.
 */

#pragma once

#include "../db/MySqlClient.h"
#include "../render/NavMeshView.h"

#include <QWidget>

class QTabWidget;
class QTableWidget;
class QLabel;

namespace world_editor::app
{

class SpawnDiagnosticsDock final : public QWidget
{
    Q_OBJECT

public:
    explicit SpawnDiagnosticsDock(QWidget* parent = nullptr);

    void setDbClient(db::MySqlClient* client) noexcept { m_dbClient = client; }
    void setSelection(render::SpawnKind kind, int64_t guid, uint32_t entry);
    void clear();
    // Re-run the queries for the current selection.  Called by MainWindow
    // after a write so the freshly committed row shows up immediately.
    void refresh();

signals:
    // linked_respawn write requests.  MainWindow runs them through
    // ConfirmSqlDialog so the operator sees the SQL before it lands.
    void addLinkedRespawnRequested(qlonglong fromGuid, qlonglong toGuid, int linkType);
    void removeLinkedRespawnRequested(qlonglong fromGuid, int linkType);
    // game_event_creature / game_event_gameobject write requests.
    void addGameEventRequested(int eventEntry);
    void removeGameEventRequested(int eventEntry);
    // smart_scripts write requests.  PK is composite (entryorguid,
    // source_type, id, link); the edit request carries that PK so
    // MainWindow can re-SELECT the full row before opening the editor.
    void addSmartScriptRequested();
    void editSmartScriptRequested(qlonglong entryorguid, int sourceType, int id, int link);
    void removeSmartScriptRequested(qlonglong entryorguid, int sourceType, int id, int link);
    // transports write requests.  PK is `guid` alone.
    void addTransportRequested();
    void editTransportRequested(qlonglong guid);
    void removeTransportRequested(qlonglong guid);

private slots:
    void onAddLinkedRespawnClicked();
    void onRemoveLinkedRespawnClicked();
    void onAddGameEventClicked();
    void onRemoveGameEventClicked();
    void onAddSmartScriptClicked();
    void onEditSmartScriptClicked();
    void onRemoveSmartScriptClicked();
    void onAddTransportClicked();
    void onEditTransportClicked();
    void onRemoveTransportClicked();

private:
    void runSmartScripts(render::SpawnKind kind, int64_t guid, uint32_t entry);
    void runLinkedRespawn(int64_t guid);
    void runGameEvent(render::SpawnKind kind, int64_t guid);
    void runTransport(render::SpawnKind kind, uint32_t entry);

    db::MySqlClient* m_dbClient = nullptr;
    render::SpawnKind m_kind  = render::SpawnKind::Creature;
    int64_t           m_guid  = 0;
    uint32_t          m_entry = 0;
    bool              m_hasSelection = false;

    QLabel*       m_headerLabel = nullptr;
    QTabWidget*   m_tabs        = nullptr;
    QTableWidget* m_smartTable  = nullptr;
    QTableWidget* m_linkTable   = nullptr;
    QTableWidget* m_eventTable  = nullptr;
    QTableWidget* m_transTable  = nullptr;
};

} // namespace world_editor::app
