--
-- Arathi Returning Player Experience, map 2927 - keep gnolls, Go'shek Farm pad, quest chain.
--
-- 90882 (kill credit) -> 90883 (travel to the farm) -> farm openers 90885 / 90886 / 90887.
--
-- PROVENANCE / VERIFICATION
-- Positions and ids come from a third-party capture of retail 12.0.7.68453 that we could not
-- re-verify here, so VerifiedBuild stays 0.
-- UNVERIFIED: creatures 244669 / 244670 / 244671 / 244672 / 244729 / 244656 / 244655,
--             quests 90882 / 90883 / 90885 / 90886 / 90887, phase ids 26588 / 26599.
-- Spawn guids continue the reserved Arathi RPE block 11002000-11002099.
--

SET @CGUID := 11002002;

-- ---------------------------------------------------------------------------
-- Hostile keep gnolls (faction 16 matches the existing 244671 template)
-- ---------------------------------------------------------------------------
UPDATE `creature_template` SET `faction`=16 WHERE `entry` IN (244669, 244670, 244672) AND `faction`=0;

-- Keep cluster + farm pad
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+21;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `VerifiedBuild`) VALUES
-- 244672 Gnoll Bruiser (the quest objective ObjectID)
(@CGUID+0,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -981.45026, -3513.761,  56.992092, 3.3214676, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+1,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -988.5839,  -3518.1963, 56.992092, 4.0285850, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+2,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -994.482,   -3525.4333, 56.992092, 0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+3,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -999.4901,  -3530.6072, 56.98025,  0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+4,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1005.5139, -3536.0608, 56.57396,  0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+5,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -996.47,    -3527.87,   56.99,     0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+6,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -975.55646, -3512.6892, 56.992092, 0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+7,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -972.067,   -3511.9495, 56.992092, 0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+8,  244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -962.2465,  -3509.7275, 56.992096, 0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
-- 244669 / 244670 / 244671 (KillCredit1 -> 244672)
(@CGUID+9,  244669, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1016.804,  -3519.98,   61.393707, 3.3338888, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+10, 244669, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1020.7656, -3517.7917, 61.757744, 0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+11, 244669, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1015.9045, -3519.804,  61.477505, 3.5282719, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+12, 244670, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1013.5469, -3574.7432, 56.647884, 5.3338089, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+13, 244670, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1014.908,  -3516.7205, 61.730278, 3.7660933, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+14, 244671, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1033.3021, -3551.8108, 56.267727, 3.1737113, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+15, 244671, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1010.8646, -3563.9548, 56.647884, 1.6648406, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+16, 244671, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1025.5286, -3489.8262, 62.304573, 0.7926827, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+17, 244671, 2927, 16432, 16432, '0', 0, 0, 0, 0, -985.0482,  -3541.8672, 56.992737, 0.9016410, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
-- Go'shek Farm pad - Bruvk / Thrall / Jaina
(@CGUID+18, 244729, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1522.33,   -3089.36,   26.34,     2.175,     120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0), -- Farmer Bruvk
(@CGUID+19, 244656, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1522.62,   -3085.87,   26.17,     1.533,     120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0), -- Thrall (farm)
(@CGUID+20, 244655, 2927, 16432, 16432, '0', 0, 0, 0, 0, -1525.88,   -3089.80,   26.12,     3.182,     120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0), -- Lady Jaina Proudmoore (farm)
-- One more 244672 for kill-count headroom while the others respawn
(@CGUID+21, 244672, 2927, 16432, 16432, '0', 0, 0, 0, 0, -983.041,   -3514.0503, 56.992092, 0,         120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0);

-- Questgiver flags on the farm pad
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (244729, 244656, 244655);
-- Bruvk was faction 0 (unusable); friendly ambient like the farm Jaina
UPDATE `creature_template` SET `faction`=35 WHERE `entry`=244729 AND `faction`=0;

-- ---------------------------------------------------------------------------
-- Horde saw a quest marker on both Jaina and Thrall - drop Jaina as 90882 starter/ender.
-- A per-NPC Alliance-only starter is not expressible through conditions; the Alliance
-- variant needs its own pass.
-- ---------------------------------------------------------------------------
DELETE FROM `creature_queststarter` WHERE `quest`=90882 AND `id`=244643;
DELETE FROM `creature_questender` WHERE `quest`=90882 AND `id`=244643;

-- ---------------------------------------------------------------------------
-- 90882 -> 90883 -> farm openers.
-- RewardNextQuest is 0 in the DB2-derived quest_template; set it so GetNextQuest /
-- the quest details path offers the follow-up on the same giver.
-- ---------------------------------------------------------------------------
UPDATE `quest_template` SET `RewardNextQuest`=90883 WHERE `ID`=90882 AND `RewardNextQuest`=0;
UPDATE `quest_template` SET `RewardNextQuest`=90885 WHERE `ID`=90883 AND `RewardNextQuest`=0;

DELETE FROM `creature_queststarter` WHERE `quest`=90883 AND `id`=244642;
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(244642, 90883, 0); -- "To Go'shek Farm" - Thrall (Hammerfall)

DELETE FROM `creature_questender` WHERE `quest`=90883 AND `id`=244729;
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(244729, 90883, 0); -- turn-in at Farmer Bruvk

DELETE FROM `creature_queststarter` WHERE `quest` IN (90885, 90886, 90887) AND `id` IN (244729, 244656, 244655);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(244729, 90885, 0), -- Bruvk
(244656, 90886, 0), -- farm Thrall
(244655, 90887, 0); -- farm Jaina

-- Enders for the farm openers: keep the pre-existing rows (244656 -> 90885/90886,
-- 244655 -> 90887); additionally let Bruvk end 90885, which he also offers.
DELETE FROM `creature_questender` WHERE `quest`=90885 AND `id`=244729;
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(244729, 90885, 0);

-- ---------------------------------------------------------------------------
-- Phase progression (CONDITION_SOURCE_TYPE_PHASE 26 + CONDITION_QUESTSTATE 47)
-- Login phases 26596 / 26618 / 27217 come from 2026_08_15_00.
-- 90882 rewarded -> drop 26618; 90883 taken -> drop 26596; farm -> 26588 / 26599.
-- ---------------------------------------------------------------------------
DELETE FROM `phase_name` WHERE `ID` IN (26588, 26599);
INSERT INTO `phase_name` (`ID`, `Name`) VALUES
(26588, 'Arathi RPE - Go''shek Farm arrive'),
(26599, 'Arathi RPE - Go''shek Farm persistent');

DELETE FROM `phase_area` WHERE `PhaseId` IN (26588, 26599) AND `AreaId`=16432;
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
(16432, 26588, 'Arathi RPE - farm arrive phase'),
(16432, 26599, 'Arathi RPE - farm persistent phase');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup` IN (26618, 26596, 26588, 26599) AND `SourceEntry`=0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `Comment`) VALUES
-- 26618: only while 90882 is not rewarded
(26, 26618, 0, 0, 0, 47, 0, 90882, 64, 0, 1, 'Arathi RPE: phase 26618 while 90882 not rewarded'),
-- 26596: only while 90883 has not been taken
(26, 26596, 0, 0, 0, 47, 0, 90883, 74, 0, 1, 'Arathi RPE: phase 26596 while 90883 not incomplete|complete|rewarded'),
-- 26588: after 90883 is started/done, until 90885 is accepted
(26, 26588, 0, 0, 0, 47, 0, 90883, 74, 0, 0, 'Arathi RPE: phase 26588 if 90883 incomplete|complete|rewarded'),
(26, 26588, 0, 0, 0, 47, 0, 90885, 74, 0, 1, 'Arathi RPE: phase 26588 while 90885 not incomplete|complete|rewarded'),
-- 26599: once the 90883 segment is reached (stays after 90885 is accepted)
(26, 26599, 0, 0, 0, 47, 0, 90883, 74, 0, 0, 'Arathi RPE: phase 26599 if 90883 incomplete|complete|rewarded');

-- 27217 stays unconditional on area 16432 - persistent through the farm segment.
