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

--
-- Recruit-A-Friend: which reward activities each account has already claimed (once-only claims).
--
DROP TABLE IF EXISTS `battlenet_account_raf_claimed`;
CREATE TABLE `battlenet_account_raf_claimed` (
  `accountId` int unsigned NOT NULL,
  `activityId` int unsigned NOT NULL,
  PRIMARY KEY (`accountId`,`activityId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
