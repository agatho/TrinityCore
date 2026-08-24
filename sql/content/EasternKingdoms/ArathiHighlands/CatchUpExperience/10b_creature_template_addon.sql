-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase C creature/GO templates :: creature_template_addon
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

-- SECTION 2 -- creature_template_addon, filtered to the in-scope roster.
-- Source: creature_template_addon.sql (253 rows total). Every row in that source uses
-- only (entry, StandState, AnimTier, SheathState) -- there is NO Emote or mount column
-- anywhere in the source file (checked: grep of distinct column sets returns exactly
-- one set for the whole 253-row file), so there is nothing to plausibility-review for
-- Emote and nothing to force mount=0 on -- neither field is present to begin with.
-- Only in-scope entries that actually appear in the source are emitted below; other
-- in-scope entries (e.g. Runk, Ettin Crusher, Stromgarde Footman, Win'sa -- see the
-- report for the full list) have no captured addon row and are simply absent upstream.
-- ============================================================================

-- entry 229955 Stromgarde Citizen
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (229955, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 230004 Beggar
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (230004, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 230248 Hammerfall Grunt
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (230248, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 232019 Mag'har Grunt
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (232019, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 232035 Keena
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (232035, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 232038 Uttnar
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (232038, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244642 Thrall
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244642, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244643 Lady Jaina Proudmoore
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244643, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244655 Lady Jaina Proudmoore
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244655, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244656 Thrall
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244656, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244657 Thrall
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244657, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244658 Lady Jaina Proudmoore
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244658, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244666 Thrall
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244666, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244667 Lady Jaina Proudmoore
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244667, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244669 Scavenging Hyena
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244669, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244670 Gnoll Bowblaster
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244670, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244671 Gnoll Ripper
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244671, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244676 Kobold Pillager
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244676, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244682 Kobold Waxmancer
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244682, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244683 Gnoll Prowler
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244683, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244685 Ogre Basher
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244685, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244709 Ro'grok
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244709, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244711 Armored Cleaver
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244711, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244714 Lady Jaina Proudmoore
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244714, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244729 Farmer Bruvk
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244729, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244785 Armored Cleaver
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244785, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244786 Gnoll Charger
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244786, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 244956 Prized Pumpkin
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (244956, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 249254 Ogre Destroyer
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (249254, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 249255 Kobold Pillager
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (249255, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);
-- entry 249269 Worn Catapult
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`) VALUES (249269, 0, 0, 1) ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`);

-- Row count: 31 creature_template_addon rows authored.
