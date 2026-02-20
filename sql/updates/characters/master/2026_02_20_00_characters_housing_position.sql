-- Add house position persistence columns to character_housing
ALTER TABLE `character_housing`
  ADD COLUMN `posX` FLOAT NOT NULL DEFAULT 0 COMMENT 'House X position on plot' AFTER `houseType`,
  ADD COLUMN `posY` FLOAT NOT NULL DEFAULT 0 COMMENT 'House Y position on plot' AFTER `posX`,
  ADD COLUMN `posZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'House Z position on plot' AFTER `posY`,
  ADD COLUMN `facing` FLOAT NOT NULL DEFAULT 0 COMMENT 'House facing angle on plot' AFTER `posZ`;
