-- Ensure character_housing has the columns the neighborhood roster query depends on:
--   houseLevel, favor, houseName
--
-- houseLevel and favor are part of the base CREATE TABLE in sql/housing/housing_schema.sql,
-- but houseName was added later by 2026_03_04_00_characters_housing_name_desc.sql. This
-- migration is idempotent (uses INFORMATION_SCHEMA + PREPARE so it's a no-op if the column
-- is already there) to cover any tester whose DB sits between those points.

SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing' AND COLUMN_NAME = 'houseName');

SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_housing`
      ADD COLUMN `houseName` VARCHAR(64) NOT NULL DEFAULT '''' COMMENT ''Player-set house display name'' AFTER `facing`,
      ADD COLUMN `houseDescription` VARCHAR(256) NOT NULL DEFAULT '''' COMMENT ''Player-set house description'' AFTER `houseName`',
    'SELECT 1');

PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
