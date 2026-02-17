-- Housing Plot AreaTrigger (entry 37358)
-- Defines the create properties for housing plot AreaTriggers that detect
-- player enter/exit events on neighborhood plots.

-- Template entry (no actions by default — AI handles the logic)
DELETE FROM `areatrigger_template` WHERE `Id` = 37358 AND `IsCustom` = 0;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`) VALUES
(37358, 0, 0, 0, 0);

-- Create properties: Sphere shape, radius 40 yards, ScriptName for AI
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
    0, 0, 0, NULL,
    0, 1, 0,
    0, 40, 40, 0, 0,  -- Shape 0 = Sphere, ShapeData0/1 = radius/radius
    0, 0, 0, 0,
    'at_housing_plot'
);
