/*
 * MySqlClient - thin libmysql wrapper for the world editor.
 *
 * Why not Qt's QSqlDatabase?  Qt 6 dropped the in-tree MySQL plugin
 * (licensing + maintenance), and we'd otherwise have to build it from
 * source against libmysql for every Qt version bump.  Direct libmysql
 * is what TC's own database/Database layer uses anyway, so we're
 * mirroring the same primitive at a smaller scale.
 *
 * Design:
 *   - One MySqlClient = one connection.  No pool.  The editor's write
 *     model is "batched commit on user button press", so one connection
 *     per session is enough.
 *   - All methods are blocking.  Callers that need async fire-and-
 *     forget queries should hand a MySqlClient over to a worker thread
 *     (QtConcurrent::run, std::thread, ...).
 *   - String fields come back as std::string (UTF-8).  Integers and
 *     floats are parsed lazily via convenience accessors on Row.
 *   - NULL fields are surfaced via Row::isNull(col); never as the
 *     string "NULL".
 *   - Errors are returned via Result::ok + Result::error.  No exceptions.
 *
 * The class does NOT pull in QtSql or Qt's database module - it's pure
 * standalone C++ + libmysql, the same pattern as MMapReader/MapReader.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward-declare the libmysql opaque handle so this header doesn't
// drag mysql.h into the rest of the editor.
struct MYSQL;
struct MYSQL_RES;

namespace world_editor::db
{

struct ConnectionParams
{
    std::string host          = "127.0.0.1";
    uint16_t    port          = 3306;
    std::string user          = "root";
    std::string password      = "";
    std::string database      = "world";  // default DB the editor opens; can be empty.
    // Shared playerbot schema (handcrafted_road + playerbot_v2_world_metadata
    // live here, NOT in the world/character DB). Operator-configured — must
    // match the server's Playerbot.SharedDatabase. Empty -> queries use the
    // connection's default DB (legacy behaviour).
    std::string sharedDatabase = "";
    std::string socket        = "";       // unix-socket path; empty -> TCP.
    uint32_t    timeoutSecs   = 5;
    bool        useCompression = false;
};

struct QueryError
{
    uint32_t    code    = 0;     // 0 means no error.
    std::string sqlState;
    std::string message;
    bool ok() const noexcept { return code == 0; }
};

class Row
{
public:
    Row() = default;
    Row(char const* const* values, unsigned long const* lengths, size_t nCols);

    [[nodiscard]] size_t size() const noexcept { return m_columnCount; }
    [[nodiscard]] bool   isNull(size_t col) const;
    [[nodiscard]] std::string getString(size_t col) const;
    [[nodiscard]] std::optional<int64_t>  getInt64 (size_t col) const;
    [[nodiscard]] std::optional<uint64_t> getUInt64(size_t col) const;
    [[nodiscard]] std::optional<double>   getDouble(size_t col) const;

private:
    // libmysql owns the row buffer; we just keep pointers into it
    // alongside the per-column length array.
    char const* const* m_values  = nullptr;
    unsigned long const* m_lengths = nullptr;
    size_t m_columnCount = 0;
};

// Owning result set.  Iterate via for (auto const& row : result.rows()) ...
// The row vector is fully materialized at fetch time, so the underlying
// MYSQL_RES* is freed by the time the caller iterates.
class QueryResult
{
public:
    QueryResult() = default;
    QueryResult(std::vector<std::string> columnNames,
                std::vector<std::vector<std::string>> data,
                std::vector<std::vector<bool>> nullFlags);
    QueryResult(QueryResult&&) noexcept            = default;
    QueryResult& operator=(QueryResult&&) noexcept = default;

    [[nodiscard]] size_t columnCount() const noexcept { return m_columnNames.size(); }
    [[nodiscard]] size_t rowCount()    const noexcept { return m_data.size(); }
    [[nodiscard]] std::string const& columnName(size_t i) const { return m_columnNames.at(i); }
    [[nodiscard]] std::optional<size_t> columnIndex(std::string_view name) const;

    // Row accessors.  Throws std::out_of_range on bad indices.
    [[nodiscard]] std::string const& cell(size_t row, size_t col) const { return m_data.at(row).at(col); }
    [[nodiscard]] bool isNull(size_t row, size_t col) const             { return m_nullFlags.at(row).at(col); }

    // Whole-row helpers.
    [[nodiscard]] std::optional<int64_t>  asInt64 (size_t row, size_t col) const;
    [[nodiscard]] std::optional<uint64_t> asUInt64(size_t row, size_t col) const;
    [[nodiscard]] std::optional<double>   asDouble(size_t row, size_t col) const;

private:
    std::vector<std::string>              m_columnNames;
    std::vector<std::vector<std::string>> m_data;       // [row][col] - raw textual repr.
    std::vector<std::vector<bool>>        m_nullFlags;  // [row][col]
};

class MySqlClient
{
public:
    MySqlClient();
    ~MySqlClient();
    MySqlClient(MySqlClient const&)            = delete;
    MySqlClient& operator=(MySqlClient const&) = delete;
    MySqlClient(MySqlClient&&) noexcept;
    MySqlClient& operator=(MySqlClient&&) noexcept;

    // Open a new connection.  Closes the previous one first.
    [[nodiscard]] QueryError connect(ConnectionParams const& params);
    void disconnect() noexcept;

    [[nodiscard]] bool isConnected() const noexcept { return m_handle != nullptr; }

    // Operator-configured shared playerbot schema (from ConnectionParams::
    // sharedDatabase). Empty when unset.
    [[nodiscard]] std::string const& sharedSchema() const noexcept { return m_sharedSchema; }
    // Qualify a shared table with the shared schema, e.g. "handcrafted_road" ->
    // "wowc_playerbot.handcrafted_road". When no shared schema is set, returns
    // the bare table (uses the connection's default DB). Keeps the schema name
    // out of every call site so it can never be hardcoded.
    [[nodiscard]] std::string qualify(std::string_view table) const
    {
        if (m_sharedSchema.empty())
            return std::string(table);
        return m_sharedSchema + "." + std::string(table);
    }

    // Server-side version string (e.g. "9.4.0-x86_64-MySQL").  Empty if
    // not connected.
    [[nodiscard]] std::string serverVersion() const;

    // Switch active database.  Equivalent to "USE <db>".
    [[nodiscard]] QueryError useDatabase(std::string const& dbName);

    // Run a SELECT and materialize the result.  For zero-row results
    // QueryResult.rowCount() returns 0; for non-SELECT queries (INSERT,
    // UPDATE, DELETE) returns an empty QueryResult and sets
    // outAffectedRows.
    [[nodiscard]] QueryError query(std::string const& sql, QueryResult& outResult);
    [[nodiscard]] QueryError exec (std::string const& sql, uint64_t* outAffectedRows = nullptr);

    // Identifier and string escaping.  Use these and never string-concat
    // user input into a query.
    [[nodiscard]] std::string escapeString(std::string_view raw) const;

    // Last-insert id (auto_increment) after the most recent INSERT.
    [[nodiscard]] uint64_t lastInsertId() const;

    // Raw handle escape hatch for prepared-statement code that wants
    // libmysql directly.  Editor commit paths will use prepared
    // statements once the diff layer lands.
    [[nodiscard]] MYSQL* raw() const noexcept { return m_handle; }

private:
    MYSQL* m_handle = nullptr;
    std::string m_sharedSchema;   // copied from ConnectionParams::sharedDatabase on connect()
};

} // namespace world_editor::db
