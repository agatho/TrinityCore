-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest_template deltas
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
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
-- reward_money/reward_xp).
--
-- ---- FIX ROUND 1 correction -- 90882/90883 reward fields ARE authored here ----
-- Original authoring trusted wdb_quest_template.sql's numeric reward_money/
-- reward_xp_difficulty for 90882/90883 even though those two rows are the SAME blank/
-- unreliable WDB cache rows whose TEXT columns had to be sourced from the addon dump
-- instead (see delta 1 below) -- reviewer correctly flagged that trusting the numeric
-- half of an already-known-unreliable row is inconsistent. FIXED: 90882 and 90883 now get
-- explicit RewardBonusMoney/RewardXPDifficulty deltas, authored from plan Part 1.1's
-- money/XP-tier column (money 5350 copper, tier "t1" -> RewardXPDifficulty=1) rather than
-- trusted from WDB.
--
-- The other 9 quests (90885/86/87/88/93/95/96/97/911) are NOT touched here -- their WDB
-- rows are NOT blank (unlike 90882/83) and their reward_money/reward_xp_difficulty values
-- (5350/1, 5350/1, 5350/1, 5350/1, 53500/5, 53500/5, 107000/6, 5350/1 for
-- 90885/86/87/88/93/95/96/97 respectively) exactly match plan Part 1.1's money/XP-tier
-- column -- TRUSTED-FROM-WORLD-DB pending Phase K verification against a real capture, not
-- re-authored here. If a future capture shows any of these 9 is also blank/wrong, add its
-- delta here following the same pattern as 90882/90883 below.
--
-- Two other deltas remain unchanged from the original authoring:
--   1) 90882 & 90883 -- BLANK text in the WDB cache (wdb_quest_template.sql: `title`='',
--      `details`='', `objectives`='' for both, confirmed by direct row inspection) --
--      author LogTitle/LogDescription/QuestDescription verbatim from
--      addon_quest_template.sql (the only source with real text for these two), PLUS the
--      reward fields per the fix-round-1 correction above.
--   2) 90897 -- AllowableRaces Alliance-only (Requirement 1; distinguishes this Alliance-
--      path "Back to Stromgarde" from the uncaptured Horde counterpart 90898 "Back to
--      Hammerfall" per plan Part 1.1 row 12). Partial-column UPDATE only -- does not
--      touch 90897's already-correct text/reward columns (90897 is one of the 9
--      trusted-from-world-DB quests above).
-- ============================================================================

-- ---- 90882 "Gnoll Way" -- text + reward delta (blank/unreliable WDB row) ----
-- Reward fields authored explicitly (fix round 1) rather than trusted from this quest's
-- own blank-text WDB row: plan Part 1.1 tier "t1" = money 5350 copper, RewardXPDifficulty 1.
--
-- ---- FIX ROUND 2 (Horde-xval H3, task-1) -- "Iluà" hardcode -- ----
-- Original authoring hardcoded the literal string "Iluà" as the greeting name in
-- LogDescription -- this was NEVER a real player name, it is a mis-transcribed/garbled
-- capture artifact. WoW's quest text substitutes the token `$N` client-side for the
-- reading player's own name (SharedStrings / quest text conventions, standard across every
-- other quest in this worktree) -- fixed below to use `$N` like every other quest in this
-- chain already does. Cross-slice grep (Horde-xval H3) confirms this was the ONLY
-- "Iluà"/"Ilu" hardcode anywhere under this feature's SQL slices.
--
-- The stored text below is Alliance/Jaina-POV ("Good to have you here, $N. Thrall and I...").
-- NOTE: the Horde/Thrall-POV variant of this SAME greeting differs in wording (retail:
-- "Lok'tar, $N. Jaina and I...") -- per the brief this is NOT authored as a second row here;
-- the client/quest-giver POV substitution for the shared opening greeter (244642 Thrall vs
-- 244643 Jaina, both now wired to 90882 as of this task's Task 2) is a Phase-K text-capture
-- item, not fixable by a $N token swap alone. Flagging so nobody assumes this single
-- Alliance-POV string is what a Horde player will actually see.
INSERT INTO `quest_template` (`ID`, `LogTitle`, `LogDescription`, `QuestDescription`, `RewardBonusMoney`, `RewardXPDifficulty`) VALUES
 (90882, 'Gnoll Way',
'Good to have you here, $N.

Thrall and I were in the area when we heard reports of a massive gnoll attack on Hammerfall.

We could use your aid eliminating the gnolls here while we aid the wounded and figure out our next move.',
'Slay 10 gnolls within Hammerfall.',
5350, 1)
ON DUPLICATE KEY UPDATE `LogTitle`=VALUES(`LogTitle`), `LogDescription`=VALUES(`LogDescription`), `QuestDescription`=VALUES(`QuestDescription`), `RewardBonusMoney`=VALUES(`RewardBonusMoney`), `RewardXPDifficulty`=VALUES(`RewardXPDifficulty`);

-- ---- 90883 "To Go'shek Farm" -- text + reward delta (blank/unreliable WDB row) ----
-- Reward fields authored explicitly (fix round 1) rather than trusted from this quest's
-- own blank-text WDB row: plan Part 1.1 tier "t1" = money 5350 copper, RewardXPDifficulty 1.
INSERT INTO `quest_template` (`ID`, `LogTitle`, `LogDescription`, `QuestDescription`, `RewardBonusMoney`, `RewardXPDifficulty`) VALUES
 (90883, 'To Go''shek Farm',
'We''ve received word a nearby farm is under attack by ogres and kobolds.

We have to move before more lives are lost.',
'Travel to Go''shek Farm',
5350, 1)
ON DUPLICATE KEY UPDATE `LogTitle`=VALUES(`LogTitle`), `LogDescription`=VALUES(`LogDescription`), `QuestDescription`=VALUES(`QuestDescription`), `RewardBonusMoney`=VALUES(`RewardBonusMoney`), `RewardXPDifficulty`=VALUES(`RewardXPDifficulty`);

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
