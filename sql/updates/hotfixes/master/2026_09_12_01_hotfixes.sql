--
-- PlayerCompanionInfo.dbd LAYOUT 55DA5E60 (12.1.0, builds 68209-69587). 2026_03_27_00_hotfixes.sql created the
-- 15-field F61B5AA1 (12.0.5) table while PlayerCompanionInfoLoadInfo / PlayerCompanionInfoEntry carry 17 fields
-- (REPORT.md 6.3): + Field_12_1_0_68209_001 (second localized string), Field_12_0_0_64499_011 renamed to
-- PlayerDataElementCharacterID, + FlavorNodeID. Columns in DB2LoadInfo.h order; types follow the field types
-- (FT_STRING -> text, unsigned FT_INT -> int unsigned, signed FT_INT -> int).
--
DROP TABLE IF EXISTS `player_companion_info`;
CREATE TABLE `player_companion_info` (
  `UnlockDescription` text,
  `Field_12_1_0_68209_001` text,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `DelvesSeasonID` int NOT NULL DEFAULT '0',
  `TraitTreeID` int NOT NULL DEFAULT '0',
  `TraitNodeID_DPS` int NOT NULL DEFAULT '0',
  `TraitNodeID_Heal` int NOT NULL DEFAULT '0',
  `TraitSubTreeID_DPS` int NOT NULL DEFAULT '0',
  `TraitSubTreeID_Heal` int NOT NULL DEFAULT '0',
  `TraitSubTreeID_Tank` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `CreatureDisplayInfoID` int NOT NULL DEFAULT '0',
  `UiModelSceneID` int NOT NULL DEFAULT '0',
  `PlayerDataElementCharacterID` int NOT NULL DEFAULT '0',
  `Field_12_0_0_64499_012` int NOT NULL DEFAULT '0',
  `FlavorNodeID` int NOT NULL DEFAULT '0',
  `ParentID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `player_companion_info_locale`;
CREATE TABLE `player_companion_info_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `UnlockDescription_lang` text,
  `Field_12_1_0_68209_001_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
