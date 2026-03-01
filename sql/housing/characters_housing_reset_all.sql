-- ============================================================================
-- FULL HOUSING RESET — clears ALL player housing data and plot assignments
-- ============================================================================
-- Use this when housing data is corrupted (e.g. DB save failures left
-- neighborhood_members.plotIndex set but character_housing missing).
--
-- This script:
--   1. Removes all housing data (houses, rooms, decor, fixtures, catalogs)
--   2. Resets all plot assignments in neighborhood_members
--   3. Resets quest progress for the housing tutorial chain
--
-- After running, players can re-purchase plots as if they never had a house.
-- ============================================================================

-- Remove all player housing data
TRUNCATE TABLE `character_housing`;
TRUNCATE TABLE `character_housing_rooms`;
TRUNCATE TABLE `character_housing_decor`;
TRUNCATE TABLE `character_housing_fixtures`;
TRUNCATE TABLE `character_housing_catalog`;

-- Reset all plot assignments (255 = INVALID_PLOT_INDEX = no plot)
UPDATE `neighborhood_members` SET `plotIndex` = 255;

-- Reset housing tutorial quests so they can be re-done
-- Quest 91863: "My First Home" (tracks house purchase via kill credit 248858)
-- Quest 91968: "Welcome Home" (requires 91863 complete)
-- Quest 91969: "Time to Decorate" (requires 91968 complete)
DELETE FROM `character_queststatus` WHERE `quest` IN (91863, 91968, 91969);
DELETE FROM `character_queststatus_objectives` WHERE `quest` IN (91863, 91968, 91969);
DELETE FROM `character_queststatus_rewarded` WHERE `quest` IN (91863, 91968, 91969);

SELECT 'Housing data reset complete. Restart the server for changes to take effect.' AS status;
