--
-- Arathi Returning Player Experience, map 2927 - Hammerfall pad ambience.
--
-- The pad is dressed with frozen-in-place Gnoll Assailants, a food vendor, two grunts and a
-- row of training dummies. Only the spawns that the source capture actually shows are added -
-- the visual count is higher than the wire set, and the difference is scene actors (see
-- 2026_08_15_06), not extra creatures.
--
-- The floating + rooted presentation is expressed the way this tree already does it for e.g.
-- 30679 Teleportation Portal: creature_template_difficulty.StaticFlags1 FLOATING (0x20000000)
-- | SESSILE (0x100) plus creature_template_addon.AnimTier 3.
--
-- PROVENANCE / VERIFICATION
-- Third-party capture of retail 12.0.7.68453, not re-verified here; VerifiedBuild stays 0.
-- UNVERIFIED: creatures 245026 / 245027 / 245028 / 249245, their model ids, ContentTuningID
--             4306, unit_flags values 33536 and 64.
-- Spawn guids continue the reserved Arathi RPE block 11002000-11002099.
--

SET @CGUID := 11002024;

-- ---------------------------------------------------------------------------
-- Template presentation
-- unit_flags 33536 = IMMUNE_TO_PC | IMMUNE_TO_NPC | CAN_SWIM
-- ---------------------------------------------------------------------------
UPDATE `creature_template_difficulty`
SET `ContentTuningID`=4306,
    `StaticFlags1`=536871168 -- FLOATING | SESSILE -> DisableGravity | Root
WHERE `Entry`=245027 AND `DifficultyID`=0;

UPDATE `creature_template`
SET `unit_flags`=33536
WHERE `entry`=245027;

-- Win'sa (Food Vendor) - npcflag 129 = gossip + vendor
UPDATE `creature_template`
SET `faction`=35,
    `npcflag`=129
WHERE `entry`=245026;

UPDATE `creature_template_difficulty`
SET `ContentTuningID`=4306
WHERE `Entry`=245026 AND `DifficultyID`=0;

-- Training Dummy - floating presentation
UPDATE `creature_template_difficulty`
SET `StaticFlags1`=536871168 -- FLOATING | SESSILE
WHERE `Entry`=249245 AND `DifficultyID`=0;

UPDATE `creature_template`
SET `unit_flags`=64
WHERE `entry`=249245;

-- AnimTier 3 (Fly) + VisFlags UNTRACKABLE for the floaters
DELETE FROM `creature_template_addon` WHERE `entry` IN (245027, 249245);
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(245027, 0, 0, 0, 3, 4, 1, 0, 0, 0, 0, 0, 0, NULL), -- Gnoll Assailant, held in stasis
(249245, 0, 0, 0, 3, 4, 1, 0, 0, 0, 0, 0, 0, NULL); -- Training Dummy, floating

-- ---------------------------------------------------------------------------
-- Pad spawns
-- ---------------------------------------------------------------------------
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+14;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `VerifiedBuild`) VALUES
-- 245027 Gnoll Assailant x7
(@CGUID+0,  245027, 2927, 16432, 16432, '0', 0, 0, 103286, 0, -1099.5348, -3538.7761, 51.677532, 5.7316, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+1,  245027, 2927, 16432, 16432, '0', 0, 0, 103695, 0, -1083.3837, -3541.8142, 52.47486,  4.9389, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+2,  245027, 2927, 16432, 16432, '0', 0, 0, 103694, 0, -1076.6423, -3550.4011, 51.509766, 3.1003, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+3,  245027, 2927, 16432, 16432, '0', 0, 0, 103695, 0, -1095.731,  -3562.3176, 49.279354, 0.6838, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+4,  245027, 2927, 16432, 16432, '0', 0, 0, 103694, 0, -1073.4567, -3557.6145, 51.731472, 2.6257, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+5,  245027, 2927, 16432, 16432, '0', 0, 0, 103286, 0, -1081.0017, -3560.2847, 51.0606,   2.3654, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+6,  245027, 2927, 16432, 16432, '0', 0, 0, 103287, 0, -1093.6285, -3548.1216, 49.634605, 5.0607, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
-- 245026 Win'sa (Food Vendor)
(@CGUID+7,  245026, 2927, 16432, 16432, '0', 0, 0, 93765,  0, -1089.5834, -3545.2188, 50.21548,  1.1165, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
-- 245028 Horde Grunt x2
(@CGUID+8,  245028, 2927, 16432, 16432, '0', 0, 0, 84548,  0, -1088.342,  -3542.4358, 50.374855, 3.9584, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+9,  245028, 2927, 16432, 16432, '0', 0, 0, 84546,  0, -1083.7812, -3553.5815, 50.545025, 2.1282, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
-- 249245 Training Dummy x5
(@CGUID+10, 249245, 2927, 16432, 16432, '0', 0, 0, 99693,  0, -1095.2067, -3534.9358, 52.150093, 4.1114, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+11, 249245, 2927, 16432, 16432, '0', 0, 0, 99693,  0, -1098.3038, -3531.007,  52.349316, 4.1114, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+12, 249245, 2927, 16432, 16432, '0', 0, 0, 99693,  0, -1105.4531, -3515.2778, 51.70594,  3.2898, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+13, 249245, 2927, 16432, 16432, '0', 0, 0, 99693,  0, -1104.4705, -3521.9202, 52.152374, 3.4049, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0),
(@CGUID+14, 249245, 2927, 16432, 16432, '0', 0, 0, 99693,  0, -1101.7101, -3526.342,  52.122147, 4.1114, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, 0);

-- ---------------------------------------------------------------------------
-- Keep gnolls from 2026_08_15_01 stand perfectly still. The capture only shows spline stubs
-- whose destination equals the spawn point, so there is no real path to import; give them a
-- small random wander until movement is captured properly.
-- Skips the farm pad guids 11002020-11002022 (Bruvk / farm Thrall / farm Jaina).
-- ---------------------------------------------------------------------------
UPDATE `creature`
SET `wander_distance`=5,
    `MovementType`=1
WHERE `guid` BETWEEN 11002002 AND 11002019
   OR `guid`=11002023;
