--
-- Phase 10H - Major Factions: world-data seed (1/6) major_faction_config
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_<faction-id>_*.json (20 files) +
--         C:\dumps\MAJORFACTIONS_DATA_TOTALS.json
--
-- Creates the runtime configuration table loaded by MajorFactionMgr::LoadWorldData()
-- (see src/server/game/MajorFactions/MajorFactionMgr.cpp:168) and seeds 20 rows -
-- one per Major Faction listed in doc/major-factions/MAJOR_FACTIONS_PLAN.md S2.1:
--
--   Dragonflight 10.x: 2503, 2507, 2510, 2511, 2564, 2574
--   Plunderstorm 10.2.6: 2616 (Journey-mode, separate RenownRewardsPlunderstorm track)
--   The War Within 11.0:  2570, 2590, 2594, 2600
--   The War Within 11.1:  2653, 2688
--   The War Within 11.2:  2658, 2685
--   Midnight 12.0:        2696, 2699, 2704, 2710, 2792
--
-- introQuestId: Horde-side intro is used as the canonical row value. Faction
-- managers that need an Alliance variant select it from the same JSON
-- (introQuestIdAlliance) via team-aware logic; for 2507 (Dragonscale) the
-- Alliance value is 65436, Horde 65435 - we seed Horde here per spec.
-- All other factions are faction-neutral and use a single introQuestId.
--
-- uiPriority: Newer factions get higher priority so they sort first in the
-- in-game expansion-page panel (matches retail behaviour: TWW factions sit
-- above DF; Midnight will sit above TWW). Pre-existing patches that re-touch
-- these rows can adjust priorities without schema changes.
--
-- displayAsJourney is set for 2616 Keg Leg Thrasher (Plunderstorm): it uses
-- the RenownRewardsPlunderstorm.db2 table rather than the standard journey
-- panel, and the client renders it via Journey-mode UI (per warband
-- Phase 10C/10D, and the renown_rewards_rrp_db2 marker in JSON).
--

CREATE TABLE IF NOT EXISTS `major_faction_config` (
  `factionId`               int unsigned NOT NULL,
  `hiddenFromExpansionPage` tinyint(1) NOT NULL DEFAULT 0,
  `displayAsJourney`        tinyint(1) NOT NULL DEFAULT 0,
  `useJourneyRewardTrack`   tinyint(1) NOT NULL DEFAULT 0,
  `useJourneyUnlockToast`   tinyint(1) NOT NULL DEFAULT 0,
  `uiPriority`              int NOT NULL DEFAULT 0,
  `introQuestId`            int unsigned NOT NULL DEFAULT 0,
  `playerCompanionId`       int unsigned NOT NULL DEFAULT 0,
  `textureKit`              varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`factionId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `major_faction_config` WHERE `factionId` IN
    (2503,2507,2510,2511,2564,2574,2570,2590,2594,2600,2616,2653,2658,2685,2688,2696,2699,2704,2710,2792);

INSERT INTO `major_faction_config`
    (`factionId`,`hiddenFromExpansionPage`,`displayAsJourney`,`useJourneyRewardTrack`,`useJourneyUnlockToast`,`uiPriority`,`introQuestId`,`playerCompanionId`,`textureKit`) VALUES
-- Dragonflight 10.x (uiPriority 100..150; introQuestId = Horde variant where applicable; playerCompanionId left 0, see UNKNOWN list)
(2503, 0, 0, 0, 0, 100, 65795, 0, 'MajorFaction-MaruukCentaur'),         -- Maruuk Centaur          | DATA_2503 lines 25 + 21
(2507, 0, 0, 0, 0, 101, 65435, 0, 'MajorFaction-DragonscaleExpedition'), -- Dragonscale Expedition  | DATA_2507 lines 23 + 30 + tcSeedingSql.majorFactionConfig
(2510, 0, 0, 0, 0, 102, 66705, 0, 'MajorFaction-ValdrakkenAccord'),      -- Valdrakken Accord       | DATA_2510 lines 25 + 21
(2511, 0, 0, 0, 0, 103, 65566, 0, 'MajorFaction-IskaaraTuskarr'),        -- Iskaara Tuskarr         | DATA_2511 lines 25 + 21
(2564, 0, 0, 0, 0, 104, 75292, 0, 'MajorFaction-LoammNiffen'),           -- Loamm Niffen            | DATA_2564 lines 27 + 23
(2574, 0, 0, 0, 0, 105, 76558, 0, 'MajorFaction-DreamWardens'),          -- Dream Wardens           | DATA_2574 lines 27 + 23
-- Plunderstorm 10.2.6 (Journey-mode, separate renown track) - empty textureKit; client uses Plunderstorm-specific UI
(2616, 0, 1, 1, 1, 150, 78443, 0, 'MajorFaction-KegLegThrasher'),        -- Keg Leg Thrasher        | DATA_2616 line 100 (intro_quest_id 78443)
-- The War Within 11.0 (uiPriority 200+)
(2570, 0, 0, 0, 0, 200, 76246, 0, 'MajorFaction-HallowfallArathi'),      -- Hallowfall Arathi       | DATA_2570 line 164 (intro_quest_id)
(2590, 0, 0, 0, 0, 201, 76061, 0, 'MajorFaction-CouncilOfDornogal'),     -- Council of Dornogal     | DATA_2590 line 164
(2594, 0, 0, 0, 0, 202, 76365, 0, 'MajorFaction-AssemblyOfTheDeeps'),    -- Assembly of the Deeps   | DATA_2594 line 164
(2600, 0, 0, 0, 0, 203, 76502, 0, 'MajorFaction-SeveredThreads'),        -- Severed Threads         | DATA_2600 line 163
-- The War Within 11.1 / 11.2
(2653, 0, 0, 0, 0, 210, 85115, 0, 'MajorFaction-CartelsOfUndermine'),    -- Cartels of Undermine    | DATA_2653 line 209
(2688, 0, 0, 0, 0, 211, 86715, 0, 'MajorFaction-FlamesRadiance'),        -- Flame's Radiance        | DATA_2688 line 132
(2658, 0, 0, 0, 0, 220, 85116, 0, 'MajorFaction-KareshTrust'),           -- K'aresh Trust           | DATA_2658 line 132
(2685, 0, 0, 0, 0, 221, 85116, 0, 'MajorFaction-GallagioLoyalty'),       -- Gallagio Loyalty (raid) | DATA_2685 line 100
-- Midnight 12.0 (uiPriority 300+)
(2696, 0, 0, 0, 0, 300, 92010, 0, 'MajorFaction-AmaniTribe'),            -- Amani Tribe             | DATA_2696 line 379
(2699, 0, 0, 0, 0, 301, 92030, 0, 'MajorFaction-Singularity'),           -- The Singularity         | DATA_2699 line 379
(2704, 0, 0, 0, 0, 302, 92020, 0, 'MajorFaction-Harati'),                -- Hara'ti                 | DATA_2704 line 379
(2710, 0, 0, 0, 0, 303, 92040, 0, 'MajorFaction-SilvermoonCourt'),       -- Silvermoon Court        | DATA_2710 line 132
(2792, 0, 0, 0, 0, 310, 95390, 0, 'MajorFaction-RitualSites');           -- Ritual Sites (aux track)| DATA_2792 line 100 (intro_quest_id)
