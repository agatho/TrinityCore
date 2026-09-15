-- ========================================================================================
-- CONTENT SLICE -- Waking Shores (map 2927) :: patrol routes recovered from SMSG_ON_MONSTER_MOVE
-- ========================================================================================
-- Generated 2026-09-09 by TCHarvest tools/author_chain_slice.py
-- Sources: rpe_a4_proj: dump_12.1.0.69587_2026-09-09_22-24-58.pkt + TCHarvest.lua + combat log; wpp_alliance; wpp_alliance2; wpp_alliance3; wpp_horde; rpe_h2_proj
--
-- A route is emitted LIVE only when TWO captures reconstructed a path for the same entry starting
-- within 15yd of each other, AND a realm spawn of that entry stands within 8yd of the route's first
-- routes: 7 live (two-capture), 28 commented for review
-- node with MovementType 0/1 (it then becomes MovementType 2 on that path). Single-capture routes
-- are COMMENTED. Wanderers are not touched here (their captured positions are roam snapshots).
-- PathIds are the rig's stable ids for traceability. Velocity from the wire is informational.
-- ========================================================================================


-- [LOW] entry 244671 'Gnoll Ripper' path 8048854: 45 nodes, span 93yd, first node (-1083.2,-3554.4); realm spawn guid 8000022 0.1yd from the route (MovementType 0); single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8048854;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8048854;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8048854, 0, 0, 10.001, 'Gnoll Ripper - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 1, -1083.1636, -3554.4028, 50.5047, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 2, -1080.8829, -3554.1692, 51.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 3, -1078.8829, -3554.1692, 51.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 4, -1075.8829, -3554.1692, 51.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 5, -1073.8829, -3553.9192, 51.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 6, -1071.8829, -3553.9192, 52.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 7, -1069.8829, -3553.9192, 52.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 8, -1068.8829, -3553.9192, 52.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 9, -1067.8829, -3553.9192, 52.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 10, -1066.8829, -3553.9192, 52.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 11, -1066.3829, -3553.9192, 52.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 12, -1065.3829, -3553.6692, 53.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 13, -1064.3829, -3553.4192, 53.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 14, -1063.3829, -3553.1692, 53.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 15, -1062.6329, -3552.9192, 53.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 16, -1061.6329, -3552.6692, 53.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 17, -1060.6329, -3552.4192, 53.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 18, -1059.6329, -3552.1692, 54.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 19, -1058.6329, -3551.9192, 54.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 20, -1058.1329, -3551.9192, 54.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 21, -1057.1329, -3551.9192, 54.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 22, -1055.1329, -3551.9192, 54.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 23, -1053.8829, -3551.9192, 54.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 24, -1049.6329, -3551.9192, 55.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 25, -1045.6329, -3551.9192, 55.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 26, -1041.6329, -3551.9192, 55.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 27, -1039.3829, -3551.9192, 55.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 28, -1037.3829, -3551.9192, 55.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 29, -1033.3829, -3551.9192, 56.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 30, -1031.3829, -3552.4192, 56.4660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 31, -1028.3829, -3552.9192, 56.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 32, -1024.8829, -3553.9192, 56.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 33, -1016.6329, -3553.9192, 56.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 34, -1008.3829, -3549.9192, 56.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 35, -1004.1329, -3549.9192, 56.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 36, -1002.3829, -3550.9192, 57.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 37, -1002.1329, -3550.9192, 57.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 38, -1000.1329, -3550.9192, 57.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 39, -998.1329, -3550.6692, 58.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 40, -996.1329, -3550.1692, 58.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 41, -995.8829, -3549.9192, 58.2160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 42, -992.1329, -3548.1692, 57.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 43, -991.6329, -3547.9192, 57.9660, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 44, -990.8829, -3548.6692, 57.7160, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048854, 45, -990.6023, -3548.9355, 57.4273, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000022;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000022, 8048854) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244672 'Gnoll Bruiser' path 8048855: 44 nodes, span 60yd, first node (-1082.2,-3551.5); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16047989
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8048855;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8048855;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8048855, 0, 0, 7.991, 'Gnoll Bruiser - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 1, -1082.1691, -3551.5017, 50.7565, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 2, -1080.0220, -3551.1537, 51.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 3, -1077.0220, -3550.6537, 51.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 4, -1075.0220, -3550.6537, 51.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 5, -1073.0220, -3550.4037, 52.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 6, -1071.0220, -3550.1537, 52.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 7, -1069.0220, -3549.9037, 52.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 8, -1068.0220, -3549.9037, 53.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 9, -1067.0220, -3549.9037, 53.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 10, -1066.5220, -3549.6537, 53.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 11, -1065.5220, -3549.6537, 53.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 12, -1064.5220, -3549.6537, 53.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 13, -1063.5220, -3549.6537, 53.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 14, -1062.5220, -3549.6537, 53.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 15, -1061.5220, -3549.6537, 54.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 16, -1060.5220, -3549.6537, 54.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 17, -1059.5220, -3549.6537, 54.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 18, -1058.5220, -3549.6537, 54.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 19, -1058.0220, -3549.6537, 54.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 20, -1057.0220, -3549.6537, 54.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 21, -1055.0220, -3549.6537, 55.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 22, -1054.0220, -3549.6537, 55.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 23, -1050.0220, -3547.6537, 55.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 24, -1045.7720, -3547.6537, 55.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 25, -1041.7720, -3547.6537, 55.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 26, -1039.7720, -3547.6537, 55.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 27, -1037.7720, -3547.6537, 55.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 28, -1033.5220, -3547.6537, 56.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 29, -1032.2720, -3544.9037, 56.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 30, -1031.2720, -3543.1537, 56.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 31, -1030.2720, -3540.4037, 56.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 32, -1029.0220, -3537.6537, 57.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 33, -1028.5220, -3536.9037, 57.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 34, -1027.7720, -3535.1537, 57.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 35, -1027.0220, -3533.4037, 57.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 36, -1025.0220, -3531.1537, 57.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 37, -1024.0220, -3529.4037, 57.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 38, -1023.7720, -3528.6537, 57.8573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 39, -1023.2720, -3527.6537, 58.6073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 40, -1022.7720, -3526.6537, 59.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 41, -1022.2720, -3525.9037, 60.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 42, -1021.7720, -3524.9037, 60.1073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 43, -1021.7720, -3523.9037, 60.3573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8048855, 44, -1021.8750, -3523.8057, 60.4580, 0);

-- [LOW] entry 244670 'Gnoll Bowblaster' path 7990316: 41 nodes, span 61yd, first node (-1074.1,-3545.8); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=7990316;
-- DELETE FROM `waypoint_path` WHERE `PathId`=7990316;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (7990316, 0, 0, 6.874, 'Gnoll Bowblaster - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 1, -1074.0702, -3545.8340, 52.0549, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 2, -1048.5312, -3536.9562, 55.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 3, -1050.2812, -3537.7062, 54.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 4, -1052.0312, -3538.7062, 54.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 5, -1053.7812, -3539.7062, 54.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 6, -1059.7812, -3542.7062, 54.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 7, -1060.7812, -3544.9562, 53.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 8, -1060.7812, -3546.9562, 53.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 9, -1062.5312, -3549.2062, 53.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 10, -1063.7812, -3550.9562, 53.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 11, -1064.2812, -3551.9562, 52.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 12, -1064.2812, -3552.9562, 52.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 13, -1064.7812, -3553.9562, 52.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 14, -1066.0312, -3554.4562, 52.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 15, -1067.0312, -3554.4562, 52.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 16, -1068.0312, -3554.4562, 52.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 17, -1069.0312, -3554.4562, 51.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 18, -1070.0312, -3554.4562, 51.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 19, -1071.0312, -3554.4562, 51.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 20, -1072.2812, -3554.9562, 51.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 21, -1076.2812, -3557.2062, 51.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 22, -1080.2812, -3557.2062, 51.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 23, -1083.2812, -3557.2062, 51.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 24, -1084.5312, -3557.2062, 51.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 25, -1087.2812, -3556.4562, 51.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 26, -1089.2812, -3555.9562, 51.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 27, -1090.2812, -3555.7062, 50.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 28, -1091.2812, -3555.4562, 50.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 29, -1092.2812, -3555.2062, 50.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 30, -1092.7812, -3554.9562, 50.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 31, -1094.7812, -3554.9562, 50.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 32, -1095.7812, -3554.9562, 49.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 33, -1096.7812, -3554.9562, 49.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 34, -1098.7812, -3554.9562, 49.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 35, -1100.7812, -3554.9562, 49.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 36, -1101.0312, -3554.9562, 49.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 37, -1103.7812, -3556.4562, 48.6028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 38, -1105.5312, -3557.2062, 48.3528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 39, -1107.2812, -3558.2062, 48.1028, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 40, -1109.5312, -3559.2062, 47.8528, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990316, 41, -1083.4923, -3550.0784, 50.6507, 0);

-- [HIGH] entry 254547 'Stromgarde Footman' path 8252148: 35 nodes, span 63yd, first node (-1432.6,-1800.4); realm spawn guid 8000204 7.1yd from the route (MovementType 0); also seen in wpp_alliance3:6260476
DELETE FROM `waypoint_path_node` WHERE `PathId`=8252148;
DELETE FROM `waypoint_path` WHERE `PathId`=8252148;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8252148, 0, 0, 3.499, 'Stromgarde Footman - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 1, -1432.6097, -1800.4067, 61.6922, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 2, -1417.6221, -1805.9470, 60.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 3, -1419.3721, -1805.1970, 61.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 4, -1422.1221, -1804.1970, 61.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 5, -1424.1221, -1803.4470, 61.6747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 6, -1425.8721, -1802.6970, 61.6747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 7, -1428.6221, -1801.6970, 61.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 8, -1433.3721, -1800.1970, 62.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 9, -1438.1221, -1798.1970, 62.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 10, -1440.8721, -1797.1970, 62.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 11, -1442.6221, -1796.4470, 62.6747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 12, -1444.6221, -1795.6970, 62.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 13, -1446.3721, -1795.1970, 62.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 14, -1448.8721, -1793.9470, 63.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 15, -1449.6221, -1793.9470, 63.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 16, -1450.6221, -1793.9470, 63.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 17, -1452.6221, -1793.9470, 63.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 18, -1454.6221, -1793.9470, 64.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 19, -1456.6221, -1794.1970, 64.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 20, -1459.6221, -1794.1970, 64.6747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 21, -1461.6221, -1794.1970, 64.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 22, -1463.6221, -1794.4470, 65.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 23, -1464.6221, -1794.4470, 65.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 24, -1465.6221, -1794.4470, 65.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 25, -1466.6221, -1794.4470, 65.6747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 26, -1467.6221, -1794.4470, 65.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 27, -1468.6221, -1794.4470, 66.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 28, -1470.6221, -1794.6970, 66.6747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 29, -1472.6221, -1794.6970, 67.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 30, -1474.6221, -1794.6970, 67.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 31, -1476.6221, -1794.6970, 67.9247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 32, -1477.6221, -1794.9470, 68.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 33, -1478.6221, -1794.9470, 68.1747, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 34, -1480.6221, -1794.9470, 68.4247, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8252148, 35, -1464.1345, -1801.4873, 67.1572, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000204;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000204, 8252148) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [LOW] entry 142334 'Plains Creeper' path 8116145: 28 nodes, span 34yd, first node (-1283.3,-3400.3); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8116145;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8116145;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8116145, 0, 0, 2.516, 'Plains Creeper - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 1, -1283.2858, -3400.3123, 42.0829, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 2, -1282.2012, -3398.1881, 41.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 3, -1281.7012, -3397.4381, 41.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 4, -1280.7012, -3395.6881, 41.4446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 5, -1279.9512, -3393.6881, 41.1946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 6, -1278.9512, -3391.9381, 40.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 7, -1278.7012, -3391.1881, 40.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 8, -1277.7012, -3389.1881, 40.1946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 9, -1276.9512, -3387.4381, 39.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 10, -1276.4512, -3386.6881, 39.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 11, -1275.9512, -3385.6881, 39.4446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 12, -1275.4512, -3384.9381, 39.1946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 13, -1274.9512, -3383.9381, 38.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 14, -1274.9512, -3383.1881, 38.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 15, -1273.9512, -3381.4381, 38.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 16, -1273.4512, -3380.6881, 38.4446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 17, -1273.2012, -3379.6881, 38.1946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 18, -1272.7012, -3378.6881, 37.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 19, -1272.2012, -3377.9381, 37.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 20, -1271.7012, -3376.9381, 37.4446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 21, -1270.9512, -3375.1881, 37.1946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 22, -1270.4512, -3374.4381, 36.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 23, -1269.9512, -3373.4381, 36.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 24, -1269.4512, -3372.4381, 36.4446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 25, -1269.2012, -3371.6881, 36.4446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 26, -1267.7012, -3368.9381, 35.9446, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 27, -1266.9512, -3367.1881, 35.6946, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8116145, 28, -1266.6166, -3366.5640, 35.3063, 0);

-- [LOW] entry 244695 'Ettin Crusher' path 8210385: 28 nodes, span 73yd, first node (-1394.0,-1800.2); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8210385;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8210385;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8210385, 0, 0, 14.431, 'Ettin Crusher - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 1, -1393.9565, -1800.1532, 60.2926, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 2, -1382.4557, -1812.8381, 60.7851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 3, -1381.4557, -1814.5881, 61.0351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 4, -1380.4557, -1816.3381, 61.0351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 5, -1379.2057, -1818.0881, 61.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 6, -1378.2057, -1819.5881, 61.5351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 7, -1377.2057, -1821.3381, 61.7851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 8, -1376.4557, -1822.3381, 62.0351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 9, -1374.4557, -1824.3381, 62.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 10, -1372.9557, -1825.8381, 62.5351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 11, -1372.2057, -1826.5881, 62.7851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 12, -1370.9557, -1827.8381, 63.0351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 13, -1370.2057, -1828.5881, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 14, -1368.7057, -1830.0881, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 15, -1368.2057, -1830.5881, 63.5351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 16, -1366.9557, -1833.8381, 63.5351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 17, -1365.7057, -1835.0881, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 18, -1364.7057, -1835.8381, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 19, -1362.7057, -1837.3381, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 20, -1356.2057, -1843.0881, 63.5351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 21, -1355.7057, -1843.5881, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 22, -1353.9557, -1845.3381, 63.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 23, -1352.9557, -1846.0881, 63.0351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 24, -1345.9557, -1855.8381, 62.7851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 25, -1345.2057, -1856.5881, 62.5351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 26, -1343.4557, -1859.0881, 62.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 27, -1342.4557, -1860.5881, 62.2851, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210385, 28, -1331.4548, -1873.0231, 61.7777, 0);

-- [HIGH] entry 249249 'Hammerfall Peon' path 8152602: 28 nodes, span 44yd, first node (-1517.8,-3123.6); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16098179, wpp_alliance2:20038169, wpp_alliance3:6118446, wpp_horde:2230354, rpe_h2_proj:10141549
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8152602;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8152602;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8152602, 0, 0, 10.194, 'Hammerfall Peon - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 1, -1517.7736, -3123.6180, 27.1573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 2, -1522.8951, -3113.6461, 26.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 3, -1523.1451, -3114.3961, 26.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 4, -1524.8951, -3117.8961, 26.4192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 5, -1528.3951, -3124.8961, 26.4192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 6, -1526.8951, -3126.1461, 26.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 7, -1524.3951, -3128.1461, 26.9192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 8, -1522.1451, -3129.8961, 27.1692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 9, -1520.3951, -3131.1461, 27.4192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 10, -1518.1451, -3132.6461, 27.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 11, -1515.8951, -3134.6461, 27.9192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 12, -1514.1451, -3135.8961, 28.1692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 13, -1513.3951, -3136.3961, 28.1692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 14, -1511.6451, -3137.3961, 28.4192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 15, -1509.8951, -3138.3961, 28.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 16, -1507.3951, -3139.8961, 28.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 17, -1505.6451, -3140.8961, 28.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 18, -1503.8951, -3141.6461, 28.9192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 19, -1501.3951, -3143.1461, 29.1692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 20, -1499.6451, -3144.1461, 29.4192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 21, -1497.8951, -3145.1461, 29.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 22, -1495.3951, -3146.6461, 29.9192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 23, -1493.6451, -3147.6461, 30.1692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 24, -1490.8951, -3149.1461, 30.1692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 25, -1489.1451, -3150.1461, 30.4192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 26, -1487.3951, -3150.8961, 30.6692, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 27, -1484.8951, -3152.3961, 30.9192, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8152602, 28, -1488.0167, -3140.1743, 30.1811, 0);

-- [HIGH] entry 244685 'Ogre Basher' path 8184533: 24 nodes, span 53yd, first node (-1314.9,-1851.8); realm spawn guid 8000142 0.0yd from the route (MovementType 2); also seen in wpp_alliance:16125060, wpp_alliance3:6186749, wpp_horde:2239556
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8184533;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8184533;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8184533, 0, 0, 5.901, 'Ogre Basher - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 1, -1314.8889, -1851.8385, 63.4401, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 2, -1298.9974, -1865.8090, 68.2415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 3, -1298.2474, -1866.3090, 68.7415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 4, -1297.7474, -1866.8090, 68.7415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 5, -1296.9974, -1867.3090, 68.9915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 6, -1295.9974, -1867.8090, 69.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 7, -1295.2474, -1868.0590, 69.7415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 8, -1294.2474, -1868.5590, 70.2415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 9, -1292.4974, -1869.5590, 70.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 10, -1291.7474, -1870.0590, 70.7415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 11, -1290.7474, -1870.5590, 71.2415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 12, -1289.9974, -1871.0590, 71.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 13, -1288.9974, -1871.5590, 71.7415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 14, -1287.7474, -1872.3090, 72.2415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 15, -1286.7474, -1872.8090, 72.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 16, -1285.9974, -1873.3090, 72.9915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 17, -1284.2474, -1874.3090, 73.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 18, -1282.4974, -1875.3090, 73.7415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 19, -1281.4974, -1875.8090, 74.2415, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 20, -1280.7474, -1876.3090, 74.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 21, -1279.7474, -1876.8090, 74.9915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 22, -1278.9974, -1877.3090, 74.9915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 23, -1277.2474, -1878.3090, 75.4915, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8184533, 24, -1262.1060, -1891.7795, 79.5429, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000142;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000142, 8184533) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 142339 'Highland Thrasher' path 8161600: 23 nodes, span 30yd, first node (-1526.9,-2833.8); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance2:20016130, wpp_horde:2231245, rpe_h2_proj:10151803
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8161600;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8161600;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8161600, 0, 0, 4.409, 'Highland Thrasher - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 1, -1526.9219, -2833.8066, 31.0612, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 2, -1515.3779, -2826.5703, 35.1404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 3, -1516.3779, -2827.0703, 34.8904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 4, -1517.3779, -2827.5703, 34.3904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 5, -1519.1279, -2828.3203, 34.1404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 6, -1519.8779, -2828.8203, 33.6404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 7, -1521.8779, -2829.5703, 33.3904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 8, -1523.6279, -2830.5703, 32.8904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 9, -1525.3779, -2831.3203, 32.6404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 10, -1526.3779, -2831.8203, 32.1404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 11, -1527.1279, -2832.3203, 31.8904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 12, -1528.1279, -2832.5703, 31.6404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 13, -1529.1279, -2833.0703, 31.6404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 14, -1530.6279, -2833.5703, 31.1404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 15, -1531.3779, -2834.0703, 30.8904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 16, -1532.3779, -2834.5703, 30.1404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 17, -1533.3779, -2834.8203, 30.1404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 18, -1535.1279, -2835.8203, 29.3904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 19, -1536.8779, -2836.5703, 28.8904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 20, -1539.6279, -2837.8203, 28.6404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 21, -1543.3779, -2839.5703, 28.3904, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 22, -1545.1279, -2840.5703, 28.6404, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161600, 23, -1533.3340, -2833.3340, 32.2195, 0);

-- [HIGH] entry 245052 'Horde Grunt' path 7990938: 21 nodes, span 24yd, first node (-1038.2,-3544.8); realm spawn guid 8001104 0.2yd from the route (MovementType 0); also seen in wpp_alliance2:19997577
DELETE FROM `waypoint_path_node` WHERE `PathId`=7990938;
DELETE FROM `waypoint_path` WHERE `PathId`=7990938;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (7990938, 0, 0, 9.996, 'Horde Grunt - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 1, -1038.1605, -3544.7861, 55.5974, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 2, -1037.1285, -3543.9197, 56.4514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 3, -1036.3785, -3543.4197, 56.7014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 4, -1034.6285, -3542.1697, 56.9514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 5, -1033.1285, -3540.9197, 57.4514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 6, -1030.6285, -3539.1697, 57.4514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 7, -1029.1285, -3536.6697, 57.7014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 8, -1027.6285, -3534.1697, 57.9514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 9, -1027.1285, -3533.1697, 57.9514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 10, -1024.8785, -3531.1697, 57.9514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 11, -1024.1285, -3529.4197, 57.7014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 12, -1023.6285, -3528.6697, 58.2014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 13, -1023.1285, -3527.6697, 58.7014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 14, -1022.8785, -3526.6697, 59.4514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 15, -1022.3785, -3525.9197, 59.9514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 16, -1021.8785, -3524.9197, 59.9514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 17, -1021.8785, -3523.9197, 60.4514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 18, -1021.8785, -3522.9197, 60.7014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 19, -1020.8785, -3521.1697, 61.2014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 20, -1020.8785, -3520.9197, 61.2014, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7990938, 21, -1020.0966, -3520.5532, 61.3054, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001104;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001104, 7990938) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 142333 'Giant Plains Creeper' path 8135215: 20 nodes, span 34yd, first node (-1467.2,-2799.5); NO realm spawn of this entry within 8yd of the route; also seen in rpe_h2_proj:10154350
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8135215;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8135215;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8135215, 0, 0, 2.651, 'Giant Plains Creeper - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 1, -1467.1578, -2799.5144, 47.1813, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 2, -1465.9208, -2801.3060, 47.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 3, -1465.1708, -2803.0560, 47.3432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 4, -1464.1708, -2806.0560, 47.0932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 5, -1463.1708, -2808.8060, 46.8432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 6, -1462.1708, -2811.5560, 46.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 7, -1461.4208, -2813.5560, 46.3432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 8, -1461.1708, -2814.5560, 46.0932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 9, -1460.6708, -2816.3060, 45.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 10, -1460.4208, -2817.0560, 45.3432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 11, -1460.1708, -2818.0560, 45.0932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 12, -1459.4208, -2819.8060, 44.3432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 13, -1458.6708, -2821.8060, 44.0932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 14, -1458.1708, -2823.5560, 43.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 15, -1457.6708, -2824.5560, 43.3432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 16, -1457.1708, -2826.5560, 42.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 17, -1456.6708, -2827.3060, 42.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 18, -1456.1708, -2829.3060, 41.8432, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 19, -1455.4208, -2831.0560, 41.5932, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8135215, 20, -1454.1837, -2833.5977, 40.5050, 0);

-- [LOW] entry 244676 'Kobold Pillager' path 8133364: 20 nodes, span 22yd, first node (-1503.2,-2971.8); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8133364;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8133364;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8133364, 0, 0, 5.925, 'Kobold Pillager - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 1, -1503.2069, -2971.7656, 13.9398, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 2, -1504.8344, -2978.2518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 3, -1506.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 4, -1507.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 5, -1508.5844, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 6, -1510.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 7, -1511.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 8, -1512.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 9, -1513.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 10, -1514.8344, -2978.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 11, -1515.8344, -2978.2518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 12, -1517.0844, -2977.2518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 13, -1519.0844, -2975.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 14, -1520.0844, -2975.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 15, -1521.0844, -2975.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 16, -1522.0844, -2975.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 17, -1523.3344, -2975.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 18, -1524.3344, -2975.7518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 19, -1525.3344, -2975.2518, 14.1897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133364, 20, -1513.4619, -2978.2380, 13.9397, 0);

-- [HIGH] entry 244691 'Gnoll Charger' path 8190598: 20 nodes, span 25yd, first node (-1447.1,-1794.9); realm spawn guid 8000123 0.1yd from the route (MovementType 2); also seen in wpp_alliance:16112716, wpp_alliance2:20042877, wpp_alliance3:6182186, wpp_horde:2237952, rpe_h2_proj:10182604
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8190598;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8190598;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8190598, 0, 0, 16.449, 'Gnoll Charger - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 1, -1447.0770, -1794.9167, 63.8825, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 2, -1441.1012, -1796.2940, 63.2479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 3, -1443.1012, -1795.7940, 63.7479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 4, -1444.1012, -1795.5440, 63.7479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 5, -1446.1012, -1795.2940, 64.2479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 6, -1448.1012, -1795.0440, 64.4979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 7, -1449.8512, -1794.5440, 64.7479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 8, -1451.8512, -1794.2940, 64.9979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 9, -1453.6012, -1794.0440, 65.4979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 10, -1454.6012, -1794.0440, 65.4979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 11, -1457.6012, -1793.5440, 65.7479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 12, -1458.6012, -1793.2940, 65.9979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 13, -1459.6012, -1793.0440, 66.2479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 14, -1460.6012, -1792.7940, 66.7479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 15, -1461.6012, -1792.7940, 66.9979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 16, -1462.6012, -1792.5440, 67.2479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 17, -1464.6012, -1792.2940, 67.7479, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 18, -1465.3512, -1792.0440, 67.9979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 19, -1466.3512, -1791.7940, 68.4979, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8190598, 20, -1459.1254, -1793.6711, 67.1133, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000123;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000123, 8190598) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 142340 'Highland Fleshstalker' path 8161496: 19 nodes, span 22yd, first node (-1696.4,-2829.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_horde:2231315
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8161496;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8161496;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8161496, 0, 0, 2.502, 'Highland Fleshstalker - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 1, -1696.3533, -2829.3257, 42.7436, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 2, -1697.7549, -2830.6473, 42.0044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 3, -1698.5049, -2831.3973, 41.5044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 4, -1700.0049, -2832.8973, 41.0044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 5, -1700.5049, -2833.6473, 40.7544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 6, -1702.0049, -2834.8973, 40.2544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 7, -1702.7549, -2835.6473, 40.0044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 8, -1704.2549, -2837.1473, 39.7544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 9, -1704.7549, -2837.8973, 39.5044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 10, -1706.2549, -2839.1473, 39.2544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 11, -1707.0049, -2839.8973, 39.0044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 12, -1708.2549, -2841.1473, 38.7544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 13, -1708.7549, -2841.6473, 38.5044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 14, -1709.5049, -2842.3973, 38.2544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 15, -1711.0049, -2843.8973, 38.0044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 16, -1713.0049, -2845.8973, 37.7544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 17, -1715.2549, -2848.1473, 37.5044, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 18, -1717.2549, -2850.1473, 37.2544, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8161496, 19, -1718.6565, -2851.4690, 36.7652, 0);

-- [HIGH] entry 142338 'Highland Strider' path 8094133: 18 nodes, span 18yd, first node (-982.0,-3381.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance3:6058095
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8094133;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8094133;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8094133, 0, 0, 2.711, 'Highland Strider - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 1, -982.0059, -3381.3047, 63.1960, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 2, -979.3303, -3379.4753, 63.9889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 3, -978.5803, -3378.7253, 64.2389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 4, -977.0803, -3377.4753, 64.7389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 5, -976.5803, -3376.7253, 64.9889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 6, -975.0803, -3375.2253, 65.2389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 7, -974.3303, -3374.4753, 65.4889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 8, -973.5803, -3373.9753, 65.4889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 9, -973.0803, -3373.2253, 65.7389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 10, -972.5803, -3372.7253, 65.9889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 11, -971.8303, -3371.9753, 65.7389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 12, -971.0803, -3371.2253, 66.2389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 13, -969.8303, -3369.7253, 66.7389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 14, -968.3303, -3368.4753, 66.9889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 15, -967.5803, -3367.7253, 67.2389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 16, -966.3303, -3366.2253, 67.7389, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 17, -965.5803, -3365.4753, 67.9889, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8094133, 18, -964.1547, -3365.1460, 67.7819, 0);

-- [HIGH] entry 230001 'Stromgarde Orphan' path 8173401: 17 nodes, span 22yd, first node (-1621.8,-1885.4); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16196808, wpp_alliance2:20044584, wpp_alliance3:6147421, wpp_horde:2233103, rpe_h2_proj:10177908
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8173401;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8173401;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8173401, 0, 0, 9.431, 'Stromgarde Orphan - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 1, -1621.7935, -1885.4149, 80.7624, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 2, -1622.9575, -1881.4297, 81.2275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 3, -1620.9575, -1887.1797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 4, -1619.9575, -1889.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 5, -1620.7075, -1889.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 6, -1620.7075, -1890.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 7, -1620.7075, -1891.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 8, -1620.7075, -1892.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 9, -1620.7075, -1893.4297, 80.4775, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 10, -1620.7075, -1894.6797, 80.4775, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 11, -1620.7075, -1895.6797, 80.4775, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 12, -1620.7075, -1896.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 13, -1620.7075, -1897.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 14, -1620.7075, -1898.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 15, -1620.7075, -1899.6797, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 16, -1621.2075, -1903.9297, 80.7275, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8173401, 17, -1622.1216, -1901.4445, 80.6925, 0);

-- [LOW] entry 149555 'Abomination' path 7996234: 16 nodes, span 23yd, first node (-1052.2,-3553.9); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=7996234;
-- DELETE FROM `waypoint_path` WHERE `PathId`=7996234;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (7996234, 0, 0, 26.696, 'Abomination - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 1, -1052.2160, -3553.8796, 54.9024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 2, -1056.5710, -3552.8235, 54.7258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 3, -1058.5710, -3552.5735, 54.4758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 4, -1059.5710, -3552.5735, 54.4758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 5, -1060.5710, -3552.5735, 54.2258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 6, -1061.5710, -3552.3235, 53.9758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 7, -1062.5710, -3552.3235, 53.7258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 8, -1063.5710, -3552.3235, 53.4758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 9, -1064.3210, -3552.3235, 53.4758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 10, -1066.3210, -3552.0735, 53.2258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 11, -1067.3210, -3551.8235, 53.2258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 12, -1068.3210, -3551.8235, 52.9758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 13, -1069.3210, -3551.5735, 52.7258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 14, -1071.3210, -3551.3235, 52.4758, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 15, -1073.3210, -3551.0735, 52.2258, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996234, 16, -1075.4261, -3550.7673, 51.5492, 0);

-- [HIGH] entry 244674 'Ogre Destroyer' path 8122223: 16 nodes, span 17yd, first node (-1417.8,-2982.2); realm spawn guid 8000153 0.0yd from the route (MovementType 2); also seen in wpp_alliance:16071648, wpp_alliance2:20014874, wpp_alliance3:6086990, wpp_horde:2225890, rpe_h2_proj:10125682
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8122223;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8122223;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8122223, 0, 0, 2.474, 'Ogre Destroyer - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 1, -1417.7865, -2982.1807, 19.1525, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 2, -1416.9054, -2982.4706, 19.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 3, -1418.6554, -2981.4706, 19.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 4, -1418.9054, -2981.2206, 18.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 5, -1418.9054, -2981.2206, 18.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 6, -1419.9054, -2980.9706, 18.4664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 7, -1421.9054, -2980.7206, 17.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 8, -1424.9054, -2980.4706, 16.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 9, -1427.6554, -2979.9706, 16.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 10, -1428.6554, -2979.7206, 15.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 11, -1429.6554, -2979.4706, 15.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 12, -1430.6554, -2979.2206, 15.4664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 13, -1431.6554, -2979.2206, 15.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 14, -1432.6554, -2978.9706, 15.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 15, -1433.4054, -2978.7206, 14.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122223, 16, -1434.0243, -2979.2605, 14.7803, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000153;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000153, 8122223) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [LOW] entry 237409 'Lesser Ghoul' path 7996225: 15 nodes, span 23yd, first node (-1049.0,-3552.9); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=7996225;
-- DELETE FROM `waypoint_path` WHERE `PathId`=7996225;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (7996225, 0, 0, 30.204, 'Lesser Ghoul - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 1, -1049.0187, -3552.8657, 54.9295, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 2, -1055.9502, -3551.6127, 54.9640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 3, -1057.9502, -3551.3627, 54.4640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 4, -1058.9502, -3551.1127, 54.4640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 5, -1059.9502, -3551.1127, 54.2140, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 6, -1060.7002, -3551.1127, 53.9640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 7, -1061.7002, -3550.8627, 53.7140, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 8, -1062.7002, -3550.8627, 53.7140, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 9, -1063.7002, -3550.6127, 53.4640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 10, -1064.7002, -3550.6127, 53.4640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 11, -1065.4502, -3550.3627, 53.4640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 12, -1066.4502, -3550.1127, 53.2140, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 13, -1067.4502, -3550.1127, 52.9640, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 14, -1070.4502, -3549.6127, 52.7140, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (7996225, 15, -1072.3817, -3549.3596, 51.9984, 0);

-- [HIGH] entry 244669 'Scavenging Hyena' path 8053635: 15 nodes, span 23yd, first node (-1016.5,-3519.9); realm spawn guid 8000026 0.6yd from the route (MovementType 0); also seen in rpe_h2_proj:10057108
DELETE FROM `waypoint_path_node` WHERE `PathId`=8053635;
DELETE FROM `waypoint_path` WHERE `PathId`=8053635;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8053635, 0, 0, 9.022, 'Scavenging Hyena - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 1, -1016.4872, -3519.9087, 61.3993, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 2, -1017.6959, -3521.3552, 61.4589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 3, -1018.9459, -3523.1052, 60.7089, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 4, -1020.1959, -3524.6052, 60.4589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 5, -1020.9459, -3525.3552, 59.9589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 6, -1021.4459, -3526.1052, 59.2089, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 7, -1022.6959, -3527.6052, 58.9589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 8, -1023.4459, -3528.6052, 58.2089, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 9, -1023.9459, -3529.3552, 57.7089, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 10, -1025.6959, -3531.3552, 57.9589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 11, -1028.6959, -3535.3552, 57.7089, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 12, -1030.6959, -3537.6052, 57.4589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 13, -1033.1959, -3540.6052, 57.2089, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 14, -1034.4459, -3542.3552, 56.9589, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8053635, 15, -1034.9045, -3542.8018, 56.5184, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000026;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000026, 8053635) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244690 'Stromgarde Footman' path 8210621: 15 nodes, span 28yd, first node (-1299.4,-1668.8); realm spawn guid 8000076 2.1yd from the route (MovementType 0); also seen in rpe_h2_proj:10193043
DELETE FROM `waypoint_path_node` WHERE `PathId`=8210621;
DELETE FROM `waypoint_path` WHERE `PathId`=8210621;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8210621, 0, 0, 9.973, 'Stromgarde Footman - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 1, -1299.4419, -1668.7621, 53.2599, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 2, -1302.5227, -1668.4201, 53.6631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 3, -1307.5227, -1668.4201, 53.6631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 4, -1308.5227, -1668.4201, 53.4131, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 5, -1309.5227, -1668.4201, 53.1631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 6, -1310.5227, -1668.4201, 53.1631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 7, -1312.5227, -1668.4201, 52.9131, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 8, -1313.5227, -1668.4201, 52.6631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 9, -1315.2727, -1668.4201, 52.4131, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 10, -1319.2727, -1668.1701, 52.1631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 11, -1323.2727, -1668.1701, 52.4131, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 12, -1324.2727, -1668.1701, 52.6631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 13, -1326.2727, -1668.1701, 53.1631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 14, -1327.2727, -1668.1701, 53.1631, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8210621, 15, -1327.6035, -1668.0781, 53.0663, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000076;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000076, 8210621) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 2620 'Prairie Dog' path 8206981: 14 nodes, span 20yd, first node (-1218.6,-1693.8); realm spawn guid 8001232 0.8yd from the route (MovementType 1); also seen in wpp_horde:2247182
DELETE FROM `waypoint_path_node` WHERE `PathId`=8206981;
DELETE FROM `waypoint_path` WHERE `PathId`=8206981;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8206981, 0, 0, 2.531, 'Prairie Dog - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 1, -1218.6494, -1693.7678, 55.0327, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 2, -1217.3644, -1693.9835, 55.0553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 3, -1215.6144, -1694.7335, 54.8053, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 4, -1213.6144, -1695.4835, 54.3053, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 5, -1212.6144, -1695.7335, 54.0553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 6, -1210.8644, -1696.4835, 53.5553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 7, -1209.8644, -1696.7335, 53.3053, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 8, -1208.3644, -1697.2335, 52.8053, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 9, -1206.3644, -1697.7335, 52.8053, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 10, -1204.3644, -1698.4835, 52.0553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 11, -1202.6144, -1698.9835, 51.8053, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 12, -1200.6144, -1699.7335, 51.0553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 13, -1198.8644, -1700.4835, 51.0553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8206981, 14, -1198.5793, -1700.6991, 50.5778, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001232;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001232, 8206981) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [LOW] entry 143622 'Wild Imp' path 8032720: 13 nodes, span 18yd, first node (-1077.5,-3549.8); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8032720;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8032720;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8032720, 0, 0, 16.759, 'Wild Imp - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 1, -1077.4921, -3549.7630, 51.3637, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 2, -1075.1381, -3549.2947, 52.0073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 3, -1073.1381, -3549.2947, 52.2573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 4, -1071.1381, -3549.2947, 52.5073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 5, -1069.1381, -3549.2947, 52.7573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 6, -1068.3881, -3549.2947, 52.7573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 7, -1067.3881, -3549.2947, 53.0073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 8, -1066.6381, -3549.2947, 53.2573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 9, -1065.6381, -3549.0447, 53.2573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 10, -1064.6381, -3549.0447, 53.5073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 11, -1063.6381, -3549.0447, 53.7573, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 12, -1061.6381, -3549.0447, 54.0073, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8032720, 13, -1059.7840, -3548.8264, 54.1509, 0);

-- [HIGH] entry 229955 'Stromgarde Citizen' path 8254063: 12 nodes, span 22yd, first node (-1589.4,-1799.6); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16198975, wpp_alliance2:20043993, wpp_alliance3:6162624, wpp_horde:2234479, rpe_h2_proj:10171979
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8254063;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8254063;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8254063, 0, 0, 2.423, 'Stromgarde Citizen - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 1, -1589.4149, -1799.5938, 71.4406, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 2, -1589.2048, -1799.6918, 71.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 3, -1590.2048, -1799.4418, 71.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 4, -1591.2048, -1799.4418, 72.2103, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 5, -1594.2048, -1799.4418, 73.7103, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 6, -1597.2048, -1799.4418, 74.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 7, -1600.2048, -1799.4418, 75.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 8, -1602.9548, -1799.4418, 76.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 9, -1605.9548, -1799.4418, 77.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 10, -1608.9548, -1799.4418, 78.7103, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 11, -1609.9548, -1799.6918, 78.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8254063, 12, -1610.9948, -1799.7899, 78.9800, 0);

-- [LOW] entry 142343 'Rampaging Owlbeast' path 8208340: 11 nodes, span 19yd, first node (-1249.9,-1549.8); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8208340;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8208340;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8208340, 0, 0, 2.573, 'Rampaging Owlbeast - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 1, -1249.8950, -1549.8433, 46.2623, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 2, -1251.1057, -1550.3455, 46.6524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 3, -1257.6057, -1553.0955, 46.9024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 4, -1259.3557, -1553.8455, 47.1524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 5, -1260.1057, -1554.0955, 47.4024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 6, -1261.8557, -1554.8455, 47.4024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 7, -1262.8557, -1555.0955, 47.6524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 8, -1263.6057, -1555.5955, 47.9024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 9, -1265.6057, -1556.3455, 48.1524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 10, -1266.3557, -1556.8455, 48.4024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8208340, 11, -1268.8164, -1557.8477, 48.5425, 0);

-- [LOW] entry 142347 'Wild Horse' path 8209995: 11 nodes, span 24yd, first node (-1271.2,-1617.2); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8209995;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8209995;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8209995, 0, 0, 2.525, 'Wild Horse - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 1, -1271.1833, -1617.2208, 51.9793, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 2, -1268.9849, -1616.0764, 51.9116, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 3, -1266.2349, -1615.0764, 51.9116, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 4, -1263.2349, -1614.3264, 51.4116, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 5, -1262.4849, -1613.8264, 50.9116, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 6, -1259.4849, -1613.0764, 50.1616, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 7, -1258.7349, -1613.0764, 49.9116, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 8, -1256.9849, -1612.3264, 49.6616, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 9, -1254.9849, -1611.8264, 49.4116, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 10, -1249.2349, -1610.0764, 49.6616, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8209995, 11, -1246.7865, -1608.9319, 49.3440, 0);

-- [HIGH] entry 244677 'Kobold Firetender' path 8122685: 10 nodes, span 16yd, first node (-1469.2,-2951.0); realm spawn guid 8300044 2.8yd from the route (MovementType 1); also seen in wpp_alliance3:6087494
DELETE FROM `waypoint_path_node` WHERE `PathId`=8122685;
DELETE FROM `waypoint_path` WHERE `PathId`=8122685;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8122685, 0, 0, 6.974, 'Kobold Firetender - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 1, -1469.1892, -2951.0105, 14.7531, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 2, -1472.6325, -2946.9473, 14.8514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 3, -1475.1325, -2945.9473, 14.8514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 4, -1475.6325, -2945.6973, 14.8514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 5, -1477.8825, -2944.6973, 14.8514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 6, -1478.3825, -2944.4473, 14.8514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 7, -1479.6325, -2943.4473, 14.3514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 8, -1480.1325, -2943.1973, 14.3514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 9, -1484.8825, -2941.4473, 14.3514, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8122685, 10, -1479.5758, -2941.3840, 13.9498, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8300044;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8300044, 8122685) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 249347 'Fightbot Version 11.2.7' path 8175643: 10 nodes, span 21yd, first node (-1544.8,-1844.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance2:20049077, wpp_alliance3:6138485, rpe_h2_proj:10171344
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8175643;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8175643;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8175643, 0, 0, 9.984, 'Fightbot Version 11.2.7 - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 1, -1544.7549, -1844.2908, 67.5855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 2, -1540.1474, -1846.8824, 68.1024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 3, -1536.6474, -1848.8824, 68.3524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 4, -1534.8974, -1849.6324, 68.3524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 5, -1532.6474, -1851.1324, 68.6024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 6, -1531.1474, -1851.8824, 68.6024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 7, -1529.6474, -1853.3824, 68.6024, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 8, -1527.6474, -1853.8824, 69.3524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 9, -1526.3974, -1854.1324, 69.3524, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8175643, 10, -1524.0399, -1854.9740, 69.1193, 0);

-- [HIGH] entry 142337 'Mesa Buzzard' path 8133285: 9 nodes, span 29yd, first node (-1571.8,-2830.7); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16074969, wpp_alliance3:6099960, wpp_horde:2224927
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8133285;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8133285;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8133285, 0, 0, 2.214, 'Mesa Buzzard - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 1, -1571.7750, -2830.6763, 39.1826, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 2, -1570.1378, -2828.6700, 39.9114, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 3, -1567.6378, -2823.4200, 39.9114, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 4, -1566.6378, -2821.4200, 40.1614, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 5, -1565.3878, -2818.9200, 40.4114, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 6, -1563.3878, -2814.6700, 40.6614, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 7, -1561.6378, -2810.9200, 40.9114, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 8, -1560.3878, -2808.4200, 41.1614, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8133285, 9, -1557.5006, -2801.6636, 40.6402, 0);

-- [HIGH] entry 142694 'Boulderfist Enforcer' path 8191111: 8 nodes, span 40yd, first node (-1347.3,-1987.0); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance2:20054441
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8191111;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8191111;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8191111, 0, 0, 2.993, 'Boulderfist Enforcer - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 1, -1347.2743, -1986.9548, 10.7256, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 2, -1342.2604, -1990.0165, 12.3352, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 3, -1337.2604, -1992.0165, 13.5852, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 4, -1332.0104, -1992.7665, 14.5852, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 5, -1327.5104, -1994.0165, 15.3352, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 6, -1321.7604, -1996.7665, 17.0852, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 7, -1315.2604, -1999.2665, 18.5852, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8191111, 8, -1307.7466, -1999.5781, 19.9447, 0);

-- [HIGH] entry 244643 'Lady Jaina Proudmoore' path 8114244: 5 nodes, span 96yd, first node (-1088.8,-3549.2); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16049347, wpp_alliance2:20006752, wpp_alliance3:6081230, wpp_horde:2218835, rpe_h2_proj:10097454
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8114244;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8114244;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8114244, 0, 0, 22.019, 'Lady Jaina Proudmoore - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8114244, 1, -1088.8317, -3549.2360, 54.5503, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8114244, 2, -1097.5903, -3544.8923, 55.1354, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8114244, 3, -1117.1163, -3529.1875, 62.0516, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8114244, 4, -1144.3698, -3502.1199, 62.0516, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8114244, 5, -1173.2570, -3453.5312, 62.0516, 0);

-- [HIGH] entry 244657 'Thrall' path 8168472: 5 nodes, span 176yd, first node (-1469.2,-1823.4); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16111896, wpp_alliance2:20042223, wpp_alliance3:6133848, wpp_horde:2237855, rpe_h2_proj:10171157
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8168472;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8168472;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8168472, 0, 0, 21.332, 'Thrall - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168472, 1, -1469.1771, -1823.3680, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168472, 2, -1446.2517, -1821.4166, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168472, 3, -1418.8716, -1814.1041, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168472, 4, -1362.3854, -1825.5920, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168472, 5, -1292.8160, -1870.1858, 103.9123, 0);

-- [HIGH] entry 244658 'Lady Jaina Proudmoore' path 8168410: 5 nodes, span 169yd, first node (-1462.1,-1806.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16111709, wpp_alliance2:20042283, wpp_alliance3:6134402, wpp_horde:2237786, rpe_h2_proj:10166821
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8168410;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8168410;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8168410, 0, 0, 22.488, 'Lady Jaina Proudmoore - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168410, 1, -1462.1302, -1806.3212, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168410, 2, -1446.5973, -1813.6111, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168410, 3, -1418.8716, -1814.1041, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168410, 4, -1362.3854, -1825.5920, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8168410, 5, -1292.8160, -1870.1858, 103.9123, 0);

-- [HIGH] entry 244666 'Thrall' path 8242257: 4 nodes, span 173yd, first node (-1004.6,-1983.3); realm spawn guid 8000139 3.8yd from the route (MovementType 0); also seen in wpp_alliance:16175686, wpp_alliance2:20078827, wpp_alliance3:6247928, wpp_horde:2256724, rpe_h2_proj:10246349
DELETE FROM `waypoint_path_node` WHERE `PathId`=8242257;
DELETE FROM `waypoint_path` WHERE `PathId`=8242257;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8242257, 0, 0, 39.185, 'Thrall - captured route (rpe_a4_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242257, 1, -1004.6042, -1983.2760, 65.3553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242257, 2, -1013.1962, -1996.6910, 77.2598, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242257, 3, -1045.4791, -2087.6910, 105.6033, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242257, 4, -1055.8663, -2156.4392, 105.6033, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000139;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000139, 8242257) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244667 'Lady Jaina Proudmoore' path 8242318: 4 nodes, span 138yd, first node (-1016.8,-1979.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16175532, wpp_alliance2:20078828, wpp_alliance3:6247642, wpp_horde:2256725, rpe_h2_proj:10246350
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=8242318;
-- DELETE FROM `waypoint_path` WHERE `PathId`=8242318;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (8242318, 0, 0, 36.589, 'Lady Jaina Proudmoore - captured route (rpe_a4_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242318, 1, -1016.7882, -1979.3385, 66.1351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242318, 2, -1038.9740, -1974.2743, 76.1866, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242318, 3, -1050.0452, -1968.0192, 97.4374, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (8242318, 4, -1154.9375, -1929.6354, 133.4239, 0);
