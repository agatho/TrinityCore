--
-- Slayer's Rise — Midnight Season 1 40v40 Epic Battleground
-- Ships on feature/slayers-rise-bg. NOT applied to the central realm.
-- Bind the map-script (battleground_slayers_rise) to Map 2799 / BattlemasterList 1141.
--
-- DB2-anchored @ 12.0.7.68887:
--   BattlemasterList 1141 "Slayer's Rise" (Epic Battleground, Min 10 / Max 40, InstanceType 3)
--   BattlemasterListXMap 1148 -> MapID 2799
--   Map 2799 "Slayer's Rise" (Voidstorm)
--
-- CAPTURE-BLOCKED: the WorldSafeLocs start-location ids + graveyard ids for map 2799
-- are NOT yet captured. WorldSafeLocs.db2 is NOT exposed on wago.tools for build
-- 12.0.7.68887 (the CSV endpoint returns {"errors":"Table not found."}), so those
-- ids cannot be datamined and MUST NOT be invented. The battleground_template
-- start-loc ids below are PLACEHOLDER 0 and MUST be replaced with real
-- WorldSafeLocs.db2 ids before graveyards / start locations work.
--

-- --- Script binding (this is what actually activates battleground_slayers_rise) ---
DELETE FROM `battleground_scripts` WHERE `MapId`=2799;
INSERT INTO `battleground_scripts` (`MapId`, `BattlemasterListId`, `ScriptName`) VALUES
(2799, 1141, 'battleground_slayers_rise');

-- --- Template (MinPlayers/MaxPlayers come from BattlemasterList.db2 1141, not here) ---
-- PLACEHOLDER start locs (0) — CAPTURE-BLOCKED, replace with WorldSafeLocs ids for map 2799.
DELETE FROM `battleground_template` WHERE `ID`=1141;
INSERT INTO `battleground_template` (`ID`, `AllianceStartLoc`, `HordeStartLoc`, `Weight`, `Comment`) VALUES
(1141, 0, 0, 1, 'Slayer''s Rise');

-- ---------------------------------------------------------------------------
-- LISTED-ONLY (NOT written — pending §7 captures in SLAYERS_RISE_BLUEPRINT.md):
--   * WorldSafeLocs.db2 entries for the two faction start locations + graveyards.
--   * graveyard_zone (ID, GhostZone) rows linking those WorldSafeLocs to map 2799's
--     zone (AreaTable 16423 "SlayersRiseBG"), optionally node-condition-gated.
--   * creature spawn SQL for the domanaar bosses Vidious (Grief Spire) / Ziadan
--     (Hate Spire) and spirit guides — creature ids + coords CAPTURE-BLOCKED.
--   * gameobject spawn SQL for the Bastion gates + Shenzar Refinery capture flag.
--   * world_state rows scoping node worldstates 29506 / 29509 / 29510 to map 2799
--     and the (uncaptured) reinforcement-counter worldstates + starting count
--     (the script uses AV's 600 as a flagged default until captured).
-- Node objective centres are DB2-anchored (AreaPOI on ContinentID 2799):
--   Grief Spire 8375, Hate Spire 8376, Bastion of Valor 8378 (WS 29506),
--   Bastion of Might 8379 (WS 29509), Shenzar Refinery 8382 (WS 29510),
--   Path of Predation 8403 — see the .cpp header for coords.
-- Additional DB2-datamined node worldstates on map 2799 (NOT modelled yet — role
-- uncaptured, do not invent): Stareater Pavilion 8620 (WS 29507),
--   Shadowridge Outpost 8621 (WS 29508), Gates of Might 8645 (WS 29511),
--   Gates of Valor 8646 (WS 29512).
-- ---------------------------------------------------------------------------
