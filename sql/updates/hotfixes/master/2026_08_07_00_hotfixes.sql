--
-- ItemLogicalCost.db2 hotfix mirror (item upgrade step costs: slot mask -> ItemExtendedCost).
--
DROP TABLE IF EXISTS `item_logical_cost`;
CREATE TABLE `item_logical_cost` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `InventoryTypeSlotMask` INT NOT NULL DEFAULT '0',
  `Flags` INT NOT NULL DEFAULT '0',
  `ItemExtendedCostID` INT NOT NULL DEFAULT '0',
  `ItemLogicalCostGroupID` INT UNSIGNED NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
