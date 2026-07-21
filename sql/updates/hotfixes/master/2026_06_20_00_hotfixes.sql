-- ============================================================================
-- Second half of the race-mask conversion: rename `X_1`/`X_2` to `X1`/`X2`
-- and drop the pre-split `X`.
--
-- Every clause is guarded INDEPENDENTLY on information_schema so the file
-- converges from any prior state. Guarding the compound ALTER as a whole is
-- not enough: a half-converted database (X_1/X_2 present but X already gone,
-- reachable by having run the older unguarded 2026_04_22_00 once) passes a
-- single guard and then fails with
--   [1091] Can't DROP 'RaceMask'; check that column/key exists
-- which aborts the file and, because TC treats a failed update as fatal,
-- stops the server.
-- ============================================================================

--
-- character_loadout
--
-- rename RaceMask_1 -> RaceMask1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `character_loadout` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_1 if RaceMask1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `character_loadout` DROP `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename RaceMask_2 -> RaceMask2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `character_loadout` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_2 if RaceMask2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `character_loadout` DROP `RaceMask_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split RaceMask only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='character_loadout' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `character_loadout` DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- chr_customization_req
--
-- rename RaceMask_1 -> RaceMask1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_1 if RaceMask1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req` DROP `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename RaceMask_2 -> RaceMask2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_2 if RaceMask2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req` DROP `RaceMask_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split RaceMask only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='chr_customization_req' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `chr_customization_req` DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- emotes
--
-- rename RaceMask_1 -> RaceMask1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `emotes` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_1 if RaceMask1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `emotes` DROP `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename RaceMask_2 -> RaceMask2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `emotes` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_2 if RaceMask2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `emotes` DROP `RaceMask_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split RaceMask only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='emotes' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `emotes` DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- faction
--
-- rename ReputationRaceMask1_1 -> ReputationRaceMask11 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask11'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask1_1` `ReputationRaceMask11` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask1_1 if ReputationRaceMask11 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask11'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask1_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask1_2 -> ReputationRaceMask12 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask12'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask1_2` `ReputationRaceMask12` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask1_2 if ReputationRaceMask12 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask12'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask1_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask2_1 -> ReputationRaceMask21 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask2_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask21'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask2_1` `ReputationRaceMask21` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask2_1 if ReputationRaceMask21 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask2_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask21'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask2_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask2_2 -> ReputationRaceMask22 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask2_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask22'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask2_2` `ReputationRaceMask22` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask2_2 if ReputationRaceMask22 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask2_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask22'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask2_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask3_1 -> ReputationRaceMask31 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask3_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask31'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask3_1` `ReputationRaceMask31` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask3_1 if ReputationRaceMask31 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask3_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask31'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask3_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask3_2 -> ReputationRaceMask32 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask3_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask32'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask3_2` `ReputationRaceMask32` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask3_2 if ReputationRaceMask32 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask3_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask32'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask3_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask4_1 -> ReputationRaceMask41 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask4_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask41'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask4_1` `ReputationRaceMask41` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask4_1 if ReputationRaceMask41 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask4_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask41'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask4_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename ReputationRaceMask4_2 -> ReputationRaceMask42 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask4_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask42'));
SET @s := IF(@g, 'ALTER TABLE `faction` CHANGE `ReputationRaceMask4_2` `ReputationRaceMask42` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned ReputationRaceMask4_2 if ReputationRaceMask42 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask4_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask42'));
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask4_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split ReputationRaceMask1 only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask1');
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split ReputationRaceMask2 only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask2');
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split ReputationRaceMask3 only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask3');
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask3`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split ReputationRaceMask4 only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='faction' AND COLUMN_NAME='ReputationRaceMask4');
SET @s := IF(@g, 'ALTER TABLE `faction` DROP `ReputationRaceMask4`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- item_search_name
--
-- rename AllowableRace_1 -> AllowableRace1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace1'));
SET @s := IF(@g, 'ALTER TABLE `item_search_name` CHANGE `AllowableRace_1` `AllowableRace1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned AllowableRace_1 if AllowableRace1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace1'));
SET @s := IF(@g, 'ALTER TABLE `item_search_name` DROP `AllowableRace_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename AllowableRace_2 -> AllowableRace2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace2'));
SET @s := IF(@g, 'ALTER TABLE `item_search_name` CHANGE `AllowableRace_2` `AllowableRace2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned AllowableRace_2 if AllowableRace2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace2'));
SET @s := IF(@g, 'ALTER TABLE `item_search_name` DROP `AllowableRace_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split AllowableRace only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_search_name' AND COLUMN_NAME='AllowableRace');
SET @s := IF(@g, 'ALTER TABLE `item_search_name` DROP `AllowableRace`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- item_sparse
--
-- rename AllowableRace_1 -> AllowableRace1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace1'));
SET @s := IF(@g, 'ALTER TABLE `item_sparse` CHANGE `AllowableRace_1` `AllowableRace1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned AllowableRace_1 if AllowableRace1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace1'));
SET @s := IF(@g, 'ALTER TABLE `item_sparse` DROP `AllowableRace_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename AllowableRace_2 -> AllowableRace2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace2'));
SET @s := IF(@g, 'ALTER TABLE `item_sparse` CHANGE `AllowableRace_2` `AllowableRace2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned AllowableRace_2 if AllowableRace2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace2'));
SET @s := IF(@g, 'ALTER TABLE `item_sparse` DROP `AllowableRace_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split AllowableRace only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_sparse' AND COLUMN_NAME='AllowableRace');
SET @s := IF(@g, 'ALTER TABLE `item_sparse` DROP `AllowableRace`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- player_condition
--
-- rename RaceMask_1 -> RaceMask1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `player_condition` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_1 if RaceMask1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `player_condition` DROP `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename RaceMask_2 -> RaceMask2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `player_condition` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_2 if RaceMask2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `player_condition` DROP `RaceMask_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split RaceMask only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='player_condition' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `player_condition` DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- skill_line_ability
--
-- rename RaceMask_1 -> RaceMask1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_1 if RaceMask1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability` DROP `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename RaceMask_2 -> RaceMask2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_2 if RaceMask2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability` DROP `RaceMask_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split RaceMask only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_line_ability' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `skill_line_ability` DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- skill_race_class_info
--
-- rename RaceMask_1 -> RaceMask1 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask_1') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info` CHANGE `RaceMask_1` `RaceMask1` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_1 if RaceMask1 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask_1') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask1'));
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info` DROP `RaceMask_1`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- rename RaceMask_2 -> RaceMask2 (only while the intermediate column exists and the target does not)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask_2') AND NOT (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info` CHANGE `RaceMask_2` `RaceMask2` int NOT NULL DEFAULT 0', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the orphaned RaceMask_2 if RaceMask2 is already in place (half-converted database)
SET @g := ((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask_2') AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask2'));
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info` DROP `RaceMask_2`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- drop the pre-split RaceMask only if it is still present
SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='skill_race_class_info' AND COLUMN_NAME='RaceMask');
SET @s := IF(@g, 'ALTER TABLE `skill_race_class_info` DROP `RaceMask`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;
