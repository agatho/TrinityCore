-- Housing: Add 2D grid coordinates to room placement
-- GridX/GridY store yard offsets from the interior origin (not grid cell indices).
-- Entry room at (0,0), first visual room at (15,0) = Entry door(+3) + Room door(+12).

ALTER TABLE `character_housing_rooms`
  ADD COLUMN IF NOT EXISTS `gridX` INT NOT NULL DEFAULT 0 AFTER `slotIndex`,
  ADD COLUMN IF NOT EXISTS `gridY` INT NOT NULL DEFAULT 0 AFTER `gridX`;

-- Migrate existing rooms: slotIndex → yard offset (slot * 15)
UPDATE `character_housing_rooms` SET `gridX` = CAST(`slotIndex` AS SIGNED) * 15, `gridY` = 0;
-- Entry room (slot 0) stays at gridX=0
UPDATE `character_housing_rooms` SET `gridX` = 0 WHERE `slotIndex` = 0;
