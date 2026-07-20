--
-- Club Finder: guild recruitment postings (one per guild).
DROP TABLE IF EXISTS `club_finder_posting`;
CREATE TABLE `club_finder_posting` (
  `postingId` int unsigned NOT NULL,
  `clubId` bigint unsigned NOT NULL COMMENT 'Guild id this posting advertises',
  `name` varchar(96) NOT NULL DEFAULT '' COMMENT 'Client clamps to 96 chars',
  `description` varchar(2048) NOT NULL DEFAULT '' COMMENT 'Client clamps to 2048 chars',
  `recruitingSpecs` bigint unsigned NOT NULL DEFAULT '0',
  `recruitmentFlags` int unsigned NOT NULL DEFAULT '0' COMMENT 'ClubFinderSettingFlags bit-index mask; locale in bits 21-25',
  `itemLevelRequirement` int unsigned NOT NULL DEFAULT '0',
  `avatarId` int unsigned NOT NULL DEFAULT '0',
  `displayFlags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Mask of (1 << ClubFinderClubPostingStatusFlags); moderation state',
  `type` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'ClubFinderRequestType: 0 None, 1 Guild, 2 Community, 3 All',
  `crossFaction` tinyint unsigned NOT NULL DEFAULT '0',
  `lastPosterGuid` bigint unsigned NOT NULL DEFAULT '0',
  `lastUpdatedTime` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`postingId`),
  UNIQUE KEY `idx_club` (`clubId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Club Finder guild recruitment postings';

DROP TABLE IF EXISTS `club_finder_application`;
CREATE TABLE `club_finder_application` (
  `postingId` int unsigned NOT NULL,
  `playerGuid` bigint unsigned NOT NULL,
  `comment` varchar(512) NOT NULL DEFAULT '' COMMENT 'Client buffer is char[513]',
  `specs` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Recruiting-spec bitmask the applicant offers',
  `status` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'PlayerClubRequestStatus: 1 Pending, 3 Declined, 4 Approved, 5 Joined, 7 Canceled',
  `lastUpdatedTime` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`postingId`,`playerGuid`),
  KEY `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Club Finder membership applications';
