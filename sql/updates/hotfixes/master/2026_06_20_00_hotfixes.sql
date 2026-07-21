-- ============================================================================
-- Guarded 2026-07-21. Second half of the race-mask conversion chain: rename
-- `X_1`/`X_2` to `X1`/`X2` and drop the pre-split `X`.
--
-- A database whose baseline is already past this conversion (e.g. WCDB v1.4,
-- whose dump ships an EMPTY `updates` ledger, so TC replays the whole update
-- history on first start) has no `_1` columns, giving
--   [1054] Unknown column 'RaceMask_1' in 'character_loadout'
-- MySQL stops at the first error, so the rest of the file never ran and TC
-- aborted startup. Each ALTER is now keyed on its rename still being pending.
-- ============================================================================

-- guarded: only rename while the intermediate `RaceMask_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask_1');
SET @s := IF(@g, 'ALTER TABLE `character_loadout`
  CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0 AFTER `ItemContext`,
  CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0 AFTER `RaceMask1`,
  DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `RaceMask_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask_1');
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req`
  CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0 AFTER `ItemModifiedAppearanceID`,
  CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0 AFTER `RaceMask1`,
  DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `RaceMask_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask_1');
SET @s := IF(@g, 'ALTER TABLE `emotes`
  CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0 AFTER `ClassMask`,
  CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0 AFTER `RaceMask1`,
  DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `ReputationRaceMask1_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1_1');
SET @s := IF(@g, 'ALTER TABLE `faction`
  CHANGE `ReputationRaceMask1_1` `ReputationRaceMask11` int NOT NULL DEFAULT 0 AFTER `ParentFactionCap2`,
  CHANGE `ReputationRaceMask1_2` `ReputationRaceMask12` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask11`,
  CHANGE `ReputationRaceMask2_1` `ReputationRaceMask21` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask12`,
  CHANGE `ReputationRaceMask2_2` `ReputationRaceMask22` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask21`,
  CHANGE `ReputationRaceMask3_1` `ReputationRaceMask31` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask22`,
  CHANGE `ReputationRaceMask3_2` `ReputationRaceMask32` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask31`,
  CHANGE `ReputationRaceMask4_1` `ReputationRaceMask41` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask32`,
  CHANGE `ReputationRaceMask4_2` `ReputationRaceMask42` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask41`,
  DROP `ReputationRaceMask1`,
  DROP `ReputationRaceMask2`,
  DROP `ReputationRaceMask3`,
  DROP `ReputationRaceMask4`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `AllowableRace_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace_1');
SET @s := IF(@g, 'ALTER TABLE `item_search_name`
  CHANGE `AllowableRace_1` `AllowableRace1` int NOT NULL DEFAULT 0 AFTER `Flags5`,
  CHANGE `AllowableRace_2` `AllowableRace2` int NOT NULL DEFAULT 0 AFTER `AllowableRace1`,
  DROP `AllowableRace`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `AllowableRace_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace_1');
SET @s := IF(@g, 'ALTER TABLE `item_sparse`
  CHANGE `AllowableRace_1` `AllowableRace1` int NOT NULL DEFAULT 0 AFTER `RequiredAbility`,
  CHANGE `AllowableRace_2` `AllowableRace2` int NOT NULL DEFAULT 0 AFTER `AllowableRace1`,
  DROP `AllowableRace`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `RaceMask_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask_1');
SET @s := IF(@g, 'ALTER TABLE `player_condition`
  CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0 AFTER `MovementFlags2`,
  CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0 AFTER `RaceMask1`,
  DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `RaceMask_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask_1');
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability`
  CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0 AFTER `SkillupSkillLineID`,
  CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0 AFTER `RaceMask1`,
  DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while the intermediate `RaceMask_1` column is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask_1');
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info`
  CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0 AFTER `SkillTierID`,
  CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0 AFTER `RaceMask1`,
  DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;
