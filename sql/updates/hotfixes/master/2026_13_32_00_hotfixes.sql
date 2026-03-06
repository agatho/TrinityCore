--
-- Table structure for table `drive_capability`
--

DROP TABLE IF EXISTS `drive_capability`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `drive_capability` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ForwardAcceleration` float NOT NULL DEFAULT '0',
  `BackwardMaxSpeed` float NOT NULL DEFAULT '0',
  `IdleFriction` float NOT NULL DEFAULT '0',
  `BackwardAcceleration` float NOT NULL DEFAULT '0',
  `Field_5` float NOT NULL DEFAULT '0',
  `Field_6` float NOT NULL DEFAULT '0',
  `Field_7` float NOT NULL DEFAULT '0',
  `Field_8` float NOT NULL DEFAULT '0',
  `Field_9` float NOT NULL DEFAULT '0',
  `Field_10` float NOT NULL DEFAULT '0',
  `Field_11` float NOT NULL DEFAULT '0',
  `Field_12` float NOT NULL DEFAULT '0',
  `Field_13` float NOT NULL DEFAULT '0',
  `Field_14` float NOT NULL DEFAULT '0',
  `Field_15` float NOT NULL DEFAULT '0',
  `Field_16` float NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `drive_capability_tier`
--

DROP TABLE IF EXISTS `drive_capability_tier`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `drive_capability_tier` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Acceleration` float NOT NULL DEFAULT '0',
  `MaxSpeed` float NOT NULL DEFAULT '0',
  `DriveCapabilityID` int NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
