-- ---------------------------------------------------------------------------
-- Migration: Add default visual room to all existing houses that only have
-- the base room (entry 18).
--
-- Base room 18 only provides a geobox boundary — it has no visible wall,
-- floor, or ceiling geometry. Players entering the interior would see an
-- empty void. This migration adds the first visual room (the room entry
-- with the most components) into slot 1 for all houses missing one.
--
-- The visual room entry ID must match what GetDefaultVisualRoomEntry()
-- returns at runtime. If the DB2 data changes, update the value below.
-- Currently the first non-base room with >1 component is used.
--
-- Run ONCE against the characters database. Safe to re-run (idempotent).
-- ---------------------------------------------------------------------------

-- Step 1: Ensure every house has a base room in slot 0 (prerequisite)
-- (Uses the same logic as characters_housing_base_room_migration.sql)
INSERT IGNORE INTO `character_housing_rooms`
    (`ownerGuid`, `houseRoomId`, `slotIndex`, `orientation`, `mirrored`,
     `themeId`, `wallpaperId`, `materialId`, `doorTypeId`, `doorSlot`,
     `ceilingTypeId`, `ceilingSlot`)
SELECT
    ch.`guid`, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
FROM `character_housing` ch
WHERE NOT EXISTS (
    SELECT 1 FROM `character_housing_rooms` cr
    WHERE cr.`ownerGuid` = ch.`guid`
      AND cr.`slotIndex` = 0
);

-- Step 2: Add visual room in slot 1 for houses that don't have any
-- non-base room yet. The runtime fixup in LoadFromDB also handles this,
-- but this SQL ensures the data is correct even before the player logs in.
--
-- Visual room entry: Use HouseRoom entry 1 (the "Main Room" with 9 components
-- including walls, floor, ceiling). If entry 1 doesn't exist in your DB2,
-- the runtime fixup will pick the correct entry automatically.
INSERT INTO `character_housing_rooms`
    (`ownerGuid`, `houseRoomId`, `slotIndex`, `orientation`, `mirrored`,
     `themeId`, `wallpaperId`, `materialId`, `doorTypeId`, `doorSlot`,
     `ceilingTypeId`, `ceilingSlot`)
SELECT
    ch.`guid`,          -- ownerGuid
    1,                  -- houseRoomId (visual room with walls/floor/ceiling)
    1,                  -- slotIndex (slot 1, next to base room in slot 0)
    0,                  -- orientation (default facing)
    0,                  -- mirrored (false)
    0,                  -- themeId (default — faction theme applied at runtime)
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
      AND cr.`houseRoomId` != 18
);
