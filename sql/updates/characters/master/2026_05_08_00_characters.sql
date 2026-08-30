--
-- Add battlenetAccount denormalized column to `characters` so warband features
-- (currency transfer, alt-XP bonus, account-wide queries) can filter by Bnet
-- account without a cross-DB join into the auth schema.
--
-- The column is refreshed by the worldserver on every character login (see
-- WorldSession::HandlePlayerLogin) and written at character creation, so it stays
-- current going forward. The UPDATE below backfills characters that existed before
-- this change straight away, rather than waiting for each to log in once.
--
ALTER TABLE `characters`
    ADD COLUMN `battlenetAccount` int unsigned NOT NULL DEFAULT 0 AFTER `account`,
    ADD KEY `idx_battlenetAccount` (`battlenetAccount`);

-- Backfill from the auth schema. Assumes the auth database is named `auth` (the
-- TrinityCore default); adjust the schema qualifier if your realm uses another name.
-- Idempotent: only rows still at the 0 default are touched.
UPDATE `characters` c
    INNER JOIN `auth`.`account` a ON c.`account` = a.`id`
    SET c.`battlenetAccount` = a.`battlenet_account`
    WHERE a.`battlenet_account` IS NOT NULL AND c.`battlenetAccount` = 0;
