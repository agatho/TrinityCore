/*
 * HandcraftedRoadRepo - implementation. See header for design notes.
 */

#include "HandcraftedRoadRepo.h"

#include "../db/MySqlClient.h"

#include <array>
#include <cstdio>
#include <string>

namespace world_editor::io
{

namespace
{

// SELECT column ordering used by every read path. Keep loadForMap() and
// loadAll() identical so we can share the row->struct decoder.
constexpr char const* kSelectCols =
    "id, mapId, fromX, fromY, toX, toY, width, "
    "COALESCE(comment, ''), verified";

// The handcrafted_road table lives in the operator-configured shared playerbot
// schema (ConnectionParams::sharedDatabase / MySqlClient::qualify), NOT a fixed
// name. Qualifying via the client means the editor always reads/writes the
// SHARED database regardless of which DB the connection defaulted to — so road
// authoring can never again land in the wrong world DB, and the schema name is
// never hardcoded.

RoadSegment decodeRow(world_editor::db::QueryResult const& res, size_t row)
{
    RoadSegment s;
    s.id       = static_cast<uint32_t>(res.asUInt64(row, 0).value_or(0));
    s.mapId    = static_cast<uint32_t>(res.asUInt64(row, 1).value_or(0));
    s.fromX    = static_cast<float>   (res.asDouble(row, 2).value_or(0.0));
    s.fromY    = static_cast<float>   (res.asDouble(row, 3).value_or(0.0));
    s.toX      = static_cast<float>   (res.asDouble(row, 4).value_or(0.0));
    s.toY      = static_cast<float>   (res.asDouble(row, 5).value_or(0.0));
    s.width    = static_cast<float>   (res.asDouble(row, 6).value_or(8.0));
    s.comment  = QString::fromStdString(res.cell(row, 7));
    s.verified = res.asUInt64(row, 8).value_or(0) != 0;
    return s;
}

} // namespace

std::vector<RoadSegment> HandcraftedRoadRepo::loadForMap(uint32_t mapId) const
{
    std::vector<RoadSegment> out;
    if (!m_client || !m_client->isConnected())
        return out;

    std::string const tbl = m_client->qualify("handcrafted_road");
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT %s FROM %s WHERE mapId = %u ORDER BY id",
        kSelectCols, tbl.c_str(), mapId);

    world_editor::db::QueryResult res;
    auto const err = m_client->query(sql, res);
    if (!err.ok())
        return out;

    out.reserve(res.rowCount());
    for (size_t i = 0; i < res.rowCount(); ++i)
        out.push_back(decodeRow(res, i));
    return out;
}

std::vector<RoadSegment> HandcraftedRoadRepo::loadAll() const
{
    std::vector<RoadSegment> out;
    if (!m_client || !m_client->isConnected())
        return out;

    std::string const tbl = m_client->qualify("handcrafted_road");
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT %s FROM %s ORDER BY mapId, id", kSelectCols, tbl.c_str());

    world_editor::db::QueryResult res;
    auto const err = m_client->query(sql, res);
    if (!err.ok())
        return out;

    out.reserve(res.rowCount());
    for (size_t i = 0; i < res.rowCount(); ++i)
        out.push_back(decodeRow(res, i));
    return out;
}

std::optional<uint32_t> HandcraftedRoadRepo::insert(RoadSegment const& seg) const
{
    if (!m_client || !m_client->isConnected())
        return std::nullopt;

    auto err = m_client->exec("START TRANSACTION");
    if (!err.ok())
        return std::nullopt;

    // Escape the operator-supplied free text. Everything else is numeric so a
    // direct format is fine.
    std::string const commentEsc = m_client->escapeString(seg.comment.toStdString());

    std::string const tbl = m_client->qualify("handcrafted_road");
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO %s "
        "(mapId, fromX, fromY, toX, toY, width, comment, verified) "
        "VALUES (%u, %.4f, %.4f, %.4f, %.4f, %.4f, '%s', %u)",
        tbl.c_str(), seg.mapId, seg.fromX, seg.fromY, seg.toX, seg.toY, seg.width,
        commentEsc.c_str(), seg.verified ? 1u : 0u);

    uint64_t affected = 0;
    err = m_client->exec(sql, &affected);
    if (!err.ok() || affected != 1)
    {
        (void)m_client->exec("ROLLBACK");
        return std::nullopt;
    }

    uint64_t const newId = m_client->lastInsertId();
    err = m_client->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_client->exec("ROLLBACK");
        return std::nullopt;
    }
    return static_cast<uint32_t>(newId);
}

bool HandcraftedRoadRepo::update(RoadSegment const& seg) const
{
    if (!m_client || !m_client->isConnected() || seg.id == 0)
        return false;

    auto err = m_client->exec("START TRANSACTION");
    if (!err.ok())
        return false;

    std::string const commentEsc = m_client->escapeString(seg.comment.toStdString());

    std::string const tbl = m_client->qualify("handcrafted_road");
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "UPDATE %s SET "
        "mapId=%u, fromX=%.4f, fromY=%.4f, toX=%.4f, toY=%.4f, "
        "width=%.4f, comment='%s', verified=%u "
        "WHERE id=%u",
        tbl.c_str(), seg.mapId, seg.fromX, seg.fromY, seg.toX, seg.toY,
        seg.width, commentEsc.c_str(), seg.verified ? 1u : 0u, seg.id);

    uint64_t affected = 0;
    err = m_client->exec(sql, &affected);
    if (!err.ok())
    {
        (void)m_client->exec("ROLLBACK");
        return false;
    }
    // affected == 0 if the row didn't exist OR no columns changed; we treat
    // "no rows affected" as success-by-noop only when the row actually
    // exists. Check explicitly.
    if (affected == 0)
    {
        char check[128];
        std::snprintf(check, sizeof(check),
            "SELECT id FROM %s WHERE id=%u", tbl.c_str(), seg.id);
        world_editor::db::QueryResult chk;
        (void)m_client->query(check, chk);
        if (chk.rowCount() == 0)
        {
            (void)m_client->exec("ROLLBACK");
            return false;
        }
    }
    err = m_client->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_client->exec("ROLLBACK");
        return false;
    }
    return true;
}

bool HandcraftedRoadRepo::remove(uint32_t id) const
{
    if (!m_client || !m_client->isConnected() || id == 0)
        return false;

    auto err = m_client->exec("START TRANSACTION");
    if (!err.ok())
        return false;

    std::string const tbl = m_client->qualify("handcrafted_road");
    char sql[128];
    std::snprintf(sql, sizeof(sql),
        "DELETE FROM %s WHERE id=%u", tbl.c_str(), id);

    uint64_t affected = 0;
    err = m_client->exec(sql, &affected);
    if (!err.ok() || affected != 1)
    {
        (void)m_client->exec("ROLLBACK");
        return false;
    }
    err = m_client->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_client->exec("ROLLBACK");
        return false;
    }
    return true;
}

} // namespace world_editor::io
