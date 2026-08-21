-- ============================================================================
-- ARATHI CATCH-UP / RPE CONSOLIDATION -- QUESTS (issue 6)
-- ============================================================================
-- Branch: feature/arathi-rpe   Path: sql/updates/world/master/   Server mapID: 2927
-- Consolidated verbatim from the authoritative content slices (guid block 8000000);
-- runs after 2026_08_21_00 cleanup. Each source slice keeps its own banner + idempotency.
-- quest_template -> addon(chain) -> objectives -> questgiver links -> Horde variant -> POI -> offer_reward -> package_item.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 30_quest_template.sql  <<<<<<<<<<<<<<<<<<<<

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

-- ---- 8 gear-reward quests -- QuestPackageID delta (class-adaptive rewards) ----
-- Wires each gear-reward quest to its minted QuestPackage (see 36_quest_package_item.sql,
-- PackageID = 64000 + questID%1000). This is the class-adaptive reward fix (issue 4b): the
-- retail RPE chain serves class-appropriate gear via QuestPackageItem.db2, NOT via the static
-- quest_template.RewardChoiceItemID1-6 columns (which BuildQuestRewards sends to every class
-- unfiltered). With QuestPackageID set + the package rows loaded, TC's CanSelectQuestPackageItem
-- filters each row by the item's own class-spec mask (DisplayType=1 CLASS), so each player is
-- offered only their class's set-family items. 35_quest_offer_reward.sql drops the old static
-- RewardChoiceItemID rows for these same 8 quests (they must NOT coexist with the package).
-- Excluded on purpose: 90883 (4 class-independent bags -- keeps its RewardChoiceItemID),
-- 90897/90911 (no gear reward). Partial-column UPDATE -> idempotent, touches nothing else.
INSERT INTO `quest_template` (`ID`, `QuestPackageID`) VALUES
 (90882, 64882),
 (90885, 64885),
 (90886, 64886),
 (90887, 64887),
 (90888, 64888),
 (90893, 64893),
 (90895, 64895),
 (90896, 64896)
ON DUPLICATE KEY UPDATE `QuestPackageID`=VALUES(`QuestPackageID`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 31_quest_template_addon.sql  <<<<<<<<<<<<<<<<<<<<

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


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 32_quest_objectives.sql  <<<<<<<<<<<<<<<<<<<<

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


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 33_creature_quest_links.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E questgiver/turn-in links
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/quest_structured_candidates.sql
--   (hard-confirmed: 244643->90882 both queststarter and questender, [C hard]), plan
--   Part 1.1 Giver/Ender columns for the remaining 10 quests (no further oracle rows were
--   captured for the rest of the chain -- authored from the plan table, cross-checked
--   against Task 1's 10_creature_template.sql to confirm every entry below exists).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (`id`,`quest`)).
-- ============================================================================

-- ---- creature_queststarter (11 Alliance rows, one per quest's Giver column) ----
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
 (244643, 90882),  -- Jaina (Hammerfall) -- [C hard], matches quest_structured_candidates.sql
 (244643, 90883),  -- Jaina (Hammerfall)
 (244729, 90885),  -- Farmer Bruvk (Go'shek)
 (244656, 90886),  -- Thrall (farm)
 (244655, 90887),  -- Jaina (farm)
 (244655, 90888),  -- Jaina (farm)
 (244657, 90893),  -- Thrall (siege entry)
 (244658, 90895),  -- Jaina (siege)
 (244666, 90896),  -- Thrall (siege climax)
 (244667, 90897),  -- Jaina (siege climax)
 (244714, 90911)   -- Jaina (Stromgarde Keep hub)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

-- ---- creature_questender (11 Alliance rows, one per quest's Ender column) ----
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
 (244643, 90882),  -- Jaina (Hammerfall) -- [C hard], matches quest_structured_candidates.sql
 (244729, 90883),  -- Farmer Bruvk (Go'shek)
 (244656, 90885),  -- Thrall (farm)
 (244656, 90886),  -- Thrall (farm)
 (244655, 90887),  -- Jaina (farm)
 (244657, 90888),  -- Thrall (siege entry)
 (244666, 90893),  -- Thrall (siege climax)
 (244667, 90895),  -- Jaina (siege climax)
 (244666, 90896),  -- Thrall (siege climax)
 (244714, 90897),  -- Jaina (Stromgarde Keep hub)
 (244714, 90911)   -- Jaina (Stromgarde Keep hub)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

-- ============================================================================
-- HORDE-XVAL FIX (H3, task-2) -- missing Horde questgivers
-- ============================================================================
-- The mid-chain givers (90885-90896, above) are SHARED between factions -- correct as
-- authored, left untouched. But the 3 faction-specific seams only had their Alliance side
-- wired: Horde players had NO creature offering/accepting 90882, 90883, or 90911 at all
-- (both 90882 and 90883 only listed Jaina 244643; 90911 only listed Jaina 244714) -- a
-- Horde character could not start or progress this chain. Fixed by mirroring the Alliance
-- rows onto their already-authored Horde counterparts (Task 1 10_creature_template.sql /
-- H2's 244715 addition):
--   * 244642 Thrall = Horde mirror of Alliance greeter 244643 Jaina (Hammerfall opening) --
--     both quest_queststarter for 90882/90883, and quest_questender for 90882 (90883's
--     ender stays the SHARED 244729 Farmer Bruvk, untouched, per Requirement 2).
--   * 244715 Thrall = Horde mirror of Alliance hub 244714 Jaina (90911 "Your Next
--     Adventure" gossip hub) -- both queststarter and questender, matching addon-captured
--     addon_creature_questender row 244715->90911 [C].
-- ---- creature_queststarter (Horde) ----
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
 (244642, 90882),  -- Thrall (Hammerfall) -- Horde mirror of Alliance 244643 Jaina
 (244642, 90883),  -- Thrall (Hammerfall) -- Horde mirror of Alliance 244643 Jaina
 (244715, 90911)   -- Thrall (Horde hub) -- Horde mirror of Alliance 244714 Jaina; [C] addon_creature_questender 244715->90911
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

-- ---- creature_questender (Horde) ----
-- 90882 turn-in via 244642 Thrall (mirror of Alliance 244643 Jaina). 90883's ender stays
-- the SHARED 244729 Farmer Bruvk (already correct above, not duplicated here).
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
 (244642, 90882),  -- Thrall (Hammerfall) -- Horde mirror of Alliance 244643 Jaina, 90882 turn-in
 (244715, 90911)   -- Thrall (Horde hub) -- Horde mirror of Alliance 244714 Jaina; [C] addon_creature_questender 244715->90911
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 70_horde_variant.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Horde finale branch
-- (quest 90898 "Back to Hammerfall")
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
--
-- STATUS (Horde-xval H3, task-3): CAPTURE-SOURCED. The original authoring of this file was
-- a documented gap stub -- the Horde run of the "Siege of Arathi Highlands" chain had not
-- yet been captured, and section 1's quest_template row below was an [INFERRED]-by-analogy
-- placeholder. A Horde cross-validation capture has since recorded the real 90898 data
-- (LogDescription flavor text, QuestDescription, RewardBonusMoney, RewardText, and the real
-- giver/ender creatures) -- section 1 and the new sections 1b/1c below now author that
-- captured data directly, replacing the placeholder text and the two inert
-- creature_queststarter/questender comment blocks from the original stub. Section 2
-- (quest_template_addon branch wiring + the 90911 PrevQuestID fix) was already correct and
-- is unchanged. See the trimmed "PHASE-K HORDE-CAPTURE GAPS" banner at the bottom for what
-- STILL remains uncaptured (objectives, phasing, POI, gossip beyond the 244715 hub picker).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
--
-- DEPENDS ON (read-only): 30_quest_template.sql / 31_quest_template_addon.sql /
-- 33_creature_quest_links.sql (Task 3, Alliance chain, owns 90882-90897/90911's rows --
-- NOT re-inserted or altered here except the single idempotent partial-column UPDATE to
-- 90911's PrevQuestID documented in section 2, which is a bug-fix completion of Task 3's
-- own single-branch-only wiring, not a change to anything Task 3 actually authored for
-- 90897). Must run AFTER 31_quest_template_addon.sql (file-order 70_ > 31_ already
-- guarantees this) since section 2's UPDATE assumes the 90911 row already exists. Also
-- depends on 33_creature_quest_links.sql's Horde-xval H3 task-2 addition of 244715's
-- gossip-hub wiring (61_gossip.sql task-5) for the 90911 hand-off to actually be reachable.
-- ============================================================================


-- ============================================================================
-- 1) quest_template (90898 "Back to Hammerfall") -- CAPTURED Horde data
-- ============================================================================
-- Horde-xval H3 task-3: replaces the original [INFERRED]-by-analogy stub row with the
-- real captured LogDescription/QuestDescription text. LogTitle 'Back to Hammerfall' was
-- already retail-confirmed (Wowhead) and is unchanged.
--
-- LogDescription is now capture-sourced (was previously OMITTED entirely -- no analogous
-- short-form text existed to mirror, and inventing multi-sentence Thrall dialogue would
-- have crossed from "inferred by analogy" into "fabricated as real"; that concern is now
-- moot, this is the captured line).
--
-- QuestDescription is now the captured short form "Return to Hammerfall." (previously an
-- [INFERRED] "Meet Thrall within Hammerfall" placeholder mirrored off 90897's own
-- QuestDescription -- superseded).
--
-- RewardBonusMoney=5350 / RewardXPDifficulty=1 -- tier 1, matches this chain's other
-- terminus-adjacent quests (unchanged from the original analogy-based value; capture
-- confirms it).
--
-- AllowableRaces = RACEMASK_HORDE, computed from THIS worktree's
-- src/server/game/Miscellaneous/RaceMask.h (RACEMASK_HORDE_v = RACEMASK_ALL_PLAYABLE_v
-- & ~(RACEMASK_NEUTRAL_v | RACEMASK_ALLIANCE_v), RaceMask.h:290), same method Task 3
-- used for 90897's RACEMASK_ALLIANCE=2973060173 (30_quest_template.sql:78-87, sanity-
-- checked by recomputing it here too and getting the identical value). Bit set (raceId,
-- GetRaceBit() per RaceMask.h:97-147): {1 Orc, 4 Undead, 5 Tauren, 7 Troll, 8 Goblin,
-- 9 BloodElf, 12 Vulpera, 13 Mag'har Orc, 15 Dracthyr(Horde), 17 Earthen(Horde),
-- 19 Haranir(Horde), 25 Pandaren(Horde), 26 Nightborne, 27 Highmountain Tauren,
-- 30 Zandalari Troll}. Sum of 2^bit = 1309324210. Unchanged from original authoring
-- (this value was always an engine-level computation, not a capture-dependent field).
INSERT INTO `quest_template` (`ID`, `LogTitle`, `LogDescription`, `QuestDescription`, `RewardBonusMoney`, `RewardXPDifficulty`, `AllowableRaces`) VALUES
 (90898, 'Back to Hammerfall',
'We''re not always so lucky that a plan such as this is ended so swiftly and without a surprise portal or two.

That being said, more dangers lurk throughout Azeroth. I know you can be of help to some.

Let us return to Hammerfall to see how they''re recovering.',
'Return to Hammerfall.',
5350, 1,
1309324210) -- RACEMASK_HORDE, see banner above
ON DUPLICATE KEY UPDATE `LogTitle`=VALUES(`LogTitle`), `LogDescription`=VALUES(`LogDescription`), `QuestDescription`=VALUES(`QuestDescription`), `RewardBonusMoney`=VALUES(`RewardBonusMoney`), `RewardXPDifficulty`=VALUES(`RewardXPDifficulty`), `AllowableRaces`=VALUES(`AllowableRaces`);

-- ============================================================================
-- 1b) quest_offer_reward (90898) -- CAPTURED RewardText
-- ============================================================================
-- Real turn-in flavor text, same table/pattern as the Alliance chain's
-- 35_quest_offer_reward.sql Section 1 (that file intentionally does not cover 90898/90911
-- -- Horde-branch rows belong here instead).
INSERT INTO `quest_offer_reward` (`ID`, `RewardText`) VALUES
 (90898, 'Hammerfall will recover in time and become stronger than it was before.')
ON DUPLICATE KEY UPDATE `RewardText`=VALUES(`RewardText`);


-- ============================================================================
-- 2) quest_template_addon -- branch wiring for 90898, plus a required fix to
--    90911's PrevQuestID so the shared terminus accepts EITHER faction finale
-- ============================================================================
--
-- ---- 2a) 90898 itself: PrevQuestID=90896 (shared final Alliance/Horde ancestor,
-- "One Last Ogre"), NextQuestID=90911 ("Your Next Adventure"), ExclusiveGroup=0. ----
-- Mirrors 90897's own row exactly (31_quest_template_addon.sql:47-48:
-- `(90896, 90895, 90897, 0)` / `(90897, 90896, 90911, 0)`).
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`) VALUES
 (90898, 90896, 90911, 0)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`), `NextQuestID`=VALUES(`NextQuestID`), `ExclusiveGroup`=VALUES(`ExclusiveGroup`);

-- ---- 2b) DECISION: AllowableRaces-only gating between 90897/90898, NOT a shared
-- ExclusiveGroup. Reasoning, verified by reading this worktree's own engine source
-- (not assumed): ----
--
-- The brief's Requirement 2 offered two options: "shared ExclusiveGroup (negative) OR
-- rely on AllowableRaces to gate them per faction". A negative shared ExclusiveGroup is
-- actively WRONG for this exact scenario and would have broken 90911 for BOTH factions
-- if used -- traced via:
--   * ObjectMgr.cpp:5292-5299 -- at load time, for EVERY quest with a nonzero
--     NextQuestID, the engine automatically appends that quest's OWN id onto
--     `<NextQuestID target>.DependentPreviousQuests`. Since both 90897.NextQuestID=90911
--     (Task 3) and 90898.NextQuestID=90911 (section 2a above) are set, 90911's
--     DependentPreviousQuests is auto-populated at runtime as [90897, 90898] -- NO SQL
--     row encodes this list; it is purely reverse-derived from NextQuestID. This is the
--     literal TC mechanism for "multiple predecessor quests funnel into one".
--   * Player.cpp:15472-15476 (SatisfyQuestDependentQuests) requires BOTH
--     SatisfyQuestPreviousQuest (the single scalar PrevQuestID field -- Player.cpp:
--     15478-15503) AND SatisfyQuestDependentPreviousQuests (the auto-derived list above
--     -- Player.cpp:15505-15546) to pass.
--   * SatisfyQuestDependentPreviousQuests (Player.cpp:15517-15522): for each entry in
--     the auto-derived list, "if IsQuestRewarded(prevId) return true" UNCONDITIONALLY
--     as long as `questInfo->GetExclusiveGroup() >= 0` for that specific prev quest --
--     i.e. plain OR-semantics (any ONE of 90897/90898 rewarded suffices) is the DEFAULT
--     behavior precisely because both already carry ExclusiveGroup=0 (non-negative).
--   * If either 90897 or 90898 carried a NEGATIVE ExclusiveGroup instead (the brief's
--     first option), Player.cpp:15524-15546 flips to "each-from-all" semantics: it would
--     then require EVERY quest in that exclusive group to ALSO be rewarded before 90911
--     unlocks -- meaning a Horde player would need 90897 (Alliance-only, structurally
--     impossible for them to ever complete) rewarded too. That would permanently lock
--     90911 for both factions. CONFIRMED WRONG for this case; not used.
--
-- Therefore: 90897 and 90898 are left with ExclusiveGroup=0 (already true for 90897,
-- Task 3's original value; set explicitly to 0 for 90898 in 2a above), and the
-- faction split is enforced ENTIRELY by AllowableRaces (section 1's Horde mask here;
-- 90897's Alliance mask in 30_quest_template.sql) -- a player only ever sees the one
-- quest matching their own race, so the auto-derived OR-list on 90911 never actually
-- offers a player the "wrong" branch; it only needs to accept whichever ONE the player
-- legitimately completed.
--
-- ---- 2c) REQUIRED FIX: 90911.PrevQuestID 90897 -> 0 (idempotent partial-column UPDATE,
-- does not touch NextQuestID/ExclusiveGroup or any other 90911 column). ----
-- Task 3's original 90911 row (31_quest_template_addon.sql:49: `(90911, 90897, 0, 0)`)
-- hardcoded PrevQuestID=90897 -- correct for an Alliance-only chain at the time it was
-- authored, but SatisfyQuestPreviousQuest (Player.cpp:15478-15503) checks PrevQuestID as
-- a SINGLE scalar id; it has no OR capability of its own. Left at 90897, a Horde player
-- who rewards 90898 would satisfy the auto-derived SatisfyQuestDependentPreviousQuests
-- check (section 2b) but FAIL SatisfyQuestPreviousQuest outright (90897 never rewarded,
-- can't be, wrong race) -- and SatisfyQuestDependentQuests requires BOTH to pass
-- (Player.cpp:15474, logical AND). Net effect: 90911 would stay permanently locked for
-- every Horde character. Setting PrevQuestID=0 makes SatisfyQuestPreviousQuest a no-op
-- (Player.cpp:15481-15482, `if (!qInfo->GetPrevQuestId()) return true;`), leaving
-- SatisfyQuestDependentPreviousQuests (90897 OR 90898 rewarded) as 90911's sole and
-- CORRECT gate for both factions. This is the "PrevQuestID handling for a faction-split
-- predecessor" the brief's Requirement 2 asks to be resolved and commented.
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`) VALUES
 (90911, 0)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`);


-- ============================================================================
-- 3) creature_questender / creature_queststarter for 90898 -- CAPTURED, now live
-- ============================================================================
-- The Horde questgiver/ender entries are now capture-sourced (previously UNKNOWN --
-- left as inert SQL comments per Requirement 3's original authoring). Both creature
-- entries already exist as real creature_template rows in this feature's Task 1
-- (10_creature_template.sql): 244666 Thrall (siege climax, already wired as 90896's
-- giver/90893's ender in 33_creature_quest_links.sql) and 244715 Thrall (the Horde
-- finale/hub NPC, added by H2, npcflag=3 gossip+questgiver) -- so unlike the original
-- stub, entry 0 is no longer a concern; both target creatures are real and confirmed.
--
-- Giver: 244666 Thrall (siege climax) -- mirrors Alliance 244667 Jaina -> 90897 (same
-- narrative beat, siege climax handing off to the "return home" quest).
-- Ender: 244715 Thrall (Hammerfall hub) -- mirrors Alliance 244714 Jaina (Stromgarde Keep
-- hub) -- same NPC that also starts/ends 90911 (Task 2 of this Horde-xval fix,
-- 33_creature_quest_links.sql).
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
 (244666, 90898)  -- Thrall (siege climax) -- mirror of Alliance 244667 Jaina -> 90897
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

INSERT INTO `creature_questender` (`id`, `quest`) VALUES
 (244715, 90898)  -- Thrall (Hammerfall hub) -- mirror of Alliance 244714 Jaina
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);
--
-- The 90911 hub's OWN giver/ender for Horde players (244715 Thrall) is now also wired --
-- see Task 2 of this Horde-xval fix (33_creature_quest_links.sql) -- so a Horde player who
-- rewards 90898 now has a creature (244715) that both accepts 90898's turn-in and
-- offers/accepts 90911 immediately after. That gap is CLOSED as of this task.


-- ============================================================================
-- 4) PHASE-K HORDE-CAPTURE GAPS -- explicitly NOT authored in this task
-- ============================================================================
-- Everything below requires a real Horde playthrough capture (addon dump / sniff /
-- WDB cache) of the "Siege of Arathi Highlands" Horde path before it can be authored as
-- candidate SQL. Updated by Horde-xval H3 task-3 -- several items below are now RESOLVED
-- (struck through) since the 90898 giver/ender entries and text are now capture-sourced
-- (section 1/1b/3 above); what remains is still genuinely uncaptured, not guessed:
--   * [RESOLVED by H3] Horde questgiver/ender creature_template entries -- 244666 (giver)
--     and 244715 (ender/hub) both exist and are now wired (section 3 above); no new
--     creature_template rows were needed, both were already authored (244666 by Task 1,
--     244715 by H2).
--   * Hammerfall-side creature spawns for the Horde mirror of this chain's encounters
--     (90882's gnoll fight already happens at Hammerfall in the SHARED opening -- only
--     the LATER, faction-diverging Stromgarde-vs-Hammerfall content is unconfirmed).
--   * Horde-side PhaseIds/phase_area rows analogous to Task 2's 15901-15905 (Alliance
--     narrative phasing) -- whether the Horde finale phases the zone at all, and how.
--   * [RESOLVED by H3] Dialogue text: 90898's LogDescription and QuestDescription are now
--     capture-sourced (section 1 above). Any Conversation/creature_text rows for the Horde
--     finale beyond the quest_template text itself remain uncaptured.
--   * quest_objectives row(s) for 90898 -- STILL out of scope (this task, H3, only fixes
--     Task 4's 90883/90911 per the brief) -- but now that the real ender 244715 exists
--     (section 3 above), a future pass CAN mirror 90897's own objective pattern
--     (32_quest_objectives.sql:166-172, Type=3 TALKTO) as target 244715 Thrall instead of
--     244714 Jaina. Do not reuse 244714 for a Horde objective.
--   * quest_poi / quest_poi_points for 90898 (Hammerfall-side map markers).
--   * Any Horde-specific gossip_menu / npc_text content at the Hammerfall hub itself
--     (distinct from the 244715 "next adventure" picker menu, which IS now wired --
--     see 61_gossip.sql Horde-xval H3 task-5 -- this item is about a possible SEPARATE
--     Hammerfall-hub-flavor gossip greeting, not the picker).
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 34_quest_poi.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest_poi (worldmap pins)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)   client uiMapID **2451** (Arathi Highlands worldmap tile -- used
--   here, unlike elsewhere in this slice where 2451 is display-only).
-- Source: plan Part 1.7 Arathi POI list (area_poi.sql, VerifiedBuild 69299) + Part 1.3
--   objective-cluster coordinates. quest_poi.sql from the harvest bundle has exactly ONE
--   908xx row (QuestID=90882, MapID=2927, UiMapID=2451) -- MapID 2927 MATCHES this
--   zone's real server map 2927 (CONFIRMED from wire), so it is VALID (the earlier
--   'noise' dismissal was the 2796 map error); MapID corrected to 2927 below;
--   every row below is authored fresh from the POI/cluster coordinates per Requirement 5.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is
--   (QuestID,BlobIndex,Idx1)). One pin (BlobIndex=1,Idx1=1) per quest, matching the
--   observed convention for simple single-objective quests elsewhere in the oracle data
--   (e.g. QuestID 34586/41141 in the captured quest_poi.sql).
-- Coordinates authored from plan clusters -- TODO Phase K: refine against a real capture.
-- ============================================================================

INSERT INTO `quest_poi` (`QuestID`, `BlobIndex`, `Idx1`, `ObjectiveIndex`, `QuestObjectiveID`, `QuestObjectID`, `MapID`, `UiMapID`, `Priority`, `Flags`, `WorldEffectID`, `PlayerConditionID`, `NavigationPlayerConditionID`, `SpawnTrackingID`, `AlwaysAllowMergingBlobs`) VALUES
 (90882, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Hammerfall (Part 1.7): 5 gnoll entries, multi-target
 (90883, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek/Dabyrie's Farmstead (Part 1.7): travel destination
 (90885, 1, 1, 0, 0, 244956, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek farm: Prized Pumpkin
 (90886, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek farm: item drops off farm mobs
 (90887, 1, 1, 0, 0, 244675, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek farm: Runk
 (90888, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Stromgarde Keep (Part 1.7): travel destination
 (90893, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Stromgarde siege battlefield (Part 1.3): multi-target progress bar
 (90895, 1, 1, 0, 0, 249269, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Catapults cluster (Part 1.3)
 (90896, 1, 1, 0, 0, 244709, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- siege battlefield: Ro'grok (former Horde encampment)
 (90897, 1, 1, 0, 0, 244714, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Stromgarde battlefield: Jaina @ Part 1.3 cluster coords
 (90911, 1, 1, 0, 0, 244714, 2927, 2451, 0, 0, 0, 0, 0, 0, 0)   -- Stromgarde battlefield: Jaina hub (same spot as 90897)
ON DUPLICATE KEY UPDATE `ObjectiveIndex`=VALUES(`ObjectiveIndex`), `QuestObjectiveID`=VALUES(`QuestObjectiveID`), `QuestObjectID`=VALUES(`QuestObjectID`), `MapID`=VALUES(`MapID`), `UiMapID`=VALUES(`UiMapID`), `Priority`=VALUES(`Priority`), `Flags`=VALUES(`Flags`), `WorldEffectID`=VALUES(`WorldEffectID`), `PlayerConditionID`=VALUES(`PlayerConditionID`), `NavigationPlayerConditionID`=VALUES(`NavigationPlayerConditionID`), `SpawnTrackingID`=VALUES(`SpawnTrackingID`), `AlwaysAllowMergingBlobs`=VALUES(`AlwaysAllowMergingBlobs`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 34b_quest_poi_points.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest_poi_points (pin coords)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)   client uiMapID 2451 (see 34_quest_poi.sql banner for source/caveats
--   -- these points back the pins authored there, one (Idx1=1, Idx2=0) point per quest).
-- X/Y/Z are integer world coordinates (schema: `X`/`Y`/`Z` int, matching the captured
--   oracle rows' convention of plain truncated-int world coords, NOT a UiMap 0-100 scale).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (QuestID,Idx1,Idx2)).
-- ============================================================================
--
-- Coordinate sources (plan Part 1.7 POIs + Part 1.3 objective clusters; Z authored from
-- Part 1.7 where given, else approximated from the nearest sourced Z -- flagged inline):
--   Hammerfall                    (-991.6, -3528.3,  56.6)  Part 1.7
--   Go'shek/Dabyrie's Farmstead   (-1090.3, -2843.2, 42.2)  Part 1.7
--   Stromgarde Keep               (-1675.4, -1802.7, 80.0)  Part 1.7
--   Catapults cluster             (-1874, -1339, [Z not captured -- approximated 80 from
--                                  the nearest sourced Z, Stromgarde Keep])  Part 1.3
--   Jaina @ siege battlefield     (-1812, -1568, [Z not captured -- approximated 80])  Part 1.3

INSERT INTO `quest_poi_points` (`QuestID`, `Idx1`, `Idx2`, `X`, `Y`, `Z`) VALUES
 (90882, 1, 0, -992,  -3528, 57),  -- Hammerfall
 (90883, 1, 0, -1090, -2843, 42),  -- Go'shek/Dabyrie's Farmstead
 (90885, 1, 0, -1090, -2843, 42),  -- Go'shek farm
 (90886, 1, 0, -1090, -2843, 42),  -- Go'shek farm
 (90887, 1, 0, -1090, -2843, 42),  -- Go'shek farm
 (90888, 1, 0, -1675, -1803, 80),  -- Stromgarde Keep
 (90893, 1, 0, -1675, -1803, 80),  -- Stromgarde siege battlefield (Keep coords reused, no finer capture)
 (90895, 1, 0, -1874, -1339, 80),  -- Catapults cluster (Z approximated)
 (90896, 1, 0, -1675, -1803, 80),  -- siege battlefield (Keep coords reused, no finer capture)
 (90897, 1, 0, -1812, -1568, 80),  -- Jaina @ siege battlefield cluster (Z approximated)
 (90911, 1, 0, -1812, -1568, 80)   -- Jaina hub, same spot as 90897
ON DUPLICATE KEY UPDATE `X`=VALUES(`X`), `Y`=VALUES(`Y`), `Z`=VALUES(`Z`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 35_quest_offer_reward.sql  <<<<<<<<<<<<<<<<<<<<

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


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 36_quest_package_item.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: class-adaptive quest rewards (QuestPackage)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Target table: HOTFIX DB `quest_package_item` (QuestPackageItem.db2 mirror).
--   Schema (this fork, sql/base/dev/hotfixes_database.sql:7800 + HotfixDatabase.cpp:1455):
--     quest_package_item (ID, PackageID, ItemID, ItemQuantity, DisplayType, VerifiedBuild)
--     PRIMARY KEY (ID, VerifiedBuild); PackageID = smallint unsigned (MAX 65535).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent: DELETE-by-PackageID before the INSERTs (no natural unique key beyond ID).
-- ============================================================================
--
-- ---- WHY THIS FILE EXISTS (the bug fix) ----
-- The RPE reward quests award CLASS-APPROPRIATE gear: each single quest's choice set spans
-- all 13 classes (e.g. quest 90882 = 29 distinct weapon items). That cannot fit the 6 static
-- `quest_template.RewardChoiceItemID1-6` columns, and TrinityCore's BuildQuestRewards() sends
-- those 6 columns to EVERY player unfiltered (no class filtering on RewardChoiceItem). Our
-- prior authoring (35_quest_offer_reward.sql Section 2) put ONE class's items (the Alliance
-- Shaman capture set) into those columns -> every player, every class, was offered Shaman mail.
--
-- Retail serves these via the QuestPackage mechanism, which TrinityCore implements fully with
-- NO core change: quest_template.QuestPackageID -> QuestPackageItem.db2 rows, filtered per
-- player. Server: Player::CanSelectQuestPackageItem (DisplayType=1 CLASS -> ItemSpecClassMask
-- & GetClassMask()); client: renders only rows whose DisplayType filter matches the player.
-- This file populates that package table; 30_quest_template.sql sets QuestPackageID; and
-- 35_quest_offer_reward.sql drops the static RewardChoiceItemID rows (they must NOT coexist).
-- Full analysis: .superpowers/sdd/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN/phk-class-rewards-report.md
--
-- ---- MINTED PackageID SCHEME (custom, fully self-contained -- no client DB2 extraction) ----
-- Real retail QuestPackageIDs are unknown (not captured; not in our db2_cache dump; TCHarvest
-- dropped the column), so we MINT custom package IDs and push them to the client via
-- VerifiedBuild=0 (a 0-build hotfix row is client-applied for display). Scheme:
--     PackageID = 64000 + (questID mod 1000)   -> 64882/64885/64886/64887/64888/64893/64895/64896
-- All <= 64896 < 65535 (PackageID is smallint unsigned). The 64xxx range sits far above any
-- real retail QuestPackageID (retail package space is well under ~10000), so mint collision is
-- implausible; and this fork's base hotfix DB ships ZERO quest_package_item rows (verified:
-- no INSERT INTO quest_package_item in hotfixes_database.sql), so there is nothing to collide
-- with locally. Row ID scheme (must be unique per VerifiedBuild):
--     ID = PackageID*100 + seq   (seq = 1..N within the package; N max 29 << 100)
--   -> 6488201.. / 6488501.. / ... (int unsigned; globally unique).
--
-- ---- DisplayType = 1 (CLASS) on every row ----
-- TC's CanSelectQuestPackageItem then filters each row by the ITEM's own class-spec mask
-- (ItemSpecClassMask & player class mask). So we author EVERY class's item for the quest into
-- one package and let the core show each player only their class's family. ItemQuantity=1.
--
-- ---- DATA SOURCING & VERIFICATION ----
-- Per-quest per-class item ids read from each quest's Wowhead reward-choice list
-- (wowhead.com/quest=<id>), which tags every item by its set-family name. Family->class map
-- (phk report Section 3.1): Oathsworn=Warrior, Sunsoul=Paladin, Heart-Lesion=DeathKnight,
-- Trailseeker=Hunter, Streamtalker=Shaman, Blue/Cobalt Winglord's=Evoker, Lightdrinker=Rogue,
-- Mistdancer=Monk, Springrain=Druid, Illidari=DemonHunter, Communal=Priest,
-- Mountainsage=Mage, Felsoul=Warlock. Shaman(Streamtalker) + Druid(Springrain) ids are
-- CAPTURE-CONFIRMED (Alliance + Horde play-session captures) and match Wowhead exactly.
-- ALL 211 item ids below were verified present in ItemSparse.12.0.7.68275.csv (0 missing).
-- Family->class spot-verified against ItemSparse.AllowableClass: 154025 Oathsworn=1(Warrior),
-- 153889 Sunsoul=2(Paladin), 153726 Heart-Lesion=32(DK), 194522 Blue Winglord's=4096(Evoker),
-- 153934 Communal=16(Priest), 153830 Mountainsage=128(Mage), 154024 Felsoul=256(Warlock).
--
-- ---- KNOWN CAVEATS / TODO-VARIANT ----
-- * 90882 Hunter: Wowhead's live page lists Trailseeker Spear 153814 + Longbow 231839; we ALSO
--   include Trailseeker Shotgun 153813 (present in ItemSparse and cited in phk report Section 3.3)
--   as a plausible Trailseeker weapon variant. TODO variant: confirm 153813 belongs on 90882.
-- * 90887 Wowhead lists BACK cloaks (capture: 153793/153998) AND chest pieces per plate/leather
--   family. Capture only surfaced the cloak for the captured toon; the package legitimately
--   spans back + chest, so all are included (each class still sees only its own family). TODO
--   variant: a loot-spec (DisplayType=0) split may narrow which piece a given spec is offered --
--   not modelled here (task specifies DisplayType=1 CLASS).
-- * 90888 DemonHunter trinkets = Demon Trophy 154743 / Charm of Demonic Fire 154744 (Illidari
--   trinket equivalents); Evoker = Claw-Carved Figurine 194531 / Blue Winglord's Insignia 194532.
-- * Illidari (DH) items carry ItemSparse.AllowableClass=-1 (all classes) but their ItemSpec
--   class mask restricts to DH, which is what CanSelectQuestPackageItem's CLASS filter uses --
--   so they still surface only to Demon Hunters. Flagged for Phase-K in-game confirmation.
-- ============================================================================

DELETE FROM `quest_package_item` WHERE `PackageID` IN (64882, 64885, 64886, 64887, 64888, 64893, 64895, 64896);

-- ---- Quest 90882 -> PackageID 64882 :: Weapons (1H/2H/shield) (29 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488201, 64882, 154025, 1, 1, 0),  -- Warrior / Oathsworn
 (6488202, 64882, 154035, 1, 1, 0),  -- Warrior / Oathsworn
 (6488203, 64882, 154036, 1, 1, 0),  -- Warrior / Oathsworn
 (6488204, 64882, 153889, 1, 1, 0),  -- Paladin / Sunsoul
 (6488205, 64882, 153891, 1, 1, 0),  -- Paladin / Sunsoul
 (6488206, 64882, 153892, 1, 1, 0),  -- Paladin / Sunsoul
 (6488207, 64882, 153893, 1, 1, 0),  -- Paladin / Sunsoul
 (6488208, 64882, 153726, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488209, 64882, 153747, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488210, 64882, 153814, 1, 1, 0),  -- Hunter / Trailseeker
 (6488211, 64882, 153813, 1, 1, 0),  -- Hunter / Trailseeker (TODO variant: Shotgun, not on live 90882 page)
 (6488212, 64882, 231839, 1, 1, 0),  -- Hunter / Trailseeker
 (6488213, 64882, 153959, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488214, 64882, 153960, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488215, 64882, 153961, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488216, 64882, 153973, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488217, 64882, 153983, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488218, 64882, 154005, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488219, 64882, 153830, 1, 1, 0),  -- Mage / Mountainsage
 (6488220, 64882, 154024, 1, 1, 0),  -- Warlock / Felsoul
 (6488221, 64882, 153934, 1, 1, 0),  -- Priest / Communal
 (6488222, 64882, 153944, 1, 1, 0),  -- Priest / Communal
 (6488223, 64882, 153835, 1, 1, 0),  -- Monk / Mistdancer
 (6488224, 64882, 153856, 1, 1, 0),  -- Monk / Mistdancer
 (6488225, 64882, 153859, 1, 1, 0),  -- Monk / Mistdancer
 (6488226, 64882, 153773, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488227, 64882, 153792, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488228, 64882, 160513, 1, 1, 0),  -- DemonHunter / Illidari
 (6488229, 64882, 194522, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90885 -> PackageID 64885 :: Rings x2 (Finger) (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488501, 64885, 153741, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488502, 64885, 153742, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488503, 64885, 153796, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488504, 64885, 153797, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488505, 64885, 153802, 1, 1, 0),  -- Hunter / Trailseeker
 (6488506, 64885, 153803, 1, 1, 0),  -- Hunter / Trailseeker
 (6488507, 64885, 153817, 1, 1, 0),  -- Mage / Mountainsage
 (6488508, 64885, 153818, 1, 1, 0),  -- Mage / Mountainsage
 (6488509, 64885, 153862, 1, 1, 0),  -- Monk / Mistdancer
 (6488510, 64885, 153863, 1, 1, 0),  -- Monk / Mistdancer
 (6488511, 64885, 153908, 1, 1, 0),  -- Paladin / Sunsoul
 (6488512, 64885, 153909, 1, 1, 0),  -- Paladin / Sunsoul
 (6488513, 64885, 153927, 1, 1, 0),  -- Priest / Communal
 (6488514, 64885, 153928, 1, 1, 0),  -- Priest / Communal
 (6488515, 64885, 153948, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488516, 64885, 153949, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488517, 64885, 153995, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488518, 64885, 153996, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488519, 64885, 154011, 1, 1, 0),  -- Warlock / Felsoul
 (6488520, 64885, 154012, 1, 1, 0),  -- Warlock / Felsoul
 (6488521, 64885, 154114, 1, 1, 0),  -- Warrior / Oathsworn
 (6488522, 64885, 154115, 1, 1, 0),  -- Warrior / Oathsworn
 (6488523, 64885, 154745, 1, 1, 0),  -- DemonHunter / Illidari
 (6488524, 64885, 154746, 1, 1, 0),  -- DemonHunter / Illidari
 (6488525, 64885, 194533, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6488526, 64885, 194534, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90886 -> PackageID 64886 :: Feet + Hands (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488601, 64886, 153735, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488602, 64886, 153736, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488603, 64886, 153785, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488604, 64886, 153786, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488605, 64886, 153806, 1, 1, 0),  -- Hunter / Trailseeker
 (6488606, 64886, 153807, 1, 1, 0),  -- Hunter / Trailseeker
 (6488607, 64886, 153820, 1, 1, 0),  -- Mage / Mountainsage
 (6488608, 64886, 153821, 1, 1, 0),  -- Mage / Mountainsage
 (6488609, 64886, 153845, 1, 1, 0),  -- Monk / Mistdancer
 (6488610, 64886, 153846, 1, 1, 0),  -- Monk / Mistdancer
 (6488611, 64886, 153902, 1, 1, 0),  -- Paladin / Sunsoul
 (6488612, 64886, 153903, 1, 1, 0),  -- Paladin / Sunsoul
 (6488613, 64886, 153936, 1, 1, 0),  -- Priest / Communal
 (6488614, 64886, 153937, 1, 1, 0),  -- Priest / Communal
 (6488615, 64886, 153952, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488616, 64886, 153953, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488617, 64886, 154001, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488618, 64886, 154002, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488619, 64886, 154014, 1, 1, 0),  -- Warlock / Felsoul
 (6488620, 64886, 154015, 1, 1, 0),  -- Warlock / Felsoul
 (6488621, 64886, 154039, 1, 1, 0),  -- Warrior / Oathsworn
 (6488622, 64886, 154040, 1, 1, 0),  -- Warrior / Oathsworn
 (6488623, 64886, 154738, 1, 1, 0),  -- DemonHunter / Illidari
 (6488624, 64886, 154741, 1, 1, 0),  -- DemonHunter / Illidari
 (6488625, 64886, 194524, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6488626, 64886, 194527, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90887 -> PackageID 64887 :: Back (+ chest variants per package) (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488701, 64887, 153718, 1, 1, 0),  -- DeathKnight / Heart-Lesion (chest)
 (6488702, 64887, 153733, 1, 1, 0),  -- DeathKnight / Heart-Lesion (chest)
 (6488703, 64887, 153734, 1, 1, 0),  -- DeathKnight / Heart-Lesion (cloak)
 (6488704, 64887, 153793, 1, 1, 0),  -- Druid / Springrain (cloak, capture-confirmed)
 (6488705, 64887, 153799, 1, 1, 0),  -- Hunter / Trailseeker (cloak)
 (6488706, 64887, 153805, 1, 1, 0),  -- Hunter / Trailseeker (chest)
 (6488707, 64887, 153829, 1, 1, 0),  -- Mage / Mountainsage (cloak)
 (6488708, 64887, 153837, 1, 1, 0),  -- Monk / Mistdancer (chest)
 (6488709, 64887, 153865, 1, 1, 0),  -- Monk / Mistdancer (cloak)
 (6488710, 64887, 153866, 1, 1, 0),  -- Monk / Mistdancer (chest)
 (6488711, 64887, 153867, 1, 1, 0),  -- Paladin / Sunsoul (chest)
 (6488712, 64887, 153875, 1, 1, 0),  -- Paladin / Sunsoul (chest)
 (6488713, 64887, 153900, 1, 1, 0),  -- Paladin / Sunsoul (chest)
 (6488714, 64887, 153901, 1, 1, 0),  -- Paladin / Sunsoul (cloak)
 (6488715, 64887, 153935, 1, 1, 0),  -- Priest / Communal (cloak)
 (6488716, 64887, 153945, 1, 1, 0),  -- Rogue / Lightdrinker (cloak)
 (6488717, 64887, 153951, 1, 1, 0),  -- Rogue / Lightdrinker (chest)
 (6488718, 64887, 153998, 1, 1, 0),  -- Shaman / Streamtalker (cloak, capture-confirmed)
 (6488719, 64887, 154023, 1, 1, 0),  -- Warlock / Felsoul (cloak)
 (6488720, 64887, 154026, 1, 1, 0),  -- Warrior / Oathsworn (chest)
 (6488721, 64887, 154037, 1, 1, 0),  -- Warrior / Oathsworn (chest)
 (6488722, 64887, 154119, 1, 1, 0),  -- Warrior / Oathsworn (cloak)
 (6488723, 64887, 154739, 1, 1, 0),  -- DemonHunter / Illidari (robe)
 (6488724, 64887, 154748, 1, 1, 0),  -- DemonHunter / Illidari (drape)
 (6488725, 64887, 194526, 1, 1, 0),  -- Evoker / Blue Winglord's (chest)
 (6488726, 64887, 194535, 1, 1, 0);  -- Evoker / Cobalt Winglord's (cloak)

-- ---- Quest 90888 -> PackageID 64888 :: Trinkets x2 (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488801, 64888, 153740, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488802, 64888, 153743, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488803, 64888, 153795, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488804, 64888, 153798, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488805, 64888, 153801, 1, 1, 0),  -- Hunter / Trailseeker
 (6488806, 64888, 153804, 1, 1, 0),  -- Hunter / Trailseeker
 (6488807, 64888, 153816, 1, 1, 0),  -- Mage / Mountainsage
 (6488808, 64888, 153819, 1, 1, 0),  -- Mage / Mountainsage
 (6488809, 64888, 153860, 1, 1, 0),  -- Monk / Mistdancer
 (6488810, 64888, 153864, 1, 1, 0),  -- Monk / Mistdancer
 (6488811, 64888, 153907, 1, 1, 0),  -- Paladin / Sunsoul
 (6488812, 64888, 153910, 1, 1, 0),  -- Paladin / Sunsoul
 (6488813, 64888, 153926, 1, 1, 0),  -- Priest / Communal
 (6488814, 64888, 153930, 1, 1, 0),  -- Priest / Communal
 (6488815, 64888, 153947, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488816, 64888, 153950, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488817, 64888, 153994, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488818, 64888, 153997, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488819, 64888, 154010, 1, 1, 0),  -- Warlock / Felsoul
 (6488820, 64888, 154013, 1, 1, 0),  -- Warlock / Felsoul
 (6488821, 64888, 154116, 1, 1, 0),  -- Warrior / Oathsworn
 (6488822, 64888, 154117, 1, 1, 0),  -- Warrior / Oathsworn
 (6488823, 64888, 154743, 1, 1, 0),  -- DemonHunter / Illidari (Demon Trophy)
 (6488824, 64888, 154744, 1, 1, 0),  -- DemonHunter / Illidari (Charm of Demonic Fire)
 (6488825, 64888, 194531, 1, 1, 0),  -- Evoker / Claw-Carved Figurine
 (6488826, 64888, 194532, 1, 1, 0);  -- Evoker / Blue Winglord's Insignia

-- ---- Quest 90893 -> PackageID 64893 :: Waist + Wrist (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6489301, 64893, 153745, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489302, 64893, 153746, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489303, 64893, 153790, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489304, 64893, 153791, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489305, 64893, 153811, 1, 1, 0),  -- Hunter / Trailseeker
 (6489306, 64893, 153812, 1, 1, 0),  -- Hunter / Trailseeker
 (6489307, 64893, 153826, 1, 1, 0),  -- Mage / Mountainsage
 (6489308, 64893, 153827, 1, 1, 0),  -- Mage / Mountainsage
 (6489309, 64893, 153857, 1, 1, 0),  -- Monk / Mistdancer
 (6489310, 64893, 153858, 1, 1, 0),  -- Monk / Mistdancer
 (6489311, 64893, 153912, 1, 1, 0),  -- Paladin / Sunsoul
 (6489312, 64893, 153913, 1, 1, 0),  -- Paladin / Sunsoul
 (6489313, 64893, 153942, 1, 1, 0),  -- Priest / Communal
 (6489314, 64893, 153943, 1, 1, 0),  -- Priest / Communal
 (6489315, 64893, 153957, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489316, 64893, 153958, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489317, 64893, 154007, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489318, 64893, 154008, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489319, 64893, 154020, 1, 1, 0),  -- Warlock / Felsoul
 (6489320, 64893, 154021, 1, 1, 0),  -- Warlock / Felsoul
 (6489321, 64893, 154049, 1, 1, 0),  -- Warrior / Oathsworn
 (6489322, 64893, 154050, 1, 1, 0),  -- Warrior / Oathsworn
 (6489323, 64893, 154740, 1, 1, 0),  -- DemonHunter / Illidari
 (6489324, 64893, 154742, 1, 1, 0),  -- DemonHunter / Illidari
 (6489325, 64893, 194523, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6489326, 64893, 194525, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90895 -> PackageID 64895 :: Legs + Neck (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6489501, 64895, 153738, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489502, 64895, 153739, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489503, 64895, 153788, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489504, 64895, 153794, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489505, 64895, 153800, 1, 1, 0),  -- Hunter / Trailseeker
 (6489506, 64895, 153809, 1, 1, 0),  -- Hunter / Trailseeker
 (6489507, 64895, 153815, 1, 1, 0),  -- Mage / Mountainsage
 (6489508, 64895, 153823, 1, 1, 0),  -- Mage / Mountainsage
 (6489509, 64895, 153850, 1, 1, 0),  -- Monk / Mistdancer
 (6489510, 64895, 153861, 1, 1, 0),  -- Monk / Mistdancer
 (6489511, 64895, 153905, 1, 1, 0),  -- Paladin / Sunsoul
 (6489512, 64895, 153906, 1, 1, 0),  -- Paladin / Sunsoul
 (6489513, 64895, 153925, 1, 1, 0),  -- Priest / Communal
 (6489514, 64895, 153939, 1, 1, 0),  -- Priest / Communal
 (6489515, 64895, 153946, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489516, 64895, 153955, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489517, 64895, 153993, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489518, 64895, 154004, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489519, 64895, 154009, 1, 1, 0),  -- Warlock / Felsoul
 (6489520, 64895, 154017, 1, 1, 0),  -- Warlock / Felsoul
 (6489521, 64895, 154042, 1, 1, 0),  -- Warrior / Oathsworn
 (6489522, 64895, 154118, 1, 1, 0),  -- Warrior / Oathsworn
 (6489523, 64895, 154736, 1, 1, 0),  -- DemonHunter / Illidari
 (6489524, 64895, 154747, 1, 1, 0),  -- DemonHunter / Illidari
 (6489525, 64895, 194529, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6489526, 64895, 194536, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90896 -> PackageID 64896 :: Head + Shoulder (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6489601, 64896, 153737, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489602, 64896, 153744, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489603, 64896, 153787, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489604, 64896, 153789, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489605, 64896, 153808, 1, 1, 0),  -- Hunter / Trailseeker
 (6489606, 64896, 153810, 1, 1, 0),  -- Hunter / Trailseeker
 (6489607, 64896, 153822, 1, 1, 0),  -- Mage / Mountainsage
 (6489608, 64896, 153825, 1, 1, 0),  -- Mage / Mountainsage
 (6489609, 64896, 153842, 1, 1, 0),  -- Monk / Mistdancer
 (6489610, 64896, 153847, 1, 1, 0),  -- Monk / Mistdancer
 (6489611, 64896, 153855, 1, 1, 0),  -- Monk / Mistdancer
 (6489612, 64896, 153904, 1, 1, 0),  -- Paladin / Sunsoul
 (6489613, 64896, 153911, 1, 1, 0),  -- Paladin / Sunsoul
 (6489614, 64896, 153938, 1, 1, 0),  -- Priest / Communal
 (6489615, 64896, 153941, 1, 1, 0),  -- Priest / Communal
 (6489616, 64896, 153954, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489617, 64896, 153956, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489618, 64896, 154003, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489619, 64896, 154006, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489620, 64896, 154016, 1, 1, 0),  -- Warlock / Felsoul
 (6489621, 64896, 154041, 1, 1, 0),  -- Warrior / Oathsworn
 (6489622, 64896, 154048, 1, 1, 0),  -- Warrior / Oathsworn
 (6489623, 64896, 154735, 1, 1, 0),  -- DemonHunter / Illidari
 (6489624, 64896, 154737, 1, 1, 0),  -- DemonHunter / Illidari
 (6489625, 64896, 194528, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6489626, 64896, 194530, 1, 1, 0);  -- Evoker / Blue Winglord's
