-- Populate delve_template with real map IDs from Map.db2
-- ScenarioID and MapChallengeModeID to be filled from DB2 data when available
DELETE FROM `delve_template`;
INSERT INTO `delve_template` (`id`, `mapId`, `scenarioId`, `mapChallengeModeId`, `zoneId`, `factionId`) VALUES
-- Isle of Dorn
(1,  2680, 0, 0, 0, 0),   -- Earthcrawl Mines (Nerubian)
(2,  2664, 0, 0, 0, 0),   -- Fungal Folly (Fungal/mushroom)
(3,  2681, 0, 0, 0, 0),   -- Kriegval's Rest (Kobold)
-- The Ringing Deeps
(4,  2683, 0, 0, 0, 0),   -- The Waterworks (Kobold)
(5,  2684, 0, 0, 0, 0),   -- The Dread Pit (Nerubian)
-- Hallowfall
(6,  2686, 0, 0, 0, 0),   -- Nightfall Sanctum (Order of Night)
(7,  2679, 0, 0, 0, 0),   -- Mycomancer Cavern (Fungal/myconid)
(8,  2685, 0, 0, 0, 0),   -- Skittering Breach (Nerubian/Order of Night)
(9,  2687, 0, 0, 0, 0),   -- The Sinkhole (Underwater/Kobyss)
-- Azj-Kahet
(10, 2688, 0, 0, 0, 0),   -- The Spiral Weave (Nerubian fortress)
(11, 2689, 0, 0, 0, 0),   -- Tak-Rethan Abyss (Underwater/Kobyss)
(12, 2690, 0, 0, 0, 0),   -- The Underkeep (Nerubian)
(13, 2682, 0, 0, 0, 0);   -- Zekvir's Lair (Special boss)
