-- Housing: Add 2D grid coordinates to room placement
-- Migrates from linear SlotIndex layout to 2D GridX/GridY system

ALTER TABLE `character_housing_rooms`
  ADD COLUMN IF NOT EXISTS `gridX` INT NOT NULL DEFAULT 0 AFTER `slotIndex`,
  ADD COLUMN IF NOT EXISTS `gridY` INT NOT NULL DEFAULT 0 AFTER `gridX`;

-- Migrate existing rooms: linear layout → gridX = slotIndex, gridY = 0
UPDATE `character_housing_rooms` SET `gridX` = `slotIndex`, `gridY` = 0 WHERE `gridX` = 0 AND `gridY` = 0;
