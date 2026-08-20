-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: creature loot (Go'shek farm mobs)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source: C:/dumps/tcharvest/out/catchup_zone/zone_2796/creature_loot_template.sql
--   (TCHarvest wire loot decoder, SMSG_LOOT_RESPONSE, opcode 0x4500BE), cross-confirmed
--   against the addon loot_capture channel. sniff_loot_confidence.txt:
--   loot_responses_decoded=27 (corpses_with_items=7 money_only=20 total_coins_copper=1648),
--   creature_loot_template new=5 (distinct Entry->Item pairs). REAL wire-captured drop
--   MEMBERSHIP, not a guess. Per capture-first precedence, Wowhead is only a fallback for
--   drop-distribution gaps (not needed here -- membership is direct wire evidence).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE on PK (Entry,Item) -> re-apply safe).
-- ============================================================================

-- ---- SECTION 1 -- lootid linkage ----
-- Task 1's 10_creature_template.sql (checked: grep for `lootid` across this quest's
-- CatchUpExperience/ dir returns ZERO matches) does NOT set `lootid` on 244674/244676/244677
-- -- TrinityCore's creature_loot_template convention is lootid = entry for a dedicated table,
-- so wire this up explicitly rather than assume the engine default (default lootid=0 -> no
-- loot table attached at all). Idempotent, scoped to only these 3 entries.
UPDATE `creature_template` SET `lootid` = `entry` WHERE `entry` IN (244674, 244676, 244677);

-- ---- SECTION 2 -- creature_loot_template : 5 captured (creature -> item) pairs ----
-- Correction vs raw decoder output (decoder emitted QuestRequired=0 for all 5 rows):
--   QuestRequired=1 for the three item-243573 rows (quest item "Poorly Written Plans" for
--   quest 90886 "Best Laid Plans of Kobolds and Ogres" -- objective 9088600 requires
--   243573 x7, see 32_quest_objectives.sql; must only drop for players on-quest).
--   QuestRequired=0 kept for 1376 and 220232 (non-quest items).
-- Chance=100 kept for the three quest-item rows (quest items drop guaranteed while
-- on-quest -- consistent with user's field observation of ~100% quest-item drop on these
-- RPE farm mobs). Chance=100 on 1376/220232 is a SINGLE-OBSERVATION placeholder only (one
-- session cannot establish a real drop%); real distribution needs a Phase-K farm-volume
-- session or a Wowhead-fallback per capture-first precedence -- do NOT read 100% as verified
-- for these two.
-- TODO Phase K: confirm item_template rows for {243573, 1376, 220232} exist in the world DB
-- (they are 11.2.7 client items sourced from the Item DB2 import, NOT hand-authored here); if
-- absent, import from DB2 before this loot table can resolve at runtime.
-- MinCount/MaxCount = observed on-wire stack Quantity (1/1 for all 5 rows).

-- 244674 Ogre Destroyer -> 243573 "Poorly Written Plans" [quest item, quest 90886]
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `VerifiedBuild`) VALUES (244674, 243573, 0, 100.0, 1, 1, 0, 1, 1, 69382) ON DUPLICATE KEY UPDATE `Chance`=VALUES(`Chance`), `QuestRequired`=VALUES(`QuestRequired`), `LootMode`=VALUES(`LootMode`), `GroupId`=VALUES(`GroupId`), `MinCount`=VALUES(`MinCount`), `MaxCount`=VALUES(`MaxCount`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- 244676 Kobold Pillager -> 243573 "Poorly Written Plans" [quest item, quest 90886]
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `VerifiedBuild`) VALUES (244676, 243573, 0, 100.0, 1, 1, 0, 1, 1, 69382) ON DUPLICATE KEY UPDATE `Chance`=VALUES(`Chance`), `QuestRequired`=VALUES(`QuestRequired`), `LootMode`=VALUES(`LootMode`), `GroupId`=VALUES(`GroupId`), `MinCount`=VALUES(`MinCount`), `MaxCount`=VALUES(`MaxCount`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- 244677 Kobold Firetender -> 243573 "Poorly Written Plans" [quest item, quest 90886]
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `VerifiedBuild`) VALUES (244677, 243573, 0, 100.0, 1, 1, 0, 1, 1, 69382) ON DUPLICATE KEY UPDATE `Chance`=VALUES(`Chance`), `QuestRequired`=VALUES(`QuestRequired`), `LootMode`=VALUES(`LootMode`), `GroupId`=VALUES(`GroupId`), `MinCount`=VALUES(`MinCount`), `MaxCount`=VALUES(`MaxCount`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- 244674 Ogre Destroyer -> 1376 [non-quest; Chance=100 SINGLE-OBSERVATION placeholder, not verified drop%]
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `VerifiedBuild`) VALUES (244674, 1376, 0, 100.0, 0, 1, 0, 1, 1, 69382) ON DUPLICATE KEY UPDATE `Chance`=VALUES(`Chance`), `QuestRequired`=VALUES(`QuestRequired`), `LootMode`=VALUES(`LootMode`), `GroupId`=VALUES(`GroupId`), `MinCount`=VALUES(`MinCount`), `MaxCount`=VALUES(`MaxCount`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- 244676 Kobold Pillager -> 220232 [non-quest; Chance=100 SINGLE-OBSERVATION placeholder, not verified drop%]
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `VerifiedBuild`) VALUES (244676, 220232, 0, 100.0, 0, 1, 0, 1, 1, 69382) ON DUPLICATE KEY UPDATE `Chance`=VALUES(`Chance`), `QuestRequired`=VALUES(`QuestRequired`), `LootMode`=VALUES(`LootMode`), `GroupId`=VALUES(`GroupId`), `MinCount`=VALUES(`MinCount`), `MaxCount`=VALUES(`MaxCount`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ---- SECTION 2b -- HORDE-XVAL ADD (2026-08-21): 244669 Scavenging Hyena -> 192617 ----
-- Wire-decoded AND addon-confirmed by the Horde cross-validation run, context quest 90882
-- (C:/dumps/tcharvest/out/catchup_horde/zone_2927/). Our build (this file's SECTION 1/2 plus
-- 40_smart_scripts.sql / 51_creature_text.sql) treats 244669 as loot-less/autoattack-only --
-- this is a genuinely NEW loot pair not previously authored for this entry. NON-quest
-- (QuestRequired=0); Chance=100 is a SINGLE-SESSION/low-confidence placeholder only (same
-- caveat as the 1376/220232 rows above -- one session cannot establish real drop%). Item
-- 192617 is an existing 11.2.7 client item (Item DB2 import) -- NOT hand-authored here.
-- TODO Phase K: confirm item_template row for 192617 exists in the world DB; if absent,
-- import from DB2 before this loot table can resolve at runtime.
-- lootid linkage: idempotent, scoped to only this entry (244674/244676/244677 already
-- wired above in SECTION 1; 244669 was NOT in that list, so add it here explicitly --
-- without this, creature_template.lootid stays 0 and the row below never resolves).
UPDATE `creature_template` SET `lootid` = `entry` WHERE `entry` = 244669;
-- 244669 Scavenging Hyena -> 192617 [non-quest; Chance=100 SINGLE-SESSION placeholder, not verified drop%]
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `VerifiedBuild`) VALUES (244669, 192617, 0, 100.0, 0, 1, 0, 2, 2, 69382) ON DUPLICATE KEY UPDATE `Chance`=VALUES(`Chance`), `QuestRequired`=VALUES(`QuestRequired`), `LootMode`=VALUES(`LootMode`), `GroupId`=VALUES(`GroupId`), `MinCount`=VALUES(`MinCount`), `MaxCount`=VALUES(`MaxCount`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ---- SECTION 3 -- money (Phase-K note, not authored here) ----
-- The wire also captured ~20 money-only corpses across the run (loot_responses_decoded=27,
-- money_only=20, total_coins_copper=1648 -- avg ~82c/corpse). This is coin evidence, not item
-- evidence, and observed money supports eventually setting `creature_template.mingold`/
-- `maxgold` on these farm mobs -- but a single session's coin amounts are approximate (no
-- variance data, mixed across ~20 different corpses/entries not individually attributed here).
-- Left as a Phase-K note; no mingold/maxgold UPDATE authored in this slice.

-- ============================================================================
-- END -- review confidence notes above before applying to any branch.
-- ============================================================================
