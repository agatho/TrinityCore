-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- DH intro P1 — late-chain PrevQuest gates + 38727/38819 spawn repair
-- Evidence:
--   Retail timeline temp/retail-sniff/dh-intro/quest-timeline.txt
--     39495 → (38727 ‖ 38819 ‖ 38725) → 40222 → 40051 → 39516 → 39663 → 38728 → 38729
--   ExclusiveGroup verify (no PrevQ needed): 38765/38766 → 38813; 38759/39049/40379 → 39050
--   dhi-horde1 CREATE Stationary banners + devastator Positions:
--     243968 @ 1356.149, 1436.212, 37.669 O=3.8069 + 96732 @ 1382.387, 1452.385, 33.454 O=1.2206
--     243967 @ 1547.845, 1221.847, 74.339 O=4.6885 + 96731 @ 1524.583, 1248.476, 70.870 O=1.7406
--     243965 @ 1813.363, 1543.425, 88.373 O=5.7421 + 93762 @ 1800.392, 1569.828, 87.131 O=2.6905
--   38819: sniff CREATE 97382 / 101947 / 97059 / 96402 / 96280 (replace round-number stub grid)
-- Wowhead: https://www.wowhead.com/quest=38727/stop-the-bombardment (objective names; coords from sniff)

SET @CGUID := 11800198;
SET @OGUID := 11800014;

-- =============================================================================
-- Task 1: late offer PrevQuestID (missing quest_template_addon rows)
-- =============================================================================
DELETE FROM `quest_template_addon` WHERE `ID` IN (40222, 40051, 39516, 39663, 38728, 38729);
INSERT INTO `quest_template_addon` (`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`, `BreadcrumbForQuestId`, `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`, `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`, `ProvidedItemCount`, `SpecialFlags`, `ScriptName`) VALUES
(40222, 0, 0, 0, 38725, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Imp Mother's Tome after Into the Foul Creche
(40051, 0, 0, 0, 40222, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Fel Secrets after Tome
(39516, 0, 0, 0, 40051, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Cry Havoc after Fel Secrets
(39663, 0, 0, 0, 39516, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- On Felbat Wings after Cry Havoc
(38728, 0, 0, 0, 39663, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Keystone after Felbat
(38729, 0, 0, 0, 38728, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''); -- Return to BT after Keystone

-- =============================================================================
-- Task 2a: Stop the Bombardment — relocate banners + devastators (sniff CREATE)
-- =============================================================================
-- Drop duplicate Soul Engine banner (keep one sniff Stationary)
DELETE FROM `gameobject` WHERE `guid`=11800011 AND `id`=243965;

UPDATE `gameobject` SET
  `position_x`=1356.1493, `position_y`=1436.2118, `position_z`=37.6693, `orientation`=3.8069,
  `rotation0`=0, `rotation1`=0, `rotation2`=0.945179, `rotation3`=-0.326552,
  `areaId`=7740, `PhaseId`=0, `VerifiedBuild`=68887
WHERE `guid`=11800009 AND `id`=243968;

UPDATE `gameobject` SET
  `position_x`=1547.8455, `position_y`=1221.8473, `position_z`=74.3391, `orientation`=4.6885,
  `rotation0`=0, `rotation1`=0, `rotation2`=0.715502, `rotation3`=-0.698611,
  `areaId`=7705, `PhaseId`=0, `VerifiedBuild`=68887
WHERE `guid`=11800010 AND `id`=243967;

UPDATE `gameobject` SET
  `position_x`=1813.3629, `position_y`=1543.4254, `position_z`=88.3732, `orientation`=5.7421,
  `rotation0`=0, `rotation1`=0, `rotation2`=0.267254, `rotation3`=-0.963626,
  `areaId`=7741, `PhaseId`=0, `VerifiedBuild`=68887
WHERE `guid`=11800008 AND `id`=243965;

-- Doom / Forge devastators (was co-located on wrong Z with banners)
UPDATE `creature` SET
  `position_x`=1382.3872, `position_y`=1452.3854, `position_z`=33.45425, `orientation`=1.2206,
  `areaId`=7740, `PhaseId`=0, `VerifiedBuild`=68887
WHERE `guid`=11800164 AND `id`=96732;

UPDATE `creature` SET
  `position_x`=1524.5834, `position_y`=1248.4757, `position_z`=70.86985, `orientation`=1.7406,
  `areaId`=7705, `PhaseId`=0, `VerifiedBuild`=68887
WHERE `guid`=11800165 AND `id`=96731;

-- Soul Engine devastator: ensure PhaseId=0 copy at sniff Position (within 50y of banner)
UPDATE `creature` SET
  `position_x`=1800.3923, `position_y`=1569.8281, `position_z`=87.13105, `orientation`=2.6905,
  `areaId`=7741, `PhaseId`=0, `VerifiedBuild`=68887
WHERE `guid`=6000552 AND `id`=93762;

-- =============================================================================
-- Task 2b: Their Numbers Are Legion — replace stub 97382 grid + add war-progress pool
-- =============================================================================
-- Remove SQL-18 stub Soul Harvesters + affine Doom Slayers (guids 11800166–11800190)
DELETE FROM `creature` WHERE `guid` BETWEEN 11800166 AND 11800190;

-- Sniff-backed pool only (dhi-horde1 CREATE). Weight sum >100 without inventing Z:
--   9×97382×5 + 97059×20 + 4×101947×2.5 + 6×96402×5 + 6×96280×1 + 1×93716×2.5 = 118.5
-- Full Amount=20 Soul Harvester density beyond these CREATE positions = NYI (sniff showed ≤9 uniques).
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+25;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
-- 97382 Soul Harvester (sniff CREATE)
(@CGUID+0,  97382, 1481, 7705, 7740, '0', 0, 0, 0, 1242.139, 1225.236, 94.087, 0.6140, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+1,  97382, 1481, 7705, 7740, '0', 0, 0, 0, 1277.160, 1297.424, 81.663, 0.0000, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+2,  97382, 1481, 7705, 7740, '0', 0, 0, 0, 1196.459, 1289.556, 24.950, 1.4861, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+3,  97382, 1481, 7705, 7740, '0', 0, 0, 0, 1204.110, 1294.332, 25.913, 0.0164, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+4,  97382, 1481, 7705, 7740, '0', 0, 0, 0, 1254.626, 1181.658, 99.794, 0.0164, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+5,  97382, 1481, 7705, 7740, '0', 0, 0, 0, 1315.855, 1200.941, 94.878, 1.4861, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+6,  97382, 1481, 7705, 7748, '0', 0, 0, 0, 1561.469, 1413.045, 149.386, 0.0164, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+7,  97382, 1481, 7705, 7705, '0', 0, 0, 0, 1518.792, 1094.896, 59.832, 0.1222, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+8,  97382, 1481, 7705, 7741, '0', 0, 0, 0, 1768.352, 1518.203, 69.972, 4.9120, 120, 0, 0, 100, 0, '', NULL, 68887),
-- King Voras (named boss, ProgressBarWeight=20) — CREATE @ 1137.596, 1236.752, 122.058
(@CGUID+9,  97059, 1481, 7705, 7740, '0', 0, 0, 0, 1137.5955, 1236.7517, 122.0579, 0.9285, 300, 8, 0, 100, 1, '', NULL, 68887),
-- Doom Fortress Stabilizer (ProgressBarWeight=2.5) — sniff CREATE Lows
(@CGUID+10, 101947, 1481, 7705, 7740, '0', 0, 0, 0, 1200.680, 1297.981, 99.024, 0.9223, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+11, 101947, 1481, 7705, 7740, '0', 0, 0, 0, 1198.134, 1268.410, 113.530, 0.9223, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+12, 101947, 1481, 7705, 7740, '0', 0, 0, 0, 1176.622, 1317.573, 97.801, 0.9223, 120, 0, 0, 100, 0, '', NULL, 68887),
(@CGUID+13, 101947, 1481, 7705, 7740, '0', 0, 0, 0, 1246.615, 1196.122, 96.521, 0.6050, 120, 0, 0, 100, 0, '', NULL, 68887),
-- Hulking Forgefiend Mo'arg (ProgressBarWeight=5) — sniff Positions near Forge
(@CGUID+14, 96402, 1481, 7705, 7705, '0', 0, 0, 0, 1522.156, 1212.832, 71.109, 1.0005, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+15, 96402, 1481, 7705, 7705, '0', 0, 0, 0, 1540.788, 1216.002, 71.135, 2.2114, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+16, 96402, 1481, 7705, 7705, '0', 0, 0, 0, 1472.854, 1162.365, 72.177, 2.6461, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+17, 96402, 1481, 7705, 7705, '0', 0, 0, 0, 1491.087, 1084.852, 72.877, 2.5732, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+18, 96402, 1481, 7705, 7705, '0', 0, 0, 0, 1409.154, 1063.050, 74.902, 2.0074, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+19, 96402, 1481, 7705, 7705, '0', 0, 0, 0, 1354.476, 1073.349, 82.211, 0.0801, 120, 5, 0, 100, 1, '', NULL, 68887),
-- Volatile Minion (ProgressBarWeight=1) — sniff Positions near Forge
(@CGUID+20, 96280, 1481, 7705, 7705, '0', 0, 0, 0, 1696.588, 1204.635, 71.271, 4.2480, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+21, 96280, 1481, 7705, 7705, '0', 0, 0, 0, 1622.897, 1189.746, 86.513, 0.0000, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+22, 96280, 1481, 7705, 7705, '0', 0, 0, 0, 1654.603, 1212.388, 87.249, 0.0000, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+23, 96280, 1481, 7705, 7705, '0', 0, 0, 0, 1701.126, 1204.885, 74.972, 2.4983, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+24, 96280, 1481, 7705, 7705, '0', 0, 0, 0, 1585.415, 1177.567, 84.609, 0.0000, 120, 5, 0, 100, 1, '', NULL, 68887),
(@CGUID+25, 96280, 1481, 7705, 7705, '0', 0, 0, 0, 1713.635, 1242.471, 87.339, 0.2464, 120, 5, 0, 100, 1, '', NULL, 68887);
