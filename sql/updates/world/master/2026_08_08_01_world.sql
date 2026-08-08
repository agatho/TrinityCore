--
-- 12.0.7.68974 tester capture dump_12.0.7.68974_2026-08-08_02-54-06 ("linformi-shop-key"):
-- Lindormi city spawn + gossip + vendor, and the day's world-quest rotation additions.
--
-- 1) Lindormi in Silvermoon/Quel'Thalas city area: the CITY NPC is creature 197711
--    ("Lindormi" <Mythic Keystones>, QUERY_CREATURE_RESPONSE idx 23518) - NOT 259053
--    (259053 remains the sniffed in-dungeon Algeth'ar Academy entry from the 68275 M+ run
--    capture, see 2026_08_07_63_world.sql; that data stands). Wire evidence:
--    - UPDATE_OBJECT create block: map 0 (packet MapID field AND her GUID128 map bits),
--      position 8672.9854 -4517.0728 23.9514, orientation 5.6468 (zone 15969, Quel'Thalas).
--    - SMSG_GOSSIP_MESSAGE GossipID 29898, BroadcastTextID 231632, options:
--        125048 ord=2 OptionNpc=None   "I seem to have misplaced my Keystone."
--        140067 ord=4 OptionNpc=Vendor "Show me items I can purchase with a Timelost Saddle."
--      Selecting 125048 pushes keystone item 180653 (SPELL_GO 352816 -> DISPLAY_TOAST ->
--      ITEM_PUSH_RESULT); the re-shown menu (same GossipID) then only offers 140067.
--    - Selecting 140067 opens SMSG_VENDOR_INVENTORY: 16 items, price 0, ExtendedCost 11574,
--      PlayerConditionID 157668, quantity unlimited.
--

-- City spawn (guid 9000200 is the Algeth'ar Academy spawn of 259053; city spawn uses 9000201)
DELETE FROM `creature` WHERE `guid` = 9000201;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `VerifiedBuild`) VALUES
(9000201, 197711, 0, 15969, 16079, '0', 0, 0, 0, 0, 8672.9854, -4517.0728, 23.9514, 5.6468, 300, 0, 0, 0, NULL, NULL, NULL, NULL, 68974);

-- Follow-up to 2026_08_07_63_world.sql assumptions: the city entry is 197711 (not 259053, and not
-- 197915 as that file's comment guessed for the DF-era id). Wire the sniffed menu/flags/script to it.
UPDATE `creature_template` SET `ScriptName` = 'npc_lindormi', `npcflag` = 129, `faction` = 35, `gossip_menu_id` = 29898, `subname` = 'Mythic Keystones', `VerifiedBuild` = 68974 WHERE `entry` = 197711;

-- Gossip menu 29898 options (repo world data lacks them; ids/texts/order sniffed 68974).
-- gossip_menu row intentionally untouched: TextID (npc_text) is not on the wire in this build.
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 29898 AND `OptionID` IN (2, 4);
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(29898, 125048, 2, 0, 'I seem to have misplaced my Keystone.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 68974),
(29898, 140067, 4, 1, 'Show me items I can purchase with a Timelost Saddle.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 68974);

-- Her Timelost Saddle exchange list (SMSG_VENDOR_INVENTORY idx 25851, decoded byte-exact:
-- slot = wire MuID, all price 0 / quantity -1 -> maxcount 0, ExtendedCost 11574,
-- PlayerConditionID 157668; every entry carried ItemModifier 28 = 872 on the instance).
DELETE FROM `npc_vendor` WHERE `entry` = 197711 AND `item` IN (182717, 187525, 199412, 204798, 209060, 213438, 226357, 237141, 247822, 248248, 275440, 275442, 275444, 275445, 275446, 275447);
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `ExtendedCost`, `type`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`) VALUES
(197711, 2, 182717, 0, 11574, 1, 157668, 0, 68974),
(197711, 3, 187525, 0, 11574, 1, 157668, 0, 68974),
(197711, 6, 199412, 0, 11574, 1, 157668, 0, 68974),
(197711, 7, 204798, 0, 11574, 1, 157668, 0, 68974),
(197711, 8, 209060, 0, 11574, 1, 157668, 0, 68974),
(197711, 9, 213438, 0, 11574, 1, 157668, 0, 68974),
(197711, 10, 226357, 0, 11574, 1, 157668, 0, 68974),
(197711, 11, 237141, 0, 11574, 1, 157668, 0, 68974),
(197711, 13, 247822, 0, 11574, 1, 157668, 0, 68974),
(197711, 14, 248248, 0, 11574, 1, 157668, 0, 68974),
(197711, 15, 275440, 0, 11574, 1, 157668, 0, 68974),
(197711, 16, 275442, 0, 11574, 1, 157668, 0, 68974),
(197711, 17, 275444, 0, 11574, 1, 157668, 0, 68974),
(197711, 18, 275445, 0, 11574, 1, 157668, 0, 68974),
(197711, 19, 275446, 0, 11574, 1, 157668, 0, 68974),
(197711, 20, 275447, 0, 11574, 1, 157668, 0, 68974);

--
-- 2) World-quest rotation additions (75 QuestIDs seen active on 2026-08-08 that are not in
--    the 2026_08_08_00 seed; same 24-byte wire entry semantics {u64 LastUpdate, u32 QuestID,
--    u32 Timer=Duration, i32 VariableID, i32 Value}). Rotated-out rows are kept (historical rows
--    remain valid pool entries). Note: quest 49091 was re-seen with VariableID 14062 (seed has
--    14245 from yesterday) - the activation worldstate can differ per rotation; seed row left as is.
--
DELETE FROM `world_quest_template` WHERE `QuestID` IN (47561, 47707, 48192, 48514, 48724, 48833, 49099, 56139, 56173, 64768, 66833, 67005, 67006, 67010, 67012, 67113, 70037, 70064, 70071, 70072, 70100, 70412, 70415, 70427, 70634, 70646, 70651, 70654, 70655, 70712, 72019, 73083, 75121, 75122, 75162, 75834, 78215, 78436, 78437, 80412, 81802, 81811, 81815, 82158, 82300, 84430, 84627, 84850, 85051, 85589, 85834, 85927, 86174, 86305, 86707, 86800, 86872, 87759, 88774, 88818, 88902, 89274, 89288, 89291, 91806, 91981, 92152, 92549, 92848, 93703, 95397, 95402, 95815, 96400, 96618);
INSERT INTO `world_quest_template` (`QuestID`, `Duration`, `VariableID`, `Value`) VALUES
 (47561,86400,13625,1),
 (47707,86400,13667,1),
 (48192,86400,13890,1),
 (48514,43200,14053,1),
 (48724,86400,14174,1),
 (48833,43200,14269,1),
 (49099,21600,14063,1),
 (56139,86400,17639,1),
 (56173,86400,17734,3),
 (64768,302400,22479,1),
 (66833,302400,21984,1),
 (67005,86400,22111,1),
 (67006,302400,22062,1),
 (67010,302400,22063,1),
 (67012,302400,22075,1),
 (67113,302400,22101,1),
 (70037,302400,22218,1),
 (70064,302400,22403,1),
 (70071,302400,22237,1),
 (70072,302400,22307,1),
 (70100,302400,22371,1),
 (70412,302400,22373,1),
 (70415,302400,22375,1),
 (70427,302400,22388,1),
 (70634,302400,22458,1),
 (70646,302400,22463,1),
 (70651,302400,22466,1),
 (70654,302400,22473,1),
 (70655,302400,22469,1),
 (70712,302400,22490,1),
 (72019,86400,22650,1),
 (73083,302400,23019,1),
 (75121,302400,23338,1),
 (75122,302400,23339,1),
 (75162,604800,23350,1),
 (75834,302400,23824,1),
 (78215,604800,24998,1),
 (78436,302400,24885,1),
 (78437,302400,24886,1),
 (80412,86400,25982,1),
 (81802,302400,26105,1),
 (81811,302400,26113,1),
 (81815,302400,26116,1),
 (82158,604800,26225,1),
 (82300,86400,26268,1),
 (84430,604800,26672,2),
 (84627,604800,26672,2),
 (84850,604800,26672,2),
 (85051,604800,26672,2),
 (85589,604800,26672,2),
 (85834,302400,27362,1),
 (85927,302400,27425,1),
 (86174,604800,27471,1),
 (86305,302400,27531,1),
 (86707,302400,28882,1),
 (86800,302400,27756,1),
 (86872,302400,27771,1),
 (87759,43200,29908,1),
 (88774,302400,28154,1),
 (88818,302400,28175,1),
 (88902,302400,28176,1),
 (89274,302400,28296,1),
 (89288,302400,28298,1),
 (89291,302400,28299,1),
 (91806,302400,29316,1),
 (91981,302400,29159,1),
 (92152,302400,29520,1),
 (92549,302400,29779,1),
 (92848,604800,29872,1),
 (93703,604800,30194,4),
 (95397,3600,30872,1),
 (95402,3600,30877,1),
 (95815,14400,31187,1),
 (96400,3600,31394,1),
 (96618,14400,31511,1);
