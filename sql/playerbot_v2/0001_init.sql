-- Migration: 0001_init
-- Date:    2026-05-01
-- Purpose: Bootstrap V2 schema. Creates the version table and the minimal
--          set of bot-specific tables documented in v2/SCHEMA.md.
-- Reverts: yes (all DROP TABLE).

-- Per v2/SCHEMA.md: bot character data lives in TrinityCore's `characters`
-- table. V2 owns ONLY what TrinityCore doesn't already store. No mirror
-- tables are permitted.

CREATE TABLE IF NOT EXISTS playerbot_v2_schema_version (
    version       INT UNSIGNED NOT NULL PRIMARY KEY,
    applied_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    sha256        CHAR(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS playerbot_v2_account (
    account_id          INT UNSIGNED NOT NULL PRIMARY KEY,
    pseudo_account_idx  INT UNSIGNED NOT NULL,
    created_at          DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_used_at        DATETIME NULL,
    KEY idx_pseudo (pseudo_account_idx)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS playerbot_v2_character (
    character_guid_low   BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    spawned_at           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_active_at       DATETIME NULL,
    rng_seed             BIGINT UNSIGNED NOT NULL,
    spawn_state          TINYINT UNSIGNED NOT NULL,
    KEY idx_active (last_active_at),
    KEY idx_state  (spawn_state)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS playerbot_v2_personality (
    character_guid_low   BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    skill_tier           TINYINT UNSIGNED NOT NULL,
    verbosity            TINYINT UNSIGNED NOT NULL,
    aggression           TINYINT UNSIGNED NOT NULL,
    risk_tolerance       TINYINT UNSIGNED NOT NULL,
    politeness           TINYINT UNSIGNED NOT NULL,
    loyalty              TINYINT UNSIGNED NOT NULL,
    activity_pref        TINYINT UNSIGNED NOT NULL,
    response_delay_ms    SMALLINT UNSIGNED NOT NULL,
    response_jitter_ms   SMALLINT UNSIGNED NOT NULL,
    mistake_rate         TINYINT UNSIGNED NOT NULL,
    CONSTRAINT fk_pers_char FOREIGN KEY (character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS playerbot_v2_preferences (
    character_guid_low       BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    preferred_neighborhood   INT UNSIGNED NULL,
    preferred_house_template INT UNSIGNED NULL,
    opt_in_dungeons          BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_raids             BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_pvp               BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_arena             BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_delves            BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_professions       BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_housing           BOOLEAN NOT NULL DEFAULT TRUE,
    accept_player_invites    TINYINT UNSIGNED NOT NULL DEFAULT 1,
    follow_distance_yd       FLOAT NOT NULL DEFAULT 5.0,
    CONSTRAINT fk_prefs_char FOREIGN KEY (character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS playerbot_v2_population_target (
    target_id        INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    realm_id         INT UNSIGNED NOT NULL,
    effective_at     DATETIME NOT NULL,
    total_target     INT UNSIGNED NOT NULL,
    floor            INT UNSIGNED NOT NULL,
    ceiling          INT UNSIGNED NOT NULL,
    horde_pct        TINYINT UNSIGNED NOT NULL,
    payload_json     TEXT NOT NULL,
    KEY idx_realm_effective (realm_id, effective_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (1, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
