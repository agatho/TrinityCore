--
-- Prey hunts — content skeleton from the 12.1.0.69273 capture.
--
-- Source: C:\dumps\SNIFF_69273_EVALUATION_AND_PLAN.md §A4, decoded quest-info
-- records in C:\dumps\scratch_prey\quests_decoded.json (decoder qdec2.py).
--
-- The capture is a 12.1.0 client stream and NO wire layout from it is emitted
-- here. Everything below is *taxonomy*: quest ids, objective ids, credit
-- creature ids and content tunings, all of which are patch-independent because
-- they are database keys rather than packet offsets.
--
-- WHAT THIS FILE DELIBERATELY DOES NOT DO
-- ---------------------------------------
-- The ten hunt quests (91098/91102/91109/91123 Normal, 91210/91212/91214/
-- 91242/91248/91255 Hard) and their twenty `quest_objectives` rows are ALREADY
-- PRESENT on this branch, imported by `2026_03_01_00_world.sql` at
-- VerifiedBuild 66384. Those rows carry LogDescription, QuestDescription,
-- PortraitGiver, sound kits and AllowableRaces that the 12.1.0 capture does not
-- reproduce, so re-inserting them from the capture would blank real data. They
-- are therefore LEFT UNTOUCHED. The capture's contribution for those ten is
-- confirmation, not content: every field §A4 quotes was compared against the
-- existing rows and matches exactly —
--
--   QuestType 2, QuestSortID -656, QuestInfoID 0, Expansion 11,
--   RewardSpell 1244010 (all ten),
--   ContentTuningID 5224 (Normal) / 5223 (Hard),
--   Flags 0x2718100 = 40993024 (Normal) / 0x2318100 = 36798720 (Hard),
--   FlagsEx 0x202000 = 2105344, FlagsEx2 0x10000000 = 268435456, FlagsEx3 0,
--   and all twenty objectives: Type 0 MONSTER, StorageIndex 0 -> ObjectID
--   246472 Flags 0 "Hunt your Prey", StorageIndex 1 -> ObjectID 253450 Flags 2
--   "<target> slain", Amount 1 on both.
--
-- 246472 ("Credit: Hunt your Prey") and 253450 ("Credit: Multiple Credit")
-- already exist in `creature_template`; both are shared by all ten hunts, i.e.
-- the per-hunt named target is not the credit source.
--

--
-- 1. Quest 96591 "Prey: Venom Ambush" — the world-quest variant.
--    This one is genuinely absent from the branch. QuestInfoID 295 is our own
--    QUEST_INFO_PREY_WORLD_QUEST (SharedDefines.h:5619) and QuestSortID -656 is
--    QUEST_SORT_PREY (SharedDefines.h:5815), so the taxonomy already exists on
--    12.0.7 — that value match is the whole cross-build correlation.
--
--    It needs no new core code: objective 473641 is Type 15
--    QUEST_OBJECTIVE_PROGRESS_BAR and the three contributing objectives carry
--    Flags 92 = 0x5C, which includes 0x40 QUEST_OBJECTIVE_FLAG_PART_OF_PROGRESS_BAR
--    with ProgressBarWeight — supported by Quest/QuestDef for years.
--
--    KNOWN GAP, read before applying: objectives 473640/473642/473643/473644
--    point at creature entries 266443/266444/266445, which have NO
--    `creature_template` row in our database. ObjectMgr will log
--    "quest can't be done" for each on world load (sql.sql, non-fatal) until
--    those creatures are imported and spawned. The quest is shipped anyway so
--    the ids stop being guesswork; the creature import is separate work.
--
DELETE FROM `quest_objectives` WHERE `ID` IN (473640, 473641, 473642, 473643, 473644);
DELETE FROM `quest_template` WHERE `ID` IN (96591);

INSERT INTO `quest_template`
  (`ID`, `QuestType`, `QuestPackageID`, `ContentTuningID`, `QuestSortID`, `QuestInfoID`,
   `SuggestedGroupNum`, `RewardNextQuest`, `RewardXPDifficulty`, `RewardXPMultiplier`,
   `RewardSpell`, `Flags`, `FlagsEx`, `FlagsEx2`, `FlagsEx3`,
   `AreaGroupID`, `TimeAllowed`, `Expansion`, `LogTitle`, `VerifiedBuild`)
VALUES
  (96591, 3, 0, 5381, -656, 295,
   0, 0, 5, 1,
   1241124, 37290240 /*0x2390100*/, 0, 0, 0,
   8797, 0, 11, 'Prey: Venom Ambush', 0);

INSERT INTO `quest_objectives`
  (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`,
   `ConditionalAmount`, `Flags`, `Flags2`, `ProgressBarWeight`, `ParentObjectiveID`,
   `Visible`, `Description`, `VerifiedBuild`)
VALUES
  -- Type 3 = QUEST_OBJECTIVE_TALKTO
  (473640, 96591,  3, 0,  3, 266443,  1, 0,  4, 0,  0, 0, 1, 'Crystal Imbued',  0),
  -- Type 15 = QUEST_OBJECTIVE_PROGRESS_BAR (StorageIndex -1: not slot-backed)
  (473641, 96591, 15, 1, -1,      0,  1, 0,  2, 0,  0, 0, 1, 'Ambush Foiled',   0),
  -- Flags 92 = 0x5C, includes 0x40 PART_OF_PROGRESS_BAR -> ProgressBarWeight applies
  (473642, 96591,  0, 2,  0, 266444, 10, 0, 92, 0, 10, 0, 1, '',                0),
  (473643, 96591,  0, 3,  1, 266445, 20, 0, 92, 0,  5, 0, 1, '',                0),
  (473644, 96591,  3, 4,  2, 266443, 50, 0, 92, 0,  2, 0, 1, '',                0);

--
-- 2. prey_hunt_template — fill in the hunt registry that
--    `2026_08_12_00_world_prey_voidforge.sql` shipped empty.
--
--    `Id` is the hunt's quest id; PreyMgr keys the rotation off it.
--    `ContentTuningId` was marked CAPTURE-BLOCKED in that file and is now known
--    from the capture: 5224 Normal / 5223 Hard.
--    `ZoneId` and `VaultActivityId` stay 0 — the capture carries neither, and a
--    guessed zone id is worse than a missing one. PreyMgr treats ZoneId 0 as
--    "any zone" so the weekly rotation still works, it just has one bucket.
--
--    These rows are inert: they only populate PreyMgr's lookup table. Nothing
--    reads them on a player-visible path until the Hunt Table wire exists.
--
DELETE FROM `prey_hunt_template` WHERE `Id` IN
  (91098, 91102, 91109, 91123, 91210, 91212, 91214, 91242, 91248, 91255);

INSERT INTO `prey_hunt_template` (`Id`, `ZoneId`, `Difficulty`, `ContentTuningId`, `VaultActivityId`) VALUES
  (91098, 0, 0, 5224, 0), -- Prey: L-N-0R the Recycler (Normal)
  (91102, 0, 0, 5224, 0), -- Prey: Nexus-Edge Hadim (Normal)
  (91109, 0, 0, 5224, 0), -- Prey: Petyoll the Razorleaf (Normal)
  (91123, 0, 0, 5224, 0), -- Prey: Grothoz, the Burning Shadow (Normal)
  (91210, 0, 1, 5223, 0), -- Prey: Magister Sunbreaker (Hard)
  (91212, 0, 1, 5223, 0), -- Prey: Magistrix Emberlash (Hard)
  (91214, 0, 1, 5223, 0), -- Prey: Senior Tinker Ozwold (Hard)
  (91242, 0, 1, 5223, 0), -- Prey: High Vindicator Vureem (Hard)
  (91248, 0, 1, 5223, 0), -- Prey: Knight-Errant Bloodshatter (Hard)
  (91255, 0, 1, 5223, 0); -- Prey: Dengzag, the Darkened Blaze (Hard)

--
-- 3. CONFLICT — TreasurePickerID. NOT APPLIED, deliberately.
--
--    §A4 quotes the capture's picker sets as [4887,4875,4877] (Normal) and
--    [4887,4874,4879] (Hard). Our `quest_treasure_pickers` already holds a
--    different, VerifiedBuild-66384 set for the same ten quests:
--        Normal -> 4269, 4541, 4774
--        Hard   -> 4270, 4541, 4777
--    Neither set is confirmed against 12.0.7.68275/68887: ours is from a 12.0.1
--    sniff, the capture's is 12.1.0, and Blizzard renumbered pickers somewhere
--    between. §R5 of the plan separately established that this capture contains
--    zero TreasurePicker hotfix records, so it cannot arbitrate its own values.
--    Overwriting a verified-for-our-line set with a newer-patch one on that
--    evidence would be a regression, so the statements stay commented. Resolve
--    with a 12.0.7 tester capture, not with this file.
--
-- DELETE FROM `quest_treasure_pickers` WHERE `QuestID` IN
--   (91098, 91102, 91109, 91123, 91210, 91212, 91214, 91242, 91248, 91255);
-- INSERT INTO `quest_treasure_pickers` (`QuestID`, `TreasurePickerID`, `OrderIndex`) VALUES
--   (91098, 4887, 0), (91098, 4875, 1), (91098, 4877, 2),
--   (91102, 4887, 0), (91102, 4875, 1), (91102, 4877, 2),
--   (91109, 4887, 0), (91109, 4875, 1), (91109, 4877, 2),
--   (91123, 4887, 0), (91123, 4875, 1), (91123, 4877, 2),
--   (91210, 4887, 0), (91210, 4874, 1), (91210, 4879, 2),
--   (91212, 4887, 0), (91212, 4874, 1), (91212, 4879, 2),
--   (91214, 4887, 0), (91214, 4874, 1), (91214, 4879, 2),
--   (91242, 4887, 0), (91242, 4874, 1), (91242, 4879, 2),
--   (91248, 4887, 0), (91248, 4874, 1), (91248, 4879, 2),
--   (91255, 4887, 0), (91255, 4874, 1), (91255, 4879, 2);

--
-- 4. Two further capture-vs-branch drifts, recorded and NOT applied:
--
--    * `quest_objectives`.`ConditionalAmount` — the capture carries 31367 (and
--      566 on two rows) on hunt objectives where our rows carry 0. Those are
--      12.1.0 PlayerCondition ids that will not resolve against our DB2, so
--      writing them would break objective display rather than improve it.
--    * `quest_template`.`RewardXPMultiplier` — the capture reports 5.0 for the
--      four Normal hunts; ours is 1. A five-fold XP change is a tuning decision,
--      not a data correction, and the capture is from a different patch.
--
