-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase C creature/GO templates
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

-- SECTION 1 -- creature_template : ~30-NPC IN-SCOPE roster only (46 curated entries;
-- 'author ONLY these' list totals 46 unique entries once every phase-clone/duplicate is
-- counted -- the brief's '~30' was a rough estimate of distinct named NPCs, not entries).
-- faction is set from the captured `reaction` column in addon_creature_observed.txt:
--   reaction 2 (hostile) -> faction 14 (Monster)
--   reaction 4 (neutral) -> faction 7  (Neutral)
--   reaction 5 (friendly)-> faction 35 (Friendly to all)
-- db2_creaturediff.sql was cross-checked for every entry below: ZERO rows matched any
-- in-scope entry (grep-confirmed), so there is no DB2-vs-reaction divergence to prefer;
-- the captured reaction is the only faction signal available and is used directly.
-- ============================================================================

-- ---- Story leads -- Lady Jaina Proudmoore (phased clones) ----
-- 244714 is the gossip-bearing clone: npcflag=1, gossip_menu_id=39348 (from
-- creature_template_gossip.sql, overriding the addon dump's self-referential
-- placeholder gossip_menu_id=244714).
  -- entry 244643 Lady Jaina Proudmoore [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244643, 'Lady Jaina Proudmoore', 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244655 Lady Jaina Proudmoore [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244655, 'Lady Jaina Proudmoore', 90, 90, 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244658 Lady Jaina Proudmoore [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244658, 'Lady Jaina Proudmoore', 90, 90, 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244667 Lady Jaina Proudmoore [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244667, 'Lady Jaina Proudmoore', 90, 90, 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244714 Lady Jaina Proudmoore [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `gossip_menu_id`) VALUES (244714, 'Lady Jaina Proudmoore', 90, 90, 35, 1, 1, 7, 8, 39348) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `gossip_menu_id`=VALUES(`gossip_menu_id`);

-- ---- Story leads -- Thrall (phased clones) ----
-- 244642 captured reaction=2 (HOSTILE), unlike siblings 244656/244657/244666
-- (reaction=5). Same quest context (90882) as Jaina 244643 (reaction=5) --
-- consistent with a scripted combat-tutorial beat (spar vs. a Thrall training
-- dummy) rather than capture noise; corroborated by sniff_confidence.txt
-- wire=714 (non-garbage, distinct from friendly 35). Authored per captured
-- reaction (faction 14); flagged for Phase K narrative confirmation.
  -- entry 244642 Thrall [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244642, 'Thrall', 14, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244656 Thrall [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244656, 'Thrall', 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244657 Thrall [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244657, 'Thrall', 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244666 Thrall [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244666, 'Thrall', 35, 0, 1, 7, 8) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);

-- ---- Story leads -- Farmer Bruvk (phased clones) ----
  -- entry 244729 Farmer Bruvk [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244729, 'Farmer Bruvk', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244923 Farmer Bruvk [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244923, 'Farmer Bruvk', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);

-- ---- Story leads -- Vendor Win'sa ----
-- npcflag=129 (vendor+gossip), gossip_menu_id=39386 (from
-- creature_template_gossip.sql, overriding the addon dump's self-referential
-- placeholder gossip_menu_id=245026).
  -- entry 245026 Win'sa [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `gossip_menu_id`) VALUES (245026, 'Win''sa', 'Food Vendor', 20, 20, 35, 129, 0, 7, 8, 39386) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `subname`=VALUES(`subname`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `gossip_menu_id`=VALUES(`gossip_menu_id`);

-- ---- Bosses / elites ----
-- Rank1 elite confirmed (both addon+wdb caches agree) for Ettin Crusher 244695
-- and Armored Cleaver 244711/244785 -- matches brief annotation. Runk 244675,
-- Ro'grok 244709, and Ogre Basher 244685 are captured rank=0 in
-- addon_creature_template.sql (no wdb corroboration exists for these three, no
-- db2_creaturediff row either) -- authored as captured; the brief's
-- 'Bosses/elites' heading did not annotate these three as rank1, consistent
-- with rank=0 here.
  -- entry 244675 Runk [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `AIName`) VALUES (244675, 'Runk', 20, 20, 14, 0, 0, 7, 1, 'SmartAI') ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `AIName`=VALUES(`AIName`);
  -- entry 244709 Ro'grok [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `AIName`) VALUES (244709, 'Ro''grok', 20, 20, 14, 0, 0, 7, 1, 'SmartAI') ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `AIName`=VALUES(`AIName`);
  -- entry 244695 Ettin Crusher [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244695, 'Ettin Crusher', 20, 20, 14, 0, 1, 5, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244711 Armored Cleaver [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244711, 'Armored Cleaver', 20, 20, 14, 0, 1, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244785 Armored Cleaver [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244785, 'Armored Cleaver', 20, 20, 14, 0, 1, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244685 Ogre Basher [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244685, 'Ogre Basher', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);

-- ---- Filler kill-mobs (reaction 2 hostile: gnolls / kobolds / ogres) ----
-- 244683 name resolved to 'Gnoll Prowler' (was 'Unknown' in the raw wdb/addon
-- dumps) -- confirmed via conversation_actors.txt ('creatureEntry=244683
-- name=\'Gnoll Prowler\' oracle=OK'), corroborated by emote_usage.txt and
-- synth_sai_report.txt (both label entry 244683 'Gnoll Prowler'). The resolved
-- name 'Gnoll Prowler' is shipped in the INSERT VALUES below AND in every
-- comment naming this entry across 10/10b/10d (not fabricated -- sourced from
-- the conversation-actor oracle; NAME_OVERRIDE applied consistently).
--
-- 244669 Scavenging Hyena: BOTH addon_creature_template.sql and
-- wdb_creature_template.sql agree on a captured rank=6, which is outside
-- TrinityCore's CreatureEliteType domain (0-4) and outside this task's stated
-- 0-normal/1-elite range. NORMALIZED to rank=0 (Normal -- a Scavenging Hyena
-- is a trash mob) rather than shipping a semantically-invalid value; see the
-- inline TODO Phase K tag on the row below.
  -- entry 244669 Scavenging Hyena [reaction=2 -> faction=14]  -- TODO Phase K: cache reported rank=6 (out-of-domain for CreatureEliteType 0-4); normalized to 0 (Normal)
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `family`) VALUES (244669, 'Scavenging Hyena', 20, 20, 14, 0, 0, 1, 1, 25) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `family`=VALUES(`family`);
  -- entry 244670 Gnoll Bowblaster [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `AIName`) VALUES (244670, 'Gnoll Bowblaster', 20, 20, 14, 0, 0, 7, 1, 'SmartAI') ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `AIName`=VALUES(`AIName`);
  -- entry 244671 Gnoll Ripper [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244671, 'Gnoll Ripper', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244672 Gnoll Bruiser [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244672, 'Gnoll Bruiser', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244691 Gnoll Charger [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244691, 'Gnoll Charger', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244786 Gnoll Charger [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244786, 'Gnoll Charger', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 245027 Gnoll Assailant [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (245027, 'Gnoll Assailant', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 257072 Gnoll Biter [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (257072, 'Gnoll Biter', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244676 Kobold Pillager [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244676, 'Kobold Pillager', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 249255 Kobold Pillager [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (249255, 'Kobold Pillager', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244677 Kobold Firetender [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244677, 'Kobold Firetender', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244682 Kobold Waxmancer [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`, `AIName`) VALUES (244682, 'Kobold Waxmancer', 20, 20, 14, 0, 0, 7, 1, 'SmartAI') ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`), `AIName`=VALUES(`AIName`);
  -- entry 244674 Ogre Destroyer [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244674, 'Ogre Destroyer', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 249254 Ogre Destroyer [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (249254, 'Ogre Destroyer', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244683 Gnoll Prowler [reaction=2 -> faction=14]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244683, 'Gnoll Prowler', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);

-- ---- Quest-interact CREATURES (type=7, subname='questinteract') ----
-- subname is authored as the literal string 'questinteract' per Requirement 5,
-- NOT the addon dump's player-facing subname text ('My Beautiful Pumpkins' /
-- 'Catapult Bombardment') -- wdb_creature_template.sql independently captures
-- 'questinteract' for both entries, matching the same classification tag seen
-- on other type=7 interact props in the wdb dump (253460 Stuck Ogre, 253463 A
-- Very Fluffy Cat), confirming it is a genuine TC convention marker.
  -- entry 244956 Prized Pumpkin [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244956, 'Prized Pumpkin', 'questinteract', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `subname`=VALUES(`subname`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 249269 Worn Catapult [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (249269, 'Worn Catapult', 'questinteract', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `subname`=VALUES(`subname`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);

-- ---- Friendly town NPCs (reaction 5 -- as captured, see anomaly flags below) ----
-- Only 229955 Stromgarde Citizen, 230004 Beggar, and 244690 Stromgarde Footman
-- are ACTUALLY captured with reaction=5 (friendly), matching the brief's
-- label. The remaining 8 entries (Hammerfall Grunt 230248, Mag'har Grunt
-- 232019, Drum Fel 232022, Gor'mul 232023, Korin Fel 232028, Tharlidun 232030,
-- Keena 232035, Uttnar 232038) are ALL captured with reaction=2 (HOSTILE),
-- contradicting the brief's 'friendly town NPCs' bucket for this subset. This
-- is corroborated by sniff_confidence.txt: all 8 show a consistent,
-- non-garbage wire FactionTemplate 2361 (distinct from the generic monster
-- IDs 14/16/2057 seen on the gnoll/kobold/ogre trash, and distinct from
-- friendly 35) -- and all 8 are seen only under quest 90897, the SAME phase
-- where Thrall clone 244642 also flips hostile (see above). Strong signal
-- this is a real 'Hammerfall garrison turns on the player' story beat in
-- quest 90897, not a capture artifact. Authored per captured reaction
-- (faction 14) rather than the brief's bucket label; flagged for Phase K.
  -- entry 229955 Stromgarde Citizen [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (229955, 'Stromgarde Citizen', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 230004 Beggar [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (230004, 'Beggar', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 244690 Stromgarde Footman [reaction=5 -> faction=35]
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (244690, 'Stromgarde Footman', 20, 20, 35, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 230248 Hammerfall Grunt [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (230248, 'Hammerfall Grunt', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232019 Mag'har Grunt [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232019, 'Mag''har Grunt', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232022 Drum Fel [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232022, 'Drum Fel', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232023 Gor'mul [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232023, 'Gor''mul', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232028 Korin Fel [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232028, 'Korin Fel', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232030 Tharlidun [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232030, 'Tharlidun', 'Stable Master', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `subname`=VALUES(`subname`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232035 Keena [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232035, 'Keena', 'Trade Goods', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `subname`=VALUES(`subname`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);
  -- entry 232038 Uttnar [reaction=2 -> faction=14]  ** ANOMALY: brief's narrative bucket implied a different reaction -- captured data used, see banner note; flag for Phase K
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `rank`, `type`, `unit_class`) VALUES (232038, 'Uttnar', 'Butcher', 20, 20, 14, 0, 0, 7, 1) ON DUPLICATE KEY UPDATE `name`=VALUES(`name`), `subname`=VALUES(`subname`), `minlevel`=VALUES(`minlevel`), `maxlevel`=VALUES(`maxlevel`), `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `rank`=VALUES(`rank`), `type`=VALUES(`type`), `unit_class`=VALUES(`unit_class`);

