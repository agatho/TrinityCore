--
-- LOCAL DIVERGENCE FROM UPSTREAM - the Malacrass guid renumber is dropped and the boss formation
-- is re-pointed, deliberately. Related collision class to 2026_07_17_00_world.sql; read that
-- header too.
--
-- Upstream ships `SET @CGUID := 144991;` and renumbers Hex Lord Malacrass with
-- `UPDATE creature SET guid=89357 WHERE guid=313406;`. Neither number means anything on this realm:
--
--   * 144991-144998 held 8 live map-1 spawns. This file's own
--     `DELETE FROM creature WHERE guid BETWEEN @CGUID+00 AND @CGUID+07` removed them silently, and
--     the 8 Zul'Aman companions were inserted on top - guids 144990 and 144999 either side are
--     still map 1, which is how the squat was spotted. @CGUID is deliberately NOT re-based: the
--     damage is already done, the 8 companion rows and spawn_group 368-375 are live and correct at
--     144991-144998, and vacating the block now would mean deleting live rows to move them. The
--     lost map-1 spawns still exist in the wc_world reference DB and can be restored separately -
--     do that to a FREE block, not to 144991.
--
--   * guid 89357 is a live map-0 creature (id 2620) and 313406 is a live map-1464 spawn (id 89699),
--     so `UPDATE creature SET guid=89357 WHERE guid=313406` failed on a duplicate key every time
--     this file ran - and had it succeeded it would have dragged an unrelated Dornogal spawn into
--     89357. It is commented out below. Hex Lord Malacrass actually lives at guid 217323 here, so
--     the formation is built on @MALACRASS_GUID; until this fix the 8 companions were formed up
--     behind creature 2620 out in Elwynn Forest.
--
--   * `DELETE FROM creature WHERE guid IN (313405,313407);` is commented out for the same reason:
--     here that is map-1464 guid space, not the old Zul'Aman companion spawns upstream meant.
--
-- Verified before re-pointing the formation: guid 217323 is the only `creature` row carrying id
-- 24239 (Hex Lord Malacrass) - map 568, spawnDifficulties '1,2' - and it appeared in
-- `creature_formations` neither as leaderGUID nor as memberGUID.
--
-- @SPAWN_GROUP_ID (368-375) is unchanged - separate id space, already correct and live.
--
-- Do not revert this on the next merge. If Malacrass is ever re-spawned at a different guid, move
-- @MALACRASS_GUID rather than restoring the upstream 89357.
SET @CGUID := 144991; -- 8 (upstream value kept on purpose - see header)
SET @SPAWN_GROUP_ID := 368; -- 8
SET @MALACRASS_GUID := 217323; -- Hex Lord Malacrass (24239) on this realm (upstream: 89357)

-- Texts
DELETE FROM `creature_text` WHERE `CreatureID` = 24239;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(24239,0,0,"Da shadow gonna fall on you....",14,0,100,0,0,12041,23591,0,"Hex Lord Malacrass SAY_AGGRO"),
(24239,1,0,"Ya don' kill me yet... ya don' get anudda chance!",14,0,100,0,0,12042,23592,0,"Hex Lord Malacrass SAY_BERSERK"),
(24239,2,0,"Dis a nightmare ya don' wake up from!",14,0,100,0,0,12043,23593,0,"Hex Lord Malacrass SAY_SLAY_1"),
(24239,2,1,"Azzaga choogo zinn!",14,0,100,0,0,12044,23594,0,"Hex Lord Malacrass SAY_SLAY_2"),
(24239,3,0,"Your will belong ta me now!",14,0,100,0,0,12045,23595,0,"Hex Lord Malacrass SAY_CHARM"),
(24239,4,0,"Darkness comin' for you....",14,0,100,0,0,12046,23596,0,"Hex Lord Malacrass SAY_DRAIN_POWER"),
(24239,5,0,"Your soul gonna bleed!",14,0,100,0,0,12047,23597,0,"Hex Lord Malacrass SAY_SPIRIT_BOLTS"),
(24239,6,0,"It not gonna make no difference.",14,0,100,0,0,12048,23598,0,"Hex Lord Malacrass SAY_PET_DIES_1"),
(24239,6,1,"You gonna die worse dan him.",14,0,100,0,0,12049,23599,0,"Hex Lord Malacrass SAY_PET_DIES_2"),
(24239,6,2,"Dat no bodda me.",14,0,100,0,0,12050,23600,0,"Hex Lord Malacrass SAY_PET_DIES_3"),
(24239,7,0,"Dis not... da end for me!",14,0,100,0,0,12051,23601,0,"Hex Lord Malacrass SAY_DEATH");

-- Spell scripts
DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
'spell_malacrass_siphon_soul',
'spell_malacrass_mind_control',
'spell_malacrass_mark_of_blood',
'spell_malacrass_blood_worms_selector',
'spell_malacrass_leap_of_faith_selector');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(43498, 'spell_malacrass_siphon_soul'),
(43550, 'spell_malacrass_mind_control'),
(61606, 'spell_malacrass_mark_of_blood'),
(97628, 'spell_malacrass_blood_worms_selector'),
(97640, 'spell_malacrass_leap_of_faith_selector');

UPDATE `spell_script_names` SET `ScriptName` = 'spell_malacrass_unstable_affliction' WHERE `ScriptName` = 'spell_hexlord_unstable_affliction';

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 13 AND `SourceEntry` = 97630;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(13,1,97630,0,0,31,0,3,24239,0,0,0,0,"","Group 0: Spell 'Blood Worms' (Effect 0) targets creature 'Hex Lord Malacrass'");

UPDATE `creature_template_addon` SET `PvpFlags`=0 WHERE `entry`=24363;
UPDATE `creature_template_difficulty` SET `LevelScalingDeltaMin`=2,`LevelScalingDeltaMax`=2,`ContentTuningID`=1112,`DamageModifier`=35,`LootID`=24239,`GoldMin`=419816,`GoldMax`=513109,`StaticFlags1`=524288 WHERE `Entry`=24239 AND `DifficultyID`=2;
-- Upstream: UPDATE `creature` SET `guid`=89357 WHERE `guid`=313406;
-- Dropped - 89357 and 313406 are both live unrelated spawns here; see header. Local Malacrass guid
-- is @MALACRASS_GUID.

UPDATE `creature_template` SET `ScriptName`='npc_malacrass_blood_worm' WHERE `entry`=52827;
UPDATE `creature_template_difficulty` SET `ContentTuningID`=1112, `VerifiedBuild`=68453 WHERE `Entry`=52827 AND `DifficultyID`=2; -- 52827 (Blood Worm)

-- Companions
UPDATE `creature_template` SET `ScriptName`='boss_alyson_antille' WHERE `entry`=24240;
UPDATE `creature_template` SET `ScriptName`='boss_thurg' WHERE `entry`=24241;
UPDATE `creature_template` SET `ScriptName`='boss_slither' WHERE `entry`=24242;
UPDATE `creature_template` SET `ScriptName`='boss_lord_raadan' WHERE `entry`=24243;
UPDATE `creature_template` SET `ScriptName`='boss_gazakroth' WHERE `entry`=24244;
UPDATE `creature_template` SET `ScriptName`='boss_fenstalker' WHERE `entry`=24245;
UPDATE `creature_template` SET `ScriptName`='boss_darkheart' WHERE `entry`=24246;
UPDATE `creature_template` SET `ScriptName`='boss_koragg' WHERE `entry`=24247;
UPDATE `creature_template_difficulty` SET `ContentTuningID`=1112,`DamageModifier`=7.5,`StaticFlags1`=524288 WHERE `Entry` IN (24240,24241,24242,24243,24244,24245,24246,24247) AND `DifficultyID`=2;

-- Upstream: DELETE FROM `creature` WHERE `guid` IN (313405,313407);
-- Dropped - on this realm that is map-1464 guid space, not the stale Zul'Aman companion spawns
-- upstream meant to clear. See header.
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+7;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseId`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`MovementType`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(@CGUID+0,24240,568,0,0,'2',0,0,1,125.57986,923.1528,33.97253,1.588249564170837402,259200,0,0,0,'',NULL,12340),
(@CGUID+1,24241,568,0,0,'2',0,0,1,125.57986,923.1528,33.97253,1.588249564170837402,259200,0,0,0,'',NULL,12340),
(@CGUID+2,24242,568,0,0,'2',0,0,0,125.57986,923.1528,33.97253,1.588249564170837402,259200,0,0,0,'',NULL,12340),
(@CGUID+3,24243,568,0,0,'2',0,0,0,125.57986,923.1528,33.97253,1.588249564170837402,259200,0,0,0,'',NULL,12340),
(@CGUID+4,24244,568,0,0,'2',0,0,0,109.30903,923.05035,33.972557,1.553343057632446289,259200,0,0,0,'',NULL,12340),
(@CGUID+5,24245,568,0,0,'2',0,0,0,109.30903,923.05035,33.972557,1.553343057632446289,259200,0,0,0,'',NULL,12340),
(@CGUID+6,24246,568,0,0,'2',0,0,0,109.30903,923.05035,33.972557,1.553343057632446289,259200,0,0,0,'',NULL,12340),
(@CGUID+7,24247,568,0,0,'2',0,0,0,109.30903,923.05035,33.972557,1.553343057632446289,259200,0,0,0,'',NULL,12340);

DELETE FROM `spawn_group` WHERE `spawnId` BETWEEN @CGUID+0 AND @CGUID+7 AND `spawnType` = 0;
INSERT INTO `spawn_group` (`groupId`, `spawnType`, `spawnId`) VALUES
(@SPAWN_GROUP_ID+0,0,@CGUID+0),
(@SPAWN_GROUP_ID+1,0,@CGUID+1),
(@SPAWN_GROUP_ID+2,0,@CGUID+2),
(@SPAWN_GROUP_ID+3,0,@CGUID+3),
(@SPAWN_GROUP_ID+4,0,@CGUID+4),
(@SPAWN_GROUP_ID+5,0,@CGUID+5),
(@SPAWN_GROUP_ID+6,0,@CGUID+6),
(@SPAWN_GROUP_ID+7,0,@CGUID+7);

DELETE FROM `spawn_group_template` WHERE `groupId` BETWEEN @SPAWN_GROUP_ID+0 AND @SPAWN_GROUP_ID+7;
INSERT INTO `spawn_group_template` (`groupId`, `groupName`, `groupFlags`) VALUES
(@SPAWN_GROUP_ID+0,"Zul'Aman - Alyson Antille",4),
(@SPAWN_GROUP_ID+1,"Zul'Aman - Thurg",4),
(@SPAWN_GROUP_ID+2,"Zul'Aman - Slither",4),
(@SPAWN_GROUP_ID+3,"Zul'Aman - Lord Raadan",4),
(@SPAWN_GROUP_ID+4,"Zul'Aman - Gazakroth",4),
(@SPAWN_GROUP_ID+5,"Zul'Aman - Fenstalker",4),
(@SPAWN_GROUP_ID+6,"Zul'Aman - Darkheart",4),
(@SPAWN_GROUP_ID+7,"Zul'Aman - Koragg",4);

-- The first statement is the upstream one, kept only to undo the damage an earlier run of this file
-- did: guid 89357 is a live map-0 creature here (id 2620), and it was left leading a formation of
-- eight Zul'Aman companions. The second keeps the file idempotent at the real local boss guid.
DELETE FROM `creature_formations` WHERE `leaderGUID` = 89357;
DELETE FROM `creature_formations` WHERE `leaderGUID` = @MALACRASS_GUID;
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@MALACRASS_GUID,@MALACRASS_GUID,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+0,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+1,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+2,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+3,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+4,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+5,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+6,0,0,3,0,0),
(@MALACRASS_GUID,@CGUID+7,0,0,3,0,0);

DELETE FROM `areatrigger_template` WHERE `Id`=3382 AND `IsCustom`=0;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `VerifiedBuild`) VALUES
(3382, 0, 68453);

DELETE FROM `areatrigger_create_properties` WHERE `Id`=407 AND `IsCustom`=0;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`, `ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `TimeToTargetScale`, `Speed`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`, `ShapeData6`, `ShapeData7`, `ScriptName`, `VerifiedBuild`) VALUES
(407, 0, 3382, 0, 0, 0, 0, 0, 0, -1, 0, 0, NULL, 30000, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 'at_malacrass_hex_sphere', 68453); -- Spell: 127737 (Hex Sphere)
