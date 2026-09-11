-- Migration: 0017_shared_split
-- Date:    2026-09-11
-- Purpose: Give the four realm-independent V2 tables a home in the SHARED
--          playerbot database, and carry over any rows an existing realm has
--          already accumulated in its character database.
-- Reverts: yes (DROP the four {SHARED} tables).
--
-- WHY THIS EXISTS
-- Playerbot data splits by scope. Anything keyed on a character guid, an account
-- id, or this realm's operational state must stay in the realm's character
-- database - character guid 12442 exists on every realm, so pooling those rows
-- into one schema shared by several realms collides. That covers
-- playerbot_v2_character / _personality / _preferences / _squad_preset /
-- _account, bot_guild_meta / _member_meta / _name_reserved (guild ids and
-- founder_low / rival_low), bot_craft_orders (requester_low / crafter_low),
-- _fleet_vitals_sample (per-realm telemetry) and _population_target (realm_id).
-- Those tables are NOT touched here; they stay exactly where they are.
--
-- The four below carry no realm identity. They are content: class/spec talent
-- builds, map-derived world metadata, quest-objective diagnostics, and the bot
-- name pool. One copy serves every realm.
--
-- The name pool is the instructive case. The list of available names is static
-- and shared, but whether a name is TAKEN is a per-realm fact - character names
-- are unique per realm, so a name consumed on one realm says nothing about
-- another. The pool therefore lives here while its usage columns (is_taken,
-- is_used, used_by_guid) are realm state; see the note at the end of this file.
--
-- Migrations 0001-0016 are deliberately left untouched. They have already been
-- recorded as applied on every existing realm, so editing them would change
-- history that will never be replayed. This file is additive.

-- @target: shared

CREATE TABLE IF NOT EXISTS {SHARED}.playerbot_v2_talent_build (
    class_id      TINYINT UNSIGNED NOT NULL,
    spec_id       INT UNSIGNED     NOT NULL,
    context       VARCHAR(32)      NOT NULL DEFAULT 'default',
    label         VARCHAR(64)      NULL,
    entries_json  MEDIUMTEXT       NOT NULL,
    source_url    VARCHAR(255)     NULL,
    updated_at    DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (class_id, spec_id, context)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS {SHARED}.playerbot_v2_world_metadata (
    id          INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    map_id      INT UNSIGNED NOT NULL,
    zone_id     INT UNSIGNED NOT NULL DEFAULT 0,
    kind        VARCHAR(32)  NOT NULL,
    pos_x       FLOAT        NOT NULL DEFAULT 0,
    pos_y       FLOAT        NOT NULL DEFAULT 0,
    pos_z       FLOAT        NOT NULL DEFAULT 0,
    radius      FLOAT        NOT NULL DEFAULT 0,
    label       VARCHAR(128) NULL,
    notes       TEXT         NULL,
    created_by  VARCHAR(64)  NULL,
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    KEY idx_map_zone (map_id, zone_id),
    KEY idx_kind (kind)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS {SHARED}.playerbot_v2_stuck_objective (
    quest_id    INT UNSIGNED     NOT NULL,
    obj_id      INT UNSIGNED     NOT NULL,
    category    VARCHAR(32)      NOT NULL DEFAULT '',
    hit_count   INT UNSIGNED     NOT NULL DEFAULT 0,
    first_seen  DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_seen   DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    sample_map  INT UNSIGNED     NOT NULL DEFAULT 0,
    sample_zone INT UNSIGNED     NOT NULL DEFAULT 0,
    sample_x    FLOAT            NOT NULL DEFAULT 0,
    sample_y    FLOAT            NOT NULL DEFAULT 0,
    sample_bot  BIGINT UNSIGNED  NOT NULL DEFAULT 0,
    PRIMARY KEY (quest_id, obj_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS {SHARED}.playerbots_names (
    name_id      INT UNSIGNED    NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name         VARCHAR(12)     NOT NULL,
    gender       TINYINT UNSIGNED NOT NULL DEFAULT 0,
    race_mask    INT UNSIGNED    NOT NULL DEFAULT 0,
    is_taken     TINYINT(1)      NOT NULL DEFAULT 0,
    is_used      TINYINT(1)      NOT NULL DEFAULT 0,
    used_by_guid BIGINT UNSIGNED NULL,
    created_at   TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_name (name),
    KEY idx_unused (is_used, gender)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Carry over rows an existing realm already has in its character database.
--
-- The unqualified source names resolve to the character database, because that
-- is the connection's default schema. No existence guard is needed: migrations
-- 0001-0016 are left untouched and still create all four tables there, and they
-- sort before 0017, so the sources always exist by the time this runs - on a
-- fresh install as well as an existing realm.
--
-- Deliberately NOT written with SET @var / PREPARE / EXECUTE. Each statement in
-- a migration is dispatched through DatabaseWorkerPool::DirectExecute, which
-- calls GetFreeConnection() per statement and unlocks afterwards, so
-- consecutive statements may run on DIFFERENT pooled connections. User
-- variables and prepared-statement handles are session-scoped, so they would
-- vanish between statements - intermittently, depending on pool size and
-- contention. Every statement below is therefore self-contained.
--
-- INSERT IGNORE, so re-running this migration never clobbers rows the shared
-- database already holds - the shared copy wins once it exists.

INSERT IGNORE INTO {SHARED}.playerbot_v2_talent_build
    (class_id, spec_id, context, label, entries_json, source_url, updated_at)
SELECT class_id, spec_id, context, label, entries_json, source_url, updated_at
FROM playerbot_v2_talent_build;

INSERT IGNORE INTO {SHARED}.playerbot_v2_world_metadata
    (id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by, created_at, updated_at)
SELECT id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by, created_at, updated_at
FROM playerbot_v2_world_metadata;

INSERT IGNORE INTO {SHARED}.playerbot_v2_stuck_objective
    (quest_id, obj_id, category, hit_count, first_seen, last_seen, sample_map, sample_zone, sample_x, sample_y, sample_bot)
SELECT quest_id, obj_id, category, hit_count, first_seen, last_seen, sample_map, sample_zone, sample_x, sample_y, sample_bot
FROM playerbot_v2_stuck_objective;

-- The name pool copies across WITHOUT its usage columns: is_taken, is_used and
-- used_by_guid describe which names THIS realm has consumed, and those flags are
-- meaningless (and actively wrong) on any other realm. The shared pool therefore
-- starts pristine and every realm tracks its own consumption.
INSERT IGNORE INTO {SHARED}.playerbots_names
    (name_id, name, gender, race_mask, is_taken, is_used, used_by_guid, created_at, updated_at)
SELECT name_id, name, gender, race_mask, 0, 0, NULL, created_at, updated_at
FROM playerbots_names;

-- NOTE - still outstanding after this migration:
-- The runtime still READS these four tables through unqualified names, i.e. from
-- the character database. Moving the read paths onto {SHARED} is a separate
-- change; until it lands, this migration only establishes the shared copies and
-- keeps them in step. Nothing regresses in the meantime because the character
-- side is untouched.
