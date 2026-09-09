/*
 * AreaInfoDock - read-only panel showing zone-level info for a single
 * AreaTable.db2 area id.
 *
 * Trigger path: MainWindow's onSpawnClicked forwards the selected
 * spawn's `creature.areaId` / `gameobject.areaId` here; the dock also
 * accepts an id from anywhere else that resolves an area (currently
 * a future hook for areatrigger / graveyard probes).
 *
 * Modern TC keeps the canonical area data in AreaTable.db2 (and the
 * Map.db2 mirror for continent name) which the world DB does NOT carry
 * by default.  Some forks / older snapshots ship mirror tables under
 * one of several names (`area_dbc`, `area_table`, `areatable_dbc`,
 * `area_table_dbc`); we probe those in priority order via
 * INFORMATION_SCHEMA.COLUMNS and render whichever schema first returns
 * a row.  If none of them carries the id we surface a "no area info
 * table" message rather than asserting a single layout.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QVBoxLayout;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class AreaInfoDock final : public QWidget
{
    Q_OBJECT

public:
    explicit AreaInfoDock(db::MySqlClient* dbClient,
                          QWidget* parent = nullptr);

    // Look up `areaId` and render its zone info.  id=0 clears.
    void setArea(uint32_t areaId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Try a SELECT against `table` projecting the union of common
    // AreaTable.db2 columns; returns true on hit (dock populated).
    // Returns false on no-row / missing-table (1146) / missing-column
    // (1054) so the caller falls through to the next probe.  Notes
    // accumulate so the operator sees which tables are absent.
    bool tryPopulateFromTable(uint32_t areaId,
                              char const* table,
                              QString& outNoteMissing);

    // Resolve an area id -> display name in the same table family.
    // Empty string on miss; used for parent + breadcrumb decoration.
    QString lookupAreaName(char const* table, uint32_t areaId);

    // Walk ParentAreaID up to the root, cap depth at 6.  Returns
    // "Root > Parent > Current" with names where available, ids
    // otherwise.  `table` is the table we already proved exists.
    QString buildBreadcrumb(char const* table, uint32_t areaId);

    // Resolve a continent (map) id -> name via Map.db2 mirror tables.
    // Returns empty string if none of the candidates exists; the
    // caller renders just the id in that case.
    QString lookupContinentName(uint32_t mapId);

    db::MySqlClient* m_db;
    QLabel*          m_header        = nullptr;
    QLabel*          m_identity      = nullptr;
    QLabel*          m_hierarchy     = nullptr;
    QLabel*          m_exploration   = nullptr;
    QLabel*          m_sound         = nullptr;
    QLabel*          m_faction       = nullptr;
    QLabel*          m_misc          = nullptr;
};

} // namespace world_editor::app
