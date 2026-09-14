-- Guarded 2026-07-21, in the same style as 2026_02_12_00_hotfixes.sql.
--
-- Second half of the race-mask conversion: rename `X_1`/`X_2` to `X1`/`X2` and
-- drop the pre-split `X`. Each clause is guarded INDEPENDENTLY so the file
-- converges from any prior state - guarding the compound ALTER as a whole is not
-- enough, because a half-converted database (X_1/X_2 present but X already gone)
-- passes a single guard and then dies on
--   [1091] Can't DROP 'RaceMask'; check that column/key exists

DROP PROCEDURE IF EXISTS `guard_race_mask_rename`;
DELIMITER //
CREATE PROCEDURE `guard_race_mask_rename`()
BEGIN
    -- rename RaceMask_1 -> RaceMask1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask_1') THEN
        ALTER TABLE `character_loadout` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_1 if RaceMask1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask1') THEN
        ALTER TABLE `character_loadout` DROP `RaceMask_1`;
    END IF;

    -- rename RaceMask_2 -> RaceMask2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask_2') THEN
        ALTER TABLE `character_loadout` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_2 if RaceMask2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask2') THEN
        ALTER TABLE `character_loadout` DROP `RaceMask_2`;
    END IF;

    -- drop the pre-split RaceMask only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_loadout' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `character_loadout` DROP `RaceMask`;
    END IF;

    -- rename RaceMask_1 -> RaceMask1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask_1') THEN
        ALTER TABLE `chr_customization_req` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_1 if RaceMask1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask1') THEN
        ALTER TABLE `chr_customization_req` DROP `RaceMask_1`;
    END IF;

    -- rename RaceMask_2 -> RaceMask2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask_2') THEN
        ALTER TABLE `chr_customization_req` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_2 if RaceMask2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask2') THEN
        ALTER TABLE `chr_customization_req` DROP `RaceMask_2`;
    END IF;

    -- drop the pre-split RaceMask only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chr_customization_req' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `chr_customization_req` DROP `RaceMask`;
    END IF;

    -- rename RaceMask_1 -> RaceMask1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask_1') THEN
        ALTER TABLE `emotes` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_1 if RaceMask1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask1') THEN
        ALTER TABLE `emotes` DROP `RaceMask_1`;
    END IF;

    -- rename RaceMask_2 -> RaceMask2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask_2') THEN
        ALTER TABLE `emotes` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_2 if RaceMask2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask2') THEN
        ALTER TABLE `emotes` DROP `RaceMask_2`;
    END IF;

    -- drop the pre-split RaceMask only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'emotes' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `emotes` DROP `RaceMask`;
    END IF;

    -- rename ReputationRaceMask1_1 -> ReputationRaceMask11 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1_1') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask1_1` `ReputationRaceMask11` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask1_1 if ReputationRaceMask11 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask11') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask1_1`;
    END IF;

    -- rename ReputationRaceMask1_2 -> ReputationRaceMask12 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1_2') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask1_2` `ReputationRaceMask12` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask1_2 if ReputationRaceMask12 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask12') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask1_2`;
    END IF;

    -- rename ReputationRaceMask2_1 -> ReputationRaceMask21 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask2_1') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask2_1` `ReputationRaceMask21` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask2_1 if ReputationRaceMask21 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask2_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask21') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask2_1`;
    END IF;

    -- rename ReputationRaceMask2_2 -> ReputationRaceMask22 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask2_2') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask2_2` `ReputationRaceMask22` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask2_2 if ReputationRaceMask22 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask2_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask22') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask2_2`;
    END IF;

    -- rename ReputationRaceMask3_1 -> ReputationRaceMask31 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask3_1') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask3_1` `ReputationRaceMask31` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask3_1 if ReputationRaceMask31 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask3_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask31') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask3_1`;
    END IF;

    -- rename ReputationRaceMask3_2 -> ReputationRaceMask32 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask3_2') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask3_2` `ReputationRaceMask32` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask3_2 if ReputationRaceMask32 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask3_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask32') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask3_2`;
    END IF;

    -- rename ReputationRaceMask4_1 -> ReputationRaceMask41 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask4_1') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask4_1` `ReputationRaceMask41` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask4_1 if ReputationRaceMask41 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask4_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask41') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask4_1`;
    END IF;

    -- rename ReputationRaceMask4_2 -> ReputationRaceMask42 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask4_2') THEN
        ALTER TABLE `faction` CHANGE `ReputationRaceMask4_2` `ReputationRaceMask42` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned ReputationRaceMask4_2 if ReputationRaceMask42 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask4_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask42') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask4_2`;
    END IF;

    -- drop the pre-split ReputationRaceMask1 only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask1') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask1`;
    END IF;

    -- drop the pre-split ReputationRaceMask2 only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask2') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask2`;
    END IF;

    -- drop the pre-split ReputationRaceMask3 only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask3') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask3`;
    END IF;

    -- drop the pre-split ReputationRaceMask4 only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'faction' AND COLUMN_NAME = 'ReputationRaceMask4') THEN
        ALTER TABLE `faction` DROP `ReputationRaceMask4`;
    END IF;

    -- rename AllowableRace_1 -> AllowableRace1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace_1') THEN
        ALTER TABLE `item_search_name` CHANGE `AllowableRace_1` `AllowableRace1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned AllowableRace_1 if AllowableRace1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace1') THEN
        ALTER TABLE `item_search_name` DROP `AllowableRace_1`;
    END IF;

    -- rename AllowableRace_2 -> AllowableRace2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace_2') THEN
        ALTER TABLE `item_search_name` CHANGE `AllowableRace_2` `AllowableRace2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned AllowableRace_2 if AllowableRace2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace2') THEN
        ALTER TABLE `item_search_name` DROP `AllowableRace_2`;
    END IF;

    -- drop the pre-split AllowableRace only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_search_name' AND COLUMN_NAME = 'AllowableRace') THEN
        ALTER TABLE `item_search_name` DROP `AllowableRace`;
    END IF;

    -- rename AllowableRace_1 -> AllowableRace1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace_1') THEN
        ALTER TABLE `item_sparse` CHANGE `AllowableRace_1` `AllowableRace1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned AllowableRace_1 if AllowableRace1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace1') THEN
        ALTER TABLE `item_sparse` DROP `AllowableRace_1`;
    END IF;

    -- rename AllowableRace_2 -> AllowableRace2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace_2') THEN
        ALTER TABLE `item_sparse` CHANGE `AllowableRace_2` `AllowableRace2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned AllowableRace_2 if AllowableRace2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace2') THEN
        ALTER TABLE `item_sparse` DROP `AllowableRace_2`;
    END IF;

    -- drop the pre-split AllowableRace only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_sparse' AND COLUMN_NAME = 'AllowableRace') THEN
        ALTER TABLE `item_sparse` DROP `AllowableRace`;
    END IF;

    -- rename RaceMask_1 -> RaceMask1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask_1') THEN
        ALTER TABLE `player_condition` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_1 if RaceMask1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask1') THEN
        ALTER TABLE `player_condition` DROP `RaceMask_1`;
    END IF;

    -- rename RaceMask_2 -> RaceMask2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask_2') THEN
        ALTER TABLE `player_condition` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_2 if RaceMask2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask2') THEN
        ALTER TABLE `player_condition` DROP `RaceMask_2`;
    END IF;

    -- drop the pre-split RaceMask only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_condition' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `player_condition` DROP `RaceMask`;
    END IF;

    -- rename RaceMask_1 -> RaceMask1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask_1') THEN
        ALTER TABLE `skill_line_ability` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_1 if RaceMask1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask1') THEN
        ALTER TABLE `skill_line_ability` DROP `RaceMask_1`;
    END IF;

    -- rename RaceMask_2 -> RaceMask2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask_2') THEN
        ALTER TABLE `skill_line_ability` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_2 if RaceMask2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask2') THEN
        ALTER TABLE `skill_line_ability` DROP `RaceMask_2`;
    END IF;

    -- drop the pre-split RaceMask only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_line_ability' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `skill_line_ability` DROP `RaceMask`;
    END IF;

    -- rename RaceMask_1 -> RaceMask1 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask_1') THEN
        ALTER TABLE `skill_race_class_info` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_1 if RaceMask1 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask_1')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask1') THEN
        ALTER TABLE `skill_race_class_info` DROP `RaceMask_1`;
    END IF;

    -- rename RaceMask_2 -> RaceMask2 only while the rename is still pending
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask_2') THEN
        ALTER TABLE `skill_race_class_info` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0;
    END IF;
    -- drop the orphaned RaceMask_2 if RaceMask2 is already in place
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask_2')
       AND EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask2') THEN
        ALTER TABLE `skill_race_class_info` DROP `RaceMask_2`;
    END IF;

    -- drop the pre-split RaceMask only while it is still present
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'skill_race_class_info' AND COLUMN_NAME = 'RaceMask') THEN
        ALTER TABLE `skill_race_class_info` DROP `RaceMask`;
    END IF;

END //
DELIMITER ;

CALL `guard_race_mask_rename`();
DROP PROCEDURE IF EXISTS `guard_race_mask_rename`;
