--
-- Mythic+ weekly keystone rollover: persist the previous week's run summary.
--
-- The weekly run rows (character_mythic_plus_weekly) are dropped the moment the finished week is pruned, but the
-- retail keystone rule needs that week's highest completed / highest timed level to pick the new key. Keeping the
-- summary only in memory loses it whenever a character save or a server restart lands between the weekly reset and
-- the keystone adjustment, silently degrading the key to the "no history" rule (one level lower). These columns
-- make it durable.
--
-- Idempotent: each column is added only when information_schema says it is missing, so the file is safe to re-run
-- and safe against a base dump that already carries the columns.
--

DELIMITER $$

DROP PROCEDURE IF EXISTS `_tc_add_mythic_plus_vault_prev_week` $$
CREATE PROCEDURE `_tc_add_mythic_plus_vault_prev_week`()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
                   WHERE `TABLE_SCHEMA` = DATABASE()
                     AND `TABLE_NAME` = 'character_mythic_plus_vault'
                     AND `COLUMN_NAME` = 'prevWeekResetTime') THEN
        ALTER TABLE `character_mythic_plus_vault`
            ADD COLUMN `prevWeekResetTime` BIGINT NOT NULL DEFAULT 0
            COMMENT 'Weekly reset boundary the summarised previous week ended at' AFTER `keystoneResetTime`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
                   WHERE `TABLE_SCHEMA` = DATABASE()
                     AND `TABLE_NAME` = 'character_mythic_plus_vault'
                     AND `COLUMN_NAME` = 'prevWeekBestLevel') THEN
        ALTER TABLE `character_mythic_plus_vault`
            ADD COLUMN `prevWeekBestLevel` INT UNSIGNED NOT NULL DEFAULT 0
            COMMENT 'Highest keystone level completed in that week (timed or not)' AFTER `prevWeekResetTime`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
                   WHERE `TABLE_SCHEMA` = DATABASE()
                     AND `TABLE_NAME` = 'character_mythic_plus_vault'
                     AND `COLUMN_NAME` = 'prevWeekBestTimedLevel') THEN
        ALTER TABLE `character_mythic_plus_vault`
            ADD COLUMN `prevWeekBestTimedLevel` INT UNSIGNED NOT NULL DEFAULT 0
            COMMENT 'Highest keystone level completed IN TIME in that week' AFTER `prevWeekBestLevel`;
    END IF;
END $$

DELIMITER ;

CALL `_tc_add_mythic_plus_vault_prev_week`();
DROP PROCEDURE IF EXISTS `_tc_add_mythic_plus_vault_prev_week`;
