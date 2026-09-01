-- ============================================================================
-- Neighborhood Hotfix Tables - Required for housing/neighborhood subsystems
-- Apply to: tc_hotfixes database
-- ============================================================================
-- Schemas match HotfixDatabase.cpp prepared statements (12.0.5 build 67186).
-- Field names follow TC convention; layouts are codegen-aligned per
-- c:/dumps/codegen_out/DB2_NEW_TABLES_67186.h.
-- ============================================================================

DROP TABLE IF EXISTS `neighborhood_initiative`;
CREATE TABLE `neighborhood_initiative` (
  `Name` text,
  `Description` text,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `InitiativeType` int NOT NULL DEFAULT '0',
  `Duration` int NOT NULL DEFAULT '0',
  `RequiredParticipants` int NOT NULL DEFAULT '0',
  `RewardCurrencyID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_initiative_locale`;
CREATE TABLE `neighborhood_initiative_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_map`;
CREATE TABLE `neighborhood_map` (
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `Radius` float NOT NULL DEFAULT '0',
  `PlotCount` int NOT NULL DEFAULT '0',
  `FactionRestriction` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_name_gen`;
CREATE TABLE `neighborhood_name_gen` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Prefix` text,
  `Suffix` text,
  `FullName` text,
  `NeighborhoodMapID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_name_gen_locale`;
CREATE TABLE `neighborhood_name_gen_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Prefix_lang` text,
  `Suffix_lang` text,
  `FullName_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_plot`;
CREATE TABLE `neighborhood_plot` (
  `Cost` bigint NOT NULL DEFAULT '0',
  `Name` text,
  `HousePositionX` float NOT NULL DEFAULT '0',
  `HousePositionY` float NOT NULL DEFAULT '0',
  `HousePositionZ` float NOT NULL DEFAULT '0',
  `HouseRotationX` float NOT NULL DEFAULT '0',
  `HouseRotationY` float NOT NULL DEFAULT '0',
  `HouseRotationZ` float NOT NULL DEFAULT '0',
  `CornerstonePositionX` float NOT NULL DEFAULT '0',
  `CornerstonePositionY` float NOT NULL DEFAULT '0',
  `CornerstonePositionZ` float NOT NULL DEFAULT '0',
  `CornerstoneRotationX` float NOT NULL DEFAULT '0',
  `CornerstoneRotationY` float NOT NULL DEFAULT '0',
  `CornerstoneRotationZ` float NOT NULL DEFAULT '0',
  `TeleportPositionX` float NOT NULL DEFAULT '0',
  `TeleportPositionY` float NOT NULL DEFAULT '0',
  `TeleportPositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `NeighborhoodMapID` int NOT NULL DEFAULT '0',
  `Field_010` int NOT NULL DEFAULT '0',
  `CornerstoneGameObjectID` int NOT NULL DEFAULT '0',
  `PlotIndex` int NOT NULL DEFAULT '0',
  `WorldState` int NOT NULL DEFAULT '0',
  `PlotGameObjectID` int NOT NULL DEFAULT '0',
  `TeleportFacing` float NOT NULL DEFAULT '0',
  `Field_016` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
