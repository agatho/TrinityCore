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
