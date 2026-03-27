-- Populate delve_template with verified data from Map.db2, MapDifficulty.db2, AreaTable.db2
-- DifficultyID = 208 ("Delves"), ScenarioType = 8
-- MapChallengeMode is NOT used by delves (confirmed: 0 entries in MCM table)
DELETE FROM `delve_template`;
INSERT INTO `delve_template` (`id`, `mapId`, `scenarioId`, `mapChallengeModeId`, `zoneId`, `factionId`) VALUES
-- Isle of Dorn
(1,  2680, 0, 0, 14999, 0),   -- Earthcrawl Mines (Nerubian)
(2,  2664, 0, 0, 14957, 0),   -- Fungal Folly (Fungal/mushroom)
(3,  2681, 0, 0, 15000, 0),   -- Kriegval's Rest (Kobold)
-- The Ringing Deeps
(4,  2683, 0, 0, 15002, 0),   -- The Waterworks (Kobold)
(5,  2684, 0, 0, 15003, 0),   -- The Dread Pit (Nerubian)
-- Hallowfall
(6,  2686, 0, 0, 15005, 0),   -- Nightfall Sanctum (Order of Night)
(7,  2679, 0, 0, 14998, 0),   -- Mycomancer Cavern (Fungal/myconid)
(8,  2685, 0, 0, 15004, 0),   -- Skittering Breach (Nerubian/Order of Night)
(9,  2767, 0, 0, 15006, 0),   -- The Sinkhole (Underwater/Kobyss) - primary map
-- Azj-Kahet
(10, 2688, 0, 0, 15007, 0),   -- The Spiral Weave (Nerubian fortress)
(11, 2689, 0, 0, 15008, 0),   -- Tak-Rethan Abyss (Underwater/Kobyss)
(12, 2690, 0, 0, 15009, 0),   -- The Underkeep (Nerubian)
(13, 2682, 0, 0, 15001, 0);   -- Zekvir's Lair (Special boss)

-- Note: alternate maps exist for variant instances:
-- The Sinkhole alt: 2687 (zoneId 15175)
-- Tak-Rethan Abyss alt: 2768 (zoneId 15327)
-- Earthcrawl Mines alt: 2836
