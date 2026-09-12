--
-- Midnight S1 delve roster: seed the 9 missing delve_template rows (Atal'Aman id 19 and
-- The Shadow Enclave id 20 already exist with full sniff-derived data).
--
-- VERIFIED (client data): mapId (Map.db2 @ 68367; Shadowguard Point = 2979, NOT the "OLD" 2928 row),
-- zoneId + lfgDungeonsId + ContentTuning per doc/research/09_DELVE_MAP_SCENARIO_IDS.md (delves branch).
--
-- PENDING SNIFF DATA (fields left 0): scenarioId/activeScenarioId (per-delve scenario variants are
-- unresolvable from Scenario.db2 alone - all Type-8 rows are named "Delves"), gossipMenuId /
-- broadcastTextId / firstTierGossipOptionId, entry/exit coordinates, worldState26903.
-- A row with entryX=0 is listed by the entrance NPC but the server-side teleport falls back to the
-- tier spell's own teleport effect; complete these fields from captures as they become available
-- (a Gulf of Memory 12.0.1 capture exists: C:\sniff\alliance_the_gulf_of_memory_delve).
--
-- rewardScenarioId 3424 matches the two seeded Midnight rows (shared end-of-run reward scenario).
--

DELETE FROM `delve_template` WHERE `id` BETWEEN 21 AND 29;
INSERT INTO `delve_template` (
    `id`, `mapId`, `scenarioId`, `mapChallengeModeId`, `zoneId`, `factionId`,
    `gossipMenuId`, `lfgDungeonsId`, `broadcastTextId`, `firstTierGossipOptionId`,
    `entryX`, `entryY`, `entryZ`, `entryO`,
    `exitX`, `exitY`, `exitZ`, `exitO`,
    `activeScenarioId`, `rewardScenarioId`, `worldState26903`
) VALUES
-- Collegiate Calamity (Silvermoon / Sunwell college)
(21, 2933, 0, 0, 16545, 0,  0, 5930, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- Parhelion Plaza (Voidstorm)
(22, 2953, 0, 0, 16542, 0,  0, 5916, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- Twilight Crypts
(23, 2961, 0, 0, 16557, 0,  0, 5924, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- The Grudge Pit
(24, 2963, 0, 0, 16548, 0,  0, 5926, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- The Gulf of Memory
(25, 2964, 0, 0, 16595, 0,  0, 5927, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- Sunkiller Sanctum
(26, 2965, 0, 0, 16592, 0,  0, 5928, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- Shadowguard Point
(27, 2979, 0, 0, 16549, 0,  0, 5971, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- The Darkway
(28, 3003, 0, 0, 16642, 0,  0, 6026, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0),
-- Torment's Rise (Nemesis delve, boss Nullaeus; T8/T11 variants, solo-unlock via T7/T10 clear)
(29, 2966, 0, 0, 16596, 0,  0, 5929, 0, 0,  0,0,0,0,  0,0,0,0,  0, 3424, 0);
