-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H gossip (vendor + entry + outro)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/: creature_template_gossip.sql,
--   gossip_menu_option.sql (properly-formed, real-schema captures for 39386/39348),
--   addon_gossip_menu.sql, addon_gossip_menu_option.sql, addon_gossip_option_state.txt
--   (REVIEW-ONLY behavioral capture -- see Section 3 note below).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================
-- SCHEMA NOTE (applies to all 3 menus below): the bundle's addon-captured
-- gossip_menu_option rows use a SHORTHAND column set (menuid, optionindex, text, icon,
-- OptionID) that is NOT the literal `gossip_menu_option` table. Cross-referencing the
-- bundle's two PROPERLY-formed rows (39386/39348, sourced from a real capture that
-- already used the live schema) shows the addon's "OptionID" field is actually the real
-- table's `GossipOptionID` column (the DB2 "flavor" id), while the real table's
-- `OptionID` column (PK component, PRIMARY KEY (MenuID,OptionID)) is a per-menu ordinal
-- NOT captured by the addon. For menu 167032 (no properly-formed capture exists, unlike
-- 39386/39348) this file maps the addon's captured `optionindex` (0-7, real observed
-- display order) 1:1 onto the real `OptionID` ordinal column -- the same pattern already
-- shipped for Chromie's OTHER root menu 25426 in 2026_08_09_20_world.sql (OptionID
-- 0,1,2,3 = GossipOptionID 51901,51902,51903,109278 in display order).
-- ============================================================================

-- ============================================================================
-- SECTION 1 -- creature_template_gossip (all 3 in-scope NPCs)
-- ============================================================================
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(245026, 39386, 69382), -- Win'sa, Food Vendor (task-1 npcflag=129)
(244714, 39348, 69382), -- Lady Jaina Proudmoore, Stromgarde Keep hub clone (task-1 npcflag=1)
(167032, 167032, 69382) -- Chromie, entry-launch hub NPC on map 85 (NOT map 2796 -- see Section 3)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 2 -- gossip_menu (TextID = matching npc_text.ID, see 62_npc_text.sql)
-- ============================================================================
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES
(39386, 39386, 69382),
(39348, 39348, 69382),
(167032, 167032, 69382)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 3a -- gossip_menu_option 39386 (Win'sa vendor browse)
-- Captured verbatim (already real-schema): GossipOptionID 133911, OptionNpc=1
-- (GOSSIP_OPTION_NPC_VENDOR), OptionID=0.
-- ============================================================================
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(39386, 133911, 0, 1, 'Let me browse your goods.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionNpc`=VALUES(`OptionNpc`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 3b -- gossip_menu_option 39348 (Jaina "next adventure" picker)
-- Captured verbatim (already real-schema): GossipOptionID 133893, OptionID=16777216
-- (0x1000000 -- captured live-server PK value, kept as-is rather than renumbered).
-- Quest-90911 "Your Next Adventure" hub; the level-routed hand-off (10-69->DF,
-- 70-80->TWW Recap, 80+->TWW per Plan Phase B Step 5) is wired by C++ (Task 7/Phase J),
-- not this data row.
-- ============================================================================
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(39348, 133893, 16777216, 0, 'Show me where I could go next.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionNpc`=VALUES(`OptionNpc`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 3c -- gossip_menu_option 167032 (Chromie entry-launch hub, map 85)
-- Chromie (167032) is a hub NPC on map 85 (Eastern Kingdoms overworld), NOT inside the
-- instance -- this is GOSSIP DATA ONLY. Do NOT author a `creature` spawn row for her in
-- this content slice (map 2796 has no Chromie spawn; confirmed absent from
-- 20_creature_spawns.sql). The "launch the Arathi Catch-Up" action itself (Movie 470 +
-- teleport into map 2796, per addon_movie_capture.txt: movieID=470, map=85,
-- last_npc=167032, context_quests=51443,62568) is wired by C++ on OptionID 51901/51902,
-- deferred to Task 7 -- this file only supplies the static menu text/order.
--
-- NOTE ON THE EXISTING MENU 25426: this content branch already ships a DIFFERENT root
-- gossip menu for creature 167032 (MenuID 25426, `creature_template_gossip` also has a
-- (167032,25426) row from 2026_08_09_20_world.sql / 2026_08_15_00_world_chromie_faq.sql
-- -- the general "Chromie Time" feature's own capture, options 51901/51902/51903/109278
-- + FAQ submenu 31336). `creature_template_gossip` PK is (CreatureID,MenuID), so adding
-- (167032,167032) alongside the existing (167032,25426) is NOT a PK conflict -- TC allows
-- multiple candidate root menus per creature template; which one actually opens for the
-- Arathi Catch-Up launch context is a script/C++ concern (Task 7), not resolved here.
-- MenuID 167032 itself is a SELF-REFERENTIAL id (MenuID==CreatureID), matching the
-- bundle's own addon_gossip_menu.sql placeholder convention for this session's capture --
-- it is NOT asserted to be a real Blizzard-assigned menu id, only a locally-reserved slot
-- for this session's distinct option layout (which differs from 25426: no OptionNpc=40 /
-- GossipNpcOptionID=32282 custom-UI opener was captured for THIS menu).
--
-- Primary 8 rows: addon_gossip_menu_option.sql, verbatim text/order (optionindex 0-7).
-- ============================================================================
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(167032, 51901,  0, 0, '|cFF0000FF(Recommended)|r Select a timeline.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382), -- optionindex 0: launch action (Task 7)
(167032, 51902,  1, 0, 'Select a different timeline.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382),                 -- optionindex 1: launch action (Task 7)
(167032, 51903,  2, 0, 'I''d like to return to the present timeline, Chromie.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382), -- optionindex 2: exit
(167032, 109314, 3, 0, 'What if I don''t want to stay in the timeline I chose?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382),
(167032, 109276, 4, 0, 'I want to talk about something else.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382),
(167032, 109278, 5, 0, 'I have a question about Timewalking Campaigns.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382),
(167032, 109317, 6, 0, 'I want to explore the afterlives.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382),
(167032, 109316, 7, 0, 'I have another question.', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ---- Question-option quest-state ALTERNATES (task-6-brief Req.3: "109278/109315/109313
-- verbatim") ----
-- addon_gossip_option_state.txt (REVIEW-ONLY behavioral capture; explicitly NOT a
-- TrinityCore world table -- "Do NOT author INSERT SQL from this file blindly") shows
-- optionindex 1 and 2 each have a SECOND observed text, gated by quest state, that this
-- session's capture never resolves the trigger condition for:
--   optionindex 1: 51902 "Select a different timeline." (context Q51443,62568 / L10)
--                  vs 109315 "What are Timewalking Campaigns?" (context Q51443 only / L11)
--   optionindex 2: 51903 "I'd like to return..." (context Q51443,62568 / L10)
--                  vs 109313 "Can my friends join me?" (context Q51443 only / L11)
-- Both members of each pair are captured verbatim below as their OWN OptionID slots
-- (8/9) rather than collapsed onto slots 1/2 -- `gossip_menu_option`'s PK is
-- (MenuID,OptionID), so two different GossipOptionID values cannot share one OptionID
-- row. Authoring them as always-visible rows would be WRONG (retail shows only one
-- member of each pair at a time, gated by quest/Chromie-Time state) -- proper gating
-- needs `conditions` rows keyed the same way MenuID 25426's are (CONDITION_SOURCE_TYPE_
-- GOSSIP_MENU_OPTION=15 on SourceGroup=167032, SourceEntry=<OptionID>), which requires
-- knowing which quest/CT-state condition selects each pair member -- NOT captured this
-- session (the state-capture file only proves the pair EXISTS, not its trigger). Left as
-- an explicit GAP for Task 7 / a future capture pass; rows are present-but-unconditioned
-- (currently 10 total on this menu, all statically visible) rather than fabricated
-- guesses at the condition.
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(167032, 109315, 8, 0, 'What are Timewalking Campaigns?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382), -- alt of optionindex 1 (51902) -- GAP: condition-gate unresolved
(167032, 109313, 9, 0, 'Can my friends join me?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69382)         -- alt of optionindex 2 (51903) -- GAP: condition-gate unresolved
ON DUPLICATE KEY UPDATE `GossipOptionID`=VALUES(`GossipOptionID`), `OptionText`=VALUES(`OptionText`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
