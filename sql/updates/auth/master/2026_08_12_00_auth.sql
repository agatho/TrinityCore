--
-- Perks Program (Trading Post) currency-accounting integrity (G1 + G6).
--
-- G6: account-wide Trader's Tender (currency 2032). The authoritative balance is stored per bnet account here
-- and shared by every character of the account (earn/spend/refund act on one wallet), instead of the
-- per-character character_currency row.
--
DROP TABLE IF EXISTS `battlenet_account_perks_tender`;
CREATE TABLE `battlenet_account_perks_tender` (
  `accountId` int unsigned NOT NULL,
  `amount` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- G1: persist the purchasing character on each Trading Post purchase, so a refund can require the original
-- buyer. Pre-existing rows default to buyerGuid 0 (no character) and are therefore non-refundable, which is safe.
--
ALTER TABLE `battlenet_account_perks_purchases`
  ADD COLUMN `buyerGuid` bigint unsigned NOT NULL DEFAULT '0' AFTER `toyId`;
