-- ============================================================================
-- "Going Postal" (housing) mail-race minigame — route config + checkpoint tables
-- ----------------------------------------------------------------------------
-- SHIPPED ON BRANCH, NOT APPLIED to the central integration realm (realm rules:
-- integ_* is UNTOUCHABLE). These are the config tables read by
-- GoingPostalMgr::Initialize (world/master).
--
-- EVIDENCE @68887 (wago.tools DB2 build 12.0.7.68887):
--   * Creature 233064 "Vaeli" <Postal Worker>                       [Creature.db2]
--   * 6 personal-best-record currencies, mapped 1:1 to (faction, route):
--       3431 Alliance Rt1 / 3432 Alliance Rt2 / 3433 Alliance Rt3
--       3434 Horde    Rt1 / 3435 Horde    Rt2 / 3436 Horde    Rt3   [CurrencyTypes.db2]
--     all under CurrencyCategory 251 "Dragon Racing UI (Hidden)".
--
-- CAPTURE-BLOCKED (documented, NOT invented here):
--   * Route CHECKPOINT COORDS / trigger GO ids (no AreaPOI/Vignette rows carry the
--     Going Postal routes @68887) → going_postal_route_checkpoint ships EMPTY.
--   * Vaeli's real world-DB gossip menu ids + her spawn (world DB, not DB2).
--   * Exact per-route time thresholds / reward tuning.
-- ============================================================================

-- ----------------------------------------------------------------------------
-- Route config: the 6 DB2-confirmed (faction, route) → personal-best currency rows.
-- The personal-best/currency MECHANISM needs no captured coords, so these ship
-- ENABLED. The in-world checkpoint progression stays inert until the (blocked)
-- checkpoint coords are seeded into going_postal_route_checkpoint below.
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `going_postal_route`;
CREATE TABLE `going_postal_route` (
  `id`         INT UNSIGNED     NOT NULL              COMMENT 'route id (PK)',
  `team`       TINYINT UNSIGNED NOT NULL DEFAULT 0    COMMENT '0 = Alliance, 1 = Horde (TeamId)',
  `routeIndex` TINYINT UNSIGNED NOT NULL DEFAULT 1    COMMENT 'route number within the faction (1..3)',
  `currencyId` INT UNSIGNED     NOT NULL DEFAULT 0    COMMENT 'DB2 personal-best currency (3431..3436)',
  `enabled`    TINYINT UNSIGNED NOT NULL DEFAULT 1    COMMENT '1 to make the route runnable',
  `name`       VARCHAR(64)      NOT NULL DEFAULT ''   COMMENT 'display name (free text)',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_team_route` (`team`, `routeIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Going Postal (housing) mail-race route config';

INSERT INTO `going_postal_route` (`id`, `team`, `routeIndex`, `currencyId`, `enabled`, `name`) VALUES
(1, 0, 1, 3431, 1, 'Alliance Route 1'),
(2, 0, 2, 3432, 1, 'Alliance Route 2'),
(3, 0, 3, 3433, 1, 'Alliance Route 3'),
(4, 1, 1, 3434, 1, 'Horde Route 1'),
(5, 1, 2, 3435, 1, 'Horde Route 2'),
(6, 1, 3, 3436, 1, 'Horde Route 3');

-- ----------------------------------------------------------------------------
-- Ordered checkpoint coords per route. SHIPS EMPTY: the real checkpoint positions
-- (and the trigger GO/area-trigger ids) are CAPTURE-BLOCKED — no AreaPOI/Vignette
-- rows carry the Going Postal routes @68887, so we do NOT invent coordinates. Seed
-- rows from a capture, then GoingPostalMgr's proximity progression starts working
-- with no code change. Example (DO NOT enable until captured):
--   INSERT INTO `going_postal_route_checkpoint`
--     (`routeId`, `seq`, `mapId`, `posX`, `posY`, `posZ`) VALUES
--     (1, 0, <mapId>, <x>, <y>, <z>),
--     (1, 1, <mapId>, <x>, <y>, <z>);
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `going_postal_route_checkpoint`;
CREATE TABLE `going_postal_route_checkpoint` (
  `routeId` INT UNSIGNED     NOT NULL              COMMENT 'FK → going_postal_route.id',
  `seq`     INT UNSIGNED     NOT NULL DEFAULT 0    COMMENT '0-based checkpoint order',
  `mapId`   INT UNSIGNED     NOT NULL DEFAULT 0    COMMENT 'map the checkpoint lives on (CAPTURE-BLOCKED)',
  `posX`    FLOAT            NOT NULL DEFAULT 0     COMMENT 'checkpoint X (CAPTURE-BLOCKED)',
  `posY`    FLOAT            NOT NULL DEFAULT 0     COMMENT 'checkpoint Y (CAPTURE-BLOCKED)',
  `posZ`    FLOAT            NOT NULL DEFAULT 0     COMMENT 'checkpoint Z (CAPTURE-BLOCKED)',
  PRIMARY KEY (`routeId`, `seq`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Going Postal route checkpoints — EMPTY, coords CAPTURE-BLOCKED';
-- (intentionally no INSERTs — checkpoint coords are CAPTURE-BLOCKED)
