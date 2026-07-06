--
-- Recruit-A-Friend: per-account recruitment state. `recruitmentCode` is minted for the account when it requests a
-- recruitment link; `recruiterAccountId` / `recruitName` are set when this account is recruited by another (so a
-- recruiter lists its recruits via WHERE recruiterAccountId = <self>).
--
DROP TABLE IF EXISTS `battlenet_account_recruitment`;
CREATE TABLE `battlenet_account_recruitment` (
  `accountId` int unsigned NOT NULL,
  `recruitmentCode` varchar(32) NOT NULL DEFAULT '',
  `recruiterAccountId` int unsigned NOT NULL DEFAULT '0',
  `recruitName` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`accountId`),
  KEY `idx_recruiter` (`recruiterAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
