--
-- Lindormi 197711 (Mythic+ keystone NPC): complete the parts that 2026_08_08_01_world.sql's abort skipped.
--
-- On integration, 2026_08_08_01_world.sql carries the Lindormi setup, but its
--   UPDATE `creature_template` SET ... `gossip_menu_id` = 29898 ... WHERE `entry` = 197711;
-- references a column this schema does not have (the creature->menu link lives in
-- `creature_template_gossip`). MySQL fails that statement with error 1054, which ABORTS the whole file,
-- so every statement AFTER it never runs on a fresh database build: Lindormi's gossip options AND her
-- Timelost Saddle vendor list were silently dropped. 2026_08_09_50_world.sql re-applied only the
-- creature_template binding (ScriptName/npcflag/faction/subname), not the gossip options or the vendor,
-- so fresh builds still left Lindormi unable to sell anything.
--
-- This migration re-applies exactly the two skipped blocks, idempotently (DELETE+INSERT), so it is safe
-- on the live realm (which already has them from an earlier good apply) and completes fresh builds. The
-- creature_template binding itself is left to 2026_08_09_50_world.sql (runs earlier, already correct).
-- Data is byte-identical to 2026_08_08_01_world.sql's Lindormi block (VerifiedBuild 68974).
--

-- Gossip menu 29898 options (repo world data lacks them; ids/texts/order sniffed 68974).
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
