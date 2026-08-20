-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D phase_area map
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Depends on: 20_creature_spawns.sql (defines/owns the canonical PhaseId map,
--   reproduced below for this file's own readability), 22_conditions_phasing.sql
--   (the quest-state gates that actually flip PhasingHandler::OnConditionChange).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (AreaId,PhaseId)).
-- ============================================================================
--
-- ---- THE CANONICAL PhaseId MAP (owned by 20_creature_spawns.sql; base 15900) ----
--   15901 Phase 1 arrival/Hammerfall warzone   15902 Phase 2 Go'shek farm
--   15903 Phase 3 Stromgarde siege             15904 Phase 4 siege climax
--   15905 Phase 5 peace (TERMINAL)
--
-- ---- AreaId source -- Plan Part 1.7 (`area_poi.sql`, VerifiedBuild 69299) ----
-- -- TODO Phase K: confirm exact RPE subzone AreaIds for map 2796. The values below are
-- the live-Arathi-Highlands-terrain AreaIds from the captured POI list (Hammerfall 7658,
-- Stromgarde Keep 7667, Refuge Pointe 7678, Go'shek/Dabyrie's Farmstead 7680, Boulderfist
-- Outpost/Hall 7682) -- ASSUMED reusable because map 2796's RPE instance is a personal-
-- phased copy of the same Arathi Highlands terrain (same WDT/ADT -> same AreaTable.db2
-- rows), not a distinct terrain build. If the re-capture in Phase K shows map 2796 has its
-- own distinct AreaTable rows (a real possibility for an instanced copy), every row below
-- needs updating.
-- ============================================================================

-- Phase 1 (arrival) -- Hammerfall + the adjacent Refuge Pointe corridor
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7658, 15901, 'Catch-Up Experience -- Hammerfall -- Phase 1 arrival/warzone (default on entry) -- TODO Phase K confirm AreaId'),
 (7678, 15901, 'Catch-Up Experience -- Refuge Pointe -- Phase 1 arrival span -- TODO Phase K confirm AreaId')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- Phase 2 (farm) -- Go'shek / Dabyrie's Farmstead
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7680, 15902, 'Catch-Up Experience -- Go''shek/Dabyrie''s Farmstead -- Phase 2 farm (90883 rewarded) -- TODO Phase K confirm AreaId')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- Phase 3 (siege) -- Stromgarde Keep battlefield
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7667, 15903, 'Catch-Up Experience -- Stromgarde Keep -- Phase 3 siege (90888 rewarded) -- TODO Phase K confirm AreaId')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- Phase 4 (siege climax) -- Boulderfist Outpost/Hall (Ro'grok's lair)
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7682, 15904, 'Catch-Up Experience -- Boulderfist Outpost/Hall -- Phase 4 siege climax (90896 taken) -- TODO Phase K confirm AreaId')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- Phase 5 (peace, TERMINAL) -- the whole battlefield returns to calm; cover every
-- sub-area the warzone touched so the peace phase applies zone-wide, not just at one hub.
INSERT INTO `phase_area` (`AreaId`, `PhaseId`, `Comment`) VALUES
 (7658, 15905, 'Catch-Up Experience -- Hammerfall -- Phase 5 peace/terminal (90896 rewarded) -- TODO Phase K confirm AreaId'),
 (7680, 15905, 'Catch-Up Experience -- Go''shek/Dabyrie''s Farmstead -- Phase 5 peace/terminal -- TODO Phase K confirm AreaId'),
 (7667, 15905, 'Catch-Up Experience -- Stromgarde Keep -- Phase 5 peace/terminal -- TODO Phase K confirm AreaId'),
 (7682, 15905, 'Catch-Up Experience -- Boulderfist Outpost/Hall -- Phase 5 peace/terminal -- TODO Phase K confirm AreaId')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- ============================================================================
-- END -- 8 phase_area rows; all 5 PhaseIds (15901-15905) covered.
-- ============================================================================
