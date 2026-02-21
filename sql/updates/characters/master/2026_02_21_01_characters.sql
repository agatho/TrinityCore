CREATE TABLE IF NOT EXISTS `warband_reputation` (
  `battlenetAccountId` int unsigned NOT NULL,
  `faction` smallint unsigned NOT NULL,
  `standing` int NOT NULL DEFAULT '0',
  `renownLevel` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`, `faction`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
