-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H Win'sa vendor
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_npc_vendor.sql
--   (22 raw rows: 5 for Win'sa 245026 + 17 for entry 5188). Entry 5188 is the Chromie-hub
--   Timewalking-tabard vendor (see addon_npc_context.txt / addon_gossip_menu.sql
--   menuid=5188 "Let me browse your goods." / "I've lost my Tabard of the Explorer.")
--   -- NOT part of the Catch-Up Experience roster (not one of Task-1's authored
--   creature_template entries). EXCLUDED per task-6-brief Req.1.
--
-- Column mapping note: addon_npc_vendor.sql's raw capture columns
-- (entry, item, slot, price, maxcount, extendedcost, type[, cost]) are the TCHarvest
-- addon's own shorthand, NOT the literal `npc_vendor` schema -- there is no `price`
-- column in `npc_vendor` (PK is entry,item,ExtendedCost,type; the client-displayed price
-- is item_template.BuyPrice, set elsewhere / already on baseline items). `price` is
-- therefore DROPPED here (informational-only in the capture); `incrtime` was not
-- captured and defaults to 0, matching maxcount=0 (unlimited stock, no restock timer
-- needed).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================

-- Win'sa (245026), Food Vendor -- npcflag=129 (gossip+vendor) already on the Task-1
-- creature_template row (10_creature_template.sql); this file only supplies the wares.
INSERT INTO `npc_vendor` (`entry`, `item`, `slot`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `VerifiedBuild`) VALUES
(245026, 197858, 1, 0, 0, 0, 1, 69382), -- slot 1, captured price 25000c
(245026, 197857, 2, 0, 0, 0, 1, 69382), -- slot 2, captured price 25000c
(245026, 197855, 3, 0, 0, 0, 1, 69382), -- slot 3, captured price 37500c
(245026, 197856, 4, 0, 0, 0, 1, 69382), -- slot 4, captured price 25000c
(245026, 194680, 5, 0, 0, 0, 1, 69382)  -- slot 5, captured price 37500c
ON DUPLICATE KEY UPDATE `slot`=VALUES(`slot`), `maxcount`=VALUES(`maxcount`), `incrtime`=VALUES(`incrtime`), `VerifiedBuild`=VALUES(`VerifiedBuild`);
