CREATE TABLE IF NOT EXISTS `warband_achievement` (
  `battlenetAccountId` int unsigned NOT NULL,
  `achievement` int unsigned NOT NULL,
  `date` bigint NOT NULL DEFAULT '0',
  `firstCharGuid` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`, `achievement`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `warband_achievement_progress` (
  `battlenetAccountId` int unsigned NOT NULL,
  `criteria` int unsigned NOT NULL,
  `counter` bigint unsigned NOT NULL,
  `date` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`, `criteria`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
