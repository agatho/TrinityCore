-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest chain wiring
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source: plan Part 1.1 "Chain" column (quest_timelines.txt tick order + reward-tier
--   cross-check; next_quest=0 in WDB for every 908xx row -- chain is NOT cache-encoded,
--   authored here per the plan's explicit instruction).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is `ID`).
-- ============================================================================
--
-- ---- THE CHAIN (11 quests, Alliance path) ----
--   90882 (start) -> 90883 -> {90885,90886,90887} (farm trio, ExclusiveGroup -190885)
--     -> 90888 (90887 is the trio member carrying the forward pointer, per plan table)
--     -> {90893,90895} (siege pair, ExclusiveGroup -190893)
--     -> 90896 (90895 is the pair member carrying the forward pointer, per plan table)
--     -> 90897 -> 90911 (terminus, gossip hub)
--
-- ExclusiveGroup is a NEGATIVE shared value per requirement; the trio/pair do not
-- literally exclude each other in the fiction (they run concurrently at the farm/siege),
-- but the brief's Requirement 2 explicitly directs authoring them as ExclusiveGroup
-- siblings, so that is followed verbatim here. Only the group's LAST member per the
-- plan table's arrow (90887 for the farm trio, 90895 for the siege pair) carries a
-- non-zero NextQuestID; siblings without an explicit "->" in the plan table keep
-- NextQuestID=0 (their own PrevQuestID=90883/90888 already gates their availability).
--
-- ---- RESERVED SYNTHETIC KEY RANGES (fix round 1 documentation, no value change) ----
-- This file reserves ExclusiveGroup values -190885 (farm trio) and -190893 (siege pair);
-- the companion file 32_quest_objectives.sql reserves quest_objectives.ID range
-- ~9088200-9091100 (QuestID*100+StorageIndex*10+row). Neither range is a real DB2/WDB-
-- sourced value -- both are self-assigned by this task because none were captured.
-- TODO Phase K: verify neither range collides with a real quest_template_addon.
-- ExclusiveGroup or quest_objectives.ID value already present in the live world DB before
-- this candidate SQL is ever applied to a branch.
-- ============================================================================

INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`) VALUES
 (90882, 0,     90883, 0),        -- chain start (no prerequisite)
 (90883, 90882, 0,     0),        -- opens the farm trio (each trio member sets its own PrevQuestID=90883)
 (90885, 90883, 0,     -190885),  -- farm trio member 1/3 "My Beautiful Pumpkins"
 (90886, 90883, 0,     -190885),  -- farm trio member 2/3 "Best Laid Plans of Kobolds and Ogres"
 (90887, 90883, 90888, -190885),  -- farm trio member 3/3 "Farmer's Nemesis" -- carries the forward pointer to 90888
 (90888, 90887, 0,     0),        -- "Saving Stromgarde Keep" -- opens the siege pair
 (90893, 90888, 0,     -190893),  -- siege pair member 1/2 "Repelling the Siege"
 (90895, 90888, 90896, -190893),  -- siege pair member 2/2 "Catapult Bombardment" -- carries the forward pointer to 90896
 (90896, 90895, 90897, 0),        -- "One Last Ogre"
 (90897, 90896, 90911, 0),        -- "Back to Stromgarde (Alliance)" -- AllowableRaces set in 30_quest_template.sql
 (90911, 90897, 0,     0)         -- "Your Next Adventure" -- terminus, gossip hub, no further chain
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`), `NextQuestID`=VALUES(`NextQuestID`), `ExclusiveGroup`=VALUES(`ExclusiveGroup`);
