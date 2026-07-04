--
-- Great Vault: WeeklyRewardChestThreshold.db2 hotfix table.
--
DROP TABLE IF EXISTS `weekly_reward_chest_threshold`;
CREATE TABLE `weekly_reward_chest_threshold` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Type` tinyint NOT NULL DEFAULT '0',
  `Threshold` int NOT NULL DEFAULT '0',
  `Index` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
