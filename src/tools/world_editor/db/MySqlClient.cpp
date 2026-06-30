#include "MySqlClient.h"

#include <mysql.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <utility>

namespace
{
constexpr unsigned int CONNECT_FLAGS_DEFAULT = 0;
} // namespace

namespace world_editor::db
{

// ---------- Row ----------

Row::Row(char const* const* values, unsigned long const* lengths, size_t nCols)
    : m_values(values), m_lengths(lengths), m_columnCount(nCols)
{
}

bool Row::isNull(size_t col) const
{
    if (!m_values || col >= m_columnCount)
        return true;
    return m_values[col] == nullptr;
}

std::string Row::getString(size_t col) const
{
    if (!m_values || col >= m_columnCount || m_values[col] == nullptr)
        return {};
    return std::string(m_values[col], m_lengths ? m_lengths[col] : std::strlen(m_values[col]));
}

std::optional<int64_t> Row::getInt64(size_t col) const
{
    std::string const s = getString(col);
    if (s.empty()) return std::nullopt;
    int64_t v = 0;
    auto const [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{}) return std::nullopt;
    return v;
}

std::optional<uint64_t> Row::getUInt64(size_t col) const
{
    std::string const s = getString(col);
    if (s.empty()) return std::nullopt;
    uint64_t v = 0;
    auto const [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{}) return std::nullopt;
    return v;
}

std::optional<double> Row::getDouble(size_t col) const
{
    std::string const s = getString(col);
    if (s.empty()) return std::nullopt;
    try
    {
        size_t consumed = 0;
        double v = std::stod(s, &consumed);
        if (consumed == 0) return std::nullopt;
        return v;
    }
    catch (...) { return std::nullopt; }
}

// ---------- QueryResult ----------

QueryResult::QueryResult(std::vector<std::string> columnNames,
                         std::vector<std::vector<std::string>> data,
                         std::vector<std::vector<bool>> nullFlags)
    : m_columnNames(std::move(columnNames))
    , m_data(std::move(data))
    , m_nullFlags(std::move(nullFlags))
{
}

std::optional<size_t> QueryResult::columnIndex(std::string_view name) const
{
    for (size_t i = 0; i < m_columnNames.size(); ++i)
        if (m_columnNames[i] == name)
            return i;
    return std::nullopt;
}

std::optional<int64_t> QueryResult::asInt64(size_t row, size_t col) const
{
    if (isNull(row, col)) return std::nullopt;
    std::string const& s = cell(row, col);
    if (s.empty()) return std::nullopt;
    int64_t v = 0;
    auto const [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{}) return std::nullopt;
    return v;
}

std::optional<uint64_t> QueryResult::asUInt64(size_t row, size_t col) const
{
    if (isNull(row, col)) return std::nullopt;
    std::string const& s = cell(row, col);
    if (s.empty()) return std::nullopt;
    uint64_t v = 0;
    auto const [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{}) return std::nullopt;
    return v;
}

std::optional<double> QueryResult::asDouble(size_t row, size_t col) const
{
    if (isNull(row, col)) return std::nullopt;
    std::string const& s = cell(row, col);
    if (s.empty()) return std::nullopt;
    try
    {
        size_t consumed = 0;
        double v = std::stod(s, &consumed);
        if (consumed == 0) return std::nullopt;
        return v;
    }
    catch (...) { return std::nullopt; }
}

// ---------- MySqlClient ----------

namespace
{
QueryError errorFromHandle(MYSQL* h)
{
    QueryError e;
    e.code = mysql_errno(h);
    if (char const* s = mysql_sqlstate(h))
        e.sqlState = s;
    if (char const* m = mysql_error(h))
        e.message = m;
    return e;
}
} // namespace

MySqlClient::MySqlClient() = default;

MySqlClient::MySqlClient(MySqlClient&& other) noexcept
    : m_handle(std::exchange(other.m_handle, nullptr))
{
}

MySqlClient& MySqlClient::operator=(MySqlClient&& other) noexcept
{
    if (this != &other)
    {
        disconnect();
        m_handle = std::exchange(other.m_handle, nullptr);
    }
    return *this;
}

MySqlClient::~MySqlClient()
{
    disconnect();
}

void MySqlClient::disconnect() noexcept
{
    if (m_handle)
    {
        mysql_close(m_handle);
        m_handle = nullptr;
    }
}

QueryError MySqlClient::connect(ConnectionParams const& params)
{
    disconnect();

    // Remember the operator-configured shared playerbot schema so qualify()
    // can target it for road / world-metadata queries regardless of which
    // database this connection actually opened.
    m_sharedSchema = params.sharedDatabase;

    m_handle = mysql_init(nullptr);
    if (!m_handle)
    {
        QueryError e;
        e.code = 1;
        e.message = "mysql_init failed (out of memory?)";
        return e;
    }

    unsigned int const timeout = params.timeoutSecs;
    mysql_options(m_handle, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(m_handle, MYSQL_OPT_READ_TIMEOUT,    &timeout);
    mysql_options(m_handle, MYSQL_OPT_WRITE_TIMEOUT,   &timeout);
    char const* charset = "utf8mb4";
    mysql_options(m_handle, MYSQL_SET_CHARSET_NAME, charset);

    unsigned int flags = CONNECT_FLAGS_DEFAULT;
    if (params.useCompression)
        flags |= CLIENT_COMPRESS;

    char const* socket = params.socket.empty() ? nullptr : params.socket.c_str();
    char const* db     = params.database.empty() ? nullptr : params.database.c_str();

    if (!mysql_real_connect(m_handle,
                            params.host.c_str(),
                            params.user.c_str(),
                            params.password.c_str(),
                            db,
                            params.port,
                            socket,
                            flags))
    {
        QueryError e = errorFromHandle(m_handle);
        mysql_close(m_handle);
        m_handle = nullptr;
        return e;
    }
    return {};
}

std::string MySqlClient::serverVersion() const
{
    if (!m_handle) return {};
    char const* v = mysql_get_server_info(m_handle);
    return v ? std::string(v) : std::string();
}

QueryError MySqlClient::useDatabase(std::string const& dbName)
{
    if (!m_handle)
    {
        QueryError e;
        e.code = 2;
        e.message = "not connected";
        return e;
    }
    if (mysql_select_db(m_handle, dbName.c_str()) != 0)
        return errorFromHandle(m_handle);
    return {};
}

QueryError MySqlClient::query(std::string const& sql, QueryResult& outResult)
{
    outResult = QueryResult{};
    if (!m_handle)
    {
        QueryError e;
        e.code = 2;
        e.message = "not connected";
        return e;
    }

    if (mysql_real_query(m_handle, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
        return errorFromHandle(m_handle);

    MYSQL_RES* res = mysql_store_result(m_handle);
    if (!res)
    {
        // No result set (likely INSERT/UPDATE/DELETE).  Check whether
        // the server expected one - non-zero field count without a
        // result means a server-side error.
        if (mysql_field_count(m_handle) != 0)
            return errorFromHandle(m_handle);
        return {};
    }

    unsigned int const nCols = mysql_num_fields(res);
    std::vector<std::string> columnNames;
    columnNames.reserve(nCols);
    if (MYSQL_FIELD* fields = mysql_fetch_fields(res))
    {
        for (unsigned int i = 0; i < nCols; ++i)
            columnNames.emplace_back(fields[i].name ? fields[i].name : "");
    }
    else
    {
        for (unsigned int i = 0; i < nCols; ++i)
            columnNames.emplace_back();
    }

    my_ulonglong const nRows = mysql_num_rows(res);
    std::vector<std::vector<std::string>> data;
    std::vector<std::vector<bool>>        nullFlags;
    data.reserve(static_cast<size_t>(nRows));
    nullFlags.reserve(static_cast<size_t>(nRows));

    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        unsigned long const* lengths = mysql_fetch_lengths(res);
        std::vector<std::string> cells;
        std::vector<bool>        nulls;
        cells.reserve(nCols);
        nulls.reserve(nCols);
        for (unsigned int i = 0; i < nCols; ++i)
        {
            if (row[i] == nullptr)
            {
                cells.emplace_back();
                nulls.push_back(true);
            }
            else
            {
                cells.emplace_back(row[i], lengths ? lengths[i] : std::strlen(row[i]));
                nulls.push_back(false);
            }
        }
        data.emplace_back(std::move(cells));
        nullFlags.emplace_back(std::move(nulls));
    }

    mysql_free_result(res);

    outResult = QueryResult(std::move(columnNames), std::move(data), std::move(nullFlags));
    return {};
}

QueryError MySqlClient::exec(std::string const& sql, uint64_t* outAffectedRows)
{
    if (!m_handle)
    {
        QueryError e;
        e.code = 2;
        e.message = "not connected";
        return e;
    }
    if (mysql_real_query(m_handle, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
        return errorFromHandle(m_handle);
    if (outAffectedRows)
        *outAffectedRows = static_cast<uint64_t>(mysql_affected_rows(m_handle));
    return {};
}

std::string MySqlClient::escapeString(std::string_view raw) const
{
    if (!m_handle || raw.empty())
        return std::string(raw);
    std::string out;
    out.resize(raw.size() * 2 + 1);
    unsigned long const n = mysql_real_escape_string(
        m_handle, out.data(), raw.data(), static_cast<unsigned long>(raw.size()));
    out.resize(n);
    return out;
}

uint64_t MySqlClient::lastInsertId() const
{
    if (!m_handle) return 0;
    return static_cast<uint64_t>(mysql_insert_id(m_handle));
}

} // namespace world_editor::db
