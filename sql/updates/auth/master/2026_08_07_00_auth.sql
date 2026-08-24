--
-- P0 batch: social contract acceptance + Battle.net UI reports.
--
-- 1) `battlenet_accounts`.`accepted_social_contract`
--    WorldSession::HandleSocialContractRequest used to hardcode ShowSocialContract = false, suppressing the
--    retail social-contract flow entirely. It now reads this flag and HandleAcceptSocialContract sets it, so the
--    prompt appears once per battlenet account and stays dismissed across relogs.
--
-- 2) `battlenet_account_report`
--    Sink for bgs report.v3::SubmitReport (Battle.net UI reports: right-click a BattleTag / a club message /
--    an entity and choose Report). Every such report was previously answered ERROR_RPC_NOT_IMPLEMENTED and
--    logged at TC_LOG_ERROR, i.e. discarded.
--
-- Idempotent: safe to re-run.
--

SET @col := (SELECT COUNT(*) FROM information_schema.COLUMNS
             WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'battlenet_accounts' AND COLUMN_NAME = 'accepted_social_contract');
SET @sql := IF(@col = 0,
    'ALTER TABLE `battlenet_accounts` ADD COLUMN `accepted_social_contract` TINYINT UNSIGNED NOT NULL DEFAULT 0',
    'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

CREATE TABLE IF NOT EXISTS `battlenet_account_report` (
  `id`                INT UNSIGNED    NOT NULL AUTO_INCREMENT,
  `reporterAccountId` INT UNSIGNED    NOT NULL,
  `targetAccountId`   BIGINT UNSIGNED NOT NULL DEFAULT 0,  -- report.v3 UserOptions.target_account_id
  `issueType`         INT UNSIGNED    NOT NULL DEFAULT 0,  -- report.v3.IssueType
  `source`            INT UNSIGNED    NOT NULL DEFAULT 0,  -- report.v3 UserSource / ClubSource
  `clubId`            BIGINT UNSIGNED NOT NULL DEFAULT 0,  -- report.v3 ClubOptions.club_id
  `streamId`          BIGINT UNSIGNED NOT NULL DEFAULT 0,  -- report.v3 ClubOptions.stream_id
  `entityId`          VARCHAR(128)    NOT NULL DEFAULT '', -- report.v3 EntityOptions.entity_id
  `entityType`        VARCHAR(64)     NOT NULL DEFAULT '', -- report.v3 EntityOptions.entity_type
  `userDescription`   TEXT            NOT NULL,
  `submittedTime`     BIGINT UNSIGNED NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_reporter` (`reporterAccountId`),
  KEY `idx_target` (`targetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
