-- ============================================================================
-- ARATHI CATCH-UP / RPE CONSOLIDATION -- TEMPLATES (issue 6)
-- ============================================================================
-- Branch: feature/arathi-rpe   Path: sql/updates/world/master/   Server mapID: 2927
-- Consolidated verbatim from the authoritative content slices (guid block 8000000);
-- runs after 2026_08_21_00 cleanup. Each source slice keeps its own banner + idempotency.
-- Order: creature_template -> template_addon(+R1 feign-death) -> model -> spell -> gameobject_template -> loot.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 10_creature_template.sql  <<<<<<<<<<<<<<<<<<<<

-- ADAPTED to integ_world 12.x schema (Arathi Catch-Up / RPE).
-- Schema mapping applied:
--   rank            -> Classification  (12.x renamed the column)
--   minlevel/maxlevel -> DROPPED        (no such columns in 12.x creature_template;
--                                        creatures already carry correct levels from base)
--   gossip_menu_id  -> moved OUT to creature_template_gossip (CreatureID, MenuID)
-- Creatures already exist in base creature_template, so ON DUPLICATE KEY UPDATE only
-- touches the RPE-relevant fields (faction, npcflag, Classification) -- base data preserved.
-- npcflag: content value OR'd with UNIT_NPC_FLAG_QUESTGIVER(2) for every creature that is
-- a quest starter/ender in 33_creature_quest_links.sql (fixes step-4 blocker: 244643).

INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (229955, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (230004, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (230248, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232019, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232022, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232023, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232028, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232030, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232035, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232038, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244642, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244643, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244655, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244656, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244657, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244658, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244666, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244667, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244669, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244670, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244671, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244672, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244674, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244675, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244676, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244677, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244682, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244683, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244685, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244690, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244691, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244695, 14, 0, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244709, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244711, 14, 0, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244714, 35, 3, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244715, 35, 3, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- questgiver ok
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244729, 35, 2, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244785, 14, 0, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244786, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244923, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244956, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (245026, 35, 129, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (245027, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (249254, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (249255, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (249269, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (257072, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);

-- gossip links moved from creature_template.gossip_menu_id -> creature_template_gossip
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(244714, 39348, 69382),
(245026, 39386, 69382)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 10b_creature_template_addon.sql (+R1 245027 feign-death)  <<<<<<<<<<<<<<<<<<<<

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

-- R1 (reconcile report §7): pad Gnoll Assailant corpses render as slain via permanent Feign Death.
-- entry 245027 Gnoll Assailant -- feign-death corpse tableau (aura 29266); full column set to carry `auras`.
INSERT INTO `creature_template_addon` (`entry`, `StandState`, `AnimTier`, `SheathState`, `auras`) VALUES (245027, 0, 0, 1, '29266') ON DUPLICATE KEY UPDATE `StandState`=VALUES(`StandState`), `AnimTier`=VALUES(`AnimTier`), `SheathState`=VALUES(`SheathState`), `auras`=VALUES(`auras`);

-- Row count: 31 creature_template_addon rows authored.


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 10c_creature_template_model.sql  <<<<<<<<<<<<<<<<<<<<

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


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 10d_creature_template_spell.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase C creature/GO templates :: creature_template_spell
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

-- SECTION 4 -- creature_template_spell, filtered to the in-scope roster.
-- Source: creature_template_spell.sql (308 rows total, ADVERTISED repertoire -- i.e.
-- spells associated with the creature record, NOT a combat-verified cast log like the
-- SECTION-1-style house example). These rows are REVIEW-ONLY: a SmartAI creature (see
-- AIName SmartAI set in 10_creature_template.sql) is not driven to cast anything by
-- creature_template_spell alone -- the real cast wiring is smart_scripts
-- SMART_ACTION_CAST, which this bundle does not derive timings/targets for. Filtering
-- is by IN-SCOPE ENTRY ONLY (per Requirement 4) -- no further per-spell plausibility
-- curation was applied; every advertised spell captured for an in-scope entry is
-- included as-is.
-- ============================================================================

-- entry 244675 Runk -- 21 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244675, 5176, 69382, 0), (244675, 8042, 69382, 1), (244675, 31935, 69382, 2), (244675, 45284, 69382, 3), (244675, 53600, 69382, 4), (244675, 81297, 69382, 5), (244675, 164812, 69382, 6), (244675, 164815, 69382, 7), (244675, 188196, 69382, 8), (244675, 188389, 69382, 9), (244675, 192109, 69382, 10), (244675, 204301, 69382, 11), (244675, 275779, 69382, 12), (244675, 285452, 69382, 13), (244675, 285466, 69382, 14), (244675, 305913, 69382, 15), (244675, 317547, 69382, 16), (244675, 432616, 69382, 17), (244675, 433808, 69382, 18), (244675, 1236111, 69382, 19), (244675, 1243133, 69382, 20) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244709 Ro'grok -- 8 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244709, 8042, 69382, 0), (244709, 188196, 69382, 1), (244709, 188389, 69382, 2), (244709, 192109, 69382, 3), (244709, 285452, 69382, 4), (244709, 305913, 69382, 5), (244709, 317547, 69382, 6), (244709, 1248782, 69382, 7) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244695 Ettin Crusher -- 9 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244695, 8042, 69382, 0), (244695, 45284, 69382, 1), (244695, 188196, 69382, 2), (244695, 188389, 69382, 3), (244695, 192109, 69382, 4), (244695, 285452, 69382, 5), (244695, 399062, 69382, 6), (244695, 399063, 69382, 7), (244695, 399065, 69382, 8) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244685 Ogre Basher -- 14 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244685, 5176, 69382, 0), (244685, 33239, 69382, 1), (244685, 164812, 69382, 2), (244685, 164815, 69382, 3), (244685, 228354, 69382, 4), (244685, 228597, 69382, 5), (244685, 228598, 69382, 6), (244685, 228600, 69382, 7), (244685, 379029, 69382, 8), (244685, 443722, 69382, 9), (244685, 1246949, 69382, 10), (244685, 1262769, 69382, 11), (244685, 1270769, 69382, 12), (244685, 1270770, 69382, 13) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244669 Scavenging Hyena -- 9 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244669, 5176, 69382, 0), (244669, 8676, 69382, 1), (244669, 30451, 69382, 2), (244669, 86392, 69382, 3), (244669, 164812, 69382, 4), (244669, 193315, 69382, 5), (244669, 204301, 69382, 6), (244669, 275779, 69382, 7), (244669, 1236111, 69382, 8) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244670 Gnoll Bowblaster -- 15 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244670, 5176, 69382, 0), (244670, 30451, 69382, 1), (244670, 31935, 69382, 2), (244670, 45297, 69382, 3), (244670, 81297, 69382, 4), (244670, 164812, 69382, 5), (244670, 164815, 69382, 6), (244670, 188389, 69382, 7), (244670, 188443, 69382, 8), (244670, 204301, 69382, 9), (244670, 275779, 69382, 10), (244670, 285452, 69382, 11), (244670, 372369, 69382, 12), (244670, 433808, 69382, 13), (244670, 1236111, 69382, 14) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244671 Gnoll Ripper -- 22 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244671, 5176, 69382, 0), (244671, 7268, 69382, 1), (244671, 8042, 69382, 2), (244671, 31935, 69382, 3), (244671, 45284, 69382, 4), (244671, 45297, 69382, 5), (244671, 53600, 69382, 6), (244671, 81297, 69382, 7), (244671, 164812, 69382, 8), (244671, 164815, 69382, 9), (244671, 188196, 69382, 10), (244671, 188389, 69382, 11), (244671, 188443, 69382, 12), (244671, 192109, 69382, 13), (244671, 204301, 69382, 14), (244671, 275779, 69382, 15), (244671, 285452, 69382, 16), (244671, 365350, 69382, 17), (244671, 433808, 69382, 18), (244671, 450499, 69382, 19), (244671, 1236111, 69382, 20), (244671, 1243133, 69382, 21) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244672 Gnoll Bruiser -- 25 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244672, 7268, 69382, 0), (244672, 8042, 69382, 1), (244672, 31935, 69382, 2), (244672, 45297, 69382, 3), (244672, 47568, 69382, 4), (244672, 49184, 69382, 5), (244672, 53600, 69382, 6), (244672, 81297, 69382, 7), (244672, 164812, 69382, 8), (244672, 188389, 69382, 9), (244672, 188443, 69382, 10), (244672, 192109, 69382, 11), (244672, 195975, 69382, 12), (244672, 204301, 69382, 13), (244672, 207230, 69382, 14), (244672, 275779, 69382, 15), (244672, 365350, 69382, 16), (244672, 433808, 69382, 17), (244672, 435802, 69382, 18), (244672, 439843, 69382, 19), (244672, 450421, 69382, 20), (244672, 450462, 69382, 21), (244672, 456139, 69382, 22), (244672, 1236111, 69382, 23), (244672, 1270764, 69382, 24) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244691 Gnoll Charger -- 2 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244691, 1604, 69382, 0), (244691, 192109, 69382, 1) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 257072 Gnoll Biter -- 15 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (257072, 5176, 69382, 0), (257072, 8042, 69382, 1), (257072, 164812, 69382, 2), (257072, 164815, 69382, 3), (257072, 188196, 69382, 4), (257072, 188389, 69382, 5), (257072, 192109, 69382, 6), (257072, 228354, 69382, 7), (257072, 228597, 69382, 8), (257072, 228598, 69382, 9), (257072, 285452, 69382, 10), (257072, 443722, 69382, 11), (257072, 1246949, 69382, 12), (257072, 1262769, 69382, 13), (257072, 1270771, 69382, 14) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244676 Kobold Pillager -- 26 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244676, 5176, 69382, 0), (244676, 31935, 69382, 1), (244676, 53600, 69382, 2), (244676, 81297, 69382, 3), (244676, 100441, 69382, 4), (244676, 164812, 69382, 5), (244676, 164815, 69382, 6), (244676, 188389, 69382, 7), (244676, 192109, 69382, 8), (244676, 204301, 69382, 9), (244676, 228354, 69382, 10), (244676, 228597, 69382, 11), (244676, 228598, 69382, 12), (244676, 275779, 69382, 13), (244676, 285452, 69382, 14), (244676, 285466, 69382, 15), (244676, 379029, 69382, 16), (244676, 432616, 69382, 17), (244676, 433808, 69382, 18), (244676, 443722, 69382, 19), (244676, 444487, 69382, 20), (244676, 448341, 69382, 21), (244676, 1236111, 69382, 22), (244676, 1243133, 69382, 23), (244676, 1246949, 69382, 24), (244676, 1262769, 69382, 25) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244677 Kobold Firetender -- 26 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244677, 31935, 69382, 0), (244677, 45284, 69382, 1), (244677, 47730, 69382, 2), (244677, 53600, 69382, 3), (244677, 81297, 69382, 4), (244677, 164812, 69382, 5), (244677, 164815, 69382, 6), (244677, 188196, 69382, 7), (244677, 188389, 69382, 8), (244677, 192109, 69382, 9), (244677, 204301, 69382, 10), (244677, 228354, 69382, 11), (244677, 228597, 69382, 12), (244677, 228598, 69382, 13), (244677, 285452, 69382, 14), (244677, 285466, 69382, 15), (244677, 379029, 69382, 16), (244677, 432616, 69382, 17), (244677, 433808, 69382, 18), (244677, 443722, 69382, 19), (244677, 444487, 69382, 20), (244677, 448429, 69382, 21), (244677, 1236111, 69382, 22), (244677, 1243133, 69382, 23), (244677, 1246949, 69382, 24), (244677, 1262769, 69382, 25) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244682 Kobold Waxmancer -- 13 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244682, 5176, 69382, 0), (244682, 164812, 69382, 1), (244682, 164815, 69382, 2), (244682, 188389, 69382, 3), (244682, 192109, 69382, 4), (244682, 228354, 69382, 5), (244682, 228597, 69382, 6), (244682, 228598, 69382, 7), (244682, 285452, 69382, 8), (244682, 285466, 69382, 9), (244682, 443722, 69382, 10), (244682, 448429, 69382, 11), (244682, 1246949, 69382, 12) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244674 Ogre Destroyer -- 17 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244674, 5176, 69382, 0), (244674, 8042, 69382, 1), (244674, 31935, 69382, 2), (244674, 53600, 69382, 3), (244674, 81297, 69382, 4), (244674, 164812, 69382, 5), (244674, 164815, 69382, 6), (244674, 188389, 69382, 7), (244674, 192109, 69382, 8), (244674, 204301, 69382, 9), (244674, 275779, 69382, 10), (244674, 285452, 69382, 11), (244674, 316890, 69382, 12), (244674, 432616, 69382, 13), (244674, 433808, 69382, 14), (244674, 1236111, 69382, 15), (244674, 1243133, 69382, 16) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244683 Gnoll Prowler -- 9 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244683, 5176, 69382, 0), (244683, 164812, 69382, 1), (244683, 164815, 69382, 2), (244683, 188389, 69382, 3), (244683, 192109, 69382, 4), (244683, 228597, 69382, 5), (244683, 285452, 69382, 6), (244683, 285466, 69382, 7), (244683, 372397, 69382, 8) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244956 Prized Pumpkin -- 1 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244956, 1236771, 69382, 0) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 249269 Worn Catapult -- 3 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (249269, 1248641, 69382, 0), (249269, 1248649, 69382, 1), (249269, 1248657, 69382, 2) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 229955 Stromgarde Citizen -- 6 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (229955, 123964, 69382, 0), (229955, 159560, 69382, 1), (229955, 290188, 69382, 2), (229955, 369566, 69382, 3), (229955, 369600, 69382, 4), (229955, 403448, 69382, 5) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
-- entry 244690 Stromgarde Footman -- 1 advertised spell(s)
INSERT INTO `creature_template_spell` (`CreatureID`, `Spell`, `VerifiedBuild`, `Index`) VALUES (244690, 1604, 69382, 0) ON DUPLICATE KEY UPDATE `Spell`=VALUES(`Spell`), `VerifiedBuild`=VALUES(`VerifiedBuild`);

-- Row count: 242 creature_template_spell rows authored across 19 in-scope entries.


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 11_gameobject_template.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase C creature/GO templates :: gameobject_template
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

-- SECTION 5 -- gameobject_template : INTENTIONALLY EMPTY.
--
-- The TCHarvest capture for zone 2796 recovered NO Arathi-zone GameObjects in scope for
-- this task. The two props named in the brief as candidate GOs -- the catapult and the
-- pumpkin -- are NOT GameObjects at all in this capture: they are CREATURES
-- (244956 Prized Pumpkin, 249269 Worn Catapult; both type=7, subname='questinteract'),
-- authored in 10_creature_template.sql instead. db2_gameobjects.sql (995KB / large row
-- count) exists in the bundle but produced no candidate rows filtered to zone 2796 for
-- this quest content -- no in-scope GO rows were found to author here.
--
-- Per Requirement 6: do NOT invent GameObject rows. This file is deliberately left with
-- no INSERT statements.
--
-- PHASE K RE-CAPTURE NOTE: if a future in-game pass identifies actual GameObject-type
-- props in the Arathi Catch-Up Experience (breakable crates, harvest nodes, siege props,
-- etc. distinct from the two questinteract creatures above), re-run TCHarvest against
-- zone 2796 with GO capture enabled and add a follow-up slice here -- this file's name is
-- reserved for that purpose.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 13_creature_loot_template.sql  <<<<<<<<<<<<<<<<<<<<

-- ADAPTED to integ_world 12.x schema (Arathi Catch-Up loot).
-- Schema mapping applied:
--   DROPPED `UPDATE creature_template SET lootid=entry`  (no lootid column in 12.x;
--            creature_loot_template is keyed directly by creature Entry)
--   DROPPED `Reference` column  (does not exist in integ_world creature_loot_template)
--   DROPPED `VerifiedBuild` column (also absent from this table in 12.x)
-- Real columns: Entry, ItemType, Item, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount, Comment
--
-- IDEMPOTENCY NOTE: integ_world's creature_loot_template has NO unique key
-- (idx_primary on (Entry,ItemType,Item) is NON_UNIQUE), so INSERT ... ON DUPLICATE KEY
-- UPDATE never fires and re-applying would duplicate rows. TC's own loot convention is
-- DELETE-then-INSERT scoped to the owned Entry set. These are brand-new content creatures
-- (244669/244674/244676/244677 -- spawned only on RPE map 2927), so ALL their loot is
-- RPE-owned; the scoped DELETE touches no base data.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (244669, 244674, 244676, 244677);

INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244674, 243573, 100.0, 1, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244676, 243573, 100.0, 1, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244677, 243573, 100.0, 1, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244674, 1376, 100.0, 0, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244676, 220232, 100.0, 0, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244669, 192617, 100.0, 0, 1, 0, 2, 2);
