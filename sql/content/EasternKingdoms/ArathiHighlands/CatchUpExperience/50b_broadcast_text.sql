-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (broadcast_text)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve
--   capture): conversation_groups.txt (conversationId=0x021826bb group text) cross-
--   referenced against broadcast_text.sql (bundle body empty -- new=0 changed=0 same=16,
--   i.e. all captured broadcastTextIds already resolve unchanged against the reference;
--   no diff to ship. This file re-authors the 7 in-scope rows explicitly and idempotently
--   for this content slice's self-containment, per task-5-brief Req.2).
--
-- *** TARGET DATABASE: HOTFIXES, NOT WORLD ***
-- broadcast_text in this core version is served from the HOTFIXES database
-- (I:/TrinityCore/mythic-plus/TrinityCore/sql/base/dev/hotfixes_database.sql:1372),
-- keyed PRIMARY KEY (`ID`,`VerifiedBuild`). This file is a hotfixes-targeted candidate
-- slice; it is kept in this feature directory alongside the world-DB slices for
-- discoverability, but MUST be applied to the hotfixes DB, not world. VerifiedBuild is
-- authored as 69382 (the capture build -- normalized, see note below; was previously
-- 68887, the project's target client build, per mythic-plus-project-layout memory note).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to
-- a live DB/realm. Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
--
-- BUILD NORMALIZE (final-review fix): VerifiedBuild below is authored as 69382 (the
-- capture build), not 68887 (project target client build). 62_npc_text.sql's own
-- broadcast_text rows (290606/290473) and the TCHarvest capture session are both 69382;
-- this file previously used 68887, which was inconsistent within the same feature's
-- broadcast_text rows. Normalized here for consistency -- see
-- .superpowers/sdd/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN/final-fix-brief.md Req.2.
--
-- SCOPE: only the 7 broadcastTextIds actually reached by the 7 in-scope conversations'
-- FirstLineId/chain (see 50_conversation.sql Section 1 comment table) are authored here.
-- The conversation_groups.txt 0x021826bb group captured 16 raw broadcastTextIds total,
-- but 9 of them are out of scope for this task and are NOT authored:
--   290606, 290569, 290576, 290473 -- belong to OTHER lines in the same client-side
--     conversation stream (82133/82141/unresolved) not part of our 7 ConvIds' chains.
--   225160, 232496, 226324, 215556, 227404 -- Wrathion/Dragonflight "Obsidian Warders"
--     intro bleed (Stormwind, Aspects' invitation) -- EXCLUDED, matches the
--     conversation_actors.sql Conversation 17844/creature 184650 exclusion in
--     50_conversation.sql; do not author.
-- Line 82136 (conversation 29727) has BroadcastTextId=0 in ConversationLine.csv --
-- no text was ever assigned to that line client-side, so there is no 8th broadcast_text
-- row to author for it (not a gap -- this is a captured, confirmed zero).
-- ============================================================================

INSERT INTO `broadcast_text` (`ID`, `VerifiedBuild`, `Text`, `Text1`, `LanguageID`, `ConditionID`, `EmotesID`, `Flags`, `ChatBubbleDurationMs`, `VoiceOverPriorityID`, `SoundKitID1`, `SoundKitID2`, `EmoteID1`, `EmoteID2`, `EmoteID3`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`)
VALUES
  -- conv 29725 / line 82130 -- bind-quest 90882 [inferred]
  (290566, 69382, 'Good to see you, $n. We must take back Hammerfall.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29726 / line 82131 -- bind-quest 90882 [inferred]
  (290567, 69382, 'The highlands have seen more than enough conflict. We have to end this before it can escalate.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29728 / line 82137 -- bind-quest 90886 [inferred]
  (290571, 69382, 'Raiding farms... the ogres and kobolds must be gathering supplies for a larger strike.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29730 / line 82140 -- bind-quest 90888 [inferred]
  (290573, 69382, 'The plans you recovered suggest that Stromgarde is their next target. We must make haste!', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29735 / line 82148 (first of farewell pair) -- bind-quest 90897 [inferred]
  (290583, 69382, 'I will return to Hammerfall to help with the repairs.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29735 / line 82149 (second of farewell pair) -- bind-quest 90897 [inferred]
  (290584, 69382, 'And I''ll do the same for Stromgarde. We could all use a moment of respite.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 30602 / line 84222 -- bind-quest 90896 [inferred] -- Ro'grok (244709) mid-fight escalation line, see 50_conversation.sql guidance note
  (295548, 69382, 'Not over yet... me can get out! Start new attack!', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Text1`=VALUES(`Text1`);
