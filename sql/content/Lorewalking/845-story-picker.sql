-- ============================================================================
-- CANDIDATE: the LOREWALKING story picker -- PlayerChoice 845.
-- ============================================================================
-- WHAT THIS IS. Lorewalking (retail patch 11.1.7, expanded 12.0.7) is a
-- story-replay mode: Lorewalker Cho / Li Li offer a picker, and choosing a
-- campaign hides your other quests, scales content, and walks you through the
-- ORIGINAL quest chains across expansions. The picker is not a bespoke wire
-- system -- it is an ordinary PlayerChoice, which is why this is authorable as
-- data.
--
-- WHY THE REALM NEEDS IT. integ_world already has PlayerChoice 845 ("Which
-- story would you like to hear?") -- with exactly ONE response, 5046
-- Elves/Begin. Five captures show the picker carrying five campaigns. Four of
-- them are simply absent from the realm, so four of the five stories cannot be
-- selected at all.
--
-- EVIDENCE. Five captures (Sniff-Midnight/*lorewalking*, build 12.1.0.69497,
-- 2026-08-29), decoded independently by WowPacketParser. Responses 4321, 4322,
-- 4379 and 5419 appear in ALL FIVE -- the strongest corroboration this project
-- has on any playerchoice row. The raw .pkt carries the picker as plain text
-- ("Lorewalking Campaigns", "Start Over", "11.1.7 Lorewalking - Restart (LAS)").
--
-- INDEX IS NOT STABLE, AND THAT IS THE POINT. Response 5419 (Loa) sits at
-- Index 4 in four captures and Index 5 in the fifth -- pushed down because that
-- capturer had the Elves campaign IN PROGRESS, which adds a second Elves row.
-- Reconcile these BY ResponseId, never by Index. Same trap already documented
-- for the Arathi finale (PlayerChoice 902).
--
-- The Index values below are therefore the canonical FRESH-PLAYER layout:
--   0 Xal'atath   1 Ethereals   2 The Lich King   3 Elves(existing)   4 Loa
-- which is exactly what the four captures without an in-progress campaign show.
--
-- REVIEW BEFORE APPLYING. Idempotent: DELETEs only the four ResponseIds it
-- inserts, so the realm's existing 5046 is untouched.
-- ============================================================================

DELETE FROM `playerchoice_response` WHERE `ChoiceId`=845 AND `ResponseId` IN (4321, 4322, 4379, 5419);

INSERT INTO `playerchoice_response`
  (`ChoiceId`, `ResponseId`, `Index`, `ChoiceArtFileId`, `Flags`, `WidgetSetID`,
   `UiTextureAtlasElementID`, `SoundKitID`, `GroupID`, `Header`, `SubHeader`,
   `ButtonTooltip`, `Answer`, `Description`, `Confirmation`, `RewardQuestID`,
   `UiTextureKitID`) VALUES
  (845, 4321, 0, 6403389, 0, 1571, 0, 0, 0, 'Xal\'atath', '', 'Xal\'atath awaits...', 'Begin', 'Xal\'atath, the Harbinger, was once known as the Blade of the Black Empire. Her history is a well-kept secret. But her words and deeds have affected so many that she is impossible to ignore. Her story must be told.', '', 0, 0),
  (845, 4322, 1, 6403387, 0, 1572, 0, 0, 0, 'Ethereals', '', 'Who are the ethereals?', 'Begin', 'Mysterious and elusive, the ethereals that have traveled to our world speak little of their history. Yet there is much to be learned from the few things they do choose to share...', '', 0, 0),
  (845, 4379, 2, 6403388, 0, 1573, 0, 0, 0, 'The Lich King', '', '', 'Begin', 'Arthas Menethil. It is said that when he was born, the very forests of Lordaeron whispered his name. But did the forests of his youth know what he would eventually grow to become?', '', 0, 0),
  (845, 5419, 4, 7525957, 0, 1860, 0, 0, 0, 'Loa', '', '', 'Begin', 'The trolls of Azeroth have long found power in their relationship with the loa. But what are the loa? How have they influenced history?', '', 0, 0);

-- ---------------------------------------------------------------------------
-- NOT EMITTED: the two Elves PROGRESS VARIANTS, and why.
-- ---------------------------------------------------------------------------
-- 5047 Elves "Continue" (SubHeader "In Progress") and 5048 Elves "Start Over"
-- were seen in ONE capture only -- the one whose character had already begun
-- that campaign. Together with the realm's existing 5046 Elves "Begin", they
-- are THREE responses for ONE campaign, and the server picks between them from
-- the player's own progress.
--
-- Applying them ungated would list Elves three times in the same picker, which
-- retail never does. They are real, wire-observed content and they are the
-- clearest illustration in this project of the per-player gating hazard: the
-- response list a capture shows is a FLOOR on the content, composed for that
-- one character, never the authored roster.
--
-- Authoring them needs the per-player Lorewalking campaign state that this core
-- does not yet have (there is no storage for it -- see the analysis already in
-- LFGHandler.cpp). Uncomment only together with that state and the conditions
-- that select between the three.
--
-- (845, 5047, 3, 6403386, 0, 1574, 0, 0, 1, 'Elves', 'In Progress', '', 'Continue', 'The elves of Azeroth each have their own stories that weave together into an intricate tapestry of history. For the blood elves and the void elves, it is a story shaped by strength in the face of utmost tragedy.', '', 0, 0)
-- (845, 5048, 4, 6403386, 0, 1574, 0, 0, 1, 'Elves', 'Start Over', '', 'Start Over', 'The elves of Azeroth each have their own stories that weave together into an intricate tapestry of history. For the blood elves and the void elves, it is a story shaped by strength in the face of utmost tragedy.', '11.1.7 Lorewalking - Restart (LAS)', 0, 0)

-- ---------------------------------------------------------------------------
-- STILL MISSING AFTER THIS FILE (data will not fix these)
-- ---------------------------------------------------------------------------
--   * per-player active-campaign state, persisted and resumable
--   * quest suppression while Lorewalking is active (no global mode in TC)
--   * quest-availability override on the wrapped legacy chains
-- The opcode pair CMSG_LFG_LOREWALKING_UPDATE_REQUEST / SMSG_LFG_SUSPEND_-
-- LOREWALKING is already wired on this fork (TrinityCore master leaves both
-- STATUS_UNHANDLED) and is only an LFG queue gate -- it never fired in any of
-- the five captures, so it is not on the critical path.

