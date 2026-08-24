--
-- Phase 10H - Major Factions: world-data seed (1/6) major_faction_config
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_<faction-id>_*.json (20 files) +
--         C:\dumps\MAJORFACTIONS_DATA_TOTALS.json
--
-- Creates the runtime configuration table loaded by MajorFactionMgr::LoadWorldData()
-- (see src/server/game/MajorFactions/MajorFactionMgr.cpp) and seeds 20 rows -
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
-- panel, and the client renders it via Journey-mode UI.
--
-- renownCampaignId: Phase 10L change. Replaces the previously-stored
-- textureKit string column with a foreign key into Campaign.db2. The
-- textureKit prefix is now resolved at read time via the chain
--   faction -> renownCampaignId -> Campaign.UiTextureKitID -> UiTextureKit.KitPrefix
-- (see MajorFactionMgr::GetTextureKitPrefix), so the canonical Blizzard
-- DB2 string is sourced from Campaign.db2 / UiTextureKit.db2 rather than
-- duplicated here. A renownCampaignId of 0 means "no associated campaign"
-- (Plunderstorm, Ritual Sites, Gallagio raid-only).
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
  `renownCampaignId`        int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`factionId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `major_faction_config` WHERE `factionId` IN
    (2503,2507,2510,2511,2564,2574,2570,2590,2594,2600,2616,2653,2658,2685,2688,2696,2699,2704,2710,2792);

INSERT INTO `major_faction_config`
    (`factionId`,`hiddenFromExpansionPage`,`displayAsJourney`,`useJourneyRewardTrack`,`useJourneyUnlockToast`,`uiPriority`,`introQuestId`,`playerCompanionId`,`renownCampaignId`) VALUES
-- Dragonflight 10.x. renownCampaignId pulled from each JSON's `campaign.id` block.
(2503, 0, 0, 0, 0, 100, 65795, 0, 166), -- Maruuk Centaur          | DATA_2503 campaign.id=166 (Ohn'ahran Plains)
(2507, 0, 0, 0, 0, 101, 65435, 0, 197), -- Dragonscale Expedition  | DATA_2507 renownCampaignId=197 (intro Campaign 165 separate)
(2510, 0, 0, 0, 0, 102, 66705, 0, 189), -- Valdrakken Accord       | DATA_2510 campaign.id=189 (Thaldraszus)
(2511, 0, 0, 0, 0, 103, 65566, 0, 174), -- Iskaara Tuskarr         | DATA_2511 campaign.id=174 (Azure Span)
(2564, 0, 0, 0, 0, 104, 75292, 0, 203), -- Loamm Niffen            | DATA_2564 campaign.id=203 (Embers of Neltharion) - chapters unmapped in CampaignXQuestLine
(2574, 0, 0, 0, 0, 105, 76558, 0, 231), -- Dream Wardens           | DATA_2574 campaign.id=231 (Guardians of the Dream)
-- Plunderstorm 10.2.6 (no campaign - dedicated Plunderstorm mode)
(2616, 0, 1, 1, 1, 150, 78443, 0,   0), -- Keg Leg Thrasher        | DATA_2616 campaign_id=0
-- The War Within 11.0
(2570, 0, 0, 0, 0, 200, 76246, 0, 238), -- Hallowfall Arathi       | DATA_2570 campaign_id=238
(2590, 0, 0, 0, 0, 201, 76061, 0, 236), -- Council of Dornogal     | DATA_2590 campaign_id=236 (Isle of Dorn)
(2594, 0, 0, 0, 0, 202, 76365, 0, 237), -- Assembly of the Deeps   | DATA_2594 campaign_id=237 (The Ringing Deeps)
(2600, 0, 0, 0, 0, 203, 76502, 0, 239), -- Severed Threads         | DATA_2600 campaign_id=239 (Azj-Kahet)
-- The War Within 11.1 / 11.2
(2653, 0, 0, 0, 0, 210, 85115, 0, 264), -- Cartels of Undermine    | DATA_2653 campaign_id=264 (Undermine, 6 chapters)
(2688, 0, 0, 0, 0, 211, 86715, 0, 267), -- Flame's Radiance        | DATA_2688 campaign_id=267 (Rise of the Red Dawn) [shared w/ Silvermoon]
(2658, 0, 0, 0, 0, 220, 85116, 0, 268), -- K'aresh Trust           | DATA_2658 campaign_id=268 (Lorewalking: Ethereals)
(2685, 0, 0, 0, 0, 221, 85116, 0,   0), -- Gallagio Loyalty (raid) | DATA_2685 campaign_id=0 (no campaign - raid lockout only)
-- Midnight 12.0
(2696, 0, 0, 0, 0, 300, 92010, 0, 270), -- Amani Tribe             | DATA_2696 campaign_id=270 (Midnight - shared 17-chapter campaign)
(2699, 0, 0, 0, 0, 301, 92030, 0, 270), -- The Singularity         | DATA_2699 campaign_id=270 (shared)
(2704, 0, 0, 0, 0, 302, 92020, 0, 270), -- Hara'ti                 | DATA_2704 campaign_id=270 (shared)
(2710, 0, 0, 0, 0, 303, 92040, 0, 267), -- Silvermoon Court        | DATA_2710 campaign_id=267 (shared w/ Flame's Radiance)
(2792, 0, 0, 0, 0, 310, 95390, 0,   0); -- Ritual Sites (aux track)| DATA_2792 campaign_id=0 (no campaign - PTR-flagged uncertain)
