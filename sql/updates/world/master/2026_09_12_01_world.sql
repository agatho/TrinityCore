--
-- Delve tier picker (SMSG_TIERED_ENTRANCE_OPEN_RESPONSE) data from the 12.1.0.69497 captures
-- (Gulf of Memory, Shadow Enclave) and the 12.0.7 Darkway capture. The tier rows are the server-only
-- TieredEntranceTier.db2 (ids 75..85 for the 12.1 delve season, identical for every delve entrance);
-- the header fields and widget sets are per entrance.
--

ALTER TABLE `delve_template`
    ADD COLUMN `tieredEntranceId` int unsigned NOT NULL DEFAULT '0' COMMENT 'TieredEntrance.db2 row id (packet field 8)' AFTER `finalBossEntry`,
    ADD COLUMN `tieredEntranceUnknown3` int unsigned NOT NULL DEFAULT '0' COMMENT 'packet field 3, meaning open' AFTER `tieredEntranceId`,
    ADD COLUMN `entranceUiWidgetSetId` int unsigned NOT NULL DEFAULT '0' COMMENT 'packet field 4' AFTER `tieredEntranceUnknown3`,
    ADD COLUMN `modifierUiWidgetSetTier1` int unsigned NOT NULL DEFAULT '0' COMMENT 'ModifierUIWidgetSetID of tier 1; tier N = this - (N-1)' AFTER `entranceUiWidgetSetId`;

DROP TABLE IF EXISTS `delve_tiered_entrance_tier`;
CREATE TABLE `delve_tiered_entrance_tier` (
  `id` int unsigned NOT NULL COMMENT 'TieredEntranceTierID, echoed by CMSG_SELECT_DELVE_ENTRANCE_TIER',
  `tier` tinyint unsigned NOT NULL,
  `suggestedILvl` int unsigned NOT NULL DEFAULT '0',
  `overrideTooltipSpellId` int unsigned NOT NULL DEFAULT '0',
  `unlockPlayerConditionId` int unsigned NOT NULL DEFAULT '0',
  `dynamicUnlockPlayerConditionId` int unsigned NOT NULL DEFAULT '0',
  `description` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `delve_tiered_entrance_tier_reward`;
CREATE TABLE `delve_tiered_entrance_tier_reward` (
  `tierId` int unsigned NOT NULL,
  `orderIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `rewardType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 item, 1 currency',
  `id` int unsigned NOT NULL,
  `quantity` int unsigned NOT NULL DEFAULT '1',
  `context` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'ItemContext',
  PRIMARY KEY (`tierId`,`orderIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 12.1 delve season tier rows (gulf 88718 == shadow enclave 1842501, byte-identical apart from widget sets)
INSERT INTO `delve_tiered_entrance_tier` (`id`,`tier`,`suggestedILvl`,`overrideTooltipSpellId`,`unlockPlayerConditionId`,`dynamicUnlockPlayerConditionId`,`description`) VALUES
(75, 1, 170, 1260939, 156924, 0, 'Tier 1'),
(76, 2, 187, 1260942, 156923, 0, 'Tier 2'),
(77, 3, 200, 1260946, 156922, 0, 'Tier 3'),
(78, 4, 259, 1260950, 156919, 0, 'Tier 4'),
(79, 5, 268, 0, 156918, 0, 'Tier 5'),
(80, 6, 275, 0, 156917, 0, 'Tier 6'),
(81, 7, 281, 0, 156916, 0, 'Tier 7'),
(82, 8, 290, 0, 156915, 0, 'Tier 8'),
(83, 9, 296, 0, 156914, 0, 'Tier 9'),
(84, 10, 303, 0, 156913, 0, 'Tier 10'),
(85, 11, 309, 0, 156908, 0, 'Tier 11');

INSERT INTO `delve_tiered_entrance_tier_reward` (`tierId`,`orderIndex`,`rewardType`,`id`,`quantity`,`context`) VALUES
(75, 0, 0, 257379, 1, 104), (75, 1, 0, 257386, 1, 107),
(76, 0, 0, 257379, 1, 104), (76, 1, 0, 257386, 1, 107),
(77, 0, 0, 257379, 1, 104), (77, 1, 0, 257386, 1, 107),
(78, 0, 0, 257386, 1, 107),
(79, 0, 0, 257386, 1, 107),
(80, 0, 0, 257386, 1, 107),
(81, 0, 0, 257386, 1, 107),
(82, 0, 0, 257386, 1, 107),
(83, 0, 0, 257386, 1, 107),
(84, 0, 0, 257386, 1, 107),
(85, 0, 0, 257386, 1, 107);

-- per-entrance header values
UPDATE `delve_template` SET `tieredEntranceId`=26, `tieredEntranceUnknown3`=80, `entranceUiWidgetSetId`=1779, `modifierUiWidgetSetTier1`=1778 WHERE `mapId`=2964; -- The Gulf of Memory (12.1 gulf 88718)
UPDATE `delve_template` SET `tieredEntranceId`=27, `tieredEntranceUnknown3`=78, `entranceUiWidgetSetId`=1756, `modifierUiWidgetSetTier1`=1767 WHERE `mapId`=2952; -- The Shadow Enclave (12.1 eversong 1842501)
UPDATE `delve_template` SET `tieredEntranceId`=29, `tieredEntranceUnknown3`=83, `entranceUiWidgetSetId`=1828, `modifierUiWidgetSetTier1`=1827 WHERE `mapId`=3003; -- The Darkway (12.0.7 shadowmoon 104632)
