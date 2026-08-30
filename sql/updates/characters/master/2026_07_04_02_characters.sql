--
-- Great Vault claim state: one reward per weekly reset (blocks a second claim until the reset rolls).
--
DROP TABLE IF EXISTS `character_mythic_plus_vault`;
CREATE TABLE `character_mythic_plus_vault` (
  `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Global Unique Identifier',
  `claimedResetTime` BIGINT NOT NULL DEFAULT 0 COMMENT 'Weekly reset boundary the vault reward was claimed for',
  PRIMARY KEY (`guid`),
  CONSTRAINT `fk_character_mythic_plus_vault_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character Great Vault weekly claim state';
