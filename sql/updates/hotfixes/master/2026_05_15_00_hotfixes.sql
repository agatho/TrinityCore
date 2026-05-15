--
-- Phase 10A.1 - Covenant.db2
-- Required as FK target for RenownRewards.db2 (RenownRewards.CovenantID -> Covenant.ID).
-- Covenant.FactionID maps to the Major Faction's Faction.ID; Covenant.CurrencyTypesID
-- maps to the renown currency (mirror of Faction.RenownCurrencyID).
--
DROP TABLE IF EXISTS `covenant`;
CREATE TABLE `covenant` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BountySetID` int NOT NULL DEFAULT '0',
  `SkillLineID` int NOT NULL DEFAULT '0',
  `DeathTeleportSpellID` int NOT NULL DEFAULT '0',
  `Unknown902_6` int NOT NULL DEFAULT '0',
  `Unknown902_7` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `CurrencyTypesID` int NOT NULL DEFAULT '0',
  `RequiredPlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `covenant_locale`;
CREATE TABLE `covenant_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
