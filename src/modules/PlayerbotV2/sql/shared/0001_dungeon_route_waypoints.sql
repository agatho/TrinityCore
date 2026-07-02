-- ===========================================================================
-- PlayerbotV2: dungeon route_waypoints table (cross-dungeon far-boss routing)
-- Target: the SHARED playerbot database (Playerbot.SharedDatabase, default
--         "playerbot"; e.g. "wowc_playerbot"). Apply this file while connected
--         to that schema -- it is NOT part of TrinityCore's world/char/login
--         updater (same as handcrafted_road / playerbots_names). Date: 2026-07-02
-- ===========================================================================
--
-- Auto-generated on-navmesh corridor waypoints (entrance -> bosses chain) that
-- let the tank far-boss advance route WINDING corridors longer than the ~74-poly
-- (~292y) PathGenerator cap. Only the Deadmines script hand-authored its
-- route_waypoints; every other dungeon relies on these generated rows.
--
-- WHY THE SHARED DB: these waypoints are STATIC, map-derived nav data --
-- chain-pathfound from the navmesh, identical across every realm running the
-- same maps, exactly like the shared handcrafted_road table. One shared copy for
-- all realms instead of duplicating them in each realm's world DB. The runtime
-- (DungeonScriptMgr::LoadGeneratedRoutes) reads them via CharacterDatabase with a
-- {SharedDb()}. schema qualifier; the offline generator
-- (src/modules/PlayerbotV2/tools/gen_dungeon_routes.py, ROUTE_DB env) writes here.
-- This migration only creates the empty table; run the generator to populate it.
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
