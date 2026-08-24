--
-- Recent Allies: players you recently grouped with, per-character notes, and the "see my location" toggle.
--
DROP TABLE IF EXISTS `character_recent_allies`;
CREATE TABLE `character_recent_allies` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `allyGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Recent Ally Global Unique Identifier',
  `allyAccount` int unsigned NOT NULL DEFAULT '0' COMMENT 'Recent Ally WoW Account Id',
  `lastGrouped` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix time last grouped',
  `note` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Personal Note',
  PRIMARY KEY (`ownerGuid`,`allyGuid`),
  KEY `allyGuid` (`allyGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

DROP TABLE IF EXISTS `character_recent_ally_settings`;
CREATE TABLE `character_recent_ally_settings` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `allowSeeLocation` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Allow recent allies to see my location',
  PRIMARY KEY (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
