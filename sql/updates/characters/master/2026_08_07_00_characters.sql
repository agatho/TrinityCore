--
-- Item upgrade per-slot high-watermark (highest item level a slot class has paid crests to; drives the
-- crest waiver and the client's ItemUpgradeHighWatermark discount display).
--
CREATE TABLE IF NOT EXISTS `character_item_upgrade_watermark` (
  `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Global Unique Identifier',
  `slotClass` TINYINT UNSIGNED NOT NULL COMMENT 'Watermark slot class index (0-16)',
  `itemLevel` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`,`slotClass`),
  CONSTRAINT `fk_char_item_upgrade_watermark_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character item upgrade watermarks';
