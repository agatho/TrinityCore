-- ---------------------------------------------------------------------------
-- Housing System - Hotfix Database Tables
--
-- These tables back the DB2 stores used by the housing system.
-- They belong in the `hotfixes` database and must be executed AFTER
-- the base hotfixes_database.sql schema has been applied.
--
-- 21 base tables + 10 locale tables = 31 total
-- ---------------------------------------------------------------------------

-- Table structure for table `data_tag_x_house_decor_record`
--

DROP TABLE IF EXISTS `data_tag_x_house_decor_record`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `data_tag_x_house_decor_record` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `DataTagID` int NOT NULL DEFAULT '0',
  `HouseDecorID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `decor_category`
--

DROP TABLE IF EXISTS `decor_category`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `decor_category` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `DisplayIndex` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `decor_category_locale`
--

DROP TABLE IF EXISTS `decor_category_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `decor_category_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `decor_dye_slot`
--

DROP TABLE IF EXISTS `decor_dye_slot`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `decor_dye_slot` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SlotIndex` int NOT NULL DEFAULT '0',
  `HouseDecorID` int unsigned NOT NULL DEFAULT '0',
  `DyeChannelType` int NOT NULL DEFAULT '0',
  `DefaultDyeRecordID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `decor_subcategory`
--

DROP TABLE IF EXISTS `decor_subcategory`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `decor_subcategory` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `DecorCategoryID` int unsigned NOT NULL DEFAULT '0',
  `DisplayIndex` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `decor_subcategory_locale`
--

DROP TABLE IF EXISTS `decor_subcategory_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `decor_subcategory_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `decor_x_decor_subcategory`
--

DROP TABLE IF EXISTS `decor_x_decor_subcategory`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `decor_x_decor_subcategory` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `DecorSubcategoryID` int unsigned NOT NULL DEFAULT '0',
  `HouseDecorID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house`
--

DROP TABLE IF EXISTS `house`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `InternalName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `HouseTypeID` int NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_decor`
--

DROP TABLE IF EXISTS `house_decor`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_decor` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `InitialRotationX` float NOT NULL DEFAULT '0',
  `InitialRotationY` float NOT NULL DEFAULT '0',
  `InitialRotationZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Field_003` int NOT NULL DEFAULT '0',
  `GameObjectID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `ModelType` tinyint unsigned NOT NULL DEFAULT '0',
  `ModelFileDataID` int NOT NULL DEFAULT '0',
  `ThumbnailFileDataID` int NOT NULL DEFAULT '0',
  `WeightCost` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `InitialScale` float NOT NULL DEFAULT '0',
  `FirstAcquisitionBonus` int NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `Size` tinyint NOT NULL DEFAULT '0',
  `StartingQuantity` int NOT NULL DEFAULT '0',
  `UiModelSceneID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_decor_locale`
--

DROP TABLE IF EXISTS `house_decor_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_decor_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_decor_material`
--

DROP TABLE IF EXISTS `house_decor_material`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_decor_material` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MaterialGUID` bigint unsigned NOT NULL DEFAULT '0',
  `HouseDecorID` int NOT NULL DEFAULT '0',
  `MaterialIndex` int NOT NULL DEFAULT '0',
  `DefaultDyeID` int NOT NULL DEFAULT '0',
  `AllowedDyeMask` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_decor_theme_set`
--

DROP TABLE IF EXISTS `house_decor_theme_set`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_decor_theme_set` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ThemeID` int NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_decor_theme_set_locale`
--

DROP TABLE IF EXISTS `house_decor_theme_set_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_decor_theme_set_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_exterior_wmo_data`
--

DROP TABLE IF EXISTS `house_exterior_wmo_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_exterior_wmo_data` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_exterior_wmo_data_locale`
--

DROP TABLE IF EXISTS `house_exterior_wmo_data_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_exterior_wmo_data_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_level_data`
--

DROP TABLE IF EXISTS `house_level_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_level_data` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Level` int NOT NULL DEFAULT '0',
  `QuestID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_level_reward_info`
--

DROP TABLE IF EXISTS `house_level_reward_info`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_level_reward_info` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `HouseLevelID` int NOT NULL DEFAULT '0',
  `RewardType` int NOT NULL DEFAULT '0',
  `RewardValue` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_level_reward_info_locale`
--

DROP TABLE IF EXISTS `house_level_reward_info_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_level_reward_info_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_room`
--

DROP TABLE IF EXISTS `house_room`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_room` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Size` tinyint NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Field_002` int NOT NULL DEFAULT '0',
  `RoomWmoDataID` int NOT NULL DEFAULT '0',
  `UiTextureAtlasElementID` int NOT NULL DEFAULT '0',
  `WeightCost` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_room_locale`
--

DROP TABLE IF EXISTS `house_room_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_room_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_theme`
--

DROP TABLE IF EXISTS `house_theme`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_theme` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `CategoryID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `house_theme_locale`
--

DROP TABLE IF EXISTS `house_theme_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `house_theme_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `neighborhood_initiative`
--

DROP TABLE IF EXISTS `neighborhood_initiative`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `neighborhood_initiative` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `InitiativeType` int NOT NULL DEFAULT '0',
  `Duration` int NOT NULL DEFAULT '0',
  `RequiredParticipants` int NOT NULL DEFAULT '0',
  `RewardCurrencyID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `neighborhood_initiative_locale`
--

DROP TABLE IF EXISTS `neighborhood_initiative_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `neighborhood_initiative_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;


--
-- Table structure for table `neighborhood_map`
--

DROP TABLE IF EXISTS `neighborhood_map`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `neighborhood_map` (
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `Radius` float NOT NULL DEFAULT '0',
  `PlotCount` int unsigned NOT NULL DEFAULT '0',
  `FactionRestriction` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Data for table `neighborhood_map`
-- Map 2736: Horde "Razorwind Shores" neighborhood
--

DELETE FROM `neighborhood_map` WHERE `ID` IN (1, 2, 7);
INSERT INTO `neighborhood_map` (`PositionX`, `PositionY`, `PositionZ`, `ID`, `MapID`, `Radius`, `PlotCount`, `FactionRestriction`, `VerifiedBuild`) VALUES
-- ID=1: Horde (Map 2736 Razorwind Shores), FactionRestriction=6 (0x2 Horde | 0x4 SystemGenerate)
(1100, 200, 50, 1, 2736, 1500, 55, 6, 56263),
-- ID=2: Alliance (Map 2735 Founder's Point), FactionRestriction=5 (0x1 Alliance | 0x4 SystemGenerate)
(2997, 435, 114, 2, 2735, 1500, 55, 5, 56263),
-- ID=7: House Interior (Map 2783), both factions, Flags=3 (0x1|0x2)
(-1000, -1000, 0.1, 7, 2783, 1500, 0, 3, 56263);

--
-- Table structure for table `neighborhood_name_gen`
--

DROP TABLE IF EXISTS `neighborhood_name_gen`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `neighborhood_name_gen` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Prefix` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Suffix` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `FullName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `NeighborhoodMapID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Data for table `neighborhood_name_gen`
-- No hotfix data needed — the base NeighborhoodNameGen.db2 (from client CASC data) contains
-- the actual localized name parts (Prefix, Suffix, FullName) for all supported UI languages.
-- The server reads entry IDs from the base DB2, stores neighborhood names in "ID1-ID2-ID3"
-- token format, and the client resolves each token to localized text using its local DB2 copy.
-- Retail sniff verified: Alliance "75-78-61", Horde "86-90-6" (12.0.1.65940)

--
-- Table structure for table `neighborhood_name_gen_locale`
--

DROP TABLE IF EXISTS `neighborhood_name_gen_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `neighborhood_name_gen_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Prefix_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Suffix_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `FullName_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `neighborhood_plot`
--

DROP TABLE IF EXISTS `neighborhood_plot`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `neighborhood_plot` (
  `Cost` bigint unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `HousePositionX` float NOT NULL DEFAULT '0',
  `HousePositionY` float NOT NULL DEFAULT '0',
  `HousePositionZ` float NOT NULL DEFAULT '0',
  `HouseRotationX` float NOT NULL DEFAULT '0',
  `HouseRotationY` float NOT NULL DEFAULT '0',
  `HouseRotationZ` float NOT NULL DEFAULT '0',
  `CornerstonePositionX` float NOT NULL DEFAULT '0',
  `CornerstonePositionY` float NOT NULL DEFAULT '0',
  `CornerstonePositionZ` float NOT NULL DEFAULT '0',
  `CornerstoneRotationX` float NOT NULL DEFAULT '0',
  `CornerstoneRotationY` float NOT NULL DEFAULT '0',
  `CornerstoneRotationZ` float NOT NULL DEFAULT '0',
  `TeleportPositionX` float NOT NULL DEFAULT '0',
  `TeleportPositionY` float NOT NULL DEFAULT '0',
  `TeleportPositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `NeighborhoodMapID` int unsigned NOT NULL DEFAULT '0',
  `Field_010` int NOT NULL DEFAULT '0',
  `CornerstoneGameObjectID` int NOT NULL DEFAULT '0',
  `PlotIndex` int NOT NULL DEFAULT '0',
  `WorldState` int NOT NULL DEFAULT '0',
  `PlotGameObjectID` int NOT NULL DEFAULT '0',
  `TeleportFacing` float NOT NULL DEFAULT '0',
  `Field_016` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Data for table `neighborhood_plot`
-- 55 plots for NeighborhoodMapID=1 (Horde), 55 for NeighborhoodMapID=2 (Alliance), 4 for NeighborhoodMapID=4 (test)
-- Positions extracted from retail NeighborhoodPlot.db2 (build 65617)
-- HousePosition, CornerstonePosition, and TeleportPosition are now DISTINCT per plot
--

DELETE FROM `neighborhood_plot` WHERE `NeighborhoodMapID` IN (1, 2, 4);
INSERT INTO `neighborhood_plot` (`Cost`, `Name`, `HousePositionX`, `HousePositionY`, `HousePositionZ`, `HouseRotationX`, `HouseRotationY`, `HouseRotationZ`, `CornerstonePositionX`, `CornerstonePositionY`, `CornerstonePositionZ`, `CornerstoneRotationX`, `CornerstoneRotationY`, `CornerstoneRotationZ`, `TeleportPositionX`, `TeleportPositionY`, `TeleportPositionZ`, `ID`, `NeighborhoodMapID`, `Field_010`, `CornerstoneGameObjectID`, `PlotIndex`, `WorldState`, `PlotGameObjectID`, `TeleportFacing`, `Field_016`, `VerifiedBuild`) VALUES
(10000000, '', 1289.8300, 1126.3199, 57.0599, 0.0000, 0.0000, 4.0490, 1256.1805, 1091.0452, 55.7604, 0.0000, 0.0000, 1.6406, 1256.3898, 1088.0525, 55.6567, 359, 1, 0, 475055, 0, 29778, 581892, 1.5708, 582099, 65617),
(10000000, '', 1239.0938, -199.7500, 74.0640, 0.0000, 0.0000, 0.0000, 772.8715, -477.5243, 11.7049, 0.0000, 0.0000, 4.1452, 774.4834, -474.9941, 11.7055, 360, 1, 0, 475085, 1, 29725, 582077, -1.5708, 577376, 65617),
(10000000, '', 1236.4062, -56.7031, 76.3940, 0.0000, 0.0000, 0.0000, 1925.2101, 948.8993, 101.6459, 0.0000, 0.0000, 1.9111, 1926.2115, 946.0714, 101.6422, 361, 1, 0, 475054, 2, 29736, 581890, 1.5708, 577377, 65617),
(10000000, '', 1320.1528, -73.6458, 74.2065, 0.0000, 0.0000, 0.0000, 791.3958, -208.5312, 15.6007, 0.0000, 0.0000, 3.4819, 794.2238, -207.5298, 15.6172, 362, 1, 0, 475094, 3, 29745, 582075, 1.5708, 577378, 65617),
(10000000, '', 1499.4045, 59.0816, 77.1373, 0.0000, 0.0000, 0.0000, 1607.9132, -242.9219, 76.2830, 0.0000, 0.0000, 3.3859, 1610.8241, -242.1961, 76.2838, 363, 1, 0, 475067, 4, 29755, 582042, -1.5708, 577379, 65617),
(10000000, '', 1675.2916, -190.5139, 78.3776, 0.0000, 0.0000, 0.0000, 733.8542, -434.1736, 2.3941, 0.0000, 0.0000, 2.4696, 736.2020, -436.0412, 3.1479, 364, 1, 0, 475106, 5, 29763, 582074, 4.7124, 577380, 65617),
(10000000, '', 1543.5868, -33.6406, 78.4231, 0.0000, 0.0000, 0.0000, 649.9757, 39.9340, 3.9184, 0.0000, 0.0000, 3.0805, 652.9701, 39.7509, 4.1798, 365, 1, 0, 475076, 6, 29765, 582072, -1.5708, 577381, 65617),
(10000000, '', 1149.9861, -109.1250, 73.9779, 0.0000, 0.0000, 0.0000, 574.2917, -316.3924, 7.1435, 0.0000, 0.0000, 2.1817, 576.0124, -318.8498, 6.8227, 366, 1, 0, 569611, 7, 29766, 582064, 0.0000, 577382, 65617),
(10000000, '', 1621.5500, -266.9930, 74.5204, 0.0000, 0.0000, 0.0000, 1343.7760, -635.8611, 21.1667, 0.0000, 0.0000, 5.3494, 1341.9916, -633.4496, 21.2839, 367, 1, 0, 475072, 8, 29767, 582048, 1.5708, 577383, 65617),
(10000000, '', 798.4844, -264.9045, 33.8333, 0.0000, 0.0000, 0.0000, 1668.4062, 1053.1858, 48.4670, 0.0000, 0.0000, 1.0734, 1666.9747, 1050.5493, 48.5917, 368, 1, 0, 475057, 9, 29768, 581895, -1.5708, 577384, 65617),
(10000000, '', 988.7743, 154.7934, 31.3477, 0.0000, 0.0000, 0.0000, 1266.1024, -171.2188, 72.9337, 0.0000, 0.0000, 4.1102, 1267.8016, -168.7464, 73.0510, 369, 1, 0, 475095, 10, 29726, 581854, -1.5708, 577385, 65617),
(10000000, '', 2104.5330, -298.5885, 136.8018, 0.0000, 0.0000, 0.0000, 1101.6841, 883.5469, 6.8333, 0.0000, 0.0000, 3.2550, 1104.6648, 883.8865, 6.8333, 370, 1, 0, 475061, 11, 29727, 582059, -1.5708, 577386, 65617),
(10000000, '', 503.5434, -81.2135, 1.4695, 0.0000, 0.0000, 0.0000, 519.7344, 693.2031, 155.5208, 0.0000, 0.0000, 2.6005, 547.1517, 638.7143, 154.8102, 371, 1, 0, 582082, 12, 29728, 582067, 0.0000, 577387, 65617),
(10000000, '', 673.8420, 11.3559, 22.7558, 0.0000, 0.0000, 0.0000, 902.3055, -545.7639, 1.4306, 0.0000, 0.0000, 4.5902, 902.6711, -542.7863, 1.9622, 372, 1, 0, 475091, 13, 29729, 582073, 1.5708, 577388, 65617),
(10000000, '', 1883.1788, -282.5799, 130.3434, 0.0000, 0.0000, 0.0000, 1180.1493, -643.6736, 5.7066, 0.0000, 0.0000, 4.8258, 1179.8097, -640.6929, 5.5850, 373, 1, 0, 475079, 14, 29730, 582049, 0.0000, 577389, 65617),
(10000000, '', 632.6736, -132.3368, 3.6205, 0.0000, 0.0000, 0.0000, 2150.5642, -740.0920, 141.6667, 0.0000, 0.0000, 2.6965, 2153.2720, -741.3835, 142.7517, 374, 1, 0, 475071, 15, 29731, 582045, 0.0000, 577390, 65617),
(10000000, '', 416.8889, -121.2083, 4.2780, 0.0000, 0.0000, 0.0000, 1493.4132, 7.5816, 78.3281, 0.0000, 0.0000, 5.6549, 1481.1500, -7.5293, 78.2176, 375, 1, 0, 576758, 16, 29732, 581853, -4.7124, 577391, 65617),
(10000000, '', 1368.7240, 230.9010, 44.7524, 0.0000, 0.0000, 0.0000, 958.1059, 843.5764, 4.7882, 0.0000, 0.0000, 2.2340, 959.9529, 841.2124, 4.7887, 376, 1, 0, 567160, 17, 29733, 582066, -1.5708, 577392, 65617),
(10000000, '', 1728.6545, -874.8160, 116.4909, 0.0000, 0.0000, 0.0000, 1531.5330, 998.0191, 36.2552, 0.0000, 0.0000, 6.2483, 1528.5348, 998.1238, 35.6213, 377, 1, 0, 475059, 18, 29734, 581896, 1.5708, 577393, 65617),
(10000000, '', 1934.4548, -541.8073, 135.9320, 0.0000, 0.0000, 0.0000, 380.6649, -107.6580, 6.0000, 0.0000, 0.0000, 3.1329, 383.6648, -107.6842, 6.4980, 378, 1, 0, 569610, 19, 29735, 582069, 4.7124, 577394, 65617),
(10000000, '', 1629.9497, -688.8802, 97.8434, 0.0000, 0.0000, 0.0000, 1864.5416, -515.5191, 130.2292, 0.0000, 0.0000, 2.5307, 1866.9990, -517.2398, 130.5387, 379, 1, 0, 475092, 20, 29737, 582050, 4.7124, 577395, 65617),
(10000000, '', 795.0643, -388.5990, 40.5091, 0.0000, 0.0000, 0.0000, 1658.4149, -169.7552, 81.0330, 0.0000, 0.0000, 4.5640, 1658.8584, -166.7882, 81.0233, 380, 1, 0, 475096, 21, 29738, 582102, -3.1416, 577396, 65617),
(10000000, '', 964.8559, -497.9826, 37.9859, 0.0000, 0.0000, 0.0000, 488.3160, 304.3420, 99.2083, 0.0000, 0.0000, 5.6287, 485.5970, 305.6099, 99.2687, 381, 1, 0, 475086, 22, 29739, 582062, 1.5708, 577397, 65617),
(10000000, '', 1960.0903, -706.0486, 133.2011, 0.0000, 0.0000, 0.0000, 1925.5990, -707.2083, 134.0330, 0.0000, 0.0000, 6.0388, 1922.6881, -706.4825, 133.9816, 382, 1, 0, 475080, 23, 29740, 582051, -1.5708, 577398, 65617),
(10000000, '', 1102.0243, 148.5156, 33.9287, 0.0000, 0.0000, 0.0000, 224.1267, 652.3698, 12.1858, 0.0000, 0.0000, 3.2812, 227.0975, 652.7874, 11.7489, 383, 1, 0, 566793, 24, 29741, 582076, 0.0000, 577399, 65617),
(10000000, '', 1833.0764, -491.0347, 133.2444, 0.0000, 0.0000, 0.0000, 1844.2587, 664.7674, 88.5258, 0.0000, 0.0000, 0.3054, 1841.3975, 663.8653, 88.3913, 384, 1, 0, 475051, 25, 29742, 581889, -1.5708, 577400, 65617),
(10000000, '', 1274.1945, -626.5035, 37.3102, 0.0000, 0.0000, 0.0000, 1078.1823, 1023.2518, 3.1476, 0.0000, 0.0000, 2.4435, 1080.4803, 1021.3234, 3.6968, 385, 1, 0, 475063, 26, 29743, 582078, -4.7124, 577401, 65617),
(10000000, '', 1955.7466, -447.7865, 137.5916, 0.0000, 0.0000, 0.0000, 1213.3298, 462.8090, 154.0208, 0.0000, 0.0000, 3.1416, 1166.0948, 461.5803, 153.9143, 386, 1, 0, 475064, 27, 29724, 581864, -1.5708, 577402, 65617),
(10000000, '', 1384.3073, 119.8872, 49.9795, 0.0000, 0.0000, 0.0000, 1663.2812, 915.1875, 37.6111, 0.0000, 0.0000, 1.4399, 1662.8896, 912.2131, 37.5281, 387, 1, 0, 475062, 28, 29744, 581897, 0.0000, 577403, 65617),
(10000000, '', 1175.2379, -600.1893, 37.2994, 0.0000, 0.0000, 0.0000, 1165.7274, 156.8663, 34.9566, 0.0000, 0.0000, 3.9706, 1167.7542, 159.0782, 34.9572, 388, 1, 0, 475104, 29, 29718, 581866, 0.5236, 577404, 65617),
(10000000, '', 1595.3993, -601.1476, 97.8912, 0.0000, 0.0000, 0.0000, 952.5903, 483.5799, 108.6701, 0.0000, 0.0000, 2.5307, 955.5142, 474.2964, 108.5331, 389, 1, 0, 475060, 30, 29747, 582080, 4.7124, 577405, 65617),
(10000000, '', 1729.5122, -541.5504, 104.1350, 0.0000, 0.0000, 0.0000, 2079.9619, -272.8559, 138.3177, 0.0000, 0.0000, 6.1610, 2076.9844, -272.4903, 138.3172, 390, 1, 0, 475098, 31, 29746, 582041, -1.5708, 577406, 65617),
(10000000, '', 1771.2274, -953.0833, 116.4910, 0.0000, 0.0000, 0.0000, 669.4583, 432.2535, 9.0017, 0.0000, 0.0000, 4.2673, 670.7499, 434.9612, 9.0179, 391, 1, 0, 475081, 32, 29748, 582068, 0.0000, 577407, 65617),
(10000000, '', 1858.2344, -873.9202, 121.2669, 0.0000, 0.0000, 0.0000, 1129.5192, 770.1805, 17.6215, 0.0000, 0.0000, 0.6370, 1127.1075, 768.3961, 17.6097, 392, 1, 0, 475053, 33, 29749, 581891, -1.5708, 577408, 65617),
(10000000, '', 2101.4270, -201.2951, 136.8017, 0.0000, 0.0000, 0.0000, 1309.7327, -105.3681, 73.2639, 0.0000, 0.0000, 1.7104, 1310.1501, -108.3389, 73.1513, 393, 1, 0, 475073, 34, 29750, 581855, 0.0000, 577409, 65617),
(10000000, '', 599.2674, -273.6424, 4.2802, 0.0000, 0.0000, 0.0000, 1788.5903, -951.0660, 92.3142, 0.0000, 0.0000, 4.2499, 1789.9290, -948.3812, 92.4790, 394, 1, 0, 475102, 35, 29751, 582052, 0.0000, 577410, 65617),
(10000000, '', 875.5243, -467.3576, 40.5584, 0.0000, 0.0000, 0.0000, 1802.2135, 639.2188, 85.6007, 0.0000, 0.0000, 4.2062, 1803.6680, 641.8426, 84.8968, 395, 1, 0, 475066, 36, 29752, 581894, -4.7124, 577411, 65617),
(10000000, '', 1188.1007, 479.7882, 122.2396, 0.0000, 0.0000, 0.0000, 304.6562, 79.8490, 24.3628, 0.0000, 0.0000, 2.8885, 307.5607, 79.0978, 23.3639, 396, 1, 0, 569609, 37, 29753, 582079, 4.7124, 577412, 65617),
(10000000, '', 1006.4601, 920.8212, 7.8578, 0.0000, 0.0000, 0.0000, 1846.5000, -879.5261, 120.7118, 0.0000, 0.0000, 5.6374, 1844.1041, -877.7206, 120.7860, 397, 1, 0, 475077, 38, 29754, 582053, -1.5708, 577413, 65617),
(10000000, '', 563.4236, 549.2014, 83.2300, 0.0000, 0.0000, 0.0000, 2235.2551, -871.0590, 160.2274, 0.0000, 0.0000, 5.0615, 2234.2290, -868.2399, 160.2278, 398, 1, 0, 582083, 39, 29720, 582046, -1.5708, 577414, 65617),
(10000000, '', 1032.3680, 660.5364, 9.1143, 0.0000, 0.0000, 0.0000, 1023.8299, 164.7917, 31.1076, 0.0000, 0.0000, 3.5430, 1026.5914, 165.9639, 31.0341, 399, 1, 0, 475090, 40, 29757, 582071, 0.0000, 577415, 65617),
(10000000, '', 1877.4966, 682.2327, 94.3097, 0.0000, 0.0000, 0.0000, 1339.8959, 616.3472, 42.0035, 0.0000, 0.0000, 4.5640, 1340.3394, 619.3143, 42.0044, 400, 1, 0, 475058, 41, 29756, 581893, -1.5708, 577416, 65617),
(10000000, '', 1345.1979, 577.5312, 42.0379, 0.0000, 0.0000, 0.0000, 1757.3854, -865.4340, 116.7535, 0.0000, 0.0000, 3.2899, 1760.3524, -864.9906, 116.5586, 401, 1, 0, 475100, 42, 29722, 582055, -1.5708, 577417, 65617),
(10000000, '', 1560.4375, 1002.4427, 37.3809, 0.0000, 0.0000, 0.0000, 639.8663, 692.5156, 113.9045, 0.0000, 0.0000, 2.6005, 642.2975, 706.8755, 113.9177, 402, 1, 0, 582085, 43, 29664, 582060, 0.0000, 577418, 65617),
(10000000, '', 1100.0312, 965.1511, 8.1755, 0.0000, 0.0000, 0.0000, 1903.5747, -448.2951, 137.0035, 0.0000, 0.0000, 0.0436, 1900.5775, -448.4260, 136.8943, 403, 1, 0, 475068, 44, 29723, 582039, 1.5708, 577419, 65617),
(10000000, '', 1676.1510, 950.6059, 37.4254, 0.0000, 0.0000, 0.0000, 1883.4618, -337.9045, 129.9809, 0.0000, 0.0000, 1.5795, 1883.4880, -340.9044, 129.6943, 404, 1, 0, 475097, 45, 29758, 582040, 0.0000, 577420, 65617),
(10000000, '', 1667.9288, 1084.5173, 45.9020, 0.0000, 0.0000, 0.0000, 2269.2761, -586.6858, 161.4757, 0.0000, 0.0000, 0.6545, 2266.8960, -588.5121, 160.8353, 405, 1, 0, 475083, 46, 29759, 582044, 1.5708, 577421, 65617),
(10000000, 'NEEDS REVIEW', 861.3733, 478.0590, 69.0120, 0.0000, 0.0000, 0.0000, 1366.4080, 85.1840, 49.7309, 0.0000, 0.0000, 0.5061, 1363.7841, 83.7296, 50.0440, 406, 1, 0, 475093, 47, 29760, 581865, 0.0000, 577422, 65617),
(10000000, '', 689.9653, 445.4879, 7.0797, 0.0000, 0.0000, 0.0000, 1215.6442, -9.7222, 104.4635, 0.0000, 0.0000, 0.9861, 1213.9884, -12.2239, 104.1680, 407, 1, 0, 475088, 48, 29761, 581863, -1.5708, 577423, 65617),
(10000000, '', 1159.8993, 787.7691, 17.5986, 0.0000, 0.0000, 0.0000, 412.0156, 194.8854, 109.7587, 0.0000, 0.0000, 4.5466, 403.9472, 195.3314, 109.7337, 408, 1, 0, 475087, 49, 29762, 582061, -4.7124, 577424, 65617),
(10000000, '', 997.0886, 738.8802, 9.4841, 0.0000, 0.0000, 0.0000, 1032.7848, 695.1458, 9.0087, 0.0000, 0.0000, 4.4506, 1033.5613, 698.0436, 9.0572, 409, 1, 0, 475052, 50, 29719, 582065, -1.5708, 577425, 65617),
(10000000, '', 945.2847, 511.0070, 69.5377, 0.0000, 0.0000, 0.0000, 637.6632, -114.5069, 2.9462, 0.0000, 0.0000, 3.2638, 640.6409, -114.1413, 2.6085, 410, 1, 0, 475105, 51, 29769, 582063, 0.0000, 577426, 65617),
(10000000, '', 617.6285, 631.9462, 81.6513, 0.0000, 0.0000, 0.0000, 1665.5017, -677.5955, 99.7431, 0.0000, 0.0000, 3.5256, 1668.2832, -676.4717, 99.8794, 411, 1, 0, 475099, 52, 29770, 582047, -1.5708, 577427, 65617),
(10000000, '', 927.5330, 819.7344, 7.1143, 0.0000, 0.0000, 0.0000, 1640.0226, -567.5486, 99.9861, 0.0000, 0.0000, 3.1241, 1643.0221, -567.6010, 100.0086, 412, 1, 0, 475103, 53, 29771, 582054, 0.0000, 577428, 65617),
(10000000, '', 1926.6024, 969.8594, 93.4767, 0.0000, 0.0000, 0.0000, 745.5347, 840.3368, 8.7535, 0.0000, 0.0000, 3.0543, 748.5233, 840.0753, 7.9814, 413, 1, 0, 582084, 54, 29772, 582070, -1.5708, 577429, 65617),
(10000000, '', 2973.0791, -245.3229, 113.1710, 0.0000, 0.0000, 0.0000, 2942.7439, -214.2656, 113.1128, 0.0000, 0.0000, 5.8381, 2939.3438, -212.2554, 113.3210, 417, 2, 0, 527268, 0, 29778, 581971, 0.0000, 581984, 65617),
(10000000, '', 2706.0791, 646.4271, 47.6890, 0.0000, 0.0000, 0.0000, 2674.3176, 651.8455, 49.2257, 0.0000, 0.0000, 5.2709, 2675.4250, 649.3803, 49.5350, 418, 2, 0, 527310, 1, 29725, 582014, 0.0000, 582019, 65617),
(10000000, '', 3453.3525, 1058.7153, 25.2093, 0.0000, 0.0000, 0.0000, 2523.3455, 171.1788, 60.4097, 0.0000, 0.0000, 0.3142, 2516.8928, 161.0453, 60.2432, 419, 2, 0, 527266, 2, 29736, 581968, 0.0000, 581989, 65617),
(10000000, '', 3196.8196, -501.5833, 190.8007, 0.0000, 0.0000, 0.0000, 3166.5469, -525.7153, 190.6510, 0.0000, 0.0000, 0.4014, 3157.2151, -526.4368, 190.9548, 420, 2, 0, 527292, 3, 29745, 582118, 0.0000, 582134, 65617),
(10000000, '', 3491.6597, 1300.3229, 17.0957, 0.0000, 0.0000, 0.0000, 3495.4878, 1400.8021, 4.2847, 0.0000, 0.0000, 4.6426, 3495.6970, 1403.7948, 4.9604, 421, 2, 0, 571209, 4, 29755, 581928, 0.0000, 581940, 65617),
(10000000, '', 2636.1875, 996.3559, 37.5333, 0.0000, 0.0000, 0.0000, 2615.6294, 965.4670, 36.8906, 0.0000, 0.0000, 0.5236, 2613.4175, 963.4402, 36.8871, 422, 2, 0, 527286, 5, 29763, 582024, 0.0000, 582015, 65617),
(10000000, '', 2710.4368, -428.7431, 122.5093, 0.0000, 0.0000, 0.0000, 2662.9531, -427.9879, 121.8333, 0.0000, 0.0000, 0.1309, 2663.6450, -444.1851, 121.0454, 423, 2, 0, 527260, 6, 29765, 581967, 0.0000, 581981, 65617),
(10000000, '', 3576.4504, 234.5955, 102.3955, 0.0000, 0.0000, 0.0000, 3581.9280, 265.0070, 102.3160, 0.0000, 0.0000, 4.9393, 3569.2817, 268.8878, 102.0982, 424, 2, 0, 527282, 7, 29766, 582116, 0.0000, 582129, 65617),
(10000000, '', 2329.7432, 34.2205, 100.2062, 0.0000, 0.0000, 0.0000, 2351.6614, -11.6042, 100.1423, 0.0000, 0.0000, 2.1817, 2358.5857, -3.7607, 100.0837, 425, 2, 0, 527274, 8, 29767, 581960, 0.0000, 581977, 65617),
(10000000, '', 3524.3047, 342.8611, 95.5538, 0.0000, 0.0000, 0.0000, 3547.5149, 322.2708, 95.7101, 0.0000, 0.0000, 2.3213, 3550.7900, 319.6280, 95.6286, 426, 2, 0, 527294, 9, 29768, 582119, 0.0000, 582137, 65617),
(10000000, '', 2427.0105, 5.3090, 100.0295, 0.0000, 0.0000, 0.0000, 2450.0540, 267.4462, 50.2309, 0.0000, 0.0000, 2.3562, 2456.3257, 284.0060, 49.6328, 427, 2, 0, 527301, 10, 29726, 581964, 0.0000, 581982, 65617),
(10000000, 'This Plot is staged', 2915.7578, 84.8854, 63.0604, 0.0000, 0.0000, 0.0000, 2937.3186, 122.1875, 62.7778, 0.0000, 0.0000, 3.1416, 2940.1553, 123.1642, 62.9803, 428, 2, 0, 527302, 11, 29727, 581990, 0.0000, 581991, 65617),
(10000000, '', 3326.5339, 1484.4810, 12.5477, 0.0000, 0.0000, 0.0000, 3340.2136, 1521.8298, 7.5035, 0.0000, 0.0000, 1.8588, 3341.0657, 1518.9534, 7.1467, 429, 2, 0, 571211, 12, 29728, 581935, 0.0000, 581943, 65617),
(10000000, '', 2844.9167, 910.2014, 28.9182, 0.0000, 0.0000, 0.0000, 2821.3098, 886.5312, 30.5833, 0.0000, 0.0000, 0.0785, 2818.3191, 886.2959, 30.5054, 430, 2, 0, 527311, 13, 29729, 582018, 0.0000, 582022, 65617),
(10000000, '', 2762.5720, 1066.3698, 24.7555, 0.0000, 0.0000, 0.0000, 2728.4653, 1070.7223, 27.2882, 0.0000, 0.0000, 5.4629, 2726.4194, 1072.9164, 27.5898, 431, 2, 0, 527265, 14, 29730, 582021, 0.0000, 582016, 65617),
(10000000, '', 2771.0461, 163.8160, 67.3829, 0.0000, 0.0000, 0.0000, 2767.0400, 122.0399, 66.8993, 0.0000, 0.0000, 1.1170, 2765.7249, 119.3435, 66.8710, 432, 2, 0, 527278, 15, 29731, 581969, 0.0000, 581976, 65617),
(10000000, '', 3010.6624, 66.3003, 65.9752, 0.0000, 0.0000, 0.0000, 2969.2170, 98.2552, 65.4271, 0.0000, 0.0000, 5.4192, 2966.3406, 99.1073, 64.9800, 433, 2, 0, 527272, 16, 29732, 581995, 0.0000, 581975, 65617),
(10000000, '', 2948.9932, -331.4618, 118.0991, 0.0000, 0.0000, 0.0000, 2938.3950, -343.1181, 121.4340, 0.0000, 0.0000, 5.5065, 2925.4436, -347.2237, 120.3004, 434, 2, 0, 527262, 17, 29733, 581963, 0.0000, 581980, 65617),
(10000000, '', 2253.0676, -232.0504, 117.5118, 0.0000, 0.0000, 0.0000, 2287.1008, -228.6111, 116.5278, 0.0000, 0.0000, 3.5779, 2284.6401, -217.2750, 116.2470, 435, 2, 0, 527277, 18, 29734, 581961, 0.0000, 581983, 65617),
(10000000, '', 3334.8733, 1133.4913, 19.1223, 0.0000, 0.0000, 0.0000, 3325.5608, 1102.2483, 13.0920, 0.0000, 0.0000, 2.2428, 3327.4285, 1099.9005, 13.0637, 436, 2, 0, 527295, 19, 29735, 581927, 0.0000, 581938, 65617),
(10000000, '', 2507.8855, -369.8542, 105.6292, 0.0000, 0.0000, 0.0000, 2471.4758, -331.3368, 101.6372, 0.0000, 0.0000, 2.8972, 2470.1252, -313.8795, 101.5918, 437, 2, 0, 527306, 20, 29737, 581970, 0.0000, 581978, 65617),
(10000000, '', 2609.5000, -486.5139, 122.4612, 0.0000, 0.0000, 0.0000, 2592.7700, -613.6649, 138.1771, 0.0000, 0.0000, 4.7647, 2592.1462, -610.7305, 137.9399, 438, 2, 0, 527281, 21, 29738, 581972, 0.0000, 581986, 65617),
(10000000, '', 3038.5278, -566.7136, 214.2806, 0.0000, 0.0000, 0.0000, 3065.1250, -579.9080, 214.3472, 0.0000, 0.0000, 2.4871, 3068.9990, -582.7709, 214.5051, 439, 2, 0, 527314, 22, 29739, 581966, 0.0000, 581974, 65617),
(10000000, '', 3339.0088, -607.7292, 204.2868, 0.0000, 0.0000, 0.0000, 3309.4219, -594.9375, 208.5573, 0.0000, 0.0000, 5.1662, 3306.9348, -593.2599, 207.9628, 440, 2, 0, 527290, 23, 29740, 582121, 0.0000, 582138, 65617),
(10000000, 'Free to Take', 2856.7605, -263.1962, 114.7814, 0.0000, 0.0000, 0.0000, 3803.5017, 1159.1060, 8.7483, 0.0000, 0.0000, 0.0436, 3802.9023, 1153.3622, 8.7812, 441, 2, 0, 527303, 24, 29741, 581930, 0.0000, 581941, 65617),
(10000000, '', 3432.1025, 1436.6649, 8.4747, 0.0000, 0.0000, 0.0000, 3440.8447, 1465.8055, 8.3160, 0.0000, 0.0000, 1.2130, 3439.7942, 1462.9955, 8.0692, 442, 2, 0, 571207, 25, 29742, 581932, 0.0000, 581945, 65617),
(10000000, '', 2599.9758, 608.1129, 45.1349, 0.0000, 0.0000, 0.0000, 2615.2639, 633.7587, 47.7326, 0.0000, 0.0000, 4.9742, 2612.5295, 640.8834, 47.2089, 443, 2, 0, 527264, 26, 29743, 582020, 0.0000, 582013, 65617),
(10000000, '', 3672.7683, 244.0712, 104.3997, 0.0000, 0.0000, 0.0000, 3639.8560, 259.3715, 104.3819, 0.0000, 0.0000, 5.3145, 3639.5754, 260.0994, 104.3643, 444, 2, 0, 527261, 27, 29724, 582124, 0.0000, 582131, 65617),
(10000000, '', 3628.0676, 1325.2153, 29.6373, 0.0000, 0.0000, 0.0000, 3637.4385, 1363.4810, 33.6493, 0.0000, 0.0000, 1.4835, 3637.1770, 1360.4924, 33.8053, 445, 2, 0, 571210, 28, 29744, 573768, 0.0000, 581939, 65617),
(10000000, '', 3516.4817, 1185.6459, 16.9398, 0.0000, 0.0000, 0.0000, 3493.4080, 1175.1337, 2.3160, 0.0000, 0.0000, 0.1571, 3490.4448, 1174.6643, 2.3485, 446, 2, 0, 527309, 29, 29718, 581934, 0.0000, 581937, 65617),
(10000000, '', 2662.3567, 831.4653, 44.1460, 0.0000, 0.0000, 0.0000, 2686.6807, 813.0677, 43.7934, 0.0000, 0.0000, 3.1416, 2697.5820, 819.9475, 44.1294, 447, 2, 0, 527275, 30, 29747, 582017, 0.0000, 582025, 65617),
(10000000, '', 3352.4229, -331.1632, 182.6550, 0.0000, 0.0000, 0.0000, 3368.1372, -403.1129, 186.4288, 0.0000, 0.0000, 2.6878, 3366.7144, -401.5177, 186.4049, 448, 2, 0, 527279, 31, 29746, 582122, 0.0000, 582130, 65617),
(10000000, 'Free to Take', 3284.9792, -376.5347, 182.7944, 0.0000, 0.0000, 0.0000, 3136.1389, -650.2778, 226.3420, 0.0000, 0.0000, 5.6810, 3131.5793, -650.7773, 226.3266, 449, 2, 0, 527283, 32, 29748, 582120, 0.0000, 582136, 65617),
(10000000, '', 2510.0764, -53.8264, 95.4555, 0.0000, 0.0000, 0.0000, 2478.6094, -80.5521, 95.2361, 0.0000, 0.0000, 0.3752, 2479.9382, -87.5352, 95.2116, 450, 2, 0, 527285, 33, 29749, 581965, 0.0000, 581979, 65617),
(10000000, '', 3457.7900, 761.2396, 81.5264, 0.0000, 0.0000, 0.0000, 3461.7629, 728.4983, 81.5017, 0.0000, 0.0000, 2.3562, 3470.9348, 735.5818, 81.4804, 451, 2, 0, 527313, 34, 29750, 582117, 0.0000, 582132, 65617),
(10000000, '', 2605.9124, 905.7847, 39.1739, 0.0000, 0.0000, 0.0000, 2440.5659, 927.9305, 36.1719, 0.0000, 0.0000, 0.5149, 2440.7600, 929.1160, 35.1740, 452, 2, 0, 527291, 35, 29751, 582026, 0.0000, 582027, 65617),
(10000000, '', 2763.0920, 1170.6858, 25.8206, 0.0000, 0.0000, 0.0000, 2737.6250, 1142.7101, 24.9653, 0.0000, 0.0000, 0.3840, 2734.8435, 1141.5863, 24.9642, 453, 2, 0, 527271, 36, 29752, 581933, 0.0000, 581944, 65617),
(10000000, '', 2477.7432, 514.4462, 41.3818, 0.0000, 0.0000, 0.0000, 2510.3604, 542.6875, 41.1493, 0.0000, 0.0000, 3.9968, 2505.1978, 554.9092, 40.7759, 454, 2, 0, 527269, 37, 29753, 582008, 0.0000, 582023, 65617),
(10000000, '', 2571.6250, -299.3351, 105.4922, 0.0000, 0.0000, 0.0000, 2570.7310, -331.2118, 105.4635, 0.0000, 0.0000, 2.2602, 2581.0757, -334.2816, 106.2156, 455, 2, 0, 527308, 38, 29754, 581973, 0.0000, 581985, 65617),
(10000000, '', 3538.4089, 646.1580, 82.6428, 0.0000, 0.0000, 0.0000, 3602.2864, 642.9583, 83.3819, 0.0000, 0.0000, 1.7017, 3601.6370, 640.0294, 83.4679, 456, 2, 0, 527300, 39, 29720, 582125, 0.0000, 582112, 65617),
(10000000, '', 3558.4358, 1398.2483, 13.0545, 0.0000, 0.0000, 0.0000, 3522.7805, 1455.1510, 9.3854, 0.0000, 0.0000, 5.9603, 3519.9355, 1456.1029, 9.4271, 457, 2, 0, 571208, 40, 29757, 581931, 0.0000, 581942, 65617),
(10000000, '', 2157.5452, -327.3924, 98.6817, 0.0000, 0.0000, 0.0000, 2190.2969, -337.1493, 98.5191, 0.0000, 0.0000, 3.1329, 2190.6108, -336.1324, 98.4821, 458, 2, 0, 527297, 41, 29756, 581962, 0.0000, 581987, 65617),
(10000000, '', 3579.2578, 470.2986, 89.6703, 0.0000, 0.0000, 0.0000, 3597.5417, 501.8351, 89.5712, 0.0000, 0.0000, 4.8433, 3589.0215, 510.0056, 89.3633, 459, 2, 0, 527270, 42, 29722, 582123, 0.0000, 582135, 65617),
(10000000, '', 3669.5876, 1187.6250, 37.6142, 0.0000, 0.0000, 0.0000, 3645.6597, 1202.9098, 37.5295, 0.0000, 0.0000, 5.7247, 3643.1155, 1204.4995, 37.2111, 460, 2, 0, 527299, 43, 29664, 581929, 0.0000, 581958, 65617),
(10000000, '', 3618.1458, 361.4375, 96.5879, 0.0000, 0.0000, 0.0000, 3647.0618, 353.1823, 102.7257, 0.0000, 0.0000, 0.4625, 3646.6846, 346.1071, 102.6371, 461, 2, 0, 527293, 44, 29723, 582128, 0.0000, 582105, 65617),
(10000000, '', 3429.6831, 654.4601, 79.9498, 0.0000, 0.0000, 0.0000, 3453.2544, 669.9184, 86.5920, 0.0000, 0.0000, 3.4907, 3457.3501, 668.0790, 86.7388, 462, 2, 0, 527263, 45, 29758, 582126, 0.0000, 582127, 65617),
(10000000, '', 3198.8899, 365.1302, 65.7900, 0.0000, 0.0000, 0.0000, 3167.9358, 364.0139, 73.8837, 0.0000, 0.0000, 0.0262, 3164.2666, 348.9492, 73.8583, 463, 2, 0, 527276, 46, 29759, 582108, 0.0000, 582107, 65617),
(10000000, '', 3333.0391, 401.8854, 69.8326, 0.0000, 0.0000, 0.0000, 3315.5149, 427.7812, 71.9219, 0.0000, 0.0000, 4.3808, 3317.8132, 429.8803, 71.9239, 464, 2, 0, 527288, 47, 29760, 582111, 0.0000, 582110, 65617),
(10000000, '', 3038.4861, 861.5660, 74.8149, 0.0000, 0.0000, 0.0000, 3047.4011, 833.1476, 74.6771, 0.0000, 0.0000, 1.6319, 3048.7029, 824.3778, 74.6531, 465, 2, 0, 527289, 48, 29761, 582011, 0.0000, 582012, 65617),
(10000000, '', 3092.3542, 407.0955, 66.3347, 0.0000, 0.0000, 0.0000, 3119.0190, 394.8698, 74.2622, 0.0000, 0.0000, 3.0107, 3130.8457, 405.7466, 74.0092, 466, 2, 0, 527267, 49, 29762, 581993, 0.0000, 581994, 65617),
(10000000, '', 3268.7378, 1029.9479, 20.3230, 0.0000, 0.0000, 0.0000, 3288.0227, 1002.5590, 22.7535, 0.0000, 0.0000, 2.4696, 3290.3706, 1000.6915, 22.6543, 467, 2, 0, 527280, 50, 29719, 582114, 0.0000, 582115, 65617),
(10000000, '', 3357.6536, 523.8125, 71.9354, 0.0000, 0.0000, 0.0000, 3348.1909, 487.5260, 72.3767, 0.0000, 0.0000, 1.7715, 3360.6262, 483.5812, 71.9281, 468, 2, 0, 527298, 51, 29769, 582106, 0.0000, 582109, 65617),
(10000000, '', 3386.4888, 979.7430, 29.0968, 0.0000, 0.0000, 0.0000, 3365.4114, 1004.1684, 32.8021, 0.0000, 0.0000, 5.6898, 3362.9243, 1005.8459, 32.0761, 469, 2, 0, 527304, 52, 29770, 582113, 0.0000, 582133, 65617),
(10000000, '', 2970.9861, 796.3924, 74.7324, 0.0000, 0.0000, 0.0000, 3003.0591, 772.6493, 74.5642, 0.0000, 0.0000, 3.8659, 3009.3625, 794.7646, 74.5634, 470, 2, 0, 527284, 53, 29771, 582010, 0.0000, 582009, 65617),
(10000000, '', 3157.6321, 252.0035, 69.2628, 0.0000, 0.0000, 0.0000, 3151.5903, 297.6250, 73.4253, 0.0000, 0.0000, 4.7560, 3151.4595, 300.6221, 73.3356, 471, 2, 0, 527305, 54, 29772, 581959, 0.0000, 581992, 65617),
(100, 'Plotty Mc Plotington', 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, -967.0000, -700.0000, 0.6815, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 737, 4, 0, 0, 0, 0, 0, 0.0000, 576482, 65617),
(100, 'Plotty Mc Plotington 2nd', 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, -967.0000, -833.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 738, 4, 0, 0, 1, 0, 0, 0.0000, 576484, 65617),
(100, 'Plotty Mc Plotington 3rd', 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, -833.0000, -700.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 739, 4, 0, 0, 2, 0, 0, 0.0000, 576380, 65617),
(0, 'Plotty Mc Plotington 4th', 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, -833.0000, -833.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 740, 4, 0, 0, 3, 0, 0, 0.0000, 576483, 65617);



--
-- Table structure for table `room_component`
--

DROP TABLE IF EXISTS `room_component`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `room_component` (
  `OffsetPosX` float NOT NULL DEFAULT '0',
  `OffsetPosY` float NOT NULL DEFAULT '0',
  `OffsetPosZ` float NOT NULL DEFAULT '0',
  `OffsetRotX` float NOT NULL DEFAULT '0',
  `OffsetRotY` float NOT NULL DEFAULT '0',
  `OffsetRotZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `RoomWmoDataID` int unsigned NOT NULL DEFAULT '0',
  `ModelFileDataID` int NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `MeshStyleFilterID` int NOT NULL DEFAULT '0',
  `ConnectionType` tinyint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `room_component_option`
--

DROP TABLE IF EXISTS `room_component_option`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `room_component_option` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `SubType` tinyint unsigned NOT NULL DEFAULT '0',
  `ModelFileDataID` int NOT NULL DEFAULT '0',
  `RoomComponentID` int NOT NULL DEFAULT '0',
  `MeshStyleFilterID` int NOT NULL DEFAULT '0',
  `HouseThemeID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `room_wmo_data`
--

DROP TABLE IF EXISTS `room_wmo_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `room_wmo_data` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BoundingBoxMinX` float NOT NULL DEFAULT '0',
  `BoundingBoxMinY` float NOT NULL DEFAULT '0',
  `BoundingBoxMinZ` float NOT NULL DEFAULT '0',
  `BoundingBoxMaxX` float NOT NULL DEFAULT '0',
  `BoundingBoxMaxY` float NOT NULL DEFAULT '0',
  `BoundingBoxMaxZ` float NOT NULL DEFAULT '0',
  `Height` float NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component`
--

DROP TABLE IF EXISTS `exterior_component`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `FileDataID` int NOT NULL DEFAULT '0',
  `ConditionID` int NOT NULL DEFAULT '0',
  `HookID` int NOT NULL DEFAULT '0',
  `Flags` tinyint unsigned NOT NULL DEFAULT '0',
  `Slot` tinyint unsigned NOT NULL DEFAULT '0',
  `SortOrder` int NOT NULL DEFAULT '0',
  `ComponentGroupID` int NOT NULL DEFAULT '0',
  `UiTextureKitID` int NOT NULL DEFAULT '0',
  `ExteriorComponentTypeID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_locale`
--

DROP TABLE IF EXISTS `exterior_component_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_exit_point`
--

DROP TABLE IF EXISTS `exterior_component_exit_point`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_exit_point` (
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `RotationX` float NOT NULL DEFAULT '0',
  `RotationY` float NOT NULL DEFAULT '0',
  `RotationZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ExteriorComponentID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_group`
--

DROP TABLE IF EXISTS `exterior_component_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_group` (
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ExteriorComponentID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_group_x_hook`
--

DROP TABLE IF EXISTS `exterior_component_group_x_hook`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_group_x_hook` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ExteriorComponentGroupID` int unsigned NOT NULL DEFAULT '0',
  `ExteriorComponentHookID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_hook`
--

DROP TABLE IF EXISTS `exterior_component_hook`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_hook` (
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `RotationX` float NOT NULL DEFAULT '0',
  `RotationY` float NOT NULL DEFAULT '0',
  `RotationZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ExteriorComponentTypeID` int NOT NULL DEFAULT '0',
  `ExteriorComponentID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_type`
--

DROP TABLE IF EXISTS `exterior_component_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_type` (
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_type_locale`
--

DROP TABLE IF EXISTS `exterior_component_type_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_type_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exterior_component_x_group`
--

DROP TABLE IF EXISTS `exterior_component_x_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exterior_component_x_group` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ExteriorComponentID` int NOT NULL DEFAULT '0',
  `ExteriorComponentGroupID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
