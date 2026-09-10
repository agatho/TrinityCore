-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- DB/Forbidden Reach: lower War Creche for quest 64864 (Phase 1b)
-- Curated from temp/Horde team from first quest till orgrimmar/Horde_team_Dracthyr_from_first_quest_till_orgrimmar_world.sql
-- NPCs: 181712 Kethahn, 181680 Tethalash, 183380 Azurathel (stasis), 187015 cave-in credit; GO 376256 Cave In

SET @CGUID := 9003900;
SET @OGUID := 9003870;
SET @NPCTEXT_KETHAHN := 6486401;

-- Creature spawns (map 2570, area 13806)
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+3;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(@CGUID+0, 181712, 2570, 13769, 13806, '0', 0, 0, 0, 5819.31787109375, -3077.546875, 213.093170166015625, 5.932741165161132812, 120, 0, 0, 100, 0, '', NULL, 64978), -- Talon Kethahn
(@CGUID+1, 181680, 2570, 13769, 13806, '0', 0, 0, 0, 5779.369140625, -3038.767333984375, 210.49530029296875, 2.480190753936767578, 120, 0, 0, 100, 0, '', NULL, 64978), -- Tethalash
(@CGUID+2, 183380, 2570, 13769, 13806, '0', 0, 0, 1, 5800.23193359375, -2905.8681640625, 207.186798095703125, 5.469690322875976562, 120, 0, 0, 100, 0, '', NULL, 64978), -- Scalecommander Azurathel (stasis)
(@CGUID+3, 187015, 2570, 13769, 13806, '0', 0, 0, 0, 5826.5224609375, -3039.640625, 211.668548583984375, 2.422572612762451171, 120, 0, 0, 100, 0, '', NULL, 64978); -- [DNT] Kill Credit: Cave In

-- Cave In gameobject (lower cavern)
DELETE FROM `gameobject` WHERE `guid`=@OGUID+0;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `VerifiedBuild`) VALUES
(@OGUID+0, 376256, 2570, 13769, 13806, '0', 0, 0, 5835.02099609375, -3041.69970703125, 211.4984893798828125, 2.422568082809448242, 0, 0, 0.936068534851074218, 0.351817727088928222, 120, 255, 1, 64978);

DELETE FROM `gameobject_template_addon` WHERE `entry`=376256;
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `WorldEffectID`, `AIAnimKitID`) VALUES
(376256, 114, 0, 0, 0);

-- Template flags (sniff: lower War Creche interaction wiring)
UPDATE `creature_template` SET `faction`=35, `npcflag`=1, `unit_flags`=0x300, `unit_flags2`=0x4000800, `unit_flags3`=0x41000000 WHERE `entry`=181712;
UPDATE `creature_template` SET `faction`=35, `npcflag`=16777216, `unit_flags2`=0x800, `unit_flags3`=0x80200 WHERE `entry`=181680;
UPDATE `creature_template` SET `faction`=35, `npcflag`=16777216, `unit_flags2`=0x800, `unit_flags3`=0x80200 WHERE `entry`=183380;
UPDATE `creature_template` SET `faction`=35, `unit_flags`=0x2000300, `unit_flags2`=0x800, `unit_flags3`=0x41000001 WHERE `entry`=187015;

-- Stasis / feign-death auras on spawn
DELETE FROM `creature_template_addon` WHERE `entry` IN (181712, 181680, 183380);
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvpFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(181712, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '29266'), -- Permanent Feign Death
(181680, 0, 0, 0, 0, 0, 1, 0, 0, 24296, 0, 0, 0, '362331'), -- Stasis
(183380, 0, 0, 0, 0, 0, 1, 0, 0, 24297, 0, 0, 0, '362331'); -- Stasis

-- Kethahn gossip (menu 27444, broadcast 213144)
DELETE FROM `creature_template_gossip` WHERE `CreatureID`=181712 AND `MenuID`=27444;
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(181712, 27444, 64978);

DELETE FROM `npc_text` WHERE `ID`=@NPCTEXT_KETHAHN;
INSERT INTO `npc_text` (`ID`, `Probability0`, `Probability1`, `Probability2`, `Probability3`, `Probability4`, `Probability5`, `Probability6`, `Probability7`, `BroadcastTextId0`, `BroadcastTextId1`, `BroadcastTextId2`, `BroadcastTextId3`, `BroadcastTextId4`, `BroadcastTextId5`, `BroadcastTextId6`, `BroadcastTextId7`, `VerifiedBuild`) VALUES
(@NPCTEXT_KETHAHN, 1, 0, 0, 0, 0, 0, 0, 0, 213144, 0, 0, 0, 0, 0, 0, 0, 64978);

DELETE FROM `gossip_menu` WHERE `MenuID`=27444;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES
(27444, @NPCTEXT_KETHAHN, 64978);

-- Kethahn TALKTO credit (TalkedToCreature not called from gossip hello in core)
UPDATE `creature_template` SET `AIName`='SmartAI', `ScriptName`='' WHERE `entry`=181712;
DELETE FROM `smart_scripts` WHERE `entryorguid`=181712 AND `source_type`=0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(181712, 0, 0, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 153, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'Talon Kethahn - On Gossip Hello - Credit Quest Objective Talk To');

-- Spellclick on Tethalash and Azurathel (stasis awaken)
DELETE FROM `npc_spellclick_spells` WHERE `npc_entry` IN (181680, 183380);
INSERT INTO `npc_spellclick_spells` (`npc_entry`, `spell_id`, `cast_flags`, `user_type`) VALUES
(181680, 362355, 1, 0),
(183380, 362355, 1, 0);

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=18 AND `SourceGroup` IN (181680, 183380) AND `SourceEntry`=362355 AND `SourceId`=0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(18, 181680, 362355, 0, 0, 47, 0, 64864, 8, 0, 0, 0, 0, '', 'Spellclick Tethalash - Disintegrate if quest Awaken, Dracthyr in log'),
(18, 183380, 362355, 0, 0, 47, 0, 64864, 8, 0, 0, 0, 0, '', 'Spellclick Scalecommander Azurathel - Disintegrate if quest Awaken, Dracthyr in log');

-- Quest turn-in
DELETE FROM `creature_questender` WHERE `id`=181056 AND `quest`=64864;
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(181056, 64864, 64978);
