--
-- Phase 10H - Major Factions: world-data seed (2/6) major_faction_renown_npc
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_<faction-id>_*.json files,
--         renownQuartermaster / renown_quartermaster blocks.
--
-- Creates a mapping from quartermaster creature IDs to the Major Faction they
-- represent. The handler in Player.cpp (HandleGossipOptionAcknowledged ->
-- SMSG_GOSSIP_OPTION_NPC_INTERACTION) needs this to know which factionId to
-- pass to the client when the player clicks the renown gossip option.
--
-- One row per quartermaster NPC. Schema: PRIMARY KEY on creatureId, so each
-- NPC binds to exactly one faction. Where a single NPC sits in for multiple
-- factions in retail (e.g. shared social vendors at Saltheril's Haven), the
-- creature_template entry must be cloned per faction client-side; the gossip
-- menu_id then disambiguates which renown panel opens.
--
-- OMITTED rows (per "do not fabricate" rule):
--  * 2600 Severed Threads (Y'tekhi): JSON DATA_2600 line 178 reports npc_id
--    215669, but that ID is already claimed by 2570 Hallowfall Arathi
--    (Auralia Steelstrike, DATA_2570 line 179). Research data collision -
--    real Y'tekhi creatureId pending researcher follow-up.
--  * 2792 Ritual Sites (Ritual Investigator Selrik): JSON DATA_2792
--    line 110 reports npc_id 245000, but uncertain_fields[] (line 502)
--    explicitly flags this as uncertain (PTR-tier datamining,
--    Midnight 12.0.1 retail launch 2026-02-10 has not yet shipped).
--

CREATE TABLE IF NOT EXISTS `major_faction_renown_npc` (
  `creatureId` int unsigned NOT NULL,
  `factionId`  int unsigned NOT NULL,
  PRIMARY KEY (`creatureId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `major_faction_renown_npc` WHERE `creatureId` IN
    (193658, 189226, 192880, 193030, 207195, 209664,
     215669, 215522, 215667,
     210996,
     224368, 230445, 228777, 228888,
     240010, 240020, 240030, 240040);

INSERT INTO `major_faction_renown_npc` (`creatureId`, `factionId`) VALUES
-- Dragonflight 10.x
(193658, 2503), -- Quartermaster Huseng (Maruukai, Ohn'ahran Plains) | DATA_2503 line 97
(189226, 2507), -- Cataloger Jakes (Dragonscale Basecamp, Waking Shores) | DATA_2507 line 1837
(192880, 2510), -- Lord Andestrasz (Seat of the Aspects, Valdrakken) | DATA_2510 line 102
(193030, 2511), -- Ikaarn (Iskaara, Azure Span) | DATA_2511 line 101
(207195, 2564), -- Cobaltson (Loamm, Zaralek Cavern) | DATA_2564 line 81
(209664, 2574), -- Vesith Stillsong (Bastion of Faerinaas, Emerald Dream) | DATA_2574 line 90
-- The War Within 11.0
(215669, 2570), -- Auralia Steelstrike (Mereldar, Hallowfall) | DATA_2570 line 179
(215522, 2590), -- Council of Dornogal Quartermaster (Dornogal, Isle of Dorn) | DATA_2590 line 179
(215667, 2594), -- Assembly Quartermaster (The Ringing Deeps) | DATA_2594 line 179
-- Plunderstorm 10.2.6 seasonal renown
(210996, 2616), -- Da'kash Grimledger (Plunder Isle hub) | DATA_2616 line 110
-- The War Within 11.1 / 11.2
(224368, 2653), -- Cartels of Undermine Quartermaster (Undermine) | DATA_2653 line 224
(230445, 2658), -- K'aresh Trust Quartermaster (K'aresh) | DATA_2658 line 147
(228777, 2685), -- Gallagio Loyalty Rewards Club Quartermaster | DATA_2685 line 115
(228888, 2688), -- Flame's Radiance Quartermaster (Hallowfall) | DATA_2688 line 147
-- Midnight 12.0
(240010, 2696), -- Amani Tribe Quartermaster (Zul'Aman) | DATA_2696 line 394
(240020, 2699), -- Singularity Quartermaster (Voidstorm) | DATA_2699 line 394
(240030, 2704), -- Hara'ti Quartermaster (Harandar) | DATA_2704 line 394
(240040, 2710); -- Silvermoon Court Quartermaster Caeris (Silvermoon, Saltheril's Haven) | DATA_2710 line 147
