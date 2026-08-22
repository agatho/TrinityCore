-- ============================================================================
-- Arathi Catch-Up / RPE -- Go'shek Farm reinforcement  (2026-08-22)
-- ============================================================================
-- REASSESSMENT of the Alliance capture (tester was correct that it holds farm data). The Alliance farm
-- mobs are NOT in the objupdate spawn file (a farmstead create-block decode gap) -- they are in the
-- DESPAWN events, because the Alliance player KILLED them during the farm quests (90885/86/87, tick
-- window ~1.90M-2.30M). Distinct-GUID kills in that window:
--   Kobold Pillager 244676 -> 15 kills across 10 spread spawn-ticks (RESPAWNING pool, ~26s cadence),
--     initial simultaneous wave ~3  -- but the DB spawns only ONE.
--   Kobold Firetender 244677 -> 8 kills (pool ~6; DB 6 = OK)
--   Ogre Destroyer   244674 -> 4 kills (pool ~6; DB 6 = OK)
--   Runk 244675 -> 1 (boss)
-- So the farm is a RESPAWNING defence, and it reads as "too few" for two capture-provable reasons:
--   (1) the Kobold Pillager standing pool is under-spawned (1 vs ~3), and
--   (2) the respawn timer (60s) is slower than the ~26s the Alliance kills show, so the farm empties
--       out after a clear instead of feeding fresh attackers.
-- Fix both, capture-grounded. Counts stay within what the capture supports (no invented mob types).

-- (1) Bring the Kobold Pillager standing pool to 3 (add 2), interpolated into the existing farm cluster
--     (z=14 farm ground). Phase 4 + light wander (matching the other farmstead raiders) + 30s respawn.
DELETE FROM `creature` WHERE `guid` IN (8002000,8002001);
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`MovementType`,`wander_distance`,`VerifiedBuild`) VALUES
 (8002000, 244676, 2927, 4, -1548.0, -2990.0, 14.0, 1.20, 30, 1, 4, 69404),
 (8002001, 244676, 2927, 4, -1520.0, -2960.0, 14.2, 4.50, 30, 1, 4, 69404);

-- (2) Respawn cadence: the Alliance kills respawned ~26s apart. Bring the farmstead raiders from 60s to
--     30s so the defence stays populated while the player fights through it (Runk 244675 the miniboss
--     keeps his 300s -- a boss, not a wave mob).
UPDATE `creature` SET `spawntimesecs`=30 WHERE `map`=2927 AND `PhaseId`=4 AND `id` IN (244674,244676,244677);
