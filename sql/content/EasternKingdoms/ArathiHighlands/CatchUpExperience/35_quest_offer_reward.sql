-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E turn-in rewards
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/quest_offer_reward.sql
--   (RewardText, addon-captured verbatim) and addon_quest_template.sql (`reward_items`
--   candidate choice rewards). Plan Part 1.1 money/XP-tier column + "Quest-authoring
--   notes" paragraph.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================
--
-- ---- SCHEMA NOTE (why this file touches two tables) ----
-- The real `quest_offer_reward` table (sql/base/dev/world_database.sql:3231) holds ONLY
-- ID/Emote1-4/EmoteDelay1-4/RewardText -- it has no money/XP/item columns. Those live on
-- `quest_template` (RewardBonusMoney, RewardMoneyDifficulty, RewardXPDifficulty,
-- RewardChoiceItemID1-6/Quantity/DisplayID) per sql/base/dev/world_database.sql:3493. The
-- TCHarvest addon dump's simplified `reward_money`/`reward_xp`/`reward_items` column names
-- do not map 1:1 onto either real table; this file authors against the REAL schema in both
-- places, kept together here because the brief's Requirement 6 scopes them as one unit.
--
-- ---- Money/XP: see 30_quest_template.sql (fix round 1 correction) ----
-- 90882 and 90883 now get an EXPLICIT RewardBonusMoney/RewardXPDifficulty delta in
-- 30_quest_template.sql (5350/1 each, plan Part 1.1 tier "t1") rather than being trusted
-- from their own blank-text WDB rows -- reviewer correctly flagged that a row already
-- known unreliable for text should not be silently trusted for its numeric fields either.
-- The other 8 non-terminus quests (90885,86,87,88,93,95,96,97) have NON-blank WDB rows and
-- keep their reward_money/reward_xp_difficulty TRUSTED-FROM-WORLD-DB (5350/1 for
-- 85,86,87,88,97; 53500/5 for 93,95; 107000/6 for 96 -- exact match to plan Part 1.1's
-- money/XP-tier column), pending Phase K verification against a real capture -- no delta
-- authored for them here or in 30_quest_template.sql. 90911 is the hub terminus and has no
-- reward at all (confirmed: no reward_money/reward_items in either wdb_quest_template.sql
-- or addon_quest_template.sql for 90911).
-- ============================================================================

-- ---- SECTION 1 -- quest_offer_reward.RewardText (addon-sourced verbatim, [C]) ----
-- 90911 intentionally excluded -- no RewardText row exists in the source dump for it
-- (hub terminus, no reward screen).
INSERT INTO `quest_offer_reward` (`ID`, `RewardText`) VALUES
 (90882, 'Excellent job. However, we cannot rest.

We have word of more chaos in the highlands.'),
 (90883, 'I never thought I''d get this much help! Please, you have to stop them!'),
 (90885, 'That''s... quite a lot of pumpkins.

I think our farmer friend should take them back to Hammerfall. Horde reinforcements should be there by now.'),
 (90886, 'Just as I thought.

I can barely read any of this.'),
 (90887, 'This should help disperse the creatures. Without a leader they''ll flee when Alliance and Horde reinforcements show up.'),
 (90888, 'This looks bad, but it''s nothing we can''t handle.'),
 (90893, 'The siege is at an end. We''ve got their leader cornered.'),
 (90895, 'With the catapults gone the soldiers of Stromgarde should be able to gain the upper hand.'),
 (90896, 'The siege is over. Arathi Highlands will hopefully return to a somewhat peaceful state now.'),
 (90897, 'It''s good to see the people here in high spirits.')
ON DUPLICATE KEY UPDATE `RewardText`=VALUES(`RewardText`);

-- ============================================================================
-- SECTION 2 -- quest_template.RewardChoiceItemID1-6 :: 90883 BAGS ONLY (the 8 gear
-- quests are now QuestPackage-driven -- see 36_quest_package_item.sql)
-- ============================================================================
-- ***** BUG FIX (issue 4b -- class-adaptive rewards) *****
-- The 8 GEAR-reward quests (90882, 90885, 90886, 90887, 90888, 90893, 90895, 90896) NO
-- LONGER carry static RewardChoiceItemID rows here. They previously did -- authored from a
-- single class's Alliance-Shaman capture (153973/153983/154005 ...) -- and because
-- Quest::BuildQuestRewards() copies RewardChoiceItemID1-6 into the reward packet for EVERY
-- player with NO class filtering, every class of every player was offered Shaman mail. The
-- retail RPE chain instead serves class-appropriate gear via the QuestPackage mechanism
-- (quest_template.QuestPackageID -> QuestPackageItem.db2), which TrinityCore implements fully.
-- The fix moves those rewards to a minted QuestPackage per quest:
--   * 30_quest_template.sql sets QuestPackageID = 64000 + questID%1000 for the 8 quests.
--   * 36_quest_package_item.sql loads every class's set-family items into those packages
--     (DisplayType=1 CLASS -> TC filters each row by the item's own class-spec mask).
-- The static RewardChoiceItemID authoring MUST NOT coexist with the package (it would still
-- be sent unfiltered), so it is REMOVED here for all 8 gear quests. Full analysis:
-- .superpowers/sdd/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN/phk-class-rewards-report.md
--
-- 90883 is the ONLY quest that keeps a static RewardChoiceItemID row: its 4 rewards are
-- class-INDEPENDENT bags (AllowableClass -1), a genuine "pick one of 4" that every class
-- sees identically -- exactly what the static columns are for, no package needed.
-- 90897 and 90911 have NO reward_items (90897 omits the column; 90911 = no-reward terminus).
INSERT INTO `quest_template` (`ID`, `RewardChoiceItemID1`, `RewardChoiceItemQuantity1`, `RewardChoiceItemID2`, `RewardChoiceItemQuantity2`, `RewardChoiceItemID3`, `RewardChoiceItemQuantity3`, `RewardChoiceItemID4`, `RewardChoiceItemQuantity4`) VALUES
 (90883, 249773, 1, 249772, 1, 249771, 1, 188213, 1)  -- class-independent bags (kept static)
ON DUPLICATE KEY UPDATE `RewardChoiceItemID1`=VALUES(`RewardChoiceItemID1`), `RewardChoiceItemQuantity1`=VALUES(`RewardChoiceItemQuantity1`), `RewardChoiceItemID2`=VALUES(`RewardChoiceItemID2`), `RewardChoiceItemQuantity2`=VALUES(`RewardChoiceItemQuantity2`), `RewardChoiceItemID3`=VALUES(`RewardChoiceItemID3`), `RewardChoiceItemQuantity3`=VALUES(`RewardChoiceItemQuantity3`), `RewardChoiceItemID4`=VALUES(`RewardChoiceItemID4`), `RewardChoiceItemQuantity4`=VALUES(`RewardChoiceItemQuantity4`);

-- ---- IDEMPOTENT CLEANUP -- clear any previously-applied static gear choices on the 8 quests ----
-- A prior apply of this slice may have written the (now-removed) single-class RewardChoiceItemID
-- rows for the 8 gear quests into a target DB. Since those rows are deleted from this file, a
-- straight re-apply would leave the stale columns behind and they would STILL be sent
-- unfiltered alongside the package. Explicitly zero all 6 choice slots for the 8 gear quests
-- so re-apply converges to the package-only state.
UPDATE `quest_template` SET
 `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0,
 `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0,
 `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0,
 `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0,
 `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0,
 `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0
WHERE `ID` IN (90882, 90885, 90886, 90887, 90888, 90893, 90895, 90896);

-- ============================================================================
-- HORDE-XVAL FIX (H3, task-6) -- documented Horde reward-choice item ids (NOT AUTHORED)
-- ============================================================================
-- `RewardChoiceItemID1-6` (Section 2 above) is SINGLE-VALUED per quest_template row, but
-- the Horde cross-validation capture confirms the actual reward-choice item SET differs by
-- faction for most of this chain's quests (only 90883 shares the identical set both sides).
-- Applying the Horde ids as a straight overwrite of Section 2 above would silently BREAK
-- Alliance turn-ins (single scalar columns, no per-faction branching in the schema as-is)
-- -- so they are documented here for Phase K, NOT inserted as live deltas. Per-faction
-- reward selection needs either `condition` rows gating which reward SET is shown (no such
-- mechanism exists for RewardChoiceItemID today) or a class/race-conditioned alternate
-- quest_template row -- a genuine design decision, out of scope for this data-correction
-- task. Do NOT overwrite Section 2's Alliance ids with these.
--
-- Horde RewardChoiceItemID set per quest (item ids, comma-separated, no quantities beyond
-- qty 1 each, matching Section 2's pattern):
--   90882 H: 153792, 153773
--   90885 H: 153797, 153796
--   90886 H: 153785, 153786
--   90887 H: 153793
--   90888 H: 153798, 153795
--   90893 H: 153790, 153791
--   90895 H: 153788, 153794
--   90896 H: 153787, 153789
--   90883: SAME both factions -- 249773, 249772, 249771, 188213 (already Section 2's live
--     Alliance set; no Horde-specific delta needed for this one quest).
-- TODO Phase K: design and author the per-faction reward-selection mechanism (conditions
-- and/or a parallel quest_template row), then wire these ids in.
-- ============================================================================
