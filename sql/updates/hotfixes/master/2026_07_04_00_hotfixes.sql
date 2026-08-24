--
-- Premade Group Finder: GroupFinderActivity.db2 hotfix table (68275 layout 0xC3DB15C2)
--
DROP TABLE IF EXISTS `group_finder_activity`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `group_finder_activity` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `FullName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ShortName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `GroupFinderCategoryID` tinyint unsigned NOT NULL DEFAULT '0',
  `OrderIndex` tinyint NOT NULL DEFAULT '0',
  `GroupFinderActivityGrpID` smallint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `MinGearLevelSuggestion` smallint unsigned NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `MapID` smallint unsigned NOT NULL DEFAULT '0',
  `DifficultyID` smallint NOT NULL DEFAULT '0',
  `AreaID` smallint unsigned NOT NULL DEFAULT '0',
  `ExpansionID` int NOT NULL DEFAULT '0',
  `MaxPlayers` tinyint unsigned NOT NULL DEFAULT '0',
  `DisplayType` tinyint unsigned NOT NULL DEFAULT '0',
  `Field_11_0_7_57361_013` int NOT NULL DEFAULT '0',
  `Field_11_0_7_57361_014` int NOT NULL DEFAULT '0',
  `Field_11_0_7_57361_015` int NOT NULL DEFAULT '0',
  `Field_11_0_7_57361_016` int NOT NULL DEFAULT '0',
  `OverrideContentTuningID` int NOT NULL DEFAULT '0',
  `MapChallengeModeID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
