-- ========================================================================================
-- CONTENT SLICE -- Waking Shores (map 2927) :: spawn reconcile across 7 captures
-- ========================================================================================
-- Generated 2026-09-10 by TCHarvest tools/author_chain_slice.py
-- Sources: rpe_h3_proj: dump_12.1.0.69587_2026-09-10_07-32-31.pkt + TCHarvest.lua + combat log; wpp_alliance; wpp_alliance2; wpp_alliance3; wpp_horde; rpe_h2_proj; rpe_a4_proj
--
-- tools/zone_census_report.py --reconcile: union by entry+position (3yd), diffed against the realm
-- (4yd, claimed matching). Wanderers reconciled by population, never by position. Entries seen in
-- one capture only are HELD (listed in the ledger below), never deleted, never emitted.
-- ========================================================================================

-- CANDIDATE spawns: present in the capture union, absent from the realm.
-- spawntimesecs: 0 measured, 77 from realm siblings, 5 fallback (300s). Per-row provenance below.
-- No respawn cadence was confidently measured. That is the expected result on a walk-through
-- capture: a create block fires on VISIBILITY, not on spawn -- see synth/timing.py for the numbers.
-- 210 row(s) from WANDERING entries were skipped: a roaming creature is captured
-- wherever it stood, so its coordinates are not placement data. Reconcile those by count.
-- Provenance is on every row. No DELETEs are generated: a realm spawn the captures
-- never saw is a coverage question, not a defect.
DELETE FROM `creature` WHERE `guid` BETWEEN 8200000 AND 8200081;
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`MovementType`) VALUES
 (8200000, 141725, 2927, 1959, -1144.2051, -2137.1113, 59.9994, 4.0919, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001116 spawntime=120s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200001, 141725, 2927, 1959, -1172.4270, -2138.7822, 60.8182, 3.9104, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001116 spawntime=120s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200002, 141727, 2927, 1959, -1580.4554, -2150.4958, 20.0660, 3.0262, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001121 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200003, 141727, 2927, 1959, -1550.5920, -2212.3608, 28.6314, 1.5566, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001118 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200004, 141727, 2927, 1959, -1517.3341, -2120.4434, 17.4855, 3.9311, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001123 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200005, 141727, 2927, 1959, -1514.6102, -2219.3555, 20.2777, 2.4891, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001120 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200006, 141727, 2927, 1959, -1581.6652, -2185.4993, 26.8471, 6.2323, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001118 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200007, 141727, 2927, 1959, -1579.4866, -2119.8276, 28.1212, 5.7641, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001122 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200008, 141727, 2927, 1959, -1479.4902, -2217.3711, 26.4769, 4.6640, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001120 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200009, 141727, 2927, 1959, -1581.9261, -2221.3210, 33.4421, 5.1602, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001118 spawntime=120s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200010, 142335, 2927, 3, -1598.8120, -2102.8850, 41.9024, 0.4387, 300, 0),  -- sources=wpp_horde phase=inherited from guid 8001210 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200011, 142335, 2927, 3, -1545.2456, -2239.0039, 46.6621, 1.4742, 300, 0),  -- sources=wpp_horde phase=inherited from guid 8001210 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200012, 142341, 2927, 3, -1794.0487, -1805.7944, 65.4227, 4.7668, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001212 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200013, 142341, 2927, 3, -1821.4918, -1428.8406, 79.1407, 3.8808, 300, 0),  -- sources=wpp_horde phase=inherited from guid 8001213 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200014, 142341, 2927, 3, -1777.0072, -1992.1748, 74.9850, 5.9430, 300, 0),  -- sources=wpp_horde phase=inherited from guid 8001214 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200015, 142342, 2927, 3, -841.7362, -1652.6749, 57.7875, 5.7250, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001215 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200016, 142342, 2927, 3, -1189.6290, -1594.7501, 44.6275, 0.5284, 300, 0),  -- sources=wpp_alliance2+wpp_alliance3 phase=inherited from guid 8001215 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200017, 142342, 2927, 3, -1162.0830, -1545.3411, 47.3289, 1.9847, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001215 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200018, 142342, 2927, 3, -1305.6252, -1603.1445, 51.7126, 3.7925, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001216 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200019, 142342, 2927, 3, -1200.5876, -1683.7819, 48.0654, 1.1807, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001216 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200020, 142342, 2927, 3, -699.7743, -1744.2118, 59.2181, 2.0926, 300, 0),  -- sources=wpp_horde phase=inherited from guid 8001215 spawntime=300s realm siblings (2 of 2 existing spawns of this entry; consistency, not a measurement)
 (8200021, 142347, 2927, 3, -1252.0294, -1608.2716, 49.4069, 3.1475, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001217 spawntime=300s realm siblings (1 of 1 existing spawns of this entry; consistency, not a measurement)
 (8200022, 142566, 2927, 1959, -977.1318, -3653.6960, 83.0802, 3.8108, 120, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001293 spawntime=120s realm siblings (1 of 1 existing spawns of this entry; consistency, not a measurement)
 (8200023, 142566, 2927, 1959, -1147.4158, -3607.2715, 42.6943, 1.1695, 120, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001293 spawntime=120s realm siblings (1 of 1 existing spawns of this entry; consistency, not a measurement)
 (8200024, 142694, 2927, 28, -1227.4375, -2072.6006, 48.5686, 2.4710, 60, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8300026 spawntime=60s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200025, 142694, 2927, 28, -1347.2743, -1986.9548, 10.7256, 2.5669, 60, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001141 spawntime=60s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200026, 142694, 2927, 28, -1268.6649, -1897.6580, 18.1191, 3.0474, 60, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001143 spawntime=60s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200027, 142694, 2927, 28, -1309.8303, -1885.6638, 20.1732, 1.6468, 60, 0),  -- sources=wpp_horde phase=inherited from guid 8300025 spawntime=60s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200028, 230001, 2927, 1959, -1710.8169, -1871.7902, 80.0109, 4.3651, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001070 spawntime=300s realm siblings (10 of 10 existing spawns of this entry; consistency, not a measurement)
 (8200029, 230001, 2927, 1959, -1588.3560, -1773.3976, 67.5805, 4.7086, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001075 spawntime=300s realm siblings (10 of 10 existing spawns of this entry; consistency, not a measurement)
 (8200030, 230001, 2927, 1959, -1526.1216, -1880.6476, 67.9007, 3.2279, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001079 spawntime=300s realm siblings (10 of 10 existing spawns of this entry; consistency, not a measurement)
 (8200031, 230001, 2927, 1959, -1522.1365, -1880.3027, 68.0341, 3.1869, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001079 spawntime=300s realm siblings (10 of 10 existing spawns of this entry; consistency, not a measurement)
 (8200032, 230001, 2927, 1959, -1634.3187, -1885.8677, 80.3016, 1.3158, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001072 spawntime=300s realm siblings (10 of 10 existing spawns of this entry; consistency, not a measurement)
 (8200033, 230001, 2927, 1959, -1634.4882, -1892.0950, 80.6018, 1.6804, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001072 spawntime=300s realm siblings (10 of 10 existing spawns of this entry; consistency, not a measurement)
 (8200034, 230004, 2927, 1610, -1575.3142, -1873.6389, 68.6289, 3.6062, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000145 spawntime=300s realm siblings (5 of 5 existing spawns of this entry; consistency, not a measurement)
 (8200035, 231219, 2927, 1959, -1561.0514, -1854.4442, 67.5802, 6.0264, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001080 spawntime=300s realm siblings (1 of 1 existing spawns of this entry; consistency, not a measurement)
 (8200036, 231287, 2927, 1959, -1344.9010, -1788.2517, 50.7579, 2.0799, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8300030 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200037, 231287, 2927, 1959, -1362.2205, -1808.9462, 61.9221, 2.8182, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001081 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200038, 231287, 2927, 1959, -1334.6285, -1799.2084, 61.9687, 3.4121, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8300030 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200039, 231309, 2927, 1959, -1514.9115, -1842.7830, 69.0335, 0.9121, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001082 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200040, 234884, 2927, 0, -1377.6406, -1559.1598, 51.8490, 5.9519, 300, 0),  -- sources=rpe_h3_proj phase=DEFAULT (no neighbour) spawntime=300s FALLBACK -- no measured cadence and no realm sibling to inherit from
 (8200041, 234884, 2927, 0, -1339.4209, -1550.3236, 55.1162, 5.7540, 300, 0),  -- sources=rpe_h3_proj phase=DEFAULT (no neighbour) spawntime=300s FALLBACK -- no measured cadence and no realm sibling to inherit from
 (8200042, 234884, 2927, 0, -1348.5781, -1577.8785, 54.3975, 1.9368, 300, 0),  -- sources=rpe_h3_proj+wpp_alliance2 phase=DEFAULT (no neighbour) spawntime=300s FALLBACK -- no measured cadence and no realm sibling to inherit from
 (8200043, 234884, 2927, 0, -1413.3445, -1579.4824, 50.9536, 0.0651, 300, 0),  -- sources=rpe_h3_proj phase=DEFAULT (no neighbour) spawntime=300s FALLBACK -- no measured cadence and no realm sibling to inherit from
 (8200044, 244671, 2927, 1961, -1024.5676, -3492.6140, 62.2881, 1.9298, 60, 0),  -- sources=wpp_horde phase=inherited from guid 8000022 spawntime=60s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200045, 244672, 2927, 1961, -960.0677, -3510.1440, 57.0754, 3.3031, 60, 0),  -- sources=wpp_alliance+wpp_alliance2+wpp_alliance3 phase=inherited from guid 8000205 spawntime=60s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200046, 244682, 2927, 28, -1211.0911, -1924.6592, 89.9463, 0.6272, 60, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000162 spawntime=60s realm siblings (12 of 12 existing spawns of this entry; consistency, not a measurement)
 (8200047, 244682, 2927, 28, -1272.6553, -1744.4030, 54.8045, 4.5671, 60, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000171 spawntime=60s realm siblings (12 of 12 existing spawns of this entry; consistency, not a measurement)
 (8200048, 244682, 2927, 28, -1372.9844, -1770.0330, 61.7927, 2.7351, 60, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000079 spawntime=60s realm siblings (12 of 12 existing spawns of this entry; consistency, not a measurement)
 (8200049, 244683, 2927, 28, -1257.2750, -1661.6400, 48.0991, 2.8342, 60, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000133 spawntime=60s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200050, 244683, 2927, 28, -1333.1406, -1804.9791, 61.8244, 1.3773, 60, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000176 spawntime=60s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200051, 244683, 2927, 28, -1245.6160, -1643.4630, 48.0943, 5.0648, 60, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000133 spawntime=60s realm siblings (7 of 7 existing spawns of this entry; consistency, not a measurement)
 (8200052, 244690, 2927, 1610, -1310.9116, -1676.6160, 51.8776, 2.1874, 300, 0),  -- sources=wpp_alliance2+wpp_alliance3 phase=inherited from guid 8000077 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200053, 244690, 2927, 1610, -1464.7189, -1794.4075, 67.6618, 0.8789, 300, 0),  -- sources=wpp_alliance2+wpp_alliance3 phase=inherited from guid 8300046 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200054, 244690, 2927, 1610, -1462.6290, -1797.0267, 67.1950, 1.2856, 300, 0),  -- sources=wpp_alliance2+wpp_alliance3 phase=inherited from guid 8300046 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200055, 244690, 2927, 1610, -1312.1323, -1656.1215, 53.3354, 5.5975, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000078 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200056, 244690, 2927, 1610, -1314.0745, -1658.9636, 52.8979, 5.8544, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000076 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200057, 244690, 2927, 1610, -1427.4785, -1987.3053, 44.2941, 1.6341, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000075 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200058, 244690, 2927, 1610, -1411.5309, -1968.1030, 50.6217, 3.7830, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000072 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200059, 244690, 2927, 1610, -1290.2808, -1663.8817, 52.5375, 3.3398, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000076 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200060, 244690, 2927, 1610, -1430.5547, -1965.6328, 47.5978, 5.3759, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000075 spawntime=300s realm siblings (8 of 8 existing spawns of this entry; consistency, not a measurement)
 (8200061, 244695, 2927, 28, -1417.2076, -1965.2820, 49.1634, 0.0688, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8300055 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200062, 244695, 2927, 28, -1330.1991, -1869.5917, 61.8718, 1.2816, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8300056 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200063, 244695, 2927, 28, -1318.7188, -1665.6024, 51.8927, 5.0599, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000080 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200064, 244695, 2927, 28, -1358.2916, -1937.5365, 58.2608, 1.1043, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8300056 spawntime=300s realm siblings (3 of 3 existing spawns of this entry; consistency, not a measurement)
 (8200065, 244711, 2927, 28, -845.6597, -2025.1423, 56.2242, 3.7937, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000111 spawntime=120s realm siblings (36 of 36 existing spawns of this entry; consistency, not a measurement)
 (8200066, 244711, 2927, 28, -884.6042, -2096.9653, 61.3475, 2.1356, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000093 spawntime=120s realm siblings (36 of 36 existing spawns of this entry; consistency, not a measurement)
 (8200067, 244785, 2927, 28, -1024.3142, -1979.8680, 60.8178, 1.3496, 120, 0),  -- sources=wpp_alliance2+wpp_alliance3 phase=inherited from guid 8000120 spawntime=120s realm siblings (5 of 5 existing spawns of this entry; consistency, not a measurement)
 (8200068, 244785, 2927, 28, -1003.8351, -1999.3021, 59.9970, 2.6882, 120, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000121 spawntime=120s realm siblings (5 of 5 existing spawns of this entry; consistency, not a measurement)
 (8200069, 244786, 2927, 28, -1016.8507, -1963.2413, 60.7704, 0.8590, 60, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8000127 spawntime=60s realm siblings (6 of 6 existing spawns of this entry; consistency, not a measurement)
 (8200070, 244956, 2927, 1959, -1553.7274, -2959.6580, 14.0230, 0.0000, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8000031 spawntime=300s realm siblings (4 of 4 existing spawns of this entry; consistency, not a measurement)
 (8200071, 249255, 2927, 4, -1517.2344, -3085.9583, 26.7478, 4.6013, 60, 0),  -- sources=wpp_alliance2+wpp_alliance3 phase=inherited from guid 8000041 spawntime=60s realm siblings (5 of 5 existing spawns of this entry; consistency, not a measurement)
 (8200072, 249255, 2927, 4, -1518.9427, -3082.7222, 26.2588, 4.7994, 60, 0),  -- sources=wpp_alliance3+wpp_horde phase=inherited from guid 8000041 spawntime=60s realm siblings (5 of 5 existing spawns of this entry; consistency, not a measurement)
 (8200073, 249351, 2927, 0, -1524.0399, -1854.9740, 69.0330, 3.0474, 300, 0),  -- sources=rpe_h2_proj+rpe_h3_proj+wpp_alliance2 phase=DEFAULT (no neighbour) spawntime=300s FALLBACK -- no measured cadence and no realm sibling to inherit from
 (8200074, 254547, 2927, 1959, -1144.1488, -1890.6250, 79.9442, 3.1416, 300, 0),  -- sources=wpp_alliance2 phase=inherited from guid 8001091 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200075, 254547, 2927, 1959, -1412.1482, -1962.0024, 49.3261, 6.2167, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001091 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200076, 254547, 2927, 1959, -1639.2284, -1780.9406, 80.0089, 2.2064, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001088 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200077, 254547, 2927, 1959, -1619.2238, -1801.6873, 79.8405, 0.0509, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001088 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200078, 254547, 2927, 1959, -1689.7659, -1713.2081, 73.2150, 4.6831, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001094 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200079, 254547, 2927, 1959, -1637.4282, -1806.2694, 80.0095, 4.4193, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001088 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200080, 254547, 2927, 1959, -1646.6788, -1835.1832, 80.0089, 1.1964, 300, 0),  -- sources=wpp_alliance3 phase=inherited from guid 8001088 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)
 (8200081, 254547, 2927, 1610, -1141.7675, -1878.9631, 82.1625, 2.0344, 300, 0);  -- sources=wpp_alliance3 phase=inherited from guid 8000203 spawntime=300s realm siblings (13 of 13 existing spawns of this entry; consistency, not a measurement)

-- ledger (tools/zone_census_report.py artifacts.txt):
-- TCHarvest capture-ARTIFACT + REVIEW ledger
-- ==========================================
-- map=2927 table=creature captures=7
-- 
-- rule 1  every entry a player SUMMONED in any capture is a capture artifact, never a
--         world spawn. Source: the combat log's SPELL_SUMMON events, via each capture's
--         summoned_entries.txt roster. Class-agnostic -- a Hunter's pet, a Shaman's
--         totem and a Warlock's imp all arrive the same way.
-- rule 2  an entry must be seen in 2 capture(s) or more to be emitted. Entries seen in
--         exactly one capture are HELD FOR REVIEW and NEVER deleted: measured on four
--         Arathi captures, the 2+ bucket was 100% real content, but the single-capture
--         bucket holds real content too (Stuck Ogre, Cindy Springstock, the Fightbot).
-- manual  --exclude-entries ADDS to rule 1. It is never the only mechanism.
-- 
-- candidate entries: 39 | emitted: 27 | excluded as artifacts: 0 | held for review: 12
-- 
-- EXCLUDED -- capture artifacts, never emitted (0)
--   (none)
-- 
-- HELD FOR REVIEW -- one capture only, withheld from the SQL but NOT deleted (12)
--   entry 59262    Demonic Gateway                  seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 59271    Demonic Gateway                  seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 89715    Franklin Martin                  seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 95061    Greater Fire Elemental           seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 144961   Akaari's Soul                    seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 158637   Guiding Orb                      seen in only 1 capture (rpe_h2_proj) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 174170   Maw Haunt                        seen in only 1 capture (rpe_h2_proj) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 227773   Travel Duffel                    seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 227774   Field Repair Anvil               seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 230278   Witherbark Raider                seen in only 1 capture (rpe_h3_proj) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 244714   Lady Jaina Proudmoore            seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
--   entry 257072   Gnoll Biter                      seen in only 1 capture (wpp_alliance2) -- HELD FOR REVIEW, not deleted: this bucket also holds real content only one run happened to see
-- 
-- EMITTED -- corroborated content (27)
--   entry 141725   Burning Exile                    corroborated by 4 captures (rpe_a4_proj, rpe_h3_proj, wpp_alliance, wpp_alliance3)
--   entry 141727   Rumbling Exile                   corroborated by 4 captures (rpe_h2_proj, wpp_alliance, wpp_alliance3, wpp_horde)
--   entry 142335   Young Mesa Buzzard               corroborated by 2 captures (wpp_alliance, wpp_horde)
--   entry 142341   Elder Mesa Buzzard               corroborated by 3 captures (wpp_alliance, wpp_alliance3, wpp_horde)
--   entry 142342   Vicious Black Bear               corroborated by 5 captures (rpe_h2_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 142347   Wild Horse                       corroborated by 2 captures (wpp_alliance, wpp_alliance2)
--   entry 142566   Drywhisker Kobold                corroborated by 4 captures (rpe_a4_proj, rpe_h3_proj, wpp_alliance2, wpp_horde)
--   entry 142694   Boulderfist Enforcer             corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 230001   Stromgarde Orphan                corroborated by 4 captures (rpe_h3_proj, wpp_alliance, wpp_alliance3, wpp_horde)
--   entry 230004   Beggar                           corroborated by 5 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance2, wpp_horde)
--   entry 231219   Stromgarde Citizen               corroborated by 2 captures (wpp_alliance2, wpp_horde)
--   entry 231287   Stromgarde Stonemason            corroborated by 5 captures (rpe_a4_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 231309   Stromic Engineer                 corroborated by 6 captures (rpe_a4_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 234884   Highlands Lumberjack             corroborated by 2 captures (rpe_h3_proj, wpp_alliance2)
--   entry 244671   Gnoll Ripper                     corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244672   Gnoll Bruiser                    corroborated by 5 captures (rpe_h2_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244682   Kobold Waxmancer                 corroborated by 5 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance2, wpp_horde)
--   entry 244683   Gnoll Prowler                    corroborated by 6 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244690   Stromgarde Footman               corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244695   Ettin Crusher                    corroborated by 2 captures (wpp_alliance2, wpp_horde)
--   entry 244711   Armored Cleaver                  corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244785   Armored Cleaver                  corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244786   Gnoll Charger                    corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 244956   Prized Pumpkin                   corroborated by 5 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance2, wpp_horde)
--   entry 249255   Kobold Pillager                  corroborated by 7 captures (rpe_a4_proj, rpe_h2_proj, rpe_h3_proj, wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
--   entry 249351   Fightbot Version 11.2.7          corroborated by 3 captures (rpe_h2_proj, rpe_h3_proj, wpp_alliance2)
--   entry 254547   Stromgarde Footman               corroborated by 4 captures (wpp_alliance, wpp_alliance2, wpp_alliance3, wpp_horde)
-- 
-- combat-log summon rosters read:
--   wpp_alliance             6      entries  out\wpp_alliance\zone_2927\combatlog_summoned_entries.txt
--   wpp_alliance2            13     entries  out\wpp_alliance2\zone_2927\combatlog_summoned_entries.txt
--   wpp_alliance3            2      entries  out\wpp_alliance3\zone_2927\combatlog_summoned_entries.txt
--   wpp_horde                1      entries  out\wpp_horde\zone_2927\combatlog_summoned_entries.txt
--   rpe_h2_proj              3      entries  out\rpe_h2_proj\zone_2927\combatlog_summoned_entries.txt
--   rpe_a4_proj              13     entries  out\rpe_a4_proj\zone_2927\combatlog_summoned_entries.txt
--   rpe_h3_proj              1      entries  out\rpe_h3_proj\zone_2927\combatlog_summoned_entries.txt
