CREATE TABLE IF NOT EXISTS `warband_currency_transfer_log` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `battlenetAccountId` int unsigned NOT NULL,
  `currencyTypeId` int unsigned NOT NULL,
  `sourceCharacterGuid` bigint unsigned NOT NULL,
  `destCharacterGuid` bigint unsigned NOT NULL,
  `quantity` int NOT NULL COMMENT 'amount sent (requested) from the source',
  `receivedQuantity` int NOT NULL DEFAULT 0 COMMENT 'amount received by the destination after transfer tax',
  `timestamp` int unsigned NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_bnet_account` (`battlenetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
