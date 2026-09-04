--
-- Timerunning season persistence. TrinityCore had a dead ActivePlayerData::TimerunningSeasonID UpdateField
-- (Player::SetTimerunningSeasonID) with no column, no creation path and creation actively refused. This adds
-- the backing column so a Timerunning (Pandaria/Legion Remix) character can exist and be converted to standard.
--
ALTER TABLE `characters` ADD COLUMN `timerunningSeasonId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `createTime`;
