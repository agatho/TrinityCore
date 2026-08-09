-- ---------------------------------------------------------------------------
-- 2026-08-09  M8: guild -> neighborhood link
-- Adds neighborhoods.guildId so guild neighborhoods created via
-- CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD persist their owning guild id and
-- NeighborhoodMgr::GetNeighborhoodByGuildId resolves after a server restart.
-- Idempotent: safe to re-run.
-- ---------------------------------------------------------------------------

SET @dbname := DATABASE();

-- Add the column only if it does not already exist.
SET @col_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = @dbname AND TABLE_NAME = 'neighborhoods' AND COLUMN_NAME = 'guildId'
);
SET @ddl := IF(@col_exists = 0,
    "ALTER TABLE `neighborhoods` ADD COLUMN `guildId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'M8: owning guild id for guild neighborhoods (0 = not guild-linked)' AFTER `createTime`",
    "SELECT 'neighborhoods.guildId already present'");
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Add the lookup index only if it does not already exist.
SET @idx_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = @dbname AND TABLE_NAME = 'neighborhoods' AND INDEX_NAME = 'idx_guild'
);
SET @ddl := IF(@idx_exists = 0,
    "ALTER TABLE `neighborhoods` ADD INDEX `idx_guild` (`guildId`)",
    "SELECT 'neighborhoods.idx_guild already present'");
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;
