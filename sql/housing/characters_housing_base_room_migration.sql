-- ---------------------------------------------------------------------------
-- Migration: Add base room (HouseRoom entry 18) to all existing houses
-- that don't already have one.
--
-- Houses created before the auto-PlaceRoom fix in Housing::Create() have
-- zero rows in character_housing_rooms. Without a base room, entering the
-- interior spawns nothing and decor placement has no Geobox.
--
-- Run ONCE against the characters database. Safe to re-run (idempotent).
-- ---------------------------------------------------------------------------

INSERT INTO `character_housing_rooms`
    (`ownerGuid`, `houseRoomId`, `slotIndex`, `orientation`, `mirrored`,
     `themeId`, `wallpaperId`, `materialId`, `doorTypeId`, `doorSlot`,
     `ceilingTypeId`, `ceilingSlot`)
SELECT
    ch.`guid`,          -- ownerGuid
    18,                 -- houseRoomId (HouseRoom.db2 base room entry)
    0,                  -- slotIndex (base room = slot 0)
    0,                  -- orientation (default facing)
    0,                  -- mirrored (false)
    0,                  -- themeId (default)
    0,                  -- wallpaperId (default)
    0,                  -- materialId (default)
    0,                  -- doorTypeId (default)
    0,                  -- doorSlot (default)
    0,                  -- ceilingTypeId (default)
    0                   -- ceilingSlot (default)
FROM `character_housing` ch
WHERE NOT EXISTS (
    SELECT 1 FROM `character_housing_rooms` cr
    WHERE cr.`ownerGuid` = ch.`guid`
      AND cr.`slotIndex` = 0
);
