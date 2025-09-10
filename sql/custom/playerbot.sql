-- =====================================================================================
-- TRINITYCORE PLAYERBOT DATABASE SCHEMA
-- Created during PHASE 1 API Migration
-- Follows TrinityCore database patterns and modern standards
-- =====================================================================================

-- Core playerbot configuration and management
CREATE TABLE IF NOT EXISTS `playerbots_ai_config` (
  `entry` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int(11) unsigned NOT NULL DEFAULT '0',
  `strategy` varchar(255) NOT NULL DEFAULT '',
  `value` text,
  `created` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`entry`),
  UNIQUE KEY `idx_bot_strategy` (`bot_guid`, `strategy`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Playerbot AI configuration and strategy settings';

-- Playerbot social relationships (moved from characters database per user request)
CREATE TABLE IF NOT EXISTS `playerbots_social_relations` (
  `entry` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int(11) unsigned NOT NULL DEFAULT '0',
  `target_guid` int(11) unsigned NOT NULL DEFAULT '0',
  `relation_type` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0=friend, 1=ignore, 2=enemy, 3=guild_member',
  `note` varchar(255) DEFAULT NULL,
  `created` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`entry`),
  UNIQUE KEY `idx_bot_target_relation` (`bot_guid`, `target_guid`, `relation_type`),
  KEY `idx_bot_guid` (`bot_guid`),
  KEY `idx_target_guid` (`target_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Playerbot social relationships and interactions';

-- Playerbot action history and learning data
CREATE TABLE IF NOT EXISTS `playerbots_action_history` (
  `entry` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int(11) unsigned NOT NULL DEFAULT '0',
  `action_name` varchar(128) NOT NULL,
  `context_data` text,
  `success` tinyint(1) NOT NULL DEFAULT '0',
  `execution_time_ms` int(11) unsigned DEFAULT '0',
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`entry`),
  KEY `idx_bot_action` (`bot_guid`, `action_name`),
  KEY `idx_timestamp` (`timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Playerbot action execution history for learning';

-- Playerbot quest management and progress
CREATE TABLE IF NOT EXISTS `playerbots_quest_data` (
  `entry` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int(11) unsigned NOT NULL DEFAULT '0',
  `quest_id` int(11) unsigned NOT NULL DEFAULT '0',
  `priority` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `status` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0=available, 1=active, 2=completed, 3=abandoned',
  `objective_progress` text,
  `created` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`entry`),
  UNIQUE KEY `idx_bot_quest` (`bot_guid`, `quest_id`),
  KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Playerbot quest management and objective tracking';

-- Playerbot memory and caching system
CREATE TABLE IF NOT EXISTS `playerbots_memory_cache` (
  `entry` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int(11) unsigned NOT NULL DEFAULT '0',
  `memory_key` varchar(255) NOT NULL,
  `memory_value` longtext,
  `expiry_time` timestamp NULL DEFAULT NULL,
  `created` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `accessed` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`entry`),
  UNIQUE KEY `idx_bot_memory_key` (`bot_guid`, `memory_key`),
  KEY `idx_expiry` (`expiry_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Playerbot memory and caching for persistent AI state';

-- CRITICAL: Missing table required by PlayerbotSocialManager.cpp
-- Table name and column names must match code exactly: botGuid, playerGuid (camelCase)
CREATE TABLE IF NOT EXISTS `playerbot_social_relations` (
  `botGuid` int(11) unsigned NOT NULL,
  `playerGuid` int(11) unsigned NOT NULL,
  `playerName` varchar(255) NOT NULL DEFAULT '',
  `relationType` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0=neutral, 1=friend, 2=ignore, 3=enemy',
  `trustLevel` tinyint(3) unsigned NOT NULL DEFAULT '50' COMMENT 'Trust level 0-100',
  `interactionCount` int(11) unsigned NOT NULL DEFAULT '0',
  `positiveInteractions` int(11) unsigned NOT NULL DEFAULT '0',
  `negativeInteractions` int(11) unsigned NOT NULL DEFAULT '0',
  `lastInteractionTime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `notes` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`botGuid`, `playerGuid`),
  KEY `idx_bot_relation` (`botGuid`, `relationType`),
  KEY `idx_player` (`playerGuid`),
  KEY `idx_interaction_time` (`lastInteractionTime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Social relationships for playerbot AI - CRITICAL for social system functionality';