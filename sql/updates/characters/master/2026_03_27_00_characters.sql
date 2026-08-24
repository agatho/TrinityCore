DROP TABLE IF EXISTS `delve_progress`;
CREATE TABLE `delve_progress` (
  `battlenetAccountId` int unsigned NOT NULL,
  `highestTierUnlocked` tinyint unsigned NOT NULL DEFAULT '3',
  `weeklyCompletions` int unsigned NOT NULL DEFAULT '0',
  `highestTierThisWeek` tinyint unsigned NOT NULL DEFAULT '0',
  `weeklyBountifulCount` int unsigned NOT NULL DEFAULT '0',
  `weeklyCofferShards` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `delve_companion`;
CREATE TABLE `delve_companion` (
  `battlenetAccountId` int unsigned NOT NULL,
  `companionId` int unsigned NOT NULL,
  `level` int unsigned NOT NULL DEFAULT '1',
  `xp` int unsigned NOT NULL DEFAULT '0',
  `selectedRole` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=Dps, 1=Healer, 2=Tank',
  `combatCurioNodeId` int unsigned NOT NULL DEFAULT '0',
  `utilityCurioNodeId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`, `companionId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
