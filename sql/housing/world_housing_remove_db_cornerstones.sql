-- ============================================================================
-- Remove DB-spawned generic cornerstones from housing neighborhood maps
-- ============================================================================
-- The WPP-generated spawn files (world_alliance/horde_neighborhood_spawns.sql)
-- include generic cornerstone GOs (entry 457142) at each plot location.
-- Our code dynamically spawns per-plot unique cornerstones (entries 475051-475106
-- for Alliance, 527260-527314 for Horde) with FJamHousingCornerstone_C entity
-- fragment data, resulting in duplicate cornerstones on the map.
--
-- This SQL removes the DB-spawned generic cornerstones to prevent duplicates.
-- ============================================================================

-- Remove generic cornerstone spawns from Alliance neighborhood (map 2735)
DELETE FROM `gameobject` WHERE `id` = 457142 AND `map` = 2735;

-- Remove generic cornerstone spawns from Horde neighborhood (map 2736)
DELETE FROM `gameobject` WHERE `id` = 457142 AND `map` = 2736;
