-- ============================================================================
-- Decor Duels (#12) — housing prop-hunt / tag minigame — template table + seed
-- ----------------------------------------------------------------------------
-- SHIPPED ON BRANCH, NOT APPLIED to the central integration realm (realm rules:
-- integ_* is UNTOUCHABLE). This is the config seam read by DecorDuelMgr::Initialize.
--
-- EVIDENCE: achievement category 15574 and achievements 61792/61793/61878-61887
-- are CONFIRMED @68887. The round/queue wire, the roles' spell ids and the
-- housing minigame MAP id are CAPTURE-BLOCKED (no Decor Duel capture held), so the
-- seed row ships DISABLED with mapId = 0. DecorDuelMgr only enables itself when
-- BOTH enabled = 1 AND mapId <> 0, so it stays a harmless no-op until a capture
-- yields the real map id and the round wire is implemented. Do NOT invent the map
-- id here — set it from a Decor Duel capture, then flip enabled to 1.
-- ============================================================================

DROP TABLE IF EXISTS `decor_duel_template`;
CREATE TABLE `decor_duel_template` (
  `id`      TINYINT UNSIGNED NOT NULL DEFAULT 1  COMMENT 'singleton config row (1)',
  `mapId`   INT UNSIGNED     NOT NULL DEFAULT 0  COMMENT 'housing neighborhood minigame map id — 0 = CAPTURE-BLOCKED',
  `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 0  COMMENT '1 to enable the round seam (requires mapId <> 0)',
  `comment` VARCHAR(255)     NOT NULL DEFAULT '' COMMENT 'free-text notes',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Decor Duels (#12) housing minigame config — scaffolding';

-- Disabled scaffolding seed. mapId/enabled remain 0 until a Decor Duel capture
-- yields the map id + round wire (see DecorDuelMgr.h CAPTURE-BLOCKED notes).
INSERT INTO `decor_duel_template` (`id`, `mapId`, `enabled`, `comment`) VALUES
(1, 0, 0, 'CAPTURE-BLOCKED: map id + round/role/kit wire pending a Decor Duel capture. Achievement seam (cat 15574) is available regardless.');
