/*
 * ZoneSummaryDock - read-only headline-stats panel for the zone that the
 * currently-hovered/clicked spawn lives in.  Fed via setZone(zoneId, mapId)
 * from MainWindow's onSpawnClicked path so the operator gets a one-glance
 * inventory of what populates the current zone (counts of creatures, GOs,
 * quest-givers, vendors, trainers, mailboxes, ...; level range; graveyards).
 *
 * Design mirrors AreaInfoDock:
 *   - Late-bound MySqlClient* (constructed before DB connection is open).
 *   - Schema-tolerant on AreaTable name lookup (probes the same mirror-
 *     table family AreaInfoDock walks).  All other queries hit canonical
 *     world DB tables (creature, gameobject, creature_template,
 *     gameobject_template, graveyard_zone) so they're stable across forks.
 *   - Result-cache keyed on zoneId.  Repeated hover over a zone or
 *     repeated clicks on different spawns in the same zone reuse the
 *     cached counts so we don't re-hit the DB.  Cleared on clear() and
 *     on setDbClient() reconnection.
 *
 * Total dock refresh budget: ~600ms cold (each COUNT query takes ~50ms
 * on a fresh world DB; we fire 12).  Warm refresh of a cached zone is
 * instantaneous - no DB hits at all.
 */

#pragma once

#include <QWidget>

#include <cstdint>
#include <unordered_map>

class QLabel;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class ZoneSummaryDock final : public QWidget
{
    Q_OBJECT

public:
    explicit ZoneSummaryDock(db::MySqlClient* dbClient,
                             QWidget* parent = nullptr);

    // Render headline stats for `zoneId` on `mapId`.  zoneId==0 clears.
    void setZone(uint32_t zoneId, uint32_t mapId);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Drops the result cache so a fresh
    // connection re-queries instead of serving stale rows.
    void setDbClient(db::MySqlClient* db);

private:
    // Resolve a zone id -> display name via AreaTable mirror probes.
    // Returns "Zone <id>" on miss so the header is always populated.
    QString lookupZoneName(uint32_t zoneId);

    // One COUNT(*) query against `sql`, returning 0 on error / absent
    // table.  Logs query failure into the "errors" pile so a single
    // broken table doesn't take down the whole dock.
    uint64_t scalarCount(QString const& sql);

    // Materialized snapshot of every count we surface for one zone.  The
    // cache holds these by zoneId so repeated hover is free.
    struct Stats
    {
        uint64_t creatures      = 0;
        uint64_t gameobjects    = 0;
        uint64_t questGivers    = 0;
        uint64_t vendors        = 0;
        uint64_t innkeepers     = 0;
        uint64_t trainers       = 0;
        uint64_t battlemasters  = 0;
        uint64_t mailboxes      = 0;
        uint64_t auctioneers    = 0;
        uint64_t flightmasters  = 0;
        uint64_t graveyards     = 0;
        int32_t  minLevel       = -1;   // -1 sentinel = no rows / NULL.
        int32_t  maxLevel       = -1;
        QString  zoneName;
    };

    // Run all 12 queries for the (zoneId, mapId) pair.  Returns a fully-
    // populated Stats; caller decides whether to cache.
    Stats computeStats(uint32_t zoneId, uint32_t mapId);

    // Render a populated Stats into the dock labels.
    void renderStats(uint32_t zoneId, Stats const& s);

    db::MySqlClient* m_db        = nullptr;
    QLabel*          m_header    = nullptr;
    QLabel*          m_counts    = nullptr;
    QLabel*          m_levelRow  = nullptr;
    QLabel*          m_errors    = nullptr;

    // Zone-id-keyed cache.  We don't key on mapId too because TC's zone
    // ids are globally unique - the same zoneId never appears on two
    // different maps.  If a future fork breaks that invariant the cache
    // key should be widened to (mapId<<32 | zoneId).
    std::unordered_map<uint32_t, Stats> m_cache;
    // Error string accumulator filled by scalarCount(); rendered on the
    // m_errors label when non-empty.  Wiped at the start of each query
    // pass so stale failures don't linger.
    QString          m_pendingErrors;
};

} // namespace world_editor::app
