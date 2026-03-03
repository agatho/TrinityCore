-- Housing Plot AreaTrigger (entry 37358)
-- Required for plot enter/exit detection on housing neighborhood maps.
-- Without this, SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT never fires
-- and the Cornerstone purchase UI cannot display.

-- Template entry
DELETE FROM `areatrigger_template` WHERE `Id` = 37358 AND `IsCustom` = 0;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`) VALUES
(37358, 0, 0, 0, 0);

-- Create properties: Box shape (35x30x94 extents), DecalPropertiesId=621, SpellForVisuals=1282351
-- Sniff-verified: ShapeType=1 (Box), BoundsRadius2D=46.098, DecalPropertiesID=621
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

-- ================================================================
-- SERVERSIDE SPELL 1266097: [DNT] Trigger Convo for Unowned Plot
-- Cast by Cornerstone GO (entry 457142) UILink handler when a
-- player clicks an unowned cornerstone.  The SPELL_EFFECT_DUMMY
-- is handled by a SpellScript that validates plot availability.
-- ================================================================

DELETE FROM `serverside_spell` WHERE `Id` = 1266097 AND `DifficultyID` = 0;
INSERT INTO `serverside_spell` (`Id`, `DifficultyID`, `CategoryId`, `Dispel`, `Mechanic`, `Attributes`, `AttributesEx`, `AttributesEx2`, `AttributesEx3`, `AttributesEx4`, `AttributesEx5`, `AttributesEx6`, `AttributesEx7`, `AttributesEx8`, `AttributesEx9`, `AttributesEx10`, `AttributesEx11`, `AttributesEx12`, `AttributesEx13`, `AttributesEx14`, `Stances`, `StancesNot`, `Targets`, `TargetCreatureType`, `RequiresSpellFocus`, `FacingCasterFlags`, `CasterAuraState`, `TargetAuraState`, `ExcludeCasterAuraState`, `ExcludeTargetAuraState`, `CasterAuraSpell`, `TargetAuraSpell`, `ExcludeCasterAuraSpell`, `ExcludeTargetAuraSpell`, `CasterAuraType`, `TargetAuraType`, `ExcludeCasterAuraType`, `ExcludeTargetAuraType`, `CastingTimeIndex`, `RecoveryTime`, `CategoryRecoveryTime`, `StartRecoveryCategory`, `StartRecoveryTime`, `InterruptFlags`, `AuraInterruptFlags1`, `AuraInterruptFlags2`, `ChannelInterruptFlags1`, `ChannelInterruptFlags2`, `ProcFlags`, `ProcFlags2`, `ProcChance`, `ProcCharges`, `ProcCooldown`, `ProcBasePPM`, `MaxLevel`, `BaseLevel`, `SpellLevel`, `DurationIndex`, `RangeIndex`, `Speed`, `LaunchDelay`, `StackAmount`, `EquippedItemClass`, `EquippedItemSubClassMask`, `EquippedItemInventoryTypeMask`, `ContentTuningId`, `SpellName`, `ConeAngle`, `ConeWidth`, `MaxTargetLevel`, `MaxAffectedTargets`, `SpellFamilyName`, `SpellFamilyFlags1`, `SpellFamilyFlags2`, `SpellFamilyFlags3`, `SpellFamilyFlags4`, `DmgClass`, `PreventionType`, `AreaGroupId`, `SchoolMask`, `ChargeCategoryId`) VALUES
(1266097, 0, 0, 0, 0, 0x20000180, 0x00000020, 0x00000004, 0x10100000, 0x00000080, 0x00000008, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 101, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, '[DNT] Trigger Convo for Unowned Plot', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1, 0);

-- Spell effect: SPELL_EFFECT_DUMMY (3), self-targeted (ImplicitTarget1 = 1)
DELETE FROM `serverside_spell_effect` WHERE `SpellID` = 1266097 AND `EffectIndex` = 0 AND `DifficultyID` = 0;
INSERT INTO `serverside_spell_effect` (`SpellID`, `EffectIndex`, `DifficultyID`, `Effect`, `EffectAura`, `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`, `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`, `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`, `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`, `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, `EffectBasePoints`, `EffectMiscValue1`, `EffectMiscValue2`, `EffectRadiusIndex1`, `EffectRadiusIndex2`, `EffectSpellClassMask1`, `EffectSpellClassMask2`, `EffectSpellClassMask3`, `EffectSpellClassMask4`, `ImplicitTarget1`, `ImplicitTarget2`) VALUES
(1266097, 0, 0, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0);
