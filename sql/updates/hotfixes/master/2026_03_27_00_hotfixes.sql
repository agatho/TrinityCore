DROP TABLE IF EXISTS `delves_season`;
CREATE TABLE IF NOT EXISTS `delves_season` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Field_11_0_7_57361_000` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `delves_season_x_spell`;
CREATE TABLE IF NOT EXISTS `delves_season_x_spell` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `DelvesSeasonID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `player_companion_info`;
CREATE TABLE IF NOT EXISTS `player_companion_info` (
  `Name` text,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `CreatureDisplayInfoID` int NOT NULL DEFAULT '0',
  `TraitTreeID` int NOT NULL DEFAULT '0',
  `TraitConfigType` int NOT NULL DEFAULT '0',
  `TraitSystemID` int NOT NULL DEFAULT '0',
  `UiTextureAtlasMemberID` int NOT NULL DEFAULT '0',
  `Field_11_0_0_55793_006` int NOT NULL DEFAULT '0',
  `Field_11_0_0_55793_007` int NOT NULL DEFAULT '0',
  `Field_11_0_0_55793_008` int NOT NULL DEFAULT '0',
  `Field_11_0_5_56647_009` int NOT NULL DEFAULT '0',
  `Field_11_0_5_56647_010` int NOT NULL DEFAULT '0',
  `Field_11_0_5_56647_011` int NOT NULL DEFAULT '0',
  `Field_11_0_7_57361_012` int NOT NULL DEFAULT '0',
  `ParentID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `player_companion_info_locale`;
CREATE TABLE IF NOT EXISTS `player_companion_info_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `locale`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
