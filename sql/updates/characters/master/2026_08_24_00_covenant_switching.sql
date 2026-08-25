-- Covenant switching / reset (spell 338503 "Reset Covenant").
--
-- A switch must never cost a character anything belonging to a covenant it may return to. Renown, reservoir
-- anima, sanctum talents, companions, conduits and sockets are all already stored per covenant and untouched.
-- The one piece with nowhere to live was WHICH SOULBIND a covenant was using: character_covenant is single-valued
-- (active covenant/soulbind), so leaving a covenant would discard its soulbind choice. This table remembers it per
-- covenant, and because a row is written for every covenant pledged to - even before a soulbind is picked - it
-- doubles as the "covenants ever joined" set that tells a switch apart from a first pledge.
--
-- Idempotent: safe to re-run.

CREATE TABLE IF NOT EXISTS `character_covenant_soulbind` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `covenantId` int unsigned NOT NULL DEFAULT '0',
  `soulbindId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`covenantId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Soulbind last active per covenant (also the set of covenants ever joined)';

-- Seed characters already in a covenant so their pledge is recognised as "joined" and their current soulbind
-- survives their first switch. INSERT IGNORE keeps a re-run from clobbering anything since recorded.
INSERT IGNORE INTO `character_covenant_soulbind` (`guid`, `covenantId`, `soulbindId`)
  SELECT `guid`, `covenantId`, `soulbindId` FROM `character_covenant` WHERE `covenantId` <> 0;
