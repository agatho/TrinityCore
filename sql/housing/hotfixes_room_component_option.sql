-- Room Component Option data extracted from retail sniffs (12.0.1.65940)
-- This table maps (RoomComponentID, HouseThemeID) → themed model + display data
-- Without this data, all room components use base DB2 FileDataIDs with optionID=0
--
-- ThemeID 2 = Horde exterior, ThemeID 6 = Alliance/generic, ThemeID 8 = Neutral interior walls
-- Sniff sources: alliance_housing_start, horde_housing (Map 2783 interior entities)

DELETE FROM `room_component_option` WHERE `VerifiedBuild` = 65940;

INSERT INTO `room_component_option` (`ID`, `Type`, `SubType`, `ModelFileDataID`, `RoomComponentID`, `MeshStyleFilterID`, `HouseThemeID`, `Flags`, `VerifiedBuild`) VALUES
-- Exterior plot room component (RoomComponentID=196, base room entry 18)
(874, 2, 1, 6322976, 196, 0, 6, 0, 65940),  -- Alliance exterior (Stucco)
(420, 2, 2, 6322976, 196, 0, 2, 0, 65940),  -- Horde exterior

-- Interior Room 1 ("Square Room Small") - Floor components
(323, 2, 1, 6426461, 25,  0, 6, 0, 65940),  -- Floor (main)
(313, 2, 1, 6426452, 202, 0, 6, 0, 65940),  -- Floor (secondary)

-- Interior Room 1 - Ceiling components
(347, 3, 1, 6426431, 201, 0, 6, 0, 65940),  -- Ceiling (type 3, theme 6)
(274, 3, 2, 6426605, 64,  0, 8, 0, 65940),  -- Ceiling (type 3, theme 8)

-- Interior Room 1 - Wall components (theme 8 = neutral interior)
(267, 1, 2, 6426665, 27,  0, 8, 0, 65940),  -- Wall segment (shared model for 27,28,29)
(283, 1, 2, 6426671, 26,  0, 8, 0, 65940),  -- Doorway wall (Field_20=1, TypeParam=2)
(284, 1, 2, 6426647, 205, 0, 8, 0, 65940),  -- Wall segment
(298, 1, 2, 6426641, 203, 0, 8, 0, 65940),  -- Wall segment (shared model for 203,204)
(309, 1, 2, 6426613, 31,  0, 8, 0, 65940),  -- Wall segment (shared model for 31,32,33)
(384, 1, 2, 6426672, 26,  0, 8, 0, 65940),  -- Doorway opening (Field_20=2, TypeParam=2)

-- Additional entries for components that share retail optionIDs but need per-component lookups
-- These use synthetic IDs (1001+) so FindRoomComponentOption(compID, themeID) works for each
(1001, 1, 2, 6426665, 28,  0, 8, 0, 65940), -- Same model as 267, for comp 28
(1002, 1, 2, 6426665, 29,  0, 8, 0, 65940), -- Same model as 267, for comp 29
(1003, 1, 2, 6426613, 32,  0, 8, 0, 65940), -- Same model as 309, for comp 32
(1004, 1, 2, 6426613, 33,  0, 8, 0, 65940), -- Same model as 309, for comp 33
(1005, 1, 2, 6426641, 204, 0, 8, 0, 65940); -- Same model as 298, for comp 204
