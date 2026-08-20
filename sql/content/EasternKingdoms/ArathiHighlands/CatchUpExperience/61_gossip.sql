-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H gossip (vendor + outro)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/: creature_template_gossip.sql,
--   gossip_menu_option.sql (properly-formed, real-schema captures for 39386/39348),
--   addon_gossip_menu.sql, addon_gossip_menu_option.sql, addon_gossip_option_state.txt
--   (REVIEW-ONLY behavioral capture -- see the Chromie reference block at the end of this
--   file for why it is NOT authored as live data).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================
-- FIX ROUND 1 (task-6 review): this file originally also authored a competing root
-- gossip menu for Chromie (CreatureID 167032, MenuID 167032). Engine-verified regression:
-- `Player::GetGossipMenuForSource` iterates `creature_template_gossip` as a flat vector
-- and the LAST menu whose gossip_menu conditions pass wins; an unconditioned
-- (167032,167032) row sorts after the shipped chromie-time menu (167032,25426) and would
-- SILENTLY OVERRIDE Chromie's real menu server-wide for every Chromie interaction on the
-- realm -- a live regression of already-shipped content. REMOVED. See the reference block
-- at the end of this file for the captured data and the correct place for it (an option
-- under the EXISTING menu 25426, chromie-time feature's domain, not authored here).
-- ============================================================================
-- SCHEMA NOTE (applies to the 2 menus below): the bundle's addon-captured
-- gossip_menu_option rows use a SHORTHAND column set (menuid, optionindex, text, icon,
-- OptionID) that is NOT the literal `gossip_menu_option` table. Cross-referencing the
-- bundle's two PROPERLY-formed rows (39386/39348, sourced from a real capture that
-- already used the live schema) shows the addon's "OptionID" field is actually the real
-- table's `GossipOptionID` column (the DB2 "flavor" id), while the real table's
-- `OptionID` column (PK component, PRIMARY KEY (MenuID,OptionID)) is a per-menu ordinal
-- NOT captured by the addon.
-- ============================================================================

-- ============================================================================
-- SECTION 1 -- creature_template_gossip (in-scope NPCs owned by THIS feature only)
-- ============================================================================
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(245026, 39386, 69382), -- Win'sa, Food Vendor (task-1 npcflag=129)
(244714, 39348, 69382)  -- Lady Jaina Proudmoore, Stromgarde Keep hub clone (task-1 npcflag=1)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- SECTION 2 -- gossip_menu (TextID = matching npc_text.ID, see 62_npc_text.sql)
-- ============================================================================
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES
(39386, 39386, 69382),
(39348, 39348, 69382)
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
-- REFERENCE ONLY -- Chromie (167032) captured timeline-picker options (map 85)
-- NOT LIVE DATA. No creature_template_gossip / gossip_menu / gossip_menu_option row is
-- authored for these anywhere in this file (FIX ROUND 1 -- see header note above).
--
-- Why this NPC has no data here at all:
-- Chromie (167032) is a hub NPC on map 85 (Eastern Kingdoms overworld), NOT inside the
-- Arathi Catch-Up instance (map 2796) -- she was never in this feature's ownership.
-- This content branch ALREADY ships her real root gossip menu, MenuID 25426
-- (2026_08_09_20_world.sql / 2026_08_15_00_world_chromie_faq.sql -- the "Chromie Time"
-- feature's own capture: options 51901/51902/51903/109278 + FAQ submenu 31336).
-- `creature_template_gossip` PK is (CreatureID,MenuID); TrinityCore's
-- `Player::GetGossipMenuForSource` walks that table as a FLAT VECTOR and the LAST menu
-- whose gossip_menu conditions pass wins -- so adding a second, unconditioned
-- (167032,167032) row (as this file originally did) sorts after the shipped (167032,25426)
-- row and SILENTLY OVERRIDES Chromie's real menu server-wide, for every Chromie
-- interaction on the realm. That is a live regression, not a scoped addition -- removed.
--
-- What was actually captured this session (addon_gossip_menu_option.sql + the REVIEW-ONLY
-- addon_gossip_option_state.txt), preserved verbatim below for provenance / Phase-K reuse:
--   optionindex 0: OptionID 51901 '|cFF0000FF(Recommended)|r Select a timeline.'  -- launch action
--   optionindex 1: OptionID 51902 'Select a different timeline.'                 -- launch action
--                  ALT (quest-state-gated, Q51443-only context): 109315 'What are Timewalking Campaigns?'
--   optionindex 2: OptionID 51903 'I''d like to return to the present timeline, Chromie.' -- exit
--                  ALT (quest-state-gated, Q51443-only context): 109313 'Can my friends join me?'
--   optionindex 3: OptionID 109314 'What if I don''t want to stay in the timeline I chose?'
--   optionindex 4: OptionID 109276 'I want to talk about something else.'
--   optionindex 5: OptionID 109278 'I have a question about Timewalking Campaigns.'
--   optionindex 6: OptionID 109317 'I want to explore the afterlives.'
--   optionindex 7: OptionID 109316 'I have another question.'
-- (The 109315/109313 alternates are NOT simultaneous options -- addon_gossip_option_state.txt
-- shows they replace 51902/51903 under a different quest-state context; the session never
-- resolved the triggering condition. Not authorable as static always-visible rows without
-- fabricating that condition.)
--
-- Canonical retail entry point for the Arathi Catch-Up launch is the Adventure Guide
-- (AdventureJournal DB2 row, Task 7 / player_catchup_enter.cpp) -- NOT this Chromie
-- gossip path. If the captured Chromie-Time timeline-picker flow above is ever wanted as
-- an ADDITIONAL entry into the Catch-Up Experience, it must be added as an OPTION under
-- the EXISTING menu 25426 (ActionMenuID wiring on one of its options, or a new
-- GossipOptionID within that menu) -- that is the chromie-time feature's domain, not this
-- one's. Flagged as a cross-feature Phase-K coordination item; not actioned here.
-- ============================================================================
