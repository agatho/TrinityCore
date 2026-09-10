-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- DH intro P1 — late Mardum critical path (Horde)
-- Evidence: retail sniff temp/retail-sniff/dh-intro/{player-choice,quest-timeline,world-transitions,scenes}.txt
--           EvryDb2Export 12.0.7.67808 spells 194938/194939/194940/192140/192141; Wowhead UI→world affine

SET @CGUID := 11800156;
SET @OGUID := 11800005;

-- PlayerChoice 231 — Fel Secrets (Havoc 478 / Vengeance 479)
DELETE FROM `playerchoice` WHERE `ChoiceId`=231;
INSERT INTO `playerchoice` (`ChoiceId`, `UiTextureKitId`, `SoundKitId`, `CloseSoundKitId`, `Duration`, `Question`, `PendingChoiceText`, `InfiniteRange`, `HideWarboardHeader`, `KeepOpenAfterChoice`, `ShowChoicesAsList`, `ForceDontShowChoicesAsList`, `VerifiedBuild`) VALUES
(231, 0, 0, 0, NULL, 'Choose between Havoc & Vengeance', '', 0, 0, 0, 0, 0, 66384);

UPDATE `playerchoice` SET `ScriptName`='playerchoice_mardum_fel_secrets' WHERE `ChoiceId`=231;

DELETE FROM `playerchoice_response` WHERE `ChoiceId`=231 AND `ResponseId` IN (478,479);
INSERT INTO `playerchoice_response` (`ChoiceId`, `ResponseId`, `Index`, `ChoiceArtFileId`, `Flags`, `WidgetSetId`, `UiTextureAtlasElementID`, `SoundKitId`, `GroupId`, `Header`, `Subheader`, `ButtonTooltip`, `Answer`, `Description`, `Confirmation`, `RewardQuestID`, `UiTextureKitID`, `VerifiedBuild`) VALUES
(231, 478, 0, 1274664, 0, 0, 0, 0, 0, 'Havoc', '', '', 'Havoc', 'Continue your mastery of Havoc.', '', 0, 0, 66384),
(231, 479, 1, 1274665, 0, 0, 0, 0, 0, 'Vengeance', '', '', 'Vengeance', 'Change to mastery of Vengeance.', '', 0, 0, 66384);

DELETE FROM `playerchoice_response_reward` WHERE `ChoiceId`=231 AND `ResponseId` IN (478,479);
INSERT INTO `playerchoice_response_reward` (`ChoiceId`, `ResponseId`, `TitleId`, `PackageId`, `SkillLineId`, `SkillPointCount`, `ArenaPointCount`, `HonorPointCount`, `Money`, `Xp`, `VerifiedBuild`) VALUES
(231, 478, 0, 0, 0, 0, 0, 0, 12300, 1700, 66384),
(231, 479, 0, 0, 0, 0, 0, 0, 12300, 1700, 66384);

-- Quest starters / enders (sniff giver order)
DELETE FROM `creature_queststarter` WHERE `quest` IN (40222,40051,39516,39663,38728,38729,38766);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(98711, 40222),
(99045, 40051),
(93127, 39516),
(93127, 39663),
(97297, 38728),
(97303, 38729),
(93759, 38766);

-- Scene 1142 (Keystone / RewardSpell 193387)
DELETE FROM `scene_template` WHERE `SceneId`=1142;
INSERT INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `Encrypted`, `ScriptName`) VALUES
(1142, 20, 1512, 0, '');

-- Transfer 192141 orientation (sniff NEW_WORLD O=1.6589355); positions already present
UPDATE `spell_target_position` SET `Orientation`=1.6589355, `VerifiedBuild`=66384 WHERE `ID`=192141;

-- Prolifica loot — Tome of Fel Secrets (quest item)
-- Modern loot schema: ItemType (0=item), not legacy Reference
DELETE FROM `creature_loot_template` WHERE `Entry`=98986 AND `ItemType`=0 AND `Item`=129957;
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(98986, 0, 129957, 100, 1, 1, 0, 1, 1, 'Prolifica - Tome of Fel Secrets');

DELETE FROM `creature_loot_template` WHERE `Entry`=93802 AND `ItemType`=0 AND `Item`=124672;
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(93802, 0, 124672, 100, 1, 1, 0, 1, 1, 'Brood Queen Tyranna - Sargerite Keystone');

-- ScriptNames (do NOT override SmartAI on 93127/96655/96420 — use smart_scripts below)
UPDATE `creature_template` SET `ScriptName`='npc_mardum_cry_havoc_korvas' WHERE `entry`=99045;
UPDATE `creature_template` SET `ScriptName`='npc_mardum_cry_havoc_mannethrel' WHERE `entry`=96652;
UPDATE `creature_template` SET `ScriptName`='npc_mardum_izal_whitemoon_felbat' WHERE `entry`=96653;
UPDATE `creature_template` SET `ScriptName`='npc_mardum_brood_queen_tyranna' WHERE `entry`=93802;

-- Cry Havoc teach credits via SmartAI gossip (preserves existing accept Talk scripts)
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (93127,96655,96420) AND `source_type`=0 AND `id` IN (10,11,12,13);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(93127, 0, 10, 0, '', 62, 0, 100, 0, 18435, 1, 0, 0, 0, '', 85, 195020, 0, 0, 0, 0, 0, 0, NULL, 7, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 'Kayn - On gossip 18435/1 - invoker cast 195020 (Cry Havoc)'),
(93127, 0, 11, 0, '', 62, 0, 100, 0, 18435, 2, 0, 0, 0, '', 85, 195020, 0, 0, 0, 0, 0, 0, NULL, 7, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 'Kayn - On gossip 18435/2 - invoker cast 195020 (Cry Havoc)'),
(96655, 0, 10, 0, '', 62, 0, 100, 0, 18935, 0, 0, 0, 0, '', 85, 194996, 0, 0, 0, 0, 0, 0, NULL, 7, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 'Allari - On gossip 18935/0 - invoker cast 194996 (Cry Havoc)'),
(96655, 0, 11, 0, '', 62, 0, 100, 0, 18935, 1, 0, 0, 0, '', 85, 194996, 0, 0, 0, 0, 0, 0, NULL, 7, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 'Allari - On gossip 18935/1 - invoker cast 194996 (Cry Havoc)'),
(96420, 0, 10, 0, '', 62, 0, 100, 0, 18936, 0, 0, 0, 0, '', 85, 195019, 0, 0, 0, 0, 0, 0, NULL, 7, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 'Cyana - On gossip 18936/0 - invoker cast 195019 (Cry Havoc)'),
(96420, 0, 11, 0, '', 62, 0, 100, 0, 18936, 1, 0, 0, 0, '', 85, 195019, 0, 0, 0, 0, 0, 0, NULL, 7, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 'Cyana - On gossip 18936/1 - invoker cast 195019 (Cry Havoc)');

UPDATE `gameobject_template` SET `ScriptName`='go_mardum_illidari_banner' WHERE `entry` IN (243965,243967,243968);
UPDATE `gameobject_template` SET `ScriptName`='go_mardum_sargerite_keystone' WHERE `entry`=245728;

-- Creature spawns (Wowhead UI% affine + sniff Z where known)
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+34;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
-- Forge / Kayn camp (phase 5160)
(@CGUID+0, 99045, 1481, 7705, 7712, '0', 5160, 0, 0, 1454.50, 1760.20, 54.5213, 0.80, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+1, 96652, 1481, 7705, 7712, '0', 5160, 0, 0, 1463.33, 1771.11, 54.5213, 3.80, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+2, 96653, 1481, 7705, 7712, '0', 5160, 0, 0, 1456.87, 1688.88, 54.5213, 1.50, 120, 0, 0, 100, 0, '', NULL, 66384),
-- Foul Creche
(@CGUID+3, 98711, 1481, 7705, 7749, '0', 0, 0, 0, 1732.72, 1281.43, 70.00, 2.40, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+4, 98986, 1481, 7705, 7749, '0', 0, 0, 0, 1877.37, 1137.49, 70.00, 3.90, 120, 0, 0, 100, 0, '', NULL, 66384),
-- Keystone plateau (sniff scene 1142 Z)
(@CGUID+5, 97297, 1481, 7705, 7748, '0', 0, 0, 0, 1474.79, 1411.79, 208.74, 0.10, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+6, 97303, 1481, 7705, 7748, '0', 0, 0, 0, 1632.63, 1409.86, 208.74, 3.50, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+7, 93802, 1481, 7705, 7748, '0', 0, 0, 0, 1568.43, 1418.13, 208.74, 4.20, 120, 0, 0, 100, 0, '', NULL, 66384),
-- Missing Legion Devastators for bombardment banners
(@CGUID+8, 96732, 1481, 7705, 7705, '0', 0, 0, 0, 1358.73, 1426.31, 100.00, 2.50, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+9, 96731, 1481, 7705, 7705, '0', 0, 0, 0, 1548.56, 1212.59, 90.00, 1.20, 120, 0, 0, 100, 0, '', NULL, 66384),
-- Their Numbers Are Legion — Soul Harvester / Doom Slayer contributors only
-- (do NOT spawn Documented-only rares / King Voras 97059)
(@CGUID+10, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1520.00, 1520.00, 78.00, 0.50, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+11, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1570.00, 1570.00, 82.00, 1.50, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+12, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1620.00, 1520.00, 86.00, 2.50, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+13, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1480.00, 1580.00, 76.00, 3.50, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+14, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1550.00, 1500.00, 80.00, 0.00, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+15, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1600.00, 1550.00, 85.00, 1.00, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+16, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1500.00, 1600.00, 75.00, 2.00, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+17, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1650.00, 1480.00, 90.00, 3.00, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+18, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1580.00, 1620.00, 88.00, 4.00, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+19, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1630.00, 1600.00, 89.00, 5.00, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+20, 93716, 1481, 7705, 7705, '0', 0, 0, 0, 1530.00, 1540.00, 79.00, 0.20, 120, 5, 0, 100, 1, '', NULL, 66384),
(@CGUID+21, 93716, 1481, 7705, 7705, '0', 0, 0, 0, 1590.00, 1490.00, 83.00, 1.20, 120, 5, 0, 100, 1, '', NULL, 66384),
(@CGUID+22, 93716, 1481, 7705, 7705, '0', 0, 0, 0, 1610.00, 1590.00, 87.00, 2.20, 120, 5, 0, 100, 1, '', NULL, 66384),
(@CGUID+23, 93716, 1481, 7705, 7705, '0', 0, 0, 0, 1490.00, 1510.00, 77.00, 3.20, 120, 5, 0, 100, 1, '', NULL, 66384),
(@CGUID+24, 93716, 1481, 7705, 7705, '0', 0, 0, 0, 1660.00, 1540.00, 91.00, 4.20, 120, 5, 0, 100, 1, '', NULL, 66384),
(@CGUID+25, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1510.00, 1560.00, 78.00, 0.70, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+26, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1560.00, 1610.00, 81.00, 1.70, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+27, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1610.00, 1470.00, 84.00, 2.70, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+28, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1470.00, 1530.00, 76.00, 3.70, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+29, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1640.00, 1580.00, 87.00, 4.70, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+30, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1535.00, 1485.00, 79.00, 5.70, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+31, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1585.00, 1535.00, 82.00, 0.30, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+32, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1635.00, 1515.00, 85.00, 1.30, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+33, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1495.00, 1595.00, 77.00, 2.30, 120, 0, 0, 100, 0, '', NULL, 66384),
(@CGUID+34, 97382, 1481, 7705, 7705, '0', 0, 0, 0, 1670.00, 1560.00, 90.00, 3.30, 120, 0, 0, 100, 0, '', NULL, 66384);

-- GameObject spawns
DELETE FROM `gameobject` WHERE `guid` BETWEEN @OGUID+0 AND @OGUID+6;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `VerifiedBuild`) VALUES
(@OGUID+0, 245112, 1481, 7705, 7712, '0', 5160, 0, 1457.27, 1761.83, 54.5213, 4.90, 0, 0, 0.626225, 0.779641, 120, 255, 1, 66384),
(@OGUID+1, 244466, 1481, 7705, 7748, '0', 0, 0, 1640.05, 1407.90, 208.74, 3.50, 0, 0, 0.983255, 0.182231, 120, 255, 1, 66384),
(@OGUID+2, 245728, 1481, 7705, 7748, '0', 0, 0, 1632.68, 1406.12, 208.74, 3.50, 0, 0, 0.983255, 0.182231, 120, 255, 1, 66384),
(@OGUID+3, 243965, 1481, 7705, 7705, '0', 0, 0, 1795.00, 1565.00, 87.1311, 2.6905, 0, 0, 0.970941, 0.239315, 120, 255, 1, 66384),
(@OGUID+4, 243968, 1481, 7705, 7705, '0', 0, 0, 1358.73, 1426.31, 100.00, 2.50, 0, 0, 0.948985, 0.315322, 120, 255, 1, 66384),
(@OGUID+5, 243967, 1481, 7705, 7705, '0', 0, 0, 1548.56, 1212.59, 90.00, 1.20, 0, 0, 0.564643, 0.825336, 120, 255, 1, 66384),
(@OGUID+6, 243965, 1481, 7705, 7705, '0', 0, 0, 1818.65, 1532.93, 87.00, 2.40, 0, 0, 0.932039, 0.362358, 120, 255, 1, 66384);

-- Gossip visibility: Cry Havoc teach options only while 39516 incomplete; Izal felbat while 39663 incomplete
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=15 AND `SourceGroup` IN (18435,18935,18936,18937,18823,18776) AND `SourceEntry` IN (0,1,2);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `Comment`) VALUES
(15, 18435, 1, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Kayn teach option if Cry Havoc incomplete'),
(15, 18435, 2, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Kayn teach option (alt) if Cry Havoc incomplete'),
(15, 18935, 0, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Allari teach if Cry Havoc incomplete'),
(15, 18935, 1, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Allari teach (alt) if Cry Havoc incomplete'),
(15, 18936, 0, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Cyana teach if Cry Havoc incomplete'),
(15, 18936, 1, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Cyana teach (alt) if Cry Havoc incomplete'),
(15, 18937, 0, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Korvas teach if Cry Havoc incomplete'),
(15, 18937, 1, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Korvas teach (alt) if Cry Havoc incomplete'),
(15, 18823, 0, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Mannethrel teach if Cry Havoc incomplete'),
(15, 18823, 1, 0, 0, 47, 0, 39516, 8, 0, '', 0, 'Mannethrel teach (alt) if Cry Havoc incomplete'),
(15, 18776, 0, 0, 0, 47, 0, 39663, 8, 0, '', 0, 'Izal felbat if On Felbat Wings incomplete');
