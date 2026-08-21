-- ============================================================================
-- ARATHI CATCH-UP / RPE CONSOLIDATION -- SPAWNS + PHASING (issue 6)
-- ============================================================================
-- Branch: feature/arathi-rpe   Path: sql/updates/world/master/   Server mapID: 2927
-- Consolidated verbatim from the authoritative content slices (guid block 8000000);
-- runs after 2026_08_21_00 cleanup. Each source slice keeps its own banner + idempotency.
-- Single authoritative spawn set (500 reconciled rows, guid 8000000-8001294). Replaces content 20+12 and the retired 11002xxx block.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: reconciled creature (staging-reconciled-creature.sql, +R1 corpse relabel)  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- STAGING / RECONCILED creature spawns -- Arathi Catch-Up (map 2927)
-- ============================================================================
-- CANDIDATE ONLY -- STAGING. Do NOT apply to any branch/realm. Human review required.
-- Produced by the spawn-reconcile pass (2026-08-21): MERGE + DEDUP of
--   Set A (content slices 20_creature_spawns.sql + 12_ambient_spawns.sql, guid 8000000 block)
--   Set B (arathi-rpe 2026_08_15_50..57, guid 11002000 block, third-party 68453 capture).
-- Set A is authoritative and carried VERBATIM (its own DELETE-by-range + INSERT..ODKU below).
-- Of Set B's 39 creature rows: 27 were exact duplicates of Set A (dropped), 9 were density
--   padding (dropped), 3 were kept as genuine camp reinforcement (SET-B-KEPT banner, main
--   section). The 245027 pad 'gnolls' the tester flagged were the DOUBLING (Set A 7 + Set B 7
--   identical rows = 14); dedup restores the real 7 (feign-death tableau, Alliance objupdate
--   confirms exactly 7). See phk-spawn-reconcile-report.md for every decision + open rulings.
-- Two clearly-separated sections preserved: MAIN ROSTER (col layout guid,id,map,PhaseId,x,y,z,
--   o,spawntimesecs,VerifiedBuild) then AMBIENT (adds wander_distance,MovementType).
-- ============================================================================

-- ####################  SECTION 1 : MAIN ROSTER (guid 8000000-8000207)  ####################

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

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000013, 245027, 2927, 37, -1099.5348, -3538.7761, 51.6775, 5.7316, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000014, 245027, 2927, 37, -1095.731, -3562.3176, 49.2794, 0.6838, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000015, 245027, 2927, 37, -1076.6423, -3550.4011, 51.5098, 3.1003, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000016, 245027, 2927, 37, -1081.0017, -3560.2847, 51.0606, 2.3654, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000017, 245027, 2927, 37, -1073.4567, -3557.6145, 51.7315, 2.6257, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000018, 245027, 2927, 37, -1093.6285, -3548.1216, 49.6346, 5.0607, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (pad corpse tableau (feign-death))
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
-- SET-B-KEPT (reconcile) -- gnoll-camp reinforcement, guid 8000205-8000207 (3 rows)
--   Origin: arathi-rpe Set B (2026_08_15_51), third-party retail capture 12.0.7.68453.
--   VerifiedBuild=0 (honest: NOT from our TCHarvest wire; positions adopted, build unverified).
--   PhaseId 37 + spawntimesecs 60 = Set A's hostile gnoll-camp filler convention.
--   WHY KEPT: Set A under-counts the quest-objective gnolls (244672 Bruiser had only 1 spawn;
--   quest 90882/90883 'slay gnolls' needs plausible simultaneous targets). Our own captures
--   put the realistic SIMULTANEOUS Bruiser count at 2-3 (Alliance objupdate=1 distinct,
--   Horde addon=2 distinct) -- NOT Set B's session-total of 10. Camp thinned to that reality.
--   >>> HUMAN RULING REQUESTED on the exact camp counts -- see report.
-- ============================================================================

-- entry 244672 Gnoll Bruiser (Set-B-kept 11002008; camp #2 -- 15.7yd from Set A's lone 8000024)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000205, 244672, 2927, 37, -975.55646, -3512.6892, 56.992092, 0.0, 60, 0)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244672 Gnoll Bruiser (Set-B-kept 11002005; camp #3 -- 44.4yd from 8000024, fills camp mid)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000206, 244672, 2927, 37, -999.4901, -3530.6072, 56.98025, 0.0, 60, 0)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244671 Gnoll Ripper (Set-B-kept 11002019; 34yd distinct -- brings Ripper camp to 3, matches Alliance objupdate=3)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000207, 244671, 2927, 37, -985.0482, -3541.8672, 56.992737, 0.901641, 60, 0)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- END -- 208 creature rows across guid 8000000-8000207 (reserved block 8000000-8000999).
--   (RECONCILE: +3 Set-B-kept camp rows 8000205-8000207 appended; see SET-B-KEPT banner above.)
--   (144 Fix-Round-2 baseline + 40 Horde-xval Task-1 expansion + 21 Horde-xval Task-2 additions)
-- ============================================================================


-- ####################  SECTION 2 : AMBIENT WILDLIFE/NPC (guid 8001000-8001294)  ####################
-- (carried verbatim from 12_ambient_spawns.sql; native column layout incl. wander_distance,MovementType)

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: ambient creature population (SPAWNS ONLY)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927.
-- SPAWNS-ONLY: all 46 entries below already exist in creature_template in the world DB
--   (verified in the oracle) -- this file authors ZERO creature_template /
--   creature_template_addon rows. Do not touch any other slice.
--   (32 Fix-Round-2 baseline entries + 14 Horde-xval Task-2 additions: wildlife 883/2620/
--   142337/142338/142339/142340/142343, NW exile/elemental 114590/141663/141946/141947/
--   142021, farm/misc 142566/253463 -- see the HORDE-XVAL FIX banners near the end of file.)
-- Source: TCHarvest re-mine of map 2927 --
--   C:/dumps/tcharvest/out/catchup_zone_REMAP/zone_2927/creature_spawns_objupdate.zone_2927.sql
--     (SMSG_UPDATE_OBJECT create-block decode -- EXACT x/y/z/o per spawned GUID instance)
--   C:/dumps/tcharvest/out/catchup_zone_REMAP/zone_2927/creature_spawns_movement.zone_2927.sql
--     (stationary-creature movement-opcode position confirmations -- x/y/z only, no facing;
--     used only for the 5 of our 32 entries that have NO objupdate row: 223453, 231309 (2nd
--     spawn), 245052, 54983 -- orientation defaulted to 0.0 on those rows, flagged per-row).
--   VerifiedBuild=69382 carried from the same capture session (neither source file restates
--   its own build number; see 20_creature_spawns.sql's banner for the same convention).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent: DELETE by the reserved guid range below, then INSERT ... ON DUPLICATE KEY
--   UPDATE (belt-and-suspenders -- re-apply safe either way).
-- ============================================================================
--
-- ---- RESERVED GUID RANGE ----
-- Base 8001000, block 8001000-8001999 reserved for this file. Actually used:
-- 8001000-8001294 (293 rows -- 2 guids in that range, 8001106-8001107, are a pre-existing
-- reserved-but-unused gap for the excluded 62821/62822, see below): 217 Fix-Round-2 baseline
-- rows [8001000-8001218 minus the gap] + 76 Horde-xval Task-2 rows [8001219-8001294: 65
-- wildlife + 9 NW exile/elemental + 2 farm/misc]. Does NOT collide with
-- 20_creature_spawns.sql's reserved 8000000-8000999 block (actual use there:
-- 8000000-8000204 after the Horde-xval fix).
--
-- ---- HORDE-XVAL FIX (H1) -- 2026-08-20 ----
-- 231219 Stromgarde Citizen (guid 8001080) UPGRADED from a movement-decoder wander centroid
--   to the Horde bundle's EXACT wire position. 8 existing entries (4075, 230002, 13321,
--   49778, 49999, 229999, 142333, 142334 -- 134 rows total) converted to MovementType=1
--   (random wander) with wander_distance=8 per Task 3 (uniform mid-of-5-10yd value; see
--   report -- Horde per-point scatter for these already-collapsed/merged entries wasn't
--   cleanly separable from decode noise in this pass). 76 new Task-2 rows appended at the
--   end of the file (wildlife/NW-exile/farm-misc, see their own banners). VerifiedBuild=69404
--   on every touched/added row. Source: creature_spawns_objupdate.zone_2927.sql (Horde
--   bundle). SPAWNS ONLY -- no new creature_template authored.
--
-- ---- DEDUP RULE APPLIED ----
-- Multiple distinct GUIDs/positions for one entry are kept as multiple real spawns (this is
-- ambient wildlife/NPC set-dressing -- many legitimate points per entry is expected, e.g.
-- Plains Creeper 142334 has 56 kept positions spread across the whole zone). Two raw wire
-- rows for the same entry were collapsed into one spawn only when their Euclidean distance
-- (x/y/z) was < 1 yard -- this fired mainly on the Hammerfall Peon 249249 cluster, where the
-- decoder emitted near-identical z for the same point twice.
--
-- ---- 3 ENTRIES WITH NO USABLE WIRE POSITION ----
-- 231219 Stromgarde Citizen, 62821 Mystic Birdhat, 62822 Cousin Slowhands: both source files
-- decode ONLY degenerate/garbage floats for these 3 entries (e.g. x=-1.48e-34, or
-- target_x/y/z/o=0.0/0.0/0.0/0.0 in the accompanying smart_scripts_candidates.sql waypoint
-- row for 231219) -- not real positions, just decode noise. Per the brief's spawns-only,
-- house-style convention (matching 20_creature_spawns.sql's tier-(c) approximate rows), each
-- gets ONE flagged PLACEHOLDER row at the Stromgarde-town cluster centroid
-- (-1550.0, -1800.0, 67.6, o=0.0) inferred from the surrounding confirmed town-NPC spawns
-- (231287 Stonemason, 231309 Engineer, 254547 Footman, 230001 Orphan all cluster in this
-- x=-1300..-1700 / y=-1600..-2200 band). Flagged per-row and in Concerns -- Phase-K
-- resurvey/replace-with-real-wire-position is mandatory before this ships blizzlike.
--
-- ---- THE PhaseId MAP (assigned per creature role, from the brief's real phase-graph ids) ----
-- PhaseId 1959 (terrain, base) -- ambient critters (always-present, non-quest fauna) +
--   town/ambient NPCs, flavor vendors, Training Dummy (populated Stromgarde/Hammerfall area)
--   + the 4 role-uncertain exiles/guardian/treant (flagged, see below).
-- PhaseId 28   (per-quest, gated to siege window 90893/90895) -- Boulderfist Ogre/Enforcer
--   (hostile siege trash) + Worn Catapult (siege-interact prop). Brief allows 28 or terrain
--   1610 for this cluster; 28 (per-quest/gated) chosen here, matching 20_creature_spawns.sql's
--   convention of routing hostile filler/trash/quest-props to the per-quest id rather than the
--   terrain id (leads/vendor/hub NPCs get the terrain id; hostiles get the gated id).
-- PhaseId 3    (completion, post-90896) -- post-siege PEACE wildlife (142333/142334/142335/
--   142341/142342/142347/223453), matching the wire's "wildlife appears when siege ends".
--
-- ---- HONEST BOUNDARY (brief Requirement 2 / role-uncertain flags) ----
-- Every PhaseId below is INFERRED by role, not a decoded per-creature phase tag (no such tag
-- exists on this wire capture -- phase_shift.sql's own header says the wire gives the
-- player's phase SET over time, not a per-spawn tag). 141659 Rumbling Guardian, 141725
-- Burning Exile, 141727 Rumbling Exile, and 54983 Treant are assigned terrain phase 1959 per
-- the brief but flagged "role uncertain -- Phase-K refine" on every row: these 4 read as a
-- distinct sub-encounter (exile camp + treant) that may belong to a narrower sub-phase not
-- pinned by this wire capture.
--
-- spawntimesecs convention (house-style, documented not decoded): 120s ambient critters +
-- role-uncertain exiles/treant (fast-cycling flavor), 300s town/vendor NPCs (persistent) and
-- peace wildlife (low-frequency post-siege scenery), 60s siege trash/catapult prop
-- (fast-cycling set-dressing, matching 20_creature_spawns.sql's hostile-filler convention).
-- ============================================================================

DELETE FROM `creature` WHERE `guid` BETWEEN 8001000 AND 8001999;

-- ========================================================================================
-- PHASE 1959 -- Ambient critters -- always-present flavor fauna (base/terrain, non-quest)
-- ========================================================================================
-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001000, 4075, 2927, 1959, -1601.327, -1840.8353, 67.8147, 3.5294, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001001, 4075, 2927, 1959, -774.3948, -2072.1633, 67.9599, 6.2215, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001002, 4075, 2927, 1959, -1493.4873, -1740.8844, 67.5802, 5.8778, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001003, 4075, 2927, 1959, -805.1129, -2062.3645, 66.6265, 4.6949, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001004, 4075, 2927, 1959, -1498.4186, -1669.3291, 68.9361, 4.25, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001005, 4075, 2927, 1959, -1499.9966, -1645.2778, 67.1101, 0.3161, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001006, 4075, 2927, 1959, -1497.0709, -1664.4011, 68.1689, 1.2687, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001007, 4075, 2927, 1959, -1496.272, -1649.5068, 67.4544, 2.0108, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001008, 4075, 2927, 1959, -1607.632, -1643.224, 67.6858, 3.159, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001009, 4075, 2927, 1959, -1497.1511, -1736.191, 67.5802, 5.4963, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001010, 4075, 2927, 1959, -1598.4961, -1840.1074, 67.5801, 6.2806, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 4075 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001011, 4075, 2927, 1959, -1294.3123, -2602.9048, 61.9397, 3.8093, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001012, 230002, 2927, 1959, -1688.1602, -1862.62, 80.3328, 2.8364, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001013, 230002, 2927, 1959, -1505.9551, -1906.6212, 68.3287, 4.9083, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001014, 230002, 2927, 1959, -1596.4822, -1762.0735, 68.1906, 5.1716, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001015, 230002, 2927, 1959, -1572.916, -1651.5625, 67.7461, 2.4982, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001016, 230002, 2927, 1959, -1604.3733, -1661.4288, 68.2041, 2.7535, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001017, 230002, 2927, 1959, -1733.4807, -1713.4701, 68.2184, 4.6881, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001018, 230002, 2927, 1959, -1654.7333, -1594.8051, 69.0499, 3.3127, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001019, 230002, 2927, 1959, -1594.0116, -1761.6497, 68.2874, 0.064, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001020, 230002, 2927, 1959, -1592.6888, -1668.562, 67.5947, 4.6519, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001021, 230002, 2927, 1959, -1632.9305, -1824.9915, 80.729, 3.4637, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001022, 230002, 2927, 1959, -1586.1875, -1661.1777, 70.3979, 3.0111, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001023, 230002, 2927, 1959, -1653.5509, -1619.0162, 69.1044, 4.2693, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001024, 230002, 2927, 1959, -1552.1998, -1646.7717, 67.5802, 1.3315, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001025, 230002, 2927, 1959, -1592.9219, -1758.8815, 68.1937, 0.0435, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001026, 230002, 2927, 1959, -1664.5387, -1646.5382, 68.304, 2.3073, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001027, 230002, 2927, 1959, -1551.0598, -1923.1654, 68.6298, 4.3862, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001028, 230002, 2927, 1959, -1582.9397, -1653.3341, 69.8, 2.6924, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001029, 230002, 2927, 1959, -1596.0151, -1798.9188, 74.2862, 5.5034, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001030, 230002, 2927, 1959, -1552.5878, -1842.9672, 67.5802, 3.2132, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001031, 230002, 2927, 1959, -1630.7986, -1631.8802, 69.2089, 5.9093, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001032, 230002, 2927, 1959, -1685.9307, -1573.0829, 54.209, 2.4212, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230002 Rat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001033, 230002, 2927, 1959, -1581.9828, -1868.1116, 68.3938, 2.293, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001034, 13321, 2927, 1959, -1437.5254, -1520.7919, 53.6889, 2.1117, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001035, 13321, 2927, 1959, -1445.7572, -1638.5079, 45.5333, 1.0056, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001036, 13321, 2927, 1959, -1399.5547, -1587.3672, 52.3039, 4.52, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001037, 13321, 2927, 1959, -1407.2656, -1658.3594, 45.001, 2.2931, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001038, 13321, 2927, 1959, -1399.1704, -1662.5526, 47.0011, 4.5575, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001039, 13321, 2927, 1959, -1453.084, -1611.3672, 56.918, 4.4314, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001040, 13321, 2927, 1959, -1442.9604, -1634.6539, 49.6938, 4.5122, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001041, 13321, 2927, 1959, -1447.4583, -1813.6556, 65.6397, 5.8729, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001042, 13321, 2927, 1959, -1406.2052, -1829.316, 53.2492, 1.5391, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001043, 13321, 2927, 1959, -1402.5275, -1586.5028, 52.7427, 2.8013, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 13321 Small Frog (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001044, 13321, 2927, 1959, -1432.6204, -1521.6969, 51.5, 0.2445, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49778 Red-Tailed Chipmunk (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001045, 49778, 2927, 1959, -982.4451, -2067.6067, 54.4047, 0.0132, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49778 Red-Tailed Chipmunk (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001046, 49778, 2927, 1959, -1111.8152, -1932.306, 74.2094, 6.0022, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001047, 49999, 2927, 1959, -1334.7598, -2102.5957, 65.9178, 1.5864, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001048, 49999, 2927, 1959, -1632.0636, -1771.0, 80.635, 0.2742, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001049, 49999, 2927, 1959, -1504.4626, -2380.4431, 71.5557, 3.2315, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001050, 49999, 2927, 1959, -1108.1143, -1937.4995, 72.4463, 0.1901, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001051, 49999, 2927, 1959, -1210.006, -1866.0193, 91.8471, 4.2567, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001052, 49999, 2927, 1959, -1667.4305, -1735.1858, 80.3195, 3.4907, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001053, 49999, 2927, 1959, -1710.416, -1750.5195, 80.2941, 1.249, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001054, 49999, 2927, 1959, -1658.7369, -1652.5032, 69.068, 3.4207, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001055, 49999, 2927, 1959, -1671.5814, -1681.1603, 67.5802, 2.9155, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001056, 49999, 2927, 1959, -1707.6079, -1681.4795, 67.9132, 2.896, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001057, 49999, 2927, 1959, -1732.5154, -1616.0376, 54.2849, 2.016, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001058, 49999, 2927, 1959, -1632.1392, -1774.6309, 80.1901, 5.4984, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001059, 49999, 2927, 1959, -1632.3906, -1584.6406, 68.2233, 5.3954, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001060, 49999, 2927, 1959, -1263.153, -2592.2073, 55.1265, 4.1173, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 49999 Grasslands Cottontail (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001061, 49999, 2927, 1959, -1695.9166, -1823.8073, 80.7118, 3.4907, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001062, 229999, 2927, 1959, -1726.6665, -1702.5503, 68.4983, 6.0808, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001063, 229999, 2927, 1959, -1564.1312, -1622.2886, 70.0212, 5.6696, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001064, 229999, 2927, 1959, -1592.4791, -1664.3959, 67.817, 0.0, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001065, 229999, 2927, 1959, -1599.1809, -1870.6063, 67.7381, 5.1692, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001066, 229999, 2927, 1959, -1592.2676, -1849.6995, 67.6167, 5.8199, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001067, 229999, 2927, 1959, -1632.705, -1594.8873, 67.6693, 3.6014, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001068, 229999, 2927, 1959, -1525.1122, -1874.7683, 68.4944, 0.0767, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229999 Stray Cat (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001069, 229999, 2927, 1959, -1599.8468, -1774.209, 68.2657, 3.7782, 120, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ========================================================================================
-- PHASE 1959 -- Town/ambient NPCs + flavor vendors + Training Dummy (base/terrain, populated Stromgarde/Hammerfall area)
-- ========================================================================================
-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001070, 230001, 2927, 1959, -1696.7769, -1869.7943, 80.0109, 2.3074, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001071, 230001, 2927, 1959, -1695.7792, -1870.3378, 80.011, 1.5847, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001072, 230001, 2927, 1959, -1627.5382, -1873.7593, 81.2262, 5.3313, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001073, 230001, 2927, 1959, -1632.8696, -1865.0547, 81.7262, 5.2182, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001074, 230001, 2927, 1959, -1634.0216, -1865.8751, 81.6284, 5.3103, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001075, 230001, 2927, 1959, -1632.7039, -1864.0686, 81.8721, 4.7192, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001076, 230001, 2927, 1959, -1652.0868, -1645.5695, 69.5904, 4.0711, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001077, 230001, 2927, 1959, -1498.6188, -1882.173, 68.1506, 5.5669, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001078, 230001, 2927, 1959, -1500.3789, -1882.0118, 68.1275, 5.9488, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230001 Stromgarde Orphan (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001079, 230001, 2927, 1959, -1501.0647, -1880.5878, 68.1673, 6.1514, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 231219 Stromgarde Citizen (Horde-xval UPGRADE: replaces the movement-decoder wander
--   centroid above with the Horde bundle's EXACT wire position -- creature_spawns_objupdate.
--   zone_2927.sql, single clean point, VerifiedBuild=69404).
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001080, 231219, 2927, 1959, -1586.625, -1861.347, 68.394, 4.241, 300, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 231287 Stromgarde Stonemason (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001081, 231287, 2927, 1959, -1344.3055, -1808.3004, 61.7815, 1.9799, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 231309 Stromic Engineer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001082, 231309, 2927, 1959, -1518.7141, -1860.5575, 69.1646, 4.5016, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 231309 Stromic Engineer (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001083, 231309, 2927, 1959, -1516.0983, -1865.9081, 69.0517, 0.0, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001084, 254547, 2927, 1959, -1559.4734, -1801.4167, 67.5802, 3.1418, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001085, 254547, 2927, 1959, -1666.8143, -1763.6508, 80.0093, 6.232, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001086, 254547, 2927, 1959, -1565.4805, -1802.2433, 67.5802, 0.0002, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001087, 254547, 2927, 1959, -1572.17, -1775.0038, 67.5802, 1.8016, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001088, 254547, 2927, 1959, -1639.6185, -1810.8005, 80.0099, 4.4238, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001089, 254547, 2927, 1959, -1540.423, -1803.3009, 67.5802, 3.1377, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001090, 254547, 2927, 1959, -1430.3347, -1809.8317, 61.29, 2.6779, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001091, 254547, 2927, 1959, -1344.2472, -1899.0155, 60.2862, 1.1569, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001092, 254547, 2927, 1959, -1361.394, -1829.2494, 61.6602, 5.8583, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001093, 254547, 2927, 1959, -1382.7245, -1818.8486, 59.0021, 2.7426, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 254547 Stromgarde Footman (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001094, 254547, 2927, 1959, -1676.489, -1762.0544, 80.01, 6.2269, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230245 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001095, 230245, 2927, 1959, -927.8088, -3509.8352, 70.086, 3.2412, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249249 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001096, 249249, 2927, 1959, -1518.1163, -3089.2622, 26.8409, 4.3889, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249249 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001097, 249249, 2927, 1959, -1526.1337, -3086.6736, 26.0891, 4.7239, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249249 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001098, 249249, 2927, 1959, -1518.4531, -3092.2744, 26.896, 3.291, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249249 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001099, 249249, 2927, 1959, -1524.3802, -3092.4236, 26.2967, 2.352, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249249 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001100, 249249, 2927, 1959, -1525.6406, -3093.1372, 26.2129, 0.8511, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249249 Hammerfall Peon (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001101, 249249, 2927, 1959, -1520.2848, -3092.4236, 26.6111, 0.0804, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245028 Horde Grunt (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001102, 245028, 2927, 1959, -1083.7812, -3553.5815, 50.545, 2.1282, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245028 Horde Grunt (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001103, 245028, 2927, 1959, -1088.342, -3542.4358, 50.3749, 3.9584, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245052 Horde Grunt (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001104, 245052, 2927, 1959, -1020.0854, -3520.7302, 61.296, 0.0, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245052 Horde Grunt (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001105, 245052, 2927, 1959, -933.7572, -3531.5946, 70.8668, 0.0, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entries 62821 Mystic Birdhat + 62822 Cousin Slowhands -- EXCLUDED (re-examined): these are roaming
--   Trading Post vendors captured on the map-85 Chromie-Time hub (addon context: 62821 map=85,
--   quests 51443/62568). The movement decoder's degenerate near-zero decode ALSO mis-tagged them
--   map=2927 -- a decode artifact, not a real Arathi-2927 spawn. They are BLEED, not instance residents.
--   No spawn authored.

-- entry 249245 Training Dummy (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001108, 249245, 2927, 1959, -1095.2067, -3534.9358, 52.1501, 4.1114, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249245 Training Dummy (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001109, 249245, 2927, 1959, -1098.3038, -3531.0071, 52.3493, 4.1114, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249245 Training Dummy (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001110, 249245, 2927, 1959, -1101.7101, -3526.342, 52.1221, 4.1114, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249245 Training Dummy (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001111, 249245, 2927, 1959, -1105.4531, -3515.2778, 51.7059, 3.2898, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249245 Training Dummy (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001112, 249245, 2927, 1959, -1104.4705, -3521.9202, 52.1524, 3.4049, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ========================================================================================
-- PHASE 1959 -- Exiles/guardian/treant -- role uncertain, flagged for Phase-K refine (assigned base terrain phase pending pin)
-- ========================================================================================
-- entry 141659 Rumbling Guardian (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001113, 141659, 2927, 1959, -1514.4219, -2136.8369, 17.2038, 1.1675, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141659 Rumbling Guardian (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001114, 141659, 2927, 1959, -1501.9913, -2174.5972, 17.2509, 5.9378, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141725 Burning Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001115, 141725, 2927, 1959, -1232.563, -2178.6963, 61.9625, 1.0012, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141725 Burning Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001116, 141725, 2927, 1959, -1176.5117, -2141.3276, 60.2205, 2.8236, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141725 Burning Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001117, 141725, 2927, 1959, -1201.9615, -2145.3186, 59.9322, 2.0636, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001118, 141727, 2927, 1959, -1586.6816, -2179.5, 28.3225, 2.3921, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001119, 141727, 2927, 1959, -1482.5868, -2181.5825, 17.2971, 3.764, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001120, 141727, 2927, 1959, -1483.8258, -2220.4832, 26.776, 4.0279, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001121, 141727, 2927, 1959, -1581.3782, -2149.7996, 19.8209, 0.6693, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001122, 141727, 2927, 1959, -1589.7151, -2111.8499, 31.5371, 2.3383, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001123, 141727, 2927, 1959, -1547.7275, -2118.2666, 17.4124, 2.6484, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141727 Rumbling Exile (src=objupdate)  [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001124, 141727, 2927, 1959, -1476.0576, -2149.2351, 18.1775, 3.1004, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 54983 Treant (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0 | role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001125, 54983, 2927, 1959, -1118.009, -3538.2949, 51.7358, 0.0, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 54983 Treant (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0 | role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001126, 54983, 2927, 1959, -1118.1913, -3532.636, 52.2071, 0.0, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 54983 Treant (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0 | role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001127, 54983, 2927, 1959, -1119.9255, -3538.3794, 51.7016, 0.0, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 54983 Treant (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0 | role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001128, 54983, 2927, 1959, -1470.2533, -2999.8691, 14.8827, 0.0, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 54983 Treant (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0 | role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001129, 54983, 2927, 1959, -1233.8911, -1760.9094, 61.9361, 0.0, 120, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ========================================================================================
-- PHASE 28 -- Boulderfist siege creatures + Worn Catapult prop (per-quest, gated to siege window 90893/90895)
-- ========================================================================================
-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001130, 142693, 2927, 28, -1283.4098, -1928.6997, 20.3317, 1.28, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001131, 142693, 2927, 28, -1280.7916, -1925.0173, 20.2801, 3.5923, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001132, 142693, 2927, 28, -1232.5955, -1999.0729, 20.4143, 5.3877, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001133, 142693, 2927, 28, -1283.066, -2039.4548, 28.723, 1.2851, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001134, 142693, 2927, 28, -1358.5452, -1955.2153, 3.4204, 6.2253, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001135, 142693, 2927, 28, -1312.401, -1924.8767, 20.2801, 4.3213, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001136, 142693, 2927, 28, -1289.0677, -1961.2101, 10.4515, 4.677, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142693 Boulderfist Ogre (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001137, 142693, 2927, 28, -1291.2118, -1965.7084, 10.9431, 0.7061, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142694 Boulderfist Enforcer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001138, 142694, 2927, 28, -1246.8212, -2040.5834, 28.407, 1.9168, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142694 Boulderfist Enforcer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001139, 142694, 2927, 28, -1222.4341, -1949.9445, 21.5277, 4.2403, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142694 Boulderfist Enforcer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001140, 142694, 2927, 28, -1300.9393, -1924.5623, 20.1968, 1.7912, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142694 Boulderfist Enforcer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001141, 142694, 2927, 28, -1300.8785, -1992.4392, 20.896, 5.7856, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142694 Boulderfist Enforcer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001142, 142694, 2927, 28, -1246.6823, -1969.7848, 18.0539, 4.6428, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142694 Boulderfist Enforcer (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001143, 142694, 2927, 28, -1256.4791, -1893.4497, 15.4443, 5.5134, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249281 Worn Catapult (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001144, 249281, 2927, 28, -1340.0555, -1949.2986, 55.3844, 2.5154, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249281 Worn Catapult (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001145, 249281, 2927, 28, -1334.3942, -1840.316, 62.6863, 2.9272, 60, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ========================================================================================
-- PHASE 3 -- Post-siege PEACE wildlife -- appears after Ro'grok dies (completion phase)
-- ========================================================================================
-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001146, 142333, 2927, 3, -1520.9431, -2776.8652, 42.6871, 4.382, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001147, 142333, 2927, 3, -1702.6444, -2796.8108, 51.087, 5.4619, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001148, 142333, 2927, 3, -1777.6512, -2948.6492, 43.308, 4.0002, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001149, 142333, 2927, 3, -1449.6646, -2798.6995, 52.8478, 1.2365, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001150, 142333, 2927, 3, -1767.0554, -3019.948, 37.9586, 4.773, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001151, 142333, 2927, 3, -1485.517, -2783.6929, 47.5741, 4.1507, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001152, 142333, 2927, 3, -1572.9003, -2748.543, 40.8153, 2.8947, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142333 Giant Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001153, 142333, 2927, 3, -1692.1506, -3039.7246, 28.7013, 4.5001, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001154, 142334, 2927, 3, -850.0668, -2232.6023, 45.2379, 3.208, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001155, 142334, 2927, 3, -916.54, -1948.656, 49.464, 4.3911, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001156, 142334, 2927, 3, -715.8772, -1949.6888, 42.4463, 1.5239, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001157, 142334, 2927, 3, -1540.3953, -2427.749, 73.102, 3.3904, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001158, 142334, 2927, 3, -1486.4691, -3313.1899, 70.0394, 2.4739, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001159, 142334, 2927, 3, -1621.2272, -2376.9348, 90.9831, 4.9773, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001160, 142334, 2927, 3, -1110.4974, -2030.6898, 64.5226, 2.2493, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001161, 142334, 2927, 3, -895.4327, -2215.4465, 50.6631, 1.895, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001162, 142334, 2927, 3, -1073.2665, -2088.3013, 62.1759, 0.7655, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001163, 142334, 2927, 3, -1598.3259, -2412.4766, 91.759, 3.0987, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001164, 142334, 2927, 3, -712.2202, -1974.1204, 42.8418, 0.9986, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001165, 142334, 2927, 3, -1681.3273, -2722.5178, 52.9849, 2.8668, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001166, 142334, 2927, 3, -943.0522, -1891.6757, 65.4453, 0.5257, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001167, 142334, 2927, 3, -706.5939, -2214.4922, 69.8036, 2.3106, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001168, 142334, 2927, 3, -775.9122, -2246.8389, 54.6654, 4.9896, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001169, 142334, 2927, 3, -1125.0, -2055.7285, 60.7691, 2.015, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001170, 142334, 2927, 3, -1510.6559, -2350.8779, 63.8357, 4.2706, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001171, 142334, 2927, 3, -752.467, -2181.509, 63.2555, 2.2483, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001172, 142334, 2927, 3, -876.6328, -1935.4583, 49.3373, 0.8704, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001173, 142334, 2927, 3, -1502.9374, -2320.9707, 54.7483, 1.8864, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001174, 142334, 2927, 3, -1628.6292, -2328.9395, 71.8736, 5.6225, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001175, 142334, 2927, 3, -667.6733, -1981.705, 55.6852, 4.9673, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001176, 142334, 2927, 3, -815.4715, -2208.6648, 54.963, 5.15, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001177, 142334, 2927, 3, -717.6382, -2244.6873, 73.2543, 2.7339, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001178, 142334, 2927, 3, -1563.5764, -2394.3237, 85.7381, 2.953, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001179, 142334, 2927, 3, -1671.8539, -2636.0034, 63.3124, 4.3696, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001180, 142334, 2927, 3, -1147.3589, -2019.6719, 61.7353, 1.9274, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001181, 142334, 2927, 3, -1597.4988, -2356.3689, 88.6901, 4.0669, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001182, 142334, 2927, 3, -1253.3427, -3353.4595, 35.98, 3.9242, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001183, 142334, 2927, 3, -984.9864, -3316.7341, 72.2752, 0.5907, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001184, 142334, 2927, 3, -1334.7703, -2378.6267, 65.5722, 4.5929, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001185, 142334, 2927, 3, -1276.4482, -2348.5056, 64.1991, 3.0201, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001186, 142334, 2927, 3, -950.0519, -1976.181, 50.8939, 1.5258, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001187, 142334, 2927, 3, -1186.4124, -1999.689, 69.4725, 2.1967, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001188, 142334, 2927, 3, -1357.1962, -2602.9441, 74.8738, 2.129, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001189, 142334, 2927, 3, -1501.7358, -2241.6191, 29.4788, 4.644, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001190, 142334, 2927, 3, -1251.1451, -3487.2061, 47.6483, 1.5591, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001191, 142334, 2927, 3, -1200.0294, -3199.6035, 43.4951, 3.9103, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001192, 142334, 2927, 3, -1278.9469, -1940.4607, 73.3658, 2.4508, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001193, 142334, 2927, 3, -935.2621, -1892.5455, 66.963, 3.472, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001194, 142334, 2927, 3, -1051.7782, -3380.2349, 57.4924, 5.959, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001195, 142334, 2927, 3, -1069.7405, -2084.5398, 60.7256, 0.3366, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001196, 142334, 2927, 3, -1142.5735, -2031.3014, 61.8548, 5.0261, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001197, 142334, 2927, 3, -1124.1306, -2051.3765, 60.4939, 3.5801, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001198, 142334, 2927, 3, -918.0269, -1950.3792, 49.464, 4.4718, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001199, 142334, 2927, 3, -1108.746, -2034.5796, 64.4957, 5.0154, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001200, 142334, 2927, 3, -1384.9119, -2321.5796, 65.7988, 0.3928, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001201, 142334, 2927, 3, -1295.563, -3397.3938, 42.65, 0.7288, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001202, 142334, 2927, 3, -1246.2665, -2007.9844, 68.6018, 6.1103, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001203, 142334, 2927, 3, -1246.223, -3411.7615, 42.188, 1.8845, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001204, 142334, 2927, 3, -1134.181, -1966.7714, 71.8143, 1.9618, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001205, 142334, 2927, 3, -1500.3719, -2349.9573, 65.6649, 6.2197, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001206, 142334, 2927, 3, -1249.0062, -3547.1819, 52.3162, 1.0126, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001207, 142334, 2927, 3, -869.5316, -1943.9698, 49.9744, 0.9871, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001208, 142334, 2927, 3, -1499.9386, -2333.2949, 58.3188, 3.9781, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 Plains Creeper (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001209, 142334, 2927, 3, -1281.2419, -3454.6931, 47.2876, 6.2417, 300, 8, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142335 Young Mesa Buzzard (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001210, 142335, 2927, 3, -1544.0166, -2248.7683, 46.6621, 5.12, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142335 Young Mesa Buzzard (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001211, 142335, 2927, 3, -1290.3623, -2340.6938, 71.447, 1.2833, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142341 Elder Mesa Buzzard (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001212, 142341, 2927, 3, -1788.168, -1743.78, 66.5403, 5.4266, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142341 Elder Mesa Buzzard (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001213, 142341, 2927, 3, -1660.6779, -1489.452, 59.6568, 5.6736, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142341 Elder Mesa Buzzard (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001214, 142341, 2927, 3, -1688.012, -1999.9595, 66.7841, 6.1719, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142342 Vicious Black Bear (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001215, 142342, 2927, 3, -1265.3796, -1553.3324, 48.0414, 4.3616, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142342 Vicious Black Bear (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001216, 142342, 2927, 3, -1303.2928, -1602.1223, 51.4265, 0.6607, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142347 Wild Horse (src=objupdate)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001217, 142347, 2927, 3, -1503.899, -1571.8815, 42.4699, 1.0923, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 223453 Arcane Phoenix -- REMOVED (tester-confirmed capture artifact, 2026-08-21).
-- This was mined from a movement-source row right next to the arrival pad (-1046,-3553); it is a
-- transient creature (a summoned/passing Arcane Phoenix, not a real RPE ambient spawn) that the
-- miner picked up. Not part of the experience -- deleted rather than authored. The scoped DELETE
-- also removes guid 8001218 from any DB where a prior apply already inserted it.
DELETE FROM `creature` WHERE `guid` = 8001218;

-- ============================================================================
-- HORDE-XVAL FIX (H1) Task 2 -- Wildlife population the Alliance capture couldn't see.
--   Source: creature_spawns_objupdate.zone_2927.sql (Horde bundle). Each entry's raw
--   Horde observation set was greedily clustered at a 20yd threshold (centroid + max
--   scatter-from-centroid -> wander_distance, clamped to the brief's 5-10yd range,
--   MovementType=1 per Task 3) to avoid spawning one creature per wire snapshot. Entries
--   with >15 clusters were evenly downsampled to 15 (all coordinates are real Horde
--   points; downsampling is noted per-entry below and in the report -- see Concerns).
-- ============================================================================

-- entry 883 Buzzard (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001219, 883, 2927, 1959, -1151.9753, -2034.1321, 64.6703, 3.7527, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 883 Buzzard (Horde-xval ADD, wildlife, n=2 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001220, 883, 2927, 1959, -986.8305, -2064.8035, 54.3704, 4.5556, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 883 Buzzard (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001221, 883, 2927, 1959, -1133.334, -2179.166, 58.4044, 0.7898, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 883 Buzzard (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001222, 883, 2927, 1959, -1000.6963, -2203.8872, 46.2437, 1.314, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 883 Buzzard (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001223, 883, 2927, 1959, -1211.2281, -1863.0592, 91.9508, 3.7502, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001224, 2620, 2927, 1959, -1600.7233, -2170.8386, 31.0871, 4.6635, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001225, 2620, 2927, 1959, -1991.9447, -2633.9167, 81.1611, 4.1004, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=4 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001226, 2620, 2927, 1959, -1079.3505, -2314.4638, 49.327, 0.5186, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001227, 2620, 2927, 1959, -683.334, -2066.666, 73.1871, 3.2605, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001228, 2620, 2927, 1959, -1119.356, -2333.126, 57.9923, 5.1318, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001229, 2620, 2927, 1959, -1600.6571, -2075.4517, 39.3231, 3.5139, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001230, 2620, 2927, 1959, -747.895, -1948.8365, 43.3884, 3.6455, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001231, 2620, 2927, 1959, -1415.2852, -2067.6406, 41.9556, 4.6311, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001232, 2620, 2927, 1959, -1211.8218, -1695.7771, 53.662, 1.8076, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001233, 2620, 2927, 1959, -954.8913, -2411.4412, 49.2302, 0.6357, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001234, 2620, 2927, 1959, -1272.3297, -1796.1613, 67.2747, 2.5416, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001235, 2620, 2927, 1959, -1456.5549, -2073.615, 23.7212, 5.7906, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001236, 2620, 2927, 1959, -1341.978, -1674.5333, 54.3614, 2.3301, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001237, 2620, 2927, 1959, -1533.004, -1619.4745, 67.4034, 5.3286, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 2620 Vulture (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 19 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001238, 2620, 2927, 1959, -1050.4669, -2255.5906, 27.2456, 1.6666, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001239, 142337, 2927, 1959, -1768.7631, -2972.7124, 47.8485, 0.4488, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001240, 142337, 2927, 1959, -1598.2699, -2946.7358, 33.4076, 3.9961, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001241, 142337, 2927, 1959, -1781.2875, -2606.7219, 56.596, 0.5393, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001242, 142337, 2927, 1959, -1749.2917, -2199.9517, 49.3391, 0.7526, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001243, 142337, 2927, 1959, -1632.3273, -2102.0005, 37.0628, 1.2998, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001244, 142337, 2927, 1959, -1642.5302, -2863.218, 32.2879, 3.4863, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001245, 142337, 2927, 1959, -1634.7731, -2149.9822, 30.4229, 2.0206, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142337 Grazing Doe (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001246, 142337, 2927, 1959, -1181.7783, -2412.708, 63.1825, 1.7553, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001247, 142338, 2927, 1959, -1257.7418, -2785.1824, 53.3798, 5.176, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001248, 142338, 2927, 1959, -1136.5038, -3006.2153, 41.7862, 1.6981, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001249, 142338, 2927, 1959, -1219.646, -3074.1379, 43.0335, 5.1183, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001250, 142338, 2927, 1959, -1305.3167, -2352.6392, 64.5256, 0.3718, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=2 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001251, 142338, 2927, 1959, -1182.4974, -2776.866, 51.3562, 4.5694, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001252, 142338, 2927, 1959, -1251.3292, -2380.2095, 61.6388, 1.6137, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001253, 142338, 2927, 1959, -1299.6226, -2870.248, 59.4688, 3.3302, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=2 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001254, 142338, 2927, 1959, -1122.2902, -2642.1311, 46.4684, 2.0832, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001255, 142338, 2927, 1959, -992.9727, -3393.3142, 59.7661, 3.8897, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001256, 142338, 2927, 1959, -1347.6777, -2351.918, 68.2043, 6.0932, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=2 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001257, 142338, 2927, 1959, -1126.6143, -2976.5089, 42.6925, 5.5434, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001258, 142338, 2927, 1959, -998.6982, -3031.6724, 55.5434, 1.6137, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001259, 142338, 2927, 1959, -1053.9897, -3316.3838, 57.6129, 0.9207, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001260, 142338, 2927, 1959, -1050.3741, -3233.6604, 48.8435, 4.2487, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142338 Grazing Buck (Horde-xval ADD, wildlife, n=1 raw pts merged (15 of 52 clusters kept, evenly downsampled))
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001261, 142338, 2927, 1959, -1017.9568, -3269.1433, 64.5446, 1.1028, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001262, 142339, 2927, 1959, -1640.2028, -2172.3308, 31.1574, 3.8496, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001263, 142339, 2927, 1959, -1783.798, -2882.4722, 42.0298, 4.3616, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=2 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001264, 142339, 2927, 1959, -1606.1157, -2075.9304, 40.0322, 3.5362, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001265, 142339, 2927, 1959, -1700.9471, -3097.6685, 29.5892, 2.44, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=2 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001266, 142339, 2927, 1959, -1651.5393, -2113.2937, 36.4567, 3.0771, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001267, 142339, 2927, 1959, -1583.6484, -2810.8223, 37.536, 1.5825, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142339 Boar (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001268, 142339, 2927, 1959, -1518.692, -2815.0762, 35.6593, 4.1146, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=2 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001269, 142340, 2927, 1959, -2002.0725, -2537.9885, 72.9987, 3.0791, 120, 6, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001270, 142340, 2927, 1959, -1830.2816, -2669.7986, 55.7674, 2.3424, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001271, 142340, 2927, 1959, -1976.6581, -2517.4238, 73.1937, 0.5073, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001272, 142340, 2927, 1959, -1947.7117, -2544.9116, 72.7807, 2.1593, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001273, 142340, 2927, 1959, -1921.2832, -2600.429, 73.017, 6.2343, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001274, 142340, 2927, 1959, -1756.8787, -2816.5625, 49.2018, 0.0, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001275, 142340, 2927, 1959, -1759.2261, -2734.6936, 51.4604, 0.0703, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001276, 142340, 2927, 1959, -2084.812, -2555.3667, 73.7086, 4.6086, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001277, 142340, 2927, 1959, -1805.7848, -2721.3962, 48.5215, 1.0217, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=2 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001278, 142340, 2927, 1959, -2074.1661, -2598.1229, 79.8928, 2.8705, 120, 9, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001279, 142340, 2927, 1959, -2033.2167, -2524.6189, 71.4395, 5.7674, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001280, 142340, 2927, 1959, -1777.6205, -2655.2852, 56.5654, 5.466, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142340 Mountain Goat (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001281, 142340, 2927, 1959, -1777.6859, -2778.0715, 52.4305, 5.211, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142343 Songbird (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001282, 142343, 2927, 1959, -646.2847, -1816.7795, 56.2798, 3.8241, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142343 Songbird (Horde-xval ADD, wildlife, n=1 raw pts merged)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
 (8001283, 142343, 2927, 1959, -1196.2404, -1691.0923, 47.9538, 5.0279, 120, 5, 1, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `wander_distance`=VALUES(`wander_distance`), `MovementType`=VALUES(`MovementType`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ============================================================================
-- HORDE-XVAL FIX (H1) Task 2 -- NW exile/elemental camp (Horde-xval ADD). Static
--   (not in Task 3's wildlife/wander list). Role/phase uncertain -- Phase-K refine,
--   same convention as the existing 141659/141725/141727 cluster above. Multi-point
--   Horde entries kept as multiple real spawns (not collapsed).
-- ============================================================================

-- entry 114590 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001284, 114590, 2927, 1959, -1950.382, -2796.9792, 83.9736, 0.0, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 114590 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001285, 114590, 2927, 1959, -1948.0642, -2781.2449, 82.0217, 0.0, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141663 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001286, 141663, 2927, 1959, -1201.6788, -2191.2952, 57.9251, 3.0858, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141946 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001287, 141946, 2927, 1959, -1970.4948, -2758.2136, 80.8837, 4.4916, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141946 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001288, 141946, 2927, 1959, -1901.0841, -2793.6667, 72.6796, 5.1124, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141947 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001289, 141947, 2927, 1959, -1914.4236, -2788.1025, 71.2149, 1.5373, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 141947 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001290, 141947, 2927, 1959, -1972.6788, -2762.1338, 80.6719, 0.6635, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142021 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001291, 142021, 2927, 1959, -1985.3351, -2782.1321, 82.5859, 6.2541, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142021 (Horde-xval ADD, NW exile/elemental camp) [role/phase uncertain -- Phase-K refine]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001292, 142021, 2927, 1959, -1860.632, -2848.5903, 62.9088, 1.8482, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- ============================================================================
-- HORDE-XVAL FIX (H1) Task 2 -- Farm/misc (Horde-xval ADD). Static.
-- ============================================================================

-- entry 142566 (Horde-xval ADD, farm/misc)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001293, 142566, 2927, 1959, -1150.728, -3610.7766, 42.8744, 1.0669, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 253463 (Horde-xval ADD, farm/misc)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001294, 253463, 2927, 1959, -1560.5243, -2879.3264, 16.8375, 4.6395, 120, 69404)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);



-- >>>>>>>>>>>>>>>>>>>>  SOURCE: reconciled creature_addon (staging-reconciled-creature_addon.sql)  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- STAGING / RECONCILED creature_addon -- Arathi Catch-Up (map 2927)
-- ============================================================================
-- CANDIDATE ONLY -- STAGING. Do NOT apply to any branch/realm. Human review required.
-- Re-keyed from Set B file 2026_08_15_56_world_arathi_rpe.sql (per-spawn StandState poses).
-- Set B keyed these to its own 11002000-block guids; here each is matched by entry+position
-- to the surviving reconciled Set A spawn and RE-KEYED to the 8000000-block guid.
-- Column layout matches Set B file 56.
--
-- MAPPING (all three matched a Set A spawn at < 0.1 yd -- exact):
--   11002031 245026 Win'sa @(-1089.58,-3545.22) KNEEL(8) -> Set A guid 8000002
--   11002032 245028 Horde Grunt @(-1088.34,-3542.44) SIT(1) -> Set A guid 8001103
--   11002033 245028 Horde Grunt @(-1083.78,-3553.58) DEAD(7) -> Set A guid 8001102
--
-- These are PER-SPAWN (each guid a different StandState: KNEEL / SIT / DEAD), so they MUST
-- stay in creature_addon (per-guid) and CANNOT collapse into creature_template_addon.
--
-- NOT re-keyed here (belong in creature_template_addon, generic per-entry -- FLAGGED, see report):
--   Set B 245027 feign-death (aura 29266) and 249245 float (AnimTier 3) are GENERIC to every
--   spawn of the entry -> creature_template_addon, not per-spawn. Content already owns a
--   10b_creature_template_addon slice; verify/author 245027+249245 there, do NOT duplicate here.
-- ============================================================================

DELETE FROM `creature_addon` WHERE `guid` IN (8000002, 8001102, 8001103);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(8000002, 0, 0, 8, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL), -- 245026 Win'sa (re-key of 11002031) - KNEEL
(8001103, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL), -- 245028 Horde Grunt (re-key of 11002032) - SIT
(8001102, 0, 0, 7, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL); -- 245028 Horde Grunt (re-key of 11002033) - DEAD


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 21_phase_area.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D phase_area map (FIX ROUND 2)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED -- was wrongly 2796; client uiMapID 2451 is display-only).
-- Depends on: 20_creature_spawns.sql (assigns/uses these REAL PhaseIds on its 144 spawn
--   rows), 22_conditions_phasing.sql (the quest-state gates that actually flip
--   PhasingHandler::OnConditionChange), phase_shift.sql (the wire-derived source graph,
--   VerifiedBuild=69382, in C:/dumps/tcharvest/out/catchup_zone_REMAP/zone_2927/).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (AreaId,PhaseId)).
-- ============================================================================
--
-- **HORDE CROSS-VALIDATION CAVEAT (2026-08-20):** a full Horde capture (build 69404) was
-- run through the --phaseshift decoder. Its personal phase ids ({1,13,65,1951}) are almost
-- ENTIRELY DISJOINT from these Alliance ids ({1959,1961,1610,28,37,...}); even the coincidental
-- overlaps (1965, 4) carry DIFFERENT quest windows. => the numeric personal-phase ids are
-- SESSION/INSTANCE-ALLOCATED, NOT portable content constants. ONLY PhaseId 3 (slot-25 completion,
-- whole-span) cross-validates as canonical on BOTH factions. The real spine is the QUEST-STATE
-- CONDITIONS in 22_conditions_phasing.sql + server-assigned personal phases (instance/C++ per
-- PhasingHandler) -- these numeric PhaseIds are ILLUSTRATIVE of the phase-graph SHAPE only. Do NOT
-- port them to a Horde build or treat them as stable; Phase K: derive real ids per-realm at deploy.
-- ---- THE REAL PhaseId MAP (Fix Round 2 -- replaces the FABRICATED 15901-15905 block) ----
-- Full phase -> quest-step correlation graph, copied verbatim from phase_shift.sql's own
-- header (18 real PhaseIds observed on the wire for map 2927; only 9 are relevant to this
-- content slice -- the rest (5,7,23,25,33,38,61,1251,1955) had "no quest window overlap"
-- in the graph and are NOT used anywhere in this feature, not authored below):
--   PhaseId=3    slot(s)=[11,25] quests=90883,90885,90886,90887,90888,90893,90895,90896
--   PhaseId=4    slot(s)=[11]    quests=90885,90886,90887
--   PhaseId=8    slot(s)=[11]    quests=90885,90886,90887
--   PhaseId=28   slot(s)=[11]    quests=90893,90895
--   PhaseId=37   slot(s)=[11]    quests=90883
--   PhaseId=1610 slot(s)=[0]     quests=90893,90895
--   PhaseId=1959 slot(s)=[0]     quests=90885,90886,90887,90888,90893,90895,90896
--   PhaseId=1961 slot(s)=[0]     quests=90883
--   PhaseId=1965 slot(s)=[0]     quests=90885,90886,90887,90893,90895
--
-- Of these 9, 20_creature_spawns.sql actively attaches 7 to a creature spawn (1961, 37,
-- 1959, 4, 1610, 28, 3). The remaining 2 (1965, 8) are documented here for COMPLETENESS
-- ONLY -- they are real, wire-confirmed phase ids whose quest windows are near-duplicates
-- of 1959's and 4's respectively (1965 = farm+siege minus the 88/96 boundary quests; 8 =
-- an exact duplicate of 4's window), and this task picked a single representative id per
-- cluster half rather than double-authoring near-identical phase_area/() rows for both
-- (see 20_creature_spawns.sql's banner "HONEST BOUNDARY" note). A future GO/spawn task
-- that needs the OTHER id of a duplicate pair can reuse the AreaId rows below unchanged.
--
-- ---- AreaId source -- unchanged reasoning from Fix Round 1 (Plan Part 1.7 area_poi.sql,
-- VerifiedBuild 69299) ----
-- -- TODO Phase K: confirm exact RPE AreaId on map 2927 (phase_shift.sql itself leaves
-- AreaId NEEDS-REVIEW -- "Personal phase ids are NOT area-keyed on the wire", so every
-- INSERT in that source file ships commented out; a reviewer supplies AreaId). The values
-- below are the live-Arathi-Highlands-terrain AreaIds from the captured POI list
-- (Hammerfall 7658, Refuge Pointe 7678, Stromgarde Keep 7667, Go'shek/Dabyrie's Farmstead
-- 7680, Boulderfist Outpost/Hall 7682) -- ASSUMED reusable because map 2927's RPE instance
-- is a personal-phased copy of the same Arathi Highlands terrain (same WDT/ADT ->
-- same AreaTable.db2 rows), not a distinct terrain build. If a future re-capture shows map
-- 2927 has its own distinct AreaTable rows, every row below needs updating.
--
-- ---- Fix Round 1's fabricated "Phase 5 peace/terminal" bucket is DROPPED entirely ----
-- The old (WRONG) file mapped invented PhaseId 15905 to a "peace, TERMINAL" state covering
-- every AreaId. No real PhaseId in the wire graph plausibly represents a post-90896 "peace"
-- state (the graph's widest phase, 3, spans the WHOLE 90883-90896 run, not just the tail).
-- Separately, the only two entries the old Phase-5 block ever spawned (883 Deer, 142334
-- wildlife burst) are NOT in this task's 46-entry creature_template roster (10_creature_
-- template.sql did not author templates for them -- confirmed in 20_creature_spawns.sql's
-- Fix-Round-1 predecessor banner), so there is nothing left in this content slice's scope
-- that would use a "peace" phase even if one existed on the wire. Not carried forward.
-- ============================================================================

-- PhaseId 1961 (terrain, slot 0) -- Hammerfall/town base, active during quest 90883's window
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7658, 1961, 'Catch-Up Experience -- Hammerfall -- REAL PhaseId 1961 (terrain, quest 90883 window) -- TODO Phase K confirm AreaId on map 2927'),
 (7678, 1961, 'Catch-Up Experience -- Refuge Pointe corridor -- REAL PhaseId 1961 span -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 37 (per-quest, slot 11) -- Hammerfall gnoll-camp filler, gated to quest 90883
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7658, 37, 'Catch-Up Experience -- Hammerfall gnoll camp -- REAL PhaseId 37 (per-quest, quest 90883) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 1959 (terrain, slot 0) -- Go'shek farm story-lead/prop base, broadest terrain
-- window (90885/86/87/88/93/95/96)
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7680, 1959, 'Catch-Up Experience -- Go''shek/Dabyrie''s Farmstead -- REAL PhaseId 1959 (terrain, quests 90885-90896 span) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 4 (per-quest, slot 11) -- Go'shek farm trash + Runk, gated to 90885/86/87
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7680, 4, 'Catch-Up Experience -- Go''shek/Dabyrie''s Farmstead -- REAL PhaseId 4 (per-quest, quests 90885/86/87) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 8 (per-quest, slot 11) -- documented for completeness only, NOT attached to any
-- spawn in this file; near-duplicate of PhaseId 4's window (90885/86/87). Same AreaId as 4.
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7680, 8, 'Catch-Up Experience -- Go''shek/Dabyrie''s Farmstead -- REAL PhaseId 8 (per-quest, quests 90885/86/87; near-duplicate of PhaseId 4, NOT used by any 20_creature_spawns.sql row -- documented for completeness) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 1965 (terrain, slot 0) -- documented for completeness only, NOT attached to any
-- spawn in this file; near-duplicate of PhaseId 1959's window minus the 88/96 boundary
-- quests (90885/86/87/93/95). Farm+siege AreaIds both plausible; listed under farm.
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7680, 1965, 'Catch-Up Experience -- Go''shek/Dabyrie''s Farmstead -- REAL PhaseId 1965 (terrain, quests 90885/86/87/93/95; near-duplicate of PhaseId 1959, NOT used by any 20_creature_spawns.sql row -- documented for completeness) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 1610 (terrain, slot 0) -- Stromgarde Keep hub leads/town NPCs base, siege window
-- (90893/95)
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7667, 1610, 'Catch-Up Experience -- Stromgarde Keep -- REAL PhaseId 1610 (terrain, quests 90893/95) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 28 (per-quest, slot 11) -- Stromgarde siege trash + Worn Catapult, gated to
-- 90893/95
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7667, 28, 'Catch-Up Experience -- Stromgarde Keep siege battlefield -- REAL PhaseId 28 (per-quest, quests 90893/95) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 3 (per-quest+"completion", slots 11 and 25) -- used here ONLY for the narrow
-- climax/Ro'grok cluster at Boulderfist Outpost/Hall, even though the graph shows this id's
-- full window spans the ENTIRE 90883-90896 run (the broadest phase on the wire) -- see
-- 20_creature_spawns.sql's banner "HONEST BOUNDARY" note for why only the climax cluster
-- uses it in this file.
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7682, 3, 'Catch-Up Experience -- Boulderfist Outpost/Hall (Ro''grok''s lair) -- REAL PhaseId 3 (per-quest/completion, full 90883-90896 window on the wire; authored here for the climax cluster only) -- TODO Phase K confirm AreaId on map 2927')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- ============================================================================
-- END -- 10 phase_area rows (9 INSERT statements; PhaseId 1961 covers 2 AreaIds); all 9
-- REAL PhaseIds from the wire graph that are relevant to this feature are covered. 7 of the
-- 9 are actively attached to a 20_creature_spawns.sql row (1961, 37, 1959, 4, 1610, 28, 3);
-- 2 (1965, 8) are documented for completeness only per the near-duplicate-window note above.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: PRESERVED 50-57 phase_name (files 50/51)  <<<<<<<<<<<<<<<<<<<<

-- ---------------------------------------------------------------------------
-- PRESERVED from 2026_08_15_50 / _51 (the only unique bits kept from Set B):
--   phase_name labels for the RPE login/farm phases. Re-asserted here so the
--   consolidated set is self-contained. These are inert name labels; the phase
--   SPINE is content's 22_conditions_phasing (real wire PhaseIds), not the 26xxx ids.
-- ---------------------------------------------------------------------------
-- from 2026_08_15_50:
DELETE FROM `phase_name` WHERE `ID` IN (26596, 26618, 27217);
INSERT INTO `phase_name` (`ID`, `Name`) VALUES
(26596, 'Arathi RPE - Hammerfall login'),
(26618, 'Arathi RPE - Hammerfall login'),
(27217, 'Arathi RPE - persistent through farm');

-- from 2026_08_15_51:
DELETE FROM `phase_name` WHERE `ID` IN (26588, 26599);
INSERT INTO `phase_name` (`ID`, `Name`) VALUES
(26588, 'Arathi RPE - Go''shek Farm arrive'),
(26599, 'Arathi RPE - Go''shek Farm persistent');


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 22_conditions_phasing.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D phasing conditions (FIX ROUND 2)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED -- was wrongly 2796; client uiMapID 2451 is display-only).
-- Depends on: 20_creature_spawns.sql (uses these REAL PhaseIds), 21_phase_area.sql
--   (AreaId->PhaseId map these conditions attach to via ConditionMgr::addToPhases --
--   SourceEntry=0 applies a condition to every AreaId already mapped to that PhaseId in
--   `phase_area`, so one row per PhaseId is enough), phase_shift.sql (the wire-derived
--   phase->quest graph, VerifiedBuild=69382).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is the full
--   SourceTypeOrReferenceId/SourceGroup/SourceEntry/SourceId/ElseGroup/
--   ConditionTypeOrReference/ConditionTarget/ConditionValue1/2/3/ConditionStringValue1
--   tuple, per `conditions`.CREATE TABLE in sql/base/dev/world_database.sql).
-- ============================================================================
--
-- **HORDE CROSS-VALIDATION CAVEAT (2026-08-20):** a full Horde capture (build 69404) was
-- run through the --phaseshift decoder. Its personal phase ids ({1,13,65,1951}) are almost
-- ENTIRELY DISJOINT from these Alliance ids ({1959,1961,1610,28,37,...}); even the coincidental
-- overlaps (1965, 4) carry DIFFERENT quest windows. => the numeric personal-phase ids are
-- SESSION/INSTANCE-ALLOCATED, NOT portable content constants. ONLY PhaseId 3 (slot-25 completion,
-- whole-span) cross-validates as canonical on BOTH factions. The real spine is the QUEST-STATE
-- CONDITIONS in 22_conditions_phasing.sql + server-assigned personal phases (instance/C++ per
-- PhasingHandler) -- these numeric PhaseIds are ILLUSTRATIVE of the phase-graph SHAPE only. Do NOT
-- port them to a Horde build or treat them as stable; Phase K: derive real ids per-realm at deploy.
-- ---- THE REAL PhaseId MAP (Fix Round 2 -- replaces the FABRICATED 15901-15905 block) ----
-- See 20_creature_spawns.sql / 21_phase_area.sql banners for the full graph. This file only
-- gates the 7 PhaseIds actually attached to a creature spawn (1961, 37, 1959, 4, 1610, 28,
-- 3). PhaseIds 8 and 1965 are documented in 21_phase_area.sql for completeness but are NOT
-- gated here -- they carry no creature spawn in this feature, so there is nothing for a
-- condition to turn on or off; a future task that spawns something against them should add
-- their conditions at that time (they'd mirror 4's and 1959's rows below respectively,
-- since the graph shows near-identical quest windows).
--
-- Enum values re-verified against I:/TrinityCore/mythic-plus/TrinityCore/src/server/game/
-- Conditions/ConditionMgr.h for this fix (unchanged from Fix Round 1):
--   CONDITION_SOURCE_TYPE_PHASE          = 26  (SourceGroup=PhaseId, SourceEntry=AreaId;
--                                                SourceEntry=0 -> "every area mapped to
--                                                this PhaseId in phase_area", see
--                                                ConditionMgr::addToPhases)
--   CONDITION_QUESTREWARDED (value 1)    =  8  (ConditionValue1=quest_id; true once the
--                                                quest has been turned in/rewarded)
--   CONDITION_QUESTTAKEN    (value 1)    =  9  (ConditionValue1=quest_id; true while the
--                                                quest is active/in the quest log)
--
-- ---- HONEST BOUNDARY on the quest-state gate chosen per PhaseId ----
-- phase_shift.sql's graph gives each PhaseId a SET of quests it was observed active during,
-- not a single canonical "on" trigger and "off" trigger -- that mapping to
-- QUESTTAKEN/QUESTREWARDED conditions is INFERRED here, same honesty caveat as
-- 20_creature_spawns.sql's per-spawn PhaseId assignment:
--   PhaseId 1961/37 (quests=[90883]) -> gated on QUESTTAKEN(90883). No wire evidence exists
--     for what precedes 90883 (the graph has no data for quest 90882, which the brief's
--     Requirement 2 cites as also relevant to this cluster) -- TODO Phase K: confirm whether
--     the arrival cluster should ALSO be visible while only 90882 is active/not yet 90883.
--   PhaseId 1959 (quests=[90885,86,87,88,93,95,96], the whole farm-through-climax span) ->
--     gated on QUESTREWARDED(90883) alone (the "entry" event into that whole span) rather
--     than reproducing the full 7-quest OR-chain -- matches Fix Round 1's positive-gate
--     design intent for this cluster, now on a real PhaseId.
--   PhaseId 4 (quests=[90885,86,87], farm-only) -> gated on QUESTREWARDED(90883) AND NOT
--     QUESTREWARDED(90888) -- the farm-trash window opens once 90883 is turned in and closes
--     once the siege quest 90888 is turned in (i.e. progression has moved past the farm).
--   PhaseId 1610/28 (quests=[90893,95], siege-only) -> gated on QUESTREWARDED(90888) AND NOT
--     QUESTREWARDED(90896) -- the siege window opens once 90888 is turned in and closes once
--     the climax quest 90896 is turned in.
--   PhaseId 3 (quests=[90883,85,86,87,88,93,95,96], the widest phase on the wire, also
--     flagged "completion phase" at slot 25) -> this file gates it on QUESTTAKEN(90896)
--     ONLY, matching how 20_creature_spawns.sql actually uses it (the narrow climax/Ro'grok
--     cluster), NOT the full multi-quest window the wire shows for this id in other
--     contexts. -- TODO Phase K: if a future task spawns something else against PhaseId 3
--     for an earlier quest step, this gate needs to become an OR across the full quest set.
--
-- ---- Fix Round 1's "peace-phase exclusivity" negated-condition system is DROPPED ----
-- The old (WRONG) file added a negated CONDITION_QUESTREWARDED(90896) leg to phases
-- 15901/15902/15903 so they would not linger alongside its invented terminal "peace" phase
-- 15905 once the whole quest chain was rewarded. That system doesn't carry forward as-is:
-- (a) PhaseId 3 (used for the climax cluster) is gated on QUESTTAKEN(90896), which already
--     self-closes once 90896 is REWARDED (a rewarded quest is no longer "taken") -- no
--     negation needed, same self-closing behavior the old file relied on for its 15904.
-- (b) PhaseId 1610/28 (siege) are explicitly gated NOT QUESTREWARDED(90896) above, so they
--     already stop once the climax quest is turned in -- this IS the old exclusivity idea,
--     just expressed as the phase's own positive+negative gate pair instead of a separate
--     bolt-on row.
-- (c) There is no real "peace" PhaseId on the wire and no in-scope creature spawn that would
--     use one (see 21_phase_area.sql's banner) -- so there is nothing left for 1961/37/1959/
--     4 to be made exclusive AGAINST. 1961/37 (arrival) and 1959/4 (farm) are left with only
--     their own positive gates; once the player has moved on to siege/climax, Hammerfall's
--     and the farm's spawns remain phased-in (same behavior a real personal-phasing "leave
--     the old phase behind as you progress" design would need a NOT-rewarded-quest leg for
--     too, but there is no wire evidence to say which quest that should be keyed to for
--     1961/37/1959/4 specifically) -- TODO Phase K: revisit if a re-capture shows these
--     phases actually drop out of the player's phase set once the player has progressed.
-- ============================================================================

-- FIX 2026-08-21 (post-live-test): re-gate the two HAMMERFALL ARRIVAL phases -- 1961 (town base +
-- the arrival questgivers Jaina 244643 / Thrall 244642) and 37 (the 90882 slay-target gnoll camp) --
-- so they are active from ARRIVAL. The wire graph had NO 90882/arrival window (its first correlated
-- quest is 90883), so both were wrongly gated on QUESTTAKEN(90883), which hides the NPCs that GIVE
-- 90882 and the gnolls you must slay for 90882 until after 90883 is taken (chicken-and-egg). They
-- are re-gated to "active until 90883 is REWARDED" (NegativeCondition on QUESTREWARDED 90883), the
-- clean complement of the farm phase 1959 (active once 90883 IS rewarded) -- so Hammerfall is
-- populated through 90882+90883 then hands off to the farm. ConditionType is part of the conditions
-- PK, so the old QUESTTAKEN(9) rows are DELETEd first.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup` IN (1961, 37);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1961, 0, 0, 0, 8, 0, 90883, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- PhaseId 1961 (Hammerfall/town base + arrival leaders 244643/244642) active from ARRIVAL until 90883 REWARDED (NegativeCondition on QUESTREWARDED 90883). FIX 2026-08-21: the wire graph had no 90882/arrival window so this was wrongly gated on QUESTTAKEN(90883), hiding the questgivers that GIVE 90882 -- corrected so the Hammerfall content is visible during 90882+90883 then hands off to the farm phase 1959 (active once 90883 rewarded).')
ON DUPLICATE KEY UPDATE `ConditionTypeOrReference`=VALUES(`ConditionTypeOrReference`), `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 37 (Hammerfall gnoll filler, per-quest) requires quest 90883 TAKEN -- same window
-- as 1961 above (both real PhaseIds correlate to quest 90883 in the graph).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 37, 0, 0, 0, 8, 0, 90883, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- PhaseId 37 (Hammerfall 90882 slay-target gnoll camp) active from ARRIVAL until 90883 REWARDED (NegativeCondition on QUESTREWARDED 90883). FIX 2026-08-21: was wrongly gated on QUESTTAKEN(90883), hiding the 90882 gnolls until after 90883 -- corrected to match 1961.')
ON DUPLICATE KEY UPDATE `ConditionTypeOrReference`=VALUES(`ConditionTypeOrReference`), `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 1959 (Go'shek farm base/leads/prop) requires quest 90883 REWARDED -- the "entry
-- event" into the wire graph's whole 90885-90896 span for this id.
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1959, 0, 0, 0, 8, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1959 (Go''shek farm base) requires quest 90883 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 4 (Go'shek farm trash + Runk, per-quest) requires quest 90883 REWARDED AND quest
-- 90888 NOT YET rewarded (farm-only window per the wire graph: 90885/86/87).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 4, 0, 0, 0, 8, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 4 (Go''shek farm trash+Runk) requires quest 90883 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 4, 0, 0, 0, 8, 0, 90888, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 4 (Go''shek farm trash+Runk) requires quest 90888 NOT rewarded (Fix Round 2, closes the farm window once siege begins)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 1610 (Stromgarde Keep base/leads/town) requires quest 90888 REWARDED AND quest
-- 90896 NOT YET rewarded (siege-only window per the wire graph: 90893/95).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1610, 0, 0, 0, 8, 0, 90888, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1610 (Stromgarde Keep base) requires quest 90888 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1610, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1610 (Stromgarde Keep base) requires quest 90896 NOT rewarded (Fix Round 2, closes the siege window once climax is turned in)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 28 (Stromgarde siege trash + catapult, per-quest) -- same gate as 1610 above (both
-- real PhaseIds correlate to the identical quest window 90893/95 in the graph).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 28, 0, 0, 0, 8, 0, 90888, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 28 (Stromgarde siege trash+catapult) requires quest 90888 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 28, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 28 (Stromgarde siege trash+catapult) requires quest 90896 NOT rewarded (Fix Round 2, closes the siege window once climax is turned in)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 3 (climax/Ro'grok cluster, as actually used in 20_creature_spawns.sql) requires
-- quest 90896 TAKEN. NOTE: the wire graph's own window for id 3 is far broader
-- (90883-90896) -- this gate reflects only THIS file's narrow climax use, see banner.
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 3, 0, 0, 0, 9, 0, 90896, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 3 (climax/Ro''grok cluster use) requires quest 90896 taken (Fix Round 2 -- narrower than this id''s full wire-graph window, see banner)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- ============================================================================
-- END -- 9 condition rows across 7 REAL PhaseIds (1961 x1, 37 x1, 1959 x1, 4 x2, 1610 x2,
-- 28 x2, 3 x1). PhaseIds 8 and 1965 (documented in 21_phase_area.sql for completeness) are
-- intentionally NOT gated here -- see this file's banner. The fabricated 15901-15905
-- exclusivity system from Fix Round 1 is fully retired (see banner for how its intent is
-- now covered, and what is honestly left un-covered).
-- ============================================================================
