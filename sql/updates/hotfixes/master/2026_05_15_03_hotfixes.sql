--
-- Phase 10A.4 - FactionGroup.db2 + CurrencyCategory.db2
-- FactionGroup (4 rows: Player/Alliance/Horde/Monster) provides the bitmask
-- values referenced by FactionTemplate.FactionGroup/FriendGroup/EnemyGroup
-- and the per-side honor/conquest currency texture file IDs.
-- CurrencyCategory (35 rows) provides parent categories for currency UI
-- grouping, including category 142 (Renown) used by Major Faction
-- renown currencies.
--
DROP TABLE IF EXISTS `faction_group`;
CREATE TABLE `faction_group` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `InternalName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MaskID` tinyint unsigned NOT NULL DEFAULT '0',
  `HonorCurrencyTextureFileID` int NOT NULL DEFAULT '0',
  `ConquestCurrencyTextureFileID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `faction_group_locale`;
CREATE TABLE `faction_group_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `currency_category`;
CREATE TABLE `currency_category` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Flags` int NOT NULL DEFAULT '0',
  `ExpansionID` tinyint unsigned NOT NULL DEFAULT '0',
  `ParentCategoryID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `currency_category_locale`;
CREATE TABLE `currency_category_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
