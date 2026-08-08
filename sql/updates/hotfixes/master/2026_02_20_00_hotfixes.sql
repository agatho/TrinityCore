--
-- Add missing battle pet hotfix tables for pet battle system
--

-- Table structure for table `battle_pet_ability_effect`
DROP TABLE IF EXISTS `battle_pet_ability_effect`;
CREATE TABLE `battle_pet_ability_effect` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BattlePetAbilityTurnID` smallint unsigned NOT NULL DEFAULT '0',
  `OrderIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `BattlePetEffectPropertiesID` smallint unsigned NOT NULL DEFAULT '0',
  `AuraBattlePetAbilityID` smallint unsigned NOT NULL DEFAULT '0',
  `BattlePetVisualID` smallint unsigned NOT NULL DEFAULT '0',
  `Param1` smallint NOT NULL DEFAULT '0',
  `Param2` smallint NOT NULL DEFAULT '0',
  `Param3` smallint NOT NULL DEFAULT '0',
  `Param4` smallint NOT NULL DEFAULT '0',
  `Param5` smallint NOT NULL DEFAULT '0',
  `Param6` smallint NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Table structure for table `battle_pet_ability_state`
DROP TABLE IF EXISTS `battle_pet_ability_state`;
CREATE TABLE `battle_pet_ability_state` (
  `ID` int NOT NULL DEFAULT '0',
  `BattlePetStateID` int unsigned NOT NULL DEFAULT '0',
  `Value` int NOT NULL DEFAULT '0',
  `BattlePetAbilityID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Table structure for table `battle_pet_ability_turn`
DROP TABLE IF EXISTS `battle_pet_ability_turn`;
CREATE TABLE `battle_pet_ability_turn` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BattlePetAbilityID` smallint unsigned NOT NULL DEFAULT '0',
  `OrderIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `TurnTypeEnum` tinyint unsigned NOT NULL DEFAULT '0',
  `EventTypeEnum` tinyint unsigned NOT NULL DEFAULT '0',
  `BattlePetVisualID` smallint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Table structure for table `battle_pet_effect_properties`
DROP TABLE IF EXISTS `battle_pet_effect_properties`;
CREATE TABLE `battle_pet_effect_properties` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ParamLabel1` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ParamLabel2` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ParamLabel3` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ParamLabel4` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ParamLabel5` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ParamLabel6` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BattlePetVisualID` smallint unsigned NOT NULL DEFAULT '0',
  `ParamTypeEnum1` tinyint unsigned NOT NULL DEFAULT '0',
  `ParamTypeEnum2` tinyint unsigned NOT NULL DEFAULT '0',
  `ParamTypeEnum3` tinyint unsigned NOT NULL DEFAULT '0',
  `ParamTypeEnum4` tinyint unsigned NOT NULL DEFAULT '0',
  `ParamTypeEnum5` tinyint unsigned NOT NULL DEFAULT '0',
  `ParamTypeEnum6` tinyint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Table structure for table `battle_pet_species_x_ability`
DROP TABLE IF EXISTS `battle_pet_species_x_ability`;
CREATE TABLE `battle_pet_species_x_ability` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BattlePetAbilityID` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `SlotEnum` tinyint NOT NULL DEFAULT '0',
  `BattlePetSpeciesID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- WarbandScene hotfix tables (from major-factions; same filename, disjoint content)
--

DROP TABLE IF EXISTS `warband_scene_animation`;
CREATE TABLE `warband_scene_animation` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellVisualKitID` int NOT NULL DEFAULT '0',
  `Event` int NOT NULL DEFAULT '0',
  `AnimKitID` int NOT NULL DEFAULT '0',
  `Field_11_0_0_54210_003` int NOT NULL DEFAULT '0',
  `TimeIsh` float NOT NULL DEFAULT '0',
  `StandState` tinyint unsigned NOT NULL DEFAULT '0',
  `SheatheState` tinyint unsigned NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_008` tinyint NOT NULL DEFAULT '0',
  `Field_11_0_0_54210_005_0` int NOT NULL DEFAULT '0',
  `Field_11_0_0_54210_005_1` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_scene_anim_chr_spec`;
CREATE TABLE `warband_scene_anim_chr_spec` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `WarbandSceneAnimationID` int NOT NULL DEFAULT '0',
  `ChrSpecializationID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_scene_placement_filter_req`;
CREATE TABLE `warband_scene_placement_filter_req` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_002` smallint unsigned NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_005` tinyint NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_003_0` int NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_003_1` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_scene_placement_option`;
CREATE TABLE `warband_scene_placement_option` (
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `WarbandScenePlacementID` int unsigned NOT NULL DEFAULT '0',
  `Orientation` float NOT NULL DEFAULT '0',
  `Scale` float NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_005` int NOT NULL DEFAULT '0',
  `Field_11_1_0_58221_006` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_scene_plcmnt_anim_override`;
CREATE TABLE `warband_scene_plcmnt_anim_override` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Field_11_0_0_54210_000` int NOT NULL DEFAULT '0',
  `WarbandSceneAnimationID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_placement_display_info`;
CREATE TABLE `warband_placement_display_info` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `WarbandScenePlacementID` int unsigned NOT NULL DEFAULT '0',
  `Field_11_2_0_61476_001` int NOT NULL DEFAULT '0',
  `Field_11_2_0_61476_002` int NOT NULL DEFAULT '0',
  `Field_11_2_0_61476_003` int NOT NULL DEFAULT '0',
  `Field_11_2_0_61476_004` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_scene_source_info`;
CREATE TABLE `warband_scene_source_info` (
  `SourceDescription` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `WarbandSceneID` int unsigned NOT NULL DEFAULT '0',
  `SourceType` tinyint NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `warband_scene_source_info_locale`;
CREATE TABLE `warband_scene_source_info_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `SourceDescription_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
