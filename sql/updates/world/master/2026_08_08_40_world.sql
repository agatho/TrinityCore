--
-- delve_template: final boss entries + LFGDungeons id corrections (client-derived)
-- ================================================================================
--
-- Every value in this file comes from the 12.0.7 client DB2s that worldserver itself loads
-- (`M:/WorldofWarcraft/dbc/enUS`, DataDir = "../WorldofWarcraft"), or from a local packet capture.
-- Nothing here is web-sourced; nothing here is guessed. Web-sourced material, if any is ever
-- needed for delves, belongs in a separate file.
--
-- The DB2s were read with a corrected WDC5 parser. The one in the workspace
-- (`c:/dumps/wdc5_read.py`) is broken three ways - it reads only section 0 (SpellName.db2 has 36),
-- it never resolves the copy table, and it mis-widths pallet fields - which makes its "row not
-- found" results worthless. Each table below was control-checked against rows known to be present
-- before any negative result was trusted.
--
--
-- 1) finalBossEntry
-- -----------------
-- `delve_template.finalBossEntry` is "creature entry whose death completes the delve"
-- (Delves::DelveInstanceScript::OnUnitDeath, delves_common.cpp:142-149). It was 0 for every row,
-- so that completion path has never fired for any delve.
--
-- ATAL'AMAN (map 2962, scenario 3147) -> 258372 "Spiritflayer Jin'Ma"
--   Pure client-data derivation, no ambiguity:
--     ScenarioStep.db2  ScenarioID 3147, OrderIndex 3, ID 16008 -> CriteriatreeID 217482
--                       ("12.0 Delves - Atal'Aman - V02 - Last Step - Kill Boss")
--     CriteriaTree.db2  217482 -> child 217483 "Spiritflayer Jin'Ma slain", Amount 1
--                       -> CriteriaID 112040
--     Criteria.db2      112040  Type = 0 (CriteriaType::KillCreature) -> Asset IS the creature
--                       entry -> Asset = 258372
--   Corroborated by DungeonEncounter.db2 ids 3433/3434/3435, MapID 2962, Name "Spiritflayer Jin'ma",
--   and by integ_world.creature_template 258372 = "Spiritflayer Jin'Ma".
--
--   VARIANT TRAP - Atal'Aman ships four Type-8 scenarios and each names a DIFFERENT boss entry:
--       3092 -> tree 211185, criteria  60399, Type 92 (generic event)
--       3117 -> tree 214482, criteria 109272, Type  0, Asset 247114
--       3147 -> tree 217482, criteria 112040, Type  0, Asset 258372   <= the live one
--       3148 -> tree 214512, criteria 111692, Type  0, Asset 258375
--   delve_template.scenarioId AND integ_world.scenarios(2962, 208) both say 3147, so 258372 it is.
--
-- THE SHADOW ENCLAVE (map 2952, scenario 3154) -> 246717 "Lord Antenorian"
--   The DB2 chain does NOT resolve this one: scenario 3154's last step uses Criteria 60399,
--   Type 92 (AnyoneTriggerGameEventScenario), whose Asset 85913 is a GameEvents id shared by 32
--   criteria trees across many delves - not a creature entry. It comes off the wire instead.
--   C:\sniff\alliance_deatholme_delve\dumps\dump_12.0.1.66562_2026-03-26_08-04-06.pkt, a complete
--   Shadow Enclave run ("Deatholme delve"):
--       SMSG_ENCOUNTER_START                 EncounterID 3368, DifficultyID 208
--       SMSG_INSTANCE_ENCOUNTER_ENGAGE_UNIT  map 2952, entry 246717
--       SMSG_BOSS_KILL                       DungeonEncounterID 3368
--       SMSG_QUEST_UPDATE_ADD_CREDIT         map 2952, entry 246717 (quest 86636 "Void Walk With Me")
--       SMSG_SCENARIO_COMPLETED              ScenarioID 3154
--       SMSG_QUERY_CREATURE_RESPONSE         entry 246717 = "Lord Antenorian"
--   DungeonEncounter.db2 id 3368 = Name "Antenorian", MapID 2952 - the same encounter id.
--   Of the seven "Lord Antenorian" creature_template entries, 246717 is the only one carrying a
--   creature_template_difficulty row at DifficultyID 208 (ContentTuningID 4898, VerifiedBuild 66562).
--
UPDATE `delve_template` SET `finalBossEntry` = 258372 WHERE `mapId` = 2962;   -- Spiritflayer Jin'Ma
UPDATE `delve_template` SET `finalBossEntry` = 246717 WHERE `mapId` = 2952;   -- Lord Antenorian

--
-- 2) lfgDungeonsId
-- ----------------
-- content/midnight-s1 seeded nine delve_template rows with lfgDungeonsId values in the 5916-6026
-- range. LFGDungeons.db2 in this client build has max_id = 3266: NONE of those nine ids exist, so
-- sLFGDungeonsStore.LookupEntry() returns nullptr for all of them at runtime.
--
-- That is not cosmetic. The field is read in two places:
--     npc_delve_entrance.cpp:115   gossipMessage.LfgDungeonsID = tmpl->LfgDungeonsId
--                                  -> this is what makes the client render
--                                     Blizzard_DelvesDifficultyPicker instead of a plain NPC menu
--     delves_common.cpp:77-78      SendUpdateWorldState(WS_DELVE_LFG_DUNGEONS_ID = 5029, ...)
--
-- The correct ids were read straight out of LFGDungeons.db2 by matching on MapID. Every delve row
-- is TypeID 1, Subtype 3, DifficultyID 208, Group_ID 38 - the signature taken from the two rows we
-- already knew (3025 Atal'Aman, 3069 The Shadow Enclave) - and ContentTuningID splits the seasons
-- cleanly: 4898 = Midnight, 2677 = The War Within.
--
--   MapID  LFGDungeons  Name_lang (LFGDungeons.db2)  ContentTuningID  was
--   -----  -----------  --------------------------   ---------------  ----
--    2933      3019     Collegiate Calamity              4898         5930
--    2951      3014     Voidrazor Sanctuary              2677 (TWW)      0
--    2952      3069     The Shadow Enclave               4898         3069  (already correct)
--    2953      3018     Parhelion Plaza                  4898         5916
--    2961      3026     Twilight Crypts                  4898         5924
--    2962      3025     Atal'Aman                        4898         3025  (already correct)
--    2963      3020     The Grudge Pit                   4898         5926
--    2964      3070     The Gulf of Memory               4898         5927
--    2965      3068     Sunkiller Sanctum                4898         5928
--    2966      3071     Torment's Rise                   4898         5929
--    2979      3021     Shadowguard Point                4898         5971
--    3003      3083     The Darkway                      4898         6026
--
-- These twelve are the complete delve set for this build: sweeping Map.db2 for
-- InstanceType = 5 (Scenario) + ExpansionID = 11 (Midnight) yields exactly these eleven Midnight
-- maps and nothing else that has an LFGDungeons delve row. (2907, 2928 "Shadowguard Point OLD",
-- 3014 "Broken Throne", 3022 "Silvermoon's Falconwing Square" and 3074 "Daggerspine Point" are
-- also InstanceType 5 / ExpansionID 11 but are NOT delves - 3014/3074 are "Ritual Site" rows at
-- DifficultyID 12 and 3022 is "Decor Duel". So no Midnight S1 delve is missing from this table,
-- Torment's Rise included.)
--
UPDATE `delve_template` SET `lfgDungeonsId` = 3019 WHERE `mapId` = 2933;   -- Collegiate Calamity
UPDATE `delve_template` SET `lfgDungeonsId` = 3014 WHERE `mapId` = 2951;   -- Voidrazor Sanctuary (TWW S3)
UPDATE `delve_template` SET `lfgDungeonsId` = 3018 WHERE `mapId` = 2953;   -- Parhelion Plaza
UPDATE `delve_template` SET `lfgDungeonsId` = 3026 WHERE `mapId` = 2961;   -- Twilight Crypts
UPDATE `delve_template` SET `lfgDungeonsId` = 3020 WHERE `mapId` = 2963;   -- The Grudge Pit
UPDATE `delve_template` SET `lfgDungeonsId` = 3070 WHERE `mapId` = 2964;   -- The Gulf of Memory
UPDATE `delve_template` SET `lfgDungeonsId` = 3068 WHERE `mapId` = 2965;   -- Sunkiller Sanctum
UPDATE `delve_template` SET `lfgDungeonsId` = 3071 WHERE `mapId` = 2966;   -- Torment's Rise
UPDATE `delve_template` SET `lfgDungeonsId` = 3021 WHERE `mapId` = 2979;   -- Shadowguard Point
UPDATE `delve_template` SET `lfgDungeonsId` = 3083 WHERE `mapId` = 3003;   -- The Darkway

--
-- 3) Torment's Rise scenario
-- --------------------------
-- Torment's Rise is the one delve whose scenario is unambiguous. Every other Midnight delve ships
-- three or four identically-titled Type-8 scenarios (route/tier variants) that are byte-identical
-- in Scenario.db2, and nothing in client data says which one is live - only a capture does. Counts,
-- from ScenarioStep.Title_lang:
--     Atal'Aman        3092, 3117, 3147, 3148        Shadow Enclave  3154, 3257, 3262
--     Gulf of Memory   3177, 3242, 3243              The Darkway     3184, 3185, 3256
--     Torment's Rise   3289                          <= single candidate
--
--   ScenarioStep.db2  ID 16581, ScenarioID 3289, OrderIndex 0, Title_lang "Torment's Rise",
--                     CriteriatreeID 222105, WidgetSetID 842
--   CriteriaTree.db2  222105 "12.0 Delve - Torment's Rise - Defeat Boss"
--                     -> child 222106 "Nullaeus defeated", Amount 1, CriteriaID 111894
--   Criteria.db2      111894  Type = 92 (AnyoneTriggerGameEventScenario), Asset = 100261 (GameEvents)
--   Scenario.db2      3289    Type = 8 (the delve scenario type, Delves::DELVE_SCENARIO_TYPE)
--
UPDATE `delve_template` SET `scenarioId` = 3289, `activeScenarioId` = 3289 WHERE `mapId` = 2966;

DELETE FROM `scenarios` WHERE `map` = 2966 AND `difficulty` = 208;
INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES (2966, 208, 3289, 3289);

--
-- NOT COVERED - reported instead of guessed
-- =========================================
--
--  * finalBossEntry for Torment's Rise (map 2966). The scenario's kill criterion is Type 92
--    (game event 100261), not Type 0, so it names no creature entry. The boss is called "Nullaeus"
--    (CriteriaTree 222106; DungeonEncounter.db2 3372/3430, MapID 2966, Name "Nullaeus"), but
--    integ_world has FIVE creature_template rows named "Nullaeus" - 252101, 252892, 252950,
--    255108, 260627 - and none of them carries a creature_template_difficulty row at
--    DifficultyID 208, so the tie-break that identified Lord Antenorian does not apply.
--    TO UNBLOCK: one capture of a Torment's Rise run - SMSG_INSTANCE_ENCOUNTER_ENGAGE_UNIT or
--    SMSG_QUERY_CREATURE_RESPONSE names the entry outright.
--
--  * finalBossEntry for the other eight Midnight delves. Same reason: their live scenario id is
--    itself unresolved (3-4 candidates each), so there is no criteria chain to walk.
--
--  * entryX/Y/Z/O, exitX/Y/Z/O, companionSpawnX/Y/Z/O, gossipMenuId, broadcastTextId,
--    firstTierGossipOptionId and worldState26903 for every delve except Atal'Aman, The Shadow
--    Enclave and The Gulf of Memory. No client DB2 carries any of them - UiMapAssignment.Region
--    gives only the delve's own interior bounding box, and gossip is entirely server-side data
--    that the client never ships. They need a capture per delve.
--
--  * creature_template_difficulty for 258372 (Spiritflayer Jin'Ma) has only a DifficultyID 0 row
--    with ContentTuningID 0, unlike the Shadow Enclave roster which 2026_04_29_03_world.sql tuned
--    to 4898. 258372 also has no spawn anywhere (map 2962 has 433 spawns and the boss is not among
--    them - see the spawn note below), so tuning it now would be tuning a creature that cannot be
--    reached. Left alone deliberately.
--
--  * zoneId. Checked, not changed: all twelve existing values are already correct. Each is the
--    AreaTable row whose ContinentID is the delve map and whose ParentAreaID is 0, named
--    identically to the map (16556 "Atal'Aman", 16594 "The Shadow Enclave", 16596 "Torment's
--    Rise", ...). No correction needed.
--
-- KNOWN DATA GAPS THAT BLOCK THESE DELVES REGARDLESS OF THIS FILE
-- ---------------------------------------------------------------
--  * Map 2952 (The Shadow Enclave) has ZERO creature and ZERO gameobject spawns in integ_world,
--    and no spawn file for it exists in this repo or in any other world DB on this machine
--    (tc_world, world, wc_world, playerbot_world, wowc_world all checked). 2026_04_29_03_world.sql
--    is UPDATE-only. The 66562 capture cited above carries the full 167-entry creature roster with
--    SMSG_UPDATE_OBJECT positions on map 2952 and is the obvious import source.
--  * Map 2962 (Atal'Aman) has 433 spawns which match scenario 3147 step for step (252930 Fleek,
--    252948 Spiritpaw Cubs x12, 247106 Consumption of Nalorakk + 253321 Hexbound Ritualist), but
--    the final boss 258372 is not spawned, so the run cannot be finished there either.
--  * The nine delves seeded by content/midnight-s1 have no spawns, no scenario, no entry
--    coordinates and no gossip menu - they are placeholder rows, not playable delves.
