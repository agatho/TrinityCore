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
-- Base 8000000, block 8000000-8000999 reserved. Actually used: 8000000-8000204, continuous,
-- 205 rows -- see per-section ranges below; the block has 795 guids of headroom left for a
-- later re-mine/expansion (guid layout was renumbered continuously from Fix Round 1's
-- gapped 8000000/8000100/8000200/8000300 sub-ranges since row counts changed substantially
-- once exact multi-instance spawns -- e.g. 244711's 36 rows -- were added).
-- HORDE-XVAL FIX (H1, 2026-08-20): guid 8000144-8000204 (61 rows) appended -- see the
--   HORDE-XVAL FIX banner near the end of the file for the full breakdown. 29 pre-existing
--   rows (guid 8000144's predecessors: 230004/230248/232022/232023/232030/232035/232038/
--   244657/244658/244666/244667/244674/244675/244676/244677/244682/244683/244685/244695/
--   244709/244956/249254, 29 rows total) were UPGRADED in place to Horde-wire-EXACT
--   positions and VerifiedBuild bumped to 69404 (see report for the full guid list).
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
 (8000003, 230248, 2927, 1961, -1049.901, -3545.01, 55.301, 3.287, 300, 69404)
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
 (8000007, 232022, 2927, 1961, -954.366, -3534.07, 70.604, 3.159, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232023 Gor'mul
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000008, 232023, 2927, 1961, -968.964, -3481.259, 55.214, 4.214, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232028 Korin Fel
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000009, 232028, 2927, 1961, -3531.0, -930.9, 56.5, 0.0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232030 Tharlidun, Stable Master
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000010, 232030, 2927, 1961, -955.191, -3491.03, 54.528, 2.981, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232035 Keena, Trade Goods
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000011, 232035, 2927, 1961, -916.493, -3535.198, 70.544, 2.396, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232038 Uttnar, Butcher
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000012, 232038, 2927, 1961, -957.208, -3478.123, 54.467, 4.08, 300, 69404)
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
 (8000031, 244956, 2927, 1959, -1514.288, -2971.832, 14.023, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000032, 244956, 2927, 1959, -1534.602, -3002.938, 14.025, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000033, 244956, 2927, 1959, -1514.288, -2971.832, 14.023, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000034, 244956, 2927, 1959, -1534.602, -3002.938, 14.025, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 4 -- Go'shek farm trash + Runk (per-quest slot-11, gated to 90885/86/87) -- guid 8000035-8000044 (10 rows)
-- ============================================================================

-- entry 244674 Ogre Destroyer
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000035, 244674, 2927, 4, -1437.175, -2943.42, 14.082, 6.048, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249254 Ogre Destroyer (alt entry)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000036, 249254, 2927, 4, -1532.484, -3088.445, 25.998, 5.0, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244676 Kobold Pillager
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000037, 244676, 2927, 4, -1564.078, -3025.773, 14.032, 0.316, 60, 69404)
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
 (8000043, 244677, 2927, 4, -1468.59, -2890.844, 14.518, 5.761, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244675 Runk
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000044, 244675, 2927, 4, -1457.769, -3005.009, 14.383, 6.094, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 1610 -- Stromgarde Keep hub leads/town NPCs (base/terrain, siege window) -- guid 8000045-8000078 (34 rows)
-- ============================================================================

-- entry 244657 Thrall (siege-entry clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000045, 244657, 2927, 1610, -1473.41, -1800.349, 94.86, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244658 Lady Jaina Proudmoore (siege-entry clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000046, 244658, 2927, 1610, -1473.169, -1803.616, 94.86, 0.0, 300, 69404)
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
 (8000071, 230004, 2927, 1610, -1577.957, -1790.967, 68.347, 3.287, 300, 69404)
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
 (8000079, 244682, 2927, 28, -1381.731, -1758.477, 52.871, 4.528, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244695 Ettin Crusher
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000080, 244695, 2927, 28, -1321.813, -1654.63, 51.882, 2.186, 300, 69404)
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
 (8000133, 244683, 2927, 28, -1243.903, -1661.156, 48.103, 3.209, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (siege-trash row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000134, 244685, 2927, 28, -1203.83, -1846.795, 115.122, 3.139, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (siege-trash row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000135, 244685, 2927, 28, -1292.62, -1924.504, 79.319, 2.507, 60, 69404)
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
 (8000138, 244709, 2927, 3, -854.188, -2133.155, 67.383, 5.228, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244666 Thrall (siege-climax clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000139, 244666, 2927, 3, -1000.915, -1982.212, 61.756, 5.586, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244667 Lady Jaina Proudmoore (siege-climax clone)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000140, 244667, 2927, 3, -1002.328, -1984.951, 61.629, 5.471, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (climax 'wave' reinforcement row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000141, 244685, 2927, 3, -1281.278, -1893.865, 76.205, 2.704, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (climax 'wave' reinforcement row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000142, 244685, 2927, 3, -1258.286, -1893.116, 80.569, 5.947, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (climax 'wave' reinforcement row)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000143, 244685, 2927, 3, -1304.764, -1796.889, 63.989, 2.941, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ============================================================================
-- HORDE-XVAL FIX (H1) -- guid 8000144-8000204 (61 rows): 40 Task-1 EXPANSION rows
--   (extra Horde-confirmed spawn points for 230004/230248/244674/244677/244682/244683/
--   244685) + 21 Task-2 ADD rows (Horde-specific population: Hammerfall cluster +
--   Stromgarde extras). Source: creature_spawns_objupdate.zone_2927.sql (Horde bundle),
--   VerifiedBuild=69404. SPAWNS ONLY -- no new creature_template authored.
-- ============================================================================

-- entry 230004 Beggar (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000144, 230004, 2927, 1610, -1631.634, -1780.17, 80.599, 1.611, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230004 Beggar (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000145, 230004, 2927, 1610, -1532.016, -1907.175, 68.386, 3.606, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230004 Beggar (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000146, 230004, 2927, 1610, -1524.858, -1666.906, 68.097, 3.15, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230004 Beggar (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000147, 230004, 2927, 1610, -1705.071, -1910.026, 80.579, 4.73, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230248 Hammerfall Grunt (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000148, 230248, 2927, 1961, -857.653, -3513.472, 73.0, 5.986, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230248 Hammerfall Grunt (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000149, 230248, 2927, 1961, -1050.076, -3556.222, 55.269, 3.074, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230248 Hammerfall Grunt (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000150, 230248, 2927, 1961, -857.986, -3524.934, 72.972, 0.157, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230248 Hammerfall Grunt (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000151, 230248, 2927, 1961, -986.306, -3528.998, 57.09, 2.575, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244674 Ogre Destroyer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000152, 244674, 2927, 4, -1600.613, -2976.786, 22.345, 0.668, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244674 Ogre Destroyer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000153, 244674, 2927, 4, -1417.783, -2982.183, 19.155, 2.625, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244674 Ogre Destroyer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000154, 244674, 2927, 4, -1524.95, -2862.679, 14.025, 1.528, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244674 Ogre Destroyer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000155, 244674, 2927, 4, -1614.679, -2876.821, 19.783, 2.412, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244674 Ogre Destroyer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000156, 244674, 2927, 4, -1534.097, -2928.875, 14.243, 1.606, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000157, 244677, 2927, 4, -1470.748, -2944.911, 14.661, 1.482, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000158, 244677, 2927, 4, -1565.342, -2917.422, 14.023, 4.179, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000159, 244677, 2927, 4, -1517.613, -2944.642, 14.023, 0.981, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000160, 244677, 2927, 4, -1509.53, -2917.193, 14.023, 4.832, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000161, 244677, 2927, 4, -1463.666, -2952.973, 14.797, 1.764, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000162, 244682, 2927, 28, -1218.688, -1915.243, 87.644, 4.656, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000163, 244682, 2927, 28, -1235.885, -1930.812, 89.101, 5.518, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000164, 244682, 2927, 28, -1343.75, -1772.363, 49.434, 0.563, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000165, 244682, 2927, 28, -1175.727, -1936.566, 90.277, 4.656, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000166, 244682, 2927, 28, -1190.448, -1959.955, 91.041, 5.846, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000167, 244682, 2927, 28, -1186.182, -1928.766, 86.019, 0.297, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000168, 244682, 2927, 28, -1364.405, -1876.208, 51.688, 1.153, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000169, 244682, 2927, 28, -1376.998, -1871.345, 51.497, 1.448, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000170, 244682, 2927, 28, -1307.547, -1793.168, 63.431, 1.187, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000171, 244682, 2927, 28, -1263.309, -1737.608, 53.224, 6.212, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244682 Kobold Waxmancer (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000172, 244682, 2927, 28, -1345.616, -1788.125, 50.779, 2.198, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000173, 244683, 2927, 28, -1366.032, -1784.486, 61.988, 1.352, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000174, 244683, 2927, 28, -1274.698, -1844.071, 81.709, 2.512, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000175, 244683, 2927, 28, -1231.531, -1782.307, 64.936, 3.508, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000176, 244683, 2927, 28, -1330.334, -1790.616, 61.988, 1.384, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000177, 244683, 2927, 28, -1257.556, -1734.151, 53.389, 1.738, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (Horde-xval EXPAND, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000178, 244683, 2927, 28, -1210.524, -1735.707, 59.291, 2.135, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (Horde-xval EXPAND, siege-trash, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000179, 244685, 2927, 28, -1200.045, -1874.247, 91.668, 2.749, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (Horde-xval EXPAND, siege-trash, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000180, 244685, 2927, 28, -1349.943, -1888.292, 63.067, 2.941, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (Horde-xval EXPAND, climax-wave, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000181, 244685, 2927, 3, -1370.384, -1846.622, 62.975, 6.009, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (Horde-xval EXPAND, climax-wave, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000182, 244685, 2927, 3, -1198.036, -1764.974, 58.873, 3.139, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher (Horde-xval EXPAND, climax-wave, extra Horde-confirmed spawn point)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000183, 244685, 2927, 3, -1159.377, -1753.982, 54.206, 5.299, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232027 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000184, 232027, 2927, 1961, -894.12, -3550.656, 71.304, 2.191, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232031 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000185, 232031, 2927, 1961, -949.701, -3532.125, 57.065, 5.304, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232033 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000186, 232033, 2927, 1961, -1016.342, -3493.712, 62.186, 5.574, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232037 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000187, 232037, 2927, 1961, -880.905, -3503.436, 71.854, 4.327, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244715 Thrall (Horde hub -- template added by H2, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000188, 244715, 2927, 1961, -942.819, -3533.63, 70.639, 2.525, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 246612 Stromgarde Exile (ENEMY, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000189, 246612, 2927, 37, -903.144, -3491.733, 70.645, 2.451, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 246612 Stromgarde Exile (ENEMY, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000190, 246612, 2927, 37, -904.248, -3498.804, 70.597, 4.918, 60, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 246613 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000191, 246613, 2927, 1961, -903.788, -3501.516, 70.617, 2.062, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 31216 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000192, 31216, 2927, 1961, -1091.763, -3554.803, 49.642, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 68662 (Horde Hammerfall cluster, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000193, 68662, 2927, 1961, -1028.302, -3554.224, 56.517, 0.637, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245052 Horde Grunt (Horde-xval ADD, supplements movement-src rows in 12_ambient_spawns.sql)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000194, 245052, 2927, 1961, -954.142, -3537.071, 56.926, 5.241, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245052 Horde Grunt (Horde-xval ADD, supplements movement-src rows in 12_ambient_spawns.sql)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000195, 245052, 2927, 1961, -952.947, -3523.74, 70.6, 4.307, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229959 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000196, 229959, 2927, 1610, -1696.443, -1584.203, 54.466, 1.941, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229966 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000197, 229966, 2927, 1610, -1783.613, -1594.852, 54.8, 1.571, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229967 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000198, 229967, 2927, 1610, -1778.295, -1597.351, 54.8, 0.925, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 231218 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000199, 231218, 2927, 1610, -1597.818, -1773.37, 68.086, 2.078, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 231218 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000200, 231218, 2927, 1610, -1597.3, -1751.754, 68.086, 3.445, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 235709 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000201, 235709, 2927, 1610, -1703.24, -1559.866, 54.66, 0.0, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249423 (Stromgarde extras, Horde-xval ADD)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000202, 249423, 2927, 1610, -1473.38, -1808.148, 94.273, 0.326, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (Horde-xval ADD, supplements 11-row cluster in 12_ambient_spawns.sql)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000203, 254547, 2927, 1610, -1340.939, -1838.618, 62.567, 2.723, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (Horde-xval ADD, supplements 11-row cluster in 12_ambient_spawns.sql)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000204, 254547, 2927, 1610, -1484.065, -1801.192, 67.52, 0.065, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- END -- 205 creature rows across guid 8000000-8000204 (reserved block 8000000-8000999).
--   (144 Fix-Round-2 baseline + 40 Horde-xval Task-1 expansion + 21 Horde-xval Task-2 additions)
-- ============================================================================
