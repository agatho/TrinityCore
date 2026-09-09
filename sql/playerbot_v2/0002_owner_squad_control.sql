-- Migration: 0002_owner_squad_control
-- Date:    2026-05-06
-- Purpose: Owner-bot binding + squad-control state. Adds persistent
--          owner_account_id / owner_player_guid so a bot's authority
--          model survives logout / group disband. Also adds squad-
--          control state (formation type/slot, follow distance,
--          verbose flag) so commands like /follow, /formation wedge,
--          and /verbose persist across restarts.
-- Reverts: yes (DROP COLUMN ... and DROP TABLE on the squad preset).

-- Per-character owner + squad state. All columns default to "no owner /
-- autonomous defaults" so existing rows keep current behaviour.
ALTER TABLE playerbot_v2_character
    ADD COLUMN owner_account_id    INT UNSIGNED NOT NULL DEFAULT 0
        AFTER spawn_state,
    ADD COLUMN owner_player_guid   BIGINT UNSIGNED NOT NULL DEFAULT 0
        AFTER owner_account_id,
    ADD COLUMN formation_type      TINYINT UNSIGNED NOT NULL DEFAULT 0
        AFTER owner_player_guid,
    ADD COLUMN formation_slot      TINYINT UNSIGNED NOT NULL DEFAULT 0
        AFTER formation_type,
    ADD COLUMN follow_distance_yd  FLOAT NOT NULL DEFAULT 5.0
        AFTER formation_slot,
    ADD COLUMN owner_verbose       TINYINT(1) NOT NULL DEFAULT 0
        AFTER follow_distance_yd,
    ADD KEY idx_owner_account (owner_account_id),
    ADD KEY idx_owner_player  (owner_player_guid);

-- Owner-saved squad presets (formation per role/class snapshots that an
-- owner can re-apply after a wipe / new dungeon entry / etc).
-- One row per (owner_account_id, preset_name); the payload_json stores
-- {bot_guid → {slot, type}} mappings. We use TEXT (JSON in 8.0+) for
-- forward compat; the application layer parses.
CREATE TABLE IF NOT EXISTS playerbot_v2_squad_preset (
    owner_account_id  INT UNSIGNED NOT NULL,
    preset_name       VARCHAR(64) NOT NULL,
    saved_at          DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    payload_json      TEXT NOT NULL,
    PRIMARY KEY (owner_account_id, preset_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (2, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
