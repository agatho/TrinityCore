-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest objectives
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_quest_objectives.sql
--   (live-counter Description text, stripped of its "N/M " progress prefix -- the DB
--   stores the plain past-tense credit line, the client prepends the live counter),
--   addon_quest_request_items.sql (item 243573 x7 for 90886), plan Part 1.1 Targets column.
-- No WDB quest_objectives rows exist for any 908xx quest (confirmed: wdb_quest_objectives.sql
--   has zero 908xx rows) -- every row below is addon-sourced or plan-table-inferred, flagged
--   per row. QuestObjective.Type enum verified against
--   src/server/game/Quests/QuestDef.h:354-378 in this worktree (0=MONSTER,1=ITEM,
--   3=TALKTO,10=AREATRIGGER,14=CRITERIA_TREE,15=PROGRESS_BAR). 14 added Horde-xval H3
--   task-4 (90883/90911 fixes, see those sections below for why AREATRIGGER/TALKTO were
--   wrong for those two rows).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is `ID`).
-- ============================================================================
--
-- ---- ID scheme (synthetic, NOT a real DB2 QuestObjective.ID -- none was captured) ----
--   ID = QuestID*100 + StorageIndex*10 + row-within-StorageIndex
-- e.g. 90882's five gnoll-credit rows (all StorageIndex 0) are 9088200..9088204.
--
-- ---- RESERVED SYNTHETIC KEY RANGES (fix round 1 documentation, no value change) ----
-- This file reserves quest_objectives.ID range ~9088200-9091100 (QuestID*100+StorageIndex*10
-- +row, per the scheme above); the companion file 31_quest_template_addon.sql reserves
-- ExclusiveGroup values -190885 (farm trio) and -190893 (siege pair). Neither range is a
-- real DB2/WDB-sourced value -- both are self-assigned by this task because none were
-- captured. TODO Phase K: verify neither range collides with a real quest_objectives.ID or
-- quest_template_addon.ExclusiveGroup value already present in the live world DB before
-- this candidate SQL is ever applied to a branch.
--
-- ---- Multi-target credit pattern (90882 gnolls) ----
-- Player::UpdateQuestObjectiveProgress (src/server/game/Entities/Player/Player.cpp:17854)
-- looks up ALL quest_objectives rows matching (Type, ObjectID) via m_questObjectiveStatus,
-- regardless of how many rows share a QuestID+StorageIndex -- so several creature entries
-- can each independently increment the SAME player quest-slot counter (StorageIndex) as
-- long as every row carries the same QuestID/StorageIndex/Amount. This is the standard TC
-- mechanism for a plain "kill N of these several species" objective (90882); verified
-- against this worktree's Player.cpp, not guessed. 90893's weighted PROGRESS_BAR objective
-- below is a DIFFERENT pattern (each contributing species needs its OWN StorageIndex, not
-- a shared one) -- see that section's banner for the fix-round-1 correction and why.
-- ============================================================================

-- ---- 90882 "Gnoll Way" -- Slay 10 gnolls within Hammerfall (5 creditable entries) ----
-- [C] addon Description "0/10 Gnoll slain" -> stored plain form "Gnoll slain".
-- Targets 244669/244670/244671/244672/245027 per plan Part 1.1 (all confirmed in Task 1's
-- 10_creature_template.sql). Amount=10 (total) is repeated on every row per TC convention
-- (each row is a "how many kills of THIS species contribute, up to the shared cap").
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088200, 90882, 0, 0, 0, 244669, 10, 'Gnoll slain'),  -- Scavenging Hyena
 (9088201, 90882, 0, 0, 0, 244670, 10, 'Gnoll slain'),  -- Gnoll Bowblaster
 (9088202, 90882, 0, 0, 0, 244671, 10, 'Gnoll slain'),  -- Gnoll Ripper
 (9088203, 90882, 0, 0, 0, 244672, 10, 'Gnoll slain'),  -- Gnoll Bruiser
 (9088204, 90882, 0, 0, 0, 245027, 10, 'Gnoll slain')   -- Gnoll Assailant
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90883 "To Go'shek Farm" -- "Ride a flying mount" (INTEGRATION FIX 2026-08-21) ----
-- [C] addon Description "0/1 Ride a flying mount". This is a SKYRIDING flight on the player's
-- OWN mount (no vehicle / no areatrigger landing). The OFFICIAL feature/arathi-rpe branch
-- already implements the completion mechanism in C++: zone_arathi_highlands_rpe.cpp's
-- PlayerScript credits NPC 239009 via KilledMonsterCredit(239009) when the player casts a
-- MOUNTED-aura spell while on map 2927 with 90883 incomplete. So the CORRECT objective row is
-- a Type=0 MONSTER kill-credit on NPC **239009** (NOT the earlier criteria-tree placeholder,
-- which had no backing row and was a different, conflicting mechanism). This unifies both
-- branches on ONE working mechanism: this objective + arathi-rpe's mount-credit script.
-- (Skyriding itself lives on feature/skyriding-player-system, combined at central integration;
--  the mount-aura credit works with or without it, so 90883 is testable now.)
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088300, 90883, 0, 0, 0, 239009, 1, 'Ride a flying mount')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90885 "My Beautiful Pumpkins" -- Recover 4 Prized Pumpkins ----
-- [C] addon Description "0/4 Prized Pumpkin recovered" -> stripped. Target 244956 Prized
-- Pumpkin is a CREATURE (Task 1: type=7, subname='questinteract'), not an item -- credited
-- via Type=0 MONSTER (TC's Player::KilledMonsterCredit is agnostic to whether the credit
-- is fired by a real kill or a scripted SAI CALL_KILLEDMONSTER action on interact, per the
-- house-style precedent in DragonIsles/WakingShores/65997-chasing-sendrax.sql section 3).
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088500, 90885, 0, 0, 0, 244956, 4, 'Prized Pumpkin recovered')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90886 "Best Laid Plans of Kobolds and Ogres" -- Collect 7 Poorly Written Plans ----
-- [C] item count/id CONFIRMED: addon_quest_request_items.sql `243573:7`. addon Description
-- was empty/garbled ("0/7  ", addon_quest_objectives.sql row 20) -- left blank rather than
-- inventing text; TODO Phase K resolve real Description.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088600, 90886, 1, 0, 0, 243573, 7, '')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90887 "Farmer's Nemesis" -- Slay Runk ----
-- [C] addon Description "0/1 Runk slain" -> stripped. Target 244675 Runk (Task 1 [C hard]).
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088700, 90887, 0, 0, 0, 244675, 1, 'Runk slain')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90888 "Saving Stromgarde Keep" -- travel (FULLY INFERRED -- no addon row exists) ----
-- No addon_quest_objectives.sql row was captured for 90888 at all (unlike 90883, which at
-- least had a captured bonus-objective line). Authored by analogy to 90883: Type=10
-- AREATRIGGER, ObjectID=0 placeholder, Description reused verbatim from the quest's own
-- QuestDescription text ("Travel to Stromgarde Keep.", already live in world DB, see
-- 30_quest_template.sql banner). TODO Phase K: confirm/replace, this is the least-confident
-- row in this file.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088800, 90888, 10, 0, 0, 0, 1, 'Travel to Stromgarde Keep')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90893 "Repelling the Siege" -- progress-bar objective (9 creditable entries) ----
-- [C] addon Description "Repel the Ogre Siege (0%)" -> stripped. Modeled per this
-- worktree's real engine mechanics (src/server/game/Quests/QuestDef.h:372-393,
-- src/server/game/Entities/Player/Player.cpp:17971-17996/18254-18265/18401-18414),
-- verified by direct code read, not assumed:
--   * A MASTER row, Type=15 QUEST_OBJECTIVE_PROGRESS_BAR, its own StorageIndex, no
--     PART_OF_PROGRESS_BAR flag -- this is the row `CanCompleteQuest`/
--     `IsQuestObjectiveComplete` actually check quest-completion against.
--   * 9 SUB rows, Type=0 MONSTER, `Flags`=64 (0x0040 QUEST_OBJECTIVE_FLAG_
--     PART_OF_PROGRESS_BAR) -- `CanCompleteQuest` explicitly SKIPS these from its own
--     per-objective completeness check (`!(obj.Flags & ...PART_OF_PROGRESS_BAR)` guard),
--     delegating entirely to `IsQuestObjectiveProgressBarComplete`.
--   * `IsQuestObjectiveProgressBarComplete` sums, per PART_OF_PROGRESS_BAR row,
--     `GetQuestSlotObjectiveData(slot, obj) * obj.ProgressBarWeight` and completes at
--     >=100.0. `GetQuestSlotObjectiveData` reads the player's per-StorageIndex kill
--     counter for THAT row -- so every sub-row MUST have its OWN, DISTINCT StorageIndex
--     (0-8 here; master row takes StorageIndex 9, within the 24-slot MAX_QUEST_COUNTS
--     array, Player.h:699 / UpdateFields.h ObjectiveProgress[24]). Sharing one
--     StorageIndex across all 9 (this file's original authoring, fixed here) would make
--     every row read the SAME shared kill count, so
--     `IsQuestObjectiveProgressBarComplete` would sum totalKills*(w1+...+w9) =
--     totalKills*100 and complete after a single kill of ANY of the 9 species --
--     caught and corrected in fix round 1, not part of the original review ask, but
--     required for the fix to actually work per the traced engine logic above.
-- Per-species ProgressBarWeight is an EQUAL split (100/9 =~ 11.11, remainder 0.01 on the
-- last row so the pool sums to exactly 100.00) -- real retail per-species weighting is not
-- recoverable from this capture; documented ASSUMPTION, not a captured value. TODO Phase K:
-- replace with real weights if a future capture records the progress-bar increments per kill.
-- Targets 244682/244685/244695/244711/244785/244691/244786/257072/244683 per plan Part 1.1
-- (all confirmed in Task 1's 10_creature_template.sql).
--
-- 0-100% pool: 11.11(244682)+11.11(244685)+11.11(244695)+11.11(244711)+11.11(244785)
--            +11.11(244691)+11.11(244786)+11.12(257072)+11.11(244683) = 100.00
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Flags`, `Description`) VALUES
 (9089390, 90893, 15, 0, 9, 0, 100, 0, 'Repel the Ogre Siege')  -- MASTER progress-bar row, StorageIndex 9
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Flags`=VALUES(`Flags`), `Description`=VALUES(`Description`);

INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Flags`, `ProgressBarWeight`, `Description`) VALUES
 (9089300, 90893, 0, 0, 0, 244682, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Kobold Waxmancer
 (9089310, 90893, 0, 0, 1, 244685, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Ogre Basher
 (9089320, 90893, 0, 0, 2, 244695, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Ettin Crusher
 (9089330, 90893, 0, 0, 3, 244711, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Armored Cleaver
 (9089340, 90893, 0, 0, 4, 244785, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Armored Cleaver
 (9089350, 90893, 0, 0, 5, 244691, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Gnoll Charger
 (9089360, 90893, 0, 0, 6, 244786, 100, 64, 11.11, 'Repel the Ogre Siege'),  -- Gnoll Charger
 (9089370, 90893, 0, 0, 7, 257072, 100, 64, 11.12, 'Repel the Ogre Siege'),  -- Gnoll Biter (remainder to sum exactly 100)
 (9089380, 90893, 0, 0, 8, 244683, 100, 64, 11.11, 'Repel the Ogre Siege')   -- Gnoll Prowler
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Flags`=VALUES(`Flags`), `ProgressBarWeight`=VALUES(`ProgressBarWeight`), `Description`=VALUES(`Description`);

-- ---- 90895 "Catapult Bombardment" -- Apply Jaina's Runes to 4 Catapults ----
-- [C] addon Description "0/4 Catapults destroyed" -> stripped. Target 249269 Worn Catapult
-- is a CREATURE (Task 1: type=7, subname='questinteract'), same "interact = kill-credit
-- via SAI" pattern as 90885's pumpkins above.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9089500, 90895, 0, 0, 0, 249269, 4, 'Catapults destroyed')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90896 "One Last Ogre" -- Slay Ro'grok ----
-- [C] addon Description "0/1 Ro'grok slain" -> stripped. Target 244709 Ro'grok.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9089600, 90896, 0, 0, 0, 244709, 1, 'Ro''grok slain')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90897 "Back to Stromgarde (Alliance)" -- talk to Jaina (INFERRED -- no addon row) ----
-- No addon_quest_objectives.sql row exists for 90897. Type=3 TALKTO fits directly: the
-- quest's own ender (244714, per plan Part 1.1 Giver/Ender column and Task 1's
-- 10_creature_template.sql) IS the NPC the player is instructed to meet/talk to.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9089700, 90897, 3, 0, 0, 244714, 1, 'Jaina Proudmoore met')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);

-- ---- 90911 "Your Next Adventure" -- gossip hub (Horde-xval H3 task-4 FIX -- was wrong
-- type AND Alliance-only hardcode) ----
-- [C] addon Description "0/1 Next Adventure Chosen" -> stripped. FIX ROUND 2 (Horde-xval
-- H3): the original Type=3 TALKTO / ObjectID=244714 authoring was WRONG on two counts --
-- (1) mechanism: 90911 is completed by picking a response in the "Where Do You Want To
-- Go?" PlayerChoice popup (choiceId ~902, see PlayerChoice.h / Player::SendPlayerChoice,
-- Player.cpp:30235-30358), not by a plain TALKTO greet -- TALKTO credits on simply opening
-- gossip with the NPC, before the player has actually made a choice; (2) faction: 244714
-- is Jaina, an ALLIANCE-ONLY creature (30_quest_template.sql AllowableRaces on the
-- Alliance side of this chain) -- hardcoding it as the objective's ObjectID meant this
-- objective could NEVER complete for a Horde character even after this task's Task 2 wired
-- 244715 Thrall as 90911's Horde giver/ender in 33_creature_quest_links.sql.
-- QuestDef.h:354-378 (this worktree) has no dedicated "complete this PlayerChoice"
-- objective type -- the closest correct mechanism is Type=14 QUEST_OBJECTIVE_CRITERIA_TREE
-- (Player.cpp:16343-16346/16683-16686, same mechanism as 90883's fix above), which is how
-- TC models bespoke non-kill/non-talk completions via a criteria_tree row. ObjectID is left
-- 0 as an explicit placeholder (no creature id, faction-neutral by construction -- this
-- fixes the Alliance hardcode even before the real criteria_tree exists) rather than
-- reusing either 244714 or 244715.
-- TODO Phase K: the real completion is PlayerChoice id ~902 (the "Where Do You Want To Go?"
-- popup, authored separately per H6) -- author a criteria_tree row whose backing Criteria
-- fires on completing PlayerChoice 902 (CriteriaType::PlayerChoice-adjacent gossip-option
-- interaction enum, DBCEnums.h:2249, is an NPCInteractionType id not a CriteriaType --
-- confirm the correct CriteriaType for "completed PlayerChoice N" against a live DB2 pull
-- before wiring it), then point ObjectID at that criteria_tree's ID.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9091100, 90911, 14, 0, 0, 0, 1, 'Next Adventure Chosen')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);
