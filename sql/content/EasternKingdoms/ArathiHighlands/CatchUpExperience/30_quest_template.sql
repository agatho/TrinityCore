-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest_template deltas
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (addon_quest_template.sql,
--   wdb_quest_template.sql). Plan Part 1.1 (11-quest Alliance "Siege of Arathi Highlands"
--   chain, 90882-90911). Depends on Task 1 creature_template (10_creature_template.sql)
--   for every questgiver/ender entry referenced by 31/33; Task 2 PhaseIds 15901-15905
--   (20_creature_spawns.sql/21_phase_area.sql) for narrative-consistency only (quests do
--   not themselves carry a PhaseId column).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is `ID`).
-- ============================================================================
--
-- SCOPE: real `quest_template` schema (sql/base/dev/world_database.sql:3493) uses
-- LogTitle/LogDescription/QuestDescription (NOT the TCHarvest addon dump's simplified
-- title/details/objectives aliases) and RewardBonusMoney/RewardXPDifficulty (NOT
-- reward_money/reward_xp). Both are confirmed ALREADY POPULATED correctly in the live/WDB-
-- sourced quest_template for every quest in the chain (reward_money 5350/5350/5350/5350/
-- 5350/53500/53500/107000/5350 and reward_xp_difficulty 1/1/1/1/1/5/5/6/1 for
-- 90882/83/85/86/87/88/93/95/96/97 respectively -- exact match to plan Part 1.1's
-- money/XP-tier column) -- NO reward-field delta is authored here for any of the 11.
--
-- Only two deltas are needed:
--   1) 90882 & 90883 -- BLANK text in the WDB cache (wdb_quest_template.sql: `title`='',
--      `details`='', `objectives`='' for both, confirmed by direct row inspection) --
--      author LogTitle/LogDescription/QuestDescription verbatim from
--      addon_quest_template.sql (the only source with real text for these two).
--   2) 90897 -- AllowableRaces Alliance-only (Requirement 1; distinguishes this Alliance-
--      path "Back to Stromgarde" from the uncaptured Horde counterpart 90898 "Back to
--      Hammerfall" per plan Part 1.1 row 12). Partial-column UPDATE only -- does not
--      touch 90897's already-correct text/reward columns.
-- The other 9 quests (90885/86/87/88/93/95/96/911) need NO quest_template delta at all.
-- ============================================================================

-- ---- 90882 "Gnoll Way" -- text-only delta (blank in WDB; addon-sourced verbatim) ----
INSERT INTO `quest_template` (`ID`, `LogTitle`, `LogDescription`, `QuestDescription`) VALUES
 (90882, 'Gnoll Way',
'Good to have you here, Iluà.

Thrall and I were in the area when we heard reports of a massive gnoll attack on Hammerfall.

We could use your aid eliminating the gnolls here while we aid the wounded and figure out our next move.',
'Slay 10 gnolls within Hammerfall.')
ON DUPLICATE KEY UPDATE `LogTitle`=VALUES(`LogTitle`), `LogDescription`=VALUES(`LogDescription`), `QuestDescription`=VALUES(`QuestDescription`);

-- ---- 90883 "To Go'shek Farm" -- text-only delta (blank in WDB; addon-sourced verbatim) ----
INSERT INTO `quest_template` (`ID`, `LogTitle`, `LogDescription`, `QuestDescription`) VALUES
 (90883, 'To Go''shek Farm',
'We''ve received word a nearby farm is under attack by ogres and kobolds.

We have to move before more lives are lost.',
'Travel to Go''shek Farm')
ON DUPLICATE KEY UPDATE `LogTitle`=VALUES(`LogTitle`), `LogDescription`=VALUES(`LogDescription`), `QuestDescription`=VALUES(`QuestDescription`);

-- ---- 90897 "Back to Stromgarde (Alliance)" -- AllowableRaces delta only ----
-- RACEMASK_ALLIANCE computed from src/server/game/Miscellaneous/RaceMask.h (this repo,
-- 2026-08-20): bits {0 Human,2 Dwarf,3 NightElf,6 Gnome,10 Draenei,21 Worgen,
-- 24 PandarenAlliance,28 VoidElf,29 LightforgedDraenei,31 KulTiran,11 DarkIronDwarf,
-- 14 Mechagnome,16 DracthyrAlliance,18 EarthenDwarfAlliance,20 HaranirAlliance} summed =
-- 2973060173. Text/reward columns untouched (already correct/live) -- partial-column
-- UPDATE keeps this idempotent without clobbering anything else on the row.
INSERT INTO `quest_template` (`ID`, `AllowableRaces`) VALUES
 (90897, 2973060173)
ON DUPLICATE KEY UPDATE `AllowableRaces`=VALUES(`AllowableRaces`);
