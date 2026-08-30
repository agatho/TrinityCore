-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E questgiver/turn-in links
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/quest_structured_candidates.sql
--   (hard-confirmed: 244643->90882 both queststarter and questender, [C hard]), plan
--   Part 1.1 Giver/Ender columns for the remaining 10 quests (no further oracle rows were
--   captured for the rest of the chain -- authored from the plan table, cross-checked
--   against Task 1's 10_creature_template.sql to confirm every entry below exists).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (`id`,`quest`)).
-- ============================================================================

-- ---- creature_queststarter (11 Alliance rows, one per quest's Giver column) ----
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

-- ---- creature_questender (11 Alliance rows, one per quest's Ender column) ----
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

-- ============================================================================
-- HORDE-XVAL FIX (H3, task-2) -- missing Horde questgivers
-- ============================================================================
-- The mid-chain givers (90885-90896, above) are SHARED between factions -- correct as
-- authored, left untouched. But the 3 faction-specific seams only had their Alliance side
-- wired: Horde players had NO creature offering/accepting 90882, 90883, or 90911 at all
-- (both 90882 and 90883 only listed Jaina 244643; 90911 only listed Jaina 244714) -- a
-- Horde character could not start or progress this chain. Fixed by mirroring the Alliance
-- rows onto their already-authored Horde counterparts (Task 1 10_creature_template.sql /
-- H2's 244715 addition):
--   * 244642 Thrall = Horde mirror of Alliance greeter 244643 Jaina (Hammerfall opening) --
--     both quest_queststarter for 90882/90883, and quest_questender for 90882 (90883's
--     ender stays the SHARED 244729 Farmer Bruvk, untouched, per Requirement 2).
--   * 244715 Thrall = Horde mirror of Alliance hub 244714 Jaina (90911 "Your Next
--     Adventure" gossip hub) -- both queststarter and questender, matching addon-captured
--     addon_creature_questender row 244715->90911 [C].
-- ---- creature_queststarter (Horde) ----
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
 (244642, 90882),  -- Thrall (Hammerfall) -- Horde mirror of Alliance 244643 Jaina
 (244642, 90883),  -- Thrall (Hammerfall) -- Horde mirror of Alliance 244643 Jaina
 (244715, 90911)   -- Thrall (Horde hub) -- Horde mirror of Alliance 244714 Jaina; [C] addon_creature_questender 244715->90911
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);

-- ---- creature_questender (Horde) ----
-- 90882 turn-in via 244642 Thrall (mirror of Alliance 244643 Jaina). 90883's ender stays
-- the SHARED 244729 Farmer Bruvk (already correct above, not duplicated here).
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
 (244642, 90882),  -- Thrall (Hammerfall) -- Horde mirror of Alliance 244643 Jaina, 90882 turn-in
 (244715, 90911)   -- Thrall (Horde hub) -- Horde mirror of Alliance 244714 Jaina; [C] addon_creature_questender 244715->90911
ON DUPLICATE KEY UPDATE `id`=VALUES(`id`), `quest`=VALUES(`quest`);
