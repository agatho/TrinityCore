-- Housing Cosmetic Phases - Phase.db2 Hotfix
--
-- These 16 phases are cosmetic overlay phases used in housing neighborhoods.
-- In retail, they exist in Phase.db2 with Flags=0x10 (Cosmetic).
-- Without this data, PhasingHandler::AddPhase() treats them as non-cosmetic,
-- which causes NonCosmeticReferences to increase, removing the Unphased flag
-- from the player's PhaseShift. This makes all default-phase NPCs/creatures
-- invisible when the phases are restored on plot exit.
--
-- The phases are removed when a player enters their own plot (to remove
-- cosmetic overlays during decoration) and restored when they leave.
--
-- Phase IDs from sniff: 25571, 26216, 27429, 27442, 27489, 27695,
--                        28304, 28312, 28313, 28314, 28315, 28316,
--                        28320, 28339, 28370, 28748

DELETE FROM `phase` WHERE `ID` IN (25571, 26216, 27429, 27442, 27489, 27695, 28304, 28312, 28313, 28314, 28315, 28316, 28320, 28339, 28370, 28748);

INSERT INTO `phase` (`ID`, `Flags`, `VerifiedBuild`) VALUES
(25571, 0x10, 65940),
(26216, 0x10, 65940),
(27429, 0x10, 65940),
(27442, 0x10, 65940),
(27489, 0x10, 65940),
(27695, 0x10, 65940),
(28304, 0x10, 65940),
(28312, 0x10, 65940),
(28313, 0x10, 65940),
(28314, 0x10, 65940),
(28315, 0x10, 65940),
(28316, 0x10, 65940),
(28320, 0x10, 65940),
(28339, 0x10, 65940),
(28370, 0x10, 65940),
(28748, 0x10, 65940);
