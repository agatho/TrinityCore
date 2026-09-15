-- ========================================================================================
-- CONTENT SLICE -- Waking Shores (map 2927) :: patrol routes recovered from SMSG_ON_MONSTER_MOVE
-- ========================================================================================
-- Generated 2026-09-10 by TCHarvest tools/author_chain_slice.py
-- Sources: rpe_h3_proj: dump_12.1.0.69587_2026-09-10_07-32-31.pkt + TCHarvest.lua + combat log; wpp_alliance; wpp_alliance2; wpp_alliance3; wpp_horde; rpe_h2_proj; rpe_a4_proj
--
-- A route is emitted LIVE only when TWO captures reconstructed a path for the same entry starting
-- within 15yd of each other, AND a realm spawn of that entry stands within 8yd of the route's first
-- routes: 5 live (two-capture), 17 commented for review
-- node with MovementType 0/1 (it then becomes MovementType 2 on that path). Single-capture routes
-- are COMMENTED. Wanderers are not touched here (their captured positions are roam snapshots).
-- PathIds are the rig's stable ids for traceability. Velocity from the wire is informational.
-- ========================================================================================


-- [HIGH] entry 249249 'Hammerfall Peon' path 2170105: 30 nodes, span 45yd, first node (-1518.5,-3124.7); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16098179, wpp_alliance2:20038169, wpp_alliance3:6118446, wpp_horde:2230354, rpe_h2_proj:10141549, rpe_a4_proj:8152602
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2170105;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2170105;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2170105, 0, 0, 11.061, 'Hammerfall Peon - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 1, -1518.5170, -3124.7330, 27.1633, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 2, -1522.6728, -3110.9312, 26.9372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 3, -1523.9228, -3113.6812, 26.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 4, -1524.4228, -3115.4312, 26.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 5, -1525.6728, -3117.9312, 26.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 6, -1526.9228, -3120.6812, 26.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 7, -1529.6728, -3126.9312, 26.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 8, -1527.4228, -3128.6812, 26.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 9, -1525.6728, -3129.9312, 26.9372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 10, -1523.4228, -3131.9312, 27.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 11, -1521.1728, -3133.4312, 27.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 12, -1519.6728, -3134.6812, 27.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 13, -1517.1728, -3136.6812, 27.9372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 14, -1514.9228, -3138.4312, 28.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 15, -1514.1728, -3139.1812, 28.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 16, -1511.6728, -3140.6812, 28.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 17, -1509.9228, -3141.6812, 28.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 18, -1508.1728, -3142.6812, 28.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 19, -1505.4228, -3143.9312, 28.9372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 20, -1503.6728, -3144.9312, 29.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 21, -1501.4228, -3146.4312, 29.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 22, -1499.6728, -3147.4312, 29.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 23, -1497.9228, -3148.4312, 29.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 24, -1495.4228, -3149.9312, 29.9372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 25, -1493.6728, -3150.9312, 30.1872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 26, -1490.9228, -3152.4312, 30.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 27, -1489.1728, -3153.4312, 30.4372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 28, -1487.4228, -3154.4312, 30.6872, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 29, -1484.9228, -3155.9312, 30.9372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2170105, 30, -1488.8286, -3141.6294, 30.2111, 0);

-- [HIGH] entry 142333 'Giant Plains Creeper' path 2192549: 29 nodes, span 35yd, first node (-1432.7,-2799.4); NO realm spawn of this entry within 8yd of the route; also seen in wpp_horde:2221378, rpe_h2_proj:10154350, rpe_a4_proj:8135215
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2192549;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2192549;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2192549, 0, 0, 2.522, 'Giant Plains Creeper - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 1, -1432.6782, -2799.3887, 57.6262, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 2, -1433.8186, -2801.0156, 57.4719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 3, -1434.8186, -2802.7656, 56.9719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 4, -1435.8186, -2804.5156, 56.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 5, -1436.8186, -2806.2656, 56.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 6, -1438.0686, -2807.7656, 55.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 7, -1439.0686, -2809.5156, 55.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 8, -1439.5686, -2810.5156, 54.9719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 9, -1440.5686, -2812.0156, 54.4719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 10, -1441.8186, -2813.7656, 53.4719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 11, -1442.3186, -2814.7656, 53.4719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 12, -1442.8186, -2815.5156, 52.9719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 13, -1443.3186, -2816.2656, 52.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 14, -1443.5686, -2817.0156, 51.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 15, -1444.0686, -2817.7656, 51.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 16, -1444.5686, -2818.5156, 50.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 17, -1446.3186, -2821.2656, 48.9719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 18, -1446.8186, -2822.0156, 47.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 19, -1447.3186, -2822.7656, 47.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 20, -1447.8186, -2823.7656, 46.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 21, -1448.8186, -2825.5156, 44.9719, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 22, -1449.3186, -2826.2656, 44.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 23, -1450.0686, -2827.0156, 43.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 24, -1450.5686, -2828.0156, 43.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 25, -1451.5686, -2829.7656, 42.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 26, -1452.0686, -2830.5156, 42.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 27, -1452.5686, -2831.2656, 41.7219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 28, -1453.0686, -2832.2656, 41.2219, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2192549, 29, -1454.4590, -2834.1426, 40.3177, 0);

-- [LOW] entry 142334 'Plains Creeper' path 2203382: 28 nodes, span 24yd, first node (-1615.9,-2335.2); realm spawn guid 8001174 5.9yd from the route (MovementType 1); single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2203382;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2203382;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2203382, 0, 0, 2.506, 'Plains Creeper - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 1, -1615.8729, -2335.2485, 78.6900, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 2, -1617.1289, -2333.5509, 77.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 3, -1617.8789, -2332.8009, 77.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 4, -1618.3789, -2332.0509, 76.6200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 5, -1619.1289, -2331.0509, 76.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 6, -1619.6289, -2330.3009, 75.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 7, -1620.3789, -2329.5509, 75.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 8, -1621.1289, -2328.8009, 74.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 9, -1622.3789, -2327.3009, 73.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 10, -1622.8789, -2326.5509, 72.6200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 11, -1623.6289, -2325.8009, 72.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 12, -1624.1289, -2325.0509, 71.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 13, -1624.8789, -2324.3009, 71.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 14, -1625.6289, -2323.5509, 70.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 15, -1625.8789, -2323.0509, 70.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 16, -1626.6289, -2322.3009, 69.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 17, -1627.1289, -2321.5509, 69.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 18, -1628.3789, -2319.8009, 68.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 19, -1629.1289, -2319.0509, 67.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 20, -1629.8789, -2318.3009, 66.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 21, -1630.3789, -2317.5509, 66.1200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 22, -1631.6289, -2316.0509, 65.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 23, -1632.3789, -2315.3009, 64.3700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 24, -1633.6289, -2313.8009, 63.6200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 25, -1634.3789, -2313.0509, 62.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 26, -1634.8789, -2312.3009, 62.6200, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 27, -1635.6289, -2311.5509, 61.8700, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2203382, 28, -1635.8849, -2311.3533, 61.5499, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001174;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001174, 2203382) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244695 'Ettin Crusher' path 2237458: 26 nodes, span 30yd, first node (-1239.0,-1772.9); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16113458, wpp_alliance2:20060524, wpp_alliance3:6193442, wpp_horde:2238684, rpe_h2_proj:10192950
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2237458;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2237458;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2237458, 0, 0, 2.431, 'Ettin Crusher - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 1, -1238.9740, -1772.8629, 64.3943, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 2, -1238.2243, -1772.1600, 64.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 3, -1236.7243, -1770.9100, 64.1355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 4, -1235.2243, -1769.6600, 63.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 5, -1234.4743, -1768.9100, 63.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 6, -1232.9743, -1767.6600, 63.6355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 7, -1232.2243, -1766.9100, 63.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 8, -1230.7243, -1765.6600, 63.1355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 9, -1229.9743, -1764.9100, 62.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 10, -1228.4743, -1763.6600, 62.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 11, -1226.9743, -1762.4100, 62.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 12, -1226.2243, -1761.6600, 62.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 13, -1224.7243, -1760.4100, 62.1355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 14, -1222.7243, -1758.6600, 61.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 15, -1221.2243, -1757.1600, 61.6355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 16, -1219.7243, -1755.9100, 61.6355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 17, -1218.9743, -1755.4100, 61.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 18, -1218.2243, -1754.6600, 61.1355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 19, -1216.7243, -1753.4100, 60.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 20, -1215.9743, -1752.6600, 60.6355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 21, -1214.4743, -1751.4100, 60.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 22, -1212.9743, -1749.9100, 60.1355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 23, -1211.4743, -1748.6600, 59.8855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 24, -1210.7243, -1747.9100, 59.6355, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 25, -1209.2243, -1746.6600, 59.3855, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2237458, 26, -1208.9746, -1746.4570, 58.8767, 0);

-- [LOW] entry 142338 'Highland Strider' path 2312731: 24 nodes, span 24yd, first node (-1068.3,-3231.6); NO realm spawn of this entry within 8yd of the route; single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2312731;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2312731;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2312731, 0, 0, 2.514, 'Highland Strider - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 1, -1068.3054, -3231.5833, 43.6421, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 2, -1065.8578, -3233.5934, 45.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 3, -1065.1078, -3234.0934, 45.3652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 4, -1064.3578, -3234.8434, 45.8652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 5, -1063.6078, -3235.5934, 46.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 6, -1062.8578, -3236.0934, 46.3652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 7, -1062.1078, -3236.8434, 47.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 8, -1060.3578, -3238.0934, 47.6152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 9, -1059.6078, -3238.8434, 48.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 10, -1058.8578, -3239.3434, 48.3652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 11, -1058.1078, -3240.0934, 48.6152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 12, -1057.3578, -3240.5934, 48.8652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 13, -1056.6078, -3241.3434, 49.3652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 14, -1056.1078, -3242.0934, 49.8652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 15, -1054.6078, -3243.0934, 50.3652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 16, -1053.8578, -3243.8434, 50.6152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 17, -1052.3578, -3245.0934, 51.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 18, -1050.8578, -3246.3434, 51.6152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 19, -1049.3578, -3247.5934, 52.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 20, -1047.8578, -3248.8434, 52.8652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 21, -1046.3578, -3250.3434, 53.3652, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 22, -1045.6078, -3250.8434, 53.6152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 23, -1044.1078, -3252.0934, 54.1152, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2312731, 24, -1043.9102, -3252.6035, 54.0884, 0);

-- [HIGH] entry 244685 'Ogre Basher' path 2245060: 24 nodes, span 22yd, first node (-1262.1,-1891.8); realm spawn guid 8000142 0.0yd from the route (MovementType 2); also seen in wpp_alliance:16125060, wpp_alliance3:6186749, wpp_horde:2239556, rpe_a4_proj:8184533
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2245060;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2245060;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2245060, 0, 0, 2.379, 'Ogre Basher - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 1, -1262.1060, -1891.7795, 79.5429, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 2, -1262.6068, -1891.4375, 79.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 3, -1262.8568, -1891.1875, 79.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 4, -1264.6068, -1890.1875, 79.1509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 5, -1265.3568, -1889.6875, 78.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 6, -1267.1068, -1888.9375, 78.1509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 7, -1268.1068, -1888.4375, 77.9009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 8, -1268.8568, -1887.9375, 77.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 9, -1269.8568, -1887.4375, 77.4009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 10, -1270.6068, -1886.9375, 76.9009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 11, -1272.3568, -1885.9375, 76.4009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 12, -1273.1068, -1885.6875, 76.1509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 13, -1273.8568, -1885.1875, 75.9009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 14, -1275.6068, -1884.1875, 75.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 15, -1276.6068, -1883.6875, 75.4009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 16, -1277.3568, -1883.1875, 75.1509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 17, -1278.3568, -1882.6875, 74.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 18, -1279.1068, -1882.1875, 74.4009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 19, -1280.1068, -1881.6875, 73.9009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 20, -1280.8568, -1881.1875, 73.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 21, -1281.6068, -1880.6875, 73.4009, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 22, -1282.6068, -1880.1875, 73.1509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 23, -1283.3568, -1879.6875, 72.6509, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245060, 24, -1283.6077, -1879.5955, 72.2590, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000142;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000142, 2245060) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 230001 'Stromgarde Orphan' path 2280125: 18 nodes, span 25yd, first node (-1625.0,-1877.3); realm spawn guid 8001072 4.0yd from the route (MovementType 0); also seen in wpp_alliance:16196808, wpp_alliance2:20044584, wpp_alliance3:6147421, wpp_horde:2233103, rpe_h2_proj:10177908, rpe_a4_proj:8173401
DELETE FROM `waypoint_path_node` WHERE `PathId`=2280125;
DELETE FROM `waypoint_path` WHERE `PathId`=2280125;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2280125, 0, 0, 7.209, 'Stromgarde Orphan - captured route (rpe_h3_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 1, -1625.0052, -1877.3317, 81.2551, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 2, -1625.0634, -1876.8881, 81.4738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 3, -1623.3134, -1879.6381, 81.4738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 4, -1621.3134, -1885.1381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 5, -1620.3134, -1887.6381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 6, -1620.8134, -1887.6381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 7, -1620.8134, -1888.6381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 8, -1620.8134, -1889.3881, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 9, -1620.8134, -1890.6381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 10, -1620.8134, -1891.6381, 80.7238, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 11, -1620.8134, -1892.6381, 80.7238, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 12, -1620.8134, -1893.6381, 80.7238, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 13, -1620.8134, -1894.6381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 14, -1620.8134, -1895.6381, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 15, -1620.8134, -1896.8881, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 16, -1620.8134, -1897.8881, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 17, -1621.3134, -1901.8881, 80.9738, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2280125, 18, -1622.1216, -1901.4445, 80.6925, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001072;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001072, 2280125) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244674 'Ogre Destroyer' path 2154826: 17 nodes, span 18yd, first node (-1417.8,-2982.2); realm spawn guid 8000153 0.0yd from the route (MovementType 2); also seen in wpp_alliance:16071648, wpp_alliance2:20014874, wpp_alliance3:6086990, wpp_horde:2225890, rpe_h2_proj:10125682, rpe_a4_proj:8122223
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2154826;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2154826;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2154826, 0, 0, 2.535, 'Ogre Destroyer - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 1, -1417.7865, -2982.1807, 19.1525, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 2, -1416.4054, -2982.7206, 19.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 3, -1417.4054, -2982.2206, 19.4664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 4, -1418.9054, -2981.2206, 18.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 5, -1419.4054, -2980.9706, 18.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 6, -1419.4054, -2980.9706, 18.4664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 7, -1420.4054, -2980.7206, 18.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 8, -1422.4054, -2980.7206, 17.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 9, -1425.4054, -2980.2206, 16.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 10, -1428.1554, -2979.7206, 16.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 11, -1428.9054, -2979.4706, 15.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 12, -1429.9054, -2979.2206, 15.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 13, -1430.9054, -2979.2206, 15.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 14, -1431.9054, -2978.9706, 15.2164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 15, -1432.9054, -2978.7206, 14.9664, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 16, -1433.9054, -2978.4706, 14.7164, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2154826, 17, -1434.0243, -2979.2605, 14.7803, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000153;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000153, 2154826) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 142694 'Boulderfist Enforcer' path 2245534: 16 nodes, span 73yd, first node (-1234.0,-1970.5); realm spawn guid 8001139 4.6yd from the route (MovementType 0); also seen in wpp_alliance:16123650, wpp_alliance3:6191007, wpp_horde:2240502, rpe_h2_proj:10207806
DELETE FROM `waypoint_path_node` WHERE `PathId`=2245534;
DELETE FROM `waypoint_path` WHERE `PathId`=2245534;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2245534, 0, 0, 2.996, 'Boulderfist Enforcer - captured route (rpe_h3_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 1, -1233.9896, -1970.4844, 21.2491, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 2, -1230.5773, -1965.0712, 21.9341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 3, -1228.3273, -1961.0712, 22.1841, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 4, -1227.0773, -1954.8212, 21.4341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 5, -1226.8273, -1948.5712, 20.9341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 6, -1226.8273, -1941.8212, 20.4341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 7, -1228.0773, -1932.5712, 19.4341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 8, -1230.3273, -1924.3212, 17.9341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 9, -1233.0773, -1917.8212, 17.6841, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 10, -1236.0773, -1912.5712, 17.1841, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 11, -1239.5773, -1907.5712, 17.1841, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 12, -1243.5773, -1903.8212, 16.4341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 13, -1249.3273, -1901.0712, 15.6841, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 14, -1255.5773, -1899.0712, 15.1841, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 15, -1261.0773, -1898.3212, 16.4341, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2245534, 16, -1268.6649, -1897.6580, 18.1191, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001139;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001139, 2245534) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244691 'Gnoll Charger' path 2228506: 15 nodes, span 20yd, first node (-1437.1,-1798.1); realm spawn guid 8000124 3.6yd from the route (MovementType 2); also seen in wpp_alliance:16112716, wpp_alliance2:20042877, wpp_alliance3:6182186, wpp_horde:2237952, rpe_h2_proj:10182604, rpe_a4_proj:8190598
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2228506;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2228506;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2228506, 0, 0, 9.316, 'Gnoll Charger - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 1, -1437.0521, -1798.1024, 62.4269, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 2, -1434.2075, -1798.5886, 62.3764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 3, -1436.2075, -1798.0886, 62.6264, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 4, -1437.9575, -1797.5886, 62.8764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 5, -1439.9575, -1797.3386, 63.3764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 6, -1440.2075, -1797.0886, 63.3764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 7, -1442.2075, -1796.8386, 63.6264, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 8, -1444.2075, -1796.8386, 63.6264, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 9, -1445.9575, -1796.5886, 63.8764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 10, -1447.9575, -1796.3386, 64.3764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 11, -1449.9575, -1796.0886, 64.6264, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 12, -1450.9575, -1796.0886, 64.8764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 13, -1451.9575, -1795.8386, 64.8764, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 14, -1453.9575, -1795.5886, 65.1264, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2228506, 15, -1451.8629, -1796.5747, 64.8258, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000124;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000124, 2228506) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 142340 'Highland Fleshstalker' path 2202082: 13 nodes, span 34yd, first node (-1700.0,-2816.7); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance2:20016005
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2202082;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2202082;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2202082, 0, 0, 2.704, 'Highland Fleshstalker - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 1, -1700.0000, -2816.6660, 47.0221, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 2, -1695.6113, -2815.7578, 47.5897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 3, -1692.6113, -2815.5078, 48.0897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 4, -1689.6113, -2815.0078, 48.0897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 5, -1685.6113, -2814.5078, 48.3397, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 6, -1679.8613, -2814.0078, 48.0897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 7, -1677.8613, -2813.7578, 47.8397, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 8, -1676.8613, -2813.7578, 47.8397, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 9, -1674.8613, -2813.5078, 47.3397, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 10, -1672.8613, -2813.2578, 46.8397, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 11, -1669.8613, -2812.7578, 46.0897, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 12, -1668.8613, -2812.7578, 45.8397, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2202082, 13, -1666.2227, -2812.3496, 45.1573, 0);

-- [HIGH] entry 244672 'Gnoll Bruiser' path 2093693: 13 nodes, span 20yd, first node (-985.6,-3514.5); realm spawn guid 8000205 6.9yd from the route (MovementType 0); also seen in rpe_h2_proj:10067446
DELETE FROM `waypoint_path_node` WHERE `PathId`=2093693;
DELETE FROM `waypoint_path` WHERE `PathId`=2093693;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2093693, 0, 0, 10.171, 'Gnoll Bruiser - captured route (rpe_h3_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 1, -985.5816, -3514.5122, 56.9921, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 2, -981.4452, -3516.3168, 57.3310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 3, -982.1952, -3517.5668, 57.8310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 4, -982.6952, -3519.5668, 58.0810, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 5, -982.9452, -3521.5668, 57.8310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 6, -983.1952, -3521.8168, 57.8310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 7, -983.1952, -3523.8168, 57.5810, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 8, -983.1952, -3525.8168, 57.3310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 9, -981.9452, -3527.3168, 57.0810, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 10, -980.6952, -3528.8168, 56.8310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 11, -979.9452, -3529.8168, 56.8310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 12, -978.1952, -3534.0668, 56.8310, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2093693, 13, -972.8088, -3533.6213, 56.6700, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000205;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000205, 2093693) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 229955 'Stromgarde Citizen' path 2274818: 12 nodes, span 23yd, first node (-1611.0,-1799.8); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16198975, wpp_alliance2:20043993, wpp_alliance3:6162624, wpp_horde:2234479, rpe_h2_proj:10171979, rpe_a4_proj:8254063
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2274818;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2274818;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2274818, 0, 0, 2.387, 'Stromgarde Citizen - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 1, -1610.9948, -1799.7899, 78.9800, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 2, -1611.9548, -1799.6918, 79.4603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 3, -1609.9548, -1799.6918, 78.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 4, -1608.9548, -1799.6918, 78.7103, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 5, -1605.9548, -1799.6918, 77.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 6, -1602.9548, -1799.6918, 76.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 7, -1600.2048, -1799.6918, 75.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 8, -1598.2048, -1799.6918, 75.4603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 9, -1596.2048, -1799.6918, 75.2103, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 10, -1593.2048, -1799.6918, 73.9603, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 11, -1590.2048, -1799.6918, 72.7103, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2274818, 12, -1589.4149, -1799.5938, 71.4406, 0);

-- [LOW] entry 142339 'Highland Thrasher' path 2201703: 10 nodes, span 18yd, first node (-1582.2,-2816.2); realm spawn guid 8001267 1.1yd from the route (MovementType 1); single capture
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2201703;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2201703;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2201703, 0, 0, 2.658, 'Highland Thrasher - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 1, -1582.1543, -2816.2050, 37.0787, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 2, -1582.5107, -2813.4639, 37.6438, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 3, -1582.7607, -2811.4639, 38.1438, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 4, -1582.7607, -2809.4639, 38.1438, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 5, -1583.0107, -2806.7139, 38.3938, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 6, -1583.0107, -2804.7139, 38.6438, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 7, -1583.0107, -2802.7139, 38.8938, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 8, -1583.2607, -2800.7139, 39.1438, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 9, -1583.2607, -2798.7139, 39.3938, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2201703, 10, -1583.8671, -2797.7227, 39.2090, 0);
-- UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001267;
-- INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001267, 2201703) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 249347 'Fightbot Version 11.2.7' path 2226081: 10 nodes, span 29yd, first node (-1553.4,-1845.2); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance2:20049077, wpp_alliance3:6138485, rpe_h2_proj:10171344, rpe_a4_proj:8175643
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2226081;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2226081;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2226081, 0, 0, 9.998, 'Fightbot Version 11.2.7 - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 1, -1553.3916, -1845.1957, 67.5802, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 2, -1538.7158, -1849.5848, 68.0997, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 3, -1537.2158, -1850.0848, 68.3497, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 4, -1535.2158, -1850.5848, 68.3497, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 5, -1532.4658, -1851.5848, 68.5997, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 6, -1531.2158, -1851.8348, 68.5997, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 7, -1529.7158, -1853.3348, 68.5997, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 8, -1527.7158, -1853.8348, 69.3497, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 9, -1526.4658, -1854.0848, 69.3497, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2226081, 10, -1524.0399, -1854.9740, 69.1193, 0);

-- [HIGH] entry 883 'Deer' path 2235793: 9 nodes, span 23yd, first node (-1207.6,-1753.6); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16117789
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2235793;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2235793;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2235793, 0, 0, 5.675, 'Deer - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 1, -1207.6060, -1753.5686, 59.3372, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 2, -1210.8372, -1751.0636, 59.6618, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 3, -1204.0872, -1759.8136, 59.9118, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 4, -1202.3372, -1762.0636, 60.1618, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 5, -1201.0872, -1763.8136, 59.9118, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 6, -1200.8372, -1764.3136, 59.4118, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 7, -1199.5872, -1765.8136, 59.1618, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 8, -1196.3372, -1769.8136, 59.1618, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2235793, 9, -1194.5684, -1774.5586, 58.9863, 0);

-- [HIGH] entry 142566 'Drywhisker Kobold' path 2080783: 9 nodes, span 16yd, first node (-1146.5,-3612.4); realm spawn guid 8001293 3.0yd from the route (MovementType 0); also seen in wpp_alliance:16015485, wpp_alliance2:19989846, wpp_alliance3:6080844, wpp_horde:2212008, rpe_h2_proj:10095849, rpe_a4_proj:7998554
DELETE FROM `waypoint_path_node` WHERE `PathId`=2080783;
DELETE FROM `waypoint_path` WHERE `PathId`=2080783;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2080783, 0, 0, 2.539, 'Drywhisker Kobold - captured route (rpe_h3_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 1, -1146.4851, -3612.4080, 42.7789, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 2, -1151.3261, -3613.7476, 43.3793, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 3, -1153.3261, -3614.7476, 43.6293, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 4, -1154.8261, -3615.2476, 43.8793, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 5, -1156.5761, -3615.9976, 43.8793, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 6, -1158.5761, -3616.7476, 44.1293, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 7, -1159.3261, -3617.2476, 44.3793, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 8, -1161.0761, -3617.9976, 44.8793, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2080783, 9, -1162.6671, -3618.0872, 44.9797, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8001293;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8001293, 2080783) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244643 'Lady Jaina Proudmoore' path 2123693: 5 nodes, span 96yd, first node (-1088.8,-3549.2); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16049347, wpp_alliance2:20006752, wpp_alliance3:6081230, wpp_horde:2218835, rpe_h2_proj:10097454, rpe_a4_proj:8114244
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2123693;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2123693;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2123693, 0, 0, 22.019, 'Lady Jaina Proudmoore - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2123693, 1, -1088.8317, -3549.2360, 54.5503, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2123693, 2, -1097.5903, -3544.8923, 55.1354, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2123693, 3, -1117.1163, -3529.1875, 62.0516, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2123693, 4, -1144.3698, -3502.1199, 62.0516, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2123693, 5, -1173.2570, -3453.5312, 62.0516, 0);

-- [HIGH] entry 244657 'Thrall' path 2212062: 5 nodes, span 176yd, first node (-1469.2,-1823.4); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16111896, wpp_alliance2:20042223, wpp_alliance3:6133848, wpp_horde:2237855, rpe_h2_proj:10171157, rpe_a4_proj:8168472
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2212062;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2212062;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2212062, 0, 0, 21.332, 'Thrall - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212062, 1, -1469.1771, -1823.3680, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212062, 2, -1446.2517, -1821.4166, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212062, 3, -1418.8716, -1814.1041, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212062, 4, -1362.3854, -1825.5920, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212062, 5, -1292.8160, -1870.1858, 103.9123, 0);

-- [HIGH] entry 244658 'Lady Jaina Proudmoore' path 2212262: 5 nodes, span 169yd, first node (-1462.1,-1806.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16111709, wpp_alliance2:20042283, wpp_alliance3:6134402, wpp_horde:2237786, rpe_h2_proj:10166821, rpe_a4_proj:8168410
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2212262;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2212262;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2212262, 0, 0, 22.488, 'Lady Jaina Proudmoore - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212262, 1, -1462.1302, -1806.3212, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212262, 2, -1446.5973, -1813.6111, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212262, 3, -1418.8716, -1814.1041, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212262, 4, -1362.3854, -1825.5920, 103.9123, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2212262, 5, -1292.8160, -1870.1858, 103.9123, 0);

-- [HIGH] entry 244666 'Thrall' path 2303727: 4 nodes, span 173yd, first node (-1004.6,-1983.3); realm spawn guid 8000139 3.8yd from the route (MovementType 0); also seen in wpp_alliance:16175686, wpp_alliance2:20078827, wpp_alliance3:6247928, wpp_horde:2256724, rpe_h2_proj:10246349, rpe_a4_proj:8242257
DELETE FROM `waypoint_path_node` WHERE `PathId`=2303727;
DELETE FROM `waypoint_path` WHERE `PathId`=2303727;
INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2303727, 0, 0, 39.185, 'Thrall - captured route (rpe_h3_proj)');
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303727, 1, -1004.6042, -1983.2760, 65.3553, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303727, 2, -1013.1962, -1996.6910, 77.2598, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303727, 3, -1045.4791, -2087.6910, 105.6033, 0);
INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303727, 4, -1055.8663, -2156.4392, 105.6033, 0);
UPDATE `creature` SET `MovementType`=2 WHERE `guid`=8000139;
INSERT INTO `creature_addon` (`guid`, `PathId`) VALUES (8000139, 2303727) ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);

-- [HIGH] entry 244667 'Lady Jaina Proudmoore' path 2303728: 4 nodes, span 138yd, first node (-1016.8,-1979.3); NO realm spawn of this entry within 8yd of the route; also seen in wpp_alliance:16175532, wpp_alliance2:20078828, wpp_alliance3:6247642, wpp_horde:2256725, rpe_h2_proj:10246350, rpe_a4_proj:8242318
-- DELETE FROM `waypoint_path_node` WHERE `PathId`=2303728;
-- DELETE FROM `waypoint_path` WHERE `PathId`=2303728;
-- INSERT INTO `waypoint_path` (`PathId`, `MoveType`, `Flags`, `Velocity`, `Comment`) VALUES (2303728, 0, 0, 36.589, 'Lady Jaina Proudmoore - captured route (rpe_h3_proj)');
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303728, 1, -1016.7882, -1979.3385, 66.1351, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303728, 2, -1038.9740, -1974.2743, 76.1866, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303728, 3, -1050.0452, -1968.0192, 97.4374, 0);
-- INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Delay`) VALUES (2303728, 4, -1154.9375, -1929.6354, 133.4239, 0);
