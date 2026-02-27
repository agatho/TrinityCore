-- Housing: Add MapDifficulty entries for neighborhood maps
--
-- ROOT CAUSE: Without MapDifficulty entries, ObjectMgr::ParseSpawnDifficulties()
-- strips all spawn difficulties as "unsupported for map" because spawnMasks[mapId]
-- is empty. This results in empty spawnDifficulties vectors, and Map.cpp ~line 2394
-- skips every spawn during grid loading.
--
-- Both DIFFICULTY_NONE (0) and DIFFICULTY_NORMAL (1) are added because housing maps
-- may be created as either WorldMaps (difficulty 0) or GarrisonMaps (difficulty 1)
-- depending on the map's Garrison flag in Map.db2.
-- VerifiedBuild=-1 marks these as custom server-side additions.

DELETE FROM map_difficulty WHERE ID IN (900001, 900002, 900003, 900004, 900005, 900006);
INSERT INTO map_difficulty (ID, DifficultyID, LockID, ResetInterval, MaxPlayers, ItemContext, ItemContextPickerID, Flags, ContentTuningID, WorldStateExpressionID, MapID, VerifiedBuild)
VALUES
  (900001, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2735, -1),  -- Founder's Point (Alliance), DIFFICULTY_NONE
  (900002, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2735, -1),  -- Founder's Point (Alliance), DIFFICULTY_NORMAL
  (900003, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2736, -1),  -- Razorwind Shores (Horde), DIFFICULTY_NONE
  (900004, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2736, -1),  -- Razorwind Shores (Horde), DIFFICULTY_NORMAL
  (900005, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2783, -1),  -- Home Interior, DIFFICULTY_NONE
  (900006, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2783, -1);  -- Home Interior, DIFFICULTY_NORMAL
