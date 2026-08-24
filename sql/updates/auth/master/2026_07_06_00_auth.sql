--
-- AccountStore: account-wide (Battle.net account) record of purchased AccountStoreItem IDs.
--
DROP TABLE IF EXISTS `battlenet_account_store_purchases`;
CREATE TABLE `battlenet_account_store_purchases` (
  `accountId` int unsigned NOT NULL,
  `accountStoreItemId` int unsigned NOT NULL DEFAULT '0',
  `purchaseTime` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`,`accountStoreItemId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
