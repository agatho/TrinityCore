--
-- 12.0.7.68974 tester capture dump_12.0.7.68974_2026-08-08_02-54-06 ("linformi-shop-key"):
-- Lindormi city spawn + gossip + vendor (the day's world-quest rotation additions live on
-- feature/world-quests under this same filename).
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
