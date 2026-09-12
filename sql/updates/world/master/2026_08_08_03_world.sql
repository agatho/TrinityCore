-- ---------------------------------------------------------------------------
-- The Darkway (delve_template 28): full run data from the tester capture
-- C:\sniff\dump_12.0.7.68974_shadowmoon_delve\dump_12.0.7.68974_delve.pkt
-- (build 12.0.7.68974, 2026-08-08 mining session; scripts C:\dumps\mine_tester3_*.py)
--
-- Source packets:
--   * SMSG_NEW_WORLD -> map 3003 at (3556.505, 4807.340, 587.949, o 6.0752).
--   * SMSG_SCENARIO_STATE -> ScenarioID 3184, steps (progression order)
--     16133 -> 16100 -> 16102 -> 16101 (4 steps; run not fully completed on wire,
--     capture ends with logout inside the delve).
--   * Worldstates on entry (SMSG_UPDATE_WORLD_STATE idx 7780-7788 + map-3003
--     SMSG_INIT_WORLD_STATES): 24430(tier)=1, 26345(in-delve)=1, 26423(map)=3003,
--     26931(tier spell)=1260940, 26903(per-delve)=1265829, 5029(lfg)=3083.
--   * Entrance NPC: creature 251896 "Enter Delve" on map 0 (zone 15969, subzone
--     16099), player stood at (9174.818, -4437.604, 3.222) facing 6.0752 when
--     interacting. NO gossip tier menu exists at 68974 -- the entrance answers
--     CMSG_TIERED_ENTRANCE_OPEN with SMSG_TIERED_ENTRANCE_OPEN_RESPONSE
--     (EntranceType=1 Delve, 11 tiers, tierIDs 23..33), then
--     CMSG_SELECT_DELVE_ENTRANCE_TIER carries the chosen tierID (23 = Tier 1).
--     gossipMenuId/firstTierGossipOptionId therefore stay 0 for this delve.
--   * EXIT: the return NEW_WORLD was NOT captured (tester logged out inside).
--     Exit coords below are the measured player position beside the entrance
--     NPC on map 0; orientation is the entry facing reversed (6.0752 - pi).
--     Replace with exact values when an exit capture exists.
UPDATE `delve_template` SET
    `scenarioId` = 3184, `activeScenarioId` = 3184,
    `worldState26903` = 1265829,
    `entryX` = 3556.505, `entryY` = 4807.340, `entryZ` = 587.949, `entryO` = 6.0752,
    `exitX` = 9174.818, `exitY` = -4437.604, `exitZ` = 3.222, `exitO` = 2.9336
WHERE `id` = 28;

-- Scenario routing for ScenarioMgr (delves are faction-neutral, difficulty 208)
DELETE FROM `scenarios` WHERE `map` = 3003 AND `difficulty` = 208;
INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES
(3003, 208, 3184, 3184);

-- ---------------------------------------------------------------------------
-- Companion verification (informational, no row change):
--   BOTH Valeera Sanguinar entries spawn inside the delve at 68974:
--     * 248567 = the ACTIVE companion: follows the player (937 monster-move
--       packets), casts 1266066/1261077/1265783/1265593, owns in-delve
--       vignette 7135 at (3541.7, 4808.9, 590.4).
--     * 249057 = a second, mostly stationary scripted Valeera (28 moves,
--       story-scene actress).
--   => Use 248567 as the follower companion creature; 249057 is the scripted
--      scene copy, not the follower.
--
-- Tier-spell observation (informational): for the Tier 1 run the wire carries
-- WS 26931 = 1260940, which does NOT match DelvesDefines.h TIER_SPELL_IDS[0]
-- (1260938, sourced from 66527). No spell of the 1260938..1260973 family is
-- ever cast in the capture; on tier select the server casts 1305941, 426853
-- and 1254713 on the player instead. Re-verify the tier->spell table before
-- relying on it at 68974.
--
-- LFG observation (informational): WS 5029 inside map 3003 = 3083, while
-- delve_template row 28 currently stores lfgDungeonsId = 6026. Left unchanged
-- pending confirmation of what WS 5029 carries at 68974.
