-- ============================================================================
-- Initiative Hotfix Tables - Required for InitiativeManager subsystem
-- Apply to: tc_hotfixes database
-- ============================================================================
-- Uses DROP + CREATE (not IF NOT EXISTS) to ensure schema updates are applied
-- when re-importing. The data SQL re-inserts all rows, so no data is lost.
-- ============================================================================

DROP TABLE IF EXISTS `initiative_cycle`;
CREATE TABLE `initiative_cycle` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `RewardGroupID` int NOT NULL DEFAULT '0',
  `CycleIndex` int NOT NULL DEFAULT '0',
  `StartDay` int NOT NULL DEFAULT '0',
  `Duration` int NOT NULL DEFAULT '0',
  `InitiativeID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_cycle_priority`;
CREATE TABLE `initiative_cycle_priority` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Priority` int NOT NULL DEFAULT '0',
  `Weight` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `InitiativeCycleID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_milestone`;
CREATE TABLE `initiative_milestone` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MilestoneIndex` int NOT NULL DEFAULT '0',
  `ProgressRequired` float NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `InitiativeCycleID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_reward`;
CREATE TABLE `initiative_reward` (
  `RewardData` bigint signed NOT NULL DEFAULT '0',
  `Name` text,
  `Description` text,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `RewardType` int NOT NULL DEFAULT '0',
  `RewardAmount` int NOT NULL DEFAULT '0',
  `CurrencyID` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_reward_locale`;
CREATE TABLE `initiative_reward_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_reward_x_milestone`;
CREATE TABLE `initiative_reward_x_milestone` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `InitiativeRewardID` int NOT NULL DEFAULT '0',
  `InitiativeMilestoneID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_task`;
CREATE TABLE `initiative_task` (
  `Name` text,
  `Description` text,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `TaskType` int NOT NULL DEFAULT '0',
  `TargetCount` int NOT NULL DEFAULT '0',
  `CriteriaTreeID` int NOT NULL DEFAULT '0',
  `SortOrder` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `UiTextureKitID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_task_locale`;
CREATE TABLE `initiative_task_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `initiative_x_task`;
CREATE TABLE `initiative_x_task` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `InitiativeTaskID` int NOT NULL DEFAULT '0',
  `SortOrder` int NOT NULL DEFAULT '0',
  `NeighborhoodInitiativeID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
