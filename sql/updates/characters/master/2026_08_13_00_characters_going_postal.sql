-- ============================================================================
-- "Going Postal" (housing) mail-race — per-character personal-best persistence
-- ----------------------------------------------------------------------------
-- SHIPPED ON BRANCH, NOT APPLIED to the central integration realm (realm rules:
-- integ_* is UNTOUCHABLE). Written/read by GoingPostalMgr (characters DB).
--
-- Stores the authoritative personal-best time (milliseconds, lower = better) per
-- (character, route). GoingPostalMgr also mirrors the record onto the DB2
-- personal-best-record currency (3431..3436) via stock ModifyCurrency; this table
-- is the source of truth for the best-time comparison. The manager tolerates this
-- table being absent (realm-safe), so applying it is optional for a fresh realm.
-- ============================================================================

DROP TABLE IF EXISTS `character_going_postal`;
CREATE TABLE `character_going_postal` (
  `guid`       BIGINT UNSIGNED NOT NULL             COMMENT 'character low GUID',
  `routeId`    INT UNSIGNED    NOT NULL             COMMENT 'going_postal_route.id',
  `bestTimeMs` INT UNSIGNED    NOT NULL DEFAULT 0   COMMENT 'personal best race time in ms (lower = better)',
  `updateTime` INT UNSIGNED    NOT NULL DEFAULT 0   COMMENT 'UNIX time the best was set',
  PRIMARY KEY (`guid`, `routeId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Going Postal (housing) mail-race personal bests';
