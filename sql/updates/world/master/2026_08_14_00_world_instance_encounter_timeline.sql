-- Encounter timeline data source (gap G20).
--
-- Backs SMSG_INSTANCE_ENCOUNTER_EVENT_SEQUENCE / _APPEND / _CAST_UPDATE. Until this table exists the
-- opcodes are implemented but nothing ever populates them, so they put zero bytes on the wire in normal
-- play. Read by ObjectMgr::LoadInstanceEncounterTimeline, armed by InstanceScript::StartEncounterTimeline
-- on every pull.
--
-- ------------------------------------------------------------------------------------------------------
-- EncounterEventID is NOT ours to allocate.
-- ------------------------------------------------------------------------------------------------------
-- It is a row id of EncounterEvent.db2, a client-side table (12.0.7: 622 rows, ids 1-809, schema
-- WOWSTATIC_12_0_7_67808). The client resolves it through C_EncounterEvents.GetEventInfo(encounterEventID)
-- to that row's SpellID / Severity / BroadcastTextID / Flags for display, and its parent-lookup column is
-- the DungeonEncounterID, so the client already knows which events belong to which encounter.
--
-- An invented id would simply not resolve: C_EncounterEvents.HasEventInfo would return false and the
-- timeline entry would reach the client with no identity to draw. So every EncounterEventID below is
-- copied out of the client's own EncounterEvent.db2 for the matching DungeonEncounterID, together with the
-- SpellID, Severity, BroadcastTextID and Flags that row carries. Nothing here is synthesised, and there is
-- no private range in use - if you add rows, take the ids from that DB2 and not from a counter.
--
-- IconFileID 0 means "send the spell's own icon", which is what retail does: EncounterEvent.db2's
-- IconFileDataID column is 0 in 621 of its 622 rows, while every icon observed on the wire resolves to an
-- interface/icons/*.blp for the spell being announced.
--
-- FirstCastMs / RepeatCastMs are a prediction of the boss script's schedule and are only legitimate when
-- copied from that script. A timeline that disagrees with the encounter is worse than no timeline, because
-- the player plans around the countdown it draws.

DROP TABLE IF EXISTS `instance_encounter_timeline`;
CREATE TABLE `instance_encounter_timeline` (
  `DungeonEncounterID` int unsigned NOT NULL COMMENT 'DungeonEncounter.db2 ID',
  `DifficultyID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '0 = every difficulty',
  `EncounterEventID` int unsigned NOT NULL COMMENT 'EncounterEvent.db2 row id - see file header, do not invent',
  `SpellID` int unsigned NOT NULL COMMENT 'EncounterEvent.db2 SpellID for that row',
  `FirstCastMs` int unsigned NOT NULL COMMENT 'ms from the pull to the first cast',
  `RepeatCastMs` int unsigned NOT NULL DEFAULT 0 COMMENT '0 = fires once per pull',
  `MaxQueueDurationMs` int unsigned NOT NULL DEFAULT 5000 COMMENT 'hold window after the countdown; 5000 in every captured element',
  `Severity` tinyint unsigned NOT NULL DEFAULT 0 COMMENT 'EncounterEventSeverity: Low=0, Medium=1, High=2',
  `BroadcastTextID` int unsigned NOT NULL DEFAULT 0 COMMENT 'EncounterEvent.db2 BroadcastTextID',
  `IconFileID` int NOT NULL DEFAULT 0 COMMENT '0 = use the spell icon, as retail does',
  `Flags` int unsigned NOT NULL DEFAULT 0 COMMENT 'EncounterEvent.db2 Flags',
  `Comment` varchar(255) NOT NULL DEFAULT '' COMMENT 'provenance of the timing',
  PRIMARY KEY (`DungeonEncounterID`,`DifficultyID`,`EncounterEventID`,`FirstCastMs`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ------------------------------------------------------------------------------------------------------
-- Azhiccar - Eco-Dome Al'dani (DungeonEncounterID 3107)
-- ------------------------------------------------------------------------------------------------------
-- Chosen because it is the one encounter where all three sources agree: EncounterEvent.db2 has rows for
-- 3107, TrinityCore has a full script for it (src/server/scripts/Karesh/EcoDomeAldani/boss_azhiccar.cpp),
-- and the SpellIDs in those DB2 rows are the same spell ids that script actually casts. Nothing had to be
-- mapped, guessed or renumbered.
--
--   EncounterEvent 2   SpellID 1217327  Severity 1  = SPELL_INVADING_SHRIEK_PERIODIC
--   EncounterEvent 460 SpellID 1217436  Severity 1  = SPELL_TOXIC_REGURGITATION_SELECTOR
--   EncounterEvent 461 SpellID 1217232  Severity 2  = SPELL_DEVOUR            (deliberately not scheduled)
--
-- Timing provenance, all from boss_azhiccar.cpp and nowhere else:
--
--   JustEngagedWith: ScheduleEvent(EVENT_INVADING_SHRIEK, 5200ms)
--                    ScheduleEvent(EVENT_TOXIC_REGURGITATION, 15400ms)
--   EVENT_INVADING_SHRIEK:     count starts at 1, ++ then Repeat(37200ms) when even else Repeat(48500ms)
--                              -> casts at 5200, 42400, 90900, 128100, 176600, ...
--   EVENT_TOXIC_REGURGITATION: count starts at 1, ++ then Repeat(18200ms) when even else Repeat(67500ms)
--                              -> casts at 15400, 33600, 101100, 119300, 186800, ...
--
-- Both abilities alternate between two intervals, which a single repeating row cannot express - but two
-- rows can, because the alternation is periodic: 37200+48500 = 85700 and 18200+67500 = 85700. Each ability
-- is therefore two rows offset by its first interval, both repeating every 85700ms, which reproduces the
-- script's cast times exactly and indefinitely rather than approximating them.
--
-- SPELL_DEVOUR (EncounterEvent 461) is NOT in this table on purpose. The script fires it from
-- EVENT_CHECK_ENERGY when the boss reaches 100 energy, on heroic and above only - it has no fixed offset
-- from the pull, so any FirstCastMs for it would be invented. It belongs on the timeline the moment
-- something in the script announces it; until then, sending a countdown for it would be a fabrication.

DELETE FROM `instance_encounter_timeline` WHERE `DungeonEncounterID` = 3107;
INSERT INTO `instance_encounter_timeline`
  (`DungeonEncounterID`,`DifficultyID`,`EncounterEventID`,`SpellID`,`FirstCastMs`,`RepeatCastMs`,`MaxQueueDurationMs`,`Severity`,`BroadcastTextID`,`IconFileID`,`Flags`,`Comment`) VALUES
(3107, 0,   2, 1217327,  5200, 85700, 5000, 1, 0, 0, 0, 'Invading Shriek 1st/3rd/5th - JustEngagedWith 5200ms, cycle 37200+48500'),
(3107, 0,   2, 1217327, 42400, 85700, 5000, 1, 0, 0, 0, 'Invading Shriek 2nd/4th - 5200+37200, cycle 37200+48500'),
(3107, 0, 460, 1217436, 15400, 85700, 5000, 1, 0, 0, 0, 'Toxic Regurgitation 1st/3rd/5th - JustEngagedWith 15400ms, cycle 18200+67500'),
(3107, 0, 460, 1217436, 33600, 85700, 5000, 1, 0, 0, 0, 'Toxic Regurgitation 2nd/4th - 15400+18200, cycle 18200+67500');
