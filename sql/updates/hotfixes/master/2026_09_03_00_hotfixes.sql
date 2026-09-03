--
-- Hotfix schema reconciliation for reconstructed DB2 stores (build 69587).
--
-- Several golden feature branches (feature/pet-battles, feature/covenant, feature/garrison*,
-- feature/soulbind, archaeology research, LFG group_finder, housing exterior components,
-- warband scenes) added DB2 hotfix loaders + prepared statements (HotfixDatabase.cpp /
-- DB2LoadInfo.h) for stores that the official TDB_full_hotfixes_1200.26021 base dump does not
-- provide. Without the backing tables/columns the worldserver aborts at startup with
-- "Could not prepare statements of the Hotfix database". This migration creates every missing
-- hotfix table and adds every missing column, with names/types taken verbatim from DB2LoadInfo.h
-- (DB2FieldMeta) and the SELECT lists in HotfixDatabase.cpp. Tables ship empty; the client reads
-- these DB2s from its own client-side stores, the server-side hotfix rows are optional overrides.
--

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
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `battle_pet_ability_state`;
CREATE TABLE `battle_pet_ability_state` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BattlePetStateID` int unsigned NOT NULL DEFAULT '0',
  `Value` int NOT NULL DEFAULT '0',
  `BattlePetAbilityID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `battle_pet_ability_turn`;
CREATE TABLE `battle_pet_ability_turn` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BattlePetAbilityID` smallint unsigned NOT NULL DEFAULT '0',
  `OrderIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `TurnTypeEnum` tinyint unsigned NOT NULL DEFAULT '0',
  `EventTypeEnum` tinyint unsigned NOT NULL DEFAULT '0',
  `BattlePetVisualID` smallint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `battle_pet_species_x_ability`;
CREATE TABLE `battle_pet_species_x_ability` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BattlePetAbilityID` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `SlotEnum` tinyint NOT NULL DEFAULT '0',
  `BattlePetSpeciesID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `char_shipment`;
CREATE TABLE `char_shipment` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ContainerID` smallint unsigned NOT NULL DEFAULT '0',
  `TreasureID` int unsigned NOT NULL DEFAULT '0',
  `DummyItemID` int NOT NULL DEFAULT '0',
  `Duration` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `OnCompleteSpellID` int NOT NULL DEFAULT '0',
  `MaxShipments` tinyint unsigned NOT NULL DEFAULT '0',
  `GarrFollowerID` smallint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `covenant`;
CREATE TABLE `covenant` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BountySetID` int NOT NULL DEFAULT '0',
  `SkillLineID` int NOT NULL DEFAULT '0',
  `DeathTeleportSpellID` int NOT NULL DEFAULT '0',
  `Field_9_0_2_36165_006` int NOT NULL DEFAULT '0',
  `Field_9_0_2_36165_007` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `CurrencyTypesID` int NOT NULL DEFAULT '0',
  `RequiredPlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_ability_category`;
CREATE TABLE `garr_ability_category` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_encounter_set_x_encounter`;
CREATE TABLE `garr_encounter_set_x_encounter` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrEncounterID` int unsigned NOT NULL DEFAULT '0',
  `GarrEncounterSetID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_encounter_x_mechanic`;
CREATE TABLE `garr_encounter_x_mechanic` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrMechanicID` int NOT NULL DEFAULT '0',
  `GarrMechanicSetID` tinyint unsigned NOT NULL DEFAULT '0',
  `GarrEncounterID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_follower_set_x_follower`;
CREATE TABLE `garr_follower_set_x_follower` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrFollowerID` int NOT NULL DEFAULT '0',
  `GarrFollowerSetID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_follower_type`;
CREATE TABLE `garr_follower_type` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrTypeID` tinyint NOT NULL DEFAULT '0',
  `MaxFollowers` tinyint unsigned NOT NULL DEFAULT '0',
  `MaxFollowerBuildingType` tinyint unsigned NOT NULL DEFAULT '0',
  `MaxItemLevel` smallint unsigned NOT NULL DEFAULT '0',
  `LevelRangeBias` tinyint unsigned NOT NULL DEFAULT '0',
  `ItemLevelRangeBias` tinyint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_mechanic`;
CREATE TABLE `garr_mechanic` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrMechanicTypeID` int NOT NULL DEFAULT '0',
  `Factor` float NOT NULL DEFAULT '0',
  `GarrAbilityID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_mechanic_set_x_mechanic`;
CREATE TABLE `garr_mechanic_set_x_mechanic` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrMechanicID` int NOT NULL DEFAULT '0',
  `GarrMechanicSetID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_mechanic_type`;
CREATE TABLE `garr_mechanic_type` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `GarrAbilityCategoryID` int NOT NULL DEFAULT '0',
  `Category` tinyint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_talent`;
CREATE TABLE `garr_talent` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrTalentTreeID` int unsigned NOT NULL DEFAULT '0',
  `Tier` tinyint NOT NULL DEFAULT '0',
  `UiOrder` tinyint NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int unsigned NOT NULL DEFAULT '0',
  `GarrAbilityID` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `TalentType` int NOT NULL DEFAULT '0',
  `PrerequisiteTalentID` int NOT NULL DEFAULT '0',
  `ResearchCostSource` int NOT NULL DEFAULT '0',
  `ActiveDurationSecs` int NOT NULL DEFAULT '0',
  `GarrTalentSocketPropertiesID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_talent_cost`;
CREATE TABLE `garr_talent_cost` (
  `MoneyQuantity` bigint unsigned NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrTalentTreeID` int unsigned NOT NULL DEFAULT '0',
  `GarrTalentID` int NOT NULL DEFAULT '0',
  `RankIndex` int NOT NULL DEFAULT '0',
  `GarrTalentRankID` int NOT NULL DEFAULT '0',
  `CostType` int NOT NULL DEFAULT '0',
  `CurrencyTypesID` int NOT NULL DEFAULT '0',
  `CurrencyQuantity` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_talent_rank`;
CREATE TABLE `garr_talent_rank` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Rank` int NOT NULL DEFAULT '0',
  `PerkSpellID` int NOT NULL DEFAULT '0',
  `PerkPlayerConditionID` int NOT NULL DEFAULT '0',
  `Points` float NOT NULL DEFAULT '0',
  `ResearchCost` int NOT NULL DEFAULT '0',
  `ResearchCostCurrencyTypesID` int NOT NULL DEFAULT '0',
  `ResearchGoldCost` int NOT NULL DEFAULT '0',
  `ResearchDurationSecs` int NOT NULL DEFAULT '0',
  `RespecCost` int NOT NULL DEFAULT '0',
  `RespecCostCurrencyTypesID` int NOT NULL DEFAULT '0',
  `RespecGoldCost` int NOT NULL DEFAULT '0',
  `RespecDurationSecs` int NOT NULL DEFAULT '0',
  `AlternateResearchCost` int NOT NULL DEFAULT '0',
  `AlternateResearchCostCurrencyTypesID` int NOT NULL DEFAULT '0',
  `AlternateResearchGoldCost` int NOT NULL DEFAULT '0',
  `AlternateResearchDurationSecs` int NOT NULL DEFAULT '0',
  `GarrTalentID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_branch`;
CREATE TABLE `research_branch` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ResearchFieldID` tinyint unsigned NOT NULL DEFAULT '0',
  `CurrencyID` smallint unsigned NOT NULL DEFAULT '0',
  `TextureFileID` int NOT NULL DEFAULT '0',
  `BigTextureFileID` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_project`;
CREATE TABLE `research_project` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Rarity` tinyint unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `ResearchBranchID` smallint unsigned NOT NULL DEFAULT '0',
  `NumSockets` tinyint unsigned NOT NULL DEFAULT '0',
  `TextureFileID` int NOT NULL DEFAULT '0',
  `RequiredWeight` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_site`;
CREATE TABLE `research_site` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapID` smallint NOT NULL DEFAULT '0',
  `QuestPOIBlobID` int NOT NULL DEFAULT '0',
  `AreaPOIIconEnum` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `soulbind`;
CREATE TABLE `soulbind` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CovenantID` int NOT NULL DEFAULT '0',
  `GarrTalentTreeID` int NOT NULL DEFAULT '0',
  `CreatureID` int NOT NULL DEFAULT '0',
  `GarrFollowerID` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_ability_category_locale`;
CREATE TABLE `garr_ability_category_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_encounter_locale`;
CREATE TABLE `garr_encounter_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_mechanic_type_locale`;
CREATE TABLE `garr_mechanic_type_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `garr_talent_locale`;
CREATE TABLE `garr_talent_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `group_finder_activity_locale`;
CREATE TABLE `group_finder_activity_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `FullName_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ShortName_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_branch_locale`;
CREATE TABLE `research_branch_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_project_locale`;
CREATE TABLE `research_project_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_site_locale`;
CREATE TABLE `research_site_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `soulbind_locale`;
CREATE TABLE `soulbind_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0'
,  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ---- column reconciliation (existing tables missing columns) ----
ALTER TABLE `exterior_component` ADD COLUMN `Size` tinyint unsigned NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `HouseExteriorWmoDataID` int unsigned NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `ParentComponentID` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `ModelFileDataID` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `Field_7` tinyint unsigned NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `Field_9` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `GameObjectID` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `Field_11` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `ItemID` int NOT NULL DEFAULT '0';
ALTER TABLE `warband_scene_placement_filter_req` ADD COLUMN `Field_11_1_0_58221_003_0` int NOT NULL DEFAULT '0';
ALTER TABLE `warband_scene_placement_filter_req` ADD COLUMN `Field_11_1_0_58221_003_1` int NOT NULL DEFAULT '0';

