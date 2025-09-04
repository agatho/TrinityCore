/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!50503 SET NAMES utf8mb4 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

--
-- TrinityCore Playerbots Database Schema
-- Ported from mod-playerbots for AzerothCore
-- Date: 2025-09-03
--

-- Core bot management and AI tables
CREATE TABLE `playerbots_custom_strategy` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `idx` int(11) DEFAULT NULL,
  `owner` int(11) DEFAULT NULL,
  `action_line` varchar(1024) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `name` (`name`),
  KEY `owner` (`owner`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Custom AI strategies for bots';

CREATE TABLE `playerbots_db_store` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `guid` int(11) NOT NULL,
  `key` varchar(32) NOT NULL,
  `value` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `guid_key` (`guid`, `key`),
  KEY `guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Key-value store for bot configuration';

-- Random bot management
CREATE TABLE `playerbots_random_bots` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `owner` int(11) NOT NULL,
  `bot` int(11) NOT NULL,
  `time` int(11) NOT NULL,
  `validIn` int(11) DEFAULT NULL,
  `event` varchar(45) DEFAULT NULL,
  `value` int(11) DEFAULT NULL,
  `data` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `owner` (`owner`),
  KEY `bot` (`bot`),
  KEY `event` (`event`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Random bot state management';

-- Guild task management
CREATE TABLE `playerbots_guild_tasks` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `owner` int(11) NOT NULL,
  `guildid` int(11) NOT NULL,
  `time` int(11) NOT NULL,
  `validIn` int(11) DEFAULT NULL,
  `type` varchar(45) DEFAULT NULL,
  `value` int(11) DEFAULT NULL,
  `data` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `owner` (`owner`),
  KEY `guildid` (`guildid`),
  KEY `type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot guild task tracking';

-- Travel system tables
CREATE TABLE `playerbots_travelnode` (
  `id` mediumint(8) NOT NULL,
  `name` varchar(1024) NOT NULL,
  `map_id` mediumint(8) NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `linked` tinyint(2) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `map_id` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Playerbot travel waypoints';

CREATE TABLE `playerbots_travelnode_link` (
  `node_id` mediumint(8) NOT NULL,
  `to_node_id` mediumint(8) NOT NULL,
  `type` tinyint(3) NOT NULL,
  `object` mediumint(8) NOT NULL,
  `distance` float NOT NULL,
  `swim_distance` float NOT NULL,
  `extra_cost` float NOT NULL,
  `calculated` tinyint(1) NOT NULL,
  `max_creature_0` tinyint(2) NOT NULL,
  `max_creature_1` tinyint(2) NOT NULL,
  `max_creature_2` tinyint(2) NOT NULL,
  PRIMARY KEY (`node_id`, `to_node_id`),
  KEY `to_node_id` (`to_node_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Travel connections between nodes';

CREATE TABLE `playerbots_travelnode_path` (
  `node_id` mediumint(8) NOT NULL,
  `to_node_id` mediumint(8) NOT NULL,
  `nr` mediumint(8) NOT NULL,
  `map_id` mediumint(8) NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  PRIMARY KEY (`node_id`, `to_node_id`, `nr`),
  KEY `map_id` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Detailed travel paths between nodes';

-- Equipment and item management
CREATE TABLE `playerbots_equip_cache` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `clazz` tinyint(3) NOT NULL,
  `lvl` int(11) NOT NULL,
  `slot` tinyint(8) NOT NULL,
  `quality` int(11) NOT NULL,
  `item` int(11) NOT NULL,
  `ilvl` int(11) DEFAULT NULL,
  `subclazz` int(11) DEFAULT NULL,
  `price` bigint(20) DEFAULT NULL,
  `spec` tinyint(3) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `clazz_level` (`clazz`, `lvl`),
  KEY `item` (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Equipment optimization cache';

CREATE TABLE `playerbots_item_info_cache` (
  `id` int(11) NOT NULL,
  `quality` int(11) DEFAULT NULL,
  `slot` int(11) DEFAULT NULL,
  `source` bigint(20) DEFAULT NULL,
  `sourceId` bigint(20) DEFAULT NULL,
  `team` bigint(20) DEFAULT NULL,
  `faction` bigint(20) DEFAULT NULL,
  `factionRepRank` bigint(20) DEFAULT NULL,
  `minLevel` bigint(20) DEFAULT NULL,
  `scale_1` bigint(20) DEFAULT NULL,
  `scale_2` bigint(20) DEFAULT NULL,
  `scale_3` bigint(20) DEFAULT NULL,
  `scale_4` bigint(20) DEFAULT NULL,
  `scale_5` bigint(20) DEFAULT NULL,
  `scale_6` bigint(20) DEFAULT NULL,
  `scale_7` bigint(20) DEFAULT NULL,
  `scale_8` bigint(20) DEFAULT NULL,
  `scale_9` bigint(20) DEFAULT NULL,
  `scale_10` bigint(20) DEFAULT NULL,
  `scale_11` bigint(20) DEFAULT NULL,
  `scale_12` bigint(20) DEFAULT NULL,
  `scale_13` bigint(20) DEFAULT NULL,
  `scale_14` bigint(20) DEFAULT NULL,
  `scale_15` bigint(20) DEFAULT NULL,
  `scale_16` bigint(20) DEFAULT NULL,
  `scale_17` bigint(20) DEFAULT NULL,
  `scale_18` bigint(20) DEFAULT NULL,
  `scale_19` bigint(20) DEFAULT NULL,
  `scale_20` bigint(20) DEFAULT NULL,
  `scale_21` bigint(20) DEFAULT NULL,
  `scale_22` bigint(20) DEFAULT NULL,
  `scale_23` bigint(20) DEFAULT NULL,
  `scale_24` bigint(20) DEFAULT NULL,
  `scale_25` bigint(20) DEFAULT NULL,
  `scale_26` bigint(20) DEFAULT NULL,
  `scale_27` bigint(20) DEFAULT NULL,
  `scale_28` bigint(20) DEFAULT NULL,
  `scale_29` bigint(20) DEFAULT NULL,
  `scale_30` bigint(20) DEFAULT NULL,
  `scale_31` bigint(20) DEFAULT NULL,
  `scale_32` bigint(20) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Item information and stat scaling cache';

CREATE TABLE `playerbots_rarity_cache` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `item` int(11) NOT NULL,
  `rarity` float NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `item` (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Item rarity cache for bot decisions';

CREATE TABLE `playerbots_rnditem_cache` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `lvl` int(11) NOT NULL,
  `type` int(11) NOT NULL,
  `item` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `lvl_type` (`lvl`, `type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Random item cache by level and type';

-- Communication and text systems
CREATE TABLE `playerbots_speech` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `text` varchar(1024) NOT NULL,
  `type` varchar(10) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `name` (`name`),
  KEY `type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot speech patterns';

CREATE TABLE `playerbots_speech_probability` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `probability` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Speech probability weights';

CREATE TABLE `ai_playerbot_texts` (
  `id` smallint(20) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL COMMENT 'name - used in strategies/code as filter',
  `text` varchar(1024) NOT NULL COMMENT 'text',
  `say_type` tinyint(3) NOT NULL DEFAULT 0 COMMENT '0 - say, 1 - yell',
  `reply_type` tinyint(3) NOT NULL DEFAULT 0 COMMENT 'if > 0 then can be filtered as a response to chat',
  `text_loc1` varchar(1024) NOT NULL DEFAULT '',
  `text_loc2` varchar(1024) NOT NULL DEFAULT '',
  `text_loc3` varchar(1024) NOT NULL DEFAULT '',
  `text_loc4` varchar(1024) NOT NULL DEFAULT '',
  `text_loc5` varchar(1024) NOT NULL DEFAULT '',
  `text_loc6` varchar(1024) NOT NULL DEFAULT '',
  `text_loc7` varchar(1024) NOT NULL DEFAULT '',
  `text_loc8` varchar(1024) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Localized bot text responses';

CREATE TABLE `ai_playerbot_texts_chance` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `probability` bigint(20) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Text response probability';

-- Enhancement and configuration
CREATE TABLE `playerbots_enchants` (
  `class` tinyint(3) NOT NULL,
  `spec` tinyint(3) NOT NULL,
  `spellid` int(11) NOT NULL,
  `slotid` tinyint(3) DEFAULT 1,
  `name` varchar(255) NOT NULL COMMENT 'name of the enchant',
  PRIMARY KEY (`class`, `spec`, `spellid`, `slotid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot enchantment recommendations';

CREATE TABLE `playerbots_weightscales` (
  `id` int(32) NOT NULL AUTO_INCREMENT,
  `name` varchar(32) NOT NULL,
  `class` tinyint(3) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_class` (`name`, `class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Stat weight scale definitions';

CREATE TABLE `playerbots_weightscale_data` (
  `id` int(32) NOT NULL,
  `field` varchar(18) NOT NULL,
  `val` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`id`, `field`),
  KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Stat weight values';

-- Utility and cache tables
CREATE TABLE `playerbots_tele_cache` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `level` tinyint(3) NOT NULL,
  `map_id` mediumint(8) NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `alliance` tinyint(1) DEFAULT 1,
  `horde` tinyint(1) DEFAULT 1,
  PRIMARY KEY (`id`),
  KEY `map_id` (`map_id`),
  KEY `level` (`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Teleportation location cache';

CREATE TABLE `playerbots_preferred_mounts` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `guid` int(11) NOT NULL,
  `type` tinyint(3) NOT NULL COMMENT '0: Ground, 1: Flying',
  `spellid` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `guid` (`guid`),
  KEY `type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot mount preferences';

-- Account management (modern additions)
CREATE TABLE `playerbots_account_keys` (
  `account_id` int(11) NOT NULL,
  `security_key` varchar(255) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot account security keys';

CREATE TABLE `playerbots_account_links` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `account_id` int(11) NOT NULL,
  `linked_account_id` int(11) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `account_link` (`account_id`, `linked_account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Account linking for bot management';

CREATE TABLE `playerbots_account_type` (
  `account_id` int(11) unsigned NOT NULL,
  `account_type` tinyint(3) unsigned NOT NULL DEFAULT 0 COMMENT '0 = unassigned, 1 = RNDbot, 2 = AddClass',
  `assignment_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot account type assignments';

-- Dungeon system
CREATE TABLE `playerbots_dungeon_suggestion_definition` (
  `id` tinyint(3) NOT NULL AUTO_INCREMENT,
  `slug` varchar(255) NOT NULL,
  `name` varchar(255) NOT NULL,
  `expansion` tinyint(1) NOT NULL,
  `difficulty` tinyint(1) NOT NULL,
  `min_level` tinyint(3) DEFAULT NULL,
  `max_level` tinyint(3) DEFAULT NULL,
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `slug` (`slug`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Dungeon suggestion definitions';

-- Database version tracking (TrinityCore style)
CREATE TABLE `version_db_playerbots` (
  `sql_rev` varchar(100) NOT NULL,
  `required_rev` varchar(100) DEFAULT NULL,
  `date` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`sql_rev`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Database version tracking';

CREATE TABLE `updates_playerbots` (
  `name` varchar(200) NOT NULL COMMENT 'filename with extension of the update.',
  `hash` char(40) DEFAULT '' COMMENT 'sha1 hash of the sql file.',
  `state` enum('RELEASED','ARCHIVED','CUSTOM') NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if an update is released or archived.',
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'timestamp when the query was applied.',
  `speed` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'time the query takes to apply in ms.',
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='List of all applied playerbot updates';

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;