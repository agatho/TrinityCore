-- ================================================================
-- HOUSING SNIFF DATA - Map 2736 (Razorwind Shores)
-- Source: housing intro + map.pkt | Build: V11_2_7_64797
-- ================================================================

-- ================================================================
-- CREATURE TEMPLATES
-- ================================================================

-- Tocho Cloudhide (233708)
DELETE FROM `creature_template` WHERE `entry`=233708;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`ScriptName`,`VerifiedBuild`) VALUES
(233708,'Tocho Cloudhide','Steward','speak',7,0,0,8,3,-3,'npc_housing_steward',64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=233708;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(233708,0,131513,1,1,64797);

-- Lyssabel Dawnpetal (233063) - Alliance steward, bind script
UPDATE `creature_template` SET `ScriptName`='npc_housing_steward' WHERE `entry`=233063;

-- The Last Architect (253596)
DELETE FROM `creature_template` WHERE `entry`=253596;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(253596,'The Last Architect','','',7,0,0,1,3,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=253596;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(253596,0,136070,1,1,64797);

-- Rotha (254687)
DELETE FROM `creature_template` WHERE `entry`=254687;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(254687,'Rotha','General Contractor','',7,0,0,1,1,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=254687;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(254687,0,138649,1,1,64797);

-- Haleth Turnwater (255125)
DELETE FROM `creature_template` WHERE `entry`=255125;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(255125,'Haleth Turnwater','Dye Crafter','',7,0,0,1,81,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=255125;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(255125,0,138654,1,1,64797);

-- Botanist Boh\'an (255301)
DELETE FROM `creature_template` WHERE `entry`=255301;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(255301,'Botanist Boh\'an','Vendor','',7,0,0,1,5,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=255301;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(255301,0,138755,1,1,64797);

-- Xiz\'ro (255520)
DELETE FROM `creature_template` WHERE `entry`=255520;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(255520,'Xiz\'ro','Lumberjack','',7,0,0,2,0,10,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=255520;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(255520,0,139022,1,1,64797);

-- Saga Mistrunner (255684)
DELETE FROM `creature_template` WHERE `entry`=255684;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(255684,'Saga Mistrunner','Meat Vendor','',7,0,0,1,4,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=255684;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(255684,0,140446,1,1,64797);

-- Trak Tuskbender (227894) - Flightmaster
DELETE FROM `creature_template` WHERE `entry`=227894;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(227894,'Trak Tuskbender','Flightmaster','',7,0,1,1,8200,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=227894;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(227894,0,139634,1,1,64797);

-- Wuls (227896) - Flightmaster
DELETE FROM `creature_template` WHERE `entry`=227896;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(227896,'Wuls','Flightmaster','',7,0,1,1,8200,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=227896;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(227896,0,139640,1,1,64797);

-- Luk\'gra (227900) - Flightmaster
DELETE FROM `creature_template` WHERE `entry`=227900;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(227900,'Luk\'gra','Flightmaster','',7,0,1,1,8200,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=227900;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(227900,0,107684,1,1,64797);

-- Reginald Glarestone (227901) - Flightmaster
DELETE FROM `creature_template` WHERE `entry`=227901;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(227901,'Reginald Glarestone','Flightmaster','',7,0,1,1,8200,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=227901;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(227901,0,139641,1,1,64797);

-- Zurlik (227902) - Flightmaster
DELETE FROM `creature_template` WHERE `entry`=227902;
INSERT INTO `creature_template` (`entry`,`name`,`subname`,`IconName`,`type`,`family`,`Classification`,`unit_class`,`npcflag`,`RequiredExpansion`,`VerifiedBuild`) VALUES
(227902,'Zurlik','Flightmaster','',7,0,1,1,8200,0,64797);
DELETE FROM `creature_template_model` WHERE `CreatureID`=227902;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(227902,0,139643,1,1,64797);

-- ================================================================
-- CREATURE SPAWNS (1279 on Map 2736)
-- ================================================================

-- Cleanup previous import (guid range 9000000-9001278)
DELETE FROM `creature` WHERE `guid` BETWEEN 9000000 AND 9001278;

-- Orgrimmar Brave (156697) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000000,156697,2736,'0',1789.158000,177.057300,100.879326,0.135454,120,64797);

-- Thrauna (227801) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000001,227801,2736,'0',2007.878500,130.784730,189.535780,5.554801,120,64797);

-- Broktar (227878) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000002,227878,2736,'0',1621.125000,207.593750,99.216606,5.080862,120,64797);

-- Trak Tuskbender (227894) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000003,227894,2736,'0',898.791700,-778.015600,16.688467,0.593842,120,64797);

-- Wuls (227896) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000004,227896,2736,'0',1069.187500,-171.154510,61.454094,2.093889,120,64797);

-- Luk'gra (227900) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000005,227900,2736,'0',619.781250,272.734380,89.083030,1.778863,120,64797);

-- Reginald Glarestone (227901) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000006,227901,2736,'0',1157.687500,609.743040,19.120180,2.180213,120,64797);

-- Zurlik (227902) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000007,227902,2736,'0',1796.033000,955.527800,78.219500,0.059102,120,64797);

-- Dead Meat (233124) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000008,233124,2736,'0',1784.521500,210.385210,104.519070,5.239374,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000009,233124,2736,'0',519.477400,216.921880,100.258410,5.703447,120,64797);

-- Tocho Cloudhide (233708) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000010,233708,2736,'0',1715.359400,95.104164,102.282540,0.483991,120,64797);

-- Generic - Empty Bunny (234427) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000011,234427,2736,'0',1727.435800,191.236110,100.032010,1.566816,120,64797);

-- Snake (234993) x38
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000012,234993,2736,'0',2037.216600,141.958950,179.034520,3.019906,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000013,234993,2736,'0',1783.001100,210.339870,104.445300,0.459294,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000014,234993,2736,'0',1751.153100,236.703900,106.104164,5.291511,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000015,234993,2736,'0',1787.073900,153.388280,100.004395,0.218517,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000016,234993,2736,'0',1632.488400,271.501900,86.095420,2.742818,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000017,234993,2736,'0',1782.900000,210.576690,104.550240,3.714092,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000018,234993,2736,'0',1787.169900,155.932900,100.131280,3.286667,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000019,234993,2736,'0',1579.753400,217.412700,80.002030,0.905046,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000020,234993,2736,'0',1784.521500,210.385210,104.519070,5.239374,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000021,234993,2736,'0',1784.038600,155.136120,99.971820,4.045245,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000022,234993,2736,'0',1722.466800,89.615234,102.788440,3.107405,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000023,234993,2736,'0',1589.731800,19.926565,91.583300,0.978863,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000024,234993,2736,'0',1649.817300,-50.222590,73.383130,0.831580,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000025,234993,2736,'0',1570.453500,-56.279804,77.834550,5.474463,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000026,234993,2736,'0',1434.632100,-57.362230,64.283455,1.312189,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000027,234993,2736,'0',1309.973300,6.753864,70.974380,1.218989,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000028,234993,2736,'0',1319.635600,-124.590836,73.799240,6.260861,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000029,234993,2736,'0',1150.619100,-77.229290,75.758300,0.083070,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000030,234993,2736,'0',1170.799900,-109.468796,74.121870,4.423857,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000031,234993,2736,'0',1112.416000,-46.400390,72.263380,2.467104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000032,234993,2736,'0',1111.170500,-44.483047,72.131360,3.583805,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000033,234993,2736,'0',1052.326700,-175.507830,58.666782,4.547644,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000034,234993,2736,'0',1135.942900,-230.729430,52.107780,3.461931,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000035,234993,2736,'0',1118.753900,-250.676940,50.248530,4.137303,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000036,234993,2736,'0',822.916000,-350.000000,40.079437,2.156537,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000037,234993,2736,'0',486.979250,248.000840,99.924225,0.817560,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000038,234993,2736,'0',683.530600,305.728900,80.520164,1.259868,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000039,234993,2736,'0',927.572400,541.340500,108.756516,3.651536,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000040,234993,2736,'0',1185.654800,619.367400,15.816275,4.384137,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000041,234993,2736,'0',1155.099500,650.744200,14.008656,0.234472,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000042,234993,2736,'0',1411.238600,924.620240,24.101545,2.338622,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000043,234993,2736,'0',1554.361900,924.569700,25.178722,4.077038,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000044,234993,2736,'0',1606.449200,909.264650,32.517754,1.069015,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000045,234993,2736,'0',1643.180000,1021.152800,40.545070,0.699614,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000046,234993,2736,'0',1754.299900,983.100400,47.505630,5.160136,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000047,234993,2736,'0',1745.967200,917.121100,50.734695,3.140475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000048,234993,2736,'0',1990.970700,424.447270,169.585630,5.014692,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000049,234993,2736,'0',2037.141600,141.871100,178.985840,1.811510,120,64797);

-- Wild Boar (234995) x25
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000050,234995,2736,'0',1384.267600,-59.850246,73.680900,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000051,234995,2736,'0',1382.252700,-93.433890,71.366684,4.707384,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000052,234995,2736,'0',1377.883900,-51.452680,72.855880,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000053,234995,2736,'0',1371.147700,-104.948560,71.505554,0.848321,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000054,234995,2736,'0',1070.420400,-125.746070,57.789610,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000055,234995,2736,'0',1126.494400,-185.470630,61.534218,4.233808,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000056,234995,2736,'0',1071.786700,-144.375270,57.776962,4.650429,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000057,234995,2736,'0',1120.093100,-197.315100,57.994843,6.101452,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000058,234995,2736,'0',1126.812700,-198.549900,58.485780,3.573142,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000059,234995,2736,'0',1063.549000,-141.092210,57.941654,4.290079,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000060,234995,2736,'0',1036.845700,-209.294920,57.215515,3.806928,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000061,234995,2736,'0',1063.723600,-216.949620,55.918260,2.903223,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000062,234995,2736,'0',1022.866760,-218.725860,57.730698,5.819538,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000063,234995,2736,'0',975.000000,-209.375000,60.525288,0.664718,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000064,234995,2736,'0',455.690060,235.828500,101.768680,0.285283,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000065,234995,2736,'0',534.382750,358.177900,96.527985,3.221588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000066,234995,2736,'0',567.867100,335.451480,96.734270,0.690504,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000067,234995,2736,'0',1205.025000,716.276400,6.377256,4.093607,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000068,234995,2736,'0',1200.854700,709.363950,5.886191,4.168748,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000069,234995,2736,'0',1406.569300,849.118900,35.285000,5.507455,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000070,234995,2736,'0',1359.785300,854.861600,32.907190,1.580216,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000071,234995,2736,'0',1383.830100,871.097660,33.447388,3.573142,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000072,234995,2736,'0',1817.653900,976.349850,78.407650,1.008783,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000073,234995,2736,'0',1835.052400,894.800900,84.091210,2.053912,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000074,234995,2736,'0',1840.258800,901.289400,83.481970,2.958204,120,64797);

-- Domesticated Raptor (234997) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000075,234997,2736,'0',973.310240,-151.510010,59.083557,2.409136,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000076,234997,2736,'0',967.521500,-157.025390,58.263190,3.902808,120,64797);

-- Desert Tallstrider (234999) x16
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000077,234999,2736,'0',1310.285500,-26.516527,74.043790,3.298037,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000078,234999,2736,'0',1324.347200,-2.451199,70.534150,3.265828,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000079,234999,2736,'0',1347.636400,-10.035530,69.506096,1.685899,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000080,234999,2736,'0',1279.010600,-70.180110,75.217995,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000081,234999,2736,'0',1281.576300,18.143345,101.912670,0.955986,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000082,234999,2736,'0',1260.490200,-102.331110,78.061630,0.376946,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000083,234999,2736,'0',1182.646900,-0.686159,104.079285,1.165336,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000084,234999,2736,'0',1134.588700,-118.710846,75.990524,0.485478,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000085,234999,2736,'0',1170.420300,-154.703280,73.119190,3.274690,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000086,234999,2736,'0',1118.718400,-133.623750,73.116170,1.618491,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000087,234999,2736,'0',1142.830600,-130.436500,74.182526,2.394946,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000088,234999,2736,'0',1183.539400,-163.482500,74.250694,3.284665,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000089,234999,2736,'0',1086.047000,-78.887620,74.887940,4.352768,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000090,234999,2736,'0',1085.793600,-96.055110,74.607056,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000091,234999,2736,'0',1141.588600,-115.018080,75.223270,2.687383,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000092,234999,2736,'0',1159.770600,-150.676510,73.001740,5.090798,120,64797);

-- Wetlands Tallstrider (235000) x31
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000093,235000,2736,'0',817.080260,-592.182500,3.721202,5.702503,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000094,235000,2736,'0',785.376650,-618.887760,2.417070,2.644741,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000095,235000,2736,'0',919.631400,-752.738000,12.099814,2.514506,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000096,235000,2736,'0',978.589700,-772.873400,9.605464,2.514506,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000097,235000,2736,'0',776.573400,-614.104500,4.178835,0.039554,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000098,235000,2736,'0',834.700560,-603.743960,2.626739,1.727588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000099,235000,2736,'0',728.690200,-593.777040,0.790868,6.038680,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000100,235000,2736,'0',647.089050,-453.614620,1.390649,4.586468,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000101,235000,2736,'0',679.912350,-483.820500,1.995071,1.509290,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000102,235000,2736,'0',663.614900,-403.552860,2.896747,2.701542,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000103,235000,2736,'0',654.572270,-403.152340,2.881463,4.077095,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000104,235000,2736,'0',585.557700,-227.204670,10.647784,3.953417,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000105,235000,2736,'0',552.618900,-206.743070,7.348624,2.095196,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000106,235000,2736,'0',1268.244600,669.758240,28.445667,1.327238,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000107,235000,2736,'0',1257.111700,664.032650,28.970050,5.537278,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000108,235000,2736,'0',1205.691700,653.715150,13.385013,0.917825,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000109,235000,2736,'0',1172.053000,660.078250,12.780240,1.804816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000110,235000,2736,'0',1123.675800,670.704830,4.772389,2.491712,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000111,235000,2736,'0',1115.934300,657.691960,5.289134,4.369500,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000112,235000,2736,'0',1276.562500,683.101440,23.266962,1.570796,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000113,235000,2736,'0',1378.848100,795.282500,34.371593,3.752724,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000114,235000,2736,'0',1370.698600,839.847800,36.098015,1.146420,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000115,235000,2736,'0',1394.498800,860.957950,35.263510,3.068419,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000116,235000,2736,'0',1401.791300,803.582500,34.984306,1.107231,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000117,235000,2736,'0',1475.649200,873.059700,35.326930,2.884235,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000118,235000,2736,'0',1398.879900,912.719600,24.118551,2.533044,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000119,235000,2736,'0',1392.504900,910.093570,24.710667,1.241900,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000120,235000,2736,'0',1525.474700,932.634500,23.183895,5.924440,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000121,235000,2736,'0',1531.081700,983.275100,35.819546,3.596027,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000122,235000,2736,'0',1512.112000,986.725160,26.200184,1.107149,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000123,235000,2736,'0',1607.629600,944.819900,34.260880,1.508256,120,64797);

-- Scorpion (235001) x26
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000124,235001,2736,'0',2072.112000,166.060880,175.213620,4.171510,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000125,235001,2736,'0',1902.080900,274.569520,128.461210,4.367480,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000126,235001,2736,'0',1884.836000,324.732400,121.565100,1.031423,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000127,235001,2736,'0',1812.189700,254.587420,117.622740,2.014136,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000128,235001,2736,'0',1742.372900,125.595690,99.948680,0.152302,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000129,235001,2736,'0',1714.124600,150.288180,99.948680,5.133905,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000130,235001,2736,'0',1684.620000,197.943270,99.402306,5.378732,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000131,235001,2736,'0',1643.940700,219.333280,95.265150,4.092234,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000132,235001,2736,'0',1812.789000,256.497000,117.572830,5.790560,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000133,235001,2736,'0',1715.404800,151.349030,99.948670,4.591208,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000134,235001,2736,'0',1739.419400,125.633804,99.957100,3.042587,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000135,235001,2736,'0',1814.539300,255.557360,117.591720,5.790891,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000136,235001,2736,'0',1644.139500,219.138090,95.335050,1.095942,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000137,235001,2736,'0',1742.006000,125.402440,99.948680,0.492893,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000138,235001,2736,'0',1695.152800,54.961807,99.707420,2.665842,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000139,235001,2736,'0',1822.856800,57.072575,106.532590,3.920529,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000140,235001,2736,'0',914.136900,-193.415500,44.165170,1.066844,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000141,235001,2736,'0',1249.417500,611.650100,30.392246,4.738882,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000142,235001,2736,'0',1110.559700,608.991640,13.916642,0.888677,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000143,235001,2736,'0',1823.826700,978.799200,79.302370,2.671990,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000144,235001,2736,'0',1835.416000,941.667000,76.696945,3.141593,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000145,235001,2736,'0',1888.234000,912.500000,92.063770,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000146,235001,2736,'0',1848.122100,885.671450,83.708560,2.089678,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000147,235001,2736,'0',1888.500000,912.500000,92.112946,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000148,235001,2736,'0',1919.485700,944.193700,101.602630,5.117783,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000149,235001,2736,'0',2069.923000,171.380740,175.097870,2.104997,120,64797);

-- Beetle (235003) x46
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000150,235003,2736,'0',1952.160500,184.296250,153.237090,0.570520,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000151,235003,2736,'0',1908.808500,225.824950,133.984830,5.806621,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000152,235003,2736,'0',1822.019800,180.588330,107.678830,0.939456,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000153,235003,2736,'0',1820.731700,179.590910,107.713684,3.278954,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000154,235003,2736,'0',1616.033400,193.162640,98.110825,0.875734,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000155,235003,2736,'0',1677.236000,329.261320,106.104164,0.140905,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000156,235003,2736,'0',1826.362500,179.452440,107.653990,5.539864,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000157,235003,2736,'0',1616.273100,192.909850,98.104910,4.787868,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000158,235003,2736,'0',1778.065900,89.970210,100.318470,5.192949,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000159,235003,2736,'0',1792.739500,7.616118,97.589920,4.361654,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000160,235003,2736,'0',1560.318200,-22.925495,91.016720,0.807494,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000161,235003,2736,'0',1620.855700,-71.632090,72.400470,5.661573,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000162,235003,2736,'0',1252.122800,-112.012955,74.965220,3.217840,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000163,235003,2736,'0',1258.140600,-59.181640,76.142380,1.454429,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000164,235003,2736,'0',1143.635400,-143.192640,73.101130,2.362920,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000165,235003,2736,'0',1120.538200,-124.198100,76.234695,1.859822,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000166,235003,2736,'0',1049.539600,-116.716770,59.517140,0.385236,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000167,235003,2736,'0',1250.206400,-111.604330,74.582924,0.689988,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000168,235003,2736,'0',1258.816200,-57.342636,75.900930,1.218396,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000169,235003,2736,'0',1050.095200,-116.491470,59.514565,0.384940,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000170,235003,2736,'0',1074.429900,-222.040650,54.356930,3.784446,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000171,235003,2736,'0',1018.954650,-207.152310,58.712830,5.246638,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000172,235003,2736,'0',975.298400,-108.664856,58.477250,5.496081,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000173,235003,2736,'0',982.219300,-237.750500,61.816372,2.727042,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000174,235003,2736,'0',1146.126500,-144.967160,73.052840,2.239445,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000175,235003,2736,'0',975.988460,-109.356270,58.504757,5.495931,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000176,235003,2736,'0',917.687000,-251.130370,58.094490,1.351883,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000177,235003,2736,'0',852.862730,-176.469010,31.952614,0.013671,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000178,235003,2736,'0',810.730470,-275.583980,13.887195,3.658443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000179,235003,2736,'0',750.413200,-147.746540,2.073792,3.399849,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000180,235003,2736,'0',815.074650,-484.883700,11.496687,5.951168,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000181,235003,2736,'0',660.288760,-454.911220,2.157776,5.244558,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000182,235003,2736,'0',716.644100,-445.093750,3.257624,6.058682,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000183,235003,2736,'0',584.361150,-116.774310,3.480008,3.763076,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000184,235003,2736,'0',611.175350,-90.171875,1.463647,3.849862,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000185,235003,2736,'0',581.590760,14.086303,3.246161,2.945500,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000186,235003,2736,'0',618.418400,55.852432,5.172324,5.999639,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000187,235003,2736,'0',652.978500,282.332030,5.919773,2.119253,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000188,235003,2736,'0',1282.655500,773.838200,34.090885,4.112466,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000189,235003,2736,'0',1317.206800,822.572700,37.301990,0.731508,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000190,235003,2736,'0',1516.153900,946.036250,23.066753,3.041511,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000191,235003,2736,'0',1682.372100,989.967040,37.684160,2.793309,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000192,235003,2736,'0',1709.549600,1072.650900,47.426598,5.479635,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000193,235003,2736,'0',1775.000000,941.667000,54.902700,6.137816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000194,235003,2736,'0',1881.232500,977.241700,101.644670,1.938206,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000195,235003,2736,'0',1951.547700,183.334820,153.366990,4.117276,120,64797);

-- Vulture (235005) x7
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000196,235005,2736,'0',697.276060,303.819460,134.597440,5.142754,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000197,235005,2736,'0',707.564300,299.225700,132.501110,3.484894,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000198,235005,2736,'0',1823.905300,862.673700,131.900540,3.541083,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000199,235005,2736,'0',1900.458400,1039.404500,162.681260,3.148383,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000200,235005,2736,'0',1896.666600,1035.446200,162.681260,3.160763,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000201,235005,2736,'0',1903.404500,1035.446200,162.681260,4.295913,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000202,235005,2736,'0',1988.385400,1015.319500,145.382680,1.215769,120,64797);

-- Kodo (235009) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000203,235009,2736,'0',1061.489700,-29.080343,55.632458,3.597680,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000204,235009,2736,'0',1033.847700,-100.470020,58.985455,4.734240,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000205,235009,2736,'0',995.798000,-114.668850,59.233130,2.547058,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000206,235009,2736,'0',954.206600,-242.753070,62.150170,5.292699,120,64797);

-- Pack Kodo (235012) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000207,235012,2736,'0',1795.055500,175.543410,100.973785,2.254252,120,64797);

-- Pygmy Hippo (235013) x6
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000208,235013,2736,'0',1146.695600,710.258400,11.415988,1.795952,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000209,235013,2736,'0',1072.877200,619.065400,5.282506,0.198545,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000210,235013,2736,'0',1535.202300,870.681150,27.256968,2.495965,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000211,235013,2736,'0',1481.732200,985.089700,25.124699,2.179121,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000212,235013,2736,'0',1615.907200,924.009460,34.943710,5.652044,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000213,235013,2736,'0',1685.407000,892.288000,38.236030,5.670119,120,64797);

-- Crab (235014) x11
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000214,235014,2736,'0',784.655000,-608.732060,4.465671,4.599647,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000215,235014,2736,'0',775.625100,-609.546400,4.769772,2.053169,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000216,235014,2736,'0',781.677250,-635.032300,-0.070226,4.584597,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000217,235014,2736,'0',745.119140,-587.572270,4.114441,5.547851,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000218,235014,2736,'0',702.821500,-515.258800,1.695694,3.438333,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000219,235014,2736,'0',650.375800,-490.516850,0.695836,5.065191,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000220,235014,2736,'0',616.727200,-431.357880,0.007600,3.600485,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000221,235014,2736,'0',608.096300,-358.853520,1.817186,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000222,235014,2736,'0',578.732540,-3.361288,4.171527,4.474456,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000223,235014,2736,'0',495.043820,55.822166,-0.144496,4.291373,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000224,235014,2736,'0',606.249760,77.294785,-0.097730,1.866828,120,64797);

-- Fox (235021) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000225,235021,2736,'0',519.322400,217.020420,100.302510,5.716979,120,64797);

-- Turtle (235442) x39
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000226,235442,2736,'0',808.163100,-256.058350,15.939484,5.890302,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000227,235442,2736,'0',768.465800,-189.641920,15.638256,1.911525,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000228,235442,2736,'0',734.477660,-266.450780,15.741155,4.731918,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000229,235442,2736,'0',665.887100,-163.517150,2.952404,1.907361,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000230,235442,2736,'0',637.040200,-173.007750,3.566934,1.781681,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000231,235442,2736,'0',641.233760,-174.783360,3.564964,1.862412,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000232,235442,2736,'0',643.912200,-250.547970,9.391308,5.840045,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000233,235442,2736,'0',808.506160,-249.215710,15.914862,4.678824,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000234,235442,2736,'0',843.984400,-604.148130,2.029648,0.653670,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000235,235442,2736,'0',902.781250,-731.964840,2.896396,3.182142,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000236,235442,2736,'0',868.422400,-858.221800,1.945535,6.003060,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000237,235442,2736,'0',901.880200,-730.666700,2.720839,2.177555,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000238,235442,2736,'0',901.563400,-728.411600,2.997261,1.571925,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000239,235442,2736,'0',941.125700,-711.437300,1.787615,5.096165,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000240,235442,2736,'0',640.056000,-400.751370,0.977164,2.574927,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000241,235442,2736,'0',696.736500,-357.510220,2.440419,3.246783,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000242,235442,2736,'0',735.713750,-264.751460,15.598906,0.406747,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000243,235442,2736,'0',638.971200,-167.245790,3.973838,2.056473,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000244,235442,2736,'0',562.506530,-204.172930,5.335384,5.490152,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000245,235442,2736,'0',628.809400,-42.917500,-0.002847,4.786218,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000246,235442,2736,'0',593.644500,-36.754890,3.591759,0.812419,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000247,235442,2736,'0',558.724600,5.486328,0.019305,2.902058,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000248,235442,2736,'0',501.424350,-57.419525,1.629804,0.206026,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000249,235442,2736,'0',487.863300,93.019394,5.964436,1.295714,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000250,235442,2736,'0',467.850460,72.743670,3.593430,5.092171,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000251,235442,2736,'0',645.208200,232.660220,3.712506,1.122969,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000252,235442,2736,'0',1195.177700,711.131600,6.311307,0.058853,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000253,235442,2736,'0',1200.080100,706.747100,5.707723,0.406245,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000254,235442,2736,'0',1091.138900,683.575900,7.718424,1.741735,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000255,235442,2736,'0',1072.475500,629.912540,7.454681,3.005287,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000256,235442,2736,'0',1071.201000,634.778700,8.607494,3.991872,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000257,235442,2736,'0',1519.206900,880.532170,29.444190,0.608276,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000258,235442,2736,'0',1495.983200,952.452700,22.222752,4.071082,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000259,235442,2736,'0',1474.955300,945.885500,24.119186,4.832953,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000260,235442,2736,'0',1525.483400,984.933350,33.984270,2.355960,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000261,235442,2736,'0',1693.375000,1041.134600,48.683537,3.048766,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000262,235442,2736,'0',1708.736200,966.660700,37.581480,6.267562,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000263,235442,2736,'0',1700.219400,1042.555900,47.780704,4.976543,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000264,235442,2736,'0',1710.924900,969.319460,37.512825,1.096232,120,64797);

-- Squirrel (235453) x3
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000265,235453,2736,'0',580.969360,342.765350,95.345130,4.022401,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000266,235453,2736,'0',887.645100,458.643100,108.381890,2.358029,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000267,235453,2736,'0',849.390260,483.308620,107.404460,4.687980,120,64797);

-- Frog (235455) x7
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000268,235455,2736,'0',1105.252400,692.152650,9.829833,6.154504,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000269,235455,2736,'0',1062.476100,599.773600,4.070558,6.136046,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000270,235455,2736,'0',1350.788700,787.681150,39.441013,0.926709,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000271,235455,2736,'0',1376.546100,810.653300,34.833580,2.950852,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000272,235455,2736,'0',1375.026600,871.014950,33.238506,4.560408,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000273,235455,2736,'0',1622.596800,1050.346900,46.251514,1.261621,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000274,235455,2736,'0',1724.707800,966.028100,36.147297,2.479712,120,64797);

-- Dead Fish (236820) x19
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000275,236820,2736,'0',698.935800,-160.125000,0.548824,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000276,236820,2736,'0',666.031250,-267.149320,1.880416,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000277,236820,2736,'0',714.461800,-302.803830,0.466905,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000278,236820,2736,'0',850.187500,-623.312500,1.020580,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000279,236820,2736,'0',863.840300,-852.505200,1.106821,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000280,236820,2736,'0',751.230900,-614.456600,1.274499,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000281,236820,2736,'0',692.126800,-512.817700,1.307193,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000282,236820,2736,'0',623.406250,-435.553830,1.301332,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000283,236820,2736,'0',550.934000,-161.940980,1.403687,5.878976,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000284,236820,2736,'0',539.076400,-74.329865,0.802224,5.878976,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000285,236820,2736,'0',572.885440,-31.246529,1.971977,5.878976,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000286,236820,2736,'0',495.920140,-44.772570,1.239042,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000287,236820,2736,'0',518.833300,65.449650,1.115129,5.173443,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000288,236820,2736,'0',1162.715300,695.604200,5.628295,3.481018,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000289,236820,2736,'0',1085.116300,666.590300,5.660009,3.481018,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000290,236820,2736,'0',1116.118000,657.694460,5.425770,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000291,236820,2736,'0',1373.086800,703.993040,24.000347,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000292,236820,2736,'0',1542.755200,884.079900,23.716763,4.449783,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000293,236820,2736,'0',1478.128500,938.975700,22.655708,5.173443,120,64797);

-- Tourist (237063) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000294,237063,2736,'0',2005.198000,120.980896,190.418030,5.324151,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000295,237063,2736,'0',1988.329300,178.257050,161.179440,4.254543,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000296,237063,2736,'0',2034.476700,130.131150,182.719280,1.493500,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000297,237063,2736,'0',1613.345200,297.726560,81.203300,5.018996,120,64797);

-- Local (237581) x38
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000298,237581,2736,'0',2038.389400,233.215400,185.964480,2.278045,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000299,237581,2736,'0',1942.372800,200.195430,148.998950,2.423864,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000300,237581,2736,'0',1888.892500,139.583980,107.800735,3.141593,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000301,237581,2736,'0',1877.366200,140.187620,107.800735,2.583141,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000302,237581,2736,'0',1870.629300,146.037770,107.784940,2.356194,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000303,237581,2736,'0',1812.018700,277.144070,120.538710,1.816661,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000304,237581,2736,'0',1765.954100,203.262010,101.885670,5.335234,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000305,237581,2736,'0',1775.618700,155.207050,99.956510,1.490560,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000306,237581,2736,'0',1866.426900,150.000000,107.574900,3.141593,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000307,237581,2736,'0',1710.413300,155.653380,99.948680,1.883772,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000308,237581,2736,'0',1722.395900,128.746540,99.905205,4.320198,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000309,237581,2736,'0',1724.725700,126.519100,99.912560,3.638561,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000310,237581,2736,'0',1692.803500,169.456570,103.204810,1.816661,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000311,237581,2736,'0',1724.045200,127.505210,99.872330,3.647290,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000312,237581,2736,'0',1773.025300,107.880780,100.388245,3.056228,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000313,237581,2736,'0',1785.661000,202.767490,103.342150,0.824770,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000314,237581,2736,'0',1827.079800,157.889540,107.800730,3.239895,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000315,237581,2736,'0',1776.648200,201.373340,102.164990,3.951848,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000316,237581,2736,'0',1768.398100,87.325340,100.325290,3.439025,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000317,237581,2736,'0',1783.273900,141.283000,99.996290,2.019751,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000318,237581,2736,'0',1802.438100,18.649492,97.682670,5.546973,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000319,237581,2736,'0',823.799800,-799.369800,2.888298,0.955776,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000320,237581,2736,'0',897.400400,-795.022950,16.700798,4.420722,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000321,237581,2736,'0',896.440250,-796.274300,16.700798,0.916321,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000322,237581,2736,'0',902.417660,-777.024900,16.771732,3.509188,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000323,237581,2736,'0',915.184500,-825.787400,10.209974,5.211886,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000324,237581,2736,'0',922.277900,-826.742250,10.921320,2.413125,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000325,237581,2736,'0',916.531250,-828.255550,10.397413,1.364475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000326,237581,2736,'0',916.814600,-826.752800,10.366196,4.526038,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000327,237581,2736,'0',938.178400,-708.110500,0.368294,1.557501,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000328,237581,2736,'0',961.781400,-783.071700,9.149290,2.807830,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000329,237581,2736,'0',997.050840,-799.832460,9.954576,2.819533,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000330,237581,2736,'0',652.009030,254.514740,4.840226,1.566785,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000331,237581,2736,'0',614.978640,213.035690,16.586813,4.866317,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000332,237581,2736,'0',649.880300,239.703700,4.089433,1.468988,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000333,237581,2736,'0',614.553340,207.274170,16.755415,0.189439,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000334,237581,2736,'0',2003.096600,422.496860,173.407940,5.447381,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000335,237581,2736,'0',2039.609600,230.868530,184.945050,5.429382,120,64797);

-- Shrew (238584) x30
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000336,238584,2736,'0',775.287100,-125.945140,2.970536,1.115832,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000337,238584,2736,'0',707.180540,-251.781250,1.734420,6.008405,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000338,238584,2736,'0',722.788400,-124.011116,3.701931,1.371986,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000339,238584,2736,'0',650.566650,-174.590710,3.716605,1.161643,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000340,238584,2736,'0',655.265440,-255.961060,6.466953,2.782116,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000341,238584,2736,'0',751.576400,-421.359380,3.067947,2.314243,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000342,238584,2736,'0',881.822450,-492.293330,10.282349,3.381573,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000343,238584,2736,'0',917.205500,-723.624800,4.397526,4.448451,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000344,238584,2736,'0',886.485960,-876.938350,2.976029,3.897776,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000345,238584,2736,'0',916.807430,-725.097200,4.890431,4.448587,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000346,238584,2736,'0',683.756960,-482.501740,2.312261,5.837296,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000347,238584,2736,'0',684.178830,-425.491330,2.451375,0.724129,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000348,238584,2736,'0',653.228500,-323.482670,3.470839,2.406565,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000349,238584,2736,'0',583.702760,-321.661770,6.396862,1.287335,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000350,238584,2736,'0',708.715450,-255.010640,2.310539,4.549686,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000351,238584,2736,'0',548.434000,106.597220,4.726935,0.828858,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000352,238584,2736,'0',459.041660,75.611115,5.019565,0.973255,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000353,238584,2736,'0',617.157300,323.062320,93.005455,5.498703,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000354,238584,2736,'0',542.242200,322.527340,98.067375,5.129514,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000355,238584,2736,'0',1147.090600,717.926000,13.330574,4.043898,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000356,238584,2736,'0',1259.292800,752.754300,32.509346,1.218076,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000357,238584,2736,'0',1409.524000,785.920800,35.176390,5.358459,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000358,238584,2736,'0',1462.003500,871.767200,35.169804,4.130134,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000359,238584,2736,'0',1450.203200,954.263400,22.605145,0.063342,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000360,238584,2736,'0',1610.321900,978.759800,36.639183,4.264241,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000361,238584,2736,'0',1511.868800,1005.484300,32.945070,0.056945,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000362,238584,2736,'0',1674.589200,1043.599400,49.062470,3.105007,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000363,238584,2736,'0',1643.493200,959.983460,37.495827,1.736019,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000364,238584,2736,'0',1724.935000,1018.290000,40.964417,0.064058,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000365,238584,2736,'0',1691.210000,909.205600,37.610786,3.106224,120,64797);

-- Seagull (239906) x9
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000366,239906,2736,'0',672.993040,-136.083330,44.614510,5.775690,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000367,239906,2736,'0',638.600770,-180.989600,41.666664,2.601802,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000368,239906,2736,'0',913.145750,-577.659670,32.845550,5.160499,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000369,239906,2736,'0',921.751700,-576.246460,41.666668,2.167366,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000370,239906,2736,'0',770.222200,-696.816000,41.666668,2.212027,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000371,239906,2736,'0',676.919000,-472.500200,41.591606,3.476531,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000372,239906,2736,'0',668.546940,-465.802060,41.666668,0.572806,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000373,239906,2736,'0',680.975650,-471.130200,41.666637,3.458506,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000374,239906,2736,'0',647.097200,-81.440910,41.666683,4.729065,120,64797);

-- Crab (239919) x16
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000375,239919,2736,'0',1946.756800,114.371820,106.502940,5.950872,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000376,239919,2736,'0',711.254330,-175.413500,1.535051,2.781687,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000377,239919,2736,'0',771.543460,-373.564240,4.311274,4.981845,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000378,239919,2736,'0',783.069460,-450.192720,8.665966,4.477862,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000379,239919,2736,'0',820.147600,-557.520800,4.747978,3.384924,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000380,239919,2736,'0',838.786440,-582.958300,1.936518,6.155689,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000381,239919,2736,'0',955.858300,-812.850800,11.887172,5.125293,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000382,239919,2736,'0',819.777000,-557.613400,4.673101,3.384734,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000383,239919,2736,'0',624.795900,-423.387240,3.682505,0.530156,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000384,239919,2736,'0',651.103100,-382.667140,-0.088816,5.062535,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000385,239919,2736,'0',750.486270,-348.591250,-0.098467,3.429722,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000386,239919,2736,'0',583.986940,-184.298770,0.022313,3.902343,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000387,239919,2736,'0',710.958900,-176.211840,1.525644,0.469351,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000388,239919,2736,'0',647.222200,-54.001736,-0.048015,3.676409,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000389,239919,2736,'0',619.003500,-28.121529,4.084237,1.256849,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000390,239919,2736,'0',578.338560,-52.340280,-0.048015,4.713177,120,64797);

-- Wyvern (239987) x61
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000391,239987,2736,'0',1582.330300,239.702200,156.342740,1.431938,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000392,239987,2736,'0',1588.619000,242.400070,172.131960,4.258645,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000393,239987,2736,'0',1584.016600,302.096340,166.646350,5.164343,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000394,239987,2736,'0',1556.447800,213.846300,154.906750,0.390937,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000395,239987,2736,'0',1520.656200,284.772580,171.152270,2.286802,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000396,239987,2736,'0',1514.123300,266.975700,171.307570,3.698885,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000397,239987,2736,'0',1577.075900,227.000630,158.619640,0.859920,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000398,239987,2736,'0',1592.077400,276.077730,170.626510,4.883883,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000399,239987,2736,'0',1535.097300,217.065830,170.942730,3.125427,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000400,239987,2736,'0',1500.338000,215.995450,160.634640,5.838588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000401,239987,2736,'0',1574.470600,316.960600,170.031280,5.413573,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000402,239987,2736,'0',1576.756200,226.637440,158.615740,0.838445,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000403,239987,2736,'0',1569.212000,220.436860,157.574460,0.577110,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000404,239987,2736,'0',1576.777200,226.660840,158.616180,0.839801,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000405,239987,2736,'0',1569.344500,220.523330,157.600160,0.579863,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000406,239987,2736,'0',1299.009000,-233.877530,107.327680,1.055371,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000407,239987,2736,'0',1075.151000,48.906250,141.098280,5.672336,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000408,239987,2736,'0',1045.623300,43.347220,155.211150,4.660232,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000409,239987,2736,'0',998.755800,-85.802025,103.392360,3.922441,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000410,239987,2736,'0',1072.638500,-242.999340,90.444920,5.901126,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000411,239987,2736,'0',1073.704800,86.616320,142.920300,0.507406,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000412,239987,2736,'0',1041.564200,67.623270,155.302080,0.153918,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000413,239987,2736,'0',1190.414700,-286.973330,109.611046,1.600010,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000414,239987,2736,'0',1083.569500,56.321667,167.673780,4.663405,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000415,239987,2736,'0',1190.599700,-294.541100,110.441010,1.588656,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000416,239987,2736,'0',1343.276500,-26.804092,108.374710,2.480718,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000417,239987,2736,'0',1013.379760,-5.641457,154.917600,0.012862,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000418,239987,2736,'0',912.870900,-292.892900,103.331420,1.244860,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000419,239987,2736,'0',1015.274350,-5.610252,154.638720,0.019981,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000420,239987,2736,'0',746.091700,144.824650,137.725830,1.299423,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000421,239987,2736,'0',763.815860,164.706590,144.340670,3.934590,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000422,239987,2736,'0',960.788200,676.322940,58.910473,5.370352,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000423,239987,2736,'0',952.904540,692.642400,64.840880,0.538467,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000424,239987,2736,'0',958.189300,701.187500,60.157803,1.474834,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000425,239987,2736,'0',1322.402800,801.724200,132.128300,5.282182,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000426,239987,2736,'0',1322.755200,810.842040,106.528440,3.219485,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000427,239987,2736,'0',1355.595500,834.031250,111.310260,3.219485,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000428,239987,2736,'0',1363.255200,820.234400,111.863174,6.156493,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000429,239987,2736,'0',1341.810800,813.593750,117.455510,4.196419,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000430,239987,2736,'0',1357.444500,812.921900,112.008606,5.597983,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000431,239987,2736,'0',1348.156200,822.756960,116.835500,0.390514,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000432,239987,2736,'0',1341.142300,828.158000,117.100690,1.378071,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000433,239987,2736,'0',1345.679800,790.578600,114.381470,3.309101,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000434,239987,2736,'0',1347.177100,799.295170,108.316345,5.020761,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000435,239987,2736,'0',1337.397600,822.791700,117.448814,2.436912,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000436,239987,2736,'0',1324.289700,795.749760,120.374140,2.569262,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000437,239987,2736,'0',1369.974900,811.462700,129.364720,1.248589,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000438,239987,2736,'0',1708.814100,1074.990000,116.005936,2.366098,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000439,239987,2736,'0',1853.380600,905.633670,134.940380,4.236090,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000440,239987,2736,'0',1788.234400,857.059270,197.232300,0.457097,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000441,239987,2736,'0',1642.546300,1004.178800,114.266300,0.310752,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000442,239987,2736,'0',1965.594500,1039.336500,192.263780,3.270183,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000443,239987,2736,'0',1881.643000,1055.688000,194.036820,0.239308,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000444,239987,2736,'0',1892.490700,1056.726900,197.400960,6.216426,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000445,239987,2736,'0',1896.386000,1056.221100,199.043760,6.092418,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000446,239987,2736,'0',1985.645000,1040.713400,188.380420,3.134147,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000447,239987,2736,'0',2145.015600,963.559000,175.687940,4.098806,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000448,239987,2736,'0',2135.212000,971.319460,176.664350,3.087814,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000449,239987,2736,'0',2077.954800,827.975340,188.448600,0.383496,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000450,239987,2736,'0',2154.826400,925.468750,183.455690,3.087814,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000451,239987,2736,'0',1921.651700,473.453060,173.086500,0.587114,120,64797);

-- Hermit Crab (240353) x11
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000452,240353,2736,'0',683.555540,-147.737850,-0.048015,4.227715,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000453,240353,2736,'0',787.961800,-590.413200,5.975839,0.434764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000454,240353,2736,'0',883.592960,-541.872740,2.164174,4.750496,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000455,240353,2736,'0',949.754460,-750.237730,10.293237,3.005037,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000456,240353,2736,'0',713.695700,-587.177300,-0.567304,5.890286,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000457,240353,2736,'0',676.736150,-350.704860,-0.048015,4.815926,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000458,240353,2736,'0',591.722960,-245.592990,9.072603,0.821491,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000459,240353,2736,'0',620.655400,-154.053360,3.396674,2.310756,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000460,240353,2736,'0',682.806760,-149.159910,-0.085900,4.227715,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000461,240353,2736,'0',517.512900,-117.941460,3.359137,0.202371,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000462,240353,2736,'0',586.953100,83.220490,-0.048015,3.049261,120,64797);

-- Harbormaster Thorendar (240396) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000463,240396,2736,'0',898.071170,-770.784700,16.700798,1.951155,120,64797);

-- Grumblehoof (240460) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000464,240460,2736,'0',907.333300,-792.227400,9.691792,3.569760,120,64797);

-- Lonomia (240465) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000465,240465,2736,'0',902.982670,-791.109400,9.089315,4.627273,120,64797);

-- Rancher Nazra (240470) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000466,240470,2736,'0',969.624760,-152.741040,58.943120,0.301370,120,64797);

-- Riding Hyena (240592) x3
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000467,240592,2736,'0',973.196170,-116.880210,58.730680,2.606492,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000468,240592,2736,'0',975.894100,-115.189240,58.705940,2.214679,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000469,240592,2736,'0',966.975700,-114.645840,58.568108,0.947048,120,64797);

-- Neighborhood Resident (240593) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000470,240593,2736,'0',966.402800,-113.993060,58.417274,5.608757,120,64797);

-- Akuta (240603) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000471,240603,2736,'0',969.524300,-110.975690,58.598495,5.934413,120,64797);

-- Itsero (240610) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000472,240610,2736,'0',973.116330,-111.949650,58.689552,2.902203,120,64797);

-- Taiyata (240611) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000473,240611,2736,'0',971.935800,-110.019100,58.598938,4.534246,120,64797);

-- Seniku (240613) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000474,240613,2736,'0',970.611150,-113.446180,58.708523,1.213957,120,64797);

-- Neighborhood Watch (240712) x6
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000475,240712,2736,'0',865.407650,-197.487880,34.206387,4.800427,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000476,240712,2736,'0',855.195400,-265.333800,38.442062,0.116158,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000477,240712,2736,'0',1165.885000,611.322200,19.551080,2.890421,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000478,240712,2736,'0',1172.796300,639.376700,13.806080,4.010072,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000479,240712,2736,'0',1463.279000,955.061100,23.011482,1.712831,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000480,240712,2736,'0',1613.751100,903.198850,34.447567,5.898534,120,64797);

-- Lizard (242103) x34
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000481,242103,2736,'0',1893.341100,247.837660,130.809920,5.693044,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000482,242103,2736,'0',1621.180500,238.730260,89.694275,0.782636,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000483,242103,2736,'0',1621.881800,239.427730,89.796470,0.782636,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000484,242103,2736,'0',1479.186300,-8.025130,78.199690,3.030240,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000485,242103,2736,'0',1310.572100,-45.395770,73.897730,0.147007,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000486,242103,2736,'0',1406.164200,-78.013410,65.573060,0.924504,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000487,242103,2736,'0',1022.987600,-74.382280,56.190224,2.227119,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000488,242103,2736,'0',877.238100,-221.097640,37.510204,1.198085,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000489,242103,2736,'0',796.804570,-189.300490,14.597450,0.872263,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000490,242103,2736,'0',877.207900,-219.298540,37.375180,4.170774,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000491,242103,2736,'0',777.114260,-311.298830,3.954106,2.433161,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000492,242103,2736,'0',862.342000,-390.383150,36.231377,1.510205,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000493,242103,2736,'0',851.428830,-507.225700,8.131765,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000494,242103,2736,'0',796.645800,-529.522600,12.033903,2.391288,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000495,242103,2736,'0',817.227400,-611.197940,2.498052,4.432312,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000496,242103,2736,'0',757.256700,-608.898250,3.318295,3.022202,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000497,242103,2736,'0',757.242200,-608.896500,3.315111,3.022202,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000498,242103,2736,'0',794.534060,-527.554100,11.751413,2.391000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000499,242103,2736,'0',616.166000,-349.734380,4.516839,3.143546,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000500,242103,2736,'0',646.260440,-117.107640,2.269335,4.169876,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000501,242103,2736,'0',551.505600,-151.770170,4.404188,0.800592,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000502,242103,2736,'0',543.773400,-210.331070,8.285590,4.724107,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000503,242103,2736,'0',549.998300,-82.197914,4.052084,3.865800,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000504,242103,2736,'0',516.335100,81.873270,3.407813,3.934880,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000505,242103,2736,'0',1185.999300,754.432700,17.491238,6.181429,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000506,242103,2736,'0',1078.927000,648.024000,4.983488,3.348184,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000507,242103,2736,'0',1075.000900,578.805540,13.845022,3.239415,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000508,242103,2736,'0',1341.996000,642.775270,38.921867,3.118749,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000509,242103,2736,'0',1224.478500,776.521700,18.171783,4.748714,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000510,242103,2736,'0',1349.564000,852.894170,33.197410,2.881721,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000511,242103,2736,'0',1408.929200,847.836700,35.349335,3.153474,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000512,242103,2736,'0',1584.346800,951.185800,37.807144,1.114613,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000513,242103,2736,'0',1890.289900,1042.607700,114.443540,5.368926,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000514,242103,2736,'0',2014.977700,378.201420,183.602430,6.226828,120,64797);

-- Toad (242122) x18
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000515,242122,2736,'0',1277.172900,-89.457010,75.637790,3.926991,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000516,242122,2736,'0',1221.069500,-144.446060,73.976110,5.637993,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000517,242122,2736,'0',1087.928100,-76.653160,74.987260,1.374167,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000518,242122,2736,'0',1054.793800,-44.593770,56.858940,3.472100,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000519,242122,2736,'0',1275.051400,-91.582730,75.917725,3.928566,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000520,242122,2736,'0',1088.379500,-78.670290,75.218216,4.451563,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000521,242122,2736,'0',1088.393700,-77.632890,75.132910,4.301032,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000522,242122,2736,'0',847.233300,-254.713320,37.276980,3.152670,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000523,242122,2736,'0',540.625000,257.291020,99.209145,1.107149,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000524,242122,2736,'0',591.235200,289.482900,91.306015,0.575728,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000525,242122,2736,'0',1174.796600,674.591500,4.347327,3.999354,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000526,242122,2736,'0',1220.833000,702.084000,4.905191,0.413473,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000527,242122,2736,'0',1218.818500,699.924740,4.905191,3.961649,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000528,242122,2736,'0',1295.446400,714.939900,22.081657,1.468652,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000529,242122,2736,'0',1393.423300,737.689400,22.200000,5.590505,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000530,242122,2736,'0',1439.003500,935.812500,22.160423,3.441206,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000531,242122,2736,'0',1540.758900,900.853400,21.056230,3.989151,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000532,242122,2736,'0',1716.506600,1060.532800,44.587727,4.022223,120,64797);

-- Neighborhood Watch (242171) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000533,242171,2736,'0',1597.553800,-49.892360,73.677185,3.324703,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000534,242171,2736,'0',1596.350700,-49.387154,73.619420,3.103584,120,64797);

-- Neighborhood Watch (243112) x15
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000535,243112,2736,'0',1744.281200,113.571180,99.988556,6.230178,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000536,243112,2736,'0',1681.609400,176.704860,99.321440,1.370423,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000537,243112,2736,'0',1611.876700,-52.081596,73.749070,5.594851,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000538,243112,2736,'0',1531.611100,44.854168,175.790420,3.986856,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000539,243112,2736,'0',925.637150,-779.447940,38.791298,1.015782,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000540,243112,2736,'0',917.362850,-799.770800,38.790276,3.607594,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000541,243112,2736,'0',910.670170,-803.798650,9.344308,4.149322,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000542,243112,2736,'0',507.552100,207.267360,130.953520,5.872610,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000543,243112,2736,'0',490.741330,197.093750,130.954900,3.923928,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000544,243112,2736,'0',483.371520,219.131940,101.731770,2.324828,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000545,243112,2736,'0',846.947940,442.506960,141.159210,3.955411,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000546,243112,2736,'0',862.942700,465.218750,112.095620,0.679735,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000547,243112,2736,'0',1147.286500,666.347200,9.317199,4.893990,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000548,243112,2736,'0',1165.604100,609.342040,19.530993,1.429888,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000549,243112,2736,'0',1711.552100,885.546900,33.403310,3.693548,120,64797);

-- Kodo (Gray) (243880) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000550,243880,2736,'0',1627.430500,-14.449653,74.289100,5.887147,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000551,243880,2736,'0',1630.682300,-39.024307,72.897220,2.572879,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000552,243880,2736,'0',1644.206700,-31.954860,73.176070,1.920858,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000553,243880,2736,'0',1664.272600,-44.585070,73.458680,5.886289,120,64797);

-- Riding Kodo (Olive) (243881) x3
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000554,243881,2736,'0',1624.895900,-74.034720,71.718030,0.165650,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000555,243881,2736,'0',1647.152800,-69.126740,72.890990,4.837951,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000556,243881,2736,'0',1640.816000,-71.803820,72.883200,5.339116,120,64797);

-- Raccoon (244166) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000557,244166,2736,'0',513.327500,228.027830,101.462510,1.757619,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000558,244166,2736,'0',894.233950,492.438750,108.909940,2.831611,120,64797);

-- Blocker [DNT] (244697) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000559,244697,2736,'0',1488.673700,820.851750,37.842686,5.341284,120,64797);

-- Vilnea (244932) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000560,244932,2736,'0',1489.330100,821.368040,37.672220,5.341284,120,64797);

-- Wagon (244934) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000561,244934,2736,'0',1487.589800,823.724600,35.600254,5.341284,120,64797);

-- Neighborhood Laborer (244938) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000562,244938,2736,'0',1488.269400,820.557600,37.642685,5.341284,120,64797);

-- Alpaca (244944) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000563,244944,2736,'0',1490.332200,817.763850,35.600254,5.341284,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000564,244944,2736,'0',1492.416600,819.280150,35.600254,5.341284,120,64797);

-- Seagull (244951) x56
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000565,244951,2736,'0',704.977400,-242.885420,15.466590,2.882692,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000566,244951,2736,'0',706.123300,-240.302080,15.324635,2.100150,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000567,244951,2736,'0',715.467040,-273.288200,15.505303,4.657816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000568,244951,2736,'0',705.979200,-246.531250,15.523233,3.577588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000569,244951,2736,'0',711.538200,-269.937500,15.417453,3.577588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000570,244951,2736,'0',628.840300,-271.996520,53.468517,2.346854,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000571,244951,2736,'0',703.251800,-330.111100,15.929522,4.112943,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000572,244951,2736,'0',724.013900,-341.559020,31.360382,2.917086,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000573,244951,2736,'0',724.925350,-339.536470,30.964132,2.542981,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000574,244951,2736,'0',703.987850,-324.505220,13.823455,1.752355,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000575,244951,2736,'0',627.555540,-276.430570,54.435093,4.101017,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000576,244951,2736,'0',726.039900,-347.876740,32.813760,3.577588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000577,244951,2736,'0',754.322940,-363.079860,33.456047,5.646888,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000578,244951,2736,'0',749.970500,-364.892360,33.005928,4.758352,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000579,244951,2736,'0',726.562500,-349.857640,33.239117,3.725496,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000580,244951,2736,'0',881.739560,-638.151060,2.094584,3.380653,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000581,244951,2736,'0',885.039900,-641.078100,1.956072,5.029403,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000582,244951,2736,'0',790.883670,-684.432300,10.248186,1.365157,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000583,244951,2736,'0',808.852400,-708.098940,7.357458,2.419024,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000584,244951,2736,'0',815.100700,-712.798650,6.872221,4.797836,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000585,244951,2736,'0',817.119800,-710.428830,7.051193,0.301330,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000586,244951,2736,'0',795.321170,-686.255200,11.577570,0.704745,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000587,244951,2736,'0',816.237850,-707.821170,7.175407,0.921272,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000588,244951,2736,'0',795.840300,-687.609400,11.945617,0.301330,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000589,244951,2736,'0',810.692700,-705.218750,6.894477,2.212666,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000590,244951,2736,'0',781.138900,-685.142400,8.705304,2.687505,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000591,244951,2736,'0',786.121500,-683.307300,10.184114,1.406741,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000592,244951,2736,'0',785.684000,-692.517400,9.834737,4.631711,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000593,244951,2736,'0',777.862850,-689.178830,7.637939,2.939188,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000594,244951,2736,'0',793.821170,-685.279540,10.924392,1.279533,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000595,244951,2736,'0',787.836800,-683.744800,10.286352,1.788156,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000596,244951,2736,'0',796.409700,-689.755200,11.890090,6.035228,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000597,244951,2736,'0',782.447940,-691.923650,9.458502,4.455342,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000598,244951,2736,'0',907.097200,-783.232670,51.158283,2.594419,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000599,244951,2736,'0',797.276400,-822.949700,32.032990,3.196923,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000600,244951,2736,'0',800.680800,-823.323550,29.419945,3.665765,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000601,244951,2736,'0',797.882600,-814.863700,29.425394,3.124011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000602,244951,2736,'0',919.942700,-773.826400,40.351673,2.155249,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000603,244951,2736,'0',921.611150,-803.845500,40.343628,4.705626,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000604,244951,2736,'0',910.241330,-789.751800,36.147713,3.049591,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000605,244951,2736,'0',918.223940,-801.604200,39.402683,4.565200,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000606,244951,2736,'0',896.364560,-868.246500,26.001892,4.032222,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000607,244951,2736,'0',819.266400,-864.899400,51.079353,4.636892,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000608,244951,2736,'0',909.694460,-870.989560,25.983185,5.146550,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000609,244951,2736,'0',898.586800,-870.652800,26.212639,4.192480,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000610,244951,2736,'0',932.006960,-780.126800,40.327248,1.294984,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000611,244951,2736,'0',704.446170,-533.057300,11.670181,3.540347,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000612,244951,2736,'0',704.576400,-535.194460,11.681487,3.405662,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000613,244951,2736,'0',701.159700,-529.059000,10.809827,3.253475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000614,244951,2736,'0',656.156250,-585.401060,8.560139,5.539850,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000615,244951,2736,'0',701.262150,-524.730900,11.648883,3.409538,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000616,244951,2736,'0',653.128500,-581.213560,7.904939,2.538833,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000617,244951,2736,'0',655.295170,-586.451400,8.672384,5.376138,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000618,244951,2736,'0',666.618040,-107.088540,3.094433,5.039702,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000619,244951,2736,'0',670.697940,-103.970490,3.264559,5.620257,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000620,244951,2736,'0',672.262150,-102.888890,3.080637,5.640308,120,64797);

-- Rat (244973) x40
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000621,244973,2736,'0',2026.642100,181.242220,175.159420,3.567729,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000622,244973,2736,'0',2057.920700,209.298070,175.860660,3.753872,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000623,244973,2736,'0',2010.413000,243.512500,191.501900,2.774082,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000624,244973,2736,'0',2017.709000,125.000000,188.757140,5.466699,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000625,244973,2736,'0',1981.567300,282.833470,185.473560,6.205039,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000626,244973,2736,'0',1840.532000,229.408230,117.558914,1.289467,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000627,244973,2736,'0',1827.407500,307.047600,121.068800,2.620366,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000628,244973,2736,'0',1855.535800,286.038300,119.342070,1.227161,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000629,244973,2736,'0',1754.127800,325.248780,106.104164,4.515779,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000630,244973,2736,'0',1839.433600,225.285460,117.636340,4.346212,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000631,244973,2736,'0',1683.986100,-18.814203,75.029970,5.885432,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000632,244973,2736,'0',1604.389800,59.252274,102.365200,2.912794,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000633,244973,2736,'0',1538.495700,-71.796480,77.722720,4.227944,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000634,244973,2736,'0',1508.123700,6.912491,78.209600,2.504246,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000635,244973,2736,'0',1375.165300,-52.272210,72.938790,1.608864,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000636,244973,2736,'0',1225.976000,-77.315414,74.407540,0.909958,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000637,244973,2736,'0',1216.968000,-18.771263,101.710630,5.820004,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000638,244973,2736,'0',1143.278300,-22.632812,75.283264,0.602086,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000639,244973,2736,'0',1110.658200,-182.843750,59.513874,1.726637,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000640,244973,2736,'0',1077.179400,-10.306416,57.033295,2.319246,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000641,244973,2736,'0',1083.440400,-158.411830,57.970660,0.068619,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000642,244973,2736,'0',1185.990500,-192.574600,73.872010,5.013183,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000643,244973,2736,'0',1143.062600,-24.253483,75.143510,0.893311,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000644,244973,2736,'0',1080.251600,-156.902880,58.418884,3.324990,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000645,244973,2736,'0',979.824650,-188.262190,58.592140,4.250713,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000646,244973,2736,'0',1004.666700,-138.460070,58.761900,5.665878,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000647,244973,2736,'0',942.104300,-228.037840,61.765858,0.203832,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000648,244973,2736,'0',887.904540,-145.784760,26.056847,5.308275,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000649,244973,2736,'0',1207.866000,654.159300,13.380354,5.958333,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000650,244973,2736,'0',1283.288200,661.986800,30.810250,5.535779,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000651,244973,2736,'0',1816.497800,919.473450,70.514890,6.018444,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000652,244973,2736,'0',1862.617900,1008.666560,98.651760,4.611210,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000653,244973,2736,'0',2005.517000,321.433930,181.781680,1.151139,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000654,244973,2736,'0',1980.680700,284.871100,184.984120,3.774552,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000655,244973,2736,'0',2010.602000,243.983870,191.454970,3.923540,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000656,244973,2736,'0',2061.071300,210.979810,175.704130,5.393456,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000657,244973,2736,'0',2024.770500,180.228970,175.110900,1.064777,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000658,244973,2736,'0',2018.322500,123.309944,188.645940,6.122931,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000659,244973,2736,'0',2019.144400,122.984400,188.431920,5.393022,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000660,244973,2736,'0',2008.096200,321.762180,182.074770,1.820554,120,64797);

-- Prepared Meat (245021) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000661,245021,2736,'0',969.338560,-161.019100,58.292080,4.200983,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000662,245021,2736,'0',965.817700,-159.032990,58.132310,4.014253,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000663,245021,2736,'0',1004.149300,-124.121530,58.567010,0.395730,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000664,245021,2736,'0',1001.470500,-117.253470,58.755085,0.653724,120,64797);

-- Skimmer (245038) x12
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000665,245038,2736,'0',683.304570,-217.631230,-1.229794,0.157778,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000666,245038,2736,'0',686.280760,-217.157610,-1.268880,0.157700,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000667,245038,2736,'0',713.434000,-319.307280,-0.048015,1.841066,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000668,245038,2736,'0',683.555540,-283.562500,-0.048015,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000669,245038,2736,'0',689.508670,-549.779540,0.057557,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000670,245038,2736,'0',684.377200,-213.605680,-1.253412,5.464602,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000671,245038,2736,'0',515.663200,-49.248264,-0.048015,1.568416,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000672,245038,2736,'0',551.302060,49.024307,-0.048015,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000673,245038,2736,'0',1120.161400,677.043330,4.353733,0.421755,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000674,245038,2736,'0',1356.540900,722.432430,21.464550,4.181012,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000675,245038,2736,'0',1327.083000,741.667000,21.232368,2.993501,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000676,245038,2736,'0',1484.897600,918.340300,21.326944,0.000000,120,64797);

-- Dragonfly (245042) x121
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000677,245042,2736,'0',1983.410500,182.144100,168.615260,0.987528,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000678,245042,2736,'0',2076.399400,91.031235,190.581920,1.093022,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000679,245042,2736,'0',2035.315100,239.980910,196.357510,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000680,245042,2736,'0',1945.693500,207.616320,157.632110,5.873309,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000681,245042,2736,'0',1909.981600,252.692720,142.266680,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000682,245042,2736,'0',1822.030300,281.736100,130.717560,5.873308,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000683,245042,2736,'0',1796.061000,228.919240,118.990710,4.421394,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000684,245042,2736,'0',1783.985200,188.773900,110.941730,3.366338,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000685,245042,2736,'0',1720.238600,232.139080,116.856670,1.622004,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000686,245042,2736,'0',1713.411300,177.518700,109.326996,0.044976,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000687,245042,2736,'0',1676.277700,152.047210,119.222176,1.244964,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000688,245042,2736,'0',1665.572100,269.423220,116.449150,4.482599,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000689,245042,2736,'0',1679.811600,343.350740,116.466150,2.490814,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000690,245042,2736,'0',1664.146400,159.633560,115.608290,6.075488,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000691,245042,2736,'0',1781.710600,184.370800,115.182330,2.679729,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000692,245042,2736,'0',1804.959200,237.772690,120.471420,0.773954,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000693,245042,2736,'0',1669.094800,165.307890,116.002220,2.829435,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000694,245042,2736,'0',1672.521600,268.477170,119.931890,2.949486,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000695,245042,2736,'0',1598.307000,170.131670,116.246370,2.752068,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000696,245042,2736,'0',1673.262200,139.648860,117.523240,1.268153,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000697,245042,2736,'0',1786.107300,193.491490,110.611336,3.517291,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000698,245042,2736,'0',1784.087300,228.756800,122.260155,3.095524,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000699,245042,2736,'0',1604.373300,157.448120,116.876750,5.904419,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000700,245042,2736,'0',1654.053700,36.175526,119.304950,5.842696,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000701,245042,2736,'0',1689.503700,62.084900,116.005260,5.783983,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000702,245042,2736,'0',1782.060700,44.149280,108.524690,4.138476,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000703,245042,2736,'0',1808.555500,17.137127,109.427090,4.138476,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000704,245042,2736,'0',1648.513400,43.425030,117.550140,3.364318,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000705,245042,2736,'0',1675.244400,159.989290,117.634660,4.934448,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000706,245042,2736,'0',1771.781100,180.837460,117.269490,2.826089,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000707,245042,2736,'0',1765.259900,186.743400,116.537740,5.179009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000708,245042,2736,'0',1778.798200,179.310990,109.806140,3.131697,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000709,245042,2736,'0',1767.011600,184.720370,117.142250,5.676484,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000710,245042,2736,'0',1774.770400,179.586840,109.947880,2.990865,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000711,245042,2736,'0',1639.224100,39.123714,118.138470,2.888436,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000712,245042,2736,'0',1721.710800,160.900300,108.084400,4.649210,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000713,245042,2736,'0',1603.435800,18.581596,102.979220,1.093006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000714,245042,2736,'0',1658.309900,-42.600690,81.817550,3.937071,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000715,245042,2736,'0',1576.784800,-10.914936,100.296000,1.093011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000716,245042,2736,'0',1498.548700,7.986103,86.911910,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000717,245042,2736,'0',1455.047500,-12.892350,85.144750,2.433770,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000718,245042,2736,'0',1437.637200,19.977428,85.289650,5.641014,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000719,245042,2736,'0',1295.300400,-58.307300,83.748720,5.641013,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000720,245042,2736,'0',1358.331700,-27.532986,82.450660,1.093006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000721,245042,2736,'0',1266.285400,-13.248262,112.487335,0.987528,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000722,245042,2736,'0',1263.428800,-83.241325,83.681370,5.641002,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000723,245042,2736,'0',1150.928800,-108.671880,85.055580,1.093011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000724,245042,2736,'0',1171.271700,-86.001740,83.712610,2.490815,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000725,245042,2736,'0',1122.898300,-17.843746,81.877280,2.433762,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000726,245042,2736,'0',1094.096300,-116.980896,85.990395,5.873315,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000727,245042,2736,'0',1117.818400,-88.652780,82.060160,5.873301,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000728,245042,2736,'0',1060.308100,-19.897566,63.365140,0.987528,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000729,245042,2736,'0',1050.517300,-81.527800,68.285355,4.138475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000730,245042,2736,'0',1126.067700,-162.460080,68.500330,5.641006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000731,245042,2736,'0',1115.350700,-202.871540,64.100480,1.093010,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000732,245042,2736,'0',963.580570,-197.138890,65.538650,5.873314,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000733,245042,2736,'0',878.871460,-178.703140,50.224354,4.138474,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000734,245042,2736,'0',899.857700,-113.836815,41.114850,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000735,245042,2736,'0',855.588560,-204.647570,45.956955,1.093011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000736,245042,2736,'0',841.624900,-154.723980,43.149757,4.138470,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000737,245042,2736,'0',764.871640,-185.557300,26.737839,5.641012,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000738,245042,2736,'0',851.443500,-287.800350,47.254280,5.873313,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000739,245042,2736,'0',786.884460,-148.060760,15.110541,0.987527,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000740,245042,2736,'0',661.894200,-152.173610,16.776321,5.641014,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000741,245042,2736,'0',649.313350,-218.336800,16.424265,0.987534,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000742,245042,2736,'0',751.221250,-312.060760,14.786829,5.873313,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000743,245042,2736,'0',844.898300,-347.442720,47.693623,2.433758,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000744,245042,2736,'0',853.105900,-410.571200,38.960896,1.093011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000745,245042,2736,'0',846.219670,-539.475700,9.114490,2.490816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000746,245042,2736,'0',777.539900,-555.689300,19.091125,6.184829,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000747,245042,2736,'0',881.125850,-528.541600,12.677644,3.937074,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000748,245042,2736,'0',786.888900,-616.644100,10.235345,6.184829,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000749,245042,2736,'0',956.915470,-711.422700,15.066091,4.644783,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000750,245042,2736,'0',800.383800,-617.975700,10.495538,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000751,245042,2736,'0',791.034800,-557.020900,19.351318,5.641003,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000752,245042,2736,'0',685.652800,-430.152740,18.329683,1.093012,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000753,245042,2736,'0',641.553700,-347.866330,18.429350,4.138475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000754,245042,2736,'0',574.094670,-343.840270,16.587688,2.490813,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000755,245042,2736,'0',613.536400,-177.968770,13.310626,4.138481,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000756,245042,2736,'0',584.349730,-156.321170,15.981576,5.873303,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000757,245042,2736,'0',627.809000,-108.131940,15.547738,5.233588,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000758,245042,2736,'0',543.854900,-189.619780,14.951497,2.433767,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000759,245042,2736,'0',625.309140,-42.883686,14.361240,5.641011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000760,245042,2736,'0',588.163940,46.576380,15.225893,5.873309,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000761,245042,2736,'0',482.895840,75.204865,14.588526,6.184829,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000762,245042,2736,'0',617.289060,65.793410,15.660604,3.937071,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000763,245042,2736,'0',480.356660,223.067700,110.611540,5.873308,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000764,245042,2736,'0',563.710940,290.690980,109.921265,0.987528,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000765,245042,2736,'0',656.935850,314.756930,98.951920,5.641006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000766,245042,2736,'0',594.331670,313.635400,105.562070,5.641011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000767,245042,2736,'0',564.262270,347.158000,107.598010,5.641010,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000768,245042,2736,'0',657.242000,311.548580,15.461667,5.873303,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000769,245042,2736,'0',651.493040,248.737850,13.994226,3.034336,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000770,245042,2736,'0',712.443400,313.942700,85.040306,5.873308,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000771,245042,2736,'0',1189.789000,798.270750,26.700102,2.433766,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000772,245042,2736,'0',1183.473000,713.147600,31.793049,5.873311,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000773,245042,2736,'0',1178.669200,652.701350,24.269150,0.987536,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000774,245042,2736,'0',1248.059000,645.892330,41.516730,4.138474,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000775,245042,2736,'0',1073.504400,561.565900,31.263838,2.490818,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000776,245042,2736,'0',1264.356800,803.518600,45.325000,4.939585,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000777,245042,2736,'0',1421.841900,757.640600,50.668964,4.138465,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000778,245042,2736,'0',1453.634400,781.989600,46.492092,5.873316,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000779,245042,2736,'0',1314.748200,846.852400,42.172916,4.138474,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000780,245042,2736,'0',1552.235000,958.685700,49.083687,2.433768,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000781,245042,2736,'0',1647.514900,902.500500,50.163383,4.893781,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000782,245042,2736,'0',1654.735200,1044.026100,57.642227,5.873315,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000783,245042,2736,'0',1761.726300,945.456400,65.044480,0.357816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000784,245042,2736,'0',1688.975800,895.973500,53.091137,0.576652,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000785,245042,2736,'0',1837.271700,976.715330,98.349815,3.937064,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000786,245042,2736,'0',1880.618800,1008.760400,113.237020,5.873311,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000787,245042,2736,'0',1877.605800,888.770800,100.653100,4.138468,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000788,245042,2736,'0',2009.705700,418.883700,188.702300,2.433757,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000789,245042,2736,'0',1981.500900,434.107670,171.909200,3.937073,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000790,245042,2736,'0',2034.014900,231.222870,202.830780,4.197562,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000791,245042,2736,'0',1973.238800,315.569460,190.837500,2.490813,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000792,245042,2736,'0',1963.136200,187.537630,174.704880,3.871659,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000793,245042,2736,'0',1959.467000,191.023710,172.605500,4.570324,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000794,245042,2736,'0',1961.929700,186.380430,175.561870,3.943948,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000795,245042,2736,'0',1957.399000,188.501450,170.820700,5.889043,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000796,245042,2736,'0',1947.354700,232.924510,158.793730,4.727155,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000797,245042,2736,'0',2023.210000,238.404770,197.224430,3.529709,120,64797);

-- [DNT] Invisible Shop Queue Bunny (245815) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000798,245815,2736,'0',1890.781200,254.854170,130.327420,2.195391,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000799,245815,2736,'0',1719.024300,208.097230,100.423570,1.034332,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000800,245815,2736,'0',1709.739600,207.588550,100.400210,1.578143,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000801,245815,2736,'0',933.465300,-828.201400,10.789916,5.401732,120,64797);

-- Salamander Eft (247210) x21
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000802,247210,2736,'0',737.061340,-144.320160,1.667384,5.255359,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000803,247210,2736,'0',741.020100,-132.107760,2.250086,4.757446,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000804,247210,2736,'0',834.237200,-543.063900,5.892097,2.991520,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000805,247210,2736,'0',851.074800,-531.929140,4.446420,5.269051,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000806,247210,2736,'0',614.717900,-309.233280,8.775085,5.577172,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000807,247210,2736,'0',611.345600,-299.751900,8.306590,0.638849,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000808,247210,2736,'0',564.584000,-108.333984,3.396674,1.842603,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000809,247210,2736,'0',549.197800,-116.867120,5.853502,0.043803,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000810,247210,2736,'0',638.233950,295.780900,12.339788,0.172796,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000811,247210,2736,'0',666.962040,270.830750,3.970936,1.350228,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000812,247210,2736,'0',1289.973000,605.060060,42.047840,0.878100,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000813,247210,2736,'0',1364.604500,695.160770,29.778230,1.676095,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000814,247210,2736,'0',1361.601300,684.434100,34.668343,0.901244,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000815,247210,2736,'0',1350.868800,787.221000,39.494858,0.611325,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000816,247210,2736,'0',1336.242400,838.229500,37.277940,4.973890,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000817,247210,2736,'0',1576.526000,958.691830,37.383034,2.232044,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000818,247210,2736,'0',1589.722800,934.362550,30.258997,2.121189,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000819,247210,2736,'0',1717.361200,1039.372100,46.226574,0.783518,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000820,247210,2736,'0',1716.017700,938.789200,37.975266,0.601406,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000821,247210,2736,'0',1700.430400,1038.223000,47.501488,4.836610,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000822,247210,2736,'0',1727.169400,934.281000,37.696106,3.388345,120,64797);

-- Salamander Eft (247211) x7
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000823,247211,2736,'0',1140.592800,-38.857720,73.831100,4.565281,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000824,247211,2736,'0',1131.198200,-49.421837,74.913870,0.201114,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000825,247211,2736,'0',881.054600,-139.143860,26.023989,0.780561,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000826,247211,2736,'0',804.760700,-371.276520,38.211887,4.243350,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000827,247211,2736,'0',1889.934100,1036.066000,112.201706,0.582869,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000828,247211,2736,'0',1908.666100,1038.297000,116.826030,0.257391,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000829,247211,2736,'0',1926.106900,1041.881800,119.625160,4.984118,120,64797);

-- Salamander (247212) x32
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000830,247212,2736,'0',776.974600,-152.010130,1.855925,2.693620,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000831,247212,2736,'0',711.003500,-158.788910,2.394875,5.527216,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000832,247212,2736,'0',726.889700,-302.142430,1.259815,2.046778,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000833,247212,2736,'0',777.511300,-322.517120,3.521540,4.134405,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000834,247212,2736,'0',804.876000,-439.494140,14.331924,4.448876,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000835,247212,2736,'0',821.850770,-541.745200,5.884141,5.694077,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000836,247212,2736,'0',849.429400,-511.382540,8.248895,1.575719,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000837,247212,2736,'0',816.062130,-532.726900,7.147884,2.143989,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000838,247212,2736,'0',639.158800,-312.055760,5.878233,5.937055,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000839,247212,2736,'0',627.419560,-357.880830,4.043471,5.773312,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000840,247212,2736,'0',664.536440,-318.915220,0.463331,0.409975,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000841,247212,2736,'0',747.378100,-305.016450,2.397392,6.143358,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000842,247212,2736,'0',541.785640,-108.424130,7.621766,5.520332,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000843,247212,2736,'0',699.287230,-143.247160,-0.131855,2.928608,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000844,247212,2736,'0',528.840760,-129.970520,2.435561,4.082158,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000845,247212,2736,'0',640.734600,-34.901028,2.249870,0.886649,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000846,247212,2736,'0',550.463800,-88.764840,5.668781,2.917166,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000847,247212,2736,'0',1157.131300,734.857400,17.272633,5.343346,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000848,247212,2736,'0',1315.440600,629.868300,41.634140,3.171172,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000849,247212,2736,'0',1331.651200,699.174000,34.846840,1.157285,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000850,247212,2736,'0',1324.784400,812.893400,39.772118,1.946367,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000851,247212,2736,'0',1365.592500,796.007450,39.538340,0.033134,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000852,247212,2736,'0',1321.823200,832.408450,38.608160,0.105016,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000853,247212,2736,'0',1407.863300,785.580140,35.404285,2.702450,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000854,247212,2736,'0',1377.110400,866.149800,34.300686,4.005086,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000855,247212,2736,'0',1582.208300,947.976000,37.235620,5.142151,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000856,247212,2736,'0',1658.144800,1011.957760,40.552494,3.587998,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000857,247212,2736,'0',1622.263100,1042.050700,42.972702,4.131587,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000858,247212,2736,'0',1690.832900,995.927500,37.957893,4.818901,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000859,247212,2736,'0',1670.098600,901.839300,36.934135,1.051650,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000860,247212,2736,'0',1714.446500,964.428800,36.183918,0.009634,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000861,247212,2736,'0',1700.525000,1045.966700,47.724360,3.666795,120,64797);

-- Salamander (247214) x18
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000862,247214,2736,'0',1272.817100,-49.822260,76.650260,0.911849,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000863,247214,2736,'0',1231.725000,-34.852497,86.458120,3.178682,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000864,247214,2736,'0',1243.526200,-49.318996,79.843390,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000865,247214,2736,'0',1159.371200,-13.015813,92.267780,4.990555,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000866,247214,2736,'0',1142.187300,-75.710440,73.608430,4.211173,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000867,247214,2736,'0',1116.318500,-26.897087,74.465126,1.557106,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000868,247214,2736,'0',1135.072300,13.085938,73.771560,4.314602,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000869,247214,2736,'0',1082.874100,-14.397728,57.684730,2.365711,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000870,247214,2736,'0',1231.270800,-60.091778,76.088320,3.862720,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000871,247214,2736,'0',1230.241600,-34.907540,86.544266,3.573104,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000872,247214,2736,'0',917.477200,-129.439600,28.550167,3.963027,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000873,247214,2736,'0',909.560100,-127.426150,29.654707,0.598258,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000874,247214,2736,'0',817.097400,-340.699950,40.091800,4.083002,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000875,247214,2736,'0',834.969100,-361.986540,40.488850,0.184492,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000876,247214,2736,'0',730.052730,333.328120,67.091255,3.753938,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000877,247214,2736,'0',1883.148100,964.614140,101.645240,2.135884,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000878,247214,2736,'0',1867.040900,949.484200,102.654650,3.560726,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000879,247214,2736,'0',1898.536400,1054.243800,119.996870,4.166613,120,64797);

-- Moth (247298) x41
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000880,247298,2736,'0',1544.058100,-44.538197,86.522560,2.490817,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000881,247298,2736,'0',1596.625000,-80.694450,85.577080,5.641005,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000882,247298,2736,'0',1518.219400,-83.585045,86.936646,2.433772,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000883,247298,2736,'0',1344.419100,8.399303,73.013016,5.873302,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000884,247298,2736,'0',1387.654500,-78.331590,80.779396,1.093010,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000885,247298,2736,'0',1275.169300,-118.484380,89.778220,2.490816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000886,247298,2736,'0',1353.111900,-134.039920,79.387596,3.937069,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000887,247298,2736,'0',1247.861900,-162.467010,81.643940,3.937069,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000888,247298,2736,'0',1219.024300,-60.345505,84.230194,4.138474,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000889,247298,2736,'0',1060.546000,-142.607620,65.782620,0.987529,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000890,247298,2736,'0',1037.671800,-210.510440,64.687800,4.138466,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000891,247298,2736,'0',1026.158600,-115.368050,66.477960,5.873305,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000892,247298,2736,'0',979.953100,-221.631970,69.903890,4.138475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000893,247298,2736,'0',985.276900,-94.663185,62.442394,3.937069,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000894,247298,2736,'0',951.922600,-109.524284,63.176476,2.433770,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000895,247298,2736,'0',933.395000,-244.201400,69.935326,2.490816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000896,247298,2736,'0',913.646700,-234.076390,64.336670,3.937069,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000897,247298,2736,'0',1213.376700,755.932250,28.896719,1.093017,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000898,247298,2736,'0',1150.154400,634.039860,29.324154,1.093013,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000899,247298,2736,'0',1220.363600,612.234400,32.195940,5.873296,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000900,247298,2736,'0',1120.545900,634.744750,22.787676,3.937070,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000901,247298,2736,'0',1113.634600,589.147600,32.620476,0.987536,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000902,247298,2736,'0',1280.507700,614.531250,49.224790,5.873301,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000903,247298,2736,'0',1279.967500,810.683000,45.332060,4.444820,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000904,247298,2736,'0',1409.289900,831.677100,49.544804,1.093004,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000905,247298,2736,'0',1385.454800,849.361100,48.653038,4.138474,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000906,247298,2736,'0',1484.463500,831.718750,58.794120,1.093012,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000907,247298,2736,'0',1471.132900,886.409700,45.719357,2.490816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000908,247298,2736,'0',1383.912400,909.019040,36.803833,0.987518,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000909,247298,2736,'0',1522.846300,990.782960,43.898144,2.433775,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000910,247298,2736,'0',1560.802000,908.001300,41.034145,2.475101,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000911,247298,2736,'0',1469.157200,949.586800,33.710762,2.490811,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000912,247298,2736,'0',1575.894900,1059.267500,52.394512,2.490805,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000913,247298,2736,'0',1617.750000,1030.868200,50.264100,1.093004,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000914,247298,2736,'0',1756.958300,993.292700,74.385070,3.067464,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000915,247298,2736,'0',1785.769000,995.418700,60.193832,2.306814,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000916,247298,2736,'0',1832.858400,918.921800,91.044330,2.490811,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000917,247298,2736,'0',1901.059100,952.916700,112.168396,5.641007,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000918,247298,2736,'0',1913.216200,911.118040,109.661095,3.937070,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000919,247298,2736,'0',1906.692700,1039.812500,127.024240,3.497470,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000920,247298,2736,'0',1957.694500,1012.223900,111.660545,3.034334,120,64797);

-- Hornet (248557) x122
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000921,248557,2736,'0',2037.607700,137.769100,196.757900,1.093011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000922,248557,2736,'0',1983.878400,253.522550,200.813000,4.138485,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000923,248557,2736,'0',1909.237900,227.397550,141.946060,5.641006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000924,248557,2736,'0',1854.106200,163.578080,120.086235,0.019172,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000925,248557,2736,'0',1853.422900,252.800350,127.891470,0.987519,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000926,248557,2736,'0',1882.638700,280.536400,138.145890,4.138483,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000927,248557,2736,'0',1863.939300,317.192720,131.069340,5.641008,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000928,248557,2736,'0',1728.995600,199.612120,109.910330,1.480689,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000929,248557,2736,'0',1751.318100,128.167270,114.829430,2.201125,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000930,248557,2736,'0',1728.477700,129.886810,118.521940,1.919535,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000931,248557,2736,'0',1669.863000,170.120760,111.357040,3.316079,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000932,248557,2736,'0',1728.093000,112.943436,116.057710,0.611613,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000933,248557,2736,'0',1722.268900,126.506380,116.144750,4.992258,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000934,248557,2736,'0',1749.195200,146.228150,110.226090,5.213057,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000935,248557,2736,'0',1864.387100,178.582240,118.200424,2.720102,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000936,248557,2736,'0',1732.626700,126.917465,113.477680,2.588109,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000937,248557,2736,'0',1670.597500,184.401540,108.671814,5.992053,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000938,248557,2736,'0',1727.241200,132.620030,118.647095,2.116115,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000939,248557,2736,'0',1847.019800,178.596710,118.349050,3.443308,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000940,248557,2736,'0',1649.522600,179.655350,111.971080,0.979939,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000941,248557,2736,'0',1727.828600,129.943220,114.101105,3.315858,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000942,248557,2736,'0',1754.399700,130.783720,112.299340,2.800971,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000943,248557,2736,'0',1746.524400,73.376600,113.630585,5.784027,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000944,248557,2736,'0',1704.048300,45.111332,121.266840,1.473744,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000945,248557,2736,'0',1800.452500,111.011100,120.876830,3.978666,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000946,248557,2736,'0',1756.446200,4.999972,106.465180,4.138476,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000947,248557,2736,'0',1668.579200,36.932606,102.212330,0.217213,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000948,248557,2736,'0',1747.069000,181.612850,110.064420,4.219728,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000949,248557,2736,'0',1661.030000,169.355480,110.693130,3.163694,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000950,248557,2736,'0',1743.926100,187.385010,112.122185,3.234577,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000951,248557,2736,'0',1664.006200,178.373140,111.819010,5.406755,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000952,248557,2736,'0',1744.803300,188.286930,108.520920,4.842968,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000953,248557,2736,'0',1671.643400,183.610060,114.491940,5.991056,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000954,248557,2736,'0',1742.070900,193.748460,111.284100,1.981770,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000955,248557,2736,'0',1724.521200,192.283660,111.807210,4.450419,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000956,248557,2736,'0',1659.724600,172.795530,108.886390,0.913251,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000957,248557,2736,'0',1747.925900,190.139620,108.240140,4.828878,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000958,248557,2736,'0',1751.540300,189.060130,112.488120,3.692798,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000959,248557,2736,'0',1744.618700,184.900160,107.285866,4.328241,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000960,248557,2736,'0',1747.587900,193.812640,112.226000,3.781281,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000961,248557,2736,'0',1786.328100,97.658424,114.410630,2.547011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000962,248557,2736,'0',1669.554600,-61.418440,81.622116,1.984659,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000963,248557,2736,'0',1669.730000,-54.376736,84.115140,3.937070,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000964,248557,2736,'0',1397.723300,-48.960075,71.720856,2.490821,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000965,248557,2736,'0',1375.020000,-33.092003,71.515990,3.937066,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000966,248557,2736,'0',1328.067700,-11.708343,80.257100,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000967,248557,2736,'0',1207.664900,-126.968770,81.846230,4.138475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000968,248557,2736,'0',1163.524400,-56.281254,81.331990,5.641010,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000969,248557,2736,'0',1190.742100,-19.097220,107.636604,0.987518,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000970,248557,2736,'0',1182.328700,-145.401020,81.846740,2.433766,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000971,248557,2736,'0',1078.037400,-59.618042,73.125450,3.937066,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000972,248557,2736,'0',1142.445300,-197.295150,84.170815,3.937067,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000973,248557,2736,'0',1085.611900,-202.074660,64.722620,3.937063,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000974,248557,2736,'0',1091.400000,-253.585070,59.220150,5.873309,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000975,248557,2736,'0',907.282000,-140.607640,41.778297,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000976,248557,2736,'0',908.494750,-258.118040,61.675205,5.641010,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000977,248557,2736,'0',787.688350,-282.347200,27.431448,0.987533,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000978,248557,2736,'0',709.694460,-202.850700,19.341793,1.093005,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000979,248557,2736,'0',715.091000,-146.913200,15.053415,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000980,248557,2736,'0',761.093800,-118.256935,14.730905,5.641014,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000981,248557,2736,'0',714.136300,-300.539920,14.838867,3.937069,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000982,248557,2736,'0',770.231750,-360.453100,13.831841,3.937072,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000983,248557,2736,'0',815.896600,-453.526000,19.458584,0.987534,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000984,248557,2736,'0',787.734400,-478.861100,19.087584,3.034336,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000985,248557,2736,'0',750.381960,-449.493070,12.788251,1.792939,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000986,248557,2736,'0',884.006840,-448.317700,48.229540,4.138475,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000987,248557,2736,'0',845.887940,-499.347200,19.246641,3.937070,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000988,248557,2736,'0',812.563300,-596.826350,11.553810,3.937072,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000989,248557,2736,'0',811.890600,-517.987850,19.445532,3.497458,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000990,248557,2736,'0',749.917400,-584.031250,11.634286,2.433771,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000991,248557,2736,'0',857.674400,-617.810800,9.100640,0.987527,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000992,248557,2736,'0',912.316400,-764.307560,17.564500,2.205873,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000993,248557,2736,'0',938.169070,-796.071500,19.650375,2.988808,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000994,248557,2736,'0',930.167100,-761.793150,21.551370,3.739489,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000995,248557,2736,'0',927.215400,-863.499940,24.539354,1.968110,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000996,248557,2736,'0',989.363100,-741.250900,23.515460,4.006816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000997,248557,2736,'0',971.977360,-799.750300,22.773083,4.308294,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000998,248557,2736,'0',932.876340,-870.930600,22.705988,1.560347,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000999,248557,2736,'0',807.364440,-519.670170,20.306982,4.138469,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001000,248557,2736,'0',717.817700,-483.586820,18.351593,0.347392,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001001,248557,2736,'0',774.254330,-477.409700,19.347897,2.490816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001002,248557,2736,'0',681.512940,-506.925320,8.914986,2.433768,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001003,248557,2736,'0',661.108460,-477.833370,9.512896,0.987522,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001004,248557,2736,'0',749.318500,-444.784730,12.789288,2.433762,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001005,248557,2736,'0',661.022640,-420.848970,15.227432,5.641006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001006,248557,2736,'0',679.439150,-384.506930,16.732063,4.138471,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001007,248557,2736,'0',585.344600,-226.986100,19.754684,3.937073,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001008,248557,2736,'0',580.746460,-84.828156,16.425507,4.138481,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001009,248557,2736,'0',571.804750,-20.918406,15.700712,2.490816,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001010,248557,2736,'0',539.639000,-93.873270,14.269100,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001011,248557,2736,'0',508.795230,-129.550350,13.648978,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001012,248557,2736,'0',661.371500,-17.046875,15.877780,1.792939,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001013,248557,2736,'0',558.156300,86.894090,13.111812,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001014,248557,2736,'0',495.290830,244.427060,109.278160,2.490815,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001015,248557,2736,'0',478.372380,293.783000,108.939760,0.987529,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001016,248557,2736,'0',526.446960,332.159730,108.033950,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001017,248557,2736,'0',875.920900,481.550350,119.373764,2.433762,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001018,248557,2736,'0',835.334300,395.953120,80.897224,2.490814,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001019,248557,2736,'0',886.550350,428.977420,104.736820,1.093015,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001020,248557,2736,'0',1152.167600,727.069460,26.809578,3.937076,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001021,248557,2736,'0',1080.925000,625.238400,19.348337,3.252269,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001022,248557,2736,'0',1245.315000,589.711730,55.948440,2.433771,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001023,248557,2736,'0',1290.655300,741.180540,46.897970,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001024,248557,2736,'0',1324.409900,775.001300,44.971615,3.467323,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001025,248557,2736,'0',1318.063100,660.064150,45.195305,2.433764,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001026,248557,2736,'0',1353.601600,672.914900,45.382576,3.937074,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001027,248557,2736,'0',1382.565800,783.585100,46.155598,4.138483,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001028,248557,2736,'0',1415.819300,887.487900,52.248756,4.138477,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001029,248557,2736,'0',1527.790000,846.262150,46.318497,5.641006,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001030,248557,2736,'0',1539.926300,861.991330,36.638306,3.937066,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001031,248557,2736,'0',1514.452900,921.494700,33.232704,4.138486,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001032,248557,2736,'0',1595.142800,945.870100,49.055702,1.571620,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001033,248557,2736,'0',1683.954800,1025.527800,61.652878,1.093011,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001034,248557,2736,'0',1648.254400,969.788200,47.822144,3.937076,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001035,248557,2736,'0',1757.225800,930.202450,70.483665,4.519752,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001036,248557,2736,'0',1723.382100,1046.133800,56.825077,5.641009,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001037,248557,2736,'0',1702.573600,979.321400,53.132603,0.934490,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001038,248557,2736,'0',1822.268100,946.503360,87.507225,5.873310,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001039,248557,2736,'0',2018.603100,355.387180,193.246490,2.433758,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001040,248557,2736,'0',1992.847400,235.235570,201.728600,0.084721,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001041,248557,2736,'0',2053.809600,135.355260,193.567800,0.028109,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001042,248557,2736,'0',2043.580800,117.977990,198.012400,5.605445,120,64797);

-- Carrion (248558) x31
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001043,248558,2736,'0',1356.296900,-34.157986,70.945625,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001044,248558,2736,'0',1244.809100,-72.395836,75.909390,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001045,248558,2736,'0',1095.555500,-46.890625,67.515110,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001046,248558,2736,'0',1185.541600,-208.989580,74.397766,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001047,248558,2736,'0',1088.696200,-185.461800,57.936080,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001048,248558,2736,'0',920.718750,-254.404510,59.073105,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001049,248558,2736,'0',756.277800,-135.032990,1.606412,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001050,248558,2736,'0',872.626800,-496.548600,8.834306,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001051,248558,2736,'0',794.220500,-579.059000,5.574372,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001052,248558,2736,'0',943.336800,-750.559000,10.227247,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001053,248558,2736,'0',702.413200,-454.909730,3.296414,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001054,248558,2736,'0',690.746500,-449.642360,3.162168,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001055,248558,2736,'0',568.539900,1.163194,2.997449,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001056,248558,2736,'0',471.656250,89.979164,3.992468,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001057,248558,2736,'0',451.262150,187.875000,109.707860,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001058,248558,2736,'0',554.786440,326.897580,97.978150,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001059,248558,2736,'0',640.529540,270.048600,5.727089,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001060,248558,2736,'0',703.923650,302.515620,75.415474,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001061,248558,2736,'0',905.326400,446.366330,106.683464,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001062,248558,2736,'0',1257.802100,752.901060,32.405586,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001063,248558,2736,'0',1179.147600,635.586800,14.221570,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001064,248558,2736,'0',1103.463500,691.144100,9.602530,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001065,248558,2736,'0',1225.060800,765.607670,17.555239,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001066,248558,2736,'0',1368.026000,867.092040,32.947815,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001067,248558,2736,'0',1477.331700,874.380200,35.428146,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001068,248558,2736,'0',1536.371600,857.421900,30.746206,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001069,248558,2736,'0',1474.171900,954.682300,23.523605,5.877561,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001070,248558,2736,'0',1613.783000,1018.250000,37.159496,5.877561,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001071,248558,2736,'0',1635.066000,903.286440,35.778786,5.877561,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001072,248558,2736,'0',1835.777800,863.138900,82.800330,0.000000,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001073,248558,2736,'0',1900.116300,1039.458400,115.461820,0.000000,120,64797);

-- Seagull (249503) x17
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001074,249503,2736,'0',713.805540,-273.039950,15.540159,4.213077,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001075,249503,2736,'0',705.784700,-245.015620,15.485677,3.578012,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001076,249503,2736,'0',701.149300,-328.428830,15.967424,3.536542,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001077,249503,2736,'0',626.420170,-275.020840,55.183754,3.003950,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001078,249503,2736,'0',726.048650,-345.277770,32.182200,3.578012,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001079,249503,2736,'0',753.184000,-364.170140,33.419426,5.570794,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001080,249503,2736,'0',882.289900,-642.187500,2.800400,4.541510,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001081,249503,2736,'0',813.961800,-704.675350,6.851370,1.075962,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001082,249503,2736,'0',789.142400,-684.175350,10.196525,1.383345,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001083,249503,2736,'0',795.958900,-818.198800,27.466633,3.155737,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001084,249503,2736,'0',901.201400,-874.194460,26.984474,4.460324,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001085,249503,2736,'0',703.824650,-531.486150,11.666335,3.338824,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001086,249503,2736,'0',710.772600,-573.277800,4.305208,4.939690,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001087,249503,2736,'0',656.569460,-584.005200,8.226099,0.614079,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001088,249503,2736,'0',661.052060,-518.559000,5.228095,3.485321,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001089,249503,2736,'0',683.845500,-559.095500,4.049947,3.290284,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001090,249503,2736,'0',669.276060,-106.065970,4.319926,5.357399,120,64797);

-- Vulture (250432) x3
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001091,250432,2736,'0',667.984400,314.470500,102.133930,6.148174,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001092,250432,2736,'0',703.868040,301.631960,75.321900,1.525146,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001093,250432,2736,'0',1956.911500,1053.331700,142.446840,4.858970,120,64797);

-- Vulture (250433) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001094,250433,2736,'0',1953.295200,1053.956700,142.820630,4.449248,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001095,250433,2736,'0',1960.475700,1053.578100,142.065580,5.342774,120,64797);

-- The Last Architect (253596) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001096,253596,2736,'0',1728.050400,205.498260,100.032010,5.708831,120,64797);

-- Rotha (254687) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001097,254687,2736,'0',1763.939200,216.388890,107.031810,4.647851,120,64797);

-- Dye Station (255110) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001098,255110,2736,'0',1693.210100,209.932300,99.416534,4.571270,120,64797);

-- Haleth Turnwater (255125) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001099,255125,2736,'0',1697.064200,207.394100,99.819330,4.705107,120,64797);

-- Worker (255243) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001100,255243,2736,'0',1757.805500,212.640620,105.372750,5.226048,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001101,255243,2736,'0',1750.743000,213.862850,105.966774,4.717991,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001102,255243,2736,'0',1755.376700,212.281250,105.778710,5.310921,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001103,255243,2736,'0',1752.626700,211.972230,106.019394,3.412887,120,64797);

-- Gronthul (255278) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001104,255278,2736,'0',1649.020900,175.843750,99.181060,0.834745,120,64797);

-- Shon'ja (255297) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001105,255297,2736,'0',1651.805500,175.406250,99.172860,1.552992,120,64797);

-- Jehzar Starfall (255298) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001106,255298,2736,'0',1676.192700,215.420140,98.904520,5.210291,120,64797);

-- Lefton Farrer (255299) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001107,255299,2736,'0',1675.199700,219.394100,98.700610,3.874907,120,64797);

-- Botanist Boh'an (255301) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001108,255301,2736,'0',1720.507100,210.007660,100.493546,4.045650,120,64797);

-- Jevien (255477) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001109,255477,2736,'0',936.961100,-835.416000,10.726726,5.098080,120,64797);

-- Xiz'ro (255520) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001110,255520,2736,'0',1697.267300,167.390620,103.329895,4.069599,120,64797);

-- Neighborhood Laborer (255612) x66
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001111,255612,2736,'0',2082.209500,163.047990,175.097870,5.270736,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001112,255612,2736,'0',1858.817900,154.355290,107.800730,1.884645,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001113,255612,2736,'0',1750.495100,266.378600,106.104160,0.956956,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001114,255612,2736,'0',1776.495200,178.260100,99.864840,4.601876,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001115,255612,2736,'0',1753.049600,267.256300,106.104164,2.524748,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001116,255612,2736,'0',1744.140600,272.830100,106.104164,0.680174,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001117,255612,2736,'0',1754.786400,291.497070,106.104164,0.880973,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001118,255612,2736,'0',1742.897500,278.967350,106.120540,1.564854,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001119,255612,2736,'0',1733.506200,266.604900,106.124700,2.043514,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001120,255612,2736,'0',1822.193700,136.760000,107.800735,6.159593,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001121,255612,2736,'0',1713.412400,251.824140,106.104164,2.106362,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001122,255612,2736,'0',1860.800400,138.930240,107.800720,4.125107,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001123,255612,2736,'0',1703.047100,244.107880,106.104164,0.020708,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001124,255612,2736,'0',1687.500000,260.900300,106.104164,5.580396,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001125,255612,2736,'0',1697.650600,273.633200,106.104164,0.178248,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001126,255612,2736,'0',1678.148000,270.305150,106.104160,4.722432,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001127,255612,2736,'0',1681.180700,294.407100,106.104160,0.925036,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001128,255612,2736,'0',1655.852300,130.315440,103.146100,4.531591,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001129,255612,2736,'0',1736.987900,321.194120,106.104164,4.125103,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001130,255612,2736,'0',1683.032800,296.865080,106.104164,6.092219,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001131,255612,2736,'0',1712.697400,326.121520,106.104164,1.673621,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001132,255612,2736,'0',1695.870700,321.010380,106.104164,0.129713,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001133,255612,2736,'0',1653.146600,129.746370,103.146100,5.503327,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001134,255612,2736,'0',1655.440800,128.064540,103.146100,4.761701,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001135,255612,2736,'0',1809.496300,132.636950,107.758450,5.657531,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001136,255612,2736,'0',1837.178800,153.125000,107.800730,3.702947,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001137,255612,2736,'0',1690.154900,272.333200,106.104164,4.366350,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001138,255612,2736,'0',1703.659400,325.000000,106.104164,4.889819,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001139,255612,2736,'0',1623.903700,148.778320,103.146100,0.447594,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001140,255612,2736,'0',1644.695700,113.320880,103.146100,4.383347,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001141,255612,2736,'0',1634.799100,123.466550,103.146100,4.276761,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001142,255612,2736,'0',1654.851200,104.168015,103.146100,3.737501,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001143,255612,2736,'0',1636.914000,121.412530,103.146100,4.287113,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001144,255612,2736,'0',1658.335200,73.130610,103.134094,1.343305,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001145,255612,2736,'0',1676.372300,72.840430,103.146100,2.204155,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001146,255612,2736,'0',1817.205900,77.443480,107.800730,4.667112,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001147,255612,2736,'0',1820.987800,98.741160,107.800720,6.151335,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001148,255612,2736,'0',1828.236000,86.786220,107.800730,4.635948,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001149,255612,2736,'0',1844.263700,68.990425,107.800730,6.082825,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001150,255612,2736,'0',1840.809200,102.125700,107.800735,4.159389,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001151,255612,2736,'0',1627.769700,141.898680,103.146100,6.269486,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001152,255612,2736,'0',1630.115200,148.959270,103.146100,1.999558,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001153,255612,2736,'0',1308.128800,-139.159290,72.102560,6.189387,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001154,255612,2736,'0',1014.002100,-171.303990,59.463276,3.876935,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001155,255612,2736,'0',1100.568800,-241.787280,50.974150,5.411378,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001156,255612,2736,'0',986.626600,-160.416020,58.889416,2.001357,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001157,255612,2736,'0',999.563200,-198.904820,58.882540,1.824633,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001158,255612,2736,'0',982.316000,-142.128480,59.827680,2.148765,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001159,255612,2736,'0',1106.591900,-248.952900,50.413830,5.411378,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001160,255612,2736,'0',1068.451400,-170.531250,61.454094,1.757054,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001161,255612,2736,'0',841.215600,-821.236800,8.085575,4.960016,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001162,255612,2736,'0',867.208800,-810.581000,7.929155,3.270894,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001163,255612,2736,'0',831.712340,-811.011050,8.091423,3.031987,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001164,255612,2736,'0',863.453900,-856.627700,0.734187,3.268070,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001165,255612,2736,'0',948.818600,-795.426500,9.775712,0.863548,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001166,255612,2736,'0',613.168950,207.734910,16.822943,3.301177,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001167,255612,2736,'0',603.155460,209.825150,16.863928,4.716778,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001168,255612,2736,'0',1430.748000,802.560600,35.326930,3.831253,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001169,255612,2736,'0',1455.979100,815.688840,35.326930,5.792084,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001170,255612,2736,'0',1444.121500,796.692400,35.326930,1.601647,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001171,255612,2736,'0',1432.882700,803.141800,35.326930,5.635871,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001172,255612,2736,'0',1434.697300,795.814760,35.326930,0.872959,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001173,255612,2736,'0',1439.777300,789.033940,35.326930,4.567314,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001174,255612,2736,'0',1452.683500,854.267640,35.326930,4.860397,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001175,255612,2736,'0',1449.444500,855.208000,35.326930,3.683284,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001176,255612,2736,'0',1516.757800,841.246340,35.337470,5.029696,120,64797);

-- Saga Mistrunner (255684) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001177,255684,2736,'0',1720.576400,124.935770,100.151380,0.857246,120,64797);

-- [DNT] Orc Tent 1 (255780) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001178,255780,2736,'0',1791.472300,170.314240,100.979350,5.306152,120,64797);

-- [DNT] Orc Tent 2 (255781) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001179,255781,2736,'0',1791.401000,155.432300,100.760140,0.961435,120,64797);

-- Domesticated Wolf (255805) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001180,255805,2736,'0',963.720700,-154.341800,58.393690,0.013542,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001181,255805,2736,'0',965.190100,-150.067690,59.122643,2.480214,120,64797);

-- Domesticated Kodo (255806) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001182,255806,2736,'0',999.485960,-118.773840,58.862860,0.671945,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001183,255806,2736,'0',988.491640,-114.615610,59.153313,3.760771,120,64797);

-- Sawtail Thresher (255911) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001184,255911,2736,'0',742.370670,-808.234560,-28.733744,0.818587,120,64797);

-- Swarming Frenzy (255913) x51
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001185,255913,2736,'0',692.660500,-134.025620,-3.476257,3.387336,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001186,255913,2736,'0',900.339050,-679.680800,-1.761875,1.526955,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001187,255913,2736,'0',900.216800,-663.962460,-1.475624,0.153401,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001188,255913,2736,'0',888.663800,-686.207700,-3.466946,5.512661,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001189,255913,2736,'0',901.097100,-684.407500,-2.555486,3.154017,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001190,255913,2736,'0',772.414700,-760.050300,-17.115686,0.954138,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001191,255913,2736,'0',773.491900,-747.305300,-15.134519,0.686666,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001192,255913,2736,'0',798.497130,-824.710200,-11.432246,2.730633,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001193,255913,2736,'0',783.156860,-751.008000,-8.585608,5.284525,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001194,255913,2736,'0',807.619300,-833.386100,-3.989315,3.108393,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001195,255913,2736,'0',819.884000,-852.713100,-4.554672,3.906045,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001196,255913,2736,'0',760.886660,-735.069900,-10.636461,2.511518,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001197,255913,2736,'0',808.338130,-853.476750,-6.831049,3.848713,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001198,255913,2736,'0',890.640870,-688.496600,-3.466946,5.424608,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001199,255913,2736,'0',898.085500,-684.444900,-2.555486,3.154017,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001200,255913,2736,'0',900.471740,-676.656300,-1.761917,1.526958,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001201,255913,2736,'0',781.074900,-745.205500,-8.610183,1.989488,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001202,255913,2736,'0',902.029850,-664.693360,-1.475624,4.756701,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001203,255913,2736,'0',771.168330,-743.374450,-14.484839,0.837105,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001204,255913,2736,'0',762.383100,-737.950600,-10.636461,5.412430,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001205,255913,2736,'0',685.328400,-593.236630,-5.112328,1.269135,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001206,255913,2736,'0',692.052100,-591.596400,-2.840061,0.230740,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001207,255913,2736,'0',668.772950,-591.868600,-5.767301,5.187683,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001208,255913,2736,'0',666.827940,-602.293640,-5.187285,1.710881,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001209,255913,2736,'0',604.532700,-412.411830,-4.465962,4.730802,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001210,255913,2736,'0',600.380600,-435.082240,-3.163182,0.265687,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001211,255913,2736,'0',598.726140,-414.754330,-7.451376,5.006543,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001212,255913,2736,'0',678.462200,-118.962814,-3.882483,1.273451,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001213,255913,2736,'0',689.715200,-131.711260,-4.291802,1.591149,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001214,255913,2736,'0',685.907530,-118.912200,-4.986576,4.271835,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001215,255913,2736,'0',685.104300,-115.613730,-4.488440,4.726689,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001216,255913,2736,'0',635.601870,-74.519350,-3.324046,0.117728,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001217,255913,2736,'0',646.450440,-75.680850,-4.146811,0.230071,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001218,255913,2736,'0',551.429260,-52.468834,-0.551640,4.088403,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001219,255913,2736,'0',637.760560,-54.633545,-3.811442,2.018155,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001220,255913,2736,'0',643.297700,-62.507687,-4.166667,2.752550,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001221,255913,2736,'0',524.767800,-28.118706,0.200000,2.422685,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001222,255913,2736,'0',516.974300,-24.276495,0.200000,1.757331,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001223,255913,2736,'0',517.170200,-5.752257,-3.529568,5.453455,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001224,255913,2736,'0',536.706400,25.973728,0.200000,5.266656,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001225,255913,2736,'0',552.482240,72.623825,-3.347539,1.889350,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001226,255913,2736,'0',551.589360,56.678497,-5.677989,5.186076,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001227,255913,2736,'0',585.693500,93.942085,-3.276111,3.774301,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001228,255913,2736,'0',540.748050,44.249470,0.200000,2.076237,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001229,255913,2736,'0',504.205700,-4.119606,-5.196225,0.892675,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001230,255913,2736,'0',585.114440,92.251730,-4.522227,6.254311,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001231,255913,2736,'0',529.940060,29.153460,0.200000,0.884202,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001232,255913,2736,'0',476.017700,11.668780,-1.170641,1.173037,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001233,255913,2736,'0',494.482540,-9.983232,-1.606767,2.162620,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001234,255913,2736,'0',466.658260,16.931060,-1.565855,2.949135,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001235,255913,2736,'0',478.603180,-4.196717,0.200000,5.374598,120,64797);

-- Coastal Eel (255915) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001236,255915,2736,'0',748.835400,-711.893200,-10.253691,2.626093,120,64797);

-- Tide Skimmer (255917) x4
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001237,255917,2736,'0',856.974850,-676.286900,-5.720441,1.979959,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001238,255917,2736,'0',777.457030,-848.452940,-32.368130,1.740956,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001239,255917,2736,'0',858.773500,-682.047600,-5.720441,5.015024,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001240,255917,2736,'0',670.801800,-564.260130,-4.751521,2.561791,120,64797);

-- Spike Fish (255919) x11
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001241,255919,2736,'0',836.610530,-735.112500,-0.907830,2.584019,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001242,255919,2736,'0',842.033400,-783.487370,0.200000,3.132803,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001243,255919,2736,'0',779.561950,-806.238040,-6.771385,2.947170,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001244,255919,2736,'0',849.546260,-853.501500,0.024617,5.028273,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001245,255919,2736,'0',939.802550,-688.779400,-1.808587,2.221514,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001246,255919,2736,'0',838.278300,-856.659670,0.024617,0.597951,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001247,255919,2736,'0',936.143000,-683.972660,-1.808587,2.221511,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001248,255919,2736,'0',704.849200,-630.353800,-5.973480,1.923923,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001249,255919,2736,'0',632.910200,-469.130250,-2.868265,0.591650,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001250,255919,2736,'0',500.626700,17.956429,-0.753548,3.981095,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001251,255919,2736,'0',573.283400,70.163890,-2.392173,3.854544,120,64797);

-- Tidal Minnow (255924) x3
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001252,255924,2736,'0',802.604800,-767.534000,-2.310072,5.993078,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001253,255924,2736,'0',636.937130,-500.106570,-4.661597,4.636721,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001254,255924,2736,'0',1340.340800,723.124630,21.612976,3.594759,120,64797);

-- Rahgi Sourbone (255945) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001255,255945,2736,'0',1709.657000,209.995730,100.340225,4.795374,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001256,255945,2736,'0',1710.042700,194.268200,99.959114,4.738882,120,64797);

-- [DNT] Orc Gazebo (255953) x2
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001257,255953,2736,'0',1746.789900,168.401050,102.027390,0.032255,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001258,255953,2736,'0',1434.376700,-95.093750,64.695470,5.142281,120,64797);

-- [DNT] Vulpera Squad Spawner (256422) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001259,256422,2736,'0',974.322940,-109.753470,59.302746,0.000000,120,64797);

-- Wolf Pup (256522) x8
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001260,256522,2736,'0',1722.322600,200.444230,99.985085,2.308257,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001261,256522,2736,'0',1728.810400,114.631130,100.005510,4.168266,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001262,256522,2736,'0',1657.238800,172.098110,98.977104,4.555584,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001263,256522,2736,'0',1736.776000,114.832120,99.955380,1.109722,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001264,256522,2736,'0',1734.903400,115.859460,99.941840,3.169038,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001265,256522,2736,'0',1662.500000,182.629970,99.013410,0.067519,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001266,256522,2736,'0',1727.698200,116.500854,100.026760,2.994487,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001267,256522,2736,'0',1659.398200,178.105290,98.984700,0.006356,120,64797);

-- Raptor Hatchling (256523) x9
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001268,256523,2736,'0',1757.377100,214.041430,105.432236,4.949168,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001269,256523,2736,'0',1730.202800,131.556850,99.951210,1.111003,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001270,256523,2736,'0',1656.371700,174.414120,98.977104,4.820854,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001271,256523,2736,'0',1661.440100,232.644100,98.156670,3.351027,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001272,256523,2736,'0',1725.128900,134.797150,99.948680,2.832353,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001273,256523,2736,'0',1641.446500,182.029370,98.924194,3.736644,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001274,256523,2736,'0',1783.526500,210.310350,104.453026,0.075095,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001275,256523,2736,'0',1717.268200,134.790740,99.948680,4.320955,120,64797);
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001276,256523,2736,'0',1667.416600,181.333160,99.018630,5.070852,120,64797);

-- [DNT] Folk Wagon (257229) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001277,257229,2736,'0',1011.375000,-177.746540,59.536125,0.626395,120,64797);

-- [DNT] Folk Wagon (257230) x1
INSERT INTO `creature` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9001278,257230,2736,'0',967.859400,-118.368060,58.504368,0.280874,120,64797);

-- ================================================================
-- GAMEOBJECT TEMPLATES
-- ================================================================

-- Cornerstone (457142)
DELETE FROM `gameobject_template` WHERE `entry`=457142;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`VerifiedBuild`) VALUES
(457142,48,110660,'Cornerstone','buy',1.0,4,0,1,0,10,0,0,70,1266097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64797);

-- Razorwind Shores Bulletin Board (576757)
DELETE FROM `gameobject_template` WHERE `entry`=576757;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`VerifiedBuild`) VALUES
(576757,48,47199,'Razorwind Shores Bulletin Board','questinteract',1.52,4,0,0,0,10,0,0,72,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64797);

-- ================================================================
-- GAMEOBJECT SPAWNS (542 on Map 2736)
-- ================================================================

-- Cleanup previous import (guid range 9000000-9000541)
DELETE FROM `gameobject` WHERE `guid` BETWEEN 9000000 AND 9000541;

-- Fishing Bobber (35591) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000000,35591,2736,'0',846.100400,-857.231140,0.000000,6.257653,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000001,35591,2736,'0',844.094360,-838.595300,0.000000,1.517587,0,0,0,1,120,64797);

-- Vulpera Caravan (282388) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000002,282388,2736,'0',967.847200,-118.338540,58.459087,0.275625,0,0,0,1,120,64797);

-- Beach Chair (334348) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000003,334348,2736,'0',1001.888900,-635.416700,2.655833,3.969447,0,0,0,1,120,64797);

-- Campfire (340644) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000004,340644,2736,'0',967.538200,-108.269100,58.123283,0.000000,0,0,0,1,120,64797);

-- Tent (340645) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000005,340645,2736,'0',977.465300,-112.121530,58.602345,2.705670,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000006,340645,2736,'0',965.769100,-108.116320,57.872260,5.565399,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000007,340645,2736,'0',963.350700,-111.508680,57.836110,6.013669,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000008,340645,2736,'0',963.092040,-115.642360,58.127310,0.795478,0,0,0,1,120,64797);

-- Cornerstone (457142) x55
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000009,457142,2736,'0',1668.406200,1053.185800,48.467014,4.214974,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000010,457142,2736,'0',1531.533000,998.019100,36.255207,3.106652,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000011,457142,2736,'0',1343.776000,-635.861150,21.166666,2.207840,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000012,457142,2736,'0',772.871500,-477.524320,11.704862,1.003564,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000013,457142,2736,'0',1180.149300,-643.673650,5.706597,1.684243,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000014,457142,2736,'0',1101.684100,883.546900,6.833334,0.113445,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000015,457142,2736,'0',1215.644200,-9.722222,104.463540,4.127711,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000016,457142,2736,'0',733.854200,-434.173600,2.394097,5.611235,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000017,457142,2736,'0',1078.182300,1023.251800,3.147569,5.585054,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000018,457142,2736,'0',1023.829900,164.791670,31.107640,0.401425,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000019,457142,2736,'0',1493.413200,7.581597,78.328125,2.513274,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000020,457142,2736,'0',1309.732700,-105.368060,73.263870,4.852020,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000021,457142,2736,'0',649.975700,39.934030,3.918403,6.222101,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000022,457142,2736,'0',1607.913200,-242.921880,76.282990,0.244346,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000023,457142,2736,'0',574.291700,-316.392360,7.143509,5.323256,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000024,457142,2736,'0',745.534700,840.336800,8.753472,6.195921,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000025,457142,2736,'0',791.395800,-208.531250,15.600695,0.340338,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000026,457142,2736,'0',1339.895900,616.347200,42.003475,1.422443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000027,457142,2736,'0',1788.590300,-951.066000,92.314240,1.108283,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000028,457142,2736,'0',1864.541600,-515.519100,130.229170,5.672322,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000029,457142,2736,'0',1846.500000,-879.526060,120.711810,2.495818,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000030,457142,2736,'0',1663.281200,915.187500,37.611115,4.581496,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000031,457142,2736,'0',1165.727400,156.866320,34.956596,0.829030,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000032,457142,2736,'0',1256.180500,1091.045200,55.918404,4.782205,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000033,457142,2736,'0',1925.210100,948.899300,101.645850,5.052732,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000034,457142,2736,'0',2269.276100,-586.685800,161.475700,3.796098,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000035,457142,2736,'0',2079.962000,-272.855900,138.317700,3.019413,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000036,457142,2736,'0',544.621500,637.102400,155.302080,0.567232,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000037,457142,2736,'0',1844.258700,664.767400,88.525760,3.447027,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000038,457142,2736,'0',2150.564200,-740.092040,141.666670,5.838129,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000039,457142,2736,'0',1883.461800,-337.904500,129.980910,4.721121,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000040,457142,2736,'0',2235.255100,-871.059000,160.227430,1.919862,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000041,457142,2736,'0',1032.784800,695.145800,9.008680,1.308995,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000042,457142,2736,'0',902.305540,-545.763900,1.430556,1.448622,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000043,457142,2736,'0',1166.564200,464.543400,154.020830,4.555311,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000044,457142,2736,'0',1366.408000,85.184030,49.730904,3.647741,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000045,457142,2736,'0',1903.574700,-448.295140,137.003480,3.185267,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000046,457142,2736,'0',1640.022600,-567.548650,99.986115,6.265733,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000047,457142,2736,'0',1925.599000,-707.208300,134.032990,2.897245,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000048,457142,2736,'0',1658.414900,-169.755200,81.032990,1.422443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000049,457142,2736,'0',1266.102400,-171.218750,72.933730,0.968657,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000050,457142,2736,'0',669.458300,432.253480,9.001737,1.125737,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000051,457142,2736,'0',1665.501700,-677.595500,99.743060,0.383971,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000052,457142,2736,'0',1757.385400,-865.434000,116.753470,0.148352,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000053,457142,2736,'0',488.315980,304.342000,99.208336,2.705255,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000054,457142,2736,'0',958.105900,843.576400,4.788195,5.375617,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000055,457142,2736,'0',639.673650,708.329900,113.782990,5.777043,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000056,457142,2736,'0',955.019100,477.255220,108.661460,4.878198,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000057,457142,2736,'0',637.663200,-114.506940,2.946181,0.122173,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000058,457142,2736,'0',1802.213500,639.218750,85.600690,1.064650,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000059,457142,2736,'0',1129.519200,770.180540,17.621529,3.778642,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000060,457142,2736,'0',403.607640,192.350700,109.798610,1.457349,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000061,457142,2736,'0',380.664950,-107.657990,6.752863,6.274459,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000062,457142,2736,'0',304.656250,79.848960,24.362848,6.030115,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000063,457142,2736,'0',224.126740,652.369800,12.185764,0.139624,0,0,0,1,120,64797);

-- Horde - Rug [DNT] (508311) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000064,508311,2736,'0',1811.694500,278.072900,120.545980,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000065,508311,2736,'0',1692.479100,170.385420,103.162890,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000066,508311,2736,'0',1746.677100,168.300350,101.531620,0.000000,0,0,0,1,120,64797);

-- Picnic Basket (508323) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000067,508323,2736,'0',1811.694500,278.072900,120.545980,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000068,508323,2736,'0',1692.479100,170.385420,103.162890,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000069,508323,2736,'0',1746.677100,168.300350,100.681230,0.000000,0,0,0,1,120,64797);

-- Campfire (525514) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000070,525514,2736,'0',971.354200,-111.706600,58.591553,0.000000,0,0,0,1,120,64797);

-- Wood Pile (529653) x11
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000071,529653,2736,'0',1820.317700,132.984380,107.800730,3.029823,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000072,529653,2736,'0',1711.257000,249.965290,106.104160,4.724186,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000073,529653,2736,'0',1675.362900,302.847230,106.104160,4.780621,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000074,529653,2736,'0',1596.224000,114.298610,103.138740,0.002808,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000075,529653,2736,'0',1815.856000,68.097220,107.800730,6.280181,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000076,529653,2736,'0',955.572940,-259.658000,62.406740,6.242171,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000077,529653,2736,'0',1436.409800,845.982670,35.326930,2.559924,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000078,529653,2736,'0',1431.762200,796.555540,35.326930,4.117186,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000079,529653,2736,'0',1457.007000,854.791700,35.326930,1.512069,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000080,529653,2736,'0',1500.701400,850.187500,35.326930,1.442668,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000081,529653,2736,'0',1512.229100,835.076400,35.326930,0.596221,0,0,0,1,120,64797);

-- Portal to Orgrimmar (531766) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000082,531766,2736,'0',2092.125000,189.678820,176.748380,3.407442,0,0,0,1,120,64797);

-- Stool (531941) x6
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000083,531941,2736,'0',1742.911500,68.319440,103.546210,5.877207,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000084,531941,2736,'0',1771.071200,27.267360,100.011680,1.218585,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000085,531941,2736,'0',1769.897600,28.986110,100.009610,0.086126,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000086,531941,2736,'0',1386.619800,-28.375000,64.127110,1.133929,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000087,531941,2736,'0',1389.427100,-28.420140,64.194496,2.133179,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000088,531941,2736,'0',1464.423600,947.055540,23.590899,1.167039,0,0,0,1,120,64797);

-- Sturdy Wooden Chair (536082) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000089,536082,2736,'0',1197.894400,27.037401,105.391594,0.261776,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000090,536082,2736,'0',1199.865600,27.471287,105.391600,3.403413,0,0,0,1,120,64797);

-- Fish Basket (539067) x6
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000091,539067,2736,'0',592.206600,116.847220,2.124844,1.628686,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000092,539067,2736,'0',645.736150,182.756940,0.694330,2.236457,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000093,539067,2736,'0',685.604200,203.784730,4.154204,1.608598,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000094,539067,2736,'0',839.098940,-821.151060,7.916040,1.554162,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000095,539067,2736,'0',820.357670,-814.484400,7.850267,0.019385,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000096,539067,2736,'0',810.241330,-797.501800,2.752653,0.000000,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Toolbox (571377) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000097,571377,2736,'0',589.583300,116.048610,2.315771,0.763914,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Hanging Rac.. (571379) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000098,571379,2736,'0',588.420170,191.484380,16.919468,0.000000,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Skinning To.. (572778) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000099,572778,2736,'0',600.185800,210.019100,18.839003,5.256345,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Skinning To.. (572780) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000100,572780,2736,'0',600.107670,210.621540,18.808697,3.717910,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fishing She.. (572820) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000101,572820,2736,'0',593.548650,194.934040,16.959986,5.867933,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fish 00 (572829) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000102,572829,2736,'0',595.187500,209.539930,18.001112,1.142659,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fish 01 (572830) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000103,572830,2736,'0',599.654540,209.145830,17.906057,1.044751,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fish Tool 0.. (572833) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000104,572833,2736,'0',600.237850,209.347230,18.842964,1.525463,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fish Tool 0.. (572835) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000105,572835,2736,'0',595.461800,210.506940,18.486542,2.584106,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000106,572835,2736,'0',594.970500,209.531250,18.662975,2.584106,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fish 02 (572836) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000107,572836,2736,'0',599.545170,210.272570,17.939075,5.505380,0,0,0,1,120,64797);

-- [DNT] Forbidden Reach Fish Barrel.. (572837) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000108,572837,2736,'0',593.364560,192.819440,16.909872,5.684750,0,0,0,1,120,64797);

-- Stool (572891) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000109,572891,2736,'0',1015.019100,-180.088550,59.452793,6.115884,0,0,0,1,120,64797);

-- [DNT] Grass 00 (Razorwind) (574202) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000110,574202,2736,'0',1792.204800,181.718750,100.580120,0.888276,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000111,574202,2736,'0',1789.211800,172.987850,100.755820,0.566227,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000112,574202,2736,'0',1752.751700,161.869800,101.580830,0.000000,0,0,0,1,120,64797);

-- [DNT] Plant 00 (Razorwind) (574203) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000113,574203,2736,'0',1791.204800,157.204860,100.763640,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000114,574203,2736,'0',1743.729100,159.791670,101.483440,0.000000,0,0,0,1,120,64797);

-- [DNT] Bush 00 (Razorwind) (574204) x5
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000115,574204,2736,'0',1790.213500,174.543410,100.760980,4.295189,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000116,574204,2736,'0',1796.335100,161.980910,100.852430,2.707517,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000117,574204,2736,'0',1749.838500,176.927080,101.476940,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000118,574204,2736,'0',1791.541600,149.906250,100.177490,2.866937,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000119,574204,2736,'0',1736.913200,168.529510,101.563780,4.226154,0,0,0,1,120,64797);

-- [DNT] Grass 01 (Razorwind) (574205) x7
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000120,574205,2736,'0',1788.836800,185.713550,100.933020,1.449664,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000121,574205,2736,'0',1789.085100,183.651050,100.850960,1.159447,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000122,574205,2736,'0',1791.960100,152.597230,100.443980,2.017999,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000123,574205,2736,'0',1755.637200,169.586800,101.547430,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000124,574205,2736,'0',1754.083400,170.498260,101.708290,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000125,574205,2736,'0',1787.494800,149.821180,99.970436,4.815392,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000126,574205,2736,'0',1741.079800,175.913200,101.323520,0.000000,0,0,0,1,120,64797);

-- [DNT] Grass 02 (Razorwind) (574206) x5
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000127,574206,2736,'0',1793.404500,179.161450,100.832090,2.776925,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000128,574206,2736,'0',1790.316000,178.850700,100.747380,6.233440,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000129,574206,2736,'0',1794.993000,165.991320,100.883160,2.321541,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000130,574206,2736,'0',1786.557300,152.185760,99.970436,5.675319,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000131,574206,2736,'0',1748.593800,161.895830,101.660680,6.200077,0,0,0,1,120,64797);

-- [DNT] Bramble 00 (Razorwind) (574207) x6
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000132,574207,2736,'0',1791.463500,180.854170,100.750180,5.940682,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000133,574207,2736,'0',1795.104100,168.883680,100.868630,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000134,574207,2736,'0',1790.600700,165.368060,100.839990,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000135,574207,2736,'0',1792.635400,158.335070,100.835490,3.900324,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000136,574207,2736,'0',1788.606000,152.803820,100.108890,1.717784,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000137,574207,2736,'0',1741.178800,162.097230,101.549360,4.273815,0,0,0,1,120,64797);

-- [DNT] Ground Cover (Razorwind) - .. (574212) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000138,574212,2736,'0',1738.460100,166.579860,101.761330,4.466668,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000139,574212,2736,'0',1749.921900,160.076390,101.653930,2.833250,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000140,574212,2736,'0',1744.501700,173.914930,101.627740,5.260372,0,0,0,1,120,64797);

-- [DNT] Rugged Orc Tent 02 (Razorwi.. (574288) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000141,574288,2736,'0',1791.392300,155.439240,100.653500,0.948722,0,0,0,1,120,64797);

-- [DNT] Rugged Orc Tent 01 (Razorwi.. (574289) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000142,574289,2736,'0',1791.423600,170.288200,100.818520,5.302654,0,0,0,1,120,64797);

-- [DNT] Rock Cluster 00 (Razorwind) (574420) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000143,574420,2736,'0',1797.579800,167.295140,100.822730,0.083194,0,0,0,1,120,64797);

-- Durotar Fence w/Rope (576417) x15
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000144,576417,2736,'0',791.534700,-222.644100,15.598905,0.124413,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000145,576417,2736,'0',733.875000,-235.454860,15.599874,0.156140,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000146,576417,2736,'0',762.347200,-270.958340,15.600884,4.899670,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000147,576417,2736,'0',738.888900,-420.190980,2.164386,2.635779,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000148,576417,2736,'0',687.897600,-418.184020,2.156883,1.018398,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000149,576417,2736,'0',695.223940,-378.000000,2.163248,2.604058,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000150,576417,2736,'0',587.711800,-268.170140,6.963840,3.772653,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000151,576417,2736,'0',633.442700,-144.048610,3.407779,3.115099,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000152,576417,2736,'0',593.519100,-161.710070,3.286744,4.700757,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000153,576417,2736,'0',603.965300,-102.909720,3.326632,4.732484,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000154,576417,2736,'0',590.048650,15.857639,3.918674,3.236645,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000155,576417,2736,'0',615.510440,47.736110,3.918673,1.650961,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000156,576417,2736,'0',534.050350,261.604160,99.196930,5.756690,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000157,576417,2736,'0',547.911440,308.381960,99.190220,4.157796,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000158,576417,2736,'0',501.307280,324.715270,99.197780,5.775183,0,0,0,1,120,64797);

-- Durotar Fence w/Leather (576419) x17
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000159,576419,2736,'0',777.220500,-198.555560,15.598905,1.718494,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000160,576419,2736,'0',735.611150,-204.913200,15.600579,1.718494,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000161,576419,2736,'0',742.531250,-274.204860,15.599730,1.718494,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000162,576419,2736,'0',724.243040,-439.751740,2.156881,4.198146,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000163,576419,2736,'0',676.904540,-411.798600,2.158247,4.195574,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000164,576419,2736,'0',736.927060,-366.588530,2.150120,4.198146,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000165,576419,2736,'0',603.510440,-293.593750,6.963840,5.335012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000166,576419,2736,'0',633.343750,-160.229170,3.388280,6.232330,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000167,576419,2736,'0',550.691000,-245.975700,6.963839,5.335012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000168,576419,2736,'0',633.843750,-104.552090,3.385281,0.011656,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000169,576419,2736,'0',564.545170,-120.500000,3.359347,0.011656,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000170,576419,2736,'0',602.986150,-23.270834,3.918675,4.768196,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000171,576419,2736,'0',647.000000,-18.739584,3.918674,4.830710,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000172,576419,2736,'0',436.187500,156.126740,109.802300,3.027490,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000173,576419,2736,'0',545.998300,282.142360,99.191060,5.768202,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000174,576419,2736,'0',497.289950,256.954860,99.196450,1.032839,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000175,576419,2736,'0',508.376740,330.767360,99.142044,1.054354,0,0,0,1,120,64797);

-- Durotar Fence Regular (576421) x38
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000176,576421,2736,'0',788.663200,-199.171880,15.578110,3.306445,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000177,576421,2736,'0',793.831600,-238.102430,15.598905,3.306445,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000178,576421,2736,'0',796.779540,-258.420140,15.598523,0.173464,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000179,576421,2736,'0',758.026060,-201.281250,15.598906,1.570941,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000180,576421,2736,'0',730.722200,-212.312500,15.600577,3.306445,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000181,576421,2736,'0',784.416700,-267.798600,15.600742,1.676229,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000182,576421,2736,'0',738.329900,-267.357640,15.598905,3.306445,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000183,576421,2736,'0',707.366330,-429.309020,2.162681,4.168902,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000184,576421,2736,'0',682.206600,-399.090270,2.169784,5.738316,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000185,576421,2736,'0',750.449650,-400.274320,2.161571,5.786075,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000186,576421,2736,'0',710.283000,-352.659730,2.160585,2.598320,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000187,576421,2736,'0',718.326400,-355.479160,2.118921,4.155878,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000188,576421,2736,'0',760.906250,-383.427100,2.156874,5.786075,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000189,576421,2736,'0',754.472200,-376.944460,2.156874,4.149351,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000190,576421,2736,'0',584.076400,-307.333340,6.963840,5.286216,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000191,576421,2736,'0',600.729200,-286.270840,6.963838,0.639755,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000192,576421,2736,'0',612.902800,-161.994800,3.396674,1.551826,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000193,576421,2736,'0',625.357670,-161.822920,3.396674,1.599588,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000194,576421,2736,'0',574.083300,-248.614580,6.963840,0.639755,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000195,576421,2736,'0',564.375000,-157.513890,3.407080,6.252577,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000196,576421,2736,'0',572.786440,-162.142360,3.366500,4.749808,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000197,576421,2736,'0',633.958300,-128.347230,3.349400,6.246049,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000198,576421,2736,'0',626.260440,-102.626740,3.396675,1.599588,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000199,576421,2736,'0',580.133670,-102.855900,3.403841,1.599588,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000200,576421,2736,'0',593.406250,-21.673610,3.918675,0.135453,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000201,576421,2736,'0',627.635440,-20.663195,3.918674,4.781916,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000202,576421,2736,'0',591.828100,-3.449653,3.918674,0.087692,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000203,576421,2736,'0',652.097200,-11.479167,3.918674,0.135453,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000204,576421,2736,'0',587.416700,36.432293,3.910854,3.285679,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000205,576421,2736,'0',591.121500,45.286457,3.918707,4.788442,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000206,576421,2736,'0',434.493070,137.907990,109.802300,3.043654,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000207,576421,2736,'0',526.793400,249.000000,99.190220,5.773015,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000208,576421,2736,'0',521.230900,243.309040,99.189610,1.043506,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000209,576421,2736,'0',481.362850,266.000000,99.194170,1.049022,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000210,576421,2736,'0',556.710100,301.409730,99.189910,2.645794,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000211,576421,2736,'0',474.536470,275.975700,99.192840,2.642285,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000212,576421,2736,'0',483.081600,291.260400,99.191050,2.642285,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000213,576421,2736,'0',526.425350,320.555570,99.190220,1.025114,0,0,0,1,120,64797);

-- Razorwind Shores Bulletin Board (576757) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000214,576757,2736,'0',1716.064200,99.927086,102.185680,6.091198,0,0,0,1,120,64797);

-- 11.2.7 Housing - Generic - Ground.. (587321) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000215,587321,2736,'0',1285.317700,1113.731000,55.939390,5.903797,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000216,587321,2736,'0',1349.484400,580.649300,41.964127,6.125979,0,0,0,1,120,64797);

-- Durotar Main Tree Palm (587323) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000217,587323,2736,'0',748.482670,-257.237850,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000218,587323,2736,'0',693.293400,-403.908000,2.155443,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000219,587323,2736,'0',622.467040,-141.954860,3.396673,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000220,587323,2736,'0',609.159700,37.920140,4.046441,4.844709,0,0,0,1,120,64797);

-- Durotar Main Tree Palm - Tall (587324) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000221,587324,2736,'0',748.326400,-260.968750,15.598907,3.679012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000222,587324,2736,'0',690.807300,-406.703120,2.156815,2.993276,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000223,587324,2736,'0',618.789900,-142.493060,3.396673,2.290311,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000224,587324,2736,'0',608.064300,34.371530,3.954427,3.418096,0,0,0,1,120,64797);

-- Durotar Main Tree Palm - Medium (587325) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000225,587325,2736,'0',754.373300,-256.815980,15.598905,5.075481,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000226,587325,2736,'0',698.109400,-407.321200,2.152105,4.389748,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000227,587325,2736,'0',623.939300,-147.680560,3.393271,3.686782,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000228,587325,2736,'0',614.958300,36.795140,3.918674,4.814557,0,0,0,1,120,64797);

-- Durotar Azshara Birch Bush (587450) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000229,587450,2736,'0',523.125000,317.579860,99.009680,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Birch Bush (587452) x12
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000230,587452,2736,'0',535.442700,284.541660,98.305520,2.144624,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000231,587452,2736,'0',536.230900,284.968750,99.189610,2.144624,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000232,587452,2736,'0',537.795170,283.562500,98.422500,2.144624,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000233,587452,2736,'0',540.333300,290.659730,99.004700,2.144624,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000234,587452,2736,'0',535.427060,294.201400,99.004700,3.250106,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000235,587452,2736,'0',537.069460,294.496520,99.004700,4.250648,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000236,587452,2736,'0',538.789900,295.432280,99.189610,3.250106,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000237,587452,2736,'0',542.861150,295.517360,98.975945,2.144624,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000238,587452,2736,'0',527.833300,313.020840,99.004740,0.283727,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000239,587452,2736,'0',526.270800,313.756960,99.004740,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000240,587452,2736,'0',528.347200,315.163200,99.004740,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000241,587452,2736,'0',527.833300,316.729160,99.004740,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Bush Leafy (587457) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000242,587457,2736,'0',493.809020,291.291660,99.004715,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000243,587457,2736,'0',493.246520,293.944460,99.004710,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Bush Leafy (587458) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000244,587458,2736,'0',490.784730,291.711820,99.004710,6.191112,0,0,0,1,120,64797);

-- Durotar Azshara Blueberry (587459) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000245,587459,2736,'0',491.489600,294.267360,99.004710,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Blueberry (587460) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000246,587460,2736,'0',495.361100,266.704860,99.004715,4.264990,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000247,587460,2736,'0',499.277770,268.885400,99.004715,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000248,587460,2736,'0',492.375000,290.736100,99.004710,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Bush Willow (587461) x10
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000249,587461,2736,'0',495.479160,268.524320,99.004715,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000250,587461,2736,'0',496.953120,265.048600,98.115650,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000251,587461,2736,'0',492.489600,266.375000,98.212980,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000252,587461,2736,'0',498.788200,267.600700,99.004715,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000253,587461,2736,'0',492.637150,268.885400,97.722090,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000254,587461,2736,'0',498.149320,271.986100,99.011860,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000255,587461,2736,'0',493.847230,272.701400,98.362625,0.048913,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000256,587461,2736,'0',513.343750,296.753480,99.004730,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000257,587461,2736,'0',509.684020,296.833340,99.004590,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000258,587461,2736,'0',513.631960,300.656250,99.041940,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Bush Willow (587462) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000259,587462,2736,'0',512.996500,296.982640,99.004720,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Bush Willow (587463) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000260,587463,2736,'0',510.131960,298.597230,99.004555,2.453868,0,0,0,1,120,64797);

-- Durotar Azshara Birch Bush (587464) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000261,587464,2736,'0',539.631960,286.890620,97.756530,0.777443,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000262,587464,2736,'0',540.090300,293.732640,99.004700,0.777443,0,0,0,1,120,64797);

-- Durotar Azshara Dune Grass (587466) x33
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000263,587466,2736,'0',777.364560,-223.843750,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000264,587466,2736,'0',786.029540,-252.543410,15.598905,4.952483,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000265,587466,2736,'0',771.144100,-211.527790,15.598905,4.989240,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000266,587466,2736,'0',756.269100,-255.944440,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000267,587466,2736,'0',753.689300,-259.387150,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000268,587466,2736,'0',744.718750,-213.291670,15.598905,4.313724,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000269,587466,2736,'0',752.435800,-255.302080,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000270,587466,2736,'0',748.151060,-260.774320,15.598907,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000271,587466,2736,'0',709.317700,-420.515620,2.159272,4.503906,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000272,587466,2736,'0',690.796900,-406.432280,2.156821,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000273,587466,2736,'0',700.133670,-407.848970,2.144469,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000274,587466,2736,'0',725.227400,-395.855900,2.151543,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000275,587466,2736,'0',697.578100,-404.925350,2.149650,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000276,587466,2736,'0',695.967040,-408.869780,2.157323,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000277,587466,2736,'0',721.071170,-376.338530,2.154335,4.875117,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000278,587466,2736,'0',746.024300,-387.578120,2.156874,4.681930,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000279,587466,2736,'0',590.630200,-290.598970,6.963840,4.269797,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000280,587466,2736,'0',625.144100,-149.371540,3.396674,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000281,587466,2736,'0',621.303830,-147.468750,3.395164,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000282,587466,2736,'0',625.078100,-145.510420,3.396673,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000283,587466,2736,'0',618.939300,-142.281250,3.396673,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000284,587466,2736,'0',565.357670,-255.053820,6.963847,4.462986,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000285,587466,2736,'0',590.270800,-151.597230,3.396674,3.744849,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000286,587466,2736,'0',573.097200,-143.024300,3.398709,3.675980,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000287,587466,2736,'0',595.553830,-118.088540,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000288,587466,2736,'0',574.033000,-114.475690,3.394531,3.770948,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000289,587466,2736,'0',607.673650,10.406250,4.294705,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000290,587466,2736,'0',610.743040,-11.376737,3.918674,4.872629,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000291,587466,2736,'0',637.753500,-6.961806,3.918674,4.803757,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000292,587466,2736,'0',607.939300,34.593750,3.966580,4.844709,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000293,587466,2736,'0',613.494800,38.753470,3.980469,4.844709,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000294,587466,2736,'0',613.640600,34.505207,3.918674,4.844709,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000295,587466,2736,'0',617.012150,37.161457,3.918674,4.844709,0,0,0,1,120,64797);

-- Durotar Azshara Dune Grass (587467) x55
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000296,587467,2736,'0',782.208300,-219.805560,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000297,587467,2736,'0',781.506960,-220.947920,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000298,587467,2736,'0',771.126800,-207.298610,15.598905,4.864300,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000299,587467,2736,'0',779.892400,-219.052080,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000300,587467,2736,'0',774.064300,-207.548610,15.598905,4.537342,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000301,587467,2736,'0',780.447940,-223.045140,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000302,587467,2736,'0',776.920170,-219.965290,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000303,587467,2736,'0',782.590300,-252.182300,15.598905,4.941577,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000304,587467,2736,'0',778.670170,-221.326390,15.598905,4.843713,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000305,587467,2736,'0',751.689300,-260.484380,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000306,587467,2736,'0',750.262150,-257.260400,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000307,587467,2736,'0',743.527800,-210.649300,15.598905,4.458896,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000308,587467,2736,'0',693.717040,-408.449650,2.157243,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000309,587467,2736,'0',727.800350,-397.741330,2.153230,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000310,587467,2736,'0',730.092040,-397.244780,2.164862,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000311,587467,2736,'0',706.369800,-418.699650,2.159101,4.493001,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000312,587467,2736,'0',728.008670,-395.272580,2.152213,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000313,587467,2736,'0',694.651060,-405.053830,2.164328,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000314,587467,2736,'0',731.421900,-397.067720,2.160123,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000315,587467,2736,'0',730.539900,-394.800350,2.164720,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000316,587467,2736,'0',749.305540,-384.901030,2.156874,4.681930,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000317,587467,2736,'0',727.956600,-393.064240,2.151647,3.955012,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000318,587467,2736,'0',722.383670,-373.751740,2.150861,4.875117,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000319,587467,2736,'0',750.468750,-388.449650,2.156874,4.681930,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000320,587467,2736,'0',594.710100,-289.458340,6.963840,4.269797,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000321,587467,2736,'0',594.355900,-293.177100,6.963840,4.269797,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000322,587467,2736,'0',619.855900,-145.701390,3.396673,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000323,587467,2736,'0',622.765600,-143.715290,3.396673,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000324,587467,2736,'0',567.604200,-253.210070,6.963847,4.462986,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000325,587467,2736,'0',593.173650,-151.690980,3.396674,3.744849,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000326,587467,2736,'0',564.967040,-273.140620,6.963840,3.542880,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000327,587467,2736,'0',566.640600,-275.765620,6.963840,3.542880,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000328,587467,2736,'0',564.133670,-275.182280,6.963840,3.542880,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000329,587467,2736,'0',566.533000,-278.199650,6.963840,3.542880,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000330,587467,2736,'0',565.244800,-277.824650,6.963840,3.542880,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000331,587467,2736,'0',577.145800,-148.060760,3.858778,3.675980,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000332,587467,2736,'0',599.227400,-121.550350,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000333,587467,2736,'0',574.774300,-147.236110,3.518913,3.675980,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000334,587467,2736,'0',600.748300,-119.571180,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000335,587467,2736,'0',596.970500,-120.914930,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000336,587467,2736,'0',598.300350,-118.821180,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000337,587467,2736,'0',599.258670,-116.817710,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000338,587467,2736,'0',600.498300,-122.001740,3.396674,3.476751,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000339,587467,2736,'0',573.112850,-111.118060,3.396674,3.760041,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000340,587467,2736,'0',612.375000,12.232639,3.918674,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000341,587467,2736,'0',612.062500,-8.789930,3.918674,4.872629,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000342,587467,2736,'0',613.329900,13.190972,3.918674,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000343,587467,2736,'0',609.520800,12.562500,4.180121,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000344,587467,2736,'0',608.125000,14.295139,4.411025,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000345,587467,2736,'0',610.840300,10.468750,3.939213,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000346,587467,2736,'0',611.256960,14.461805,4.079427,4.604525,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000347,587467,2736,'0',640.689300,-3.883681,3.918674,4.803757,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000348,587467,2736,'0',642.267400,-7.256945,3.918674,4.803757,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000349,587467,2736,'0',610.878500,37.435764,3.959636,4.844709,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000350,587467,2736,'0',611.421900,33.965280,3.918674,4.844709,0,0,0,1,120,64797);

-- Durotar Azshara Dune Grass (587468) x19
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000351,587468,2736,'0',772.331600,-208.833330,15.598905,4.736194,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000352,587468,2736,'0',746.079900,-211.777790,15.598905,4.267695,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000353,587468,2736,'0',745.010440,-208.777790,15.598905,4.401671,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000354,587468,2736,'0',752.105900,-258.673600,15.598905,5.105633,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000355,587468,2736,'0',695.192700,-407.317720,2.156517,4.419900,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000356,587468,2736,'0',748.873300,-386.805570,2.156925,4.757653,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000357,587468,2736,'0',723.109400,-376.439240,2.152164,4.875117,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000358,587468,2736,'0',724.769100,-373.717000,2.151689,1.459786,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000359,587468,2736,'0',592.666700,-294.531250,6.963840,4.345523,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000360,587468,2736,'0',569.798650,-254.135420,6.963847,1.047653,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000361,587468,2736,'0',621.727400,-145.781250,3.396673,3.716936,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000362,587468,2736,'0',567.191000,-255.963550,6.963847,4.462986,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000363,587468,2736,'0',591.017400,-153.489580,3.396674,3.744849,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000364,587468,2736,'0',572.611150,-148.371540,2.763177,3.751707,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000365,587468,2736,'0',588.006960,-149.336800,3.334473,3.744849,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000366,587468,2736,'0',612.777800,-11.501737,3.918674,4.872629,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000367,587468,2736,'0',607.725700,-12.449653,3.918674,4.872629,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000368,587468,2736,'0',641.517400,-9.293403,3.918674,4.879482,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000369,587468,2736,'0',612.296900,35.605904,3.918674,4.844709,0,0,0,1,120,64797);

-- Durotar Rock (587578) x18
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000370,587578,2736,'0',778.753500,-227.795140,15.598905,2.016356,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000371,587578,2736,'0',769.241330,-206.899300,15.598905,5.045871,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000372,587578,2736,'0',776.185800,-210.944440,15.598905,4.113991,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000373,587578,2736,'0',723.039900,-399.421880,2.152456,1.127656,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000374,587578,2736,'0',749.663200,-391.126740,2.156874,1.279625,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000375,587578,2736,'0',748.423650,-383.185760,2.156874,4.595106,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000376,587578,2736,'0',594.592040,-287.534730,6.963840,4.182975,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000377,587578,2736,'0',592.539900,-295.305570,6.963840,0.867494,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000378,587578,2736,'0',578.104200,-142.704860,3.629612,3.589156,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000379,587578,2736,'0',570.027800,-149.899300,3.301584,0.273675,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000380,587578,2736,'0',591.960100,-120.248270,3.396674,0.649393,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000381,587578,2736,'0',608.086800,6.239584,4.051650,1.777170,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000382,587578,2736,'0',641.800350,-10.031250,3.918674,1.401454,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000383,587578,2736,'0',639.612850,-2.295139,3.918674,4.716935,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000384,587578,2736,'0',541.923650,292.274320,99.004700,5.745266,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000385,587578,2736,'0',492.809020,266.743070,99.004715,4.968503,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000386,587578,2736,'0',515.142400,298.152770,99.004720,2.034125,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000387,587578,2736,'0',510.836820,300.069460,99.004680,4.968503,0,0,0,1,120,64797);

-- Durotar Rock (587579) x5
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000388,587579,2736,'0',784.788200,-248.656250,15.598905,3.954423,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000389,587579,2736,'0',709.887150,-416.470500,2.156054,3.505849,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000390,587579,2736,'0',577.213560,-111.854160,3.396674,2.772887,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000391,587579,2736,'0',496.196200,272.300350,98.077385,5.673182,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000392,587579,2736,'0',536.531250,291.045140,99.004700,0.553831,0,0,0,1,120,64797);

-- Durotar Rock (587580) x5
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000393,587580,2736,'0',740.944460,-214.038200,15.598905,4.611285,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000394,587580,2736,'0',718.119800,-373.873260,2.160419,4.368459,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000395,587580,2736,'0',563.644100,-251.618060,6.963847,3.956327,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000396,587580,2736,'0',591.239560,-147.875000,3.396674,3.238198,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000397,587580,2736,'0',607.795170,-8.897570,3.918674,4.365972,0,0,0,1,120,64797);

-- 11.2.7 Housing - Generic - Ground.. (587581) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000398,587581,2736,'0',605.753500,726.873300,113.732540,2.662768,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000399,587581,2736,'0',515.517400,287.375000,99.004690,1.088587,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000400,587581,2736,'0',401.381960,159.711800,109.732620,3.043303,0,0,0,1,120,64797);

-- Durotar Azshara Tree Pine (593424) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000401,593424,2736,'0',524.850700,315.704860,99.004740,0.777443,0,0,0,1,120,64797);

-- Razorwind Shores Front Door (602705) x10
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000402,602705,2736,'0',1522.390000,-36.415390,80.534300,6.206418,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000403,602705,2736,'0',1326.848400,-81.426970,75.574800,3.396438,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000404,602705,2736,'0',1220.455900,12.052189,107.072800,4.129476,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000405,602705,2736,'0',1250.723300,-189.747020,75.120800,0.193760,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000406,602705,2736,'0',752.542200,-529.084660,14.150804,1.162421,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000407,602705,2736,'0',891.201350,-567.567300,3.844349,1.040245,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000408,602705,2736,'0',930.574500,494.524900,111.102800,4.304010,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000409,602705,2736,'0',1556.260700,1009.196800,39.033913,2.480140,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000410,602705,2736,'0',1669.692600,941.650450,40.034233,3.963665,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000411,602705,2736,'0',1921.118700,971.468700,104.052795,4.487268,0,0,0,1,120,64797);

-- Stool (610260) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000412,610260,2736,'0',1722.395900,128.746540,99.905205,4.320198,0,0,0,1,120,64797);

-- Stool (610261) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000413,610261,2736,'0',1724.045200,127.505210,99.872330,3.647290,0,0,0,1,120,64797);

-- Stool (610262) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000414,610262,2736,'0',1724.725700,126.519100,99.912560,3.638561,0,0,0,1,120,64797);

-- Chair (610968) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000415,610968,2736,'0',2047.559100,197.500000,177.009430,4.572764,0,0,0,1,120,64797);

-- Chair (610971) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000416,610971,2736,'0',2048.475800,199.241320,175.822240,6.060352,0,0,0,1,120,64797);

-- Chair (611003) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000417,611003,2736,'0',634.404540,283.263900,88.896280,3.123647,0,0,0,1,120,64797);

-- Stool (611040) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000418,611040,2736,'0',1901.987900,290.720500,125.865530,1.038471,0,0,0,1,120,64797);

-- Stool (611041) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000419,611041,2736,'0',1900.324700,292.017360,125.940970,0.584686,0,0,0,1,120,64797);

-- Stool (611042) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000420,611042,2736,'0',1899.673600,293.411470,125.761840,0.383973,0,0,0,1,120,64797);

-- Campfire (611220) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000421,611220,2736,'0',1746.566000,168.232640,101.749130,0.000000,0,0,0,1,120,64797);

-- Stool (611224) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000422,611224,2736,'0',1267.717000,802.692700,33.169872,3.892087,0,0,0,1,120,64797);

-- Chair (612163) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000423,612163,2736,'0',1903.142300,295.000000,125.892070,4.337147,0,0,0,1,120,64797);

-- Chair (612164) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000424,612164,2736,'0',1888.972300,251.626740,130.297400,0.383973,0,0,0,1,120,64797);

-- Chair (612166) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000425,612166,2736,'0',1721.682300,124.788190,99.898120,1.370734,0,0,0,1,120,64797);

-- Stool (612360) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000426,612360,2736,'0',1266.781200,797.406250,32.845486,2.312558,0,0,0,1,120,64797);

-- 11.2.7 - Housing - Barrel - Rugge.. (613479) x6
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000427,613479,2736,'0',1698.921900,214.142360,99.399670,1.147989,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000428,613479,2736,'0',1692.286500,215.182300,99.151160,5.962599,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000429,613479,2736,'0',1699.231000,212.093750,99.542700,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000430,613479,2736,'0',1696.777800,214.347230,99.271706,5.760652,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000431,613479,2736,'0',1697.998300,215.204860,99.282770,5.928258,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000432,613479,2736,'0',1693.534800,215.848950,99.106580,0.000000,0,0,0,1,120,64797);

-- [DNT] Founder's Point Rope 01 (613604) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000433,613604,2736,'0',1013.213600,-180.734380,60.426014,0.000000,0,0,0,1,120,64797);

-- [DNT] Founder's Point Burlap Sack.. (613669) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000434,613669,2736,'0',1016.845500,-177.901050,59.452793,1.049614,0,0,0,1,120,64797);

-- [DNT] Founder's Point Burlap Sack.. (613670) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000435,613670,2736,'0',1016.968800,-177.840290,60.102955,0.476214,0,0,0,1,120,64797);

-- Red Dye (613804) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000436,613804,2736,'0',1690.932300,208.973950,100.418740,6.275194,0,0,0,1,120,64797);

-- Green Dye (613805) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000437,613805,2736,'0',1695.427100,208.506940,100.423340,6.268989,0,0,0,1,120,64797);

-- Gold Dye (613807) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000438,613807,2736,'0',1690.385400,209.513890,100.441180,6.077439,0,0,0,1,120,64797);

-- [DNT] Razorwind Crate 01 (613819) x11
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000439,613819,2736,'0',1793.581700,170.355910,100.852990,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000440,613819,2736,'0',1795.941000,164.638890,100.874950,6.077930,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000441,613819,2736,'0',1796.557300,163.998260,101.684960,5.758778,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000442,613819,2736,'0',1797.218800,164.003480,100.768240,4.653412,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000443,613819,2736,'0',1796.036500,163.423610,100.867100,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000444,613819,2736,'0',1791.890600,156.338550,100.724730,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000445,613819,2736,'0',1792.142300,151.769100,101.357920,0.240935,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000446,613819,2736,'0',1755.145900,170.467010,101.645420,6.009172,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000447,613819,2736,'0',1012.741300,-173.564240,59.452793,5.404431,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000448,613819,2736,'0',1013.401100,-174.881940,60.444900,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000449,613819,2736,'0',595.753500,121.640630,1.673558,5.794017,0,0,0,1,120,64797);

-- [DNT] Razorwind Crate 02 (613820) x7
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000450,613820,2736,'0',1793.989600,169.026050,100.862070,4.902636,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000451,613820,2736,'0',1792.935800,152.121540,100.391760,4.106182,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000452,613820,2736,'0',1791.670200,151.407990,100.327570,2.045113,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000453,613820,2736,'0',1737.340300,167.288200,101.695080,0.668022,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000454,613820,2736,'0',1013.319500,-174.888890,59.452793,0.953783,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000455,613820,2736,'0',1013.114600,-180.722230,59.452793,1.202026,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000456,613820,2736,'0',594.086800,121.795140,1.673820,0.237225,0,0,0,1,120,64797);

-- [DNT] Razorwind Crate 03 (613821) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000457,613821,2736,'0',1791.510400,172.282990,100.822100,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000458,613821,2736,'0',1019.302100,-175.538200,59.452800,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000459,613821,2736,'0',593.566000,131.340290,1.686625,5.987526,0,0,0,1,120,64797);

-- [DNT] Razorwind Barrel 01 (613822) x17
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000460,613822,2736,'0',1789.996600,185.553820,100.882520,4.680082,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000461,613822,2736,'0',1789.265600,184.564240,100.870030,1.841626,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000462,613822,2736,'0',1790.526000,184.152790,100.826410,3.376932,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000463,613822,2736,'0',1794.947900,170.463550,100.859560,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000464,613822,2736,'0',1794.956700,158.842010,100.836590,5.873539,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000465,613822,2736,'0',1793.800400,158.114580,101.190750,0.408862,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000466,613822,2736,'0',1792.102400,157.574660,100.794670,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000467,613822,2736,'0',1793.043500,156.411450,100.722900,4.680082,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000468,613822,2736,'0',1017.989600,-175.848950,59.452793,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000469,613822,2736,'0',1018.647600,-176.940980,59.452793,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000470,613822,2736,'0',1017.397600,-176.866320,59.452793,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000471,613822,2736,'0',1018.034700,-176.560760,60.835570,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000472,613822,2736,'0',592.260440,130.121540,2.150154,5.272715,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000473,613822,2736,'0',587.402800,168.802080,25.033583,3.696091,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000474,613822,2736,'0',587.180540,170.484380,25.025507,0.319026,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000475,613822,2736,'0',584.630200,179.114580,30.133888,5.422811,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000476,613822,2736,'0',594.951400,211.437500,16.749506,0.000000,0,0,0,1,120,64797);

-- Bench (614034) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000477,614034,2736,'0',1184.191000,-134.347230,73.362740,3.194002,0,0,0,1,120,64797);

-- Bench (614035) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000478,614035,2736,'0',1178.251700,-137.756940,73.478840,0.855211,0,0,0,1,120,64797);

-- Chair (614159) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000479,614159,2736,'0',1676.149300,220.440980,98.515625,4.145160,0,0,0,1,120,64797);

-- Chair (614160) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000480,614160,2736,'0',1676.192700,215.420140,98.904520,5.210291,0,0,0,1,120,64797);

-- Stool (614440) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000481,614440,2736,'0',1722.619800,71.840280,105.431560,5.803221,0,0,0,1,120,64797);

-- Stool (614441) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000482,614441,2736,'0',1723.298600,69.460070,105.463220,1.178096,0,0,0,1,120,64797);

-- Stool (614442) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000483,614442,2736,'0',1725.566000,70.288190,105.463110,2.609261,0,0,0,1,120,64797);

-- Stool (614443) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000484,614443,2736,'0',1724.796900,72.769100,105.428410,4.258607,0,0,0,1,120,64797);

-- Advert: Housing - Generic - Broom.. (614479) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000485,614479,2736,'0',2084.468800,161.833330,176.285350,2.982855,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000486,614479,2736,'0',1910.512200,266.052100,130.495560,5.415659,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000487,614479,2736,'0',1761.567700,211.335070,105.733350,5.415659,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000488,614479,2736,'0',1725.612900,78.956600,106.697980,2.741623,0,0,0,1,120,64797);

-- [DNT] Endeavor Razorwind Gazebo (614891) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000489,614891,2736,'0',1434.307300,-95.067710,64.611790,6.196456,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000490,614891,2736,'0',1746.642300,168.388890,101.804460,0.026191,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Table (614898) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000491,614898,2736,'0',1794.180500,162.586800,100.875830,0.025729,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Trough (614899) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000492,614899,2736,'0',1791.781200,178.996540,100.722510,0.591173,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Sack Pil.. (614901) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000493,614901,2736,'0',1790.356000,182.512160,100.785000,1.180762,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000494,614901,2736,'0',1796.619800,166.022570,100.818310,4.138326,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000495,614901,2736,'0',1794.505200,161.032990,101.500040,2.678605,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000496,614901,2736,'0',1787.783000,151.394100,100.005930,3.252437,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Sack Pil.. (614902) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000497,614902,2736,'0',1789.828100,152.064240,100.248730,0.463729,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000498,614902,2736,'0',1009.071200,-175.104170,59.452793,0.000000,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Sack 6 (614903) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000499,614903,2736,'0',1794.345500,171.472230,100.851150,4.786806,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000500,614903,2736,'0',1794.609400,157.684040,100.779430,1.372879,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000501,614903,2736,'0',1007.543400,-176.378480,59.452793,6.084012,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Rug 1 (614905) x3
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000502,614905,2736,'0',1434.279500,-95.092020,64.482820,5.093810,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000503,614905,2736,'0',1792.595500,162.420140,100.866310,5.818457,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000504,614905,2736,'0',1017.762100,-176.736110,59.452793,6.196859,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Rug 2 (614906) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000505,614906,2736,'0',1791.524300,169.935760,100.841470,0.627872,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000506,614906,2736,'0',1790.949700,155.494800,100.658780,5.481770,0,0,0,1,120,64797);

-- Hay Pile (615910) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000507,615910,2736,'0',988.843750,-161.159730,58.905884,2.809953,0,0,0,1,120,64797);

-- [DNT] Razorwind Wood Plank 01 (616517) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000508,616517,2736,'0',1011.987900,-178.421880,62.109524,5.395431,0,0,0,1,120,64797);

-- [DNT] Razorwind Wood Plank Narrow.. (616518) x4
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000509,616518,2736,'0',1012.871500,-178.244800,62.196280,3.627247,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000510,616518,2736,'0',1010.184000,-177.331600,61.704468,0.680280,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000511,616518,2736,'0',1010.689300,-177.109380,61.704468,0.345070,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000512,616518,2736,'0',1011.855900,-178.208330,62.508247,5.416702,0,0,0,1,120,64797);

-- [DNT] Razorwind Lumber Axe (616523) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000513,616523,2736,'0',1018.699600,-177.567700,60.353558,3.470118,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000514,616523,2736,'0',1013.625000,-174.246540,60.280890,5.993312,0,0,0,1,120,64797);

-- [DNT] Endeavor Razrowind Rug 1 - .. (616532) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000515,616532,2736,'0',1009.013900,-174.800350,59.452793,0.000000,0,0,0,1,120,64797);

-- [DNT] Endeavor Razorwind Wagon (616685) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000516,616685,2736,'0',1011.345500,-177.769100,59.452793,0.633475,0,0,0,1,120,64797);

-- [DNT] Razorwind Log Pile - Small (616686) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000517,616686,2736,'0',1011.539900,-177.803820,61.176250,5.376676,0,0,0,1,120,64797);

-- [DNT] Razorwind Curtain 03 (616688) x5
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000518,616688,2736,'0',1013.069500,-176.416670,63.342937,0.672199,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000519,616688,2736,'0',1427.220500,-98.303820,70.780060,0.446107,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000520,616688,2736,'0',1434.965300,-87.189240,70.780060,4.674272,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000521,616688,2736,'0',1440.994800,-99.512150,70.780060,2.535511,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000522,616688,2736,'0',1008.857700,-179.381940,63.360890,0.631643,0,0,0,1,120,64797);

-- [DNT] Razorwind Brazier (616689) x2
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000523,616689,2736,'0',1434.352400,-95.062500,64.482820,0.000000,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000524,616689,2736,'0',1030.911500,-170.710070,58.469807,6.097950,0,0,0,1,120,64797);

-- Pet Food and Water (616870) x5
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000525,616870,2736,'0',1757.319500,214.888890,105.293240,0.262885,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000526,616870,2736,'0',1704.670200,210.998260,99.934670,4.949419,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000527,616870,2736,'0',1728.286500,114.250000,99.782974,2.767554,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000528,616870,2736,'0',1656.897600,171.548610,98.758490,3.158795,0,0,0,1,120,64797);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000529,616870,2736,'0',1660.185800,234.635420,97.624960,5.507559,0,0,0,1,120,64797);

-- Chair (617550) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000530,617550,2736,'0',1708.434100,208.805560,100.340280,4.834563,0,0,0,1,120,64797);

-- Bench (617626) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000531,617626,2736,'0',1760.632400,225.390000,107.030235,3.499390,0,0,0,1,120,64797);

-- Chair (617627) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000532,617627,2736,'0',1766.859300,229.491030,106.974050,0.314160,0,0,0,1,120,64797);

-- Bench (618419) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000533,618419,2736,'0',490.199650,215.814240,130.880130,2.146753,0,0,0,1,120,64797);

-- Chair (618421) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000534,618421,2736,'0',1527.487900,51.890625,175.704570,2.993224,0,0,0,1,120,64797);

-- Chair (618465) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000535,618465,2736,'0',845.156250,450.352450,141.087840,2.635444,0,0,0,1,120,64797);

-- Bench (618466) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000536,618466,2736,'0',860.366330,459.057280,141.072600,0.418879,0,0,0,1,120,64797);

-- Bench (618467) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000537,618467,2736,'0',862.942700,456.611100,141.071330,1.186823,0,0,0,1,120,64797);

-- Bench (618581) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000538,618581,2736,'0',1989.704800,381.380220,183.602430,2.007128,0,0,0,1,120,64797);

-- Bench (618586) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000539,618586,2736,'0',1991.151000,387.753480,183.602430,3.717554,0,0,0,1,120,64797);

-- Bench (618587) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000540,618587,2736,'0',1983.909800,383.296880,183.602430,0.549779,0,0,0,1,120,64797);

-- Bench (618593) x1
INSERT INTO `gameobject` (`guid`,`id`,`map`,`spawnDifficulties`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`VerifiedBuild`) VALUES
(9000541,618593,2736,'0',1985.557300,388.743070,183.562910,5.235988,0,0,0,1,120,64797);

-- ================================================================
-- QUEST DATA
-- ================================================================

-- Quest 91863: My First Home
DELETE FROM `quest_template` WHERE `ID`=91863;
INSERT INTO `quest_template` (`ID`,`QuestInfoID`,`Flags`,`FlagsEx`,`FlagsEx2`,`RewardBonusMoney`,`RewardSpell`,`PortraitGiver`,`PortraitGiverModelSceneID`,`LogTitle`,`LogDescription`,`QuestDescription`,`QuestCompletionLog`,`PortraitGiverText`,`RewardItem1`,`RewardAmount1`,`VerifiedBuild`) VALUES
(91863,282,41418752,4202624,524296,23400,1272733,137995,1356,'My First Home','Familiarize yourself with the neighborhood and its residents, and then find a place to build a house.','Welcome to the neighborhood! Feel free to explore and have a look at the plots available.\n\nSpeak to the steward to learn more about the process of becoming a homeowner or if you wish to create your own neighborhood.','Have you found your forever home?','Interact with a For Sale Sign to purchase a house.',0,0,64797);

DELETE FROM `quest_objectives` WHERE `QuestID`=91863;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463615,91863,0,0,0,249851,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(464280,91863,3,1,1,233063,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(464281,91863,3,2,2,233708,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463609,91863,0,3,3,248857,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463630,91863,19,4,4,38951,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463631,91863,19,5,5,38956,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463632,91863,19,6,6,38955,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463633,91863,19,7,7,38954,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463634,91863,19,8,8,38953,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463635,91863,19,9,9,38952,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463636,91863,19,10,10,38957,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463637,91863,19,11,11,38959,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463638,91863,19,12,12,38958,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463639,91863,19,13,13,38961,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463640,91863,19,14,14,38962,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463641,91863,19,15,15,38960,0,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463613,91863,0,16,16,248860,6,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463610,91863,0,17,17,248858,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463746,91863,0,18,18,249093,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(463747,91863,0,19,19,249093,2,64797);

DELETE FROM `creature_queststarter` WHERE `quest`=91863;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (233708,91863,64797);
DELETE FROM `creature_questender` WHERE `quest`=91863;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (233708,91863,64797);

-- Quest 93087: Decor Treasure Hunt
DELETE FROM `quest_template` WHERE `ID`=93087;
INSERT INTO `quest_template` (`ID`,`QuestInfoID`,`Flags`,`FlagsEx`,`FlagsEx2`,`RewardBonusMoney`,`RewardSpell`,`PortraitGiver`,`PortraitGiverModelSceneID`,`LogTitle`,`LogDescription`,`QuestDescription`,`QuestCompletionLog`,`PortraitGiverText`,`RewardItem1`,`RewardAmount1`,`VerifiedBuild`) VALUES
(93087,0,40894464,8320,8,117000,0,0,0,'Decor Treasure Hunt','Solve the riddle and find the treasure.','Are you ready for today\'s challenge? I\'ve hidden a piece from my collection here in the neighborhood. To find it, you must merely solve this simple riddle:\n\nBelow the towers.\nTownsfolk guarded from the spray.\nDig in awning\'s shade.\n\nGood luck!','','',246260,1,64797);

DELETE FROM `quest_objectives` WHERE `QuestID`=93087;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(466091,93087,2,0,0,587703,1,64797);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`VerifiedBuild`) VALUES
(468626,93087,2,1,1,587681,1,64797);

DELETE FROM `creature_queststarter` WHERE `quest`=93087;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (253596,93087,64797);
DELETE FROM `creature_questender` WHERE `quest`=93087;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (253596,93087,64797);

-- ================================================================
-- GOSSIP MENUS
-- ================================================================

DELETE FROM `gossip_menu` WHERE `MenuID`=35728;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES (35728,25033,64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=35728 AND `OptionID`=0;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(35728,123435,0,8,'Show me where I can fly.',64797);

DELETE FROM `gossip_menu` WHERE `MenuID`=40076;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES (40076,294617,64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=40076 AND `OptionID`=0;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(40076,136778,0,0,'Who are you?',64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=40076 AND `OptionID`=1;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(40076,136776,1,0,'Where can I find decor?',64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=40076 AND `OptionID`=2;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(40076,136775,2,0,'Are you really giving away treasure?',64797);

DELETE FROM `gossip_menu` WHERE `MenuID`=41365;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES (41365,1,64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=41365 AND `OptionID`=0;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(41365,137155,0,0,'I\'d like to upgrade my house.',64797);

DELETE FROM `gossip_menu` WHERE `MenuID`=41367;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES (41367,303805,64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=41367 AND `OptionID`=0;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(41367,137154,0,0,'I\'ll be back.',64797);

DELETE FROM `gossip_menu` WHERE `MenuID`=41385;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES (41385,303853,64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=41385 AND `OptionID`=0;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(41385,137178,0,0,'Can I use this device to make dyes?',64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=41385 AND `OptionID`=1;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(41385,137177,1,0,'Where else can I obtain dyes?',64797);
DELETE FROM `gossip_menu_option` WHERE `MenuID`=41385 AND `OptionID`=2;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`VerifiedBuild`) VALUES
(41385,137571,2,5,'I\'d like to learn the basics of Inscription or Alchemy.',64797);

-- ================================================================
-- CORNERSTONE (457142) POSITIONS = Housing Plot Locations
-- 55 unique positions = 55 plots in neighborhood
-- ================================================================

-- Plot   1: (  224.1267,   652.3698,    12.1858)
-- Plot   2: (  304.6562,    79.8490,    24.3628)
-- Plot   3: (  380.6649,  -107.6580,     6.7529)
-- Plot   4: (  403.6076,   192.3507,   109.7986)
-- Plot   5: (  488.3160,   304.3420,    99.2083)
-- Plot   6: (  544.6215,   637.1024,   155.3021)
-- Plot   7: (  574.2917,  -316.3924,     7.1435)
-- Plot   8: (  637.6632,  -114.5069,     2.9462)
-- Plot   9: (  639.6736,   708.3299,   113.7830)
-- Plot  10: (  649.9757,    39.9340,     3.9184)
-- Plot  11: (  669.4583,   432.2535,     9.0017)
-- Plot  12: (  733.8542,  -434.1736,     2.3941)
-- Plot  13: (  745.5347,   840.3368,     8.7535)
-- Plot  14: (  772.8715,  -477.5243,    11.7049)
-- Plot  15: (  791.3958,  -208.5312,    15.6007)
-- Plot  16: (  902.3055,  -545.7639,     1.4306)
-- Plot  17: (  955.0191,   477.2552,   108.6615)
-- Plot  18: (  958.1059,   843.5764,     4.7882)
-- Plot  19: ( 1023.8299,   164.7917,    31.1076)
-- Plot  20: ( 1032.7848,   695.1458,     9.0087)
-- Plot  21: ( 1078.1823,  1023.2518,     3.1476)
-- Plot  22: ( 1101.6841,   883.5469,     6.8333)
-- Plot  23: ( 1129.5192,   770.1805,    17.6215)
-- Plot  24: ( 1165.7274,   156.8663,    34.9566)
-- Plot  25: ( 1166.5642,   464.5434,   154.0208)
-- Plot  26: ( 1180.1493,  -643.6736,     5.7066)
-- Plot  27: ( 1215.6442,    -9.7222,   104.4635)
-- Plot  28: ( 1256.1805,  1091.0452,    55.9184)
-- Plot  29: ( 1266.1024,  -171.2188,    72.9337)
-- Plot  30: ( 1309.7327,  -105.3681,    73.2639)
-- Plot  31: ( 1339.8959,   616.3472,    42.0035)
-- Plot  32: ( 1343.7760,  -635.8611,    21.1667)
-- Plot  33: ( 1366.4080,    85.1840,    49.7309)
-- Plot  34: ( 1493.4132,     7.5816,    78.3281)
-- Plot  35: ( 1531.5330,   998.0191,    36.2552)
-- Plot  36: ( 1607.9132,  -242.9219,    76.2830)
-- Plot  37: ( 1640.0226,  -567.5486,    99.9861)
-- Plot  38: ( 1658.4149,  -169.7552,    81.0330)
-- Plot  39: ( 1663.2812,   915.1875,    37.6111)
-- Plot  40: ( 1665.5017,  -677.5955,    99.7431)
-- Plot  41: ( 1668.4062,  1053.1858,    48.4670)
-- Plot  42: ( 1757.3854,  -865.4340,   116.7535)
-- Plot  43: ( 1788.5903,  -951.0660,    92.3142)
-- Plot  44: ( 1802.2135,   639.2188,    85.6007)
-- Plot  45: ( 1844.2587,   664.7674,    88.5258)
-- Plot  46: ( 1846.5000,  -879.5261,   120.7118)
-- Plot  47: ( 1864.5416,  -515.5191,   130.2292)
-- Plot  48: ( 1883.4618,  -337.9045,   129.9809)
-- Plot  49: ( 1903.5747,  -448.2951,   137.0035)
-- Plot  50: ( 1925.2101,   948.8993,   101.6458)
-- Plot  51: ( 1925.5990,  -707.2083,   134.0330)
-- Plot  52: ( 2079.9620,  -272.8559,   138.3177)
-- Plot  53: ( 2150.5642,  -740.0920,   141.6667)
-- Plot  54: ( 2235.2551,  -871.0590,   160.2274)
-- Plot  55: ( 2269.2761,  -586.6858,   161.4757)

-- ================================================================
-- HOUSING ENTRANCE PORTAL (Map 1 / Kalimdor)
-- ================================================================

-- Portal to Razorwind Shores (543406) - on Map 1
DELETE FROM `gameobject_template` WHERE `entry`=543406;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`VerifiedBuild`) VALUES
(543406,22,0,'Portal to Razorwind Shores','',1.0,2736,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64797);

-- ================================================================
-- AREA TRIGGERS
-- ================================================================

-- Housing zone boundary (Kalimdor side)
-- AreaTrigger 8018: pos (1630.03, -4435.12, 16.70) on Map 0

-- Quest 91863 tour stops (type 19 objectives)
-- NOTE: These are NOT standard CMSG_AREA_TRIGGER packets;
-- they use quest-internal AreaTriggerEnter tracking mechanism
-- IDs: 38951-38962 (12 stops, positions from AreaTrigger DB2)

-- ================================================================
-- HOUSING SPELLS (reference)
-- ================================================================

-- Spell 432021: Housing Zone Aura (2h duration, applied on map entry)
-- Spell 1235590: Housing Transfer (portal teleport spell, instant cast)
-- Spell 1266097: Cornerstone Placement (from GO Data[8])
-- Spell 1272733: Quest 91863 reward spell

-- ================================================================
-- MAP TRANSFER SEQUENCE (Map 1 -> Map 2736)
-- ================================================================

-- 1. Player interacts with Portal GO 543406 on Map 1 (Kalimdor)
-- 2. Spell 1235590 cast (instant, target=DestinationLocation)
-- 3. SMSG_TRANSFER_PENDING: Map 2736, pos (2061.13, 183.18, 175.1)
-- 4. SMSG_NEW_WORLD: arrival (1440.579, -4427.840, 25.454)
-- 5. Housing aura 432021 applied (2h duration)
-- 6. Advanced flying enabled on Map 2736

-- ================================================================
-- WORLD STATES (Map 2736)
-- ================================================================

-- WorldState 13437 = 326471669
-- WorldState 13438 = 44689263

-- Total: 1279 creatures, 542 GOs, 2 quests, 5 gossip menus, 55 plots
-- Faction: HORDE (Razorwind Shores neighborhood)
-- Alliance has a separate equivalent neighborhood
