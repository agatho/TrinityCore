-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H npc_text (gossip bodies)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_npc_text.sql (8 raw
--   rows). Only the 3 rows belonging to this task's in-scope NPCs (245026 Win'sa, 244714
--   Jaina, 167032 Chromie) are authored here per task-6-brief Req.5; the other 5
--   (npc:3370 guild, npc:5188 Chromie-hub tabard vendor, npc:189600/189603 dracthyr
--   intro, npc:241677 Sunwell) are OUT OF SCOPE for this task (not among 61_gossip.sql's
--   3 NPCs) and are not authored.
--
-- SCHEMA NOTE: `npc_text` has no direct text column -- gossip body text is indirected
-- through `BroadcastTextID0..7` (hotfixes-DB lookup). The bundle's addon_npc_text.sql
-- gives only raw plain-text strings keyed by a placeholder 'npc:<id>' id (not a literal,
-- insertable npc_text row). Real broadcastTextIds for 2 of the 3 lines were cross-found
-- verbatim in conversation_groups.txt (same session capture, same exact text strings);
-- the 3rd (Chromie) has no broadcastTextId anywhere in the bundle -- left as an explicit
-- GAP rather than fabricated (see Section 2).
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

-- ---- Chromie (167032) -- GAP: no broadcastTextId captured ----
-- addon_npc_text.sql captured the raw string verbatim:
--   "Hey there, Agathorz! Wherever you want to go, I can help you get there!
--
--   Time works in mysterious ways, but you don't look like a stranger to mystery."
-- ...but unlike Win'sa/Jaina, this exact string does not appear anywhere in
-- conversation_groups.txt (nor any other bundle file with a broadcastTextId attached), so
-- there is no real id to bind. Authored present-but-inert (BroadcastTextID0=0, matching
-- the column DEFAULT) rather than inventing one -- gossip body will render empty until a
-- future capture/DB2 search resolves the real broadcastTextId (Phase K).
INSERT INTO `npc_text` (`ID`, `Probability0`, `BroadcastTextID0`, `VerifiedBuild`) VALUES
(167032, 1, 0, 69382)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);
