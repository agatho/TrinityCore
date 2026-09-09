-- Guarded 2026-07-21, in the same style as 2026_02_12_00_hotfixes.sql.
--
-- WCDB ships an EMPTY `updates` ledger, so a fresh install replays the whole
-- hotfix history. Against a baseline already past this conversion the pre-split
-- `RaceMask`/`AllowableRace` column no longer exists and this file died with
--   [1054] Unknown column 'RaceMask' in 'field list'
-- MySQL stops at the first error, so the rest of the file never ran, and TC
-- treats a failed update as fatal and refuses to start.
--
-- The 64-bit -> 2x int32 split itself is unchanged (low 32 sign-extended into
-- _1, high 32 into _2).

DROP PROCEDURE IF EXISTS `guard_race_mask_split`;
DELIMITER //
CREATE PROCEDURE `guard_race_mask_split`()
BEGIN
    -- only add the split columns while `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `character_loadout`
          ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `ItemContext`,
          ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`;
    END IF;

    -- only convert while the pre-split `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask') THEN
        UPDATE `character_loadout` SET 
          `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
          `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF);
    END IF;

    -- only add the split columns while `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `chr_customization_req`
          ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `ItemModifiedAppearanceID`,
          ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`;
    END IF;

    -- only convert while the pre-split `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask') THEN
        UPDATE `chr_customization_req` SET 
          `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
          `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF);
    END IF;

    -- only add the split columns while `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `emotes`
          ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `ClassMask`,
          ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`;
    END IF;

    -- only convert while the pre-split `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask') THEN
        UPDATE `emotes` SET 
          `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
          `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF);
    END IF;

    -- only add the split columns while `ReputationRaceMask1` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1') THEN
        ALTER TABLE `faction`
          ADD `ReputationRaceMask1_1` int NOT NULL DEFAULT 0 AFTER `ParentFactionCap2`,
          ADD `ReputationRaceMask1_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask1_1`,
          ADD `ReputationRaceMask2_1` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask1_2`,
          ADD `ReputationRaceMask2_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask2_1`,
          ADD `ReputationRaceMask3_1` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask2_2`,
          ADD `ReputationRaceMask3_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask3_1`,
          ADD `ReputationRaceMask4_1` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask3_2`,
          ADD `ReputationRaceMask4_2` int NOT NULL DEFAULT 0 AFTER `ReputationRaceMask4_1`;
    END IF;

    -- only convert while the pre-split `ReputationRaceMask1` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1') THEN
        UPDATE `faction` SET 
          `ReputationRaceMask1_1` = IF((`ReputationRaceMask1` >>  0) & 0x80000000, CAST(((`ReputationRaceMask1` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask1` >>  0) & 0xFFFFFFFF),
          `ReputationRaceMask1_2` = IF((`ReputationRaceMask1` >> 32) & 0x80000000, CAST(((`ReputationRaceMask1` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask1` >> 32) & 0xFFFFFFFF),
          `ReputationRaceMask2_1` = IF((`ReputationRaceMask2` >>  0) & 0x80000000, CAST(((`ReputationRaceMask2` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask2` >>  0) & 0xFFFFFFFF),
          `ReputationRaceMask2_2` = IF((`ReputationRaceMask2` >> 32) & 0x80000000, CAST(((`ReputationRaceMask2` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask2` >> 32) & 0xFFFFFFFF),
          `ReputationRaceMask3_1` = IF((`ReputationRaceMask3` >>  0) & 0x80000000, CAST(((`ReputationRaceMask3` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask3` >>  0) & 0xFFFFFFFF),
          `ReputationRaceMask3_2` = IF((`ReputationRaceMask3` >> 32) & 0x80000000, CAST(((`ReputationRaceMask3` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask3` >> 32) & 0xFFFFFFFF),
          `ReputationRaceMask4_1` = IF((`ReputationRaceMask4` >>  0) & 0x80000000, CAST(((`ReputationRaceMask4` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask4` >>  0) & 0xFFFFFFFF),
          `ReputationRaceMask4_2` = IF((`ReputationRaceMask4` >> 32) & 0x80000000, CAST(((`ReputationRaceMask4` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`ReputationRaceMask4` >> 32) & 0xFFFFFFFF);
    END IF;

    -- skip if `Money` is already present
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_extended_cost' AND COLUMN_NAME = 'Money') THEN
        ALTER TABLE `item_extended_cost` ADD `Money` bigint UNSIGNED NOT NULL DEFAULT 0 AFTER `ID`;
    END IF;

    -- only add the split columns while `AllowableRace` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace') THEN
        ALTER TABLE `item_search_name`
          ADD `AllowableRace_1` int NOT NULL DEFAULT 0 AFTER `Flags5`,
          ADD `AllowableRace_2` int NOT NULL DEFAULT 0 AFTER `AllowableRace_1`;
    END IF;

    -- only convert while the pre-split `AllowableRace` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace') THEN
        UPDATE `item_search_name` SET 
          `AllowableRace_1` = IF((`AllowableRace` >>  0) & 0x80000000, CAST(((`AllowableRace` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >>  0) & 0xFFFFFFFF),
          `AllowableRace_2` = IF((`AllowableRace` >> 32) & 0x80000000, CAST(((`AllowableRace` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >> 32) & 0xFFFFFFFF);
    END IF;

    -- only add the split columns while `AllowableRace` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace') THEN
        ALTER TABLE `item_sparse`
          ADD `AllowableRace_1` int NOT NULL DEFAULT 0 AFTER `RequiredAbility`,
          ADD `AllowableRace_2` int NOT NULL DEFAULT 0 AFTER `AllowableRace_1`;
    END IF;

    -- only convert while the pre-split `AllowableRace` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace') THEN
        UPDATE `item_sparse` SET 
          `AllowableRace_1` = IF((`AllowableRace` >>  0) & 0x80000000, CAST(((`AllowableRace` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >>  0) & 0xFFFFFFFF),
          `AllowableRace_2` = IF((`AllowableRace` >> 32) & 0x80000000, CAST(((`AllowableRace` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`AllowableRace` >> 32) & 0xFFFFFFFF);
    END IF;

    -- skip if `Summary` is already present
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'movie' AND COLUMN_NAME = 'Summary') THEN
        ALTER TABLE `movie` ADD `Summary` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL AFTER `ID`;
    END IF;

    -- only add the split columns while `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `player_condition`
          ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `MovementFlags2`,
          ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`;
    END IF;

    -- only convert while the pre-split `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask') THEN
        UPDATE `player_condition` SET 
          `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
          `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF);
    END IF;

    -- skip if `UiScenarioDisplayInfoID` is already present
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'scenario' AND COLUMN_NAME = 'UiScenarioDisplayInfoID') THEN
        ALTER TABLE `scenario` ADD `UiScenarioDisplayInfoID` int UNSIGNED NOT NULL DEFAULT 0 AFTER `UiTextureKitID`;
    END IF;

    -- only add the split columns while `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `skill_line_ability`
          ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `SkillupSkillLineID`,
          ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`;
    END IF;

    -- only convert while the pre-split `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask') THEN
        UPDATE `skill_line_ability` SET 
          `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
          `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF);
    END IF;

    -- only add the split columns while `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `skill_race_class_info`
          ADD `RaceMask_1` int NOT NULL DEFAULT 0 AFTER `SkillTierID`,
          ADD `RaceMask_2` int NOT NULL DEFAULT 0 AFTER `RaceMask_1`;
    END IF;

    -- only convert while the pre-split `RaceMask` is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask') THEN
        UPDATE `skill_race_class_info` SET 
          `RaceMask_1` = IF((`RaceMask` >>  0) & 0x80000000, CAST(((`RaceMask` >>  0) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >>  0) & 0xFFFFFFFF),
          `RaceMask_2` = IF((`RaceMask` >> 32) & 0x80000000, CAST(((`RaceMask` >> 32) & 0xFFFFFFFF) | 0xFFFFFFFF00000000 AS SIGNED), (`RaceMask` >> 32) & 0xFFFFFFFF);
    END IF;

    -- skip if `OutfitIndex` is already present
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'transmog_outfit_entry' AND COLUMN_NAME = 'OutfitIndex') THEN
        ALTER TABLE `transmog_outfit_entry` ADD `OutfitIndex` int NOT NULL DEFAULT 0 AFTER `OverrideCostModifier`;
    END IF;

END //
DELIMITER ;

CALL `guard_race_mask_split`();
DROP PROCEDURE IF EXISTS `guard_race_mask_split`;

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

ALTER TABLE `taxi_path_node` MODIFY COLUMN `PathID` int unsigned NOT NULL DEFAULT 0 AFTER `ID`;
