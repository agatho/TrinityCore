-- ============================================================================
-- Guarded 2026-07-21 so this file can be replayed against a baseline that is
-- already past it (e.g. WCDB v1.4, whose dump ships an EMPTY `updates` ledger,
-- so TC replays the whole update history on first start).
-- Unguarded it died on a duplicate//missing column; MySQL stops at the first
-- error, so the rest of the file never ran, and TC aborts startup because it
-- treats any non-zero mysql exit as a failed update.
-- ============================================================================

--

-- guarded: only add while `TitleText` is absent

SET @g := (SELECT COUNT(*)=0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='trait_tree' AND COLUMN_NAME='TitleText');
SET @s := IF(@g, 'ALTER TABLE `trait_tree` ADD COLUMN `TitleText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL FIRST', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while `Unused1000_1` is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='trait_tree' AND COLUMN_NAME='Unused1000_1');
SET @s := IF(@g, 'ALTER TABLE `trait_tree` CHANGE `Unused1000_1` `BaseNodeGroup` int NOT NULL DEFAULT 0 AFTER `TraitSystemID`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while `Unused1000_2` is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='trait_tree' AND COLUMN_NAME='Unused1000_2');
SET @s := IF(@g, 'ALTER TABLE `trait_tree` CHANGE `Unused1000_2` `MinZoom` float NOT NULL DEFAULT 0 AFTER `Flags`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only rename while `Unused1000_3` is still present

SET @g := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='trait_tree' AND COLUMN_NAME='Unused1000_3');
SET @s := IF(@g, 'ALTER TABLE `trait_tree` CHANGE `Unused1000_3` `MaxZoom` float NOT NULL DEFAULT 0 AFTER `MinZoom`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

-- guarded: only add while `UiTextureKitID` is absent

SET @g := (SELECT COUNT(*)=0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='trait_tree' AND COLUMN_NAME='UiTextureKitID');
SET @s := IF(@g, 'ALTER TABLE `trait_tree` ADD COLUMN `UiTextureKitID` int NOT NULL DEFAULT 0 AFTER `MaxZoom`', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

--
-- Table structure for table `trait_tree_locale`
--

DROP TABLE IF EXISTS `trait_tree_locale`;

CREATE TABLE `trait_tree_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `TitleText_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB);
