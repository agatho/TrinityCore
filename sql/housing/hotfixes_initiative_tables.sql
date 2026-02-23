-- ============================================================================
-- Initiative Hotfix Tables - Required for InitiativeManager subsystem
-- Apply to: tc_hotfixes database
-- ============================================================================

CREATE TABLE IF NOT EXISTS `initiative_cycle` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `InitiativeID` int NOT NULL DEFAULT '0',
  `CycleIndex` int NOT NULL DEFAULT '0',
  `StartDay` int NOT NULL DEFAULT '0',
  `Duration` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `initiative_cycle_priority` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Priority` int NOT NULL DEFAULT '0',
  `Weight` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `InitiativeCycleID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `initiative_milestone` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MilestoneIndex` int NOT NULL DEFAULT '0',
  `ProgressRequired` float NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `InitiativeCycleID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `initiative_reward` (
  `RewardData` text,
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

CREATE TABLE IF NOT EXISTS `initiative_reward_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `initiative_reward_x_milestone` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `InitiativeRewardID` int NOT NULL DEFAULT '0',
  `InitiativeMilestoneID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `initiative_task` (
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

CREATE TABLE IF NOT EXISTS `initiative_task_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `initiative_x_task` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `InitiativeTaskID` int NOT NULL DEFAULT '0',
  `SortOrder` int NOT NULL DEFAULT '0',
  `NeighborhoodInitiativeID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
