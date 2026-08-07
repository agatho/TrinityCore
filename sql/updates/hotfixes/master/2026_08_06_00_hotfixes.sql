--
-- ItemConversion.db2 / ItemConversionEntry.db2 (Matrix Catalyst item conversion): hotfix mirrors.
--
DROP TABLE IF EXISTS `item_conversion`;
CREATE TABLE `item_conversion` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `Unknown920` INT NOT NULL DEFAULT '0',
  `ItemBonusTreeID` INT NOT NULL DEFAULT '0',
  `ItemLogicalCostGroupID` INT NOT NULL DEFAULT '0',
  `AlternateItemLogicalCostGroupID` INT NOT NULL DEFAULT '0',
  `PlayerConditionID` INT NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `item_conversion_entry`;
CREATE TABLE `item_conversion_entry` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `ItemID` INT NOT NULL DEFAULT '0',
  `ItemConversionID` INT UNSIGNED NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
