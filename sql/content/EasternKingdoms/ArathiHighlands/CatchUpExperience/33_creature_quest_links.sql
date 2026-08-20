-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E questgiver/turn-in links
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/quest_structured_candidates.sql
--   (hard-confirmed: 244643->90882 both queststarter and questender, [C hard]), plan
--   Part 1.1 Giver/Ender columns for the remaining 10 quests (no further oracle rows were
--   captured for the rest of the chain -- authored from the plan table, cross-checked
--   against Task 1's 10_creature_template.sql to confirm every entry below exists).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (`id`,`quest`)).
-- ============================================================================

-- ---- creature_queststarter (11 rows, one per quest's Giver column) ----
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
 (244643, 90882),  -- Jaina (Hammerfall) -- [C hard], matches quest_structured_candidates.sql
 (244643, 90883),  -- Jaina (Hammerfall)
 (244729, 90885),  -- Farmer Bruvk (Go'shek)
 (244656, 90886),  -- Thrall (farm)
 (244655, 90887),  -- Jaina (farm)
 (244655, 90888),  -- Jaina (farm)
 (244657, 90893),  -- Thrall (siege entry)
 (244658, 90895),  -- Jaina (siege)
 (244666, 90896),  -- Thrall (siege climax)
 (244667, 90897),  -- Jaina (siege climax)
 (244714, 90911)   -- Jaina (Stromgarde Keep hub)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

-- ---- creature_questender (11 rows, one per quest's Ender column) ----
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
 (244643, 90882),  -- Jaina (Hammerfall) -- [C hard], matches quest_structured_candidates.sql
 (244729, 90883),  -- Farmer Bruvk (Go'shek)
 (244656, 90885),  -- Thrall (farm)
 (244656, 90886),  -- Thrall (farm)
 (244655, 90887),  -- Jaina (farm)
 (244657, 90888),  -- Thrall (siege entry)
 (244666, 90893),  -- Thrall (siege climax)
 (244667, 90895),  -- Jaina (siege climax)
 (244666, 90896),  -- Thrall (siege climax)
 (244714, 90897),  -- Jaina (Stromgarde Keep hub)
 (244714, 90911)   -- Jaina (Stromgarde Keep hub)
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);
