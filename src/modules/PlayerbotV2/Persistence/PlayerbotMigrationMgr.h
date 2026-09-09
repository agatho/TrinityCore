// PlayerbotMigrationMgr - Runs sql/playerbot_v2/*.sql migrations at startup.
// Idempotent: tracked in playerbot_v2_schema_version.

#pragma once

#include "Bot/BotTypes.h"
#include <string>
#include <unordered_set>

namespace Playerbot {

class PlayerbotMigrationMgr
{
public:
    // Returns true if all required migrations applied (or were already applied).
    // Returns false if a migration failed; caller should refuse module init.
    bool run_all();

    // Set of versions present in playerbot_v2_schema_version. Empty when
    // the table doesn't exist yet. We use a set rather than a MAX(version)
    // scalar so a migration registered LATER with a LOWER version (e.g.
    // 0006 added after 0007/0008 already ran in a previous build) still
    // gets applied — `MAX(version)` would mark every gap as "already done".
    std::unordered_set<uint32> applied_versions() const;

private:
    bool ensure_version_table() const;
    bool apply_one(uint32 version, std::string const& sql_path);
};

} // namespace Playerbot
