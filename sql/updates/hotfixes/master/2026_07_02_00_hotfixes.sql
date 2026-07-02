--
-- WarbandScenePlacementFilterReq: 12.0.7 (68275) layout 0x16F5877A
-- dropped the 64-bit Field_11_1_0_58221_000, RaceMasks[2] named via DBD match
--
DROP TABLE IF EXISTS `warband_scene_placement_filter_req`;
CREATE TABLE `warband_scene_placement_filter_req` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_002` smallint NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_005` tinyint NOT NULL DEFAULT '0',
  `RaceMasks1` int NOT NULL DEFAULT '0',
  `RaceMasks2` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
