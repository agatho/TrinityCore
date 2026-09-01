-- Housing: Add 2D grid coordinates and floor index to room placement
-- GridX/GridY store yard offsets from the interior origin.
-- FloorIndex: 0=ground, 1+=upper floors (for stairwell rooms).

ALTER TABLE `character_housing_rooms`
  ADD COLUMN IF NOT EXISTS `gridX` INT NOT NULL DEFAULT 0 AFTER `slotIndex`,
  ADD COLUMN IF NOT EXISTS `gridY` INT NOT NULL DEFAULT 0 AFTER `gridX`,
  ADD COLUMN IF NOT EXISTS `floorIndex` INT NOT NULL DEFAULT 0 AFTER `gridY`;

-- Migrate existing rooms: slotIndex → yard offset (slot * 15), floor 0
UPDATE `character_housing_rooms` SET `gridX` = CAST(`slotIndex` AS SIGNED) * 15, `gridY` = 0, `floorIndex` = 0;
-- Entry room (slot 0) stays at gridX=0
UPDATE `character_housing_rooms` SET `gridX` = 0 WHERE `slotIndex` = 0;
