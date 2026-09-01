-- Remove broken static 574432 (Housing Platform WMO) spawns.
-- The sniff-extracted Z values are incorrect (underground), causing the platforms
-- to not render. Platforms are now spawned dynamically by HousingMap::SpawnHouseForPlot
-- at the correct DB2 HousePosition with proper rotation.

-- Remove the old 23 manually-placed spawns (from 2026_02_17_00_world.sql)
-- These had correct Z but didn't cover all DB2 plots, and are now redundant.
DELETE FROM `gameobject` WHERE `id` = 574432 AND `guid` BETWEEN 9101845 AND 9101867;

-- Remove the sniff-extracted spawns with wrong Z values.
-- These are in the @OGUID range from world_alliance_neighborhood_spawns.sql.
-- We can't use @OGUID variables here, so delete all static 574432 spawns on map 2735
-- since they're all now dynamically spawned.
DELETE FROM `gameobject` WHERE `id` = 574432 AND `map` = 2735;
