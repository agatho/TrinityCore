-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (creature_text)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/creature_text.sql
--   (16 rows, new=16 changed=0 same=16 against the reference -- captured verbatim).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live
-- DB/realm. Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
--
-- ROSTER CHECK: all 10 distinct CreatureIDs below (244670, 244671, 244672, 244675,
-- 244655, 244658, 244677, 244682, 244709, 257072) are confirmed present in Task 1's
-- authored roster (10_creature_template.sql) -- grep-verified, no entries outside that
-- file's ~46-entry curated set.
--
-- EMOTE COLUMN: every row in the captured bundle has Emote=0 (no visible emote played
-- alongside the bark). Authored as captured -- 0 is a valid "no emote" value, so no
-- "-- Emote unverified (Phase K Emotes.db2)" tag is needed on any row below (that tag
-- would only apply to a captured non-zero Emote id, and none exist in this bundle).
--
-- *** TASK-4 CONTRACT: Runk (244675) / Ro'grok (244709) GroupID remap ***
-- 40_smart_scripts.sql's SMART_ACTION_TALK rows for Runk and Ro'grok reference
-- creature_text groupid 0 (SMART_EVENT_AGGRO) and groupid 1 (SMART_EVENT_DEATH),
-- target_type SMART_TARGET_SELF (see that file's "id0: aggro bark -- depends: Task 5
-- creature_text (groupid 0=aggro)" / "id3: death bark -- depends: Task 5 creature_text
-- (groupid 1=death)" comments for both bosses). The RAW capture, however, has BOTH of
-- each boss's lines under GroupID=0 (as sequential IDs 0 and 1 within that one group --
-- see the bundle SQL: (244675,0,0,...)/(244675,0,1,...) and (244709,0,0,...)/
-- (244709,0,1,...)). That raw grouping cannot satisfy Task 4's aggro/death split, so the
-- two rows for EACH of these two bosses are deliberately REMAPPED below:
--   GroupID 0 (aggro) <- the boastful "we will win" line (ID reset to 0 within the group)
--   GroupID 1 (death) <- the anguished/last-words line (ID reset to 0 within its group)
-- This split is not arbitrary: the line text itself disambiguates aggro vs. death ("We
-- take farm, Stromgarde, then ALL Arathi!" / "Me still win! Destroy ALL in Arathi!" are
-- defiant boasts fitting SMART_EVENT_AGGRO; "How... plan... fail?" / "Arathi... never...
-- be ours..." are last words fitting SMART_EVENT_DEATH), and for Ro'grok it is also
-- independently corroborated by the plan sec 1.4 combat-log evidence table, which
-- directly evidences Ro'grok's death trigger as "-> TALK (bark)" (reproduced in
-- 40_smart_scripts.sql's banner). No groupid collisions result: each boss now has
-- exactly one line in GroupID 0 and one line in GroupID 1.
-- All 12 remaining rows (244670, 244671, 244672, 244677 x3, 244655, 244658, 257072 x3,
-- 244682) have no Task 4 dependency and keep their captured GroupID/ID exactly as
-- harvested.
-- ============================================================================

INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244670, 0, 0, 'No more nasty Horde!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244671, 0, 0, 'Wanted... tasty... meats...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244672, 0, 0, 'Wanted... tasty... meats...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244677, 0, 0, 'We get LOTS of candles for this!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244677, 0, 1, 'Candle... burn... no more...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244677, 0, 2, 'Maybe... make.. bad... deal...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244655, 0, 0, 'The plans you recovered suggest that Stromgarde is their next target. We must make haste!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244658, 0, 0, 'Thrall and I will locate their leader. Meet up with us once you''ve disrupted their forces.', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 0, 'Burn... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 1, 'We take Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
  -- captured duplicate of GroupID0/ID0 text -- preserved as harvested, not deduplicated
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 2, 'Burn... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244682, 0, 0, 'We take ogre deal! You no stop!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- Runk 244675 -- REMAPPED to satisfy Task 4 groupid 0=aggro / 1=death contract (see banner) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244675, 0, 0, 'We take farm, Stromgarde, then ALL Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244675, 1, 0, 'How... plan... fail?', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- Ro'grok 244709 -- REMAPPED to satisfy Task 4 groupid 0=aggro / 1=death contract (see banner) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244709, 0, 0, 'Me still win! Destroy ALL in Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244709, 1, 0, 'Arathi... never... be ours...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
