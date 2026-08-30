-- ============================================================================
-- Arathi Catch-Up / RPE -- Hammerfall reconciliation  (2026-08-22)
-- ============================================================================
-- Reconciles the per-phase re-derivation (agent a321c8...) against the live realm. The agent's core
-- premise -- "slot-11 phases don't render, move 214 rows to terrain phases" -- was REJECTED: every RPE
-- phase (3/4/8/28/37) has a `phase_area` row, so in TrinityCore (which has no client 'slot' concept,
-- it just accumulates phase ids) they DO render when the player is in that area with the phase's
-- condition satisfied -- exactly as the tester saw the phase-4 farm mobs. Collapsing them to terrain
-- phases would have destroyed the captured per-quest-step phasing. Instead this file takes only the
-- agent's genuinely-independent, capture-grounded findings, plus the one real condition bug it exposed.
--
-- Tester status: Hammerfall + farm have been walked; Stromgarde/finale not yet. The phase-28 fix and
-- the 1965 finale rework below are therefore correct-but-unverified in-game; the gnolls/movers/finale-
-- declutter are in the tested Hammerfall area.
-- Idempotent.
-- ============================================================================

-- (1) PHASE 28 orphaned condition. Phase 28 (94 Stromgarde siege creatures, area 16453) had
--     SourceEntry=0 -> the condition is rejected at load ("Area 0 does not have phase 28") and the phase
--     applies UNCONDITIONALLY whenever the player is in area 16453 -> the siege crowd shows in every
--     quest state. Its terrain pair 1610 is gated (90888 rewarded AND NOT 90896); give 28 the same
--     anchor so the two gate together. (Same orphan-repair already done for 4/1961/1959/1610/3.)
UPDATE `conditions` SET `SourceEntry`=16453 WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup`=28 AND `SourceEntry`=0;

-- (2) MISSING back-Hammerfall gnolls. The camp was under-spawned (Gnoll Ripper 244671: 17 distinct
--     GUIDs observed vs 3 rows; Bowblaster/Hyena/Bruiser similar) AND was stranded on non-rendering
--     phase 37 (already moved 37->1961 earlier). Add 16 spawns from the capture's create-block decode,
--     on the proven-rendering arrival phase 1961, deduped >=3yd from existing rows.
DELETE FROM `creature` WHERE `guid` BETWEEN 8001300 AND 8001315;
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001300, 244669, 2927, 1961, -921.7726, -3490.4480, 70.6689, 1.6754, 300, 69382),
 (8001301, 244669, 2927, 1961, -892.3472, -3507.6946, 71.0410, 0.4970, 300, 69382),
 (8001302, 244670, 2927, 1961, -938.4080, -3538.3767, 70.7458, 3.8582, 300, 69382),
 (8001303, 244670, 2927, 1961, -920.2465, -3489.1790, 70.6622, 2.1922, 300, 69382),
 (8001304, 244670, 2927, 1961, -890.7969, -3545.0903, 80.3137, 6.0488, 300, 69382),
 (8001305, 244670, 2927, 1961, -892.5191, -3504.5034, 71.0979, 0.8016, 300, 69382),
 (8001306, 244671, 2927, 1961, -1013.1059, -3488.3665, 62.3278, 2.9777, 300, 69382),  -- MOVER rep (MovementType=2, path 9200001)
 (8001307, 244671, 2927, 1961, -954.2639, -3526.8108, 70.6541, 1.2117, 300, 69382),
 (8001308, 244671, 2927, 1961, -950.3420, -3488.9253, 54.4752, 4.8530, 300, 69382),
 (8001309, 244671, 2927, 1961, -940.8958, -3543.8838, 70.7726, 2.9907, 300, 69382),
 (8001310, 244671, 2927, 1961, -952.3941, -3477.4602, 54.4671, 3.1675, 300, 69382),
 (8001311, 244671, 2927, 1961, -911.8854, -3530.9827, 58.4712, 2.2223, 300, 69382),
 (8001312, 244671, 2927, 1961, -901.3524, -3547.9150, 58.4993, 5.5323, 300, 69382),
 (8001313, 244671, 2927, 1961, -955.9273, -3537.2729, 56.8461, 3.0340, 300, 69382),
 (8001314, 244671, 2927, 1961, -953.5347, -3541.2500, 56.8696, 1.0550, 300, 69382),
 (8001315, 244672, 2927, 1961, -1003.9313, -3532.9255, 57.0132, 3.7653, 300, 69382);

-- (3) STATIONARY movers. Gnoll Ripper 244671 and Gnoll Bruiser 244672 carry real multi-point
--     SMSG_ON_MONSTER_MOVE splines at back-Hammerfall but sat MovementType=0 (idle) -> the tester saw
--     them frozen. Give each a waypoint patrol (this build's NEW waypoint system: creature.MovementType=2
--     + creature_addon.PathId -> waypoint_path/_node; the legacy waypoint_data table does not exist here).
UPDATE `creature` SET `MovementType`=2 WHERE `guid` IN (8001306,8000024);
INSERT INTO `creature_addon` (`guid`,`PathId`) VALUES (8001306, 9200001), (8000024, 9200002)
  ON DUPLICATE KEY UPDATE `PathId`=VALUES(`PathId`);
DELETE FROM `waypoint_path_node` WHERE `PathId` IN (9200001,9200002);
DELETE FROM `waypoint_path` WHERE `PathId` IN (9200001,9200002);
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Velocity`,`Comment`) VALUES
 (9200001, 0, 0, 0, 'Gnoll Ripper 244671 - back-Hammerfall patrol loop'),
 (9200002, 0, 0, 0, 'Gnoll Bruiser 244672 - back-Hammerfall patrol line');
INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`) VALUES
 (9200001, 1, -1013.1059, -3488.3665, 62.3278),
 (9200001, 2, -1012.5374, -3485.6146, 62.5873),
 (9200001, 3, -1018.9836, -3498.3473, 62.3019),
 (9200001, 4, -1022.3550, -3499.8542, 62.3122),
 (9200001, 5, -1021.3029, -3496.4480, 62.5685),
 (9200001, 6, -1023.3333, -3495.9028, 62.3255),
 (9200001, 7, -1013.3021, -3494.2361, 62.3632),
 (9200001, 8, -1020.2336, -3499.0973, 62.5519),
 (9200001, 9, -1023.3029, -3495.9480, 62.3185),
 (9200001, 10, -1028.2118, -3496.3959, 62.5652),
 (9200001, 11, -1020.2726, -3484.4932, 62.3114),
 (9200001, 12, -1013.3021, -3494.2361, 62.3632),
 (9200002, 1, -895.5868, -3517.2900, 70.4694),
 (9200002, 2, -889.3510, -3517.8618, 70.9922),
 (9200002, 3, -883.6010, -3518.1118, 71.2422),
 (9200002, 4, -880.6152, -3518.4336, 71.5151),
 (9200002, 5, -875.3415, -3518.6803, 72.2319),
 (9200002, 6, -870.5915, -3518.6803, 72.7319),
 (9200002, 7, -875.0772, -3518.3585, 72.2090),
 (9200002, 8, -878.0772, -3518.1085, 71.7090),
 (9200002, 9, -882.0772, -3518.1085, 71.4590),
 (9200002, 10, -889.5772, -3517.6085, 71.2090),
 (9200002, 11, -896.9392, -3516.9324, 70.4652),
 (9200002, 12, -895.5868, -3517.2900, 70.4694);

-- (4) FINALE town crowd stranded on the ARRIVAL phase. 13 restored-town NPCs (Innkeeper Adegwa,
--     vendors Keena/Tharlidun/Uttnar, named Drum Fel/Gor'mul/etc.) + the finale hub Thrall 244715 sit
--     in the back-Hammerfall town (x -880..-1016, east of the arrival pad at x~-1088) but on phase 1961,
--     so they show during the initial under-attack arrival ("too many Horde chars", "the final-quest
--     Thrall is visible"). Move them to the finale phase 1965. 1965 was empty and mis-attached to the
--     farm area 16456 with no gate; re-point it to the Hammerfall area 16466 and gate it on the climax
--     quest 90896 being rewarded, so the restored town appears only after the climax.
--     (90896 = climax quest, per phase 3's QUESTTAKEN(90896) gate. Inferred finale gate -- confirm at
--     finale testing; flip to 1959 only if the realm's finale backdrop proves to be the farm phase.)
UPDATE `phase_area` SET `AreaId`=16466 WHERE `PhaseId`=1965 AND `AreaId`=16456;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup`=1965;
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`Comment`) VALUES
 (26, 1965, 16466, 0, 0, 8, 0, 90896, 0, 0, 0, 'Catch-Up RPE -- PhaseId 1965 (Hammerfall FINALE / restored town) requires climax quest 90896 rewarded');
UPDATE `creature` SET `PhaseId`=1965
 WHERE `map`=2927 AND `PhaseId`=1961 AND `position_y` BETWEEN -3600 AND -3450
   AND `id` IN (244715,230248,232019,232022,232023,232027,232030,232031,232033,232035,232037,232038,246612,246613);
