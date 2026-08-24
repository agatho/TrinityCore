-- DelvesSeason.dbd LAYOUT D8CA312 (build 12.0.5.67186)
DROP TABLE IF EXISTS `delves_season`;
CREATE TABLE IF NOT EXISTS `delves_season` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- DelvesSeasonXSpell.dbd LAYOUT 13DB27BC (build 12.0.5.67186)
DROP TABLE IF EXISTS `delves_season_x_spell`;
CREATE TABLE IF NOT EXISTS `delves_season_x_spell` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `DelvesSeasonID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- PlayerCompanionInfo.dbd LAYOUT F61B5AA1 (build 12.0.5.67186)
DROP TABLE IF EXISTS `player_companion_info`;
CREATE TABLE IF NOT EXISTS `player_companion_info` (
  `UnlockDescription` text,
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
  `Field_12_0_0_64499_011` int NOT NULL DEFAULT '0',
  `Field_12_0_0_64499_012` int NOT NULL DEFAULT '0',
  `ParentID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `player_companion_info_locale`;
CREATE TABLE IF NOT EXISTS `player_companion_info_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `UnlockDescription_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `locale`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
