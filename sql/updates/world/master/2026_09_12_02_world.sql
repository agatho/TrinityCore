--
-- Midnight delves: retail run flow data for The Gulf of Memory (2964), The Shadow Enclave (2952), The Darkway (3003)
-- ======================================================================================================================
--
-- Every value below is read off the five captures analysed in C:\sniff\tcharvest\out\delve_research\REPORT.md
-- (gulf = gulfofmemorydelve 12.1.0.69497, eversong = eversongwoodsandshadowedelve 12.1.0.69497, deatholme =
-- alliance_deatholme_delve 12.0.1.66562, shadowmoon = dump_12.0.7.68974_shadowmoon_delve). Positions are the
-- SMSG_UPDATE_OBJECT create-block position quads of the cached frames (delve_research/_cache/*.pkl): GameObjects at
-- movement-block offset 7, creatures/vehicles self-validated against the 2952 roster already in the world DB
-- (guids 13000000+, every shared row agrees to < 0.001 yd). Map 2952 is byte-identical between 12.0.1 and 12.1.
-- Anything not measured is marked REVIEW.
--

-- ----------------------------------------------------------------------------------------------------------------------
-- 1) instance_template - without these rows no DelveInstanceScript binds (Map.cpp uses instance_template.script)
--    (REPORT 6.0 / 6.5, work item 1). Difficulty 208 is the LFGDungeons difficulty of all three maps (ws 5029).
-- ----------------------------------------------------------------------------------------------------------------------
DELETE FROM `instance_template` WHERE `map` IN (2952, 2964, 3003);
INSERT INTO `instance_template` (`map`, `parent`, `script`, `insideResurrection`) VALUES
(2952, 0, 'instance_shadow_enclave_delve',  0),  -- The Shadow Enclave, scenario 3154 (scenarios row (2952,208,3154))
(2964, 0, 'instance_gulf_of_memory_delve',  0),  -- The Gulf of Memory,  scenario 3177 (scenarios row (2964,208,3177))
(3003, 0, 'instance_the_darkway_delve',     0);  -- The Darkway,         scenario 3184 (scenarios row (3003,208,3184))

-- ----------------------------------------------------------------------------------------------------------------------
-- 2) Script bindings
-- ----------------------------------------------------------------------------------------------------------------------
-- Companion: 248567 "Valeera Sanguinar" is summoned by the PLAYER-cast spell 1247560 (REPORT 3.1) and driven by
-- npc_delve_companion (follow / stealth 1252003 / observed kit). Replaces the never-implemented 'npc_valeera_companion'.
UPDATE `creature_template` SET `ScriptName` = 'npc_delve_companion' WHERE `entry` = 248567;
-- Mul'tha'ul, DungeonEncounter 3359 (gulf SMSG_ENCOUNTER_START 969466 / SMSG_BOSS_KILL 1062320)
UPDATE `creature_template` SET `ScriptName` = 'boss_multhaul' WHERE `entry` = 250939;
-- The Darkway's "Enter Delve" is a different template (251896, shadowmoon CMSG_TIERED_ENTRANCE_OPEN 104364)
UPDATE `creature_template` SET `ScriptName` = 'npc_delve_entrance' WHERE `entry` IN (212407, 251896);
-- Leave-O-Bot 7000: spell-click 411497 -> GameEvent 91282 -> DelveMgr::LeaveDelve (REPORT 5.6); the name the DB
-- already referenced and the core did not implement until now
UPDATE `creature_template` SET `ScriptName` = 'npc_leave_o_bot' WHERE `entry` = 205496;
-- Heavy Trunks (REPORT 2.3: 584517 -> currency 3316 +10 + curio; 584519 -> GameEvent 88888 -> Leave-O-Bot)
UPDATE `gameobject_template` SET `ScriptName` = 'go_heavy_trunk' WHERE `entry` IN (584517, 584519);
-- Mislaid Curiosity -> PlayerChoice 822 "Discovered Treasure" + currency 3253 (REPORT 2.4)
UPDATE `gameobject_template` SET `ScriptName` = 'go_mislaid_curiosity' WHERE `entry` = 584752;
-- "Leave Delve" GO -> GameEvent 93004 -> DelveMgr::LeaveDelve (REPORT 5.6, eversong 2650933)
UPDATE `gameobject_template` SET `ScriptName` = 'go_leave_delve' WHERE `entry` = 408227;

-- Curio items: CMSG_USE_ITEM -> SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI(ItemID) (REPORT 3.2: gulf 1096575 271132,
-- eversong 2688539 249219, deatholme 75171 249222)
DELETE FROM `item_script_names` WHERE `ScriptName` = 'item_delve_curio';
INSERT INTO `item_script_names` (`Id`, `ScriptName`) VALUES
(271132, 'item_delve_curio'),
(249219, 'item_delve_curio'),
(249222, 'item_delve_curio');

-- ----------------------------------------------------------------------------------------------------------------------
-- 3) PlayerChoice 822 "Discovered Treasure" - the delve power (REPORT 2.4, eversong 2121802, byte-exact decode)
--    The stored 12.0.1 response ("Banshee Wail", ChoiceArtFileId 2492256, deatholme 155033) is replaced by the 12.1
--    witness. Only one power was ever offered per choice; the pool/roll rule is not observable, so one response.
--    Wire values without a column: BorderUiTextureAtlasMemberID 11555 (playerchoice_response_maw_power has none),
--    ExpireTime ~23 min ahead -> Duration 1380 s (approximate, REVIEW).
-- ----------------------------------------------------------------------------------------------------------------------
UPDATE `playerchoice` SET `UiTextureKitId` = 5493, `SoundKitId` = 213311, `CloseSoundKitId` = 0, `Duration` = 1380,
    `PendingChoiceText` = 'Discovered Treasure', `Question` = 'You\'ve found some treasure!',
    `HideWarboardHeader` = 0, `KeepOpenAfterChoice` = 0, `ShowChoicesAsList` = 0, `RequiresSelection` = 0,
    `ShowChoicesAsGrid` = 0, `HideAnswerArt` = 0, `ShowChoicesAsColumns` = 0, `InfiniteRange` = 0,
    `ScriptName` = 'playerchoice_delve_discovered_treasure', `VerifiedBuild` = 69497
WHERE `ChoiceId` = 822;

DELETE FROM `playerchoice_response` WHERE `ChoiceId` = 822;
INSERT INTO `playerchoice_response` (`ChoiceId`, `ResponseId`, `Index`, `ChoiceArtFileId`, `Flags`, `WidgetSetID`, `UiTextureAtlasElementID`, `SoundKitID`, `GroupID`, `UiTextureKitID`, `Answer`, `Header`, `SubHeader`, `ButtonTooltip`, `Description`, `Confirmation`, `RewardQuestID`, `VerifiedBuild`) VALUES
(822, 0, 0, 136182, 0, 0, 0, 0, 0, 0, '', 'Stomach Turner', '', '', 'Occasionally inflict Stomach Turner on enemies in combat, inflicting Nature damage over time and causing them to vomit out an aggressive slime.', '', 0, 69497);

DELETE FROM `playerchoice_response_maw_power` WHERE `ChoiceId` = 822;
INSERT INTO `playerchoice_response_maw_power` (`ChoiceId`, `ResponseId`, `TypeArtFileID`, `Rarity`, `SpellID`, `MaxStacks`, `VerifiedBuild`) VALUES
(822, 0, 0, 3, 1305224, 10, 69497);   -- "Stomach Turner", aura 42 PROC_TRIGGER_SPELL

-- ----------------------------------------------------------------------------------------------------------------------
-- 4) Spawns. GUID range 13100001+ (the 2952 roster import used 13000000-13000199).
--    spawnDifficulties = 208 as the existing delve rows use; zoneId/areaId = the SMSG_INIT_WORLD_STATES area of each
--    map (REPORT 1.6: 2964 -> 16595, 2952 -> 16594, 3003 -> 16642).
-- ----------------------------------------------------------------------------------------------------------------------

-- 4a) The captured companion position on 2952 is a SUMMON arrival, not a spawn point (REPORT 3.1: player-cast
--     1247560 creates her; she walks). A static Valeera would double the companion.
DELETE FROM `creature` WHERE `guid` = 13000058 AND `id` = 248567 AND `map` = 2952;

DELETE FROM `creature` WHERE `guid` BETWEEN 13100001 AND 13100011;
INSERT INTO `creature`
(`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,
 `terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,
 `spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`VerifiedBuild`) VALUES
-- The Darkway entrance (REPORT 6.5: "entrance 251896 not spawned"). shadowmoon create 84249 on map 0; 8 yd from the
-- template's exit coordinates (9174.82, -4437.6), which corroborates the decode.
(13100001, 251896, 0,    0,     0,     '0',   0, 0, 0, -1, 0, 0, 9179.904, -4439.127,  10.320, 6.0891, 180, 0, 0, 100, 0, 68974),
-- The Gulf of Memory (2964) standard kit - gulf 102719 (Delvers' Supplies x2, Abandoned Restoration Stone) / 102157 (Generic Bunny)
(13100002, 207283, 2964, 16595, 16595, '208', 0, 0, 0, -1, 0, 0,  -18.049,   515.969, 199.534, 2.8514, 180, 0, 0, 100, 0, 69497),
(13100003, 207283, 2964, 16595, 16595, '208', 0, 0, 0, -1, 0, 0,  149.436,   646.500, 186.945, 3.6993, 180, 0, 0, 100, 0, 69497),
(13100004, 209780, 2964, 16595, 16595, '208', 0, 0, 0, -1, 0, 0,  -13.363,   516.493, 199.474, 4.6974, 180, 0, 0, 100, 0, 69497),
(13100005, 221379, 2964, 16595, 16595, '208', 0, 0, 0, -1, 0, 0,  183.962,   649.219, 196.544, 0.0000, 180, 0, 0, 100, 0, 69497),
-- The Darkway (3003) standard kit - shadowmoon 113493 / 113517 / 113448
(13100006, 207283, 3003, 16642, 16642, '208', 0, 0, 0, -1, 0, 0, 3200.024,  4795.432, 602.374, 5.9975, 180, 0, 0, 100, 0, 68974),
(13100007, 207283, 3003, 16642, 16642, '208', 0, 0, 0, -1, 0, 0, 3541.513,  4791.605, 590.350, 5.5311, 180, 0, 0, 100, 0, 68974),
(13100008, 209780, 3003, 16642, 16642, '208', 0, 0, 0, -1, 0, 0, 3203.468,  4793.760, 602.396, 1.0418, 180, 0, 0, 100, 0, 68974),
(13100009, 221379, 3003, 16642, 16642, '208', 0, 0, 0, -1, 0, 0, 3556.854,  4839.401, 589.219, 0.0000, 180, 0, 0, 100, 0, 68974),
-- The Darkway objectives seen in the capture: 252595 "Technician Mireille" (step 16133, created with the kit at 113517)
-- and 252102 "Voidbreaker Oglok" (step 16102 kill target, created 184265 when the player approached - REVIEW whether
-- his spawn is step-gated; the capture cannot tell)
(13100010, 252595, 3003, 16642, 16642, '208', 0, 0, 0, -1, 0, 0, 3544.146,  4799.223, 590.350, 3.0345, 180, 0, 0, 100, 0, 68974),
(13100011, 252102, 3003, 16642, 16642, '208', 0, 0, 0, -1, 0, 0, 3236.527,  4804.478, 602.388, 6.2696, 180, 0, 0, 100, 0, 68974);
-- NOT spawned statically: 250939 Mul'tha'ul (created 373 ms after scenario step 16082 begins, gulf 865087 - summoned by
-- instance_gulf_of_memory_delve), the Heavy Trunks / Fragment / Leave-O-Bot (spawned on completion by
-- DelveInstanceScript), the Mislaid Curiosities of 2964/3003 (personal summons of player spell 1259272 x11 at entry,
-- gulf 102678 - the 2952 import already carries ten static ones, kept).

DELETE FROM `gameobject` WHERE `guid` BETWEEN 13100001 AND 13100007;
INSERT INTO `gameobject`
(`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,
 `position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,
 `spawntimesecs`,`animprogress`,`state`,`VerifiedBuild`) VALUES
-- The Gulf of Memory (2964): "Leave Delve" 408227 + map-name GO 612259 at entry (gulf 102157); the two Delve
-- Campfires 618844 appeared at 490389 / 894832 (checkpoints reached on retail - spawned unconditionally here, REVIEW)
(13100001, 408227, 2964, 16595, 16595, '208', 0, 0, 0, -1,  183.951,  649.278, 196.465, 4.8221, 0, 0, SIN(4.8221/2), COS(4.8221/2), 180, 255, 1, 69497),
(13100002, 612259, 2964, 16595, 16595, '208', 0, 0, 0, -1,  128.830,  655.104, 193.516, 4.5169, 0, 0, SIN(4.5169/2), COS(4.5169/2), 180, 255, 1, 69497),
(13100003, 618844, 2964, 16595, 16595, '208', 0, 0, 0, -1,   81.010,  746.187, 188.015, 0.8036, 0, 0, SIN(0.8036/2), COS(0.8036/2), 180, 255, 1, 69497),
(13100004, 618844, 2964, 16595, 16595, '208', 0, 0, 0, -1,  -21.319,  510.011, 199.480, 3.1655, 0, 0, SIN(3.1655/2), COS(3.1655/2), 180, 255, 1, 69497),
-- The Shadow Enclave (2952): the map-name GO 611933 was missing from the roster import (eversong 1847045)
(13100005, 611933, 2952, 16594, 16594, '208', 0, 0, 0, -1,  -48.905,  168.403, 257.520, 0.7754, 0, 0, SIN(0.7754/2), COS(0.7754/2), 180, 255, 1, 69497),
-- The Darkway (3003): "Leave Delve" (shadowmoon 113120) and the campfire that appeared at 404376 (checkpoint, REVIEW)
(13100006, 408227, 3003, 16642, 16642, '208', 0, 0, 0, -1, 3557.017, 4837.307, 590.610, 0.0000, 0, 0, SIN(0.0000/2), COS(0.0000/2), 180, 255, 1, 68974),
(13100007, 618844, 3003, 16642, 16642, '208', 0, 0, 0, -1, 3366.362, 4830.813, 587.788, 5.8890, 0, 0, SIN(5.8890/2), COS(5.8890/2), 180, 255, 1, 68974);
-- The Darkway's map-name GO was not created in the shadowmoon capture (census.txt) - nothing to place.

-- ----------------------------------------------------------------------------------------------------------------------
-- NOT COVERED - reported instead of guessed
-- ----------------------------------------------------------------------------------------------------------------------
--  * The Darkway final boss entry: DungeonEncounter 3361 "Infiltrator Gulkat" has three creature_template candidates
--    (251015, 251600, 256817); the capture ends before him. instance_the_darkway_delve accepts any of the three,
--    delve_template.finalBossEntry stays 0, and no Gulkat spawn is placed.
--  * Reward object placements for 3003 (Heavy Trunks, Leave-O-Bot): unobserved; the instance script falls back to
--    positions around the boss corpse.
--  * The Gulf's full creature/gameobject roster (39 creature + 27 gameobject entries, census.txt) - the same import
--    the 2952 roster got; not part of this file.
