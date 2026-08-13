--
-- Voidscar Arena (Midnight 8th dungeon) -- Map 2923.
-- SHIPPED ON feature/voidscar-arena, LISTED NOT APPLIED to any shared realm.
-- Realm-safe: additive; absent/duplicate rows do not crash worldserver.
--

-- Bind the InstanceMapScript to the dungeon map. The InstanceMapScript is ALSO registered by mapId
-- in code (InstanceMapScript("instance_voidscar_arena", 2923)); this row makes map 2923 instanceable
-- and records the script name. parent = 0 (no shared-progress parent).
DELETE FROM `instance_template` WHERE `map` = 2923;
INSERT INTO `instance_template` (`map`, `parent`, `script`) VALUES
(2923, 0, 'instance_voidscar_arena');

-- ---------------------------------------------------------------------------------------------------
-- DEFERRED / LISTED-ONLY (NOT written this pass -- all CAPTURE-BLOCKED, see blueprint section 7):
--
--   * Encounter tracking needs NO world-DB table at this baseline: it is DB2-driven via
--     LoadDungeonEncounterData(sDungeonEncounterStore) using the CONFIRMED DungeonEncounter.db2 ids
--     3285 (Taz'Rah) / 3286 (Atroxus) / 3287 (Charonus). Nothing to ship here.
--
--   * creature_template rows for the 3 bosses -- CAPTURE-BLOCKED (creature entries are world-DB, not
--     in client DB2 @68887). Once captured, add rows with ScriptName = 'boss_tazrah' /
--     'boss_atroxus' / 'boss_charonus' to bind the (already-registered) BossAI stubs. Example shape:
--       INSERT INTO creature_template (entry, ... , ScriptName) VALUES (<capture>, ..., 'boss_tazrah');
--
--   * creature spawns (bosses + trash) on map 2923 -- CAPTURE-BLOCKED (entries + coords).
--   * gameobject spawns (arena gates / doors) + door/boundary data -- CAPTURE-BLOCKED.
--   * M+ tie-in is DATA-gated, NOT code-gated: the fork's ChallengeModeMgr::LoadMapPool() iterates
--     sMapChallengeModeStore keyed by MapID. Map 2923 has NO MapChallengeMode.db2 row @68887
--     (Season 2 forward-looking). When Blizzard ships that DB2 row (or a hotfix adds one), the
--     dungeon auto-joins the M+ pool with no code change.
-- ---------------------------------------------------------------------------------------------------
