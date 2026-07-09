-- Area POI map blips: which POIs broadcast as active + their timer/worldstate.
-- Loaded by AreaPoiMgr; feeds SMSG_AREA_POI_UPDATE_RESPONSE. Duration in seconds. VariableID/Value =
-- optional WorldState pair (0 = none). Seed rows are real AreaPOIs observed on the 12.0.7 wire.
CREATE TABLE IF NOT EXISTS `area_poi_template` (
  `AreaPoiID` INT UNSIGNED NOT NULL,
  `Duration` INT UNSIGNED NOT NULL DEFAULT '3600',
  `VariableID` INT NOT NULL DEFAULT '0',
  `Value` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`AreaPoiID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Area POI blip activation templates';

DELETE FROM `area_poi_template`;
INSERT INTO `area_poi_template` (`AreaPoiID`, `Duration`, `VariableID`, `Value`) VALUES
 (5178,21600,13005,1),
 (5270,21600,13005,1),
 (5359,21600,14064,1),
 (5366,21600,14063,1),
 (5372,21600,14245,1),
 (5377,604800,14249,1),
 (7104,3600,22527,1),
 (7261,86400,22526,1),
 (7413,3600,22527,1),
 (7461,1500,23298,1),
 (7902,3600,25959,5),
 (8175,3540,27062,1),
 (8757,604800,29528,2);
