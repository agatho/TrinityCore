--
-- Sniff-mined M+/delve world data (C:\dumps\GULF_FONT_MINE.md).
--

-- Font of Power in Algeth'ar Academy (GO 246779; template ships in the imported world data).
-- Position from two identical create blocks in the 68275 m+ capture, guid confirmed by
-- QUERY_GAME_OBJECT_RESPONSE. rotation2/3 = sin/cos(o/2).
-- NOTE: start door GO 469490 was mined at (1410.405, -2772.776, 955.645, o=4.7136) but has no
-- gameobject_template in the world data yet - spawn deferred until a template (with display id) exists.
DELETE FROM `gameobject` WHERE `guid` = 9000300;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `VerifiedBuild`) VALUES
(9000300, 246779, 2526, 0, 0, '8,23', 0, 0, 1408.444, -2785.141, 955.656, 1.4030, 0, 0, 0.645530, 0.763735, 7200, 0, 1, 68275);

-- The Gulf of Memory (delve_template 25): full run data from the 12.0.1.66709 capture
-- (values that are stable across builds: scenario, gossip ids, world positions).
-- Entry NEW_WORLD map 2964 (155.129, 634.757, 187.334, o 2.8133); exit NEW_WORLD back to map 2694
-- at (48.420, 816.046, 1110.519, o 1.7356), right beside the entrance. Tier gossip menu 40278,
-- OptionID = 135349 - tier (Tier 1 = 135348 .. Tier 11 = 135338). Scenario 3177 (4 steps
-- 16081/16083/16084/16082). Entrance NPC: creature 212407 on map 2694.
UPDATE `delve_template` SET
    `scenarioId` = 3177, `activeScenarioId` = 3177,
    `gossipMenuId` = 40278, `firstTierGossipOptionId` = 135348,
    `entryX` = 155.129, `entryY` = 634.757, `entryZ` = 187.334, `entryO` = 2.8133,
    `exitX` = 48.420, `exitY` = 816.046, `exitZ` = 1110.519, `exitO` = 1.7356
WHERE `id` = 25;

-- Scenario routing for ScenarioMgr (delves are faction-neutral)
DELETE FROM `scenarios` WHERE `map` = 2964 AND `difficulty` = 208;
INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES
(2964, 208, 3177, 3177);

