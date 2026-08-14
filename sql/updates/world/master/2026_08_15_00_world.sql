--
-- Arathi Returning Player Experience ("Catch Up"), map 2927 - Hammerfall pad starters.
--
-- PROVENANCE / VERIFICATION
-- All ids and positions below come from a third-party capture of retail build 12.0.7.68453
-- that we do not hold and could not re-verify in this tree. Nothing here is verified against
-- our own DB2/world data, so every row carries VerifiedBuild 0 rather than claiming 68453.
-- UNVERIFIED: map 2927, zone/area 16432, creatures 244642 / 244643, quest 90882,
--             phase ids 26596 / 26618 / 27217.
-- Spawn guids use the reserved Arathi RPE block 11002000-11002099 (free in this tree; the
-- highest guid otherwise allocated is 11000831).
--

SET @CGUID := 11002000;

-- Creatures (Hammerfall pad)
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+1;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `VerifiedBuild`) VALUES
(@CGUID+0, 244642, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1086.4791, -3554.7744, 50.192024, 0.09973325, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0), -- Thrall (map 2927 Arathi RPE) - UNVERIFIED entry
(@CGUID+1, 244643, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1084.2153, -3559.9722, 50.44527,  5.0222363,  120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0); -- Lady Jaina Proudmoore (map 2927 Arathi RPE) - UNVERIFIED entry

-- Questgiver flag on the pad starters
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (244642, 244643);

-- 90882 "Gnoll Way" - Horde giver Thrall; Jaina present for the Alliance text variant
DELETE FROM `creature_queststarter` WHERE `quest`=90882 AND `id` IN (244642, 244643);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(244642, 90882, 0),
(244643, 90882, 0);

DELETE FROM `creature_questender` WHERE `quest`=90882 AND `id` IN (244642, 244643);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(244642, 90882, 0),
(244643, 90882, 0);

-- Cosmetic phases the client is put into when it enters map 2927 - UNVERIFIED phase ids
DELETE FROM `phase_name` WHERE `ID` IN (26596, 26618, 27217);
INSERT INTO `phase_name` (`ID`, `Name`) VALUES
(26596, 'Arathi RPE - Hammerfall login'),
(26618, 'Arathi RPE - Hammerfall login'),
(27217, 'Arathi RPE - persistent through farm');

DELETE FROM `phase_area` WHERE `PhaseId` IN (26596, 26618, 27217) AND `AreaId`=16432;
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
(16432, 26596, 'Arathi RPE zone - login phase'),
(16432, 26618, 'Arathi RPE zone - login phase'),
(16432, 27217, 'Arathi RPE zone - login phase');
