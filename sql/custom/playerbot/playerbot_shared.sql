-- Playerbot V2 shared database.
--
-- Import into the database named by the worldserver config key
-- `Playerbot.SharedDatabase` (default: playerbot). This database is NOT managed by the
-- TrinityCore SQL updater. See README.md in this directory.
--
-- Schemas reconstructed from the module's loader queries; see README.md.

CREATE DATABASE IF NOT EXISTS `playerbot` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `playerbot`;

-- Handcrafted road corridors. Applied to each map's navmesh at load time by
-- Road::ApplyCorridorsToNavmesh, which retags polygons within width/2 of the segment
-- as NAV_AREA_ROAD. No mmap regeneration is involved.
-- Loader: HandcraftedRoadStorage::Load
--   SELECT id, mapId, fromX, fromY, toX, toY, width, COALESCE(comment, ''), verified
DROP TABLE IF EXISTS `handcrafted_road`;
CREATE TABLE `handcrafted_road` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `mapId` int unsigned NOT NULL,
  `fromX` float NOT NULL COMMENT 'TrinityCore world coordinates',
  `fromY` float NOT NULL,
  `toX` float NOT NULL,
  `toY` float NOT NULL,
  `width` float NOT NULL DEFAULT '10' COMMENT 'Full corridor width in yards; half applies each side',
  `comment` varchar(255) DEFAULT NULL,
  `verified` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Authored and checked in world_editor',
  PRIMARY KEY (`id`),
  KEY `idx_map` (`mapId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Handcrafted road corridors for bot pathfinding';

-- Dungeon route waypoints, walked in `seq` order per map+difficulty.
-- Loader: DungeonScript
--   SELECT map_id, difficulty, position_x, position_y, position_z ... ORDER BY map_id, difficulty, seq
DROP TABLE IF EXISTS `playerbot_dungeon_routes`;
CREATE TABLE `playerbot_dungeon_routes` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `map_id` int unsigned NOT NULL,
  `difficulty` int unsigned NOT NULL DEFAULT '0',
  `seq` int unsigned NOT NULL COMMENT 'Order along the route',
  `position_x` float NOT NULL,
  `position_y` float NOT NULL,
  `position_z` float NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_route` (`map_id`,`difficulty`,`seq`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Bot dungeon route waypoints';

-- Authored traversal links (ledge drops, gap hops) the navmesh cannot express.
-- Only rows with verified = 1 are loaded.
-- Loader: DungeonScript
--   SELECT id, map_id, from_x, from_y, from_z, to_x, to_y, to_z, radius, bidirectional WHERE verified=1
DROP TABLE IF EXISTS `playerbot_nav_links`;
CREATE TABLE `playerbot_nav_links` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `map_id` int unsigned NOT NULL,
  `from_x` float NOT NULL,
  `from_y` float NOT NULL,
  `from_z` float NOT NULL,
  `to_x` float NOT NULL,
  `to_y` float NOT NULL,
  `to_z` float NOT NULL,
  `radius` float NOT NULL DEFAULT '2' COMMENT 'Capture radius around the from-point',
  `bidirectional` tinyint unsigned NOT NULL DEFAULT '0',
  `verified` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Only verified rows are loaded',
  PRIMARY KEY (`id`),
  KEY `idx_map` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Authored navmesh traversal links';

-- Name pool for generated bots.
-- Loader: SELECT name_id, name, gender FROM {}.playerbots_names
DROP TABLE IF EXISTS `playerbots_names`;
CREATE TABLE `playerbots_names` (
  `name_id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(12) NOT NULL COMMENT 'Character name limit is 12',
  `gender` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 male, 1 female',
  PRIMARY KEY (`name_id`),
  UNIQUE KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Name pool for generated bots';
