// PlayerbotMigrationMgr - Runs sql/playerbot_v2/*.sql migrations at startup.
// Idempotent: tracked in playerbot_v2_schema_version.
//
// Migrations are routed per statement to one of two databases. Playerbot data
// splits cleanly by scope:
//
//   * REALM-SCOPED data keys on a character guid, an account id or this realm's
//     operational state - playerbot_v2_character, _personality, _preferences,
//     _squad_preset, _account, bot_guild_meta/_member_meta/_name_reserved,
//     bot_craft_orders, _fleet_vitals_sample, _population_target. These live in
//     the realm's character database. Pooling them into a schema shared by
//     several realms would collide: character guid 12442 exists on every realm.
//
//   * STATIC content is identical on every realm - _talent_build (class/spec
//     builds), _world_metadata (map-derived points), _stuck_objective
//     (quest/objective diagnostics), playerbots_names (the name pool). These
//     live in the shared database (Playerbot.SharedDatabase), so one copy
//     serves every realm.
//
// A migration file selects its destination with a directive line:
//
//   -- @target: shared          all following statements go to the shared DB
//   -- @target: characters      all following statements go to the character DB
//
// The directive may appear several times in one file, so a single migration can
// create realm tables and shared tables. Files without a directive default to
// `characters`, which is what every migration did before routing existed - so
// the 16 existing files keep their present behaviour untouched.
//
// Statements targeting the shared database must qualify every table they name
// with the {SHARED} placeholder, which is replaced by the configured schema
// name. There is no separate connection pool: both targets are executed over
// the existing CharacterDatabase connection, and the qualifier is what directs
// a statement at the other schema.

#pragma once

#include "Bot/BotTypes.h"
#include <string>
#include <unordered_set>

namespace Playerbot {

// Destination database for a migration statement.
enum class MigrationTarget
{
    Characters,     // this realm's character database (the connection default)
    Shared          // Playerbot.SharedDatabase, reached via the {SHARED} qualifier
};

class PlayerbotMigrationMgr
{
public:
    // Returns true if all required migrations applied (or were already applied).
    // Returns false if a migration failed; caller should refuse module init.
    bool run_all();

    // Set of versions present in the ledger of `target`. Empty when the table
    // doesn't exist yet. We use a set rather than a MAX(version) scalar so a
    // migration registered LATER with a LOWER version (e.g. 0006 added after
    // 0007/0008 already ran in a previous build) still gets applied —
    // `MAX(version)` would mark every gap as "already done".
    //
    // Each target keeps its OWN ledger, because "has this migration run?" is a
    // per-database question. A realm that has applied every character-side
    // migration has said nothing about the shared database, which several
    // realms write to.
    std::unordered_set<uint32> applied_versions(MigrationTarget target) const;

private:
    bool ensure_version_table(MigrationTarget target) const;
    bool apply_one(uint32 version, std::string const& sql_path,
                   std::unordered_set<uint32> const& applied_characters,
                   std::unordered_set<uint32> const& applied_shared);
};

} // namespace Playerbot
