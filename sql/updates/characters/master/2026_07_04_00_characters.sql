--
-- Mythic+ (Challenge Mode): per-character best-run storage.
--
CREATE TABLE IF NOT EXISTS `character_mythic_plus` (
  `guid` bigint unsigned NOT NULL DEFAULT 0 COMMENT 'Global Unique Identifier',
  `challengeModeId` int unsigned NOT NULL DEFAULT 0 COMMENT 'MapChallengeMode.db2 ID',
  `level` int unsigned NOT NULL DEFAULT 0 COMMENT 'Keystone level of the best run',
  `durationMs` int unsigned NOT NULL DEFAULT 0 COMMENT 'Effective run time in milliseconds (incl. death penalty)',
  `deaths` int unsigned NOT NULL DEFAULT 0,
  `completionDate` bigint NOT NULL DEFAULT 0 COMMENT 'Unix time of the run',
  `score` float NOT NULL DEFAULT 0,
  `affix1` int unsigned NOT NULL DEFAULT 0,
  `affix2` int unsigned NOT NULL DEFAULT 0,
  `affix3` int unsigned NOT NULL DEFAULT 0,
  `affix4` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`,`challengeModeId`),
  CONSTRAINT `fk_character_mythic_plus_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Mythic Keystone best runs';
