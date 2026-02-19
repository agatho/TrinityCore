-- Add exteriorLocked, houseSize, houseType columns to character_housing
ALTER TABLE `character_housing`
  ADD COLUMN `exteriorLocked` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Whether exterior editing is locked (1=locked, 0=unlocked)' AFTER `settingsFlags`,
  ADD COLUMN `houseSize` TINYINT UNSIGNED NOT NULL DEFAULT 2 COMMENT 'HousingFixtureSize: 1=Any, 2=Small, 3=Medium, 4=Large' AFTER `exteriorLocked`,
  ADD COLUMN `houseType` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'HouseExteriorWmoData DB2 entry ID (architectural style)' AFTER `houseSize`;
