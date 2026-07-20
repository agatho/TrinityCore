-- ===========================================================================
-- DEPRECATED / MOVED (2026-07-02)
-- ===========================================================================
-- The dungeon route_waypoints table has MOVED to the shared playerbot database
-- (Playerbot.SharedDatabase) because these waypoints are static, map-derived nav
-- data identical across every realm -- the same rationale as handcrafted_road.
-- See: sql/shared/0001_dungeon_route_waypoints.sql (the authoritative schema),
-- read at runtime by DungeonScriptMgr::LoadGeneratedRoutes via CharacterDatabase
-- with a {SharedDb()}. qualifier, and written by tools/gen_dungeon_routes.py.
--
-- This world-DB table is no longer read. It is left creatable (harmless) only so
-- an already-migrated realm's orphaned copy is not surprising; it can be dropped:
--   DROP TABLE IF EXISTS `playerbot_dungeon_routes`;
-- ===========================================================================

CREATE TABLE IF NOT EXISTS `playerbot_dungeon_routes` (
  `map_id`     SMALLINT UNSIGNED NOT NULL,
  `difficulty` TINYINT  UNSIGNED NOT NULL DEFAULT 0,
  `seq`        SMALLINT UNSIGNED NOT NULL,
  `position_x` FLOAT NOT NULL,
  `position_y` FLOAT NOT NULL,
  `position_z` FLOAT NOT NULL,
  PRIMARY KEY (`map_id`, `difficulty`, `seq`),
  KEY `map_idx` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='DEPRECATED: moved to the shared playerbot DB (see sql/shared/0001_dungeon_route_waypoints.sql)';
