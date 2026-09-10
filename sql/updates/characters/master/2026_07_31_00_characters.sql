--
-- M+ Great Vault per-run levels. The vault's three slots reward the level of the 1st/4th/8th-best run, but the
-- server only stored a single `bestLevel` per activity row and reported it for ALL slots (overstating slots 2 and 3).
-- Persist the individual run levels (comma-separated, sorted high->low, capped at the highest slot threshold) so
-- each slot can advertise the correct Nth-best run level. Empty for legacy rows (falls back to bestLevel).
--
-- `ADD COLUMN IF NOT EXISTS` is MariaDB-only syntax and is a parse error on MySQL, so the guard is
-- expressed through INFORMATION_SCHEMA instead.
--
SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_weekly_reward_activity' AND COLUMN_NAME = 'levels');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_weekly_reward_activity` ADD COLUMN `levels` VARCHAR(64) NOT NULL DEFAULT '''' AFTER `bestLevel`',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
