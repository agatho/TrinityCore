CREATE TABLE IF NOT EXISTS `warband_reputation_faction` (
  `factionId` smallint unsigned NOT NULL,
  `comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`factionId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- The War Within account-wide renown factions
INSERT IGNORE INTO `warband_reputation_faction` (`factionId`, `comment`) VALUES
(2570, 'Hallowfall Arathi'),
(2590, 'Council of Dornogal'),
(2594, 'The Assembly of the Deeps'),
(2600, 'The Severed Threads');
