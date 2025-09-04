--
-- TrinityCore Playerbot Integration - World Database Tables  
-- Date: 2025-09-03
-- Phase: 1.1 Database Infrastructure Setup
--

-- RPG race assignments for immersive bot behavior
CREATE TABLE `playerbots_rpg_races` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `entry` int(11) DEFAULT NULL,
  `race` int(11) DEFAULT NULL,
  `minl` int(11) DEFAULT NULL,
  `maxl` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `entry` (`entry`),
  KEY `race_level` (`race`, `minl`, `maxl`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Bot RPG race assignments for immersive behavior';