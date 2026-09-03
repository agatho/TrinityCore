--
-- Covenant / Soulbind DB2 hotfix tables. feature/covenant added the DB2 loaders +
-- prepared statements (HotfixDatabase.cpp) but shipped no table SQL.
--

DROP TABLE IF EXISTS `covenant`;
CREATE TABLE `covenant` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BountySetID` int NOT NULL DEFAULT '0',
  `SkillLineID` int NOT NULL DEFAULT '0',
  `DeathTeleportSpellID` int NOT NULL DEFAULT '0',
  `Field_9_0_2_36165_006` int NOT NULL DEFAULT '0',
  `Field_9_0_2_36165_007` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `CurrencyTypesID` int NOT NULL DEFAULT '0',
  `RequiredPlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `soulbind`;
CREATE TABLE `soulbind` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CovenantID` int NOT NULL DEFAULT '0',
  `GarrTalentTreeID` int NOT NULL DEFAULT '0',
  `CreatureID` int NOT NULL DEFAULT '0',
  `GarrFollowerID` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `soulbind_locale`;
CREATE TABLE `soulbind_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

