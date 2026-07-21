-- ============================================================================
-- Align hotfix DB2 tables with the 12.0.7 (68275) DB2 structures.
--
-- These three structures were changed in DB2Structure.h / DB2LoadInfo.h without
-- a matching hotfixes migration. The new shapes exist only in
-- sql/base/dev/hotfixes_database.sql, which the updater never imports (see
-- sql/base/dev/DO_NOT_IMPORT_THESE_FILES.txt), so an existing hotfixes database
-- kept the old columns and HotfixDatabase.cpp failed to prepare its statements
-- with "Unknown column ...", aborting worldserver startup at
-- "Could not prepare statements of the Hotfix database".
--
-- All three tables are DB2 mirrors and were empty; the renames below preserve
-- any rows that do exist, and exterior_component is rebuilt because its layout
-- changed wholesale rather than gaining columns.
-- ============================================================================

--
-- Covenant.db2 - Unknown902_6/7 renamed to Field_9_0_2_36165_006/007.
-- Pure rename: CHANGE keeps existing hotfix rows intact.
--
ALTER TABLE `covenant`
  CHANGE COLUMN `Unknown902_6` `Field_9_0_2_36165_006` int NOT NULL DEFAULT '0',
  CHANGE COLUMN `Unknown902_7` `Field_9_0_2_36165_007` int NOT NULL DEFAULT '0';

--
-- TransmogOutfitEntry.db2 - gained OutfitIndex (OrderIndex is unrelated and stays).
--
ALTER TABLE `transmog_outfit_entry`
  ADD COLUMN `OutfitIndex` int NOT NULL DEFAULT '0' AFTER `OverrideCostModifier`;

--
-- ExteriorComponent.db2 - restructured for 12.0.5+ (Size/ParentComponentID/
-- ModelFileDataID/Field_7/Field_9/GameObjectID/Field_11/ItemID/
-- HouseExteriorWmoDataID replace the old Type/FileDataID/ConditionID/HookID/
-- Slot/SortOrder/ComponentGroupID/UiTextureKitID/ExteriorComponentTypeID set).
-- Column order and signedness follow ExteriorComponentLoadInfo::Fields exactly.
--
DROP TABLE IF EXISTS `exterior_component`;
CREATE TABLE `exterior_component` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Size` tinyint unsigned NOT NULL DEFAULT '0',
  `ParentComponentID` int NOT NULL DEFAULT '0',
  `ModelFileDataID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Field_7` tinyint unsigned NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `Field_9` int NOT NULL DEFAULT '0',
  `GameObjectID` int NOT NULL DEFAULT '0',
  `Field_11` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `HouseExteriorWmoDataID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
