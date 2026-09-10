-- WoD Garrison resource cache: per-garrison last-collection timestamp (accrual base).
--
-- `ADD COLUMN IF NOT EXISTS` is MariaDB-only syntax and is a parse error on MySQL, so the guard is
-- expressed through INFORMATION_SCHEMA instead.
--
SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_garrison' AND COLUMN_NAME = 'cacheLastUsed');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_garrison` ADD COLUMN `cacheLastUsed` BIGINT NOT NULL DEFAULT 0 AFTER `lastMissionStartDay`',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
