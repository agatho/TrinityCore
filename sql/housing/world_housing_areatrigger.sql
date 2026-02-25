-- Housing Plot AreaTrigger (entry 37358)
-- Defines the create properties for housing plot AreaTriggers.
-- Sniff-verified: Box shape (35x30x94 extents) provides plot boundaries
-- for the client's decor placement validation (OutsidePlotBounds check).
-- Also handles player enter/exit detection for NeighborhoodPlayerEnterPlot.

-- Template entry (no actions by default — AI handles the logic)
DELETE FROM `areatrigger_template` WHERE `Id` = 37358 AND `IsCustom` = 0;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`) VALUES
(37358, 0, 0, 0, 0);

-- Create properties: Box shape, extents (35, 30, 94), DecalPropertiesId=621, SpellForVisuals=1282351
-- Sniff: ShapeType=1 (Box), Extents=(35,30,94), BoundsRadius2D=46.098, DecalPropertiesID=621
DELETE FROM `areatrigger_create_properties` WHERE `Id` = 37358 AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties` (
    `Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`,
    `MoveCurveId`, `ScaleCurveId`, `MorphCurveId`, `FacingCurveId`,
    `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`,
    `TimeToTargetScale`, `Speed`, `SpeedIsTime`,
    `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`,
    `ShapeData4`, `ShapeData5`, `ShapeData6`, `ShapeData7`,
    `ScriptName`
) VALUES (
    37358, 0, 37358, 0, 0,
    0, 0, 0, 0,
    0, 0, 621, 1282351,
    0, 1, 0,
    1, 35, 30, 94, 35,  -- Shape 1 = Box, ShapeData0-2 = extents, ShapeData3-5 = extentsTarget
    30, 94, 0, 0,
    'at_housing_plot'
);
