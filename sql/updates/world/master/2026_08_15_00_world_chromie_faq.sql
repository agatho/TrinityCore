-- Chromie Time: FAQ submenu tree, refusal texts, Orgrimmar Chromie's Hourglass.
--
-- PROVENANCE / SOURCING
-- ---------------------------------------------------------------------------
-- All content rows below are MINED (not copied wholesale) from an independent
-- 12.0.7 fork by evrydayjunglist, which captured them from its own client
-- sniffs at build 12.0.7.68887 (captures it labels "ct-lvl3" / "ct-start" /
-- "CT-A"). We do not hold those captures; the data is therefore SECOND-HAND
-- sniff data - accurate to the best of the source's claim, but NOT
-- independently re-verified against a capture of our own. Source commits:
--   * d30202289de8cb4374c21485cad1cf75ab918513
--     "feat(chromie-time): Dorn hard refuse vs FAQ soft refuse from ct-lvl3"
--     -> sql/updates/world/master/2026_13_32_14_world.sql  (npc_text 40348)
--     -> sql/updates/world/master/2026_13_32_15_world.sql  (FAQ tree, 40349)
--   * d9bc9350620377bf98c8d585bceba9f58edaa357
--     "feat(chromie-time): select BC Ui ID 6, persist CTR/UF, gate gossip, Org hourglas"
--     -> sql/updates/world/master/2026_13_32_11_world.sql  (gameobject 350063)
-- Their fake 2026_13_32_* filenames are NOT reused; this file is in our real
-- date sequence.
--
-- RESOLVES A STALE COMMENT
-- ---------------------------------------------------------------------------
-- sql/updates/world/master/2026_08_09_20_world.sql:75 states:
--     "109278's Timewalking-info submenu (ActionMenuID) is unmined - option is inert"
-- That is no longer true as of this file: 109278 now points at FAQ hub menu
-- 31336. The comment in that file is deliberately left UNEDITED - it has
-- already been applied on live realms, and UpdateFetcher.cpp:276 re-applies
-- any file whose SHA1 changed. Because updates are processed in filename order,
-- a comment-only edit there would re-run its
-- "DELETE FROM gossip_menu_option WHERE MenuID=25426" + re-insert with
-- ActionMenuID=0 AFTER this file had already been recorded as applied,
-- silently reverting the wiring below. This header is the resolution of record.
--
-- MERGE SAFETY
-- ---------------------------------------------------------------------------
-- Menu 25426 is shared with 2026_08_09_20_world.sql (which deletes the whole
-- menu and re-inserts options 51901/51902/51903/109278). This file NEVER
-- deletes the whole menu - it deletes and re-inserts ONLY GossipOptionID
-- 109278 - so it is safe to apply after that file, and re-applying it is
-- idempotent.
--
-- DELIBERATELY NOT IMPORTED FROM THE SOURCE FORK
-- ---------------------------------------------------------------------------
--   * chromie_time_expansion_quest breadcrumb rows - their MoP/SL/BfA quest
--     ids are wiki-sourced and conflict with our quest_template-verified ids
--     (theirs 60965/60964 for MoP, ours 60125/60126). Ours stand.
--   * All of their `conditions` rows - their CONDITION_CHROMIE_TIME is 61,
--     ours is 60, so their condition SQL would silently mis-target our rows.
--     Nothing here needs a condition: option 109278 is unconditional in our
--     menu 25426 layout (our conditions cover OrderIndex 0/1/2 only).
--   * Their creature_template ScriptName='npc_chromie_time' - we bind
--     'npc_chromie_timewalking' (2026_08_08_08_world.sql).
--   * Their duplicate Orgrimmar Chromie 167032 spawn (guid 11800156) and their
--     duplicate 51901/51902/51903 option rows - we already ship both.
--   * All C++ from those commits.

-- ============================================================================
-- npc_text bodies
-- Source: fork commits above, sniff build 12.0.7.68887 (second-hand).
-- UNVERIFIED: we have not confirmed that broadcast_text rows 240872 / 240784 /
-- 240843 / 240841 / 269086 / 206524 exist in the TDB base we deploy against
-- (no broadcast_text data ships in sql/updates). If any is absent the world
-- server will log a non-fatal "non-existing or incompatible BroadcastTextId"
-- error for that npc_text and the gossip body will render empty.
-- ============================================================================
DELETE FROM `npc_text` WHERE `ID` IN (40348,40349,40352,40353,40354,40357);
INSERT INTO `npc_text` (`ID`, `Probability0`, `Probability1`, `Probability2`, `Probability3`, `Probability4`, `Probability5`, `Probability6`, `Probability7`, `BroadcastTextId0`, `BroadcastTextId1`, `BroadcastTextId2`, `BroadcastTextId3`, `BroadcastTextId4`, `BroadcastTextId5`, `BroadcastTextId6`, `BroadcastTextId7`, `VerifiedBuild`) VALUES
-- Refusal texts. DATA ONLY: nothing in our tree reads these yet - our
-- npc_chromie_timewalking script does not branch on them. They are staged here
-- so the text ids are pinned; the script-side gating is separate future work.
(40348, 1, 0, 0, 0, 0, 0, 0, 0, 269086, 0, 0, 0, 0, 0, 0, 0, 68887), -- hard refuse: Isle of Dorn / not eligible to pick a timeline (fork d30202289d, sniff "ct-start" @68887)
(40349, 1, 0, 0, 0, 0, 0, 0, 0, 206524, 0, 0, 0, 0, 0, 0, 0, 68887), -- soft refuse: "come back with a little more experience" (fork d30202289d, sniff "ct-lvl3" @68887)
-- FAQ bodies
(40352, 1, 0, 0, 0, 0, 0, 0, 0, 240872, 0, 0, 0, 0, 0, 0, 0, 68887), -- FAQ hub body (menu 31336)
(40353, 1, 0, 0, 0, 0, 0, 0, 0, 240784, 0, 0, 0, 0, 0, 0, 0, 68887), -- answer: what are Timewalking Campaigns (menu 31370)
(40354, 1, 0, 0, 0, 0, 0, 0, 0, 240843, 0, 0, 0, 0, 0, 0, 0, 68887), -- answer: can my friends join me (menu 31368)
(40357, 1, 0, 0, 0, 0, 0, 0, 0, 240841, 0, 0, 0, 0, 0, 0, 0, 68887); -- answer: leaving the chosen timeline (menu 31369)

-- ============================================================================
-- FAQ menus. Retail MenuIDs from fork d30202289d (sniff "ct-lvl3" @68887).
-- None of 31336/31368/31369/31370 was previously present in our tree.
-- ============================================================================
DELETE FROM `gossip_menu` WHERE `MenuID` IN (31336,31368,31369,31370);
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES
(31336, 40352, 68887), -- Timewalking Campaigns FAQ hub
(31370, 40353, 68887), -- "What are Timewalking Campaigns?"
(31368, 40354, 68887), -- "Can my friends join me?"
(31369, 40357, 68887); -- "What if I don't want to stay in the timeline I chose?"

-- ============================================================================
-- Wire option 109278 on Chromie's root menu 25426 to the FAQ hub.
-- Scoped delete: ONLY GossipOptionID 109278 (see MERGE SAFETY above).
--
-- The row is otherwise re-shipped exactly as 2026_08_09_20_world.sql has it -
-- OptionID/OrderIndex stays 3 (our capture A layout @68275), NOT the 5 the
-- fork reports from its 68887 sniff. With only four options on this menu the
-- gap is cosmetic, and holding OrderIndex 3 keeps our existing
-- CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION rows (which key on OrderIndex
-- 0/1/2) unambiguous. Because the shipped row is therefore NOT byte-identical
-- to the 68887 sniff, VerifiedBuild stays at our own 68275; only ActionMenuID
-- is second-hand 68887 data.
-- ============================================================================
DELETE FROM `gossip_menu_option` WHERE `MenuID`=25426 AND `GossipOptionID`=109278;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
(25426,109278,3,0,'I have a question about Timewalking Campaigns.',0,0,0,31336,0,NULL,0,0,NULL,0,NULL,NULL,68275); -- ActionMenuID 31336 from fork d30202289d @68887

-- ============================================================================
-- FAQ hub + answer-leaf options. All from fork d30202289d (sniff "ct-lvl3"
-- @68887); OrderIndexes are the fork's as-sniffed values.
-- ============================================================================
DELETE FROM `gossip_menu_option` WHERE `MenuID` IN (31336,31368,31369,31370);
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
-- 31336: FAQ hub
(31336,109315,1,0,'What are Timewalking Campaigns?',0,0,0,31370,0,NULL,0,0,NULL,0,NULL,NULL,68887),
(31336,109313,2,0,'Can my friends join me?',0,0,0,31368,0,NULL,0,0,NULL,0,NULL,NULL,68887),
(31336,109314,3,0,'What if I don''t want to stay in the timeline I chose?',0,0,0,31369,0,NULL,0,0,NULL,0,NULL,NULL,68887),
(31336,109276,4,0,'I want to talk about something else.',0,0,0,25426,0,NULL,0,0,NULL,0,NULL,NULL,68887), -- back to Chromie root menu
-- answer leaves, each looping back to the hub
(31370,109328,1,0,'I have another question.',0,0,0,31336,0,NULL,0,0,NULL,0,NULL,NULL,68887),
(31368,109327,0,0,'I have another question.',0,0,0,31336,0,NULL,0,0,NULL,0,NULL,NULL,68887),
(31369,109326,0,0,'I have another question.',0,0,0,31336,0,NULL,0,0,NULL,0,NULL,NULL,68887);

-- ============================================================================
-- Orgrimmar Chromie's Hourglass (gameobject 350063).
-- Checked first: gameobject_template 350063 already exists in our tree
-- (massparse VerifiedBuild rows + locales in 2026_03_17_00_world.sql), but no
-- `gameobject` SPAWN row for it exists anywhere in sql/updates or sql/custom.
-- Position/rotation are the fork's as-sniffed values (d9bc935062, "CT-A"
-- @68887); guid is renumbered into our own custom range (8003442, adjacent to
-- our Chromie creature guid 8003441) instead of their 11800157.
--
-- KNOWN MISMATCH - NOT RESOLVED HERE: retail places this prop at the foot of
-- Chromie. The fork's Orgrimmar Chromie sits at (1557.18, -4216.54, 56.07)
-- areaId 5170, so their hourglass is directly under her. OUR Orgrimmar Chromie
-- (guid 8003441, 2026_08_09_20_world.sql) is at (1606.17, -4389.46, 19.47)
-- areaId 8618 - roughly 180 yd away and 36 yd lower. The retail-accurate
-- hourglass position is kept here rather than being moved to match our spawn;
-- reconciling which of the two Chromie positions is correct is deliberately
-- left as follow-up work, since the fork's creature rows are excluded from
-- this import. Until then the hourglass will not appear beside our Chromie.
-- ============================================================================
DELETE FROM `gameobject` WHERE `guid`=8003442;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(8003442, 350063, 1, 1637, 5170, '0', 0, 0, 0, -1, 1557.0938, -4216.905, 54.11084, 0.08049467, 0, 0, 0.040236473, 0.99919015, 120, 255, 1, '', NULL, 68887);
