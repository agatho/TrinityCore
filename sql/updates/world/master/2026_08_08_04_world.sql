-- Wild battle-pet spawns recovered from retail 12.0.7 sniffs (feature/pet-battles)
-- Sources mined:
--   C:/sniff/b_pets12.0.7.pkt (dedicated battle-pet capture, realm 3883, map 0
--     Elwynn Forest -> Duskwood corridor; SMSG opcodes compatible 68275..68974)  -> 31 spawn points
--   dump_12.0.7.68974_2026-08-07_21-54-14.pkt                                    -> 0 wild-pet creates
--   dump_12.0.7.68974_2026-08-08_02-54-06.pkt (linformi-shop-key)                -> 0 wild-pet creates
--   dump_12.0.7.68974_delve.pkt (shadowmoon delve)                               -> 0 wild-pet creates
-- Method: SMSG_UPDATE_OBJECT create blocks whose GUID128 entry is in the 1120-entry
--   creature_template.type=14 wild-pet list; position = 4 floats at moverGuid+16
--   (MoveIndex/flags/flags2/MoveTime precede x,y,z,o). Sightings deduped by GUID
--   spawn counter, then clustered per entry within 5 yd (wild pets wander); the
--   cluster centroid is kept as the spawn point. 45 create reads -> 31 unique
--   creatures -> 31 spawn points. Mining scripts: C:/dumps/mine_wildpets_*.py,
--   notes: C:/dumps/WILDPETS_MIDNIGHT_MINE.md.

SET @CGUID := 9100000;

-- Creature
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+30;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `VerifiedBuild`) VALUES
(@CGUID+0, 61071, 0, 0, 0, '0', 0, 0, 0, 0, -10116.1074, 372.5624, 25.3465, 2.3451, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+1, 61071, 0, 0, 0, '0', 0, 0, 0, 0, -10036.9365, 433.689, 25.0377, 2.5509, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+2, 61071, 0, 0, 0, '0', 0, 0, 0, 0, -10033.377, 407.3526, 26.7229, 4.939, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+3, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -10046.7715, 160.4433, 28.0217, 4.3687, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+4, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -10016.9263, 154.536, 34.2566, 2.4611, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+5, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9834.4443, 33.1531, 31.5575, 4.6835, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+6, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9534.5684, 300.4945, 53.2899, 5.7548, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+7, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9362.5791, 250.6715, 63.6632, 2.2983, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+8, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9168.568, 472.3054, 104.3422, 4.7124, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+9, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9146.6383, 420.951, 94.2842, 4.2324, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+10, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9138.3851, 356.1178, 91.718, 2.2334, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+11, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -9076.6178, 380.9993, 92.562, 4.3695, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+12, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -8325.0645, 467.8264, 123.4492, 4.4977, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+13, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -8102.9658, 506.4588, 119.336, 0.0962, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+14, 61080, 0, 0, 0, '0', 0, 0, 0, 0, -8075.4746, 519.543, 118.5309, 0.6021, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+15, 61081, 0, 0, 0, '0', 0, 0, 0, 0, -9464.0049, 322.6101, 53.577, 3.6446, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+16, 61081, 0, 0, 0, '0', 0, 0, 0, 0, -8263.9199, 504.8769, 119.9567, 0.9904, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+17, 61081, 0, 0, 0, '0', 0, 0, 0, 0, -8080.5684, 435.959, 127.2043, 3.7843, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+18, 61143, 0, 0, 0, '0', 0, 0, 0, 0, -11150.751, 242.2912, 38.3512, 2.9867, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+19, 61143, 0, 0, 0, '0', 0, 0, 0, 0, -11010.9717, 219.958, 27.1959, 3.2658, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+20, 61165, 0, 0, 0, '0', 0, 0, 0, 0, -9950.998, -7.8566, 34.0685, 1.7614, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+21, 61165, 0, 0, 0, '0', 0, 0, 0, 0, -9745.5947, 318.1782, 44.9754, 1.604, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+22, 61165, 0, 0, 0, '0', 0, 0, 0, 0, -9242.1074, 249.0106, 71.4421, 5.4259, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+23, 61165, 0, 0, 0, '0', 0, 0, 0, 0, -9181.835, 414.1699, 89.4263, 1.2696, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+24, 61253, 0, 0, 0, '0', 0, 0, 0, 0, -10688.374, 76.593, 39.5603, 1.153, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+25, 61257, 0, 0, 0, '0', 0, 0, 0, 0, -10686.1816, 238.2191, 41.8843, 5.2407, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+26, 61257, 0, 0, 0, '0', 0, 0, 0, 0, -10345.7305, 245.12, 35.4894, 2.914, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+27, 61258, 0, 0, 0, '0', 0, 0, 0, 0, -11123.5928, 144.5677, 26.0957, 4.9171, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+28, 61258, 0, 0, 0, '0', 0, 0, 0, 0, -11108.9131, 103.072, 29.0787, 3.8786, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+29, 61258, 0, 0, 0, '0', 0, 0, 0, 0, -10974.9893, 283.4061, 28.9281, 4.1198, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974), -- wild battle pet (sniff: b_pets12.0.7)
(@CGUID+30, 110826, 0, 0, 0, '0', 0, 0, 0, 0, -9870.5684, 92.9841, 32.3032, 5.6119, 120, 5, 0, 1, NULL, NULL, NULL, NULL, 68974); -- wild battle pet (sniff: b_pets12.0.7)
