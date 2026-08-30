-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- DH intro — Mardum graveyard + Area Spirit Healer (Horde + Alliance)
-- Evidence (retail sniff dhi-gy1, Horde DH map 1481):
--   SMSG_DEATH_RELEASE_LOC Map 1481 @ (1180.8108, 3303.8604, 74.4226) = world_safe_locs 5082
--   SMSG_MOVE_TELEPORT same coords after CMSG_REPOP_REQUEST
--   Area Spirit Healer entry 65183 @ (1180.7778, 3309.2378, 75.193016) o=4.76015
--   CMSG_AREA_SPIRIT_HEALER_QUERY/QUEUE + aura 2584 → auto-rez (~10–18s)
-- Root cause: 0 graveyard_zone rows for GhostZone 7705 → GetClosestGraveyard fallback (Barrens)
-- Alliance: same map/zone; graveyard_zone has no Faction column and no CONDITION_TEAM → both teams

SET @CGUID := 11800191;

-- Link named Mardum GYs (not corpse-catcher locs — those lack spirit healers)
DELETE FROM `graveyard_zone` WHERE `GhostZone`=7705 AND `ID` IN (5082,5083,5119,5140,5188,5284);
INSERT INTO `graveyard_zone` (`ID`, `GhostZone`, `Comment`) VALUES
(5082, 7705, 'DH-Mardum - (01) Start'),
(5083, 7705, 'DH-Mardum - (03) Seat of Command'),
(5119, 7705, 'DH-Mardum - (04) Illidari Foothold'),
(5140, 7705, 'DH-Mardum - (05) Volcano'),
(5188, 7705, 'DH-Mardum - (06) The Fel Hammer'),
(5284, 7705, 'DH-Mardum - (02) Molten Shore');

-- Area Spirit Healer 65183 (npcflag UNIT_NPC_FLAG_AREA_SPIRIT_HEALER) at each named GY
-- Start GY position exact from sniff; others offset ~+5.4 Y from world_safe_locs (same pattern as 5082)
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+5;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(@CGUID+0, 65183, 1481, 7705, 7705, '0', 0, 0, 0, 1180.7778, 3309.2378, 75.193016, 4.760153, 120, 0, 0, 100, 0, '', NULL, 68887), -- Start (sniff)
(@CGUID+1, 65183, 1481, 7705, 7705, '0', 0, 0, 0,  848.3980, 2400.0800, -52.028900, 4.760153, 120, 0, 0, 100, 0, '', NULL, 68887), -- Molten Shore
(@CGUID+2, 65183, 1481, 7705, 7705, '0', 0, 0, 0, 1062.6200, 2587.9400, -37.329500, 4.760153, 120, 0, 0, 100, 0, '', NULL, 68887), -- Seat of Command
(@CGUID+3, 65183, 1481, 7705, 7705, '0', 0, 0, 0, 1414.3200, 1779.7900,  56.412800, 4.760153, 120, 0, 0, 100, 0, '', NULL, 68887), -- Illidari Foothold
(@CGUID+4, 65183, 1481, 7705, 7705, '0', 0, 0, 0, 1807.1400, 1338.5200,  97.748100, 4.760153, 120, 0, 0, 100, 0, '', NULL, 68887), -- Volcano
(@CGUID+5, 65183, 1481, 7705, 7705, '0', 0, 0, 0, 1644.0200, 1419.4400, 243.324000, 4.760153, 120, 0, 0, 100, 0, '', NULL, 68887); -- Fel Hammer
