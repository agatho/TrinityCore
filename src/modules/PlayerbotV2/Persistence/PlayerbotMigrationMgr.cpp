#include "PlayerbotMigrationMgr.h"
#include "BuiltInConfig.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace Playerbot {

namespace {

std::string LoadFile(std::string const& path)
{
    std::ifstream in(path);
    if (!in.is_open()) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// V2 currently uses the character database for its tables. Long-term we'll
// likely promote to a dedicated `playerbot_v2` database, but reusing the
// existing `characters` connection avoids configuring a new pool for V1.0.
auto& Db() { return CharacterDatabase; }

} // anonymous

bool PlayerbotMigrationMgr::ensure_version_table() const
{
    Db().DirectExecute(
        "CREATE TABLE IF NOT EXISTS playerbot_v2_schema_version ("
        " version INT UNSIGNED NOT NULL PRIMARY KEY,"
        " applied_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " sha256 CHAR(64) NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    return true;
}

std::unordered_set<uint32> PlayerbotMigrationMgr::applied_versions() const
{
    std::unordered_set<uint32> out;
    auto result = Db().Query("SELECT version FROM playerbot_v2_schema_version");
    if (!result) return out;
    do
    {
        Field* fields = result->Fetch();
        if (!fields[0].IsNull())
            out.insert(fields[0].GetUInt32());
    } while (result->NextRow());
    return out;
}

bool PlayerbotMigrationMgr::apply_one(uint32 version, std::string const& sql_path)
{
    const std::string sql = LoadFile(sql_path);
    if (sql.empty())
    {
        TC_LOG_ERROR("playerbot.v2", "[PlayerbotV2] Migration {} not found at {}", version, sql_path);
        return false;
    }

    // The .sql files contain multiple statements separated by ';'.
    //
    // Pass 1 — strip whole-line `--` comments. We do this BEFORE the
    // semicolon split because comment text legitimately contains
    // semicolons ("survives logout / group disband; adds squad-...")
    // and the previous parser fed those mid-comment fragments to MySQL
    // as syntactically broken statements. The convention in our
    // migration files (and TrinityCore's) is that `--` always begins
    // at column 0 (or after pure whitespace) — we only strip lines
    // matching that, so legitimate inline `--` inside a quoted string
    // is preserved.
    std::string cleaned;
    cleaned.reserve(sql.size());
    {
        std::stringstream ss_lines(sql);
        std::string line;
        while (std::getline(ss_lines, line))
        {
            size_t i = 0;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i + 1 < line.size() && line[i] == '-' && line[i + 1] == '-')
                continue;   // comment-only line — drop entirely
            cleaned += line;
            cleaned.push_back('\n');
        }
    }

    // Pass 2 — split on ';' and execute each statement. (TrinityCore's
    // DirectExecute is single-statement.)
    std::stringstream ss(cleaned);
    std::string buf;
    std::string statement;
    while (std::getline(ss, buf, ';'))
    {
        statement = buf;
        // Trim whitespace.
        size_t s = statement.find_first_not_of(" \t\r\n");
        size_t e = statement.find_last_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        statement = statement.substr(s, e - s + 1);
        if (statement.empty()) continue;
        Db().DirectExecute(statement.c_str());
    }
    return true;
}

bool PlayerbotMigrationMgr::run_all()
{
    if (!ensure_version_table()) return false;

    auto const applied = applied_versions();
    TC_LOG_INFO("playerbot.v2",
        "[PlayerbotV2] Schema versions applied so far: {} rows",
        uint32(applied.size()));

    // Discovery is a directory scan of `sql/playerbot_v2/` (was a hardcoded
    // list capped at 3, then 8 — which silently skipped 0009/0010 and any
    // future migration). Every `NNNN_*.sql` file is a migration; the leading
    // integer is its version. We sort ascending by that integer so migrations
    // apply in order regardless of filesystem enumeration, and skip any version
    // already recorded in playerbot_v2_schema_version. New migrations need only
    // be dropped into the directory — no code change.
    //
    // The directory is resolved against the source tree (baked at build time
    // from CMAKE_SOURCE_DIR, overridable via the `SourceDirectory` config key).
    // worldserver runs from a deploy dir distinct from the source checkout, so
    // a CWD-relative path can't find the .sql files.
    std::string const sourceDir = BuiltInConfig::GetSourceDirectory();
    std::filesystem::path const migDir =
        std::filesystem::path(sourceDir) / "sql" / "playerbot_v2";

    std::error_code ec;
    if (!std::filesystem::is_directory(migDir, ec))
    {
        TC_LOG_ERROR("playerbot.v2",
            "[PlayerbotV2] Migration directory not found: {}", migDir.string());
        return false;
    }

    std::vector<std::pair<uint32, std::string>> migrations;
    for (auto const& entry : std::filesystem::directory_iterator(migDir, ec))
    {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".sql") continue;
        std::string const fname = entry.path().filename().string();
        // Leading digits = version. Skip files without a numeric prefix.
        size_t i = 0;
        while (i < fname.size() && std::isdigit(static_cast<unsigned char>(fname[i]))) ++i;
        if (i == 0) continue;
        uint32 const version = static_cast<uint32>(std::stoul(fname.substr(0, i)));
        migrations.emplace_back(version, entry.path().string());
    }
    std::sort(migrations.begin(), migrations.end(),
              [](auto const& a, auto const& b) { return a.first < b.first; });

    for (auto const& [version, fullPath] : migrations)
    {
        if (applied.contains(version)) continue;
        TC_LOG_INFO("playerbot.v2", "[PlayerbotV2] Applying migration {} from {}", version, fullPath);
        if (!apply_one(version, fullPath))
            return false;
        // The migration file itself records the row in playerbot_v2_schema_version.
    }
    return true;
}

} // namespace Playerbot
