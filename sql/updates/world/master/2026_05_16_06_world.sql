--
-- Phase 10H - Major Factions: world-data seed (6/6) weekly community quest pools
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_<faction-id>_*.json
--         (weeklyCommunityQuests / weekly_community_quests / weeklyQuests blocks)
--
-- Registers per-faction WEEKLY-rotation quest pools using the existing
-- quest_pool_template + quest_pool_members tables (see
-- src/server/game/Pools/QuestPools.cpp:80).
--
-- Pool semantics:
--   * Each quest_pool_template row defines a pool with a numActive value -
--     the number of quest indices that are "live" each weekly reset.
--   * quest_pool_members maps individual questIds into the pool with a
--     poolIndex (multiple quests sharing an index rotate as a group).
--
-- poolId reservation: 7001..7019. Range previously unused.
--
-- OMITTED weekly quests (per "do not fabricate" rule):
--   * Any quest entry where the JSON questId is null / "approxQuestId": null
--     (notably 2507's "Siege on Dragonbane Keep weekly" and "Researchers
--      Under Fire" entries at lines 2014-2022, listed without questIds).
--   * 2792 Ritual Sites - DATA_2792.uncertain_fields[] (line 506)
--     explicitly flags weekly_community_quests[].quest_id as uncertain.
--   * 2503 Maruuk Centaur - the only weekly entry with a numeric questId
--     (70750 "Aiding the Accord") is itself flagged "removed 10.2.6a" in the
--     JSON (line 126), so its pool would be empty.
--   * 2510 / 2511 / 2564 / 2574 - their JSON weekly entries either lack
--     questIds or share quest 70750 (removed-status). Their world events
--     (Aerial Challenges, Community Feast, Researchers Under Fire,
--     Superbloom) are world-event objectives rather than rotating weekly
--     quests, so they belong in a different system (world_event_quest /
--     scenario template) and are intentionally not pooled here.
--
-- Each faction's weekly quests get their own pool. Cross-faction shared
-- weeklies (e.g. 82898 "Auditor Biergoth's Dungeon Quest", which awards rep
-- to a player-chosen faction) get one canonical pool (7019) and are NOT
-- duplicated across per-faction pools - they would conflict on questId
-- (PRIMARY KEY on quest_pool_members.questId).
--

-- ---------------------------------------------------------------------
-- pool_template rows: numActive = number of weekly variants live at once
-- ---------------------------------------------------------------------
DELETE FROM `quest_pool_template` WHERE `poolId` BETWEEN 7001 AND 7019;
INSERT INTO `quest_pool_template` (`poolId`,`numActive`,`description`) VALUES
-- Dragonflight 10.x faction weeklies (Aiding the Accord variants - 2507 only)
(7001, 1, 'Dragonscale Expedition: Aiding the Accord weekly rotation (8 variants)'),
-- TWW 11.0 faction-specific weeklies (one per faction)
(7007, 1, 'Hallowfall Arathi: Spreading the Light + Beledar (2 variants)'),
(7008, 1, 'Council of Dornogal: Theater Troupe weekly (single variant)'),
(7009, 1, 'Assembly of the Deeps: Awakening the Machine weekly (single variant)'),
(7010, 1, 'Severed Threads: Pact of General/Vizier/Weaver rotation (3 variants, 1 live per reset)'),
-- TWW 11.1 / 11.2
(7011, 1, 'Cartels of Undermine: Side Gig + Reefwalker (2 variants)'),
(7012, 1, 'K''aresh Trust: Project Eco-Dome + Ethereal Plane Tracker (2 variants)'),
(7013, 1, 'Gallagio Loyalty Rewards Club: Liberation of Undermine first-clear (raid weekly)'),
(7014, 1, 'Flame''s Radiance: Nightfall + Repelling Tides (2 variants)'),
-- Midnight 12.x
(7015, 1, 'Amani Tribe: Abundance weekly (single variant)'),
(7016, 1, 'The Singularity: Stormarion Assault weekly (single variant)'),
(7017, 1, 'Hara''ti: Legends of the Haranir weekly (single variant)'),
(7018, 1, 'Silvermoon Court: Saltheril''s Soirees + Quel''Danas (2 variants)'),
-- Cross-faction shared weeklies
(7019, 1, 'Shared TWW/Midnight universal weekly dungeon (Biergoth 82898 / Brightwing 92600 / dispatcher quest pool)');

-- ---------------------------------------------------------------------
-- quest_pool_members: questId -> poolId mapping.
-- ---------------------------------------------------------------------
DELETE FROM `quest_pool_members` WHERE `poolId` BETWEEN 7001 AND 7019;

-- 7001 Dragonscale Expedition (DATA_2507 lines 1972-2007, "Aiding the Accord" + variants).
-- Each variant rotates weekly; indices 0..7 cycle.
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(72068, 7001, 0, 'Aiding the Accord (DF10) [2507 weekly variant 0]'),
(72782, 7001, 1, 'Aiding the Accord: A Feast For All [2507 weekly variant 1]'),
(73055, 7001, 2, 'Aiding the Accord: A Tale of Two Tarrasques [2507 weekly variant 2]'),
(75259, 7001, 3, 'Aiding the Accord: A Worthy Ally [2507 weekly variant 3]'),
(73294, 7001, 4, 'Aiding the Accord: Storms Brewing [2507 weekly variant 4]'),
(72651, 7001, 5, 'Aiding the Accord: The Hunt is On [2507 weekly variant 5]'),
(73061, 7001, 6, 'Aiding the Accord: Trial of Elements [2507 weekly variant 6]'),
(73062, 7001, 7, 'Aiding the Accord: Trial of Flood [2507 weekly variant 7]');

-- 7007 Hallowfall Arathi (DATA_2570 lines 1020-1040).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(82938, 7007, 0, 'Spreading the Light (Hallowfall weekly) [2570]'),
(79984, 7007, 1, 'Hallowfall Beledar weekly [2570]');

-- 7008 Council of Dornogal (DATA_2590 lines 1052-1058).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(81381, 7008, 0, 'Theater Troupe (Dornogal weekly) [2590]');

-- 7009 Assembly of the Deeps (DATA_2594 lines 1074-1080).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(82511, 7009, 0, 'Awakening the Machine (Ringing Deeps weekly) [2594]');

-- 7010 Severed Threads Pact rotation (DATA_2600 lines 1448-1467).
-- Pact rotation: one Pact variant active each weekly reset (the player chooses
-- their Pact, the other two are locked) - hence numActive=1 on the 3-member
-- pool: the quest pool system will pick one index per cycle.
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(80571, 7010, 0, 'Pact of the General [2600 Severed Threads]'),
(80572, 7010, 1, 'Pact of the Vizier [2600 Severed Threads]'),
(80573, 7010, 2, 'Pact of the Weaver [2600 Severed Threads]');

-- 7011 Cartels of Undermine (DATA_2653 lines 937-957).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(85120, 7011, 0, 'Side Gig (rotating cartel) [2653 Undermine]'),
(85220, 7011, 1, 'Reefwalker tracker (world boss weekly) [2653 Undermine]');

-- 7012 K'aresh Trust (DATA_2658 lines 778-799).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
-- NOTE: K'aresh and Undermine both list quest_id 85120 - K'aresh attaches it to
-- "Project Eco-Dome", Undermine attaches it to "Side Gig". Researcher data
-- error (likely a JSON copy-paste). Since quest_pool_members.questId is
-- PRIMARY KEY, we cannot duplicate. Omit 85120 here; the row above in pool
-- 7011 wins. Kareh's Ethereal Plane Tracker (85230) is the canonical 2658 row.
(85230, 7012, 0, 'Ethereal Plane Tracker [2658 K''aresh]');

-- 7013 Gallagio Loyalty Rewards Club raid weekly (DATA_2685 line 831).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(86200, 7013, 0, 'Liberation of Undermine first-clear bonus [2685 Gallagio raid weekly]');

-- 7014 Flame's Radiance (DATA_2688 lines 705-719).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(86730, 7014, 0, 'Nightfall scenario first weekly clear bonus [2688 Flame''s Radiance]'),
(86740, 7014, 1, 'Repelling Tides weekly [2688 Flame''s Radiance]');

-- 7015 Amani Tribe (DATA_2696 lines 1062-1068).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(92520, 7015, 0, 'Abundance (Midnight weekly) [2696 Amani Tribe]');

-- 7016 The Singularity (DATA_2699 lines 1106-1112).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(92530, 7016, 0, 'Stormarion Assault (Midnight weekly) [2699 Singularity]');

-- 7017 Hara'ti (DATA_2704 lines 1156-1162).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(92540, 7017, 0, 'Legends of the Haranir (weekly) [2704 Hara''ti]');

-- 7018 Silvermoon Court (DATA_2710 lines 838-858).
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(92550, 7018, 0, 'Saltheril''s Soirees (weekly) [2710 Silvermoon Court]'),
(92560, 7018, 1, 'March on Quel''Danas (weekly raid prelude) [2710 Silvermoon Court]');

-- 7019 Cross-faction shared TWW/Midnight universal weekly dungeon.
-- Per the JSON data: 82898 is Auditor Biergoth's dispatch (TWW 11.0/11.1/11.2);
-- 92600 is Halduron Brightwing's Midnight equivalent. Both feed rep to a
-- player-chosen faction, hence single shared pool.
INSERT INTO `quest_pool_members` (`questId`,`poolId`,`poolIndex`,`description`) VALUES
(82898, 7019, 0, 'Auditor Biergoth''s Dungeon Quest (TWW universal weekly dungeon)'),
(82900, 7019, 1, 'Khaz Algar Bounty Hunter (TWW weekly world bounty)'),
(92600, 7019, 2, 'Halduron Brightwing''s Dungeon Quest (Midnight universal weekly dungeon)');
