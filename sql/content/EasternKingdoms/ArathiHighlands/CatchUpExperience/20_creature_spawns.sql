-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D creature spawns
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve capture)
-- Sources used: addon_creature_spawns.sql (81 rows total; the map=2451 subset is the
--   IN-SCOPE ~50 -- per CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN.md Global Constraint #4,
--   "the addon capture's coordinates are world coords in map-2796 space", so map=2451
--   rows are re-mapped to `creature`.`map`=2796 here -- the addon dump's own `map`
--   column is a display-uiMap artifact of the client capture, not the server map),
--   despawn_events.txt (transient-vs-permanent classification), Plan Part 1.3
--   (spawn clusters) + 1.6 (phase graph) + 1.7 (POI z references).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent: DELETE by the reserved guid range below, then INSERT ... ON DUPLICATE KEY
--   UPDATE (belt-and-suspenders -- re-apply safe either way).
-- ============================================================================
--
-- ---- RESERVED GUID RANGE (this file owns it; do not reuse elsewhere) ----
-- Base 8000000, block 8000000-8000999 reserved. Actually used sub-ranges:
--   8000000-8000015  Phase 1 (arrival)         16 rows
--   8000100-8000113  Phase 2 (farm)            14 rows
--   8000200-8000219  Phase 3 (siege)           20 rows
--   8000300-8000305  Phase 4 (climax)           6 rows
--   8000400-8000408  Phase 5 (peace, terminal)  9 rows
-- Total 65 rows (brief estimate was "~50"; overshoot is grouping, not bleed -- see
-- report for the breakdown: 4 pumpkin rows for 1 entry, 4 catapult rows for 1 entry,
-- 5 Ogre Basher "wave" rows for 1 entry with no captured coord, and a 9-row wildlife
-- burst for 2 entries neither of which Task 1 authored a template for -- see below).
--
-- ---- THE CANONICAL PhaseId MAP (this task OWNS this map; base 15900) ----
-- These PhaseIds are NEWLY RESERVED -- no phase IDs were present in the capture
-- (Plan gap G2: "Phasing is 100% undecoded"). The real retail phase IDs are a
-- Phase-K item to reconcile against a re-capture's PhaseShift block.
--   PhaseId 15901 -- Phase 1 "arrival/Hammerfall warzone" (DEFAULT on entry; gnolls
--                    hostile). Gates on quest 90882 active / not-90896-complete.
--   PhaseId 15902 -- Phase 2 "Go'shek farm" (on 90883 rewarded; farm mobs + Bruvk
--                    244923 + pumpkins 244956). Quests 90885/86/87.
--   PhaseId 15903 -- Phase 3 "Stromgarde siege" (on 90888 rewarded; siege mobs +
--                    catapults 249269 + siege-actors Jaina 244658/Thrall 244657).
--                    Quests 90893/95.
--   PhaseId 15904 -- Phase 4 "siege climax" (on 90896 taken; Ogre Basher waves;
--                    Ro'grok 244709 active). Quest 90896.
--   PhaseId 15905 -- Phase 5 "peace" TERMINAL (on 90896 rewarded; battlefield
--                    clears, wildlife 883 Deer + 142334 spawn). Quests 90897/90911.
-- See 21_phase_area.sql for the AreaId->PhaseId map and 22_conditions_phasing.sql
-- for the quest-state gates that drive PhasingHandler::OnConditionChange.
--
-- ---- Z / O AUTHORING NOTE (required -- source z is NULL, o is 0 for all 81 rows) ----
-- -- TODO Phase K: z from VMAP/re-capture (`.gps` on scratch realm at each cluster, or
-- Map::GetHeight). Placeholder z below is the nearest cluster's known POI z from Plan
-- Part 1.7 (`area_poi.sql`, VerifiedBuild 69299): Hammerfall z~=56.5, Go'shek/Dabyrie's
-- Farmstead z~=42.2, Stromgarde Keep z~=80.0. Boulderfist Outpost/Hall (climax cluster)
-- has no captured POI z -- Stromgarde's 80.0 is reused as the nearest available
-- reference, also TODO Phase K. o=0 for every row (source o is 0; a real facing
-- requires the same re-capture/VMAP pass) -- -- TODO Phase K: o from re-capture.
-- ALL positions in this file are [A]-approximate.
--
-- ---- NOTE on y-range vs. brief guidance ----
-- The brief's verification guidance approximates y in "-1000..-1600". The actual
-- authored y span across all 65 rows in this file is -1569.1 (Stromgarde Citizen/
-- Beggar, 229955/230004, addon-captured) to -860.3 (Ogre Basher climax "wave" #3,
-- 8000305, a TODO-placeholder offset near Ro'grok) -- wider than that rough estimate
-- at the top end, but still unambiguously server-map-2796 Arathi space, NOT bleed.
-- Bleed rows (map-85 Chromie hub, map-84 Dornogal) sit in a completely different
-- coordinate regime (x~=+330 / -4200..-4380, y~=+1550..+1600 / -8300..-8315) and are
-- excluded entirely (see "DROPPED / bleed" list at the end of this banner).
--
-- ---- DROPPED entries (captured in addon_creature_spawns.sql map=2451 subset but
-- NOT spawned here) -- bleed, not in Task 1's creature_template, or off-scope:
--   245028, 32639, 32638, 245052, 249245, 249249, 59271, 142340, 246612
--     -- none of these 9 entries appear in 10_creature_template.sql's 46-entry
--     roster; no template exists to spawn against. 32639/32638/59271 read as
--     generic/critter baseline entries (not catchup-specific); 142340 is a
--     DIFFERENT entry from the brief's wildlife entry 142334 (peace phase) --
--     NOT the same creature, dropped rather than substituted.
--   244669 Scavenging Hyena, 244685 Ogre Basher -- IN Task 1's template but have
--     NO captured coordinate in addon_creature_spawns.sql at all. Spawned anyway
--     (both are named in the brief's phase requirements) at TODO placeholder
--     coordinates near their narrative cluster -- flagged per-row below.
--
-- ---- Bruvk vehicle-ride splines: NOT authored here (later task; the 109 vehicle
-- waypoints in waypoint_paths_movement.sql belong to Bruvk 244923's escort ride).
-- -- TODO: vehicle spline task picks this up against 244923's guid 8000105 below.
--
-- ---- Wildlife entries 883 (Deer) and 142334 (peace-phase burst): these are STOCK/
-- OTHER-SOURCE creature templates -- Task 1's 10_creature_template.sql did NOT author
-- entry 883 or 142334 (they are outside its 46-entry curated roster). Spawning them
-- here assumes their `creature_template` rows already exist elsewhere (baseline DB2/
-- core data for a generic Deer, and a not-yet-identified wildlife entry 142334).
-- -- TODO Phase K: confirm 883/142334 templates exist server-side; if not, author
-- them as their own content slice (out of this task's scope per the brief).
-- ============================================================================

DELETE FROM `creature` WHERE `guid` BETWEEN 8000000 AND 8000999;

-- ============================================================================
-- PHASE 1 (PhaseId 15901) -- Arrival / Hammerfall warzone -- guid 8000000-8000015
-- Default phase on zone entry. Requirement-1 static/ambient set: permanent story
-- leads + Win'sa + Hammerfall town/garrison NPCs, plus the gnoll-camp filler trash
-- that is hostile from the moment the player arrives (90882).
-- ============================================================================

-- entry 244643 Lady Jaina Proudmoore (Hammerfall, permanent story lead) -- addon row, Hammerfall cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000000, 244643, 2796, 15901, -3554.4001, -1101.7001, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244642 Thrall (Hammerfall, permanent story lead; captured hostile -- see Task 1 banner anomaly note) -- addon row, Hammerfall cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000001, 244642, 2796, 15901, -3554.4001, -1101.7001, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245026 Win'sa (Hammerfall food vendor) -- addon row, Hammerfall cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000002, 245026, 2796, 15901, -3554.4001, -1101.7001, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 245027 Gnoll Assailant (gnoll camp filler, hostile) -- addon row, Hammerfall cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000003, 245027, 2796, 15901, -3554.4001, -1101.7001, 56.5, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244670 Gnoll Bowblaster (gnoll camp filler, hostile; AIName=SmartAI per Task 1) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000004, 244670, 2796, 15901, -3553.3, -1028.5, 56.5, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244671 Gnoll Ripper (gnoll camp filler, hostile; autoattack only per Plan 1.4) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000005, 244671, 2796, 15901, -3557.8, -1085.4, 56.5, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244672 Gnoll Bruiser (gnoll camp filler, hostile) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000006, 244672, 2796, 15901, -3510.4001, -919.7, 56.5, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244669 Scavenging Hyena -- ** NO captured coordinate in addon_creature_spawns.sql **
-- -- TODO Phase K: placed near the Hammerfall gnoll-camp cluster centroid (+10/+10 offset);
-- re-capture or VMAP-walk to get a real position. In Task 1's template with rank normalized
-- to 0 (Normal); grouped here with the other gnoll-camp filler per Plan 1.2.
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000007, 244669, 2796, 15901, -3544.4001, -1091.7001, 56.5, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230248 Hammerfall Grunt (captured hostile -- Task 1 banner flags a possible 90897
-- "garrison turns on the player" story beat, which per the brief's quest list (90897/90911)
-- would actually belong to PhaseId 15905. Authored at the literal default 15901 per
-- Requirement 1's plain instruction; -- TODO Phase K: reconcile against Task 1's sniff
-- evidence and move to 15905 if confirmed.) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000008, 230248, 2796, 15901, -3516.3001, -915.1, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232019 Mag'har Grunt (same 90897 anomaly caveat as 230248 above) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000009, 232019, 2796, 15901, -3551.0, -1041.7001, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232022 Drum Fel (same 90897 anomaly caveat) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000010, 232022, 2796, 15901, -3530.6001, -930.5, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232023 Gor'mul (same 90897 anomaly caveat) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000011, 232023, 2796, 15901, -3484.4001, -958.9, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232028 Korin Fel (same 90897 anomaly caveat) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000012, 232028, 2796, 15901, -3531.0, -930.9, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232030 Tharlidun, Stable Master (same 90897 anomaly caveat) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000013, 232030, 2796, 15901, -3513.0, -979.9, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232035 Keena, Trade Goods (same 90897 anomaly caveat) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000014, 232035, 2796, 15901, -3516.3001, -915.1, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 232038 Uttnar, Butcher (same 90897 anomaly caveat) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000015, 232038, 2796, 15901, -3485.8, -959.1, 56.5, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 2 (PhaseId 15902) -- Go'shek / Dabyrie's Farmstead -- guid 8000100-8000113
-- Spawns on 90883 rewarded (see 22_conditions_phasing.sql). Quests 90885/86/87.
-- ============================================================================

-- entry 244674 Ogre Destroyer (farm trash, hostile) -- addon row, farm cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000100, 244674, 2796, 15902, -3051.8, -1544.6, 42.2, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249254 Ogre Destroyer (alt entry id, same name -- farm trash, hostile) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000101, 249254, 2796, 15902, -3091.2, -1522.3, 42.2, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244676 Kobold Pillager (farm trash, hostile) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000102, 244676, 2796, 15902, -3072.4001, -1531.1, 42.2, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249255 Kobold Pillager (alt entry id, same name) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000103, 249255, 2796, 15902, -3090.6001, -1523.1, 42.2, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244677 Kobold Firetender (farm trash, hostile; aggro cast 448429 per Plan 1.4) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000104, 244677, 2796, 15902, -2933.8001, -1506.8001, 42.2, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244923 Farmer Bruvk (escort/vehicle-ride clone -- vehicle splines NOT authored
-- here, later task; see banner) -- addon row, farm cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000105, 244923, 2796, 15902, -3090.6001, -1523.1, 42.2, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244656 Thrall (farm-phase clone) -- addon row, farm cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000106, 244656, 2796, 15902, -3090.6001, -1523.1, 42.2, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244655 Lady Jaina Proudmoore (farm-phase clone) -- addon row, farm cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000107, 244655, 2796, 15902, -3090.6001, -1523.1, 42.2, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244675 Runk (Go'shek Farm mini-boss; quests 90885/86/87 per Plan 1.2; death-cast
-- 305913 per Plan 1.4; AIName=SmartAI per Task 1) -- addon row, farm cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000112, 244675, 2796, 15902, -2994.1001, -1476.9001, 42.2, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244729 Farmer Bruvk (Go'shek farm clone, non-vehicle variant -- distinct from the
-- vehicle-ride clone 244923 above) -- addon row, farm cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000113, 244729, 2796, 15902, -3090.6001, -1523.1, 42.2, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244956 Prized Pumpkin x4 (quest-interact creature-prop, type=7/subname=questinteract
-- per Task 1; grouped per Requirement 2 -- 4 rows at the captured coord with small offsets
-- so they don't stack exactly)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000108, 244956, 2796, 15902, -3006.6001, -1536.9001, 42.2, 0, 300, 69299),
 (8000109, 244956, 2796, 15902, -3002.6001, -1536.9001, 42.2, 0, 300, 69299),
 (8000110, 244956, 2796, 15902, -3006.6001, -1532.9001, 42.2, 0, 300, 69299),
 (8000111, 244956, 2796, 15902, -3002.6001, -1532.9001, 42.2, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 3 (PhaseId 15903) -- Stromgarde siege battlefield -- guid 8000200-8000219
-- Spawns on 90888 rewarded (see 22_conditions_phasing.sql). Quests 90893/95.
-- ============================================================================

-- entry 244682 Kobold Waxmancer (siege trash, hostile; AIName=SmartAI per Task 1) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000200, 244682, 2796, 15903, -1913.0, -1237.6, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244695 Ettin Crusher (elite rank1 giant; death-cast 399062 per Plan 1.4) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000201, 244695, 2796, 15903, -1802.4, -1468.9, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244711 Armored Cleaver (elite rank1) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000202, 244711, 2796, 15903, -1994.8001, -1016.2, 80.0, 0, 120, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244785 Armored Cleaver (alt entry id, same name) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000203, 244785, 2796, 15903, -1971.6, -1051.1, 80.0, 0, 120, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244691 Gnoll Charger (siege trash, hostile) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000204, 244691, 2796, 15903, -1802.4, -1460.0, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244786 Gnoll Charger (alt entry id, same name) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000205, 244786, 2796, 15903, -1972.0, -1050.6, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 257072 Gnoll Biter (siege trash, hostile) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000206, 257072, 2796, 15903, -1771.3001, -1198.0, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244690 Stromgarde Footman (friendly reaction=5; requirement-2 explicitly lists
-- this as a siege-phase transient, overriding requirement-1's generic default) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000207, 244690, 2796, 15903, -1802.4, -1468.9, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 249269 Worn Catapult x4 (quest-interact creature-prop, type=7/subname=questinteract
-- per Task 1, quest 90895; grouped per Requirement 2 -- 4 rows at the captured coord with
-- small offsets)
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000208, 249269, 2796, 15903, -1874.9, -1339.0, 80.0, 0, 300, 69299),
 (8000209, 249269, 2796, 15903, -1869.9, -1339.0, 80.0, 0, 300, 69299),
 (8000210, 249269, 2796, 15903, -1874.9, -1334.0, 80.0, 0, 300, 69299),
 (8000211, 249269, 2796, 15903, -1869.9, -1334.0, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244657 Thrall (siege-entry clone) -- addon row, siege cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000212, 244657, 2796, 15903, -1802.8001, -1474.4, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244658 Lady Jaina Proudmoore (siege-entry clone) -- addon row, siege cluster
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000213, 244658, 2796, 15903, -1802.8001, -1474.4, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 229955 Stromgarde Citizen (friendly, reaction=5) -- captured at Stromgarde Keep hub
-- coords, geographically co-located with Footman 244690 (above) and hub-lead 244714 (below);
-- assigned 15903 rather than requirement-1's generic 15901 default because Stromgarde is not
-- reachable/relevant until the siege phase -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000214, 229955, 2796, 15903, -1808.5, -1569.1, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 230004 Beggar (friendly, reaction=5; same Stromgarde-hub co-location reasoning as
-- 229955 above) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000215, 230004, 2796, 15903, -1808.5, -1569.1, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244714 Lady Jaina Proudmoore (Stromgarde Keep hub lead, npcflag=1, gossip 39348 per
-- Task 1) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000216, 244714, 2796, 15903, -1812.4, -1568.0, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244683 Gnoll Prowler (siege-battlefield trash, hostile; name resolved via
-- conversation_actors.txt per Task 1 banner) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000217, 244683, 2796, 15903, -1803.9, -1371.4, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher -- ** NO captured coordinate ** -- named in BOTH the brief's
-- siege-mob list (->15903) AND its climax "Ogre Basher waves" list (->15904); resolved as
-- 2 regular siege-trash rows here (15903) + 3 additional "wave" rows in the Phase-4 block
-- below (15904), rather than picking one PhaseId and dropping the other mention.
-- -- TODO Phase K: real coordinate via re-capture/VMAP; sub-20% cast 33239 per Plan 1.4.
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000218, 244685, 2796, 15903, -1900.0, -1300.0, 80.0, 0, 60, 69299),
 (8000219, 244685, 2796, 15903, -1892.0, -1292.0, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 4 (PhaseId 15904) -- Siege climax -- guid 8000300-8000305
-- Spawns on 90896 taken (see 22_conditions_phasing.sql). Quest 90896.
-- ============================================================================

-- entry 244709 Ro'grok (Boulderfist siege leader, final boss; sub-20% casts 305913/317547,
-- death-cast 305913 per Plan 1.4; AIName=SmartAI per Task 1) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000300, 244709, 2796, 15904, -2120.9001, -870.3, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244666 Thrall (siege-climax clone per Plan 1.2) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000301, 244666, 2796, 15904, -1980.7001, -1004.5, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244667 Lady Jaina Proudmoore (siege-climax clone per Plan 1.2) -- addon row
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000302, 244667, 2796, 15904, -1983.7001, -1004.4, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 244685 Ogre Basher "wave" reinforcements (3 rows) -- see the 15903 block above for
-- the base-siege-trash rows and the reasoning; -- TODO Phase K: real coordinates + actual
-- wave timing/triggers (this is 3 static rows, not a scripted wave spawner -- SmartAI/
-- SPAWN_GROUP wave logic is a later task, not this content slice).
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000303, 244685, 2796, 15904, -2120.9001, -870.3, 80.0, 0, 60, 69299),
 (8000304, 244685, 2796, 15904, -2110.9001, -870.3, 80.0, 0, 60, 69299),
 (8000305, 244685, 2796, 15904, -2120.9001, -860.3, 80.0, 0, 60, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- PHASE 5 (PhaseId 15905) -- Peace, TERMINAL -- guid 8000400-8000408
-- Spawns on 90896 rewarded (see 22_conditions_phasing.sql). Quests 90897/90911.
-- Per Plan 1.6: "the clearest evidence -- a 10-entry spawn-burst of wildlife (883 Deer x1,
-- 142334 x8) snaps in -- the warzone-to-calm transformation." Neither 883 nor 142334 has a
-- captured coordinate or a Task-1-authored `creature_template` row -- see banner.
-- Placed across the former siege battlefield (the area that "clears").
-- ============================================================================

-- entry 883 Deer x1 -- STOCK/OTHER-SOURCE TEMPLATE, NOT authored by Task 1 -- TODO Phase K
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000400, 883, 2796, 15905, -1900.0, -1300.0, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- entry 142334 x8 -- STOCK/OTHER-SOURCE TEMPLATE, NOT authored by Task 1 -- TODO Phase K
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`VerifiedBuild`) VALUES
 (8000401, 142334, 2796, 15905, -1910.0, -1310.0, 80.0, 0, 300, 69299),
 (8000402, 142334, 2796, 15905, -1890.0, -1310.0, 80.0, 0, 300, 69299),
 (8000403, 142334, 2796, 15905, -1910.0, -1290.0, 80.0, 0, 300, 69299),
 (8000404, 142334, 2796, 15905, -1890.0, -1290.0, 80.0, 0, 300, 69299),
 (8000405, 142334, 2796, 15905, -1900.0, -1320.0, 80.0, 0, 300, 69299),
 (8000406, 142334, 2796, 15905, -1900.0, -1280.0, 80.0, 0, 300, 69299),
 (8000407, 142334, 2796, 15905, -1920.0, -1300.0, 80.0, 0, 300, 69299),
 (8000408, 142334, 2796, 15905, -1880.0, -1300.0, 80.0, 0, 300, 69299)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `map`=VALUES(`map`), `PhaseId`=VALUES(`PhaseId`), `position_x`=VALUES(`position_x`), `position_y`=VALUES(`position_y`), `position_z`=VALUES(`position_z`), `orientation`=VALUES(`orientation`), `spawntimesecs`=VALUES(`spawntimesecs`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- ============================================================================
-- END -- 65 creature rows across guid 8000000-8000408 (reserved block 8000000-8000999).
-- ============================================================================
