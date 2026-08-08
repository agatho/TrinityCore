--
-- Phase 10 followup - Alliance intro-quest column for major_faction_config.
--
-- Most Major Factions share a single faction-neutral intro quest. Dragonscale
-- Expedition (2507) is the exception: Horde uses quest 65435 ("The Dragon
-- Isles Await") while Alliance uses quest 65436 (same English title, distinct
-- quest ID, distinct QuestLineXQuest OrderIndex). The original Phase 10H
-- schema only had a single `introQuestId` column; this migration adds
-- `introQuestIdAlliance` and seeds the Dragonscale Alliance variant. A value
-- of 0 in `introQuestIdAlliance` means "no team-specific Alliance variant;
-- fall back to introQuestId for both teams" - the team-aware getter in
-- MajorFactionMgr handles this.
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_2507_DRAGONSCALE.json
--   introQuestIdHorde: 65435
--   introQuestIdAlliance: 65436
--

-- Idempotent column add: MySQL 8+ lacks `ADD COLUMN IF NOT EXISTS`, so we
-- guard via INFORMATION_SCHEMA and a stored procedure (same pattern used in
-- the trait_tree migration).
DROP PROCEDURE IF EXISTS `add_major_faction_config_alliance_intro`;
DELIMITER //
CREATE PROCEDURE `add_major_faction_config_alliance_intro`()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
                   WHERE TABLE_SCHEMA = DATABASE()
                     AND TABLE_NAME   = 'major_faction_config'
                     AND COLUMN_NAME  = 'introQuestIdAlliance') THEN
        ALTER TABLE `major_faction_config`
            ADD COLUMN `introQuestIdAlliance` INT UNSIGNED NOT NULL DEFAULT 0
            AFTER `introQuestId`;
    END IF;
END //
DELIMITER ;

CALL `add_major_faction_config_alliance_intro`();
DROP PROCEDURE IF EXISTS `add_major_faction_config_alliance_intro`;

-- Seed Dragonscale Alliance intro quest.
UPDATE `major_faction_config` SET `introQuestIdAlliance` = 65436 WHERE `factionId` = 2507;
