-- ============================================================================
-- Arathi Catch-Up / RPE -- Stromgarde siege + Boulderfist climax motion  (2026-08-22)
-- ============================================================================
-- The siege and climax casts were entirely MovementType=0 (frozen). Re-ran motion classification with
-- the CORRECTED test (an earlier per-POI agent rejected every path on small first-to-last-node
-- distance, which is exactly what a route that loops back produces -- see the farm fix). Correct test =
-- node count + CONFINEMENT (total path length / max span): a designed patrol meanders (ratio >= 1.8),
-- a combat chase runs monotonically at the player (ratio ~1).
--
-- Paths were matched to spawns by POSITION (path start within 6 yd of a live spawn) rather than by the
-- rig's sparse path->entry comments, which only cover a fraction of the captured paths.
-- Survivors on phases 28 / 1610 / 3 (both captures searched, best-confinement route kept per spawn):
--     229955 Stromgarde Citizen  x2 (independently confirms the earlier per-POI report)
--     244691 Gnoll Charger       x2
--     244685 Ogre Basher         x2
-- One Citizen route (capture path 2268321) was DISCARDED: an ~4 yd box of micro-jitter, which would
-- read in game as the NPC twitching in place. That spawn gets ambient wander instead.
-- 142334 Plains Creeper already carries ambient wander (MovementType=1) and is left alone.
--
-- Nodes are captured coordinates, sub-sampled to <=12. This is the NEW waypoint system
-- (waypoint_path/_node + creature_addon.PathId + MovementType=2).
-- NOT YET TESTED IN GAME: the tester has not reached the siege or the climax.
-- ============================================================================

DELETE FROM `waypoint_path_node` WHERE `PathId` BETWEEN 9200021 AND 9200025;
DELETE FROM `waypoint_path` WHERE `PathId` BETWEEN 9200021 AND 9200025;
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Velocity`,`Comment`) VALUES
 (9200021, 0, 0, 0, 'Stromgarde Citizen 229955 patrol (capture 2263486)'),
 (9200022, 0, 0, 0, 'Gnoll Charger 244691 patrol (capture 16112853)'),
 (9200023, 0, 0, 0, 'Gnoll Charger 244691 patrol (capture 2237987)'),
 (9200024, 0, 0, 0, 'Ogre Basher 244685 patrol (capture 2239209)'),
 (9200025, 0, 0, 0, 'Ogre Basher 244685 patrol (capture 16113112)');

INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`) VALUES
 (9200021, 1, -1557.6233, -1881.5122, 67.5802),
 (9200021, 2, -1561.8733, -1886.7622, 68.5802),
 (9200021, 3, -1561.1233, -1885.7622, 68.8302),
 (9200021, 4, -1560.8733, -1885.2622, 68.8302),
 (9200021, 5, -1557.3733, -1881.0122, 68.8302),
 (9200021, 6, -1556.6233, -1880.0122, 68.5802),
 (9200021, 7, -1554.8733, -1878.0122, 67.5802),
 (9200021, 8, -1554.6233, -1877.5122, 67.3302),
 (9200021, 9, -1554.1233, -1877.0122, 67.0802),
 (9200021, 10, -1557.6233, -1881.5122, 67.5802),
 (9200022, 1, -1459.2661, -1793.6954, 67.1213),
 (9200022, 2, -1454.3726, -1795.0348, 66.2223),
 (9200022, 3, -1455.3726, -1795.2848, 66.2223),
 (9200022, 4, -1456.3726, -1795.2848, 66.4723),
 (9200022, 5, -1457.3726, -1795.2848, 66.7223),
 (9200022, 6, -1459.1226, -1795.5348, 66.9723),
 (9200022, 7, -1461.1226, -1795.5348, 67.2223),
 (9200022, 8, -1463.1226, -1795.7848, 67.7223),
 (9200022, 9, -1464.1226, -1795.7848, 67.9723),
 (9200022, 10, -1458.4791, -1797.3741, 66.3234),
 (9200023, 1, -1457.1035, -1797.4160, 65.9397),
 (9200023, 2, -1449.8605, -1797.1307, 65.1030),
 (9200023, 3, -1451.8605, -1797.1307, 65.1030),
 (9200023, 4, -1453.6105, -1797.1307, 65.3530),
 (9200023, 5, -1455.6105, -1797.1307, 65.6030),
 (9200023, 6, -1456.6105, -1797.1307, 65.8530),
 (9200023, 7, -1449.1174, -1796.8453, 64.2664),
 (9200024, 1, -1262.1060, -1891.7795, 79.5429),
 (9200024, 2, -1253.1060, -1894.7795, 81.0429),
 (9200024, 3, -1254.8560, -1894.0295, 80.7929),
 (9200024, 4, -1259.6060, -1892.2795, 80.2929),
 (9200024, 5, -1260.6060, -1892.0295, 80.0429),
 (9200024, 6, -1263.1060, -1891.2795, 79.5429),
 (9200024, 7, -1265.1060, -1890.7795, 79.5429),
 (9200024, 8, -1265.8560, -1890.2795, 79.2929),
 (9200024, 9, -1268.8560, -1889.2795, 78.7929),
 (9200024, 10, -1269.8560, -1889.0295, 78.5429),
 (9200024, 11, -1271.6060, -1888.2795, 78.0429),
 (9200024, 12, -1273.6060, -1887.7795, 77.7929),
 (9200025, 1, -1368.1476, -1847.2517, 63.1481),
 (9200025, 2, -1364.5916, -1847.7627, 63.3938),
 (9200025, 3, -1372.5916, -1846.2627, 63.3938),
 (9200025, 4, -1373.0916, -1846.2627, 63.3938),
 (9200025, 5, -1373.5916, -1846.2627, 63.3938),
 (9200025, 6, -1374.0916, -1846.0127, 63.3938),
 (9200025, 7, -1368.0356, -1847.2737, 63.1394);

-- Seat each mover on its route and switch it to waypoint movement.
UPDATE `creature` SET `position_x`=-1557.6233, `position_y`=-1881.5122, `position_z`=67.5802, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000065;  -- Stromgarde Citizen
UPDATE `creature` SET `position_x`=-1459.2661, `position_y`=-1793.6954, `position_z`=67.1213, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000123;  -- Gnoll Charger
UPDATE `creature` SET `position_x`=-1457.1035, `position_y`=-1797.4160, `position_z`=65.9397, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000124;  -- Gnoll Charger
UPDATE `creature` SET `position_x`=-1262.1060, `position_y`=-1891.7795, `position_z`=79.5429, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000142;  -- Ogre Basher
UPDATE `creature` SET `position_x`=-1368.1476, `position_y`=-1847.2517, `position_z`=63.1481, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000181;  -- Ogre Basher

DELETE FROM `creature_addon` WHERE `guid` IN (8000065,8000123,8000124,8000142,8000181);
INSERT INTO `creature_addon` (`guid`,`PathId`) VALUES
 (8000065, 9200021), (8000123, 9200022), (8000124, 9200023), (8000142, 9200024), (8000181, 9200025)
 ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- Every other spawn of these three entries gets ambient wander sized to the observed movement span,
-- so the siege/climax reads as populated rather than frozen (same treatment the farm raiders got).
UPDATE `creature` SET `MovementType`=1, `wander_distance`=8  WHERE `map`=2927 AND `id`=229955 AND `MovementType`<>2;  -- citizens milling in town
UPDATE `creature` SET `MovementType`=1, `wander_distance`=6  WHERE `map`=2927 AND `id`=244691 AND `MovementType`<>2;  -- gnoll chargers
UPDATE `creature` SET `MovementType`=1, `wander_distance`=10 WHERE `map`=2927 AND `id`=244685 AND `MovementType`<>2;  -- ogre bashers
