--
-- Great Vault weekly Mythic+ run tracking (unlocks the 1/4/8-run reward slots).
--
DROP TABLE IF EXISTS `character_mythic_plus_weekly`;
CREATE TABLE `character_mythic_plus_weekly` (
  `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Global Unique Identifier',
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `challengeModeId` INT UNSIGNED NOT NULL DEFAULT 0,
  `level` INT UNSIGNED NOT NULL DEFAULT 0,
  `completionDate` BIGINT NOT NULL DEFAULT 0,
  `resetTime` BIGINT NOT NULL DEFAULT 0 COMMENT 'Weekly reset boundary these runs belong to',
  PRIMARY KEY (`id`),
  KEY `idx_guid` (`guid`),
  CONSTRAINT `fk_character_mythic_plus_weekly_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character Mythic+ runs completed this Great Vault week';
