-- ============================================================================
-- Arathi Catch-Up / RPE -- Go'shek Farm raider PATROLS  (2026-08-22)
-- ============================================================================
-- REVISION of the earlier "wander_distance=4" polish. Tester (Horde char, time spent at the farm):
-- "there have to be patrol moves". Correct -- the per-POI agent's "no patrol, only combat chases"
-- verdict was WRONG: it rejected paths on a small first-vs-last-node distance, but a patrol that loops
-- back has a small first-last distance BY DEFINITION. Re-reading the Horde capture's movement splines
-- (waypoint_paths_movement) the farm raiders each carry a genuine MULTI-NODE route (8-17 nodes confined
-- to a 15-37yd area = wander/patrol, not a 2-3 node straight-line chase), mapped via the smart_scripts
-- WP_START comments:
--   Ogre Destroyer   244674 -> path 2225890 (17 nodes, ~21yd)
--   Kobold Pillager  244676 -> path 2220764 (10 nodes, ~37yd arc)
--   Kobold Firetender 244677 -> path 2219909 (8 nodes, ~15yd)
-- Reproduce each as a real waypoint patrol on ONE representative spawn per entry (seated on the route),
-- with this build's NEW waypoint system (creature.MovementType=2 + creature_addon.PathId ->
-- waypoint_path/_node). The other same-entry spawns keep ambient wander, bumped from the too-small 4 to
-- roughly half the observed span so they visibly move too. Nodes sub-sampled to <=12, captured coords.
-- ============================================================================

-- ---- Patrol routes (captured nodes) ----
DELETE FROM `waypoint_path_node` WHERE `PathId` IN (9200010,9200011,9200012);
DELETE FROM `waypoint_path` WHERE `PathId` IN (9200010,9200011,9200012);
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Velocity`,`Comment`) VALUES
 (9200010, 0, 0, 0, 'Go-shek Ogre Destroyer 244674 patrol (capture 2225890)'),
 (9200011, 0, 0, 0, 'Go-shek Kobold Pillager 244676 patrol (capture 2220764)'),
 (9200012, 0, 0, 0, 'Go-shek Kobold Firetender 244677 patrol (capture 2219909)');
INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`) VALUES
 -- Ogre Destroyer 244674
 (9200010, 1, -1417.786, -2982.181, 19.152),
 (9200010, 2, -1416.405, -2982.721, 19.716),
 (9200010, 3, -1419.405, -2980.971, 18.716),
 (9200010, 4, -1422.405, -2980.721, 17.716),
 (9200010, 5, -1425.405, -2980.221, 16.966),
 (9200010, 6, -1428.155, -2979.721, 16.216),
 (9200010, 7, -1429.905, -2979.221, 15.716),
 (9200010, 8, -1430.905, -2979.221, 15.216),
 (9200010, 9, -1432.905, -2978.721, 14.966),
 (9200010, 10, -1433.905, -2978.471, 14.716),
 -- Kobold Pillager 244676
 (9200011, 1, -1496.731, -2857.214, 13.942),
 (9200011, 2, -1498.647, -2857.602, 14.193),
 (9200011, 3, -1493.897, -2856.852, 14.193),
 (9200011, 4, -1488.147, -2859.602, 14.193),
 (9200011, 5, -1488.147, -2866.602, 14.693),
 (9200011, 6, -1488.397, -2868.602, 14.693),
 (9200011, 7, -1488.397, -2871.352, 14.693),
 (9200011, 8, -1488.397, -2874.352, 14.193),
 (9200011, 9, -1488.647, -2879.352, 14.193),
 (9200011, 10, -1491.562, -2882.490, 13.944),
 -- Kobold Firetender 244677
 (9200012, 1, -1469.024, -2936.565, 14.797),
 (9200012, 2, -1467.033, -2937.559, 14.993),
 (9200012, 3, -1460.033, -2932.809, 14.993),
 (9200012, 4, -1459.783, -2932.559, 15.243),
 (9200012, 5, -1459.283, -2932.309, 15.493),
 (9200012, 6, -1457.542, -2933.553, 15.190);

-- ---- Movers: seat the representative spawn on its route + switch to waypoint movement ----
UPDATE `creature` SET `position_x`=-1417.786, `position_y`=-2982.181, `position_z`=19.152, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000153; -- Ogre (already on start)
UPDATE `creature` SET `position_x`=-1496.731, `position_y`=-2857.214, `position_z`=13.942, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8002001; -- Kobold Pillager (reseated to route)
UPDATE `creature` SET `position_x`=-1469.024, `position_y`=-2936.565, `position_z`=14.797, `MovementType`=2, `wander_distance`=0 WHERE `guid`=8000157; -- Firetender (reseated to route)

DELETE FROM `creature_addon` WHERE `guid` IN (8000153,8002001,8000157);
INSERT INTO `creature_addon` (`guid`,`PathId`) VALUES (8000153,9200010),(8002001,9200011),(8000157,9200012)
  ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- ---- Non-mover same-entry spawns: ambient wander sized to the observed movement span ----
UPDATE `creature` SET `MovementType`=1, `wander_distance`=10 WHERE `map`=2927 AND `id`=244674 AND `MovementType`<>2; -- Ogre span ~21
UPDATE `creature` SET `MovementType`=1, `wander_distance`=10 WHERE `map`=2927 AND `id`=244676 AND `MovementType`<>2; -- Pillager span ~37 (kept moderate for the tight farm)
UPDATE `creature` SET `MovementType`=1, `wander_distance`=7  WHERE `map`=2927 AND `id`=244677 AND `MovementType`<>2; -- Firetender span ~15
