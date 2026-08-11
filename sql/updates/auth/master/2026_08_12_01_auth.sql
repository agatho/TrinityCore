--
-- Perks Program (Trading Post): base monthly Trader's Tender allowance (G7).
--
-- Track the last Trading Post interval (UTC month-start unix time) for which the account was granted its
-- automatic base monthly Tender (the Collector's Cache), so the grant is idempotent per account per period.
--
ALTER TABLE `battlenet_account_perks_tender`
  ADD COLUMN `lastCacheGrantPeriod` bigint unsigned NOT NULL DEFAULT '0' AFTER `amount`;
