--
-- Quel'Thalas (Midnight, Season 1) zone events - ZoneEventMgr schema + seed.
--
-- SHIPS ON feature/quelthalas-zone-events. DO NOT apply to the shared integration
-- realm at M:\IntegratedServer. This is the SQL listed in the blueprint
-- (C:\dumps\QUELTHALAS_EVENTS_BLUEPRINT.md); it is applied only on a disposable
-- test DB once per-event content is unblocked.
--
-- Worldstate ids below are CONFIRMED client-recognised worldstates (referenced by
-- WorldStateExpression blobs in build 12.0.7.68887 and observed on the wire in the
-- 68974 tester sniffs):
--   29616 = Stormarion assault fill-meter (+500 steps, cap 1,000,000, resets to 0)
--   22984 = next-event unix timestamp (rotation clock, 360s cadence observed)
--    5207 = 3-phase rotation state A     5208 = 3-phase rotation state B
--   28763 = 1 Hz active-event countdown (zone 15968)
-- Zones: 15968 Eversong Woods (Eastern Kingdoms, map 0), 15969 Silvermoon City.
-- Event maps: 0 (Eastern Kingdoms), 2694 (Har'alnor), 2771 (Stormarion Keep).

DROP TABLE IF EXISTS `zone_event_template`;
CREATE TABLE `zone_event_template` (
  `Id`                    INT UNSIGNED  NOT NULL,
  `Type`                  TINYINT UNSIGNED NOT NULL COMMENT '0 Abundance, 1 Saltheril, 2 Stormarion, 3 Haranir',
  `ZoneId`                INT UNSIGNED  NOT NULL DEFAULT 0,
  `MapId`                 INT UNSIGNED  NOT NULL DEFAULT 0,
  `StateWorldStateId`     INT           NOT NULL DEFAULT 0 COMMENT 'rotation/assault-meter WS (5207/5208/29616)',
  `TimerWorldStateId`     INT           NOT NULL DEFAULT 0 COMMENT 'next-event unix stamp WS (22984)',
  `CountdownWorldStateId` INT           NOT NULL DEFAULT 0 COMMENT '1 Hz countdown WS (28763)',
  `PeriodSeconds`         INT UNSIGNED  NOT NULL DEFAULT 0 COMMENT '28800 8h / 604800 weekly / 1800 30m',
  `DurationSeconds`       INT UNSIGNED  NOT NULL DEFAULT 0,
  `MeterCap`              INT           NOT NULL DEFAULT 0 COMMENT 'assault fill cap (1000000 Stormarion)',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed rows. Cadence values follow the retail design (8h / weekly / 30m); the
-- worldstate ids are the confirmed captures. Per-event spawn/reward content is
-- CAPTURE-BLOCKED and NOT seeded here (see blueprint tester-ask list).
INSERT INTO `zone_event_template`
  (`Id`,`Type`,`ZoneId`,`MapId`,`StateWorldStateId`,`TimerWorldStateId`,`CountdownWorldStateId`,`PeriodSeconds`,`DurationSeconds`,`MeterCap`) VALUES
  (1, 2, 15968, 2771, 29616, 22984, 28763,  1800,  1800, 1000000), -- Stormarion Assault (Scenario 3021)
  (2, 0, 15968,    0,  5207, 22984,     0, 28800, 28800,       0), -- Abundance caves (8h rotation, DB2 AreaPOIs 8525-8528)
  (3, 1, 15968,    0,  5208, 22984,     0, 604800, 86400,      0), -- Saltheril's Soiree (weekly, quest 89289)
  (4, 3, 15968, 2694,     0, 22984,     0, 604800, 604800,     0); -- Legends of the Haranir (weekly, quest 93932; scenario CAPTURE-BLOCKED)

-- ---------------------------------------------------------------------------
-- Phase 1 (Stormarion Assault) content wiring.
-- ---------------------------------------------------------------------------

--
-- 1) Scenario handoff. ScenarioMgr attaches Scenario 3021 "Stormarion Assault"
--    when the Stormarion Keep instance map (2771) spins up, via the stock
--    `scenarios` (mapid,difficulty)->scenario table. This IS the existing
--    handoff path; ZoneEventMgr owns only the open-world rotation/timer.
--    difficulty 1 = DIFFICULTY_NORMAL. CAPTURE-BLOCKED: confirm the exact
--    difficulty map 2771 is entered at (default NORMAL used here).
--
DELETE FROM `scenarios` WHERE `map` = 2771 AND `difficulty` = 1;
INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES
  (2771, 1, 3021, 3021); -- Stormarion Assault

--
-- 2) Scenario wave -> assault-meter wiring. Each of Scenario 3021's three waves
--    has a DB2-confirmed CriteriaTree + RewardQuest (build 68887). When a wave's
--    CriteriaTree completes, ZoneEventMgr advances the assault meter (WS 29616)
--    by MeterStep (+500). The reward quest is granted by the stock scenario
--    system (Scenario::CompleteStep); MeterStep is the zone-meter contribution.
--
DROP TABLE IF EXISTS `zone_event_scenario_step`;
CREATE TABLE `zone_event_scenario_step` (
  `Id`             INT UNSIGNED  NOT NULL,
  `EventId`        INT UNSIGNED  NOT NULL COMMENT 'FK zone_event_template.Id',
  `ScenarioId`     INT UNSIGNED  NOT NULL DEFAULT 0 COMMENT '3021 Stormarion Assault',
  `WaveIndex`      TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '1..3',
  `CriteriaTreeId` INT UNSIGNED  NOT NULL DEFAULT 0 COMMENT 'wave CriteriaTree (DB2 68887)',
  `RewardQuestId`  INT UNSIGNED  NOT NULL DEFAULT 0 COMMENT 'granted by scenario system',
  `MeterStep`      INT           NOT NULL DEFAULT 500 COMMENT 'WS 29616 contribution',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `zone_event_scenario_step`
  (`Id`,`EventId`,`ScenarioId`,`WaveIndex`,`CriteriaTreeId`,`RewardQuestId`,`MeterStep`) VALUES
  (1, 1, 3021, 1, 210107, 91464, 500), -- Wave 1 "Defend the Singularity Anchor"
  (2, 1, 3021, 2, 211420, 91465, 500), -- Wave 2
  (3, 1, 3021, 3, 211423, 90943, 500); -- Wave 3 "final push by Domanaar forces"

--
-- 3) Spawn mechanism. ZoneEventMgr summons these on event start and despawns
--    them on end. Ships with NO DATA ROWS: the spawn COORDINATES for the event
--    actor 256697 (casts spell 1253107) and the destructibles Conjured Defense
--    241418 / Ward Fragment 241419 are CAPTURE-BLOCKED (no AreaTrigger/SceneObject
--    or spawn-point capture in hand). The mechanism is a safe no-op with 0 rows;
--    add rows once positions are captured. Kind: 0 = creature, 1 = gameobject.
--
DROP TABLE IF EXISTS `zone_event_spawn`;
CREATE TABLE `zone_event_spawn` (
  `Id`          INT UNSIGNED  NOT NULL,
  `EventId`     INT UNSIGNED  NOT NULL COMMENT 'FK zone_event_template.Id',
  `Kind`        TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 creature, 1 gameobject',
  `Entry`       INT UNSIGNED  NOT NULL DEFAULT 0 COMMENT 'creature/gameobject template entry',
  `MapId`       INT UNSIGNED  NOT NULL DEFAULT 0 COMMENT '0 = use zone_event_template.MapId',
  `PosX`        FLOAT         NOT NULL DEFAULT 0,
  `PosY`        FLOAT         NOT NULL DEFAULT 0,
  `PosZ`        FLOAT         NOT NULL DEFAULT 0,
  `Orientation` FLOAT         NOT NULL DEFAULT 0,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- CAPTURE-BLOCKED rows (positions unknown) -- listed for reference, NOT inserted:
--   (1, 1, 0, 256697, 2771, ?, ?, ?, ?), -- event actor (casts spell 1253107)
--   (2, 1, 0, 241418, 2771, ?, ?, ?, ?), -- Conjured Defense (absorb-shield)
--   (3, 1, 0, 241419, 2771, ?, ?, ?, ?)  -- Ward Fragment (absorb-shield)

-- NOTE: the client only reacts to these worldstates when a `world_state` row for
-- each id lists the zone AreaIDs (15968/15969) so WorldStateMgr scopes the
-- broadcast. Those rows are stock Blizzard worldstates; if absent on the test DB,
-- add them alongside this file. Deferred until per-event content is unblocked.

-- ---------------------------------------------------------------------------
-- Phase 3 (Saltheril's Soiree) content wiring.
-- ---------------------------------------------------------------------------
--
-- The Saltheril seed row already ships above:
--   (3, 1, 15968, 0, 5208, 22984, 0, 604800, 86400, 0)
--   Type 1 (Saltheril), weekly cadence (PeriodSeconds 604800 == one week), so
--   ZoneEventMgr snaps its activation to the server's weekly quest reset boundary
--   (World::GetNextWeeklyQuestsResetTime -- same source the weekly-reset framework
--   uses). StateWorldStateId 5208 is the zone rotation state broadcast on activate;
--   TimerWorldStateId 22984 carries the next-window unix stamp. No meter, no countdown.
--   DurationSeconds 86400 (1 day active window) is the skeleton assumption -- the
--   exact retail window length is CAPTURE-BLOCKED (not derivable from the data on
--   hand); adjust once captured.
--
-- The quest side is ALREADY LIVE and authored in content/midnight-s1 (applied to the
-- live world DB): quest 89289 "Favor of the Court", giver/ender creature 240832,
-- objective ObjectID 241313, QuestLine 5841 "Saltheril's Haven". NO quest authoring
-- here -- the event only advertises the already-completable quest on its weekly window.
--
-- AreaPOI 8600 "Saltheril's Soiree" -- map-marker seed (LISTED, NOT APPLIED).
--   Confirmed DB2 values (build 12.0.7.68887, live-realm verified):
--     PoiId               8600
--     Position            (7212.02, -3886.55, 69.62)
--     AreaID              15968  (Eversong Woods; sub-area 15997 "Saltheril's Haven")
--     PlayerConditionID   152346 (gates POI visibility; EXACT gated value CAPTURE-BLOCKED)
--     PoiData             89289  (keyed to the weekly quest)
--     QuestLineID         5841   ("Saltheril's Haven")
--     UiWidgetSetID       1941
--     UiTextureKitID      2710
--   AreaPOI is a client DB2 record; stock TrinityCore has no server-side area_poi
--   table, and this branch (off baseline 560165c0a6) ships no AreaPoiMgr / area_poi
--   loader -- that arrives with feature/world-quests (see blueprint merge plan). So
--   this seed is LISTED for reference only; it is NOT inserted (no reader exists on
--   this branch, and inventing a loader is out of scope). When AreaPoiMgr merges,
--   materialise it there. The server-side "soiree live" signal ZoneEventMgr already
--   emits is StateWorldStateId (WS 5208) on activate; binding AreaPOI 8600's
--   PlayerConditionID 152346 to that worldstate value is CAPTURE-BLOCKED (decode 152346).
--
--   Reference row (DO NOT APPLY on this branch -- no area_poi_template reader here):
--     -- INSERT INTO `area_poi_template`
--     --   (`ID`,`PosX`,`PosY`,`PosZ`,`AreaID`,`PlayerConditionID`,`PoiData`,`QuestLineID`,`UiWidgetSetID`,`UiTextureKitID`) VALUES
--     --   (8600, 7212.02, -3886.55, 69.62, 15968, 152346, 89289, 5841, 1941, 2710);
--
-- CAPTURE-BLOCKED / deferred for the soiree (documented, NOT invented):
--   * Decode PlayerConditionID 152346 -> the exact worldstate value AreaPOI 8600 gates on
--     (so the marker follows the server rotation and hides on window close).
--   * The soiree's INTERNAL activities beyond quest 89289 (host gossip chain
--     243357/243553/241450/241452/243500/243352/243349/243527, party mini-games) and
--     any reward beyond the quest / the "Fortify the Runestones" warband unlock.
--   * The exact weekly active-window length (DurationSeconds; 86400 is a placeholder).
