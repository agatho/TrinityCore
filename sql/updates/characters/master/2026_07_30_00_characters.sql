--
-- WoD Shipyard: persist the built shipyard tier per garrison. The shipyard is a garrison sub-feature (GarrBuilding
-- 205/206/207 = Lunarfall/Frostwall Shipyard L1/L2/L3, BuildingType 9) that is NOT placed on a normal architect
-- plot (it has no GarrBuildingPlotInst entry) and lives on the naval map. We track only its current building tier;
-- 0 = no shipyard built.
--
-- `ADD COLUMN IF NOT EXISTS` is MariaDB-only syntax and is a parse error on MySQL, so the guard is
-- expressed through INFORMATION_SCHEMA instead.
--
SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_garrison' AND COLUMN_NAME = 'shipyardBuildingId');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_garrison` ADD COLUMN `shipyardBuildingId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `cacheLastUsed`',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
