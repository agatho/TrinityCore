-- ===========================================================================
-- World-DB restore: Eye of the Storm central Netherstorm Flag (map 566)
-- Target DB: world (this server: wc_world)
-- Date:      2026-06-22
-- ===========================================================================
--
-- ROOT CAUSE (EotS central flag never picked up / capturable):
--   The core EotS script (battleground_eye_of_the_storm.cpp) expects a GameObject
--   with entry 208977 ("Netherstorm Flag") to be table-spawned on map 566 — it
--   captures it reactively in OnGameObjectCreate (_flagGUID) exactly like WSG
--   captures its flags 227740/227741. On this server that GO was:
--     1. MISSING — no `gameobject` spawn row for 208977 on map 566 (only the 4
--        CONTROL_ZONE towers 184080-184083 + doors exist), and
--     2. MIS-TYPED — `gameobject_template`.type = 24 (FLAGSTAND) whereas the core
--        flag state-machine (GameObject::GetFlagState / GetFlagCarrierGUID) only
--        functions for NEW_FLAG (36); type 24 returns FlagState(0)/Empty, so even
--        a human could not carry it.
--   Net effect: bots (and players) reach mid but there is no functioning flag GO
--   to lift; EotS scored on tower control only. Fix = make 208977 a proper
--   NEW_FLAG(36) mirroring the known-good WSG flag template, and spawn it at the
--   arena centre. (BattlegroundScriptMgr aliases / EotS advice already advertise
--   the flag types, so bots pick it up immediately once it exists.)

-- 1. Template: FLAGSTAND(24) -> NEW_FLAG(36), mirror WSG's working newflag Data
--    (Data1 = pickupSpell, kept at this entry's existing 100196; Data0 = no lock;
--     Data3 requireLOS=1; Data5 GiganticAOI=1; Data7/8 Expire/Respawn=10000).
UPDATE `gameobject_template`
   SET `type` = 36,
       `Data0` = 0,        -- open (Lock): none  (was 1479 = a lock that blocked use)
       `Data1` = 34976,    -- pickupSpell = BG_EY_NETHERSTORM_FLAG_SPELL (carry aura
                           -- the core casts on Use and removes on capture; 100196
                           -- was wrong — flag was Used but never went Taken)
       `Data2` = 0,        -- openTextID
       `Data3` = 1,        -- requireLOS
       `Data4` = 0,        -- conditionID1
       `Data5` = 1,        -- GiganticAOI
       `Data6` = 0,        -- InfiniteAOI
       `Data7` = 10000,    -- ExpireDuration (dropped-flag return)
       `Data8` = 10000     -- RespawnTime (after capture)
 WHERE `entry` = 208977;

-- 2. Spawn the flag at the EotS arena centre on map 566 (ground z~1159.3 there).
DELETE FROM `gameobject` WHERE `id` = 208977 AND `map` = 566;
INSERT INTO `gameobject`
  (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,
   `position_x`,`position_y`,`position_z`,`orientation`,
   `rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`)
VALUES
  (11000005, 208977, 566, 3820, 3870, '0', 0, 0, 0,
   2174.444, 1569.42, 1159.71, 0, 0, 0, 0, 1, 7200, 255, 1);
