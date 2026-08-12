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

-- NOTE: the client only reacts to these worldstates when a `world_state` row for
-- each id lists the zone AreaIDs (15968/15969) so WorldStateMgr scopes the
-- broadcast. Those rows are stock Blizzard worldstates; if absent on the test DB,
-- add them alongside this file. Deferred until per-event content is unblocked.
