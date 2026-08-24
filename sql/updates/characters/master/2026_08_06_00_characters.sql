--
-- Mythic+ keystone lifecycle: track timed-ness of weekly runs (drives the weekly keystone level adjustment)
-- and the reset boundary the keystone was last adjusted for.
--
ALTER TABLE `character_mythic_plus_weekly` ADD `timed` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Run beat the par time' AFTER `level`;
ALTER TABLE `character_mythic_plus_vault` ADD `keystoneResetTime` BIGINT NOT NULL DEFAULT 0 COMMENT 'Weekly reset boundary the keystone was last adjusted for' AFTER `claimedResetTime`;
