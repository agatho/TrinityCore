-- Migration: 0000_legacy_name_pool_repair
-- Purpose: Bring a PRE-EXISTING legacy `playerbots_names` up to the shape that
--          0015_name_pool.sql expects, before 0015 runs.
--
--          Some character databases already ship a `playerbots_names` table
--          with only (name_id, name, gender) - WCDB's characters dump is one.
--          0015 opens with CREATE TABLE IF NOT EXISTS, which silently does
--          NOTHING when such a table is present, and its INSERT then dies with
--              [1054] Unknown column 'race_mask' in 'field list'
--          That aborts the migration run, so PlayerbotMigrationMgr stops and
--          the whole module is disabled - while the server itself starts
--          normally, so the failure is easy to miss.
--
--          This runs at version 0 (the manager sorts by the leading integer,
--          so it applies before 0001) because a LATER migration cannot repair
--          a state that blocks an EARLIER one. Per the directory's discipline
--          the shipped numbered migrations are left untouched.
--
--          Non-destructive by design: columns are added, never dropped, and no
--          rows are touched - an existing pool keeps its claim state
--          (is_used / used_by_guid). A DROP+CREATE would have been simpler but
--          would discard which names live bots are already using.
--
-- Reverts: no-op on a database that never had the legacy table.

-- Nothing to do unless a legacy table is actually present.
SET @has_table := (SELECT COUNT(*) FROM information_schema.TABLES
                   WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names');

SET @need := (@has_table > 0) AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS
              WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
                AND COLUMN_NAME = 'race_mask');
SET @s := IF(@need,
    "ALTER TABLE `playerbots_names` ADD COLUMN `race_mask` int unsigned NOT NULL DEFAULT '4294967295' COMMENT 'Bitmask of compatible races'",
    'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @need := (@has_table > 0) AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS
              WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
                AND COLUMN_NAME = 'is_taken');
SET @s := IF(@need, "ALTER TABLE `playerbots_names` ADD COLUMN `is_taken` tinyint(1) NOT NULL DEFAULT '0'", 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @need := (@has_table > 0) AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS
              WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
                AND COLUMN_NAME = 'is_used');
SET @s := IF(@need, "ALTER TABLE `playerbots_names` ADD COLUMN `is_used` tinyint(1) NOT NULL DEFAULT '0'", 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @need := (@has_table > 0) AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS
              WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
                AND COLUMN_NAME = 'used_by_guid');
SET @s := IF(@need, "ALTER TABLE `playerbots_names` ADD COLUMN `used_by_guid` bigint unsigned DEFAULT NULL", 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @need := (@has_table > 0) AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS
              WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
                AND COLUMN_NAME = 'created_at');
SET @s := IF(@need, "ALTER TABLE `playerbots_names` ADD COLUMN `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP", 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @need := (@has_table > 0) AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS
              WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
                AND COLUMN_NAME = 'updated_at');
SET @s := IF(@need, "ALTER TABLE `playerbots_names` ADD COLUMN `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP", 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- 0015 relies on INSERT IGNORE against UNIQUE(name) to merge rather than
-- duplicate. Only add the key when it is missing AND the existing rows are
-- already unique, so this can never fail on legacy data; if duplicates exist
-- the key is skipped and 0015 simply appends instead of merging.
SET @need := (@has_table > 0)
    AND NOT (SELECT COUNT(*) FROM information_schema.STATISTICS
             WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'playerbots_names'
               AND INDEX_NAME = 'idx_unique_name')
    AND NOT (SELECT COUNT(*) FROM (SELECT `name` FROM `playerbots_names`
             GROUP BY `name` HAVING COUNT(*) > 1) d);
SET @s := IF(@need, "ALTER TABLE `playerbots_names` ADD UNIQUE KEY `idx_unique_name` (`name`)", 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;
