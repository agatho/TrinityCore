-- ===========================================================================
-- World-DB fixes: Twin Peaks flag spawns + Silvershard Mines template config
-- Target DB: world (this server: wc_world)
-- Date:      2026-06-22
-- ===========================================================================
--
-- TWIN PEAKS (BML 108 -> map 726): same gap as EotS/WSG. The core TP script
-- expects the two CTF flags to be table-spawned (entries 227740 Horde / 227741
-- Alliance — TP reuses the WSG flag templates, both already type 36 NEW_FLAG and
-- proven working in WSG). On this server map 726 had only a Speed Buff GO — both
-- flags were missing, so the flag could never be picked up. Spawn them at the TP
-- base flag positions (from TwinPeaksScript / V1 data).
DELETE FROM `gameobject` WHERE `id` IN (227740,227741) AND `map` = 726;
INSERT INTO `gameobject`
  (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,
   `position_x`,`position_y`,`position_z`,`orientation`,
   `rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`)
VALUES
  (11000006, 227740, 726, 5031, 5775, '0', 0, 0, 0, 1578.339, 344.063,  2.419, 0, 0,0,0,1, 7200, 255, 1),
  (11000007, 227741, 726, 5031, 5775, '0', 0, 0, 0, 2118.210, 191.621, 44.052, 0, 0,0,0,1, 7200, 255, 1);

-- SILVERSHARD MINES (BML 708 -> map 727): template config was zeroed (0/0/0/0),
-- so the autonomous seed computed max_total_bots=0 and never filled it (same
-- issue the modern WSG template 1014 had). SM's scoring objective is the mine
-- CARTS, which are creatures (not GOs), so the data layer is otherwise present.
-- Restore a normal 10v10 sizing so it can be seeded + filled.
UPDATE `battleground_template`
   SET `MinPlayersPerTeam` = 2,
       `MaxPlayersPerTeam` = 10,
       `MinLvl`            = 10,
       `MaxLvl`            = 120
 WHERE `ID` = 708;
