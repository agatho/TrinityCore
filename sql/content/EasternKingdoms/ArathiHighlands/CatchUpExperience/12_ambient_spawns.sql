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

-- entry 223453 Arcane Phoenix (src=movement)  [movement-source row lacks facing; orientation defaulted to 0.0]
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8001218, 223453, 2927, 3, -1046.0452, -3553.0237, 55.9417, 0.0, 300, 69382)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

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

