-- Append Season 2 / Season 3 delves to delve_template
--
-- Source: doc/research/09_DELVE_MAP_SCENARIO_IDS.md (DB2-extracted from wago.tools)
--   cross-checked against doc/research/delves_content_catalog.md (Wowhead/Icy Veins,
--   compiled 2026-04-28).
--
-- All entries continue to use:
--   DifficultyID = 208 ("Delves", 1-5 players, ItemContext 9)
--   ScenarioType = 8
--   MapChallengeModeId = 0  (delves never use MCM, confirmed: zero MCM entries
--                            for any delve map across IDs 2 through 583)
--
-- factionId left at 0 — numeric Faction.db2 IDs for the four TWW reputations
-- (Council of Dornogal, Assembly of the Deeps, Hallowfall Arathi, The Severed
-- Threads) plus the new K'aresh / Undermine factions are not yet authoritatively
-- captured. Populate via a follow-up hotfix once Faction.db2 IDs are extracted.
--
-- companionSpawn{X,Y,Z,O} not populated — Brann spawn coords are inside-instance
-- positions not exposed by datamining; pull from sniff
-- C:/sniff/alliance_deatholme_delve/dumps/ or from the warband-scene branch's
-- creature SQL once that's confirmed against build 67186.

INSERT INTO `delve_template` (`id`, `mapId`, `scenarioId`, `mapChallengeModeId`, `zoneId`, `factionId`) VALUES
-- The War Within Season 2 (Undermine, 11.1) additions
(14, 2826, 0, 0, 15990, 0),   -- Sidestreet Sluice (Undermine, goblin sewer/chemical theme)
(15, 2815, 0, 0, 15836, 0),   -- Excavation Site 9 (Ringing Deeps, earthen archaeology)
(16, 2831, 0, 0, 15991, 0),   -- Demolition Dome (Undermine, S2 nemesis Underpin host)

-- The War Within Season 3 (Ghosts of K'aresh, 11.2) additions
(17, 2803, 0, 0, 16427, 0),   -- Archival Assault (K'aresh, ethereal pirates)
(18, 2951, 0, 0, 16539, 0);   -- Voidrazor Sanctuary (K'aresh, solo nemesis Ky'veza)
