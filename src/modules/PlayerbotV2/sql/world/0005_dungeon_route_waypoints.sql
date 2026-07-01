-- ===========================================================================
-- PlayerbotV2: dungeon route_waypoints table (cross-dungeon far-boss routing)
-- Target: WorldDatabase (wc_world). Date: 2026-07-01
-- ===========================================================================
--
-- Auto-generated on-navmesh corridor waypoints (entrance -> bosses chain) that
-- let the tank far-boss advance route WINDING corridors longer than the ~74-poly
-- (~292y) PathGenerator cap. Only the Deadmines script hand-authored its
-- route_waypoints; every other dungeon had none, so the advance stalled at
-- reach=0 on winding routes (Wailing Caverns, etc.). DungeonScriptMgr::
-- LoadGeneratedRoutes() loads this table at module init and GetAdvice() injects
-- the rows for a map ONLY when that dungeon's script left route_waypoints empty
-- (authored chains still win).
--
-- Rows are produced offline by src/modules/PlayerbotV2/tools/gen_dungeon_routes.py
-- (chain-pathfinds the navmesh with mmap_probe). This migration only creates the
-- empty table; run the generator to populate it (it writes INSERTs directly).
-- ===========================================================================

CREATE TABLE IF NOT EXISTS `playerbot_dungeon_routes` (
  `map_id`     SMALLINT UNSIGNED NOT NULL,
  `difficulty` TINYINT  UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 = any difficulty (geometry is difficulty-invariant)',
  `seq`        SMALLINT UNSIGNED NOT NULL COMMENT 'ordered chain index: entrance -> boss1 -> boss2 ...',
  `position_x` FLOAT NOT NULL,
  `position_y` FLOAT NOT NULL,
  `position_z` FLOAT NOT NULL,
  PRIMARY KEY (`map_id`, `difficulty`, `seq`),
  KEY `map_idx` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='PlayerbotV2 auto-generated dungeon route waypoints (navmesh corridor chain for far-boss advance)';
