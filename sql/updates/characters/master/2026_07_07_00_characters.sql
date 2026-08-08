--
-- Great Vault / Weekly Rewards: per-character weekly activity tracking and claim state.
--
DROP TABLE IF EXISTS `character_weekly_reward_activity`;
CREATE TABLE `character_weekly_reward_activity` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `activityType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Great Vault row: 0 Dungeon, 1 Raid, 2 World',
  `period` int unsigned NOT NULL DEFAULT '0' COMMENT 'Weekly period index this activity belongs to',
  `count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Qualifying completions this period',
  `bestLevel` int unsigned NOT NULL DEFAULT '0' COMMENT 'Best difficulty/keystone level this period',
  PRIMARY KEY (`ownerGuid`,`activityType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

DROP TABLE IF EXISTS `character_weekly_reward_state`;
CREATE TABLE `character_weekly_reward_state` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `claimedPeriod` int unsigned NOT NULL DEFAULT '0' COMMENT 'Last weekly period a reward was claimed',
  PRIMARY KEY (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
