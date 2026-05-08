--
-- Add battlenetAccount denormalized column to `characters` so warband features
-- (currency transfer, alt-XP bonus, account-wide queries) can filter by Bnet
-- account without a cross-DB join into the auth schema.
--
-- The column is populated by the worldserver at character login. To backfill
-- existing characters from your auth database, run (substitute your auth DB
-- name if not `auth`):
--   UPDATE characters c
--     INNER JOIN auth.account a ON c.account = a.id
--     SET c.battlenetAccount = a.battlenet_account
--     WHERE a.battlenet_account IS NOT NULL AND c.battlenetAccount = 0;
--
ALTER TABLE `characters`
    ADD COLUMN `battlenetAccount` int unsigned NOT NULL DEFAULT 0 AFTER `account`,
    ADD KEY `idx_battlenetAccount` (`battlenetAccount`);
