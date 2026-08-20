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
