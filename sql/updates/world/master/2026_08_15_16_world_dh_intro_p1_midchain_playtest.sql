-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- DH intro P1 — mid-chain playtest fixes (Beliash + cages + early trash loot)
-- Evidence: retail sniff temp/retail-sniff/dump_12.0.7.68887_2026-08-02_17-09-06 dhi-horde1_parsed.txt
--           CREATE 93221 @ 1590.7916, 2713.3325, 19.31737 O=5.56466; credit 106003 on death
--           Spire Stabilizer GO 244601/244604 CREATE at Seat of Command; goober spells Data10
--           Jailer Cage 242987/242989/242990/244916 re-use after free (autoClose=1, consumable=0)
--           Early trash loot items from temp/retail-sniff/dh-intro/loot.txt

SET @CGUID := 11800197;
SET @OGUID := 11800012;

-- Task 1: Doom Commander Beliash — clear bogus KillCredit1 (95226 Anguish Jailer); explicit 106003 via ScriptName
UPDATE `creature_template` SET `KillCredit1`=0, `ScriptName`='npc_mardum_doom_commander_beliash' WHERE `entry`=93221;

DELETE FROM `creature` WHERE `guid`=@CGUID+0;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(@CGUID+0, 93221, 1481, 7705, 7742, '0', 0, 0, 0, 1590.7916, 2713.3325, 19.31737, 5.56466, 120, 0, 0, 100, 0, '', NULL, 68887);

-- Task 1: Spire Stabilizers (fight helpers; Data10 already has deactivate spells; sniff CREATE coords)
DELETE FROM `gameobject` WHERE `guid` BETWEEN @OGUID+0 AND @OGUID+1;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `VerifiedBuild`) VALUES
(@OGUID+0, 244601, 1481, 7705, 7742, '0', 0, 0, 1577.0469, 2718.5217, 19.882166, 0, 0, 0, 0, 1, 120, 255, 1, 68887),
(@OGUID+1, 244604, 1481, 7705, 7742, '0', 0, 0, 1606.717, 2690.2283, 20.276869, 0, 0, 0, 0, 1, 120, 255, 1, 68887);

-- Task 2: Jailer Cages — one-shot after free (ScriptName; includes Belath 242989 sibling of handoff trio)
UPDATE `gameobject_template` SET `ScriptName`='go_mardum_jailer_cage' WHERE `entry` IN (242987, 242989, 242990, 244916);

-- Task 3: Early Invasion trash loot (sniff item pool; LootID = entry)
UPDATE `creature_template_difficulty` SET `LootID`=98486 WHERE `entry`=98486;
UPDATE `creature_template_difficulty` SET `LootID`=98484 WHERE `entry`=98484;
UPDATE `creature_template_difficulty` SET `LootID`=98482 WHERE `entry`=98482;
UPDATE `creature_template_difficulty` SET `LootID`=98483 WHERE `entry`=98483;
UPDATE `creature_template_difficulty` SET `LootID`=98497 WHERE `entry`=98497;

DELETE FROM `creature_loot_template` WHERE `Entry` IN (98486, 98484, 98482, 98483, 98497);
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
-- Mo'arg Brutalizer 98486
(98486, 0, 130268, 20, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98486, 0, 130267, 35, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98486, 0, 132753, 25, 0, 1, 1, 1, 2, 'Shard - Invasion trash'),
(98486, 0, 129196, 10, 0, 1, 1, 1, 1, 'Junk - Invasion trash'),
-- Felguard Invader 98484
(98484, 0, 130268, 20, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98484, 0, 130267, 35, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98484, 0, 132753, 25, 0, 1, 1, 1, 2, 'Shard - Invasion trash'),
-- Wrath-Lord Lekos trash peers
(98482, 0, 130264, 30, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98482, 0, 130267, 20, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98483, 0, 130264, 30, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98497, 0, 130267, 30, 0, 1, 0, 1, 1, 'Green Item - Invasion trash'),
(98497, 0, 132753, 20, 0, 1, 1, 1, 2, 'Shard - Invasion trash');
