--
-- Table structure for table `encounter_event`
--
-- EncounterEvent.db2 (FileDataID 7571075, layout 0x54A32DBB) is the only DB2 whose contents travel on the
-- SMSG_INSTANCE_ENCOUNTER_EVENT_* wire. TrinityCore already served the file but never loaded it.
--
CREATE TABLE IF NOT EXISTS `encounter_event` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `Unknown1200` int NOT NULL DEFAULT '0',
  `BroadcastTextID` int NOT NULL DEFAULT '0',
  `Severity` tinyint NOT NULL DEFAULT '0',
  `Unknown1200_2` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `DungeonEncounterID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
