--
-- Table structure for table `instance_encounter_timeline`
--
-- Feeds the SMSG_INSTANCE_ENCOUNTER_EVENT_* family. There is no EncounterTimeline.db2 - the client only
-- knows EncounterEvent.db2 (spell, icon, severity, icon mask), the order and the timing of an encounter
-- timeline are entirely server defined.
--
DROP TABLE IF EXISTS `instance_encounter_timeline`;
CREATE TABLE `instance_encounter_timeline` (
  `DungeonEncounterID` int unsigned NOT NULL,
  `Index` int unsigned NOT NULL DEFAULT '0',
  `EncounterEventID` int unsigned NOT NULL DEFAULT '0' COMMENT 'EncounterEvent.db2 ID, must belong to DungeonEncounterID',
  `Delay` int unsigned NOT NULL DEFAULT '0' COMMENT 'milliseconds after the encounter started',
  `Duration` int unsigned NOT NULL DEFAULT '0' COMMENT 'milliseconds the cast or effect itself takes',
  `IsApproximation` tinyint unsigned NOT NULL DEFAULT '0',
  `Flags` int unsigned NOT NULL DEFAULT '0' COMMENT 'bit 0 makes the client skip the entry',
  PRIMARY KEY (`DungeonEncounterID`,`Index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
