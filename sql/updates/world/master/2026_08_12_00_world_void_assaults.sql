-- ============================================================================
-- Void Assaults (Midnight 12.0.5) + Void Assault "Escalations" (12.0.7)
-- open-world invasion framework -- world DB schema.
--
-- SHIPS ON feature/void-assaults. **NOT APPLIED** to the shared realm
-- (M:\IntegratedServer / integ_*). VoidAssaultMgr::LoadFromDB() tolerates the
-- absent table (idle no-op), so a realm without this SQL is unaffected.
--
-- Evidence tags: [DB2] = wago.tools 12.0.7.68887, [SNIFF] = tester capture,
-- [RESEARCH] = web design guide (intent only). Seed rows are placeholders that
-- drive ONLY the worldstate/rotation spine; no spawns or rewards ship (those are
-- CAPTURE-BLOCKED). See C:\dumps\VOID_ASSAULTS_BLUEPRINT.md.
-- ============================================================================

DROP TABLE IF EXISTS `void_assault_template`;
CREATE TABLE `void_assault_template` (
  `Id`                    INT UNSIGNED NOT NULL,
  `Type`                  TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 VoidStrike, 1 VoidIncursion, 2 VoidEscalation',
  `ZoneId`                INT UNSIGNED NOT NULL DEFAULT 0,
  `MapId`                 INT UNSIGNED NOT NULL DEFAULT 0,
  `StateWorldStateId`     INT NOT NULL DEFAULT 0 COMMENT 'assault meter 29616 or rotation 5207/5208 [SNIFF]',
  `TimerWorldStateId`     INT NOT NULL DEFAULT 0 COMMENT 'next-event unix clock 22984 [SNIFF]',
  `CountdownWorldStateId` INT NOT NULL DEFAULT 0 COMMENT '1 Hz countdown 28763 [SNIFF]',
  `PeriodSeconds`         INT UNSIGNED NOT NULL DEFAULT 0,
  `DurationSeconds`       INT UNSIGNED NOT NULL DEFAULT 0,
  `MeterCap`              INT NOT NULL DEFAULT 0 COMMENT 'escalation fill cap 1000000 [SNIFF], 0 = none',
  `StrikesPerIncursion`   INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Void Strikes to trigger an Incursion, 0 = none',
  `PortalWorldMapA`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Naigtal map 3075 [DB2]',
  `PortalWorldMapB`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Val map 3047 [DB2]',
  `HeroicContentTuningId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Heroic World Tier scaling [DB2 ContentTuning -- CAPTURE-BLOCKED]',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Seed rows (placeholders -- drive ONLY the WS/rotation spine; realm-idle):
--   Id 1 = 12.0.5 Void Assault in Eversong Woods (area 15968), weekly swap with
--          Zul'Aman; Void Strikes feed meter 29616 (cap 1e6), Incursion at 20 strikes.
--   Id 2 = 12.0.7 Void "Escalation": portal-world window alternating Naigtal(3075)
--          / Val(3047), weekly; carries the world boss + (CAPTURE-BLOCKED) HWT.
INSERT INTO `void_assault_template`
  (`Id`,`Type`,`ZoneId`,`MapId`,`StateWorldStateId`,`TimerWorldStateId`,`CountdownWorldStateId`,`PeriodSeconds`,`DurationSeconds`,`MeterCap`,`StrikesPerIncursion`,`PortalWorldMapA`,`PortalWorldMapB`,`HeroicContentTuningId`) VALUES
  (1, 0, 15968, 0,    29616, 22984, 28763, 604800, 604800, 1000000, 20, 0,    0,    0),
  (2, 2, 16943, 3075, 5207,  22984, 28763, 604800, 604800, 0,        0, 3075, 3047, 0);

-- Spawn mechanism table. Ships EMPTY -- event-actor / world-boss / destructible
-- coordinates are CAPTURE-BLOCKED. VoidAssaultMgr iterates whatever rows exist
-- and is a safe no-op when empty.
DROP TABLE IF EXISTS `void_assault_spawn`;
CREATE TABLE `void_assault_spawn` (
  `AssaultId`   INT UNSIGNED NOT NULL,
  `Kind`        TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 creature, 1 gameobject',
  `Entry`       INT UNSIGNED NOT NULL DEFAULT 0,
  `MapId`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 = use template MapId',
  `PosX`        FLOAT NOT NULL DEFAULT 0,
  `PosY`        FLOAT NOT NULL DEFAULT 0,
  `PosZ`        FLOAT NOT NULL DEFAULT 0,
  `Orientation` FLOAT NOT NULL DEFAULT 0,
  KEY `AssaultId` (`AssaultId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- NO spawn rows shipped (CAPTURE-BLOCKED). Reference candidates for when
-- coordinates are captured (do NOT insert until verified in Creature.db2):
--   Naigtal world boss "Nexus-Captain Leth'ir" 260875 [RESEARCH]
--   Val world boss     "Imperator Pertinax"    261072 [RESEARCH]
