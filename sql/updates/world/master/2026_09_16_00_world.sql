--
-- Open-world scenarios and scenario step spells (retail 12.1.0.69497 / 69587 captures)
--

DROP TABLE IF EXISTS `scenario_world`;
CREATE TABLE `scenario_world` (
  `ScenarioID` int unsigned NOT NULL,
  `MapID` int unsigned NOT NULL,
  `AreaID` int unsigned NOT NULL COMMENT 'players inside this area or any of its sub-areas take part',
  `Flags` int unsigned NOT NULL DEFAULT '0' COMMENT '0x1 = start when the map is created',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ScenarioID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `scenario_step_spell`;
CREATE TABLE `scenario_step_spell` (
  `ScenarioStepID` int unsigned NOT NULL,
  `Idx` tinyint unsigned NOT NULL,
  `SpellID` int unsigned NOT NULL,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ScenarioStepID`,`Idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Vaults of Atal'Utek (map 2916, zone 16535): the temple events run one after another and are started by the zone's
-- event script, never on their own. Each SMSG_SCENARIO_STATE GUID carries map 2916.
INSERT INTO `scenario_world` (`ScenarioID`,`MapID`,`AreaID`,`Flags`,`VerifiedBuild`) VALUES
(3358,2916,16535,0,69587), -- Temple Strike: The Underbelly
(3362,2916,16535,0,69587), -- Temple Incursion: Cache of the Three
(3363,2916,16535,0,69587), -- Temple Incursion: Supplies Must Flow
(3364,2916,16535,0,69587), -- Temple Strike: Ruuk'Jar's Clutch
(3405,2916,16535,0,69587), -- Temple Strike: Cursed Depths
(3412,2916,16535,0,69587); -- Ancient Foe: Susarikk

-- Arcana Overload (Eversong Woods, map 0): active when the capturing player logged in at Runestone Falithas (16000),
-- vacated with reason Left on leaving that sub-area. What starts it on retail was not captured, so it runs from map
-- creation.
INSERT INTO `scenario_world` (`ScenarioID`,`MapID`,`AreaID`,`Flags`,`VerifiedBuild`) VALUES
(2958,0,16000,1,69497);

-- Blade in Twilight (scenario 991): SMSG_SCENARIO_STATE of each step change lists these spells
INSERT INTO `scenario_step_spell` (`ScenarioStepID`,`Idx`,`SpellID`,`VerifiedBuild`) VALUES
(2089,0,311663,69497), -- Dark Passage: Mass Dispel
(2099,0,311663,69497), -- Death to the Deacon: Mass Dispel
(2116,0,201904,69497); -- The True Death of Zakajz: Dark Drain
