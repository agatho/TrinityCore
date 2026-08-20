-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest objectives
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_quest_objectives.sql
--   (live-counter Description text, stripped of its "N/M " progress prefix -- the DB
--   stores the plain past-tense credit line, the client prepends the live counter),
--   addon_quest_request_items.sql (item 243573 x7 for 90886), plan Part 1.1 Targets column.
-- No WDB quest_objectives rows exist for any 908xx quest (confirmed: wdb_quest_objectives.sql
--   has zero 908xx rows) -- every row below is addon-sourced or plan-table-inferred, flagged
--   per row. QuestObjective.Type enum verified against
--   src/server/game/Quests/QuestDef.h:358-376 in this worktree (0=MONSTER,1=ITEM,
--   3=TALKTO,10=AREATRIGGER,15=PROGRESS_BAR).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is `ID`).
-- ============================================================================
--
-- ---- ID scheme (synthetic, NOT a real DB2 QuestObjective.ID -- none was captured) ----
--   ID = QuestID*100 + StorageIndex*10 + row-within-StorageIndex
-- e.g. 90882's five gnoll-credit rows (all StorageIndex 0) are 9088200..9088204.
--
-- ---- Multi-target credit pattern (90882 gnolls, 90893 siege) ----
-- Player::UpdateQuestObjectiveProgress (src/server/game/Entities/Player/Player.cpp:17854)
-- looks up ALL quest_objectives rows matching (Type, ObjectID) via m_questObjectiveStatus,
-- regardless of how many rows share a QuestID+StorageIndex -- so several creature entries
-- can each independently increment the SAME player quest-slot counter (StorageIndex) as
-- long as every row carries the same QuestID/StorageIndex/Amount. This is the standard TC
-- mechanism for "kill N of these several species" objectives; verified against this
-- worktree's Player.cpp, not guessed.
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

-- ---- 90883 "To Go'shek Farm" -- travel (INFERRED Type/ObjectID -- flagged [G]) ----
-- [C] addon Description "0/1 Ride a flying mount" (addon_quest_objectives.sql row 7) is a
-- genuine captured bonus-detail objective, not a guess -- but no AreaTrigger ID was
-- captured for the flight's landing point. Authored as Type=10 AREATRIGGER (the closest
-- fit for a scripted-taxi arrival credit) with ObjectID=0 as an explicit placeholder.
-- TODO Phase K: replace ObjectID with a real AreaTrigger once the landing trigger is
-- captured/created; until then this objective will not auto-complete in-game.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9088300, 90883, 10, 0, 0, 0, 1, 'Ride a flying mount')
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
-- [C] addon Description "Repel the Ogre Siege (0%)" -> stripped, Amount=100 (percent-to-
-- completion). Modeled as 9 Type=0 MONSTER credit rows sharing StorageIndex=0 (same
-- multi-target pattern as 90882) with an EQUAL ProgressBarWeight split (100/9 =~ 11.11 per
-- species) -- real retail per-species weighting is not recoverable from this capture, so
-- equal-split is a documented ASSUMPTION, not a captured value. TODO Phase K: replace with
-- real weights if a future capture records the progress-bar increments per kill.
-- Targets 244682/244685/244695/244711/244785/244691/244786/257072/244683 per plan Part 1.1
-- (all confirmed in Task 1's 10_creature_template.sql).
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `ProgressBarWeight`, `Description`) VALUES
 (9089300, 90893, 0, 0, 0, 244682, 100, 11.11, 'Repel the Ogre Siege'),  -- Kobold Waxmancer
 (9089301, 90893, 0, 0, 0, 244685, 100, 11.11, 'Repel the Ogre Siege'),  -- Ogre Basher
 (9089302, 90893, 0, 0, 0, 244695, 100, 11.11, 'Repel the Ogre Siege'),  -- Ettin Crusher
 (9089303, 90893, 0, 0, 0, 244711, 100, 11.11, 'Repel the Ogre Siege'),  -- Armored Cleaver
 (9089304, 90893, 0, 0, 0, 244785, 100, 11.11, 'Repel the Ogre Siege'),  -- Armored Cleaver
 (9089305, 90893, 0, 0, 0, 244691, 100, 11.11, 'Repel the Ogre Siege'),  -- Gnoll Charger
 (9089306, 90893, 0, 0, 0, 244786, 100, 11.11, 'Repel the Ogre Siege'),  -- Gnoll Charger
 (9089307, 90893, 0, 0, 0, 257072, 100, 11.12, 'Repel the Ogre Siege'),  -- Gnoll Biter (remainder to sum exactly 100)
 (9089308, 90893, 0, 0, 0, 244683, 100, 11.11, 'Repel the Ogre Siege')   -- Gnoll Prowler
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `ProgressBarWeight`=VALUES(`ProgressBarWeight`), `Description`=VALUES(`Description`);

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

-- ---- 90911 "Your Next Adventure" -- gossip hub (Choose your next adventure) ----
-- [C] addon Description "0/1 Next Adventure Chosen" -> stripped. Type=3 TALKTO: completed
-- via gossip interaction with the ender 244714 (same clone, npcflag=1/gossip_menu_id=39348
-- per Task 1), matching the "gossip hub" framing in plan Part 1.1.
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Description`) VALUES
 (9091100, 90911, 3, 0, 0, 244714, 1, 'Next Adventure Chosen')
ON DUPLICATE KEY UPDATE `Type`=VALUES(`Type`), `Order`=VALUES(`Order`), `StorageIndex`=VALUES(`StorageIndex`), `ObjectID`=VALUES(`ObjectID`), `Amount`=VALUES(`Amount`), `Description`=VALUES(`Description`);
