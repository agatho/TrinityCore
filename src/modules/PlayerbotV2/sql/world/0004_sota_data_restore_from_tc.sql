-- ===========================================================================
-- World-DB restore: Strand of the Ancients (map 607) — gameobjects + creatures
-- Source: tc_world (canonical TDB); Target: wc_world (custom, was EMPTY on 607)
-- Date:   2026-06-22
-- ===========================================================================
--
-- wc_world had ZERO gameobjects and ZERO creatures on map 607 — SoTA was 100%
-- absent (no gates, relics, demolishers, graveyards), so it could never be
-- played. tc_world has the full canonical set (244 GOs incl. 6 type-33 gates +
-- relic, 57 creatures incl. Battleground Demolisher 28781 x8 the siege vehicles,
-- Antipersonnel Cannons, Spirit Guides). All referenced templates already exist
-- in wc_world, so only the SPAWNS are copied. Guids are offset by +12,000,000 to
-- clear the wc/tc "different basis" guid collisions (wc max GO guid ~11,000,007,
-- creature ~11,000,156). Reversible: DELETE ... WHERE guid >= 12000000 AND map=607.

DELETE FROM `gameobject` WHERE `map` = 607 AND `guid` >= 12000000;
INSERT INTO `gameobject`
  (guid,id,map,zoneId,areaId,spawnDifficulties,phaseUseFlags,PhaseId,PhaseGroup,terrainSwapMap,
   position_x,position_y,position_z,orientation,rotation0,rotation1,rotation2,rotation3,
   spawntimesecs,animprogress,state,ScriptName,StringId,VerifiedBuild)
SELECT guid+12000000,id,map,zoneId,areaId,spawnDifficulties,phaseUseFlags,PhaseId,PhaseGroup,terrainSwapMap,
   position_x,position_y,position_z,orientation,rotation0,rotation1,rotation2,rotation3,
   spawntimesecs,animprogress,state,ScriptName,StringId,VerifiedBuild
FROM `tc_world`.`gameobject` WHERE `map` = 607;

DELETE FROM `creature` WHERE `map` = 607 AND `guid` >= 12000000;
INSERT INTO `creature`
  (guid,id,map,zoneId,areaId,spawnDifficulties,phaseUseFlags,PhaseId,PhaseGroup,terrainSwapMap,
   modelid,equipment_id,position_x,position_y,position_z,orientation,spawntimesecs,wander_distance,
   currentwaypoint,curHealthPct,MovementType,npcflag,unit_flags,unit_flags2,unit_flags3,
   ScriptName,StringId,VerifiedBuild)
SELECT guid+12000000,id,map,zoneId,areaId,spawnDifficulties,phaseUseFlags,PhaseId,PhaseGroup,terrainSwapMap,
   modelid,equipment_id,position_x,position_y,position_z,orientation,spawntimesecs,wander_distance,
   currentwaypoint,curHealthPct,MovementType,npcflag,unit_flags,unit_flags2,unit_flags3,
   ScriptName,StringId,VerifiedBuild
FROM `tc_world`.`creature` WHERE `map` = 607;
