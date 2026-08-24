--
-- These tables were only ever added to sql/base/characters_database.sql, which the updater applies
-- solely when creating a database from scratch. Any server upgrading an existing database therefore
-- never got them, and the features silently failed to prepare their statements at startup:
-- Covenant, Great Vault / content tracking, Crafting Orders and Recent Allies.
--
CREATE TABLE IF NOT EXISTS `character_content_tracking` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `targetType` int NOT NULL DEFAULT '0' COMMENT 'ContentTrackingTargetType of the tracked entry',
  `targetId` int NOT NULL DEFAULT '0' COMMENT 'Id of the tracked target within its type',
  `collectableSourceInfoId` int NOT NULL DEFAULT '0' COMMENT 'Client-resolved CollectableSourceInfo id',
  PRIMARY KEY (`ownerGuid`,`targetType`,`targetId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

CREATE TABLE IF NOT EXISTS `character_covenant_renown` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `covenantId` int unsigned NOT NULL DEFAULT '0',
  `grantedLevel` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`covenantId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Highest covenant renown level whose rewards were granted';

CREATE TABLE IF NOT EXISTS `character_recent_allies` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `allyGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Recent Ally Global Unique Identifier',
  `allyAccount` int unsigned NOT NULL DEFAULT '0' COMMENT 'Recent Ally WoW Account Id',
  `lastGrouped` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix time last grouped',
  `note` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Personal Note',
  PRIMARY KEY (`ownerGuid`,`allyGuid`),
  KEY `allyGuid` (`allyGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

CREATE TABLE IF NOT EXISTS `character_recent_ally_settings` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `allowSeeLocation` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Allow recent allies to see my location',
  PRIMARY KEY (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

CREATE TABLE IF NOT EXISTS `character_soulbind_conduit_sockets` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `garrTalentId` int unsigned NOT NULL DEFAULT '0',
  `conduitId` int unsigned NOT NULL DEFAULT '0',
  `garrTalentTreeId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`garrTalentId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Conduits socketed into soulbind tree nodes';

CREATE TABLE IF NOT EXISTS `character_soulbind_conduits` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `conduitId` int unsigned NOT NULL DEFAULT '0',
  `rankIndex` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`conduitId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player soulbind conduit collection (owned conduit -> rank)';

CREATE TABLE IF NOT EXISTS `character_weekly_reward_activity` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `activityType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Great Vault row: 0 Dungeon, 1 Raid, 2 World',
  `period` int unsigned NOT NULL DEFAULT '0' COMMENT 'Weekly period index this activity belongs to',
  `count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Qualifying completions this period',
  `bestLevel` int unsigned NOT NULL DEFAULT '0' COMMENT 'Best difficulty/keystone level this period',
  PRIMARY KEY (`ownerGuid`,`activityType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

CREATE TABLE IF NOT EXISTS `character_weekly_reward_state` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `claimedPeriod` int unsigned NOT NULL DEFAULT '0' COMMENT 'Last weekly period a reward was claimed',
  PRIMARY KEY (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';

CREATE TABLE IF NOT EXISTS `crafting_order_reagents` (
  `OrderID` bigint unsigned NOT NULL,
  `Slot` tinyint unsigned NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `CurrencyID` int NOT NULL DEFAULT '0',
  `Quantity` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`OrderID`,`Slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `crafting_orders` (
  `OrderID` bigint unsigned NOT NULL,
  `SkillLineAbilityID` int NOT NULL DEFAULT '0',
  `OrderState` tinyint NOT NULL DEFAULT '0',
  `OrderType` tinyint unsigned NOT NULL DEFAULT '0',
  `MinQuality` int unsigned NOT NULL DEFAULT '0',
  `EndDate` bigint NOT NULL DEFAULT '0',
  `ClaimEndDate` bigint NOT NULL DEFAULT '0',
  `TipAmount` bigint unsigned NOT NULL DEFAULT '0',
  `HouseCutAmount` bigint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `CustomerGuid` bigint unsigned NOT NULL DEFAULT '0',
  `CrafterGuid` bigint unsigned NOT NULL DEFAULT '0',
  `CustomerAccountId` int unsigned NOT NULL DEFAULT '0',
  `CustomerNotes` text,
  PRIMARY KEY (`OrderID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
