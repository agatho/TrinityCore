--
-- Social Contract acceptance (CMSG_ACCEPT_SOCIAL_CONTRACT / CMSG_SOCIAL_CONTRACT_REQUEST).
-- Per-battlenet-account flag: once the account accepts the Social Contract at character select the
-- client must stop being prompted. The world server reads it to answer GetShouldShowSocialContract
-- and sets it on accept.
--
-- `ADD COLUMN IF NOT EXISTS` is MariaDB-only syntax and is a parse error on MySQL, so the guard is
-- expressed through INFORMATION_SCHEMA instead.
--
SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'battlenet_accounts' AND COLUMN_NAME = 'social_contract_accepted');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `battlenet_accounts` ADD COLUMN `social_contract_accepted` tinyint unsigned NOT NULL DEFAULT ''0'' AFTER `LoginTicketExpiry`',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
