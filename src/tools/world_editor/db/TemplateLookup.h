/*
 * TemplateLookup - shared, Qt-free lookup over the world-DB content templates
 * (creature_template / gameobject_template / quest_template).
 *
 * Today every dialog that references a template entry (TemplatePickerDialog plus
 * ~30 others) runs its own inline SELECT against these tables. This consolidates
 * that into one reusable service used by the pickers AND the spawn-placement
 * FK-validation gate ("only entries that exist in *_template can be placed"),
 * removing the duplication and giving a single headless-testable surface.
 *
 * std::string in/out (matches MySqlClient); the GUI layer converts to QString.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace world_editor::db
{

class MySqlClient;

class TemplateLookup
{
public:
    enum class Table
    {
        Creature,    // creature_template (entry, name)
        GameObject,  // gameobject_template (entry, name)
        Quest,       // quest_template (ID, LogTitle)
    };

    struct Row
    {
        uint32_t    entry = 0;
        std::string name;
    };

    TemplateLookup(MySqlClient* db, std::string worldDb);

    // Resolve an entry to its display name. Empty string if the entry does not
    // exist (or on a query error).
    [[nodiscard]] std::string name(Table table, uint32_t entry) const;

    // FK-validation gate: does this template entry exist?
    [[nodiscard]] bool exists(Table table, uint32_t entry) const;

    // Load every (entry, name) row of a table, ordered by entry. Used by the
    // picker's "load all once + filter client-side" model; can be large (TC
    // ships 70k+ creature_template rows).
    [[nodiscard]] std::vector<Row> loadAll(Table table) const;

    // Search by a numeric entry and/or a case-insensitive name substring.
    // A purely numeric query also matches the exact entry id. Results are
    // ordered by entry and capped at `limit`.
    [[nodiscard]] std::vector<Row> search(Table table, std::string const& query,
                                          int limit = 200) const;

private:
    struct TableInfo { char const* table; char const* idCol; char const* nameCol; };
    [[nodiscard]] TableInfo info(Table table) const;

    MySqlClient* m_db = nullptr;
    std::string  m_worldDb;
};

} // namespace world_editor::db
