-- Add "For Sale" sign gameobject template for housing plots
-- Type 5 = GAMEOBJECT_TYPE_GENERIC (simple interactable sign)
-- Used by NeighborhoodPlot as PlotGameObjectID to mark available plots

DELETE FROM `gameobject_template` WHERE `entry` = 417487;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `VerifiedBuild`) VALUES
(417487, 5, 8206, 'For Sale', '', '', '', 0.999999821186065673, 56263);
