-- Pull DoomCore-derived delve metadata into the TC delve pipeline.
--
-- Source: stevebone/DoomCore drop integrated into agatho/StefalWoW (`origin/master`),
-- specifically `src/server/scripts/Custom/DoomcoreCustom/Delves/DelveData.h` and
-- the `sql/custom/world/Delves/{AtalAman,TheShadowEnclave}/*_setup.sql` files.
-- Sniff origin: 12.0.1.66527 packet captures.
--
-- This migration:
--   1. Extends `delve_template` with the gossip-based tier-selection fields
--      that the retail client uses to render the Blizzard_DelvesDifficultyPicker
--      (LfgDungeonsID + per-tier gossip option SpellIDs).
--   2. Adds rows for the two Midnight delves the DoomCore drop fully captured —
--      Atal'Aman (Map 2962) and The Shadow Enclave (Map 2952).
--   3. Registers the (map, difficulty=208) → scenario mapping in the standard
--      TC `scenarios` world table for ScenarioMgr.
--
-- The 11 universal tier-scaling spells (1260938..1260973) are NOT stored in this
-- table — they live as a constexpr array in DelvesDefines.h::TIER_SPELL_IDS,
-- because they're identical across every delve.

-- ---------------------------------------------------------------------------
-- 1. Schema extension
-- ---------------------------------------------------------------------------

ALTER TABLE `delve_template`
    ADD COLUMN `gossipMenuId`            int unsigned NOT NULL DEFAULT '0' AFTER `factionId`,
    ADD COLUMN `lfgDungeonsId`           int unsigned NOT NULL DEFAULT '0' AFTER `gossipMenuId`,
    ADD COLUMN `broadcastTextId`         int unsigned NOT NULL DEFAULT '0' AFTER `lfgDungeonsId`,
    ADD COLUMN `firstTierGossipOptionId` int unsigned NOT NULL DEFAULT '0' AFTER `broadcastTextId`,
    ADD COLUMN `entryX`                  float        NOT NULL DEFAULT '0' AFTER `firstTierGossipOptionId`,
    ADD COLUMN `entryY`                  float        NOT NULL DEFAULT '0' AFTER `entryX`,
    ADD COLUMN `entryZ`                  float        NOT NULL DEFAULT '0' AFTER `entryY`,
    ADD COLUMN `entryO`                  float        NOT NULL DEFAULT '0' AFTER `entryZ`,
    ADD COLUMN `exitX`                   float        NOT NULL DEFAULT '0' AFTER `entryO`,
    ADD COLUMN `exitY`                   float        NOT NULL DEFAULT '0' AFTER `exitX`,
    ADD COLUMN `exitZ`                   float        NOT NULL DEFAULT '0' AFTER `exitY`,
    ADD COLUMN `exitO`                   float        NOT NULL DEFAULT '0' AFTER `exitZ`,
    ADD COLUMN `activeScenarioId`        int unsigned NOT NULL DEFAULT '0' AFTER `exitO`,
    ADD COLUMN `rewardScenarioId`        int unsigned NOT NULL DEFAULT '0' AFTER `activeScenarioId`,
    ADD COLUMN `worldState26903`         int unsigned NOT NULL DEFAULT '0' AFTER `rewardScenarioId`;

-- ---------------------------------------------------------------------------
-- 2. Midnight delves — Atal'Aman and The Shadow Enclave
-- ---------------------------------------------------------------------------
-- All field values lifted verbatim from DoomCore's DELVE_TABLE[] in DelveData.h.
-- Zone IDs cross-checked against doc/research/09_DELVE_MAP_SCENARIO_IDS.md.

INSERT INTO `delve_template` (
    `id`, `mapId`, `scenarioId`, `mapChallengeModeId`, `zoneId`, `factionId`,
    `gossipMenuId`, `lfgDungeonsId`, `broadcastTextId`, `firstTierGossipOptionId`,
    `entryX`, `entryY`, `entryZ`, `entryO`,
    `exitX`, `exitY`, `exitZ`, `exitO`,
    `activeScenarioId`, `rewardScenarioId`, `worldState26903`
) VALUES
-- Atal'Aman (Midnight, Halls of Atal'Aman zone)
(19, 2962, 3147, 0, 16556, 0,
    39751, 3025, 292789, 134444,
    5134.376, -5861.9463, 217.18855, 0.017530434,
    5121.0,   -5861.4,    217.1,     3.22,
    3147, 3424, 1278258),
-- The Shadow Enclave (Midnight, K'aresh shadow zone)
(20, 2952, 3154, 0, 16594, 0,
    40277, 3069, 296637, 135336,
    -17.399,  233.831,  265.446, 3.678,
    4781.7,  -4120.3,    31.3,   0.0,
    3154, 3424, 1265777);

-- ---------------------------------------------------------------------------
-- 3. Scenario routing for ScenarioMgr
-- ---------------------------------------------------------------------------
-- (map, difficulty=208) → scenario_A / scenario_H
-- Delves are faction-neutral, so scenario_A == scenario_H.

INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES
(2962, 208, 3147, 3147),
(2952, 208, 3154, 3154)
ON DUPLICATE KEY UPDATE
    `scenario_A` = VALUES(`scenario_A`),
    `scenario_H` = VALUES(`scenario_H`);
