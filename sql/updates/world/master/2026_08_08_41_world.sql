--
-- The Shadow Enclave + The Gulf of Memory: entrance spawns and sniff-exact template data
-- ======================================================================================
--
-- Everything here is read out of two local packet captures. No value is invented, and nothing here
-- is web-sourced. File offsets and record indices are quoted so each number can be re-checked.
--
--   F1 = C:\sniff\alliance_deatholme_delve\dumps\dump_12.0.1.66562_2026-03-26_08-04-06.pkt
--        build 12.0.1.66562, 54,348 records - a complete The Shadow Enclave (map 2952) run,
--        entrance gossip through boss kill through the return teleport.
--   F2 = C:\sniff\alliance_the_gulf_of_memory_delve\dumps\dump_12.0.1.66709_2026-04-02_14-09-57.pkt
--        build 12.0.1.66709, 60,318 records - a complete The Gulf of Memory (map 2964) run.
--
-- (The third local capture, dump_12.0.7.68974_shadowmoon_delve, is The Darkway / map 3003 and is
-- already handled by 2026_08_08_03_world.sql. It is only referenced here where it corroborates.)
--
--
-- 1) WHY THE SHADOW ENCLAVE GOSSIP WAS UNREACHABLE
-- ------------------------------------------------
-- It was never the gossip rows. All of them already exist in integ_world and are correct:
--     gossip_menu         40277, TextID 212407                              (VerifiedBuild 66562)
--     gossip_menu_option  40277, OptionIDs 0..10 "Tier 1".."Tier 11",
--                         GossipOptionIDs 135336 down to 135326,
--                         SpellIDs 1260938/1260942/.../1260973              (VerifiedBuild 66562)
--     gossip_menu_addon   40277, LfgDungeonsID 3069
-- and F1 confirms every one of those values on the wire (SMSG_GOSSIP_MESSAGE, rec idx 7001, file
-- offset 0x92D630: GossipID 40277, LfgDungeonsID 3069, BroadcastTextID 296637, 11 options, first
-- GossipOptionID 135336, SpellIDs matching the DB rows exactly).
--
-- Three separate defects made the menu unreachable, and all three are fixed in the C++ of this
-- branch. They are listed here because this file is useless without them:
--
--   a) ObjectMgr::LoadGossipMenuAddon() had an inverted existence check (a lost `!`, upstream
--      e59eef5432) that ZEROED LfgDungeonsID for every id that does exist. It hit exactly the two
--      delve rows, 39751 and 40277 - the live realm logs both every boot. Without a LfgDungeonsID
--      the client never opens Blizzard_DelvesDifficultyPicker.
--   b) The entrance script was registered as "npc_delve_entranceAI" (RegisterCreatureAI stringizes
--      the type name) while creature_template.ScriptName says "npc_delve_entrance", so it was never
--      bound to creature 212407 at all.
--   c) Every entrance lookup went through Creature::GetGossipMenuId(), which is the script-override
--      slot and is 0 for every DB-spawned creature - SetGossipMenuId() has no call site in the
--      core. This killed the 12.0.7 CMSG_TIERED_ENTRANCE_OPEN path outright, for all delves.
--
-- And then there is the piece that belongs in this file: THERE IS NO SHADOW ENCLAVE ENTRANCE NPC
-- SPAWNED. integ_world has exactly two spawns of creature 212407 "Enter Delve" - map 2552
-- (949.38, -1648.65) and map 2706 (-10.21, 694.79) - and neither is anywhere near The Shadow
-- Enclave. Fixed below with the position off the wire.
--
--
-- 2) Entrance NPC spawns
-- ----------------------
-- Positions come from the SMSG_UPDATE_OBJECT create blocks for creature 212407 in each capture.
-- The overworld map for each is taken from the SMSG_PRELOAD_WORLD OldMapPosition recorded when the
-- player stepped into the delve.
--
--   The Shadow Enclave  F1  entrance 212407 at (4746.054, -4145.010, 22.570), overworld map 0
--                           (PRELOAD_WORLD rec idx 6491 off 0x91F7D7, OldMapPosition
--                           (4780.695, -4122.754, 30.520) on map 0)
--   The Gulf of Memory  F2  entrance 212407 at ( 16.708,   800.372, 1101.362), overworld map 2694
--                           (PRELOAD_WORLD rec idx 6718 off 0x949DCA, OldMapPosition
--                           (37.934, 799.356, 1105.953) on map 2694 "Harandar")
--
-- ORIENTATION IS NOT SNIFFED. The create blocks carry it but it was not extracted, so each NPC is
-- faced at the point the capture shows the player standing while interacting - a derived value, not
-- a measured one, and the only value in this file that is not read straight off the wire. It has no
-- gameplay effect on a gossip NPC; correct it if an exact facing is ever captured.
--   Shadow Enclave  atan2(-4122.754 - -4145.010, 4780.695 - 4746.054) = 0.5709
--   Gulf of Memory  atan2(  799.356 -   800.372,   37.934 -   16.708) = 6.2354
--
-- guids 50100001/50100002 are above integ_world's current MAX(creature.guid) of 50,052,013.
--
DELETE FROM `creature` WHERE `guid` IN (50100001, 50100002);
INSERT INTO `creature`
(`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,
 `terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,
 `spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`VerifiedBuild`) VALUES
-- "Enter Delve" -> The Shadow Enclave (map 2952). Deatholme, Ghostlands, map 0.
(50100001, 212407, 0,    0, 0, '0', 0, 0, 0, -1, 0, 0, 4746.054, -4145.010,   22.570, 0.5709, 180, 0, 0, 100, 0, 66562),
-- "Enter Delve" -> The Gulf of Memory (map 2964). Harandar, map 2694.
(50100002, 212407, 2694, 0, 0, '0', 0, 0, 0, -1, 0, 0,   16.708,   800.372, 1101.362, 6.2354, 180, 0, 0, 100, 0, 66709);

--
-- 3) delve_template: sniff-exact values
-- -------------------------------------
-- The Shadow Enclave (map 2952). The row already carried close-but-rounded coordinates; these are
-- the exact wire values.
--   entry  SMSG_NEW_WORLD rec idx 7090  off 0x92F997  -> map 2952, (-16.695, 232.975, 265.462) o 3.75350
--   exit   SMSG_NEW_WORLD rec idx 51621 off 0x124540A -> map 0,    (4781.234, -4121.170, 31.018) o 0.80030
--          (a genuine post-completion return, not an approximation - it lands 1.7 yd from the
--           pre-entry overworld position)
--   scenario 3154 confirmed live: SMSG_SCENARIO_STATE rec idx 7597 off 0x9721DC, steps
--          16032 -> 16028 -> 16029 -> 16030 -> 16031, then SMSG_SCENARIO_COMPLETED 3154 at idx 49497
--   worldState 26903 = 1265777 (SMSG_UPDATE_WORLD_STATE idx 7098-7102; already stored, re-asserted)
--   broadcastTextId 296637 and firstTierGossipOptionId 135336 already stored; re-asserted so the
--          row is self-evidently sniff-backed.
UPDATE `delve_template` SET
    `entryX` = -16.695, `entryY` = 232.975, `entryZ` = 265.462, `entryO` = 3.75350,
    `exitX`  = 4781.234, `exitY` = -4121.170, `exitZ` =  31.018, `exitO` = 0.80030,
    `scenarioId` = 3154, `activeScenarioId` = 3154,
    `gossipMenuId` = 40277, `broadcastTextId` = 296637, `firstTierGossipOptionId` = 135336,
    `worldState26903` = 1265777
WHERE `mapId` = 2952;

-- The Gulf of Memory (map 2964). content/midnight-s1 wired the coordinates correctly; what was
-- missing is the per-delve world state, the gossip broadcast text and the final boss.
--   entry  SMSG_NEW_WORLD rec idx 8064  off 0x97AD34  -> map 2964, (155.129, 634.757, 187.334) o 2.81330
--   exit   SMSG_NEW_WORLD rec idx 50927 off 0x12AEDF5 -> map 2694, (48.420, 816.046, 1110.519) o 1.73560
--   gossip SMSG_GOSSIP_MESSAGE rec idx 7567 off 0x968B65 -> GossipID 40278, LfgDungeonsID 3070,
--          BroadcastTextID 296756, first GossipOptionID 135348
--          (LfgDungeonsID 3070 here independently confirms the DB2-derived correction in
--           2026_08_08_40_world.sql, which replaced the seeded 5927)
--   scenario 3177 confirmed live: SMSG_SCENARIO_STATE idx 8631 off 0x9C9FA9, steps
--          16081 -> 16083 -> 16084 -> 16082, SMSG_SCENARIO_COMPLETED 3177 at idx 48250
--   worldState 26903 = 1277243 (SMSG_UPDATE_WORLD_STATE idx 8070-8075 off 0x97AE96..0x97AF68)
--   final boss: SMSG_BOSS_KILL rec idx 48173 off 0x1257E29, DungeonEncounterID 3359.
--          DungeonEncounter.db2 3359 = Name "Mul'tha'ul", MapID 2964; integ_world has exactly one
--          creature_template named "Mul'tha'ul", entry 250939, and its spawn/first-move position
--          (-198.67, 645.15, 176.69) is the room holding the exit portal (-189.6, 650.6, 176.6).
UPDATE `delve_template` SET
    `broadcastTextId` = 296756,
    `worldState26903` = 1277243,
    `finalBossEntry`  = 250939
WHERE `mapId` = 2964;

--
-- NOT COVERED - reported instead of guessed
-- =========================================
--
--  * The Darkway (map 3003) final boss. F3 ends mid-run with the tester logged out inside, so
--    there is no SMSG_BOSS_KILL. The only candidate seen is 252102 "Voidbreaker Oglok" at
--    (3236.53, 4804.48, 602.39); that is a position, not proof, so finalBossEntry stays 0.
--    Its exit coordinates likewise remain the approximation 2026_08_08_03_world.sql already flags.
--
--  * The eight Midnight delves that appear in NO capture - 2933 Collegiate Calamity, 2953
--    Parhelion Plaza, 2961 Twilight Crypts, 2962 Atal'Aman, 2963 The Grudge Pit, 2965 Sunkiller
--    Sanctum, 2966 Torment's Rise, 2979 Shadowguard Point. Not one of entry coords, exit coords,
--    gossipMenuId, broadcastTextId, firstTierGossipOptionId or worldState26903 exists for them in
--    any source on this machine, and none of it is in client DB2. One capture per delve closes each
--    of them completely; nothing less will.
--
--  * Atal'Aman's stored worldState26903 = 1278258 is UNVERIFIED. It predates this work and no
--    capture visits map 2962. It is left untouched rather than "corrected" on a hunch. (For scale:
--    the three delves that were captured read 1265777 / 1277243 / 1265829, so 1278258 is at least
--    the right shape.)
--
--  * The Shadow Enclave's 55-entry / 230-position creature roster and 33-entry gameobject roster
--    are fully recoverable from F1 and would make map 2952 actually playable - it currently has
--    ZERO spawns. That is a much larger import than this file and is deliberately left out; see the
--    report accompanying this branch.
--
-- OBSERVATION, NOT APPLIED
-- ------------------------
-- All three captures put SMSG_UPDATE_WORLD_STATE 26931 at 1260940 on a TIER 1 run, while the
-- SMSG_GOSSIP_MESSAGE tier-1 option carries SpellID 1260938 - and integ_world's gossip_menu_option
-- rows agree with the wire (1260938). DelvesDefines.h TIER_SPELL_IDS[0] is 1260940, so the
-- script-built menu advertises a different tier-1 spell than retail does. Only tier-1 evidence
-- exists, so the ladder is NOT being changed here on one data point; a tier-2+ capture would settle
-- whether 26931 is simply a different id space from the gossip SpellID.
