DROP TABLE IF EXISTS `delve_template`;
CREATE TABLE `delve_template` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `mapId` int unsigned NOT NULL DEFAULT '0',
  `scenarioId` int unsigned NOT NULL DEFAULT '0',
  `mapChallengeModeId` int unsigned NOT NULL DEFAULT '0',
  `zoneId` int unsigned NOT NULL DEFAULT '0',
  `factionId` int unsigned NOT NULL DEFAULT '0',
  `companionSpawnX` float NOT NULL DEFAULT '0',
  `companionSpawnY` float NOT NULL DEFAULT '0',
  `companionSpawnZ` float NOT NULL DEFAULT '0',
  `companionSpawnO` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_mapId` (`mapId`),
  KEY `idx_mapChallengeModeId` (`mapChallengeModeId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `delve_tier_rewards`;
CREATE TABLE `delve_tier_rewards` (
  `tier` tinyint unsigned NOT NULL DEFAULT '0',
  `itemContext` tinyint unsigned NOT NULL DEFAULT '0',
  `maxRevives` tinyint unsigned NOT NULL DEFAULT '5',
  `crestType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=none, 1=weathered, 2=carved, 3=runed, 4=gilded',
  `crestCount` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Tier reward data: tier, itemContext (maps to ItemContext enum), maxRevives, crestType, crestCount
-- ItemContext: Delves_1=104, Delves_2=106, Delves_3=107
-- Crest types: 0=none, 1=weathered, 2=carved, 3=runed, 4=gilded
INSERT INTO `delve_tier_rewards` (`tier`, `itemContext`, `maxRevives`, `crestType`, `crestCount`) VALUES
(1,  104, 255, 0, 0),   -- Tier 1: Explorer, unlimited revives, no crests
(2,  104, 255, 0, 0),   -- Tier 2: Explorer, unlimited revives, no crests
(3,  104, 255, 1, 2),   -- Tier 3: Explorer, unlimited revives, weathered crests
(4,  106, 5,   1, 3),   -- Tier 4: Adventurer, 5 revives, weathered crests
(5,  106, 5,   1, 4),   -- Tier 5: Adventurer, 5 revives, weathered crests
(6,  106, 5,   2, 3),   -- Tier 6: Veteran, 5 revives, carved crests
(7,  107, 5,   2, 4),   -- Tier 7: Champion, 5 revives, carved crests
(8,  107, 5,   3, 4),   -- Tier 8: Champion, 5 revives, runed crests
(9,  107, 4,   3, 5),   -- Tier 9: Champion (same loot as T8), 4 revives, runed crests
(10, 107, 3,   3, 6),   -- Tier 10: Champion (same loot as T8), 3 revives, runed crests
(11, 107, 3,   4, 3);   -- Tier 11: Champion (same loot as T8), 3 revives, gilded crests
