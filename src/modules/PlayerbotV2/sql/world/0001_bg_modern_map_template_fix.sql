-- ===========================================================================
-- World-DB correction: modern WSG battleground_template config (map 2106)
-- Target DB: world (this server: wc_world)
-- Date:      2026-06-22
-- ===========================================================================
--
-- ROOT CAUSE (bot BG flag-pickup failure):
--   The autonomous bot BG seed (BotPopulationManager::SeedBgMatches) queued the
--   LEGACY BattlemasterList ids for WSG (2) and AB (3). Those ids resolve, via
--   BattlemasterListXMap.db2, to the RETIRED maps:
--       BML 2  "Warsong Gulch - Classic" -> map 489  (0 flag GOs, NO BG script)
--       BML 3  "Arathi Basin"            -> map 529  (0 node GOs, NO BG script)
--   The live flag/node GameObjects and the registered BattlegroundScript live on
--   the MODERN maps:
--       BML 1014 "Warsong Gulch" -> map 2106 (10 GOs, battleground_warsong_gulch)
--       BML 1018 "AB New"        -> map 2107 (34 GOs, battleground_arathi_basin)
--   Bots therefore reached a live match on an EMPTY field (no flag GO ever
--   spawned) -> zero pickups -> score_delta stayed 0 forever despite [bgcoord]
--   assigning carriers. (BG instance map = BattlemasterListXMap[template.ID];
--   the entry teleport in BattlegroundMgr::SendToBattleground lands the player
--   on the template's WorldSafeLocs start-loc map. For 489/529 both are the dead
--   legacy map.)
--
-- The seed was switched to the modern ids {1014, 1018, ...} in
-- BotPopulationManager.cpp. The modern WSG template (1014) was present with
-- valid start locs on map 2106 (7050/7051) BUT had a zeroed player/level
-- config, so the seed computed max_total_bots=0 and never filled it. This
-- restores the canonical WSG sizing (mirrors the legacy template id 2).
-- (AB template 1018 "AB New" already carried a valid 8/15, 10-120 config.)

UPDATE `battleground_template`
   SET `MinPlayersPerTeam` = 2,
       `MaxPlayersPerTeam` = 10,
       `MinLvl`            = 10,
       `MaxLvl`            = 120
 WHERE `ID` = 1014;
