-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase C creature/GO templates :: creature_template_model
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve capture)
-- Sources used: addon_creature_template.sql, wdb_creature_template.sql,
--   addon_creature_observed.txt (per-entry reaction), db2_creaturediff.sql
--   (checked -- zero rows for any in-scope entry, no divergence to resolve),
--   creature_template_gossip.sql (authoritative gossip MenuID), combatlog_creature_template_ainame.sql.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================

-- SECTION 3 -- creature_template_model, filtered to the in-scope roster.
-- Source: wdb_creature_template_model.sql (14 rows total, WDB-client-cache derived).
-- Only in-scope entries present in that source are emitted; the other 8 rows in the
-- 14-row source (29812, 72654, 92870, 111190, 31643, 43499, 32606, 246613) are all
-- out-of-scope bleed (unrelated vehicle-cursor / other-zone entries) and are excluded.
-- ============================================================================

-- entry 244642 Thrall
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES (244642, 0, 115495, 1.2000000476837158, 1.0) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`), `DisplayScale`=VALUES(`DisplayScale`), `Probability`=VALUES(`Probability`);
-- entry 244656 Thrall
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES (244656, 0, 115495, 1.2000000476837158, 1.0) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`), `DisplayScale`=VALUES(`DisplayScale`), `Probability`=VALUES(`Probability`);
-- entry 244657 Thrall
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES (244657, 0, 115495, 1.2000000476837158, 1.0) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`), `DisplayScale`=VALUES(`DisplayScale`), `Probability`=VALUES(`Probability`);
-- entry 244666 Thrall
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES (244666, 0, 115495, 1.2000000476837158, 1.0) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`), `DisplayScale`=VALUES(`DisplayScale`), `Probability`=VALUES(`Probability`);
-- entry 244675 Runk
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES (244675, 0, 91851, 2.299999952316284, 1.0) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`), `DisplayScale`=VALUES(`DisplayScale`), `Probability`=VALUES(`Probability`);
-- entry 244709 Ro'grok
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES (244709, 0, 129376, 2.299999952316284, 1.0) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`), `DisplayScale`=VALUES(`DisplayScale`), `Probability`=VALUES(`Probability`);

-- Row count: 6 creature_template_model rows authored.
