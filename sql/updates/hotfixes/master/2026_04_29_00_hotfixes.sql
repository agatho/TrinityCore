--
-- 2026-04-29: housing door click — spell 1271876 + exterior_component schema refresh
--
-- Sniff-decoded from C:\sniff\housing_stuff\{alliance,horde}_housing\dump_12.0.5.67186_2026-04-28_*.pkt:
-- The retail client casts SpellID 1271876 (uint32 at CMSG_CAST_SPELL body offset 0x15) when a
-- player clicks a housing door GO. Bytes 0x15-0x18 are identical across both faction sniffs;
-- only the target PackedGUID at offset 0x3D differs. The cast (not CMSG_GAME_OBJ_REPORT_USE,
-- which is the criteria-tracking follow-up) is what triggers TRANSFER_PENDING + NEW_WORLD on
-- retail. Adding a minimal spell record so server-side cast validation accepts the spell —
-- the actual door action is wired via gameobject_template.data2 (goober.spell) in the world
-- update + the SpellScript in src/server/scripts/Spells/spell_housing.cpp.

-- Task A: Spell 1271876 "Housing Door Open"
DELETE FROM `spell_name` WHERE `id` = 1271876;
INSERT INTO `spell_name` (`id`, `name`, `VerifiedBuild`) VALUES
  (1271876, 'Housing Door Open', 67186);

DELETE FROM `spell_misc` WHERE `SpellID` = 1271876;
INSERT INTO `spell_misc` (
  `Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, `Attributes5`, `Attributes6`, `Attributes7`,
  `Attributes8`, `Attributes9`, `Attributes10`, `Attributes11`, `Attributes12`, `Attributes13`, `Attributes14`,
  `Attributes15`, `Attributes16`, `Attributes17`,
  `DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, `RangeIndex`, `SchoolMask`,
  `Speed`, `LaunchDelay`, `MinDuration`,
  `SpellIconFileDataID`, `ActiveIconFileDataID`, `ContentTuningID`, `ShowFutureSpellPlayerConditionID`,
  `SpellVisualScript`, `ActiveSpellVisualScript`,
  `SpellID`, `VerifiedBuild`
) VALUES
  (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0, 1, 0, 0, 1, 0,
   0, 0, 0,
   0, 0, 0, 0,
   504329, 0,
   1271876, 67186);

-- Effect 0: SPELL_EFFECT_DUMMY (3), targeting the GameObject (TARGET_GAMEOBJECT_TARGET = 50).
-- The spell needs at least one effect for SpellScript hooks to fire and for SpellInfo
-- validation to pass. The actual door action runs in spell_housing_door_open's
-- HandleDummy override (see src/server/scripts/Spells/spell_housing.cpp), which calls
-- GameObject::Use() on the target so go_housing_door::OnGossipHello executes.
DELETE FROM `spell_effect` WHERE `SpellID` = 1271876;
INSERT INTO `spell_effect` (
  `ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`,
  `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`,
  `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`,
  `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`,
  `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`,
  `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`,
  `Variance`, `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`,
  `EffectBasePoints`, `EffectMiscValue1`, `EffectMiscValue2`,
  `EffectRadiusIndex1`, `EffectRadiusIndex2`,
  `EffectSpellClassMask1`, `EffectSpellClassMask2`,
  `EffectSpellClassMask3`, `EffectSpellClassMask4`,
  `ImplicitTarget1`, `ImplicitTarget2`,
  `SpellID`, `VerifiedBuild`
) VALUES
  (1271876, 0, 0, 0, 3,                  -- ID, EffectAura, DifficultyID, EffectIndex, Effect=DUMMY
   0, 0, 0,                              -- EffectAmplitude, EffectAttributes, EffectAuraPeriod
   0, 0, 0,                              -- EffectBonusCoefficient, EffectChainAmplitude, EffectChainTargets
   0, 0, 0,                              -- EffectItemType, EffectMechanic, EffectPointsPerResource
   0, 0, 0,                              -- EffectPosFacing, EffectRealPointsPerLevel, EffectTriggerSpell
   0, 0, 0,                              -- BonusCoefficientFromAP, PvpMultiplier, Coefficient
   0, 0, 0,                              -- Variance, ResourceCoefficient, GroupSizeBasePointsCoefficient
   0, 0, 0,                              -- EffectBasePoints, EffectMiscValue1, EffectMiscValue2
   0, 0,                                 -- EffectRadiusIndex1, EffectRadiusIndex2
   0, 0, 0, 0,                           -- EffectSpellClassMask1..4
   23, 0,                                -- ImplicitTarget1=TARGET_GAMEOBJECT_TARGET, ImplicitTarget2=NONE
   1271876, 67186);


-- Task D: ExteriorComponent — schema refresh to match 12.0.5 DB2LoadInfo (build 67186 layout 0x53EA0925)
-- Old SQL columns (FileDataID/ConditionID/HookID/Slot/SortOrder/ComponentGroupID/UiTextureKitID/
-- ExteriorComponentTypeID) were from an older WoW build and don't line up with the 14-field
-- 12.0.5 record (Name + 3 floats + ID + Size + ParentComponentID + ModelFileDataID + Flags +
-- Field_7 + Type + Field_9 + GameObjectID + Field_11 + ItemID + HouseExteriorWmoDataID).
-- HousingMap::SpawnExtCompTree reads `comp->GameObjectID` to spawn the door GO; with the old
-- schema that column doesn't exist and HotfixDatabase loads zeros, so doors never spawn.
DROP TABLE IF EXISTS `exterior_component`;
CREATE TABLE `exterior_component` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Size` tinyint unsigned NOT NULL DEFAULT '0',
  `ParentComponentID` int NOT NULL DEFAULT '0',
  `ModelFileDataID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Field_7` tinyint unsigned NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `Field_9` int NOT NULL DEFAULT '0',
  `GameObjectID` int NOT NULL DEFAULT '0',
  `Field_11` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `HouseExteriorWmoDataID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
