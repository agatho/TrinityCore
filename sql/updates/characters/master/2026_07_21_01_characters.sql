--
-- Columns added by integrated features that were only ever written into
-- sql/base/characters_database.sql. The updater applies the base schema solely when creating a
-- database from scratch, so upgraded databases never received them and the affected prepared
-- statements failed at startup.
--
-- Guarded per column: the base dump now carries both, so a database created from scratch already
-- has them and must skip the ALTER (an unguarded ADD COLUMN aborts the whole characters update
-- chain with MySQL 1060 on every fresh install).
--
SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_covenant' AND COLUMN_NAME = 'covenantId');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_covenant` ADD COLUMN `covenantId` int unsigned NOT NULL DEFAULT ''0'' AFTER `guid`',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_covenant' AND COLUMN_NAME = 'soulbindId');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_covenant` ADD COLUMN `soulbindId` int unsigned NOT NULL DEFAULT ''0''',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
