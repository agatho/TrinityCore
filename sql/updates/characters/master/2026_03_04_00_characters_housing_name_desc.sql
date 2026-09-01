-- Add houseName and houseDescription columns to character_housing
ALTER TABLE `character_housing`
  ADD COLUMN `houseName` VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'Player-set house display name' AFTER `facing`,
  ADD COLUMN `houseDescription` VARCHAR(256) NOT NULL DEFAULT '' COMMENT 'Player-set house description' AFTER `houseName`;
