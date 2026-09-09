--
-- Prey system + Voidforge (Midnight Season 1 solo hunts) — world schema.
-- SHIPPED ON BRANCH, NOT APPLIED to the shared realm. PreyMgr::LoadFromDB()
-- tolerates the table being absent (realm-safe no-op).
--
-- Difficulty: 0 = Normal, 1 = Hard, 2 = Nightmare  (enum PreyDifficulty)
-- All ids are DB2-anchored; content columns left 0/TODO where CAPTURE-BLOCKED
-- (ContentTuningId scaling and the Great Vault activity row are not yet identified
--  from a capture — see C:\dumps\PREY_VOIDFORGE_BLUEPRINT.md §6).
--

DROP TABLE IF EXISTS `prey_hunt_template`;
CREATE TABLE `prey_hunt_template` (
  `Id`              INT UNSIGNED NOT NULL,
  `ZoneId`          INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Midnight zone the hunt runs in',
  `Difficulty`      TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 Normal / 1 Hard / 2 Nightmare',
  `ContentTuningId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'CAPTURE-BLOCKED scaling id',
  `VaultActivityId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Great Vault activity row this hunt credits',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed rows are intentionally LEFT COMMENTED so the shared realm stays idle even if
-- this file is ever applied. Uncomment on a disposable test DB to exercise the spine.
-- INSERT INTO `prey_hunt_template` (`Id`,`ZoneId`,`Difficulty`,`ContentTuningId`,`VaultActivityId`) VALUES
--   (1, 0, 0, 0, 0),   -- Normal hunt  (drops Adventurer -> fills Veteran vault slot)
--   (2, 0, 1, 0, 0),   -- Hard hunt    (drops Veteran   -> fills Champion vault slot)
--   (3, 0, 2, 0, 0);   -- Nightmare    (drops Champion  -> fills Hero + Nebulous Voidcore 3418)
