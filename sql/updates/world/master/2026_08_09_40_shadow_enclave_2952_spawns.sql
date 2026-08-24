--
-- Shadow Enclave (delve map 2952) - creature + gameobject roster
-- ==============================================================
--
-- THE PROBLEM
-- -----------
-- `delve_template` id 20 (mapId 2952, scenarioId 3154, zoneId 16594, finalBossEntry 246717 "Lord Antenorian")
-- is fully configured, but map 2952 has ZERO creature and ZERO gameobject spawns - the delve is empty and
-- therefore unrunnable no matter what the delve code does. Verified in EVERY world DB on this box:
-- integ_world, world, tc_world, wowc_world, wc_world, playerbot_world all return 0 for both
-- `SELECT COUNT(*) FROM creature WHERE map=2952` and the gameobject equivalent. Control for the method:
-- the same query on map 2679 (an already-populated delve) returns 94.
--
-- WHERE THE DATA COMES FROM
-- -------------------------
-- Packet capture `C:/sniff/alliance_deatholme_delve/dumps/dump_12.0.1.66562_2026-03-26_08-04-06.pkt`
-- (custom PKT 3.1, client build 12.0.1.66562) - one complete run of this delve, 12.7 minutes on the wire.
-- Extraction is reproducible: `python C:/dumps/extract_shadow_enclave_2952.py` regenerates
-- `C:/dumps/shadow_enclave_2952_roster.json` and `C:/dumps/shadow_enclave_2952_extract.log`.
--
-- Opcodes were identified empirically for 66562 (they differ from 68275):
--   SMSG_UPDATE_OBJECT = 0x580000 - isolated by scanning every SMSG for LE uint16 0x0B88 (2952) at payload
--     offset 4 (TC's UpdateData::BuildPacket writes uint16 MapID first, then uint32 BlockCount): 2878 hits vs
--     2 for the next-best candidate. Proof beyond frequency: all 3193 records of that opcode decode with a
--     zero-failure block chain, each consuming exactly its declared DataSize.
--   SMSG_NEW_WORLD = 0x42002B - two records at tick 54033 carry MapID 2952 and
--     (-16.6948, 232.9746, 265.4615, 3.7535), byte-exact to `delve_template` id 20's entry position; the
--     record at tick 816681 carries MapID 0 and (4781.2339, -4121.1699, 31.0185, 0.8003), byte-exact to its
--     exit position. All map-2952 object traffic is strictly bracketed by those two.
--
-- CONTROLS
--   * Positive control on a different map with the identical decoder: map 0 in the same capture yields 536
--     create blocks (118 Creature, 86 GameObject, 318 Item, 10 Player) - so an empty result for 2952 would
--     have been meaningful.
--   * Independent coordinate frame: the player's own movement heartbeats (CMSG 0x3E002E, 925/925 decoded by a
--     path that touches no create block) trace x[-98.16..169.12] y[-186.85..255.15] z[200.38..272.91],
--     fully inside the extracted object bounding box.
--   * Closure: of the 250 map-2952 Creature/GameObject GUIDs seen anywhere in the capture, all 250 have a
--     create block; zero appear only in VALUES blocks.
--
-- WHAT IS IMPORTED, AND WHAT IS NOT
-- ---------------------------------
-- The capture contains 190 creature positions and 60 gameobject positions. Importing all of them verbatim
-- would bake encounter summons into the static world tables. They are split by WHEN the create block arrived:
--   STATIC   = arrived in the arrival burst, <= +3564 ms after SMSG_NEW_WORLD.
--   RUNTIME  = everything later. The boundary is not a chosen threshold: the next create after the burst is
--              at +97992 ms - a 94-second empty corridor.
-- Two facts close the "late creates are just grid loading" hole: across the whole run there were ZERO
-- out-of-range events and ZERO recreations (so visibility never dropped and re-added anything), and every
-- late object sits 0.8-14.2 yd from an object the burst had already loaded, inside the burst bounding box.
--
-- IMPORTED: 142 creature positions / 27 entries, 34 gameobject positions / 21 entries.
-- DELIBERATELY NOT IMPORTED:
--   * 42 creature + 26 gameobject RUNTIME positions - scripted set-pieces and summons (Summoned Voidbreaker /
--     Voidbinder / Siphoid Prime, [DNT] Portal, Pursue Antenorian, the +212037 ritual batch, ...).
--   * 3 `static_respawn_duplicate` rows (Rat x2, Dusty Firefly x1) - respawns of burst spawn points that are
--     already imported; adding them would duplicate the spawn point.
--   * The campfire pair 433898 "Campfire (INVISIBLE)" + 618844 "Delve Campfire": one physical campfire that
--     MOVED 231.7 yd mid-run. Only the burst instance (+3564 ms, -34.0/220.7/264.3) is imported. There is no
--     second spawn.
--   * 248453 "Adventurer's Echo" (6 GUIDs, 3 runtime + 3 unclassified_recurring). None is in the arrival
--     burst, so there is no static anchor and importing them would assert spawn points the capture does not
--     evidence. Recorded as unresolved in SANCTUM_INERT_SWEEP_68275.md; unblocked by a second capture of the
--     same delve - if the Echoes appear in that run's arrival burst they are static and can be added then.
--
-- TEMPLATE VERIFICATION (integ_world, this session)
--   All 27 creature entries exist in `creature_template`. All 21 gameobject entries exist in
--   `gameobject_template`. NOTHING IS MISSING and no template row is invented by this file.
--   Control: the same query resolves 246717 and does not resolve 999999999.
--
-- FINAL BOSS. 246717 "Lord Antenorian" - the delve's configured `finalBossEntry` - IS in the static set,
-- exactly one spawn at (127.4583, -56.4132, 215.5835, o 5.5634). That is inside the object bounding box
-- (x -111.16..189.88, y -192.75..258.23, z 200.44..274.59) and inside the region the player's own heartbeats
-- traversed, i.e. reachable on foot from the entry point (-16.695, 232.975, 265.462).
--
-- GUID RANGE. @CGUID / @OGUID := 13000000, verified free this session:
--   creature   MAX(guid) = 12000357 below the 50M bot block (which starts at 50000001); the 13M decade holds 0 rows
--   gameobject MAX(guid) = 12000201; the 13M decade holds 0 rows
--   no `SET @CGUID`/`SET @OGUID` anywhere under sql/updates/world in any worktree on this box allocates a
--   13xxxxxx value (highest existing allocations are 11800100 and 12xxxxx).
--
-- FIELD SOURCING
--   position_x/y/z/orientation come straight from the create blocks.
--   `spawnDifficulties` = '208' is the convention every already-populated delve map on this server uses
--     (maps 2664/2679/2680/2681/2683/2684/2685/2686/2688/2689/2690/2803/2962 - 798 creature rows, 100% '208').
--   gameobject rotation is the unit quaternion of the captured orientation (0, 0, sin(o/2), cos(o/2)) - derived,
--     not invented; the capture's create block was not decoded far enough to recover a tilted quaternion.
--   `animprogress` 255 / `state` 1 / `spawntimesecs` 180 match the existing delve gameobject rows on map 2680.
--     GO state and animprogress are NOT derivable from this capture (they live in the UF::GameObject update-field
--     fragment, which the extractor skips), so these are the table defaults used by the sibling delve maps and
--     not a claim about the capture.
--   modelid / equipment_id / npcflag / unit_flags* are left at 0/NULL as on every existing delve spawn.
--
-- NO NEW TABLES and NO NEW PREPARED STATEMENTS are introduced by this file.
-- Idempotent (guid-range DELETE then INSERT).
--
SET @CGUID := 13000000;
SET @OGUID := 13000000;

DELETE FROM `creature_addon`       WHERE `guid` BETWEEN @CGUID AND @CGUID + 141;
DELETE FROM `game_event_creature`  WHERE `guid` BETWEEN @CGUID AND @CGUID + 141;
DELETE FROM `creature`             WHERE `guid` BETWEEN @CGUID AND @CGUID + 141;
DELETE FROM `game_event_gameobject` WHERE `guid` BETWEEN @OGUID AND @OGUID + 33;
DELETE FROM `gameobject`            WHERE `guid` BETWEEN @OGUID AND @OGUID + 33;

INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`) VALUES
(@CGUID + 0  , 207283, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     41.3420,    -72.9410,   207.1032,  6.1765, 180, 0, 0, 100, 0),  -- Delvers' Supplies
(@CGUID + 1  , 207283, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -30.1094,    134.6476,   255.0198,  6.1765, 180, 0, 0, 100, 0),  -- Delvers' Supplies
(@CGUID + 2  , 209780, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     47.1024,    -70.0538,   207.5396,  5.8005, 180, 0, 0, 100, 0),  -- Abandoned Restoration Stone
(@CGUID + 3  , 212807, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    170.5365,   -101.8663,   218.6115,  0.0000, 180, 0, 0, 100, 0),  -- Stalker
(@CGUID + 4  , 221379, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     16.0799,    258.0312,   274.3101,  3.7874, 180, 0, 0, 100, 0),  -- Generic Bunny
(@CGUID + 5  , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -52.4410,     56.8368,   203.4557,  3.0945, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 6  , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -48.4410,     56.8368,   204.6195,  4.6088, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 7  , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -50.4410,     58.8368,   204.5150,  2.6699, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 8  , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      9.6545,    -26.6806,   204.0293,  0.0000, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 9  , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      7.6545,    -28.6806,   204.3882,  3.8006, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 10 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     11.6545,    -28.6806,   204.8343,  0.0000, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 11 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      4.1979,     96.2066,   219.5024,  0.0000, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 12 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      8.1979,     96.2066,   219.6739,  6.1109, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 13 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      6.1979,     98.2066,   219.6444,  4.5283, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 14 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     38.6562,   -155.5365,   208.7654,  4.7382, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 15 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     36.6562,   -157.5365,   208.7858,  4.5140, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 16 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     40.6562,   -157.5365,   208.7062,  3.3695, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 17 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -16.6128,   -170.2639,   209.2833,  1.4220, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 18 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -14.6128,   -168.2639,   209.2833,  3.1440, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 19 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -12.6128,   -170.2639,   209.2833,  0.0000, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 20 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     31.8712,    137.3219,   219.4945,  0.2660, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 21 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     27.9253,    138.7917,   219.5231,  0.0000, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 22 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     27.7010,    135.8349,   219.5119,  5.7890, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 23 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -44.4100,      2.0627,   203.6465,  2.8331, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 24 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -46.4968,     -0.6199,   203.3913,  3.1752, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 25 , 246348, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -40.4809,     -0.5521,   203.4075,  0.0000, 180, 0, 0, 100, 0),  -- Shadowspawn
(@CGUID + 26 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -13.8490,    -11.0920,   202.3423,  2.5210, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 27 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -11.8490,    -13.0920,   202.2833,  0.5782, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 28 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -37.6181,    -88.3385,   208.0611,  2.5573, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 29 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -39.6181,    -86.3385,   208.0726,  5.9419, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 30 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     71.1910,    123.6181,   231.3294,  0.1570, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 31 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     73.1910,    121.6181,   231.3723,  0.0000, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 32 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     15.5608,   -181.2517,   209.0802,  4.6811, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 33 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -76.6631,     47.7867,   201.3189,  3.9398, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 34 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     26.9913,    173.8565,   242.6682,  4.0841, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 35 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     36.5226,    100.2158,   219.7027,  2.0004, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 36 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     36.4459,     98.5443,   219.6265,  5.2450, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 37 , 246350, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -40.1917,     -0.4630,   203.3149,  0.0389, 180, 0, 0, 100, 0),  -- Cult Adept
(@CGUID + 38 , 246351, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -13.8490,    -13.0920,   202.3069,  2.4080, 180, 0, 0, 100, 0),  -- Shadowstone Elemental
(@CGUID + 39 , 246351, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -39.6181,    -88.3385,   208.0751,  0.0000, 180, 0, 0, 100, 0),  -- Shadowstone Elemental
(@CGUID + 40 , 246351, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     25.1215,    173.2691,   243.2670,  2.3387, 180, 0, 0, 100, 0),  -- Shadowstone Elemental
(@CGUID + 41 , 246351, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     97.3941,    -90.6406,   211.0935,  0.0000, 180, 0, 0, 100, 0),  -- Shadowstone Elemental
(@CGUID + 42 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -50.1867,     56.5774,   204.2605,  4.7749, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 43 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -3.6545,    -62.1701,   208.0802,  4.2069, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 44 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      6.1979,     96.2066,   219.6232,  2.0986, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 45 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     71.1910,    121.6181,   231.3506,  0.0000, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 46 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     17.5608,   -181.2517,   209.1433,  0.0000, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 47 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     25.1215,    175.2691,   243.1827,  0.0000, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 48 , 246352, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     35.3628,     98.3819,   219.6740,  1.7579, 180, 0, 0, 100, 0),  -- Bladesworn Cultist
(@CGUID + 49 , 246357, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -14.6128,   -170.2639,   209.2833,  0.2451, 180, 0, 0, 100, 0),  -- Shadowburn Vortex
(@CGUID + 50 , 246359, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      9.6545,    -28.6806,   204.4813,  0.5377, 180, 0, 0, 100, 0),  -- Ogre Browbeater
(@CGUID + 51 , 246359, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -5.6545,    -62.1701,   208.0388,  4.8857, 180, 0, 0, 100, 0),  -- Ogre Browbeater
(@CGUID + 52 , 246359, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     38.6562,   -157.5365,   208.7460,  2.9902, 180, 0, 0, 100, 0),  -- Ogre Browbeater
(@CGUID + 53 , 246359, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -77.8013,     47.3146,   201.1091,  4.4366, 180, 0, 0, 100, 0),  -- Ogre Browbeater
(@CGUID + 54 , 246359, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     93.6706,    -88.1311,   211.1003,  2.1726, 180, 0, 0, 100, 0),  -- Ogre Browbeater
(@CGUID + 55 , 246359, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     26.0098,    137.4244,   219.4647,  2.8226, 180, 0, 0, 100, 0),  -- Ogre Browbeater
(@CGUID + 56 , 246361, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -9.4062,     84.9340,   219.1649,  0.6283, 180, 0, 0, 100, 0),  -- Umbral Skullcrusher
(@CGUID + 57 , 246717, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    127.4583,    -56.4132,   215.5835,  5.5634, 180, 0, 0, 100, 0),  -- Lord Antenorian
(@CGUID + 58 , 248567, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -31.5747,    222.4115,   264.5180,  3.7589, 180, 0, 0, 100, 0),  -- Valeera Sanguinar
(@CGUID + 59 , 250266, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -18.6215,     90.3750,   219.3926,  0.6652, 180, 0, 0, 100, 0),  -- Void Focus
(@CGUID + 60 , 250266, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -8.1823,     73.2969,   219.3985,  0.8030, 180, 0, 0, 100, 0),  -- Void Focus
(@CGUID + 61 , 250275, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    117.1389,    -47.6840,   214.8800,  5.7136, 180, 0, 0, 100, 0),  -- Antenorian's Devoted
(@CGUID + 62 , 250275, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    139.1389,    -45.5972,   216.7596,  3.9068, 180, 0, 0, 100, 0),  -- Antenorian's Devoted
(@CGUID + 63 , 250275, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    118.3681,    -69.4219,   216.6265,  0.9349, 180, 0, 0, 100, 0),  -- Antenorian's Devoted
(@CGUID + 64 , 250319, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -18.0087,     79.6354,   220.7617,  3.8865, 180, 0, 0, 100, 0),  -- Void Barricade
(@CGUID + 65 , 250351, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     18.0833,    104.1962,   219.5441,  0.0000, 180, 0, 0, 100, 0),  -- Pursue Antenorian
(@CGUID + 66 , 250354, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    144.1424,    -70.6076,   217.3679,  2.4113, 180, 0, 0, 100, 0),  -- [DNT] Portal
(@CGUID + 67 , 251534, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    128.4549,    -57.2118,   215.6902,  0.0000, 180, 0, 0, 100, 0),  -- [DNT] Dummy Bunny
(@CGUID + 68 , 251895, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -15.8524,     72.7118,   217.4650,  3.7937, 180, 0, 0, 100, 0),  -- Void Barricade
(@CGUID + 69 , 253267, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     72.5920,    148.6562,   233.9828,  4.3075, 180, 0, 0, 100, 0),  -- Wailing Spirit
(@CGUID + 70 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -21.3403,    149.7252,   254.9546,  0.9974, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 71 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -19.9488,    144.6770,   254.9140,  2.4294, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 72 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -28.7497,    151.1613,   255.0529,  3.4757, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 73 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     12.9224,     85.1202,   219.5906,  5.2419, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 74 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     14.8275,     99.7491,   219.5906,  4.7246, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 75 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -43.7292,   -128.0711,   209.2000,  3.0615, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 76 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    123.5959,    -43.6860,   214.2636,  5.9710, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 77 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    179.9447,     55.5232,   212.6226,  0.8836, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 78 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     64.3136,     36.3499,   217.6162,  3.4224, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 79 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -31.6922,    227.1538,   264.6010,  1.3325, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 80 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -25.5232,    235.1904,   264.8744,  1.2376, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 81 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -5.2117,    -18.2699,   202.5145,  4.5071, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 82 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -91.8155,     47.7977,   200.5259,  3.6746, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 83 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -86.6425,     48.8227,   200.6429,  1.1723, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 84 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -63.1783,      4.1348,   202.1119,  2.7856, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 85 , 257571, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     95.2153,   -103.4986,   209.3159,  4.0255, 180, 0, 0, 100, 0),  -- Rat
(@CGUID + 86 , 257586, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      6.8641,    133.7838,   220.6427,  5.6780, 180, 0, 0, 100, 0),  -- Vorewing
(@CGUID + 87 , 257586, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     76.7001,     42.2739,   218.5073,  1.3515, 180, 0, 0, 100, 0),  -- Vorewing
(@CGUID + 88 , 257586, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      3.9531,   -192.7500,   211.5897,  4.9641, 180, 0, 0, 100, 0),  -- Vorewing
(@CGUID + 89 , 257586, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      2.2289,   -192.7092,   211.0504,  6.2595, 180, 0, 0, 100, 0),  -- Vorewing
(@CGUID + 90 , 257586, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -66.5096,      3.4624,   202.8866,  3.1748, 180, 0, 0, 100, 0),  -- Vorewing
(@CGUID + 91 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    188.9273,     30.6311,   216.6481,  4.4591, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 92 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -29.3578,   -132.0092,   210.2011,  2.9005, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 93 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    123.8470,    -43.5700,   215.2799,  1.8045, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 94 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -9.0322,    -56.0368,   209.2962,  3.3577, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 95 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     73.3049,     38.3793,   218.5906,  1.4773, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 96 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      3.3862,   -188.1502,   210.1834,  1.2237, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 97 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -29.4978,    231.6057,   265.6986,  1.4064, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 98 , 257587, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     87.7632,   -111.0396,   209.5087,  0.6854, 180, 0, 0, 100, 0),  -- Moth
(@CGUID + 99 , 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    189.8767,     33.3968,   214.9650,  1.1027, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 100, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     10.4490,     94.3212,   220.6184,  2.0073, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 101, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    177.5955,     63.1562,   212.1806,  0.0000, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 102, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -7.6210,    -59.7631,   208.8767,  2.1546, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 103, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     11.5929,    135.0260,   220.4792,  0.8976, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 104, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -8.2830,    -17.1059,   202.5787,  0.0000, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 105, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -88.3212,     48.9977,   201.9618,  1.4665, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 106, 257589, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    100.4218,   -106.2645,   210.2929,  0.3348, 180, 0, 0, 100, 0),  -- Dusty Firefly
(@CGUID + 107, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    188.5603,     33.3314,   214.7661,  1.3929, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 108, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -26.7823,   -132.8796,   209.3418,  3.2218, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 109, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    126.3443,    -43.5514,   214.6319,  4.5038, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 110, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    181.8326,     48.4860,   213.1008,  4.9935, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 111, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -4.5528,    -66.4589,   207.9628,  2.9534, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 112, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     22.7054,    131.8647,   219.3902,  1.3980, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 113, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -8.1976,    -27.0465,   203.8884,  4.7210, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 114, 257590, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -72.4184,      7.5322,   200.9969,  6.2306, 180, 0, 0, 100, 0),  -- Dusty Cockroach
(@CGUID + 115, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      2.2726,    -54.0567,   208.4817,  3.7396, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 116, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -83.4460,     14.7607,   200.8163,  2.6927, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 117, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -19.9892,    -10.7022,   202.4337,  5.6893, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 118, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      0.6701,    162.9844,   251.6336,  0.4876, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 119, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     79.6024,   -100.8472,   209.2299,  0.0000, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 120, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      1.9392,   -188.9444,   209.1545,  1.4664, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 121, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     10.7741,   -183.4848,   209.1035,  0.5857, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 122, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -40.2070,   -121.6615,   209.2151,  1.7065, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 123, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -0.5451,    -67.7309,   208.1914,  1.8994, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 124, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     62.4319,   -138.1648,   208.1411,  3.6650, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 125, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     10.0573,    -24.2153,   204.2667,  3.7421, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 126, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -33.5040,     66.4362,   210.8128,  3.7337, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 127, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     36.4514,    112.4740,   220.3037,  3.7188, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 128, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     66.4062,    137.3194,   232.6324,  0.5707, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 129, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     37.3420,    135.4444,   219.5294,  3.6542, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 130, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     74.9818,   -123.4654,   208.3310,  3.9886, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 131, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -35.9869,   -138.5304,   209.2168,  4.9711, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 132, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -27.0923,   -157.2818,   209.2000,  5.4028, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 133, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -59.1441,     -2.5972,   203.8611,  1.1522, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 134, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -90.7815,     32.5443,   200.5884,  1.2158, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 135, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    105.6282,    -82.3118,   212.7828,  3.9937, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 136, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,      7.6233,    114.6823,   219.5140,  5.4006, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 137, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     22.4473,    106.9543,   219.5602,  0.1875, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 138, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     66.9670,    160.5417,   238.4071,  3.6371, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 139, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     -3.3494,     82.9949,   219.3883,  2.4963, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 140, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,     73.0853,    124.6438,   231.2749,  3.9875, 180, 0, 0, 100, 0),  -- Eye of Antenorian
(@CGUID + 141, 257834, 2952, 0, 0, '208', 0, 0, 0, -1, 0, 0,    -32.6923,    -10.9908,   203.2946,  1.4009, 180, 0, 0, 100, 0);  -- Eye of Antenorian

INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`isActive`) VALUES
(@OGUID + 0 , 373465, 2952, 0, 0, '208', 0, 0, 0, -1,    -83.5113,     48.8820,   200.7652,  0.0000, 0, 0, 0.0000000, 1.0000000, 180, 255, 1, 0),  -- Forgotten Gravestone
(@OGUID + 1 , 373465, 2952, 0, 0, '208', 0, 0, 0, -1,     75.8655,   -131.8123,   207.8872,  0.0000, 0, 0, 0.0000000, 1.0000000, 180, 255, 1, 0),  -- Forgotten Gravestone
(@OGUID + 2 , 402605, 2952, 0, 0, '208', 0, 0, 0, -1,     17.3524,    258.2292,   274.5866,  3.8676, 0, 0, 0.9348345, -0.3550838, 180, 255, 1, 0),  -- Door
(@OGUID + 3 , 407408, 2952, 0, 0, '208', 0, 0, 0, -1,     47.1024,    -70.0538,   207.5396,  5.8005, 0, 0, 0.2390066, -0.9710179, 180, 255, 1, 0),  -- Collision
(@OGUID + 4 , 408227, 2952, 0, 0, '208', 0, 0, 0, -1,     16.1337,    258.0382,   274.2351,  3.7874, 0, 0, 0.9483180, -0.3173215, 180, 255, 1, 0),  -- Leave Delve
(@OGUID + 5 , 433898, 2952, 0, 0, '208', 0, 0, 0, -1,    -34.0209,    220.6749,   264.3436,  3.7589, 0, 0, 0.9527434, -0.3037762, 180, 255, 1, 0),  -- Campfire (INVISIBLE)
(@OGUID + 6 , 516932, 2952, 0, 0, '208', 0, 0, 0, -1,     42.7170,     30.7448,   217.7092,  0.0000, 0, 0, 0.0000000, 1.0000000, 180, 255, 1, 0),  -- Tranquility Bloom
(@OGUID + 7 , 516932, 2952, 0, 0, '208', 0, 0, 0, -1,    -23.3802,    -19.1823,   203.5887,  0.0000, 0, 0, 0.0000000, 1.0000000, 180, 255, 1, 0),  -- Tranquility Bloom
(@OGUID + 8 , 523284, 2952, 0, 0, '208', 0, 0, 0, -1,    -71.2604,     26.2552,   203.2666,  0.0000, 0, 0, 0.0000000, 1.0000000, 180, 255, 1, 0),  -- Lightfused Refulgent Copper
(@OGUID + 9 , 523295, 2952, 0, 0, '208', 0, 0, 0, -1,    -47.3281,   -140.7274,   209.0964,  0.0000, 0, 0, 0.0000000, 1.0000000, 180, 255, 1, 0),  -- Brilliant Silver
(@OGUID + 10, 570933, 2952, 0, 0, '208', 0, 0, 0, -1,    -17.4635,     78.8333,   216.9727,  0.6042, 0, 0, 0.2975258, 0.9547138, 180, 255, 1, 0),  -- [DNT] Invisible Wall
(@OGUID + 11, 570933, 2952, 0, 0, '208', 0, 0, 0, -1,    -27.4740,      9.5990,   204.1108,  4.4148, 0, 0, 0.8041194, -0.5944678, 180, 255, 1, 0),  -- [DNT] Invisible Wall
(@OGUID + 12, 571067, 2952, 0, 0, '208', 0, 0, 0, -1,    -22.7535,     10.6337,   202.9860,  5.0183, 0, 0, 0.5911167, -0.8065860, 180, 255, 1, 0),  -- [DNT] TB Blocker 01
(@OGUID + 13, 571068, 2952, 0, 0, '208', 0, 0, 0, -1,    -35.0312,     11.2552,   204.9386,  1.4638, 0, 0, 0.6682843, 0.7439060, 180, 255, 1, 0),  -- [DNT] TB Blocker 02
(@OGUID + 14, 571097, 2952, 0, 0, '208', 0, 0, 0, -1,    163.1649,    -86.8229,   217.3596,  0.7568, 0, 0, 0.3694341, 0.9292569, 180, 255, 1, 0),  -- Rock Wall
(@OGUID + 15, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,     19.5260,     27.3281,   217.0552,  4.8867, 0, 0, 0.6428726, -0.7659731, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 16, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,     51.4809,   -129.0781,   208.4297,  4.8985, 0, 0, 0.6383422, -0.7697527, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 17, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,     65.3472,     54.9531,   218.3352,  5.1442, 0, 0, 0.5392048, -0.8421746, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 18, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,     73.9896,    -95.7205,   212.6613,  4.9816, 0, 0, 0.6058172, -0.7956038, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 19, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,    -60.4427,     12.9757,   201.2890,  4.5161, 0, 0, 0.7729912, -0.6344167, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 20, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,   -111.1649,      9.8576,   200.5597,  4.3119, 0, 0, 0.8336272, -0.5523275, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 21, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,     87.8872,    -64.2135,   214.3014,  5.0508, 0, 0, 0.5779322, -0.8160848, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 22, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,    142.6424,      4.1493,   207.8872,  5.3206, 0, 0, 0.4629254, -0.8863973, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 23, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,    -15.7483,     -0.0764,   203.3889,  4.7165, 0, 0, 0.7056518, -0.7085588, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 24, 584752, 2952, 0, 0, '208', 0, 0, 0, -1,    166.6441,   -104.8438,   218.7448,  5.2096, 0, 0, 0.5113824, -0.8593533, 180, 255, 1, 0),  -- Mislaid Curiosity
(@OGUID + 25, 617855, 2952, 0, 0, '208', 0, 0, 0, -1,   -100.6094,     -6.6632,   200.4448,  4.3943, 0, 0, 0.8101703, -0.5861945, 180, 255, 1, 0),  -- |cffA335EESturdy Chest
(@OGUID + 26, 618111, 2952, 0, 0, '208', 0, 0, 0, -1,     65.5556,     -6.9861,   224.5646,  5.7262, 0, 0, 0.2749067, -0.9614709, 180, 255, 1, 0),  -- |cffA335EESturdy Chest
(@OGUID + 27, 618112, 2952, 0, 0, '208', 0, 0, 0, -1,    131.7674,    -15.9757,   219.6326,  2.5757, 0, 0, 0.9602370, 0.2791860, 180, 255, 1, 0),  -- |cffA335EESturdy Chest
(@OGUID + 28, 618844, 2952, 0, 0, '208', 0, 0, 0, -1,    -34.0209,    220.6749,   264.3436,  3.7589, 0, 0, 0.9527434, -0.3037762, 180, 255, 1, 0),  -- Delve Campfire
(@OGUID + 29, 619451, 2952, 0, 0, '208', 0, 0, 0, -1,    -80.0382,     -5.8698,   202.6618,  4.4089, 0, 0, 0.8058696, -0.5920931, 180, 255, 1, 0),  -- Scroll of Shared Agony
(@OGUID + 30, 619452, 2952, 0, 0, '208', 0, 0, 0, -1,    100.0469,   -119.9931,   209.4051,  2.9168, 0, 0, 0.9936902, 0.1121598, 180, 255, 1, 0),  -- Betrayer's Blade
(@OGUID + 31, 619452, 2952, 0, 0, '208', 0, 0, 0, -1,    -19.9427,      0.5243,   203.8321,  4.7503, 0, 0, 0.6935770, -0.7203825, 180, 255, 1, 0),  -- Betrayer's Blade
(@OGUID + 32, 619453, 2952, 0, 0, '208', 0, 0, 0, -1,     46.0191,    129.8576,   220.7542,  1.5039, 0, 0, 0.6830643, 0.7303583, 180, 255, 1, 0),  -- (unnamed)
(@OGUID + 33, 619455, 2952, 0, 0, '208', 0, 0, 0, -1,    -50.5556,   -147.6684,   210.2985,  4.0149, 0, 0, 0.9061719, -0.4229095, 180, 255, 1, 0);  -- Summoning 404
