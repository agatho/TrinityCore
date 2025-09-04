--
-- TrinityCore Playerbot Integration - Character Database Tables
-- Date: 2025-09-03
-- Phase: 1.1 Database Infrastructure Setup
--

-- Bot name management for random bot generation
CREATE TABLE `playerbots_names` (
  `name_id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `gender` tinyint(3) unsigned NOT NULL,
  PRIMARY KEY (`name_id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Random bot name pool';

-- Guild name management for bot guilds
CREATE TABLE `playerbots_guild_names` (
  `name_id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(24) NOT NULL,
  PRIMARY KEY (`name_id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot guild name pool';

-- Arena team name management for bot arena teams
CREATE TABLE `playerbots_arena_team_names` (
  `name_id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(24) NOT NULL,
  `type` tinyint(3) NOT NULL COMMENT '2v2, 3v3, 5v5',
  PRIMARY KEY (`name_id`),
  UNIQUE KEY `name_type` (`name`, `type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot arena team name pool';