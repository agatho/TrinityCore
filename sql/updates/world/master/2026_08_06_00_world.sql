--
-- Matrix Catalyst (item conversion) output mapping. The eligible INPUT items come from the client's
-- ItemConversionEntry.db2; this table provides the server-content OUTPUT side: which item an input becomes.
-- Resolution order for a conversion set: exact InputItemID match first, then (ClassID, InventoryType) with
-- 0 as a wildcard on either. Typical tier usage: one row per (class, slot) pointing at the class tier piece.
--
CREATE TABLE IF NOT EXISTS `item_conversion_output` (
  `ItemConversionID` INT UNSIGNED NOT NULL COMMENT 'ItemConversion.db2 ID',
  `ClassID` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ChrClasses.db2 ID, 0 = any class',
  `InventoryType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Input inventory type, 0 = any',
  `InputItemID` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Exact input item, 0 = any eligible input',
  `OutputItemID` INT UNSIGNED NOT NULL COMMENT 'Item the input converts into',
  `Comment` VARCHAR(255) DEFAULT NULL,
  PRIMARY KEY (`ItemConversionID`,`ClassID`,`InventoryType`,`InputItemID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Item conversion (Matrix Catalyst) output mapping';
