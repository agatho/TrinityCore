-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D creature spawns (FIX ROUND 2)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED -- was wrongly 2796; client uiMapID 2451 is display-only,
--   left untouched in 34_quest_poi.sql).
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone_REMAP/zone_2927/ (TCHarvest re-mine,
--   VerifiedBuild=69382 per phase_shift.sql's capture-session header -- the sibling
--   creature_spawns_objupdate.zone_2927.sql / creature_spawns_movement.zone_2927.sql files
--   do not restate a build number of their own; 69382 is carried from the same session).
-- Sources used:
--   creature_spawns_objupdate.zone_2927.sql -- SMSG_UPDATE_OBJECT create-block decode,
--     EXACT x/y/z/o per spawned GUID instance (case-1(broad): >=2 distinct GUIDs observed
--     for that entry on this map, per sniff_spawn_confidence.txt -- read as real, separate,
--     likely-permanent spawn points, NOT decode noise).
--   creature_spawns_movement.zone_2927.sql -- stationary-creature movement-opcode position
--     confirmations; consulted but NOT used directly (every one of our 46 roster entries
--     that appears here also has an objupdate row, and position-priority rule (a) wins).
--   phase_shift.sql / sniff_phaseshift_confidence.txt -- REAL personal-phasing PhaseIds +
--     the phase->quest-step correlation graph (reproduced in the PhaseId map below).
--   The prior (WRONG) 20_creature_spawns.sql -- reused for its addon-approximate x/y and
--     its narrative clustering for the 26 entries that have NO remine coordinate.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent: DELETE by the reserved guid range below, then INSERT ... ON DUPLICATE KEY
--   UPDATE (belt-and-suspenders -- re-apply safe either way).
-- ============================================================================
--
-- ---- RESERVED GUID RANGE (unchanged from Fix Round 1; this file owns it) ----
-- Base 8000000, block 8000000-8000999 reserved. Actually used: 8000000-8000143, continuous,
-- 144 rows -- see per-section ranges below; the block has 856 guids of headroom left for a
-- later re-mine/expansion (guid layout was renumbered continuously from Fix Round 1's
-- gapped 8000000/8000100/8000200/8000300 sub-ranges since row counts changed substantially
-- once exact multi-instance spawns -- e.g. 244711's 36 rows -- were added).
--
-- ---- POSITION-PRIORITY RULE APPLIED (brief Requirement 1) ----
--   (a) EXACT remine position (creature_spawns_objupdate.zone_2927.sql), multiple distinct
--       GUIDs kept as multiple real spawns (collapsed only when two raw rows were within
--       ~1yd of each other -- Euclidean over x/y/z, see report for the dedup pass): 20 of
--       the 46 roster entries, 111 rows total.
--   (b) remine movement-file position: 0 of the 46 roster entries needed this tier (every
--       entry with a movement-file row also had an objupdate row, which wins per rule (a)).
--   (c) approximate: old addon x/y carried forward verbatim, map set to 2927, z/o remain
--       the old cluster-reference placeholders (Hammerfall 56.5 / Go'shek farm 42.2 /
--       Stromgarde+Boulderfist 80.0, o=0) -- TODO Phase K on every (c) row: 26 of the 46
--       roster entries, 33 rows total (includes Prized Pumpkin x4 offset-stack and Ogre
--       Basher's 2 siege-trash + 3 climax-wave rows, neither of which has a remine coord).
-- Total: 144 rows across 46 entries (46/46 roster entries present -- none dropped).
--
-- ---- THE REAL PhaseId MAP (Fix Round 2 -- replaces the FABRICATED 15901-15905 block) ----
-- Source: phase_shift.sql's phase->quest-step correlation graph (byte-proven off the wire,
-- VerifiedBuild=69382; see that file's own header for the full 18-PhaseId table -- only the
-- 7 ids actually attached to a creature spawn below are summarized here):
--   PhaseId 1961 (slot 0, terrain) -- quests=[90883]            -- Hammerfall/town BASE
--   PhaseId 37   (slot 11, per-quest) -- quests=[90883]          -- Hammerfall gnoll filler
--   PhaseId 1959 (slot 0, terrain) -- quests=[90885,86,87,88,93,95,96] -- farm-lead BASE
--   PhaseId 4    (slot 11, per-quest) -- quests=[90885,86,87]     -- farm trash + Runk
--   PhaseId 1610 (slot 0, terrain) -- quests=[90893,95]           -- Stromgarde-hub BASE
--   PhaseId 28   (slot 11, per-quest) -- quests=[90893,95]        -- siege trash + catapult
--   PhaseId 3    (slot 11+25, per-quest/"completion") -- quests=[90883,85,86,87,88,93,95,96]
--                (broadest phase on the wire; used here for the narrow climax/Ro'grok
--                cluster only -- see 21_phase_area.sql's banner for the full-window caveat)
-- 21_phase_area.sql additionally documents PhaseId 1965 and 8 (both real, both in the
-- graph, NEITHER attached to a creature spawn in this file -- see that file's banner).
--
-- ---- HONEST BOUNDARY on the per-spawn PhaseId assignment (brief Requirement 2) ----
-- phase_shift.sql's own header says it plainly: "the wire gives the player's phase SET
-- over time, not a per-spawn tag." Every PhaseId below is INFERRED by matching each
-- spawn's existing (Fix-Round-1) narrative cluster against the real quest-window each real
-- PhaseId was observed active during -- it is NOT a decoded per-creature phase tag (no such
-- tag exists on this wire capture). Within a cluster that has BOTH a terrain id and a
-- per-quest id sharing the same quest window (e.g. arrival's 1961+37, siege's 1610+28), the
-- split used here is: story leads/vendor/hub-town NPCs -> the terrain id (base, "always
-- visible" while the cluster is live); hostile filler/trash/quest-interact props -> the
-- per-quest id (gated, "appears for this quest step"). Farm's 1959-vs-1965 and
-- 4-vs-8 pairs are near-duplicate quest windows in the graph; 1959 and 4 were picked as the
-- single representative id for each half of that cluster (documented, not fabricated --
-- both 1965 and 8 are real ids from the same graph, just not the one chosen here).
-- ============================================================================

DELETE FROM `creature` WHERE `guid` BETWEEN 8000000 AND 8000999;

-- ============================================================================
-- PHASE 1961 -- Hammerfall/Refuge Pointe town + story leads (base/always-present during 90883 window) -- guid 8000000-8000012 (13 rows)
-- ============================================================================

-- entry 244643 Lady Jaina Proudmoore (Hammerfall, permanent story lead)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000000, 244643, 2927, 1961, -1084.2153, -3559.9722, 50.4453, 5.0222, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244642 Thrall (Hammerfall, permanent story lead; captured hostile -- combat-tutorial beat)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000001, 244642, 2927, 1961, -1086.4791, -3554.7744, 50.192, 0.0997, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245026 Win'sa (Hammerfall food vendor)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000002, 245026, 2927, 1961, -1089.5834, -3545.2188, 50.2155, 1.1165, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230248 Hammerfall Grunt
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000003, 230248, 2927, 1961, -3516.3001, -915.1, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232019 Mag'har Grunt
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000004, 232019, 2927, 1961, -982.3021, -3551.429, 57.1367, 2.0699, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232019 Mag'har Grunt
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000005, 232019, 2927, 1961, -1027.4844, -3560.1736, 56.6495, 3.6577, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232019 Mag'har Grunt
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000006, 232019, 2927, 1961, -903.8544, -3515.4417, 70.4608, 6.0788, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232022 Drum Fel
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000007, 232022, 2927, 1961, -3530.6001, -930.5, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232023 Gor'mul
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000008, 232023, 2927, 1961, -3484.4001, -958.9, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232028 Korin Fel
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000009, 232028, 2927, 1961, -3531.0, -930.9, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232030 Tharlidun, Stable Master
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000010, 232030, 2927, 1961, -3513.0, -979.9, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232035 Keena, Trade Goods
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000011, 232035, 2927, 1961, -3516.3001, -915.1, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232038 Uttnar, Butcher
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000012, 232038, 2927, 1961, -3485.8, -959.1, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 37 -- Hammerfall gnoll-camp filler (per-quest slot-11, gated to 90883 window) -- guid 8000013-8000026 (14 rows)
-- ============================================================================

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000013, 245027, 2927, 37, -1099.5348, -3538.7761, 51.6775, 5.7316, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000014, 245027, 2927, 37, -1095.731, -3562.3176, 49.2794, 0.6838, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000015, 245027, 2927, 37, -1076.6423, -3550.4011, 51.5098, 3.1003, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000016, 245027, 2927, 37, -1081.0017, -3560.2847, 51.0606, 2.3654, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000017, 245027, 2927, 37, -1073.4567, -3557.6145, 51.7315, 2.6257, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000018, 245027, 2927, 37, -1093.6285, -3548.1216, 49.6346, 5.0607, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000019, 245027, 2927, 37, -1083.3837, -3541.8142, 52.4749, 4.9389, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244670 Gnoll Bowblaster (gnoll camp filler, hostile; AIName=SmartAI)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000020, 244670, 2927, 37, -1013.5469, -3574.7432, 56.6479, 5.3338, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244670 Gnoll Bowblaster (gnoll camp filler, hostile; AIName=SmartAI)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000021, 244670, 2927, 37, -1014.908, -3516.7205, 61.7303, 0.0, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244671 Gnoll Ripper (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000022, 244671, 2927, 37, -1033.3021, -3551.8108, 56.2677, 3.1737, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244671 Gnoll Ripper (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000023, 244671, 2927, 37, -1010.8646, -3563.9548, 56.6479, 1.6648, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244672 Gnoll Bruiser (gnoll camp filler, hostile)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000024, 244672, 2927, 37, -960.0677, -3510.144, 57.0754, 3.3031, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244669 Scavenging Hyena
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000025, 244669, 2927, 37, -1020.7656, -3517.7917, 61.7577, 0.0, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244669 Scavenging Hyena
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000026, 244669, 2927, 37, -1015.9045, -3519.804, 61.4775, 3.5283, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 1959 -- Go'shek farm story-lead clones + Prized Pumpkin prop (base/terrain, farm window) -- guid 8000027-8000034 (8 rows)
-- ============================================================================

-- entry 244655 Lady Jaina Proudmoore (farm-phase clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000027, 244655, 2927, 1959, -1525.875, -3089.7986, 26.1175, 3.1821, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244656 Thrall (farm-phase clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000028, 244656, 2927, 1959, -1522.6198, -3085.8699, 26.1657, 1.5328, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244923 Farmer Bruvk (vehicle-ride clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000029, 244923, 2927, 1959, -3090.6001, -1523.1, 42.2, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244729 Farmer Bruvk (Go'shek farm clone, non-vehicle variant)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000030, 244729, 2927, 1959, -1522.3317, -3089.3594, 26.342, 2.1754, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000031, 244956, 2927, 1959, -3006.6001, -1536.9001, 42.2, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000032, 244956, 2927, 1959, -3010.6001, -1536.9001, 42.2, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000033, 244956, 2927, 1959, -3006.6001, -1540.9001, 42.2, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000034, 244956, 2927, 1959, -3010.6001, -1540.9001, 42.2, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 4 -- Go'shek farm trash + Runk (per-quest slot-11, gated to 90885/86/87) -- guid 8000035-8000044 (10 rows)
-- ============================================================================

-- entry 244674 Ogre Destroyer
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000035, 244674, 2927, 4, -3051.8, -1544.6, 42.2, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249254 Ogre Destroyer (alt entry)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000036, 249254, 2927, 4, -3091.2, -1522.3, 42.2, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244676 Kobold Pillager
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000037, 244676, 2927, 4, -3072.4001, -1531.1, 42.2, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249255 Kobold Pillager (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000038, 249255, 2927, 4, -1524.0903, -3097.3801, 26.0778, 3.0874, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249255 Kobold Pillager (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000039, 249255, 2927, 4, -1528.5521, -3094.2605, 26.0383, 1.6089, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249255 Kobold Pillager (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000040, 249255, 2927, 4, -1515.0104, -3094.2935, 27.6268, 3.2719, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249255 Kobold Pillager (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000041, 249255, 2927, 4, -1521.3229, -3079.894, 25.7415, 1.1354, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249255 Kobold Pillager (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000042, 249255, 2927, 4, -1527.1302, -3082.8594, 25.7332, 6.1616, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000043, 244677, 2927, 4, -2933.8001, -1506.8001, 42.2, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244675 Runk
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000044, 244675, 2927, 4, -2994.1001, -1476.9001, 42.2, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 1610 -- Stromgarde Keep hub leads/town NPCs (base/terrain, siege window) -- guid 8000045-8000078 (34 rows)
-- ============================================================================

-- entry 244657 Thrall (siege-entry clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000045, 244657, 2927, 1610, -1802.8001, -1474.4, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244658 Lady Jaina Proudmoore (siege-entry clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000046, 244658, 2927, 1610, -1802.8001, -1474.4, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244714 Lady Jaina Proudmoore (Stromgarde Keep hub lead)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000047, 244714, 2927, 1610, -1812.4, -1568.0, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000048, 229955, 2927, 1610, -1697.6875, -1883.2379, 80.0943, 5.8082, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000049, 229955, 2927, 1610, -1694.9653, -1892.3455, 80.0943, 5.6922, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000050, 229955, 2927, 1610, -1741.9045, -1635.8298, 53.8835, 2.2888, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000051, 229955, 2927, 1610, -1584.6858, -1853.1285, 67.6635, 1.1914, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000052, 229955, 2927, 1610, -1630.0817, -1795.7882, 80.0089, 5.8878, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000053, 229955, 2927, 1610, -1727.9445, -1591.4548, 52.5742, 1.1807, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000054, 229955, 2927, 1610, -1581.3923, -1909.0278, 68.0077, 0.9503, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000055, 229955, 2927, 1610, -1587.8085, -1902.4324, 69.8861, 4.9086, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000056, 229955, 2927, 1610, -1700.4062, -1581.9705, 53.684, 5.1514, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000057, 229955, 2927, 1610, -1652.0469, -1626.856, 69.9227, 1.361, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000058, 229955, 2927, 1610, -1589.5955, -1861.8212, 68.3938, 5.5461, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000059, 229955, 2927, 1610, -1682.6788, -1753.3368, 80.0923, 0.0, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000060, 229955, 2927, 1610, -1565.4062, -1907.0591, 67.9922, 6.1154, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000061, 229955, 2927, 1610, -1642.5596, -1780.4692, 80.0089, 5.5029, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000062, 229955, 2927, 1610, -1520.4062, -1895.356, 67.8515, 2.5698, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000063, 229955, 2927, 1610, -1569.3125, -1849.0851, 67.658, 4.2063, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000064, 229955, 2927, 1610, -1649.1788, -1637.257, 69.3422, 2.4771, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000065, 229955, 2927, 1610, -1559.6498, -1883.7653, 67.9979, 0.9031, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000066, 229955, 2927, 1610, -1735.5416, -1699.3195, 68.5629, 0.0, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000067, 229955, 2927, 1610, -1652.757, -1642.0139, 69.5957, 2.884, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000068, 229955, 2927, 1610, -1641.776, -1642.6771, 69.5955, 0.7231, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000069, 229955, 2927, 1610, -1639.9045, -1636.9618, 69.343, 0.2398, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000070, 229955, 2927, 1610, -1637.9185, -1635.9618, 69.343, 3.8026, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230004 Beggar
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000071, 230004, 2927, 1610, -1808.5, -1569.1, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000072, 244690, 2927, 1610, -1403.5928, -1963.7554, 50.7254, 2.0218, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000073, 244690, 2927, 1610, -1339.5931, -1671.2512, 54.5296, 0.6916, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000074, 244690, 2927, 1610, -1325.5704, -1657.3679, 51.8209, 4.4973, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000075, 244690, 2927, 1610, -1420.9205, -1973.3511, 49.9553, 1.1446, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000076, 244690, 2927, 1610, -1315.397, -1666.3346, 52.1274, 3.289, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000077, 244690, 2927, 1610, -1319.109, -1673.2623, 51.909, 2.6385, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000078, 244690, 2927, 1610, -1321.9951, -1660.0991, 51.9341, 4.0278, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 28 -- Stromgarde siege trash + Worn Catapult (per-quest slot-11, gated to 90893/95) -- guid 8000079-8000137 (59 rows)
-- ============================================================================

-- entry 244682 Kobold Waxmancer
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000079, 244682, 2927, 28, -1913.0, -1237.6, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244695 Ettin Crusher
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000080, 244695, 2927, 28, -1802.4, -1468.9, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000081, 244711, 2927, 28, -818.0312, -2006.1024, 58.178, 1.0265, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000082, 244711, 2927, 28, -882.816, -2024.9271, 55.4537, 4.9047, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000083, 244711, 2927, 28, -911.4809, -2031.9375, 55.9329, 4.6276, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000084, 244711, 2927, 28, -901.9965, -2070.5417, 56.8756, 5.1018, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000085, 244711, 2927, 28, -825.4879, -2043.1285, 60.2276, 2.5716, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000086, 244711, 2927, 28, -910.2188, -2039.9341, 56.4189, 3.5336, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000087, 244711, 2927, 28, -743.7552, -2062.2415, 66.4363, 0.1241, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000088, 244711, 2927, 28, -827.5243, -1968.2067, 52.7832, 1.6718, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000089, 244711, 2927, 28, -926.3108, -2078.7483, 59.9009, 4.5126, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000090, 244711, 2927, 28, -791.6788, -2020.5938, 58.6182, 6.119, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000091, 244711, 2927, 28, -859.6684, -2033.0781, 55.8945, 3.7937, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000092, 244711, 2927, 28, -839.5972, -1967.4028, 52.8969, 1.6718, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000093, 244711, 2927, 28, -876.3246, -2090.6406, 61.556, 2.3882, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000094, 244711, 2927, 28, -807.2864, -2069.8176, 68.8234, 5.0067, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000095, 244711, 2927, 28, -921.2413, -2037.2291, 56.7165, 0.0616, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000096, 244711, 2927, 28, -864.125, -2075.1858, 63.3473, 1.3784, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000097, 244711, 2927, 28, -899.7882, -2076.5139, 58.1608, 0.8796, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000098, 244711, 2927, 28, -824.4219, -1987.224, 52.9394, 4.304, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000099, 244711, 2927, 28, -842.0104, -1986.316, 53.4248, 4.6154, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000100, 244711, 2927, 28, -745.3733, -2073.3889, 66.1235, 0.1241, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000101, 244711, 2927, 28, -798.5695, -2074.5139, 68.8374, 3.252, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000102, 244711, 2927, 28, -818.2292, -2029.4254, 58.1741, 5.4539, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000103, 244711, 2927, 28, -848.2274, -2073.1997, 63.0546, 4.9134, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000104, 244711, 2927, 28, -866.2882, -2113.9915, 67.7591, 2.1356, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000105, 244711, 2927, 28, -816.0295, -2088.2014, 68.8234, 1.069, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000106, 244711, 2927, 28, -948.0886, -2157.3767, 59.9549, 3.9927, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000107, 244711, 2927, 28, -876.0695, -2023.2014, 55.4132, 4.9047, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000108, 244711, 2927, 28, -885.2604, -2006.9896, 57.9554, 1.3906, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000109, 244711, 2927, 28, -938.5608, -2172.5051, 60.1994, 3.9927, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000110, 244711, 2927, 28, -818.8542, -2077.7656, 68.8234, 5.9962, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000111, 244711, 2927, 28, -850.717, -2033.3854, 56.5718, 3.7937, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000112, 244711, 2927, 28, -872.9757, -2037.316, 55.6615, 3.884, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000113, 244711, 2927, 28, -931.7708, -2104.3977, 63.492, 1.3055, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000114, 244711, 2927, 28, -861.6493, -2006.4062, 55.9344, 4.9047, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000115, 244711, 2927, 28, -953.1788, -2020.3993, 54.7213, 2.5562, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000116, 244711, 2927, 28, -961.7465, -2031.4861, 55.6252, 2.5562, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244785 Armored Cleaver (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000117, 244785, 2927, 28, -1019.033, -1958.3004, 60.8064, 3.9718, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244785 Armored Cleaver (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000118, 244785, 2927, 28, -1022.6632, -2012.3524, 60.7162, 5.2502, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244785 Armored Cleaver (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000119, 244785, 2927, 28, -997.2274, -1968.3004, 61.5005, 2.5114, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244785 Armored Cleaver (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000120, 244785, 2927, 28, -1031.5087, -1996.6389, 60.8994, 1.1845, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244785 Armored Cleaver (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000121, 244785, 2927, 28, -1005.7396, -1978.2223, 61.6785, 1.5361, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244691 Gnoll Charger
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000122, 244691, 2927, 28, -1405.6216, -1816.1788, 59.8153, 2.6738, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244691 Gnoll Charger
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000123, 244691, 2927, 28, -1461.2007, -1795.8401, 67.0157, 3.0576, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244691 Gnoll Charger
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000124, 244691, 2927, 28, -1453.7101, -1797.1337, 65.3092, 3.4688, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244691 Gnoll Charger
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000125, 244691, 2927, 28, -1405.3923, -1809.9567, 59.8799, 2.9593, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000126, 244786, 2927, 28, -1022.2205, -1991.6649, 60.7162, 2.8385, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000127, 244786, 2927, 28, -1017.783, -1971.0278, 60.8181, 3.0052, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000128, 244786, 2927, 28, -1021.816, -2020.7014, 60.7165, 5.1677, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000129, 244786, 2927, 28, -1014.5452, -2004.2673, 60.7354, 3.7821, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000130, 244786, 2927, 28, -996.3472, -2020.5, 59.4196, 2.4807, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000131, 244786, 2927, 28, -1018.0573, -1986.908, 60.7296, 2.932, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 257072 Gnoll Biter
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000132, 257072, 2927, 28, -1771.3001, -1198.0, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000133, 244683, 2927, 28, -1803.9, -1371.4, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (siege-trash row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000134, 244685, 2927, 28, -1900.0, -1300.0, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (siege-trash row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000135, 244685, 2927, 28, -1892.0, -1292.0, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249269 Worn Catapult
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000136, 249269, 2927, 28, -1212.0104, -1869.9601, 91.6107, 2.74, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249269 Worn Catapult
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000137, 249269, 2927, 28, -1308.5868, -1787.2188, 62.8026, 3.14, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 3 -- Siege climax: Ro'grok + climax clones + Ogre Basher waves -- guid 8000138-8000143 (6 rows)
-- ============================================================================

-- entry 244709 Ro'grok
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000138, 244709, 2927, 3, -2120.9001, -870.3, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244666 Thrall (siege-climax clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000139, 244666, 2927, 3, -1980.7001, -1004.5, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244667 Lady Jaina Proudmoore (siege-climax clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000140, 244667, 2927, 3, -1983.7001, -1004.4, 80.0, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (climax 'wave' reinforcement row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000141, 244685, 2927, 3, -2120.9001, -870.3, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (climax 'wave' reinforcement row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000142, 244685, 2927, 3, -2110.9001, -870.3, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (climax 'wave' reinforcement row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000143, 244685, 2927, 3, -2120.9001, -860.3, 80.0, 0.0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- END -- 144 creature rows across guid 8000000-8000143 (reserved block 8000000-8000999).
-- ============================================================================
