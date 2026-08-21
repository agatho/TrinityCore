-- ============================================================================
-- ARATHI CATCH-UP / RPE CONSOLIDATION -- CLEANUP (runs FIRST) (issue 6)
-- ============================================================================
-- Branch: feature/arathi-rpe   Path: sql/updates/world/master/
-- Server mapID: 2927 (Arathi Returning-Player-Experience instance).
--
-- WHY THIS FILE EXISTS
-- The RPE data used to be split across two guid blocks that STACKED on the realm
-- (every creature doubled -- the tester's "too many gnolls at Hammerfall"):
--   * Set A (authoritative content slices) -- guid block 8000000+ -- now the single
--     authoritative spawn set, re-authored by 2026_08_21_02 from the reconciled file.
--   * Set B (early rough opening) -- guid block 11002000+ -- authored by the historical
--     migrations 2026_08_15_50..57. Superseded.
-- The 2026_08_15_50..57 FILES are LEFT IN PLACE (they are migrations already shipped;
-- this branch's convention is not to rewrite superseded historical updates). This cleanup
-- therefore retires their now-superseded rows so that BOTH a fresh apply (50..57 then this)
-- AND an already-applied realm converge on the single 8000000-block authoritative set.
--
-- WHAT IS DELETED HERE (all superseded by content re-authored in 2026_08_21_01..05):
--   * the entire 11002000-11002999 creature spawn block on map 2927 (the doubling source);
--   * the three per-guid creature_addon poses on 11002031/32/33 -- re-keyed to the surviving
--     8000000-block guids (8000002/8001102/8001103) in 2026_08_21_02;
--   * the 50..57 phase-gating conditions (CONDITION_SOURCE_TYPE_PHASE 26 on the 26xxx login
--     phase ids) -- content 22_conditions_phasing replaces the phase spine with the real
--     wire-derived PhaseId scheme (3/4/8/28/37/1610/1959/1961/1965 on real Arathi AreaIds);
--   * the one divergent 50..57 quest-link that content does NOT re-author
--     (creature_questender 244729->90885; content's 90885 ender is 244656). The remaining
--     50..57 quest-link pairs are re-authored identically by 33_creature_quest_links /
--     70_horde_variant (INSERT ... ON DUPLICATE), so they are deleted here and re-added there.
--
-- WHAT IS DELIBERATELY *NOT* DELETED (see phk-consolidation-report.md):
--   * phase_name (26596/26618/27217/26588/26599) -- PRESERVED per task; re-asserted in
--     2026_08_21_02. scene_template (3692/3749) -- PRESERVED; re-asserted in 2026_08_21_04.
--   * creature_template_addon (245027/249245/244642/244643) -- by-entry (PK=entry), these do
--     NOT double. Content 10b (INSERT ... ON DUPLICATE) OVERWRITES the shared entries in
--     2026_08_21_01; NOT deleting them keeps 249245's floating presentation and Thrall/Jaina
--     pad-pose auras (1237057/1237118) that content 10b does not itself re-author.
--   * quest_template_addon (90883) -- by-ID (PK=ID); content 31 re-authors it via
--     INSERT ... ON DUPLICATE, so no delete is needed.
-- ============================================================================

-- 1) The doubled 11002000-block spawns (map 2927) -- the actual stacking source.
DELETE FROM `creature` WHERE `map`=2927 AND `guid` BETWEEN 11002000 AND 11002999;

-- 2) The per-guid pad poses re-keyed to the 8000000 block in 2026_08_21_02.
DELETE FROM `creature_addon` WHERE `guid` IN (11002031, 11002032, 11002033);

-- 3) The superseded 50..57 phase-gating conditions (26xxx login-phase spine).
--    Matches the rows inserted by 2026_08_15_51 exactly.
DELETE FROM `conditions`
 WHERE `SourceTypeOrReferenceId`=26
   AND `SourceGroup` IN (26618, 26596, 26588, 26599)
   AND `SourceEntry`=0;

-- 4) The 50..57 quest-link pairs. Content 33_creature_quest_links / 70_horde_variant
--    (in 2026_08_21_03, which runs AFTER this file) re-author the authoritative set; the
--    only pair they do NOT re-add is the divergent 244729->90885 ender, which is thereby
--    retired. Scoped strictly to the 50..57 (id,quest) footprint.
DELETE FROM `creature_queststarter`
 WHERE `quest` IN (90882, 90883, 90885, 90886, 90887)
   AND `id` IN (244642, 244643, 244729, 244656, 244655);
DELETE FROM `creature_questender`
 WHERE `quest` IN (90882, 90883, 90885, 90886, 90887)
   AND `id` IN (244642, 244643, 244729, 244656, 244655);
