--
-- Perks Program (Trading Post): account-wide record of purchases, used to authorise refunds.
--
DROP TABLE IF EXISTS `battlenet_account_perks_purchases`;
CREATE TABLE `battlenet_account_perks_purchases` (
  `accountId` int unsigned NOT NULL,
  `perksVendorItemId` int NOT NULL DEFAULT '0',
  `price` int NOT NULL DEFAULT '0',
  `purchaseTime` int unsigned NOT NULL DEFAULT '0',
  `mountId` int NOT NULL DEFAULT '0',
  `toyId` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`,`perksVendorItemId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
