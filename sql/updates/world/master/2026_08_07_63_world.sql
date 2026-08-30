--
-- Lindormi <Mythic Keystones> - Midnight identity, sniff-verified (C:\dumps\MPLUS_CONTENT_MINE_68275.md).
-- Web guides cite NPC 244792, but that entry appears in NO 68275 capture; the live Midnight Lindormi is
-- creature 259053 (subname confirmed on the wire via QUERY_CREATURE_RESPONSE), observed spawning inside
-- Algeth'ar Academy (map 2526) at run start as the keystone-lowering / end-of-run exchange NPC.
-- 197915 is the DF-era entry, also still queried by clients mid-run.
--
-- Spawn below = the sniff-observed Algeth'ar Academy position. City spawn (Silvermoon, next to the golden
-- Timeways portal) still needs a capture for exact coordinates. Other S1 dungeons' in-dungeon positions
-- likewise pending captures.
--

DELETE FROM `creature_template` WHERE `entry` = 259053;
INSERT INTO `creature_template` (`entry`, `KillCredit1`, `KillCredit2`, `name`, `femaleName`, `subname`, `TitleAlt`, `IconName`, `RequiredExpansion`, `VignetteID`, `unit_class`, `WidgetSetID`, `WidgetSetUnitConditionID`, `family`, `type`, `RacialLeader`, `movementId`, `VerifiedBuild`) VALUES
(259053, 0, 0, 'Lindormi', '', 'Mythic Keystones', '', '', 11, 0, 1, 0, 0, 0, 7, 0, 0, 68275);

UPDATE `creature_template` SET `ScriptName` = 'npc_lindormi', `npcflag` = 1, `faction` = 35 WHERE `entry` = 259053;

DELETE FROM `creature_template_difficulty` WHERE `Entry` = 259053;
INSERT INTO `creature_template_difficulty` (`Entry`, `DifficultyID`, `HealthScalingExpansion`, `HealthModifier`, `ManaModifier`, `CreatureDifficultyID`, `TypeFlags`, `TypeFlags2`, `TypeFlags3`) VALUES
(259053, 0, 11, 1, 1, 0, 0x2, 0, 0);

-- Algeth'ar Academy spawn (mythic difficulties only; sniffed create-block position)
DELETE FROM `creature` WHERE `guid` = 9000200;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `VerifiedBuild`) VALUES
(9000200, 259053, 2526, 0, 0, '8,23', 0, 0, 0, 0, 1417.47, -2786.39, 955.74, 2.13, 7200, 0, 0, 0, NULL, NULL, NULL, NULL, 68275);
