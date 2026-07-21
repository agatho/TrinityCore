-- ============================================================================
-- Guarded 2026-07-21: every statement below is now safe to run against a
-- hotfixes database that is ALREADY past this conversion (e.g. a WCDB v1.4
-- baseline, whose dump ships an EMPTY `updates` ledger - so TC re-applies the
-- whole update history on first start and reaches this file).
--
-- Unguarded, the UPDATEs referenced a pre-split `RaceMask`/`AllowableRace`
-- column that no longer exists, giving
--   [1054] Unknown column 'RaceMask' in 'field list'
-- MySQL stops at the first error, so the rest of this file never ran, and TC
-- aborts startup on a failed update.
--
-- The 64-bit -> 2x int32 split itself is unchanged: low 32 bits sign-extended
-- into _1, high 32 into _2.
-- ============================================================================

-- guarded: only add split columns while pre-split `RaceMask` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `character_loadout`
  ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `ItemContext`,
  ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `RaceMask` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'UPDATE `character_loadout` SET 
  `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
  `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `RaceMask` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req`
  ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `ItemModifiedAppearanceID`,
  ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `RaceMask` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'UPDATE `chr_customization_req` SET 
  `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
  `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `RaceMask` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `emotes`
  ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `ClassMask`,
  ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `RaceMask` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'UPDATE `emotes` SET 
  `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
  `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `ReputationRaceMask1` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1');
SET @s := IF(@g, 'ALTER TABLE `faction`
  ADD `ReputationRaceMask1_1` int NOT NULL DEFAULT 0 AFTER `ParentFactionCap2`,
  ADD `ReputationRaceMask1_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask1_1`,
  ADD `ReputationRaceMask2_1` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask1_2`,
  ADD `ReputationRaceMask2_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask2_1`,
  ADD `ReputationRaceMask3_1` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask2_2`,
  ADD `ReputationRaceMask3_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask3_1`,
  ADD `ReputationRaceMask4_1` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask3_2`,
  ADD `ReputationRaceMask4_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask4_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `ReputationRaceMask1` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1');
SET @s := IF(@g, 'UPDATE `faction` SET 
  `ReputationRaceMask1_1` = IF((`ReputationRaceMask1` >>  0) & 0x80000000, CAST(((`ReputationRaceMask1` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask1` >>  0) & 0xFFFFFFFF),
  `ReputationRaceMask1_2` = IF((`ReputationRaceMask1` >> 32) & 0x80000000, CAST(((`ReputationRaceMask1` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask1` >> 32) & 0xFFFFFFFF),
  `ReputationRaceMask2_1` = IF((`ReputationRaceMask2` >>  0) & 0x80000000, CAST(((`ReputationRaceMask2` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask2` >>  0) & 0xFFFFFFFF),
  `ReputationRaceMask2_2` = IF((`ReputationRaceMask2` >> 32) & 0x80000000, CAST(((`ReputationRaceMask2` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask2` >> 32) & 0xFFFFFFFF),
  `ReputationRaceMask3_1` = IF((`ReputationRaceMask3` >>  0) & 0x80000000, CAST(((`ReputationRaceMask3` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask3` >>  0) & 0xFFFFFFFF),
  `ReputationRaceMask3_2` = IF((`ReputationRaceMask3` >> 32) & 0x80000000, CAST(((`ReputationRaceMask3` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask3` >> 32) & 0xFFFFFFFF),
  `ReputationRaceMask4_1` = IF((`ReputationRaceMask4` >>  0) & 0x80000000, CAST(((`ReputationRaceMask4` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask4` >>  0) & 0xFFFFFFFF),
  `ReputationRaceMask4_2` = IF((`ReputationRaceMask4` >> 32) & 0x80000000, CAST(((`ReputationRaceMask4` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask4` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: skip if the column is already present

SET @g := (SELECT COUNT(*)=0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_extended_cost' AND COLUMN_NAME='Money');
SET @s := IF(@g, 'ALTER TABLE `item_extended_cost` ADD `Money` bigint UNSIGNED NOT NULL DEFAULT 0 AFTER `ID`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `AllowableRace` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace');
SET @s := IF(@g, 'ALTER TABLE `item_search_name`
  ADD `AllowableRace_1` int NOT NULL DEFAULT 0 AFTER `Flags5`,
  ADD `AllowableRace_2` int NOT NULL DEFAULT 0 AFTER `AllowableRace_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `AllowableRace` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace');
SET @s := IF(@g, 'UPDATE `item_search_name` SET 
  `AllowableRace_1` = IF((`AllowableRace` >>  0) & 0x80000000, CAST(((`AllowableRace` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >>  0) & 0xFFFFFFFF),
  `AllowableRace_2` = IF((`AllowableRace` >> 32) & 0x80000000, CAST(((`AllowableRace` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `AllowableRace` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace');
SET @s := IF(@g, 'ALTER TABLE `item_sparse`
  ADD `AllowableRace_1` int NOT NULL DEFAULT 0 AFTER `RequiredAbility`,
  ADD `AllowableRace_2` int NOT NULL DEFAULT 0 AFTER `AllowableRace_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `AllowableRace` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace');
SET @s := IF(@g, 'UPDATE `item_sparse` SET 
  `AllowableRace_1` = IF((`AllowableRace` >>  0) & 0x80000000, CAST(((`AllowableRace` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >>  0) & 0xFFFFFFFF),
  `AllowableRace_2` = IF((`AllowableRace` >> 32) & 0x80000000, CAST(((`AllowableRace` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: skip if the column is already present

SET @g := (SELECT COUNT(*)=0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='movie' AND COLUMN_NAME='Summary');
SET @s := IF(@g, 'ALTER TABLE `movie` ADD `Summary` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL AFTER `ID`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- Table structure for table `movie_locale`
--
DROP TABLE IF EXISTS `movie_locale`;

CREATE TABLE `movie_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Summary_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB);

ALTER TABLE `path_node` MODIFY `PathID` int NOT NULL DEFAULT 0 AFTER `ID`;

ALTER TABLE `path_property` MODIFY `PathID` int NOT NULL DEFAULT 0 AFTER `ID`;

-- guarded: only add split columns while pre-split `RaceMask` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `player_condition`
  ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `MovementFlags2`,
  ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `RaceMask` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'UPDATE `player_condition` SET 
  `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
  `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: skip if the column is already present

SET @g := (SELECT COUNT(*)=0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scenario' AND COLUMN_NAME='UiScenarioDisplayInfoID');
SET @s := IF(@g, 'ALTER TABLE `scenario` ADD `UiScenarioDisplayInfoID` int UNSIGNED NOT NULL DEFAULT 0 AFTER `UiTextureKitID`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `RaceMask` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability`
  ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `SkillupSkillLineID`,
  ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `RaceMask` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'UPDATE `skill_line_ability` SET 
  `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
  `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add split columns while pre-split `RaceMask` still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info`
  ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `SkillTierID`,
  ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only convert while the pre-split `RaceMask` column still exists

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'UPDATE `skill_race_class_info` SET 
  `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
  `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF)', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

ALTER TABLE `taxi_path_node` MODIFY COLUMN `PathID` int unsigned NOT NULL DEFAULT 0 AFTER `ID`;

-- guarded: skip if the column is already present

SET @g := (SELECT COUNT(*)=0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='transmog_outfit_entry' AND COLUMN_NAME='OutfitIndex');
SET @s := IF(@g, 'ALTER TABLE `transmog_outfit_entry` ADD `OutfitIndex` int NOT NULL DEFAULT 0 AFTER `OverrideCostModifier`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;
