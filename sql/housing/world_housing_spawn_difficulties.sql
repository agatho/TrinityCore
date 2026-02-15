-- Housing: Update spawnDifficulties for neighborhood map spawns
--
-- Housing neighborhood maps (2735, 2736) may be created as GarrisonMaps with
-- DIFFICULTY_NORMAL (1), but the imported sniff data has spawnDifficulties='0'
-- (DIFFICULTY_NONE only). Setting to '0,1' ensures creatures and gameobjects
-- appear regardless of which map creation path is used.

UPDATE creature SET spawnDifficulties = '0,1' WHERE map IN (2735, 2736) AND spawnDifficulties = '0';
UPDATE gameobject SET spawnDifficulties = '0,1' WHERE map IN (2735, 2736) AND spawnDifficulties = '0';
