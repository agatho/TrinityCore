-- Evoker Aug+Pres finish debt (EVR-44 / EVR-37 follow-on)
-- Flow 2R impossible-date; dual-check free NN=41 on master + evry (tip through _40).

-- Motes of Possibility AT (Spell 419934 → create-properties 28777)
DELETE FROM `areatrigger_template` WHERE (`Id`=33277 AND `IsCustom`=0);
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `VerifiedBuild`) VALUES
(33277, 0, 0, 67808);

DELETE FROM `areatrigger_create_properties` WHERE (`Id`=28777 AND `IsCustom`=0);
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`, `ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `TimeToTargetScale`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`, `ShapeData6`, `ShapeData7`, `ScriptName`, `VerifiedBuild`) VALUES
(28777, 0, 33277, 0, 0, 0, 0, 0, 0, -1, 0, 0, NULL, 9000, 0, 2.5, 2.5, 0, 0, 0, 0, 0, 0, 'at_evo_motes_of_possibility', 67808);

-- Temporal Anomaly orbs (Spell 373861 → create-properties 25294 / 34997)
DELETE FROM `areatrigger_template` WHERE (`IsCustom`=0 AND `Id` IN (33278, 33279));
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `VerifiedBuild`) VALUES
(33278, 0, 0, 67808),
(33279, 0, 0, 67808);

DELETE FROM `areatrigger_create_properties` WHERE (`IsCustom`=0 AND `Id` IN (25294, 34997));
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`, `ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `TimeToTargetScale`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`, `ShapeData6`, `ShapeData7`, `ScriptName`, `VerifiedBuild`) VALUES
(25294, 0, 33278, 0, 0, 0, 0, 0, 0, -1, 0, 0, NULL, 8000, 0, 6, 6, 0, 0, 0, 0, 0, 0, 'at_evo_temporal_anomaly', 67808),
(34997, 0, 33279, 0, 0, 0, 0, 0, 0, -1, 0, 0, NULL, 8000, 0, 6, 6, 0, 0, 0, 0, 0, 0, 'at_evo_temporal_anomaly', 67808);

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_activate_weyrnstone',
  'spell_evo_blistering_scales_explode',
  'spell_evo_lifespark'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(408234, 'spell_evo_activate_weyrnstone'),
(360828, 'spell_evo_blistering_scales_explode'),
(443177, 'spell_evo_lifespark');
