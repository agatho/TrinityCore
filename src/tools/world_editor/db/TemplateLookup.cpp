// TemplateLookup.cpp - see TemplateLookup.h for the contract.

#include "TemplateLookup.h"

#include "MySqlClient.h"

#include <algorithm>
#include <cctype>

namespace world_editor::db
{

TemplateLookup::TemplateLookup(MySqlClient* db, std::string worldDb)
    : m_db(db), m_worldDb(std::move(worldDb))
{
}

TemplateLookup::TableInfo TemplateLookup::info(Table table) const
{
    switch (table)
    {
        case Table::Creature:   return { "creature_template",   "entry", "name" };
        case Table::GameObject: return { "gameobject_template", "entry", "name" };
        case Table::Quest:      return { "quest_template",      "ID",    "LogTitle" };
    }
    return { "creature_template", "entry", "name" };
}

std::string TemplateLookup::name(Table table, uint32_t entry) const
{
    if (!m_db || !m_db->isConnected())
        return {};
    TableInfo const ti = info(table);
    std::string const sql =
        "SELECT COALESCE(`" + std::string(ti.nameCol) + "`, '') FROM `" + m_worldDb +
        "`.`" + ti.table + "` WHERE `" + ti.idCol + "` = " + std::to_string(entry) + " LIMIT 1";
    QueryResult res;
    if (!m_db->query(sql, res).ok() || res.rowCount() == 0)
        return {};
    return res.cell(0, 0);
}

bool TemplateLookup::exists(Table table, uint32_t entry) const
{
    if (!m_db || !m_db->isConnected())
        return false;
    TableInfo const ti = info(table);
    std::string const sql =
        "SELECT 1 FROM `" + m_worldDb + "`.`" + ti.table + "` WHERE `" + ti.idCol +
        "` = " + std::to_string(entry) + " LIMIT 1";
    QueryResult res;
    return m_db->query(sql, res).ok() && res.rowCount() > 0;
}

std::vector<TemplateLookup::Row> TemplateLookup::loadAll(Table table) const
{
    std::vector<Row> out;
    if (!m_db || !m_db->isConnected())
        return out;
    TableInfo const ti = info(table);
    std::string const sql =
        "SELECT `" + std::string(ti.idCol) + "`, COALESCE(`" + ti.nameCol + "`, '') FROM `" +
        m_worldDb + "`.`" + ti.table + "` ORDER BY `" + ti.idCol + "`";
    QueryResult res;
    if (!m_db->query(sql, res).ok())
        return out;
    out.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        Row row;
        row.entry = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        row.name  = res.cell(r, 1);
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<TemplateLookup::Row> TemplateLookup::search(Table table,
                                                        std::string const& query,
                                                        int limit) const
{
    std::vector<Row> out;
    if (!m_db || !m_db->isConnected())
        return out;
    if (limit <= 0)
        limit = 200;

    TableInfo const ti = info(table);

    std::string trimmed = query;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))  trimmed.pop_back();

    bool const numeric = !trimmed.empty() &&
        std::all_of(trimmed.begin(), trimmed.end(),
                    [](unsigned char c) { return std::isdigit(c) != 0; });

    std::string where;
    if (trimmed.empty())
    {
        where = "1"; // no filter -> first `limit` rows
    }
    else
    {
        std::string const esc = m_db->escapeString(trimmed);
        where = "`" + std::string(ti.nameCol) + "` LIKE '%" + esc + "%'";
        if (numeric)
            where = "(`" + std::string(ti.idCol) + "` = " + trimmed + " OR " + where + ")";
    }

    std::string const sql =
        "SELECT `" + std::string(ti.idCol) + "`, COALESCE(`" + ti.nameCol + "`, '') FROM `" +
        m_worldDb + "`.`" + ti.table + "` WHERE " + where +
        " ORDER BY `" + ti.idCol + "` LIMIT " + std::to_string(limit);

    QueryResult res;
    if (!m_db->query(sql, res).ok())
        return out;

    out.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        Row row;
        row.entry = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        row.name  = res.cell(r, 1);
        out.push_back(std::move(row));
    }
    return out;
}

} // namespace world_editor::db
