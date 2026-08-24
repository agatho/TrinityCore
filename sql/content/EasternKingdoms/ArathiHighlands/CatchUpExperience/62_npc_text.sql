-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H npc_text (gossip bodies)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_npc_text.sql (8 raw
--   rows). Only the 2 rows belonging to NPCs THIS FEATURE OWNS (245026 Win'sa, 244714
--   Jaina) are authored here per task-6-brief Req.5; the other 6 (npc:3370 guild, npc:5188
--   Chromie-hub tabard vendor, npc:167032 Chromie, npc:189600/189603 dracthyr intro,
--   npc:241677 Sunwell) are OUT OF SCOPE and are not authored. Chromie (167032) in
--   particular is a map-85 hub NPC never owned by this feature -- see FIX ROUND 1 note
--   in 61_gossip.sql: her real gossip menu (25426) already ships elsewhere on this
--   branch, and this file must not touch her data (removed here, was previously
--   authored present-but-inert; see 61_gossip.sql's reference block for the captured
--   text, preserved there for provenance instead).
--
-- SCHEMA NOTE: `npc_text` has no direct text column -- gossip body text is indirected
-- through `BroadcastTextID0..7` (hotfixes-DB lookup). The bundle's addon_npc_text.sql
-- gives only raw plain-text strings keyed by a placeholder 'npc:<id>' id (not a literal,
-- insertable npc_text row). Real broadcastTextIds for both in-scope lines were cross-found
-- verbatim in conversation_groups.txt (same session capture, same exact text strings).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================

-- ============================================================================
-- SECTION 1 -- broadcast_text
-- *** TARGET DATABASE: HOTFIXES, NOT WORLD ***
-- Same split as 50b_broadcast_text.sql (Phase G): broadcast_text in this core version is
-- served from the HOTFIXES database (sql/base/dev/hotfixes_database.sql), PK
-- (`ID`,`VerifiedBuild`). Kept in this world-DB-targeted directory for discoverability;
-- MUST be applied to the hotfixes DB, not world.
-- Provenance: conversation_groups.txt, conversationId=0x021826bb group (same capture
-- session as 50_conversation.sql / 50b_broadcast_text.sql) -- these 2 lines are in that
-- group but were NOT among the 7 in-scope ConversationLine chains 50b authored, so they
-- are authored here instead, for their actual use (npc_text gossip greetings).
-- ============================================================================
INSERT INTO `broadcast_text` (`ID`, `VerifiedBuild`, `Text`, `Text1`, `LanguageID`, `ConditionID`, `EmotesID`, `Flags`, `ChatBubbleDurationMs`, `VoiceOverPriorityID`, `SoundKitID1`, `SoundKitID2`, `EmoteID1`, `EmoteID2`, `EmoteID3`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`)
VALUES
  -- Win'sa (245026) gossip greeting -- conversation_groups.txt broadcastTextId=290606
  (290606, 69382, 'I got what ya need here.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- Lady Jaina Proudmoore (244714) gossip greeting -- conversation_groups.txt broadcastTextId=290473
  (290473, 69382, 'I know of a few places that could use your help.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Text1`=VALUES(`Text1`);

-- ============================================================================
-- SECTION 2 -- npc_text
-- ============================================================================
INSERT INTO `npc_text` (`ID`, `Probability0`, `BroadcastTextID0`, `VerifiedBuild`) VALUES
(39386, 1, 290606, 69382), -- Win'sa: "I got what ya need here."
(39348, 1, 290473, 69382)  -- Jaina: "I know of a few places that could use your help."
ON DUPLICATE KEY UPDATE `BroadcastTextID0`=VALUES(`BroadcastTextID0`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
