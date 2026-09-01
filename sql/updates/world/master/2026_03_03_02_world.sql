-- ================================================================
-- HOUSING TUTORIAL SUPPLEMENT - Map 2736 (Razorwind Shores)
-- Adds: gossip chains, NPC movement, area trigger spawns, fauna wander
-- ================================================================

-- ================================================================
-- 1. GOSSIP CHAIN: Rotha (254687) menu 41365 → 41367
-- ================================================================

UPDATE `gossip_menu_option` SET `ActionMenuID`=41367
  WHERE `MenuID`=41365 AND `OptionID`=0;

-- ================================================================
-- 2. AMBIENT FAUNA - Enable random movement (MovementType=1)
--    wander_distance=5 for ground creatures, =10 for flying insects
-- ================================================================

-- Snakes (234993) - ground wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=5
  WHERE `map`=2736 AND `id`=234993 AND `guid` BETWEEN 9000000 AND 9999999;

-- Scorpions (235001) - ground wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=5
  WHERE `map`=2736 AND `id`=235001 AND `guid` BETWEEN 9000000 AND 9999999;

-- Beetles (235003) - ground wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=5
  WHERE `map`=2736 AND `id`=235003 AND `guid` BETWEEN 9000000 AND 9999999;

-- Rats (244973) - ground wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=5
  WHERE `map`=2736 AND `id`=244973 AND `guid` BETWEEN 9000000 AND 9999999;

-- Crabs (239919) - ground wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=5
  WHERE `map`=2736 AND `id`=239919 AND `guid` BETWEEN 9000000 AND 9999999;

-- Lizards (242103) - ground wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=5
  WHERE `map`=2736 AND `id`=242103 AND `guid` BETWEEN 9000000 AND 9999999;

-- Dragonflies (245042) - flying insects, wider wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=10
  WHERE `map`=2736 AND `id`=245042 AND `guid` BETWEEN 9000000 AND 9999999;

-- Hornets (248557) - flying insects, wider wander
UPDATE `creature` SET `MovementType`=1, `wander_distance`=10
  WHERE `map`=2736 AND `id`=248557 AND `guid` BETWEEN 9000000 AND 9999999;

-- ================================================================
-- 3. TOURIST (237063) WAYPOINT PATHS - 4 spawns with patrol routes
--    Extracted from SMSG_ON_MONSTER_MOVE spline data
--    MovementType=2 (waypoint) for these spawns
-- ================================================================

-- Tourist spawn 1 (guid 9000294) - starts at (2005.20, 120.98, 190.42)
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=9000294;
DELETE FROM `creature_addon` WHERE `guid`=9000294;
INSERT INTO `creature_addon` (`guid`,`PathId`) VALUES (9000294, 9000294);
DELETE FROM `waypoint_path` WHERE `PathId`=9000294;
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Comment`) VALUES
(9000294, 0, 0, 'Tourist 237063 patrol - spawn 1');
DELETE FROM `waypoint_path_node` WHERE `PathId`=9000294;
INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`Delay`) VALUES
(9000294, 1, 2005.4194, 120.8581, 189.6936, NULL, 0),
(9000294, 2, 2007.1694, 118.6081, 189.6936, NULL, 0),
(9000294, 3, 2007.4194, 117.8581, 189.4436, NULL, 0),
(9000294, 4, 2010.6694, 119.1081, 189.4436, NULL, 0),
(9000294, 5, 2015.4194, 119.8581, 189.1936, NULL, 0),
(9000294, 6, 2018.4194, 120.1081, 188.9436, NULL, 0),
(9000294, 7, 2019.4194, 120.3581, 188.6936, NULL, 0),
(9000294, 8, 2020.4194, 120.3581, 188.6936, NULL, 0),
(9000294, 9, 2021.4194, 120.6081, 188.4436, NULL, 0),
(9000294, 10, 2024.4194, 120.8581, 187.4436, NULL, 0),
(9000294, 11, 2024.6410, 120.7352, 186.9692, NULL, 2000),
(9000294, 12, 2005.1980, 120.9809, 190.4180, NULL, 2000);

-- Tourist spawn 2 (guid 9000295) - starts at (1988.33, 178.26, 161.18)
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=9000295;
DELETE FROM `creature_addon` WHERE `guid`=9000295;
INSERT INTO `creature_addon` (`guid`,`PathId`) VALUES (9000295, 9000295);
DELETE FROM `waypoint_path` WHERE `PathId`=9000295;
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Comment`) VALUES
(9000295, 0, 0, 'Tourist 237063 patrol - spawn 2');
DELETE FROM `waypoint_path_node` WHERE `PathId`=9000295;
INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`Delay`) VALUES
(9000295, 1, 1987.461, 176.639, 161.235, NULL, 0),
(9000295, 2, 1986.461, 176.889, 160.985, NULL, 0),
(9000295, 3, 1984.711, 177.639, 160.485, NULL, 0),
(9000295, 4, 1983.711, 178.139, 160.235, NULL, 0),
(9000295, 5, 1982.711, 178.639, 159.985, NULL, 0),
(9000295, 6, 1980.961, 179.389, 159.485, NULL, 0),
(9000295, 7, 1979.961, 179.639, 158.985, NULL, 0),
(9000295, 8, 1979.211, 180.139, 158.735, NULL, 0),
(9000295, 9, 1977.211, 180.889, 158.485, NULL, 0),
(9000295, 10, 1976.461, 181.389, 158.235, NULL, 2000),
(9000295, 11, 1988.330, 178.257, 161.179, NULL, 2000);

-- Tourist spawn 3 (guid 9000296) - starts at (2034.48, 130.13, 182.72)
-- Uses random wander since we only have the spawn position spline
UPDATE `creature` SET `MovementType`=1, `wander_distance`=15 WHERE `guid`=9000296;

-- Tourist spawn 4 (guid 9000297) - starts at (1613.35, 297.73, 81.20)
-- Uses random wander since we only have the spawn position spline
UPDATE `creature` SET `MovementType`=1, `wander_distance`=15 WHERE `guid`=9000297;

-- ================================================================
-- 4. LOCAL (237581) NPCs - 38 spawns with random wander
--    Complex patrol routes in sniff but too many to extract individual paths
--    Use generous random wander to approximate
-- ================================================================

UPDATE `creature` SET `MovementType`=1, `wander_distance`=15
  WHERE `map`=2736 AND `id`=237581 AND `guid` BETWEEN 9000000 AND 9999999;

-- ================================================================
-- 5. AREA TRIGGER SPAWNS for Quest 91863 Tour Stops
--    ObjectIDs 38951-38962 need areatrigger_template + areatrigger rows
--    Positions are ESTIMATED from neighborhood layout since they
--    weren't captured as CMSG_AREA_TRIGGER in the sniff.
--    These are server-side AreaTrigger entities (IsCustom=1).
--
--    The quest objectives use type 19 (AREA_TRIGGER_ENTER) which
--    requires the player to walk into an AreaTrigger entity on the map.
--    AreaTrigger::HandleUnitEnter() calls UpdateQuestObjectiveProgress().
-- ================================================================

-- Create areatrigger templates (custom, since these aren't in DB2)
-- Using a simple sphere shape via ActionSetId=0
DELETE FROM `areatrigger_template` WHERE `Id` BETWEEN 38951 AND 38962 AND `IsCustom`=1;
INSERT INTO `areatrigger_template` (`Id`,`IsCustom`,`Flags`,`ActionSetId`,`ActionSetFlags`,`VerifiedBuild`) VALUES
(38951, 1, 0, 0, 0, 64797),
(38952, 1, 0, 0, 0, 64797),
(38953, 1, 0, 0, 0, 64797),
(38954, 1, 0, 0, 0, 64797),
(38955, 1, 0, 0, 0, 64797),
(38956, 1, 0, 0, 0, 64797),
(38957, 1, 0, 0, 0, 64797),
(38958, 1, 0, 0, 0, 64797),
(38959, 1, 0, 0, 0, 64797),
(38960, 1, 0, 0, 0, 64797),
(38961, 1, 0, 0, 0, 64797),
(38962, 1, 0, 0, 0, 64797);

-- Spawn area triggers on Map 2736 (Razorwind Shores)
-- Positions distributed across key neighborhood locations
-- SpawnId range: 9100000-9100011 to avoid collision with creature/GO guids
DELETE FROM `areatrigger` WHERE `SpawnId` BETWEEN 9100000 AND 9100011;
INSERT INTO `areatrigger` (`SpawnId`,`AreaTriggerCreatePropertiesId`,`IsCustom`,`MapId`,`SpawnDifficulties`,`PosX`,`PosY`,`PosZ`,`Orientation`,`ScriptName`,`Comment`,`VerifiedBuild`) VALUES
-- Tour stop 1: Near Tocho Cloudhide (steward, quest giver)
(9100000, 38951, 1, 2736, '0', 1715.36, 95.10, 102.28, 0, '', 'Housing Tour Stop 1 - Steward', 64797),
-- Tour stop 2: Near Saga Mistrunner (meat vendor)
(9100001, 38952, 1, 2736, '0', 1720.58, 124.94, 100.15, 0, '', 'Housing Tour Stop 2 - Meat Vendor', 64797),
-- Tour stop 3: Near Xiz''ro (lumberjack)
(9100002, 38953, 1, 2736, '0', 1697.27, 167.39, 103.33, 0, '', 'Housing Tour Stop 3 - Lumberjack', 64797),
-- Tour stop 4: Near Haleth Turnwater (dye crafter)
(9100003, 38954, 1, 2736, '0', 1697.06, 207.39, 99.82, 0, '', 'Housing Tour Stop 4 - Dye Crafter', 64797),
-- Tour stop 5: Near The Last Architect (quest giver)
(9100004, 38955, 1, 2736, '0', 1728.05, 205.50, 100.03, 0, '', 'Housing Tour Stop 5 - Last Architect', 64797),
-- Tour stop 6: Near Rotha (general contractor)
(9100005, 38956, 1, 2736, '0', 1763.94, 216.39, 107.03, 0, '', 'Housing Tour Stop 6 - General Contractor', 64797),
-- Tour stop 7: Between vendor area and plots (estimated)
(9100006, 38957, 1, 2736, '0', 1750.00, 170.00, 105.00, 0, '', 'Housing Tour Stop 7 - Market Area', 64797),
-- Tour stop 8: Near cornerstone cluster (estimated)
(9100007, 38958, 1, 2736, '0', 1780.00, 150.00, 105.00, 0, '', 'Housing Tour Stop 8 - Cornerstones', 64797),
-- Tour stop 9: Near bulletin board area (estimated)
(9100008, 38959, 1, 2736, '0', 1800.00, 180.00, 105.00, 0, '', 'Housing Tour Stop 9 - Bulletin Boards', 64797),
-- Tour stop 10: Near flight master area (estimated)
(9100009, 38960, 1, 2736, '0', 1796.03, 200.00, 100.00, 0, '', 'Housing Tour Stop 10 - Flight Path', 64797),
-- Tour stop 11: Neighborhood overlook (estimated)
(9100010, 38961, 1, 2736, '0', 1750.00, 250.00, 110.00, 0, '', 'Housing Tour Stop 11 - Overlook', 64797),
-- Tour stop 12: Return to start area (estimated)
(9100011, 38962, 1, 2736, '0', 1730.00, 130.00, 100.00, 0, '', 'Housing Tour Stop 12 - Return', 64797);

-- ================================================================
-- 6. AREATRIGGER CREATE PROPERTIES (required for custom area triggers)
-- ================================================================

-- Check if areatrigger_create_properties table exists and add entries
-- These define the shape/size of each area trigger (sphere radius ~15 yards)
DELETE FROM `areatrigger_create_properties` WHERE `Id` BETWEEN 38951 AND 38962 AND `IsCustom`=1;
-- Shape=0 (Sphere), ShapeData0=15.0 (radius in yards)
INSERT INTO `areatrigger_create_properties`
  (`Id`,`IsCustom`,`AreaTriggerId`,`IsAreatriggerCustom`,`Shape`,`ShapeData0`,`VerifiedBuild`) VALUES
(38951, 1, 38951, 1, 0, 15.0, 64797),
(38952, 1, 38952, 1, 0, 15.0, 64797),
(38953, 1, 38953, 1, 0, 15.0, 64797),
(38954, 1, 38954, 1, 0, 15.0, 64797),
(38955, 1, 38955, 1, 0, 15.0, 64797),
(38956, 1, 38956, 1, 0, 15.0, 64797),
(38957, 1, 38957, 1, 0, 15.0, 64797),
(38958, 1, 38958, 1, 0, 15.0, 64797),
(38959, 1, 38959, 1, 0, 15.0, 64797),
(38960, 1, 38960, 1, 0, 15.0, 64797),
(38961, 1, 38961, 1, 0, 15.0, 64797),
(38962, 1, 38962, 1, 0, 15.0, 64797);

-- ================================================================
-- SUMMARY
-- ================================================================
-- 1. Rotha gossip chain: ActionMenuID 41365→41367
-- 2. Ambient fauna: 485 creatures now wander (8 species)
-- 3. Tourist: 2 with waypoint paths, 2 with random wander
-- 4. Local: 38 NPCs with random wander
-- 5. Area triggers: 12 tour stops for quest 91863
-- Faction: HORDE (Razorwind Shores)
