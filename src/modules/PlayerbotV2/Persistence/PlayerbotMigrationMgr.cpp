#include "PlayerbotMigrationMgr.h"
#include "BuiltInConfig.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
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

// Both targets run over the character-database connection. Realm-scoped
// statements use it directly (it is the connection's default schema); shared
// statements reach the other schema by qualifying their table names. That keeps
// V2 on a single connection pool - no second pool to configure or exhaust.
auto& Db() { return CharacterDatabase; }

// Name of the shared playerbot schema. Must match the schema the shared SQL was
// installed into (sql/playerbot_shared/). Read once - a mid-run change would
// leave a migration half-applied across two schemas.
std::string const& SharedDb()
{
    static std::string const db =
        sConfigMgr->GetStringDefault("Playerbot.SharedDatabase", "playerbot");
    return db;
}

// Replace every {SHARED} placeholder with the backtick-quoted schema name, so
// `{SHARED}.playerbot_v2_talent_build` becomes `playerbot`.`playerbot_v2_talent_build`.
std::string SubstituteShared(std::string s)
{
    static constexpr std::string_view kToken = "{SHARED}";
    std::string const replacement = "`" + SharedDb() + "`";
    for (size_t pos = s.find(kToken); pos != std::string::npos;
         pos = s.find(kToken, pos + replacement.size()))
        s.replace(pos, kToken.size(), replacement);
    return s;
}

// Parse a `-- @target: shared|characters` directive. Returns false when the
// line is not a target directive at all.
bool ParseTargetDirective(std::string const& line, MigrationTarget& out)
{
    size_t i = line.find("--");
    if (i == std::string::npos) return false;
    size_t at = line.find("@target:", i);
    if (at == std::string::npos) return false;
    std::string value = line.substr(at + 8);
    // Trim and lowercase.
    size_t s = value.find_first_not_of(" \t\r\n");
    if (s == std::string::npos) return false;
    size_t e = value.find_last_not_of(" \t\r\n");
    value = value.substr(s, e - s + 1);
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (value == "shared")     { out = MigrationTarget::Shared;     return true; }
    if (value == "characters") { out = MigrationTarget::Characters; return true; }
    return false;
}

char const* TargetName(MigrationTarget t)
{
    return t == MigrationTarget::Shared ? "shared" : "characters";
}

// Ledger table, qualified for the target.
std::string VersionTable(MigrationTarget t)
{
    return t == MigrationTarget::Shared
        ? "`" + SharedDb() + "`.playerbot_v2_schema_version"
        : std::string("playerbot_v2_schema_version");
}

} // anonymous

bool PlayerbotMigrationMgr::ensure_version_table(MigrationTarget target) const
{
    Db().DirectExecute(
        ("CREATE TABLE IF NOT EXISTS " + VersionTable(target) + " ("
         " version INT UNSIGNED NOT NULL PRIMARY KEY,"
         " applied_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
         " sha256 CHAR(64) NOT NULL"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4").c_str());
    return true;
}

std::unordered_set<uint32> PlayerbotMigrationMgr::applied_versions(MigrationTarget target) const
{
    std::unordered_set<uint32> out;
    auto result = Db().Query(("SELECT version FROM " + VersionTable(target)).c_str());
    if (!result) return out;
    do
    {
        Field* fields = result->Fetch();
        if (!fields[0].IsNull())
            out.insert(fields[0].GetUInt32());
    } while (result->NextRow());
    return out;
}

bool PlayerbotMigrationMgr::apply_one(uint32 version, std::string const& sql_path,
                                      std::unordered_set<uint32> const& applied_characters,
                                      std::unordered_set<uint32> const& applied_shared)
{
    const std::string sql = LoadFile(sql_path);
    if (sql.empty())
    {
        TC_LOG_ERROR("playerbot.v2", "[PlayerbotV2] Migration {} not found at {}", version, sql_path);
        return false;
    }

    // The .sql files contain multiple statements separated by ';'.
    //
    // Pass 1 — strip whole-line `--` comments, but FIRST read any
    // `-- @target:` directive off the line, because the directive lives in a
    // comment and would otherwise be discarded here. Each retained statement is
    // tagged with the target in force when it was read.
    //
    // We strip comments BEFORE the semicolon split because comment text
    // legitimately contains semicolons ("survives logout / group disband; adds
    // squad-...") and the previous parser fed those mid-comment fragments to
    // MySQL as syntactically broken statements. The convention in our migration
    // files (and TrinityCore's) is that `--` always begins at column 0 (or after
    // pure whitespace) — we only strip lines matching that, so a legitimate
    // inline `--` inside a quoted string is preserved.
    //
    // Default is Characters: a file with no directive behaves exactly as it did
    // before routing existed, which is what keeps the 16 existing migrations
    // applying to the character database unchanged.
    std::vector<std::pair<MigrationTarget, std::string>> statements;
    {
        MigrationTarget current = MigrationTarget::Characters;
        std::string pending;

        auto flush = [&](std::string const& chunk)
        {
            size_t s = chunk.find_first_not_of(" \t\r\n");
            size_t e = chunk.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) return;
            statements.emplace_back(current, chunk.substr(s, e - s + 1));
        };

        std::stringstream ss_lines(sql);
        std::string line;
        while (std::getline(ss_lines, line))
        {
            size_t i = 0;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            bool const comment_only = (i + 1 < line.size() && line[i] == '-' && line[i + 1] == '-');

            if (comment_only)
            {
                MigrationTarget parsed;
                if (ParseTargetDirective(line, parsed))
                {
                    // A directive terminates whatever statement is being
                    // accumulated, so a file can never straddle two targets
                    // inside one statement.
                    flush(pending);
                    pending.clear();
                    current = parsed;
                }
                continue;   // comment-only line — drop entirely
            }

            pending += line;
            pending.push_back('\n');

            // Split off every complete statement in what we have so far.
            size_t semi;
            while ((semi = pending.find(';')) != std::string::npos)
            {
                flush(pending.substr(0, semi));
                pending.erase(0, semi + 1);
            }
        }
        flush(pending);     // trailing statement without a closing ';'
    }

    // Pass 2 — execute each statement against its target. (TrinityCore's
    // DirectExecute is single-statement.) A target whose ledger already records
    // this version is skipped, so re-running a file that spans both databases
    // tops up only the side that is behind.
    uint32 ran_characters = 0;
    uint32 ran_shared = 0;
    for (auto const& [target, statement] : statements)
    {
        if (target == MigrationTarget::Characters)
        {
            if (applied_characters.contains(version)) continue;
            Db().DirectExecute(statement.c_str());
            ++ran_characters;
        }
        else
        {
            if (applied_shared.contains(version)) continue;
            Db().DirectExecute(SubstituteShared(statement).c_str());
            ++ran_shared;
        }
    }

    // Record the version in the ledger of every target we actually wrote to.
    // INSERT IGNORE because the older migration files also self-record into the
    // character-side ledger; a duplicate key there is expected, not an error.
    if (ran_characters)
        Db().DirectExecute(("INSERT IGNORE INTO " + VersionTable(MigrationTarget::Characters) +
                            " (version, sha256) VALUES (" + std::to_string(version) + ", '')").c_str());
    if (ran_shared)
        Db().DirectExecute(("INSERT IGNORE INTO " + VersionTable(MigrationTarget::Shared) +
                            " (version, sha256) VALUES (" + std::to_string(version) + ", '')").c_str());

    TC_LOG_INFO("playerbot.v2",
        "[PlayerbotV2] Migration {}: {} statement(s) to characters, {} to shared ({}).",
        version, ran_characters, ran_shared, SharedDb());
    return true;
}

bool PlayerbotMigrationMgr::run_all()
{
    // Both ledgers, because a migration may write to either database and
    // "already applied" is a per-database fact.
    if (!ensure_version_table(MigrationTarget::Characters)) return false;
    if (!ensure_version_table(MigrationTarget::Shared))
    {
        TC_LOG_ERROR("playerbot.v2",
            "[PlayerbotV2] Could not reach the shared schema `{}`. Create it and apply "
            "sql/playerbot_shared/playerbot_shared_schema.sql, or correct "
            "Playerbot.SharedDatabase.", SharedDb());
        return false;
    }

    auto const applied_characters = applied_versions(MigrationTarget::Characters);
    auto const applied_shared     = applied_versions(MigrationTarget::Shared);
    TC_LOG_INFO("playerbot.v2",
        "[PlayerbotV2] Schema versions applied so far: {} in characters, {} in shared (`{}`)",
        uint32(applied_characters.size()), uint32(applied_shared.size()), SharedDb());

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
        // Only skip outright when BOTH ledgers already have it. A file that
        // spans the two databases is otherwise re-read so apply_one can top up
        // whichever side is behind — that is how an existing realm, which has
        // versions 1..15 in characters and nothing in shared, gets its shared
        // tables created without re-running the character-side statements.
        if (applied_characters.contains(version) && applied_shared.contains(version))
            continue;
        TC_LOG_INFO("playerbot.v2", "[PlayerbotV2] Applying migration {} from {}", version, fullPath);
        if (!apply_one(version, fullPath, applied_characters, applied_shared))
            return false;
    }
    return true;
}

} // namespace Playerbot
