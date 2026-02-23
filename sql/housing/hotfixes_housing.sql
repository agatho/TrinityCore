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

DELETE FROM `neighborhood_map` WHERE `ID` IN (1, 2);
INSERT INTO `neighborhood_map` (`PositionX`, `PositionY`, `PositionZ`, `ID`, `MapID`, `Radius`, `PlotCount`, `FactionRestriction`, `VerifiedBuild`) VALUES
(1100, 200, 50, 1, 2736, 1500, 55, 0, 56263),
(2997, 435, 114, 2, 2735, 1500, 55, 0, 56263);

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
-- 55 plots for NeighborhoodMapID=1 (Map 2736, Horde Razorwind Shores)
-- Positions sourced from cornerstone gameobject spawns (GUIDs 9000009-9000063)
-- Cost = 10000000 copper (1000 gold)
--

DELETE FROM `neighborhood_plot` WHERE `NeighborhoodMapID` IN (1, 2);
INSERT INTO `neighborhood_plot` (`Cost`, `Name`, `HousePositionX`, `HousePositionY`, `HousePositionZ`, `HouseRotationX`, `HouseRotationY`, `HouseRotationZ`, `CornerstonePositionX`, `CornerstonePositionY`, `CornerstonePositionZ`, `CornerstoneRotationX`, `CornerstoneRotationY`, `CornerstoneRotationZ`, `TeleportPositionX`, `TeleportPositionY`, `TeleportPositionZ`, `ID`, `NeighborhoodMapID`, `Field_010`, `CornerstoneGameObjectID`, `PlotIndex`, `WorldState`, `PlotGameObjectID`, `TeleportFacing`, `Field_016`, `VerifiedBuild`) VALUES
(10000000, '', 1668.4062, 1053.1858, 48.467014, 0, 0, 4.214974, 1668.4062, 1053.1858, 48.467014, 0, 0, 4.214974, 1668.4062, 1053.1858, 48.467014, 1, 1, 0, 457142, 0, 0, 417487, 4.214974, 0, 56263),
(10000000, '', 1531.5330, 998.0191, 36.255207, 0, 0, 3.106652, 1531.5330, 998.0191, 36.255207, 0, 0, 3.106652, 1531.5330, 998.0191, 36.255207, 2, 1, 0, 457142, 1, 0, 417487, 3.106652, 0, 56263),
(10000000, '', 1343.7760, -635.8612, 21.166666, 0, 0, 2.207840, 1343.7760, -635.8612, 21.166666, 0, 0, 2.207840, 1343.7760, -635.8612, 21.166666, 3, 1, 0, 457142, 2, 0, 417487, 2.207840, 0, 56263),
(10000000, '', 772.8715, -477.5243, 11.704862, 0, 0, 1.003564, 772.8715, -477.5243, 11.704862, 0, 0, 1.003564, 772.8715, -477.5243, 11.704862, 4, 1, 0, 457142, 3, 0, 417487, 1.003564, 0, 56263),
(10000000, '', 1180.1493, -643.6737, 5.706597, 0, 0, 1.684243, 1180.1493, -643.6737, 5.706597, 0, 0, 1.684243, 1180.1493, -643.6737, 5.706597, 5, 1, 0, 457142, 4, 0, 417487, 1.684243, 0, 56263),
(10000000, '', 1101.6841, 883.5469, 6.833334, 0, 0, 0.113445, 1101.6841, 883.5469, 6.833334, 0, 0, 0.113445, 1101.6841, 883.5469, 6.833334, 6, 1, 0, 457142, 5, 0, 417487, 0.113445, 0, 56263),
(10000000, '', 1215.6442, -9.7222, 104.463540, 0, 0, 4.127711, 1215.6442, -9.7222, 104.463540, 0, 0, 4.127711, 1215.6442, -9.7222, 104.463540, 7, 1, 0, 457142, 6, 0, 417487, 4.127711, 0, 56263),
(10000000, '', 733.8542, -434.1736, 2.394097, 0, 0, 5.611235, 733.8542, -434.1736, 2.394097, 0, 0, 5.611235, 733.8542, -434.1736, 2.394097, 8, 1, 0, 457142, 7, 0, 417487, 5.611235, 0, 56263),
(10000000, '', 1078.1823, 1023.2518, 3.147569, 0, 0, 5.585054, 1078.1823, 1023.2518, 3.147569, 0, 0, 5.585054, 1078.1823, 1023.2518, 3.147569, 9, 1, 0, 457142, 8, 0, 417487, 5.585054, 0, 56263),
(10000000, '', 1023.8299, 164.7917, 31.107640, 0, 0, 0.401425, 1023.8299, 164.7917, 31.107640, 0, 0, 0.401425, 1023.8299, 164.7917, 31.107640, 10, 1, 0, 457142, 9, 0, 417487, 0.401425, 0, 56263),
(10000000, '', 1493.4132, 7.5816, 78.328125, 0, 0, 2.513274, 1493.4132, 7.5816, 78.328125, 0, 0, 2.513274, 1493.4132, 7.5816, 78.328125, 11, 1, 0, 457142, 10, 0, 417487, 2.513274, 0, 56263),
(10000000, '', 1309.7327, -105.3681, 73.263870, 0, 0, 4.852020, 1309.7327, -105.3681, 73.263870, 0, 0, 4.852020, 1309.7327, -105.3681, 73.263870, 12, 1, 0, 457142, 11, 0, 417487, 4.852020, 0, 56263),
(10000000, '', 649.9757, 39.9340, 3.918403, 0, 0, 6.222101, 649.9757, 39.9340, 3.918403, 0, 0, 6.222101, 649.9757, 39.9340, 3.918403, 13, 1, 0, 457142, 12, 0, 417487, 6.222101, 0, 56263),
(10000000, '', 1607.9132, -242.9219, 76.282990, 0, 0, 0.244346, 1607.9132, -242.9219, 76.282990, 0, 0, 0.244346, 1607.9132, -242.9219, 76.282990, 14, 1, 0, 457142, 13, 0, 417487, 0.244346, 0, 56263),
(10000000, '', 574.2917, -316.3924, 7.143509, 0, 0, 5.323256, 574.2917, -316.3924, 7.143509, 0, 0, 5.323256, 574.2917, -316.3924, 7.143509, 15, 1, 0, 457142, 14, 0, 417487, 5.323256, 0, 56263),
(10000000, '', 745.5347, 840.3368, 8.753472, 0, 0, 6.195921, 745.5347, 840.3368, 8.753472, 0, 0, 6.195921, 745.5347, 840.3368, 8.753472, 16, 1, 0, 457142, 15, 0, 417487, 6.195921, 0, 56263),
(10000000, '', 791.3958, -208.5313, 15.600695, 0, 0, 0.340338, 791.3958, -208.5313, 15.600695, 0, 0, 0.340338, 791.3958, -208.5313, 15.600695, 17, 1, 0, 457142, 16, 0, 417487, 0.340338, 0, 56263),
(10000000, '', 1339.8959, 616.3472, 42.003475, 0, 0, 1.422443, 1339.8959, 616.3472, 42.003475, 0, 0, 1.422443, 1339.8959, 616.3472, 42.003475, 18, 1, 0, 457142, 17, 0, 417487, 1.422443, 0, 56263),
(10000000, '', 1788.5903, -951.0660, 92.314240, 0, 0, 1.108283, 1788.5903, -951.0660, 92.314240, 0, 0, 1.108283, 1788.5903, -951.0660, 92.314240, 19, 1, 0, 457142, 18, 0, 417487, 1.108283, 0, 56263),
(10000000, '', 1864.5416, -515.5191, 130.229170, 0, 0, 5.672322, 1864.5416, -515.5191, 130.229170, 0, 0, 5.672322, 1864.5416, -515.5191, 130.229170, 20, 1, 0, 457142, 19, 0, 417487, 5.672322, 0, 56263),
(10000000, '', 1846.5000, -879.5261, 120.711810, 0, 0, 2.495818, 1846.5000, -879.5261, 120.711810, 0, 0, 2.495818, 1846.5000, -879.5261, 120.711810, 21, 1, 0, 457142, 20, 0, 417487, 2.495818, 0, 56263),
(10000000, '', 1663.2812, 915.1875, 37.611115, 0, 0, 4.581496, 1663.2812, 915.1875, 37.611115, 0, 0, 4.581496, 1663.2812, 915.1875, 37.611115, 22, 1, 0, 457142, 21, 0, 417487, 4.581496, 0, 56263),
(10000000, '', 1165.7274, 156.8663, 34.956596, 0, 0, 0.829030, 1165.7274, 156.8663, 34.956596, 0, 0, 0.829030, 1165.7274, 156.8663, 34.956596, 23, 1, 0, 457142, 22, 0, 417487, 0.829030, 0, 56263),
(10000000, '', 1256.1805, 1091.0452, 55.918404, 0, 0, 4.782205, 1256.1805, 1091.0452, 55.918404, 0, 0, 4.782205, 1256.1805, 1091.0452, 55.918404, 24, 1, 0, 457142, 23, 0, 417487, 4.782205, 0, 56263),
(10000000, '', 1925.2101, 948.8993, 101.645850, 0, 0, 5.052732, 1925.2101, 948.8993, 101.645850, 0, 0, 5.052732, 1925.2101, 948.8993, 101.645850, 25, 1, 0, 457142, 24, 0, 417487, 5.052732, 0, 56263),
(10000000, '', 2269.2761, -586.6858, 161.475700, 0, 0, 3.796098, 2269.2761, -586.6858, 161.475700, 0, 0, 3.796098, 2269.2761, -586.6858, 161.475700, 26, 1, 0, 457142, 25, 0, 417487, 3.796098, 0, 56263),
(10000000, '', 2079.9620, -272.8559, 138.317700, 0, 0, 3.019413, 2079.9620, -272.8559, 138.317700, 0, 0, 3.019413, 2079.9620, -272.8559, 138.317700, 27, 1, 0, 457142, 26, 0, 417487, 3.019413, 0, 56263),
(10000000, '', 544.6215, 637.1024, 155.302080, 0, 0, 0.567232, 544.6215, 637.1024, 155.302080, 0, 0, 0.567232, 544.6215, 637.1024, 155.302080, 28, 1, 0, 457142, 27, 0, 417487, 0.567232, 0, 56263),
(10000000, '', 1844.2587, 664.7674, 88.525760, 0, 0, 3.447027, 1844.2587, 664.7674, 88.525760, 0, 0, 3.447027, 1844.2587, 664.7674, 88.525760, 29, 1, 0, 457142, 28, 0, 417487, 3.447027, 0, 56263),
(10000000, '', 2150.5642, -740.0920, 141.666670, 0, 0, 5.838129, 2150.5642, -740.0920, 141.666670, 0, 0, 5.838129, 2150.5642, -740.0920, 141.666670, 30, 1, 0, 457142, 29, 0, 417487, 5.838129, 0, 56263),
(10000000, '', 1883.4618, -337.9045, 129.980910, 0, 0, 4.721121, 1883.4618, -337.9045, 129.980910, 0, 0, 4.721121, 1883.4618, -337.9045, 129.980910, 31, 1, 0, 457142, 30, 0, 417487, 4.721121, 0, 56263),
(10000000, '', 2235.2551, -871.0590, 160.227430, 0, 0, 1.919862, 2235.2551, -871.0590, 160.227430, 0, 0, 1.919862, 2235.2551, -871.0590, 160.227430, 32, 1, 0, 457142, 31, 0, 417487, 1.919862, 0, 56263),
(10000000, '', 1032.7848, 695.1458, 9.008680, 0, 0, 1.308995, 1032.7848, 695.1458, 9.008680, 0, 0, 1.308995, 1032.7848, 695.1458, 9.008680, 33, 1, 0, 457142, 32, 0, 417487, 1.308995, 0, 56263),
(10000000, '', 902.3055, -545.7639, 1.430556, 0, 0, 1.448622, 902.3055, -545.7639, 1.430556, 0, 0, 1.448622, 902.3055, -545.7639, 1.430556, 34, 1, 0, 457142, 33, 0, 417487, 1.448622, 0, 56263),
(10000000, '', 1166.5642, 464.5434, 154.020830, 0, 0, 4.555311, 1166.5642, 464.5434, 154.020830, 0, 0, 4.555311, 1166.5642, 464.5434, 154.020830, 35, 1, 0, 457142, 34, 0, 417487, 4.555311, 0, 56263),
(10000000, '', 1366.4080, 85.1840, 49.730904, 0, 0, 3.647741, 1366.4080, 85.1840, 49.730904, 0, 0, 3.647741, 1366.4080, 85.1840, 49.730904, 36, 1, 0, 457142, 35, 0, 417487, 3.647741, 0, 56263),
(10000000, '', 1903.5747, -448.2951, 137.003480, 0, 0, 3.185267, 1903.5747, -448.2951, 137.003480, 0, 0, 3.185267, 1903.5747, -448.2951, 137.003480, 37, 1, 0, 457142, 36, 0, 417487, 3.185267, 0, 56263),
(10000000, '', 1640.0226, -567.5487, 99.986115, 0, 0, 6.265733, 1640.0226, -567.5487, 99.986115, 0, 0, 6.265733, 1640.0226, -567.5487, 99.986115, 38, 1, 0, 457142, 37, 0, 417487, 6.265733, 0, 56263),
(10000000, '', 1925.5990, -707.2083, 134.032990, 0, 0, 2.897245, 1925.5990, -707.2083, 134.032990, 0, 0, 2.897245, 1925.5990, -707.2083, 134.032990, 39, 1, 0, 457142, 38, 0, 417487, 2.897245, 0, 56263),
(10000000, '', 1658.4149, -169.7552, 81.032990, 0, 0, 1.422443, 1658.4149, -169.7552, 81.032990, 0, 0, 1.422443, 1658.4149, -169.7552, 81.032990, 40, 1, 0, 457142, 39, 0, 417487, 1.422443, 0, 56263),
(10000000, '', 1266.1024, -171.2188, 72.933730, 0, 0, 0.968657, 1266.1024, -171.2188, 72.933730, 0, 0, 0.968657, 1266.1024, -171.2188, 72.933730, 41, 1, 0, 457142, 40, 0, 417487, 0.968657, 0, 56263),
(10000000, '', 669.4583, 432.2535, 9.001737, 0, 0, 1.125737, 669.4583, 432.2535, 9.001737, 0, 0, 1.125737, 669.4583, 432.2535, 9.001737, 42, 1, 0, 457142, 41, 0, 417487, 1.125737, 0, 56263),
(10000000, '', 1665.5017, -677.5955, 99.743060, 0, 0, 0.383971, 1665.5017, -677.5955, 99.743060, 0, 0, 0.383971, 1665.5017, -677.5955, 99.743060, 43, 1, 0, 457142, 42, 0, 417487, 0.383971, 0, 56263),
(10000000, '', 1757.3854, -865.4340, 116.753470, 0, 0, 0.148352, 1757.3854, -865.4340, 116.753470, 0, 0, 0.148352, 1757.3854, -865.4340, 116.753470, 44, 1, 0, 457142, 43, 0, 417487, 0.148352, 0, 56263),
(10000000, '', 488.3160, 304.3420, 99.208336, 0, 0, 2.705255, 488.3160, 304.3420, 99.208336, 0, 0, 2.705255, 488.3160, 304.3420, 99.208336, 45, 1, 0, 457142, 44, 0, 417487, 2.705255, 0, 56263),
(10000000, '', 958.1059, 843.5764, 4.788195, 0, 0, 5.375617, 958.1059, 843.5764, 4.788195, 0, 0, 5.375617, 958.1059, 843.5764, 4.788195, 46, 1, 0, 457142, 45, 0, 417487, 5.375617, 0, 56263),
(10000000, '', 639.6737, 708.3299, 113.782990, 0, 0, 5.777043, 639.6737, 708.3299, 113.782990, 0, 0, 5.777043, 639.6737, 708.3299, 113.782990, 47, 1, 0, 457142, 46, 0, 417487, 5.777043, 0, 56263),
(10000000, '', 955.0191, 477.2552, 108.661460, 0, 0, 4.878198, 955.0191, 477.2552, 108.661460, 0, 0, 4.878198, 955.0191, 477.2552, 108.661460, 48, 1, 0, 457142, 47, 0, 417487, 4.878198, 0, 56263),
(10000000, '', 637.6632, -114.5069, 2.946181, 0, 0, 0.122173, 637.6632, -114.5069, 2.946181, 0, 0, 0.122173, 637.6632, -114.5069, 2.946181, 49, 1, 0, 457142, 48, 0, 417487, 0.122173, 0, 56263),
(10000000, '', 1802.2135, 639.2188, 85.600690, 0, 0, 1.064650, 1802.2135, 639.2188, 85.600690, 0, 0, 1.064650, 1802.2135, 639.2188, 85.600690, 50, 1, 0, 457142, 49, 0, 417487, 1.064650, 0, 56263),
(10000000, '', 1129.5192, 770.1805, 17.621529, 0, 0, 3.778642, 1129.5192, 770.1805, 17.621529, 0, 0, 3.778642, 1129.5192, 770.1805, 17.621529, 51, 1, 0, 457142, 50, 0, 417487, 3.778642, 0, 56263),
(10000000, '', 403.6076, 192.3507, 109.798610, 0, 0, 1.457349, 403.6076, 192.3507, 109.798610, 0, 0, 1.457349, 403.6076, 192.3507, 109.798610, 52, 1, 0, 457142, 51, 0, 417487, 1.457349, 0, 56263),
(10000000, '', 380.6650, -107.6580, 6.752863, 0, 0, 6.274459, 380.6650, -107.6580, 6.752863, 0, 0, 6.274459, 380.6650, -107.6580, 6.752863, 53, 1, 0, 457142, 52, 0, 417487, 6.274459, 0, 56263),
(10000000, '', 304.6563, 79.8490, 24.362848, 0, 0, 6.030115, 304.6563, 79.8490, 24.362848, 0, 0, 6.030115, 304.6563, 79.8490, 24.362848, 54, 1, 0, 457142, 53, 0, 417487, 6.030115, 0, 56263),
(10000000, '', 224.1267, 652.3698, 12.185764, 0, 0, 0.139624, 224.1267, 652.3698, 12.185764, 0, 0, 0.139624, 224.1267, 652.3698, 12.185764, 55, 1, 0, 457142, 54, 0, 417487, 0.139624, 0, 56263),
-- Alliance Founder's Point (Map 2735) - NeighborhoodMapID=2, IDs 56-110
(10000000, '', 2941.2317, -214.5868, 113.15452, 0, 0, 2.2514734, 2941.2317, -214.5868, 113.15452, 0, 0, 2.2514734, 2941.2317, -214.5868, 113.15452, 56, 2, 0, 457142, 0, 0, 417487, 2.2514734, 0, 56263),
(10000000, '', 2678.3794, 648.8594, 49.677082, 0, 0, 2.967041, 2678.3794, 648.8594, 49.677082, 0, 0, 2.967041, 2678.3794, 648.8594, 49.677082, 57, 2, 0, 457142, 1, 0, 417487, 2.967041, 0, 56263),
(10000000, '', 2519.8472, 160.5243, 60.381947, 0, 0, 2.967041, 2519.8472, 160.5243, 60.381947, 0, 0, 2.967041, 2519.8472, 160.5243, 60.381947, 58, 2, 0, 457142, 2, 0, 417487, 2.967041, 0, 56263),
(10000000, '', 3160.2058, -526.2014, 190.55382, 0, 0, 3.2201612, 3160.2058, -526.2014, 190.55382, 0, 0, 3.2201612, 3160.2058, -526.2014, 190.55382, 59, 2, 0, 457142, 3, 0, 417487, 3.2201612, 0, 56263),
(10000000, '', 3495.4878, 1400.8021, 4.2847223, 0, 0, 1.5009829, 3495.4878, 1400.8021, 4.2847223, 0, 0, 1.5009829, 3495.4878, 1400.8021, 4.2847223, 60, 2, 0, 457142, 4, 0, 417487, 1.5009829, 0, 56263),
(10000000, '', 2615.6294, 965.46704, 36.84028, 0, 0, 3.8833635, 2615.6294, 965.46704, 36.84028, 0, 0, 3.8833635, 2615.6294, 965.46704, 36.84028, 61, 2, 0, 457142, 5, 0, 417487, 3.8833635, 0, 56263),
(10000000, '', 2666.506, -443.283, 121.72222, 0, 0, 3.4470272, 2666.506, -443.283, 121.72222, 0, 0, 3.4470272, 2666.506, -443.283, 121.72222, 62, 2, 0, 457142, 6, 0, 417487, 3.4470272, 0, 56263),
(10000000, '', 3569.2031, 265.8889, 102.31598, 0, 0, 1.5446155, 3569.2031, 265.8889, 102.31598, 0, 0, 1.5446155, 3569.2031, 265.8889, 102.31598, 63, 2, 0, 457142, 7, 0, 417487, 1.5446155, 0, 56263),
(10000000, '', 2356.2708, -1.8524306, 100.08332, 0, 0, 5.5937834, 2356.2708, -1.8524306, 100.08332, 0, 0, 5.5937834, 2356.2708, -1.8524306, 100.08332, 64, 2, 0, 457142, 8, 0, 417487, 5.5937834, 0, 56263),
(10000000, '', 3546.7942, 321.49307, 95.71007, 0, 0, 5.462882, 3546.7942, 321.49307, 95.71007, 0, 0, 5.462882, 3546.7942, 321.49307, 95.71007, 65, 2, 0, 457142, 9, 0, 417487, 5.462882, 0, 56263),
(10000000, '', 2453.6892, 285.4375, 49.61111, 0, 0, 5.7857695, 2453.6892, 285.4375, 49.61111, 0, 0, 5.7857695, 2453.6892, 285.4375, 49.61111, 66, 2, 0, 457142, 10, 0, 417487, 5.7857695, 0, 56263),
(10000000, '', 2937.3186, 122.1875, 62.77778, 0, 0, 0.3316107, 2937.3186, 122.1875, 62.77778, 0, 0, 0.3316107, 2937.3186, 122.1875, 62.77778, 67, 2, 0, 457142, 11, 0, 417487, 0.3316107, 0, 56263),
(10000000, '', 3340.2136, 1521.8298, 7.5034723, 0, 0, 5.000373, 3340.2136, 1521.8298, 7.5034723, 0, 0, 5.000373, 3340.2136, 1521.8298, 7.5034723, 68, 2, 0, 457142, 12, 0, 417487, 5.000373, 0, 56263),
(10000000, '', 2821.3098, 886.53125, 30.583334, 0, 0, 3.2201612, 2821.3098, 886.53125, 30.583334, 0, 0, 3.2201612, 2821.3098, 886.53125, 30.583334, 69, 2, 0, 457142, 13, 0, 417487, 3.2201612, 0, 56263),
(10000000, '', 2728.4653, 1070.7223, 27.288195, 0, 0, 2.321287, 2728.4653, 1070.7223, 27.288195, 0, 0, 2.321287, 2728.4653, 1070.7223, 27.288195, 70, 2, 0, 457142, 14, 0, 417487, 2.321287, 0, 56263),
(10000000, '', 2767.04, 122.03993, 66.89931, 0, 0, 4.258607, 2767.04, 122.03993, 66.89931, 0, 0, 4.258607, 2767.04, 122.03993, 66.89931, 71, 2, 0, 457142, 15, 0, 417487, 4.258607, 0, 56263),
(10000000, '', 2969.217, 98.25521, 65.427086, 0, 0, 2.853604, 2969.217, 98.25521, 65.427086, 0, 0, 2.853604, 2969.217, 98.25521, 65.427086, 72, 2, 0, 457142, 16, 0, 417487, 2.853604, 0, 56263),
(10000000, '', 2927.5833, -349.3264, 121.12674, 0, 0, 2.3649182, 2927.5833, -349.3264, 121.12674, 0, 0, 2.3649182, 2927.5833, -349.3264, 121.12674, 73, 2, 0, 457142, 17, 0, 417487, 2.3649182, 0, 56263),
(10000000, '', 2281.6494, -217.51042, 116.24653, 0, 0, 0.0785381, 2281.6494, -217.51042, 116.24653, 0, 0, 0.0785381, 2281.6494, -217.51042, 116.24653, 74, 2, 0, 457142, 18, 0, 417487, 0.0785381, 0, 56263),
(10000000, '', 3325.5608, 1102.2483, 13.092014, 0, 0, 5.384347, 3325.5608, 1102.2483, 13.092014, 0, 0, 5.384347, 3325.5608, 1102.2483, 13.092014, 75, 2, 0, 457142, 19, 0, 417487, 5.384347, 0, 56263),
(10000000, '', 2467.1477, -313.5139, 101.59202, 0, 0, 6.1610146, 2467.1477, -313.5139, 101.59202, 0, 0, 6.1610146, 2467.1477, -313.5139, 101.59202, 76, 2, 0, 457142, 20, 0, 417487, 6.1610146, 0, 56263),
(10000000, '', 2592.77, -613.6649, 138.13716, 0, 0, 1.780234, 2592.77, -613.6649, 138.13716, 0, 0, 1.780234, 2592.77, -613.6649, 138.13716, 77, 2, 0, 457142, 21, 0, 417487, 1.780234, 0, 56263),
(10000000, '', 3066.9722, -580.559, 214.53473, 0, 0, 5.4541574, 3066.9722, -580.559, 214.53473, 0, 0, 5.4541574, 3066.9722, -580.559, 214.53473, 78, 2, 0, 457142, 22, 0, 417487, 5.4541574, 0, 56263),
(10000000, '', 3309.4219, -594.9375, 209.13368, 0, 0, 2.5481794, 3309.4219, -594.9375, 209.13368, 0, 0, 2.5481794, 3309.4219, -594.9375, 209.13368, 79, 2, 0, 457142, 23, 0, 417487, 2.5481794, 0, 56263),
(10000000, '', 3805.8994, 1153.493, 8.75, 0, 0, 3.1852667, 3805.8994, 1153.493, 8.75, 0, 0, 3.1852667, 3805.8994, 1153.493, 8.75, 80, 2, 0, 457142, 24, 0, 417487, 3.1852667, 0, 56263),
(10000000, '', 3440.8447, 1465.8055, 8.315972, 0, 0, 4.354601, 3440.8447, 1465.8055, 8.315972, 0, 0, 4.354601, 3440.8447, 1465.8055, 8.315972, 81, 2, 0, 457142, 25, 0, 417487, 4.354601, 0, 56263),
(10000000, '', 2610.4836, 638.6893, 47.387154, 0, 0, 0.82030326, 2610.4836, 638.6893, 47.387154, 0, 0, 0.82030326, 2610.4836, 638.6893, 47.387154, 82, 2, 0, 457142, 26, 0, 417487, 0.82030326, 0, 56263),
(10000000, '', 3641.8567, 258.15103, 104.38194, 0, 0, 2.4347348, 3641.8567, 258.15103, 104.38194, 0, 0, 2.4347348, 3641.8567, 258.15103, 104.38194, 83, 2, 0, 457142, 27, 0, 417487, 2.4347348, 0, 56263),
(10000000, '', 3637.4385, 1363.481, 33.743057, 0, 0, 4.625129, 3637.4385, 1363.481, 33.743057, 0, 0, 4.625129, 3637.4385, 1363.481, 33.743057, 84, 2, 0, 457142, 28, 0, 417487, 4.625129, 0, 56263),
(10000000, '', 3493.408, 1175.1337, 2.3159723, 0, 0, 3.2986872, 3493.408, 1175.1337, 2.3159723, 0, 0, 3.2986872, 3493.408, 1175.1337, 2.3159723, 85, 2, 0, 457142, 29, 0, 417487, 3.2986872, 0, 56263),
(10000000, '', 2694.933, 821.3559, 43.868057, 0, 0, 5.794494, 2694.933, 821.3559, 43.868057, 0, 0, 5.794494, 2694.933, 821.3559, 43.868057, 86, 2, 0, 457142, 30, 0, 417487, 5.794494, 0, 56263),
(10000000, '', 3363.8455, -400.64062, 186.40451, 0, 0, 5.98648, 3363.8455, -400.64062, 186.40451, 0, 0, 5.98648, 3363.8455, -400.64062, 186.40451, 87, 2, 0, 457142, 31, 0, 417487, 5.98648, 0, 56263),
(10000000, '', 3134.3706, -651.8768, 226.34203, 0, 0, 2.7663376, 3134.3706, -651.8768, 226.34203, 0, 0, 2.7663376, 3134.3706, -651.8768, 226.34203, 88, 2, 0, 457142, 32, 0, 417487, 2.7663376, 0, 56263),
(10000000, '', 2482.4253, -85.85764, 95.197914, 0, 0, 3.735006, 2482.4253, -85.85764, 95.197914, 0, 0, 3.735006, 2482.4253, -85.85764, 95.197914, 89, 2, 0, 457142, 33, 0, 417487, 3.735006, 0, 56263),
(10000000, '', 3468.8135, 737.7031, 81.50174, 0, 0, 5.497789, 3468.8135, 737.7031, 81.50174, 0, 0, 5.497789, 3468.8135, 737.7031, 81.50174, 90, 2, 0, 457142, 34, 0, 417487, 5.497789, 0, 56263),
(10000000, '', 2443.3057, 929.1007, 34.734375, 0, 0, 3.115388, 2443.3057, 929.1007, 34.734375, 0, 0, 3.115388, 2443.3057, 929.1007, 34.734375, 91, 2, 0, 457142, 35, 0, 417487, 3.115388, 0, 56263),
(10000000, '', 2737.625, 1142.7101, 24.965279, 0, 0, 3.5255723, 2737.625, 1142.7101, 24.965279, 0, 0, 3.5255723, 2737.625, 1142.7101, 24.965279, 92, 2, 0, 457142, 36, 0, 417487, 3.5255723, 0, 56263),
(10000000, '', 2502.7708, 553.1458, 41, 0, 0, 0.6283169, 2502.7708, 553.1458, 41, 0, 0, 0.6283169, 2502.7708, 553.1458, 41, 93, 2, 0, 457142, 37, 0, 417487, 0.6283169, 0, 56263),
(10000000, '', 2580.684, -331.30728, 105.79514, 0, 0, 4.8432918, 2580.684, -331.30728, 105.79514, 0, 0, 4.8432918, 2580.684, -331.30728, 105.79514, 94, 2, 0, 457142, 38, 0, 417487, 4.8432918, 0, 56263),
(10000000, '', 3602.2864, 642.9583, 83.38194, 0, 0, 4.4942265, 3602.2864, 642.9583, 83.38194, 0, 0, 4.4942265, 3602.2864, 642.9583, 83.38194, 95, 2, 0, 457142, 39, 0, 417487, 4.4942265, 0, 56263),
(10000000, '', 3522.7805, 1455.151, 9.385417, 0, 0, 2.8186984, 3522.7805, 1455.151, 9.385417, 0, 0, 2.8186984, 3522.7805, 1455.151, 9.385417, 96, 2, 0, 457142, 40, 0, 417487, 2.8186984, 0, 56263),
(10000000, '', 2188.6824, -338.43057, 98.46007, 0, 0, 0.8726639, 2188.6824, -338.43057, 98.46007, 0, 0, 0.8726639, 2188.6824, -338.43057, 98.46007, 97, 2, 0, 457142, 41, 0, 417487, 0.8726639, 0, 56263),
(10000000, '', 3587.9463, 507.20486, 89.57118, 0, 0, 1.2042773, 3587.9463, 507.20486, 89.57118, 0, 0, 1.2042773, 3587.9463, 507.20486, 89.57118, 98, 2, 0, 457142, 42, 0, 417487, 1.2042773, 0, 56263),
(10000000, '', 3645.6597, 1202.9098, 37.529514, 0, 0, 2.5830877, 3645.6597, 1202.9098, 37.529514, 0, 0, 2.5830877, 3645.6597, 1202.9098, 37.529514, 99, 2, 0, 457142, 43, 0, 417487, 2.5830877, 0, 56263),
(10000000, '', 3649.619, 346.7309, 102.72569, 0, 0, 3.3510466, 3649.619, 346.7309, 102.72569, 0, 0, 3.3510466, 3649.619, 346.7309, 102.72569, 100, 2, 0, 457142, 44, 0, 417487, 3.3510466, 0, 56263),
(10000000, '', 3453.2544, 669.9184, 86.59202, 0, 0, 0.34906524, 3453.2544, 669.9184, 86.59202, 0, 0, 0.34906524, 3453.2544, 669.9184, 86.59202, 101, 2, 0, 457142, 45, 0, 417487, 0.34906524, 0, 56263),
(10000000, '', 3167.2656, 349.02777, 73.93229, 0, 0, 3.1677976, 3167.2656, 349.02777, 73.93229, 0, 0, 3.1677976, 3167.2656, 349.02777, 73.93229, 102, 2, 0, 457142, 46, 0, 417487, 3.1677976, 0, 56263),
(10000000, '', 3318.1008, 426.8941, 71.921875, 0, 0, 1.6667867, 3318.1008, 426.8941, 71.921875, 0, 0, 1.6667867, 3318.1008, 426.8941, 71.921875, 103, 2, 0, 457142, 47, 0, 417487, 1.6667867, 0, 56263),
(10000000, '', 3048.6243, 827.3768, 74.65279, 0, 0, 4.7385726, 3048.6243, 827.3768, 74.65279, 0, 0, 4.7385726, 3048.6243, 827.3768, 74.65279, 104, 2, 0, 457142, 48, 0, 417487, 4.7385726, 0, 56263),
(10000000, '', 3128.4497, 407.5521, 74.21528, 0, 0, 5.637416, 3128.4497, 407.5521, 74.21528, 0, 0, 5.637416, 3128.4497, 407.5521, 74.21528, 105, 2, 0, 457142, 49, 0, 417487, 5.637416, 0, 56263),
(10000000, '', 3288.0227, 1002.559, 22.753473, 0, 0, 5.611235, 3288.0227, 1002.559, 22.753473, 0, 0, 5.611235, 3288.0227, 1002.559, 22.753473, 106, 2, 0, 457142, 50, 0, 417487, 5.611235, 0, 56263),
(10000000, '', 3361.25, 486.51562, 72.37674, 0, 0, 4.5029545, 3361.25, 486.51562, 72.37674, 0, 0, 4.5029545, 3361.25, 486.51562, 72.37674, 107, 2, 0, 457142, 51, 0, 417487, 4.5029545, 0, 56263),
(10000000, '', 3365.4114, 1004.1684, 32.802082, 0, 0, 2.5481794, 3365.4114, 1004.1684, 32.802082, 0, 0, 2.5481794, 3365.4114, 1004.1684, 32.802082, 108, 2, 0, 457142, 52, 0, 417487, 2.5481794, 0, 56263),
(10000000, '', 3006.3699, 794.97394, 74.56424, 0, 0, 6.2133737, 3006.3699, 794.97394, 74.56424, 0, 0, 6.2133737, 3006.3699, 794.97394, 74.56424, 109, 2, 0, 457142, 53, 0, 417487, 6.2133737, 0, 56263),
(10000000, '', 3151.5903, 297.625, 73.42535, 0, 0, 1.614428, 3151.5903, 297.625, 73.42535, 0, 0, 1.614428, 3151.5903, 297.625, 73.42535, 110, 2, 0, 457142, 54, 0, 417487, 1.614428, 0, 56263);

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
