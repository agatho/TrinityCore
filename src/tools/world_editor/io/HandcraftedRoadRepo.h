/*
 * HandcraftedRoadRepo - data access for the `handcrafted_road` world-DB table.
 *
 * Operators handcraft road segments in the editor; the worldserver loads them
 * at startup and tags intersected navmesh polys NAV_AREA_ROAD=7. This repo is
 * the editor-side gateway (CRUD + bulk fetch). Auto-extracted .road sidecars
 * are untouched - this table is the only operator-curated road source.
 *
 * All DML wraps START TRANSACTION / COMMIT (ROLLBACK on any failure) so a
 * mid-batch crash never leaves the table half-mutated.
 *
 * Not thread-safe - mirrors MySqlClient ownership: one repo per UI/worker
 * thread, holding its own MySqlClient.
 */

#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace world_editor::db { class MySqlClient; }

namespace world_editor::io
{

struct RoadSegment
{
    uint32_t id       = 0;     // 0 -> not yet persisted
    uint32_t mapId    = 0;
    float    fromX    = 0.0f;
    float    fromY    = 0.0f;
    float    toX      = 0.0f;
    float    toY      = 0.0f;
    float    width    = 8.0f;  // yards
    QString  comment;
    bool     verified = false;
};

class HandcraftedRoadRepo
{
public:
    explicit HandcraftedRoadRepo(world_editor::db::MySqlClient* client) noexcept
        : m_client(client) {}

    // Load every segment for a single map. Empty vector on error or no rows.
    [[nodiscard]] std::vector<RoadSegment> loadForMap(uint32_t mapId) const;

    // INSERT a new segment. Returns the new auto_increment id on success,
    // std::nullopt on failure. The input row's id field is ignored.
    [[nodiscard]] std::optional<uint32_t> insert(RoadSegment const& seg) const;

    // UPDATE every mutable column on the row with seg.id.
    [[nodiscard]] bool update(RoadSegment const& seg) const;

    // DELETE the row with this id.
    [[nodiscard]] bool remove(uint32_t id) const;

    // Bulk fetch used by worldserver-sync flows (operator export, diff vs
    // live worldserver, etc.).
    [[nodiscard]] std::vector<RoadSegment> loadAll() const;

private:
    world_editor::db::MySqlClient* m_client = nullptr;
};

} // namespace world_editor::io
