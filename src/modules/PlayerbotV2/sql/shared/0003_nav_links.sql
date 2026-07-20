-- ===========================================================================
-- PlayerbotV2: DB-authored traversal links (playerbot_nav_links)
-- Target: the SHARED playerbot database (Playerbot.SharedDatabase, e.g.
--         "wowc_playerbot") — NOT part of TC's world/char/login updater.
--         Date: 2026-07-03
-- ===========================================================================
--
-- A traversal link is a human-verified "from A you can just MOVE to B" edge:
-- a real geometric split players cross by jumping, or an unmeshed-but-walkable
-- stretch. It is the BEHAVIORAL alternative to baking off-mesh connections
-- into the binary mmap tiles: shipped mmaps stay byte-identical to stock TC
-- data, rows are editor-authored, hot-reloadable (.playerbot reloadroutes),
-- reversible per-row, and auditable (verified/created_by, like
-- handcrafted_road).
--
-- Runtime: the dungeon stepper (DungeonTargetReachableAndStep) consumes links
-- on its reject paths — when the on-mesh path dead-ends at a link mouth the
-- bot walks to the mouth, then commits the crossing through the existing
-- set_dungeon_cross/DungeonHonorCross machinery, executed as a straight
-- no-pathfind MovePoint spline ("just move, don't think").
-- ONLY verified=1 rows are loaded — a row must be confirmed in-game (or by a
-- validated editor flow) before it may steer bots.
-- ===========================================================================

CREATE TABLE IF NOT EXISTS `playerbot_nav_links` (
  `id`            INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `map_id`        SMALLINT UNSIGNED NOT NULL,
  `from_x`        FLOAT NOT NULL,
  `from_y`        FLOAT NOT NULL,
  `from_z`        FLOAT NOT NULL,
  `to_x`          FLOAT NOT NULL,
  `to_y`          FLOAT NOT NULL,
  `to_z`          FLOAT NOT NULL,
  `radius`        FLOAT NOT NULL DEFAULT 12 COMMENT 'how close (3D, yards) a bot must be to a mouth to use the link',
  `bidirectional` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `kind`          VARCHAR(16) NOT NULL DEFAULT 'jump' COMMENT 'jump | walk (semantic only; both execute as a direct move)',
  `comment`       VARCHAR(255) DEFAULT NULL,
  `verified`      TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'only verified=1 rows are loaded',
  `created_by`    VARCHAR(64) DEFAULT NULL,
  `created_at`    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `map_idx` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='PlayerbotV2 human-verified traversal links (behavioral off-mesh alternative; see DungeonScript.h NavLink)';

-- Wailing Caverns: cliff-walk jump-split on the route to Lord Serpentis.
-- Owner-verified in-game 2026-07-03 (real split; players jump/walk straight
-- across; far vertex = owner's standing position on the ledge).
INSERT INTO `playerbot_nav_links`
  (`map_id`, `from_x`, `from_y`, `from_z`, `to_x`, `to_y`, `to_z`,
   `radius`, `bidirectional`, `kind`, `comment`, `verified`, `created_by`)
VALUES
  (43, -289.07, 3.20, -63.96, -288.23, -6.15, -58.98,
   12, 1, 'jump', 'WC Serpentis cliff-walk jump-split (owner-verified 2026-07-03)', 1, 'owner');
