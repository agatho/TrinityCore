-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (conversations)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve capture)
-- Sources used: conversation_template.sql (7 rows), conversation_actors.sql (10 rows,
--   1 EXCLUDED, see below), conversation_line_template.sql (bundle body empty --
--   new=0 changed=0 same=11, i.e. no captured divergence from reference; rows below
--   authored explicitly anyway for documentation/completeness per task-5-brief Req.1),
--   conversation_groups.txt (conversationId=0x021826bb group, 16 raw lines, cross-
--   referenced against the DB2-backed NextConversationLineID chains reproduced in that
--   file), C:/dumps/tcharvest/out/db2_csv/ConversationLine.csv (per-line
--   BroadcastTextID/NextConversationLineID -- this CSV has NO ActorIdx column, so
--   per-line actor resolution below is TODO Phase K where more than one actor exists).
-- Plan reference: C:/dumps/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN.md sec 1.5.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================
--
-- SCOPE: only the 7 in-scope ConversationIds from plan sec 1.5 are authored below.
-- conversation_actors.sql's 10th row -- (17844, 64220, Idx 1, CreatureId 184650,
-- CreatureDisplayInfoId 107146) -- is Wrathion/Dragonflight bleed (Conversation 17844,
-- creature entry 184650, the Obsidian Warders intro at Stormwind) and is INTENTIONALLY
-- EXCLUDED, per task-5-brief explicit instruction. Conversation 17844 / creature entry
-- 184650 do NOT appear anywhere in this file.
--
-- The 7 conversations (ConvId -> FirstLineId -> bind-quest [inferred], plan sec 1.5):
--   29725 -> 82130 (bcast 290566 "Good to see you, $n. We must take back Hammerfall.") -> 90882 accept
--   29726 -> 82131 (bcast 290567 "The highlands have seen more than enough conflict...") -> 90882
--   29727 -> 82136 (player + actor 107910; BroadcastTextId=0 for this line -- no on-screen
--                   text captured, likely an emote/camera-only conversation beat) -> 90883
--   29728 -> 82137 (bcast 290571 "Raiding farms... gathering supplies for a larger strike.") -> 90886
--   29730 -> 82140 (bcast 290573 "The plans you recovered suggest that Stromgarde is their next target...") -> 90888
--   29735 -> 82148/82149 (bcast 290583/290584, two-line farewell exchange: "I will return to
--                   Hammerfall..."/"And I'll do the same for Stromgarde...") -> 90897
--   30602 -> 84222 (bcast 295548 "Not over yet... me can get out! Start new attack!" -- Ro'grok) -> 90896
--
-- REQUIREMENT 4 (GAP): conversation -> quest binding is a capture GAP -- 0
-- SMART_ACTION_CREATE_CONVERSATION triggers were found in the capture (see
-- quest_conversation_triggers.txt / quest_conversation_smart_scripts_candidates.sql --
-- both empty of confirmed hits for these 7 ConvIds). Do NOT author smart_scripts here;
-- Task 4 (40_smart_scripts.sql) owns all SmartAI for this content slice and did not
-- author conversation-trigger scripts either (out of its scope). As AUTHORING GUIDANCE
-- for a Phase-K follow-up, each conversation is intended to fire via
-- SMART_ACTION_CREATE_CONVERSATION (action_type=98) keyed off the bind-quest above:
--   29725/29726 -> quest 90882 accept (SMART_EVENT_ACCEPTED_QUEST or gossip-on-accept,
--                  on the Jaina/Thrall quest giver -- two sequential conv beats)
--   29727       -> quest 90883 objective/accept beat (actor 107910 interact)
--   29728       -> quest 90886 objective progress beat (farm-raid investigation)
--   29730       -> quest 90888 accept/objective beat (Stromgarde intel reveal)
--   29735       -> quest 90897 SMART_EVENT_QUEST_REWARDED (Jaina/Thrall farewell,
--                  matches the "Hammerfall garrison turns hostile" story beat flagged
--                  in 10_creature_template.sql for this same quest)
--   30602       -> quest 90896 SMART_EVENT_DEATH or HEALTH_PCT beat on Ro'grok (244709)
--                  -- "Not over yet... me can get out! Start new attack!" reads as a
--                  mid-fight escalation line, not a death line (compare creature_text
--                  244709 groupid 1 "Arathi... never... be ours..." which IS the death
--                  bark, see 51_creature_text.sql) -- likely a HEALTH_PCT-triggered
--                  conversation, not on-death; human must confirm against retail before
--                  wiring the SmartAI event.
-- None of the above SMART_ACTION_CREATE_CONVERSATION rows are authored in this file or
-- in 40_smart_scripts.sql -- this is guidance only, left for Phase K.
-- ============================================================================

-- ---- SECTION 1: conversation_template (7 rows) ----
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29725, 82130) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29726, 82131) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29727, 82136) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29728, 82137) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29730, 82140) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29735, 82148) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (30602, 84222) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);

-- ---- SECTION 2: conversation_actors (9 rows -- 10th bundle row is Wrathion/Dragonflight
--      bleed, ConversationId 17844, EXCLUDED per scope above). ActorId values (107908,
--      107909, 107910, 107913, 107917, 107937, 107938, 109705) are ConversationActor.db2
--      references, not creature_template entries -- CreatureId=0 on every row below means
--      the actor's model/name is resolved entirely from that DB2, so there is no FK
--      relationship to the Task 1 creature_template roster to verify here. ----
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29725, 107908, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29726, 107909, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
  -- 29727 Idx0 = the player themself (ActivePlayerObject=1), Idx1 = creature actor 107910
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29727, 0, 0, 0, 0, 0, 1) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29727, 107910, 1, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29728, 107913, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29730, 107917, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
  -- 29735 two-actor farewell exchange: Idx0 = actor 107938, Idx1 = actor 107937 (see
  -- conversation_line_template ActorIdx TODO below -- which actor speaks which line
  -- is unresolved from this capture)
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29735, 107938, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29735, 107937, 1, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (30602, 109705, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);

-- ---- SECTION 3: conversation_line_template (8 rows -- the 8 distinct ConversationLine
--      IDs reached by our 7 conversations' chains: 82130, 82131, 82136, 82137, 82140,
--      82148, 82149, 84222). Schema per ConversationDataStore.cpp::LoadConversationTemplates:
--      (Id, UiCameraID, ActorIdx, Flags, ChatType); Id must exist in ConversationLine.db2
--      (all 8 do -- confirmed via ConversationLine.csv). UiCameraID/Flags/ChatType are not
--      captured by this bundle -- authored as 0 (engine default), not fabricated non-zero
--      values. ActorIdx: ConversationLine.csv carries NO ActorIdx column, so it cannot be
--      resolved from this source for any line; single-actor conversations (29725, 29726,
--      29728, 29730, 30602 -- one ConversationActor at Idx 0) are safely authored as
--      ActorIdx=0 (the only actor). The two multi-actor conversations (29727: player+
--      actor 107910; 29735: actor 107938+actor 107937) get an explicit TODO Phase K tag
--      below since ActorIdx=0 may be wrong for lines actually spoken by the Idx-1 actor. ----
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82130, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82131, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
  -- TODO Phase K: actor Idx from ConversationLine.db2 -- conv 29727 has 2 actors (player Idx0, creature 107910 Idx1); BroadcastTextId=0 for this line (no on-screen text captured) so ActorIdx=0 default is low-risk, but unconfirmed
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82136, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82137, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82140, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
  -- TODO Phase K: actor Idx from ConversationLine.db2 -- conv 29735 has 2 actors (107938 Idx0, 107937 Idx1) exchanging farewell lines 82148/82149; ConversationLine.csv has no ActorIdx column to disambiguate which actor speaks which line, authored as ActorIdx=0 pending confirmation
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82148, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
  -- TODO Phase K: actor Idx from ConversationLine.db2 -- see 82148 note; this is the second (likely Idx1) speaker's line
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82149, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (84222, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
