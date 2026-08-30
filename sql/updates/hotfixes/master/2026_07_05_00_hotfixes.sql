--
-- PerksProgram vendor + activity-threshold hotfix tables (Trading Post)
--
DROP TABLE IF EXISTS `perks_activity_threshold`;
CREATE TABLE `perks_activity_threshold` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BonusTendies` int NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `Threshold` int NOT NULL DEFAULT '0',
  `PerksActivityThresholdGroupID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `perks_activity_threshold_group`;
CREATE TABLE `perks_activity_threshold_group` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `PerksMonth` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `perks_activity_threshold_group_locale`;
CREATE TABLE `perks_activity_threshold_group_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `perks_vendor_category`;
CREATE TABLE `perks_vendor_category` (
  `DisplayName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `PerksVendorType` int NOT NULL DEFAULT '0',
  `DefaultUIModelSceneID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `perks_vendor_category_locale`;
CREATE TABLE `perks_vendor_category_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `DisplayName_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `perks_vendor_item`;
CREATE TABLE `perks_vendor_item` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `PerksVendorCategoryID` tinyint NOT NULL DEFAULT '0',
  `Field_10_0_5_47118_002` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `Field_10_0_5_47118_004` int NOT NULL DEFAULT '0',
  `CreatureDisplayInfoID` int NOT NULL DEFAULT '0',
  `Cost` int NOT NULL DEFAULT '0',
  `UiModelSceneID` int NOT NULL DEFAULT '0',
  `UiGroupInfo` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `perks_vendor_item_x_interval`;
CREATE TABLE `perks_vendor_item_x_interval` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `PerksVendorItemID` int NOT NULL DEFAULT '0',
  `PerksActivityThresholdID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
