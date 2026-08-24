--
-- M+ Great Vault per-run levels. The vault's three slots reward the level of the 1st/4th/8th-best run, but the
-- server only stored a single `bestLevel` per activity row and reported it for ALL slots (overstating slots 2 and 3).
-- Persist the individual run levels (comma-separated, sorted high->low, capped at the highest slot threshold) so
-- each slot can advertise the correct Nth-best run level. Empty for legacy rows (falls back to bestLevel).
--
ALTER TABLE `character_weekly_reward_activity` ADD COLUMN IF NOT EXISTS `levels` VARCHAR(64) NOT NULL DEFAULT '' AFTER `bestLevel`;
