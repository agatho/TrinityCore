-- Housing: Add 2D grid coordinates to room placement
-- Migrates from linear SlotIndex layout to 2D GridX/GridY system

ALTER TABLE `character_housing_rooms`
  ADD COLUMN IF NOT EXISTS `gridX` INT NOT NULL DEFAULT 0 AFTER `slotIndex`,
  ADD COLUMN IF NOT EXISTS `gridY` INT NOT NULL DEFAULT 0 AFTER `gridX`;

-- Migrate ALL existing rooms: linear layout → gridX = slotIndex, gridY = 0
-- This is safe to run multiple times (idempotent for linear layouts)
UPDATE `character_housing_rooms` SET `gridX` = CAST(`slotIndex` AS SIGNED), `gridY` = 0;
