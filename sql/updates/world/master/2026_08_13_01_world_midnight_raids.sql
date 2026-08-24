--
-- Midnight Season 1 raids -- instance shells for The Voidspire (2912), March on Quel'Danas (2913),
-- The Dreamrift (2939) and Sporefall (1592 / DevMapE dev-shell).
-- SHIPPED ON feature/midnight-raids, LISTED NOT APPLIED to any shared realm.
-- Realm-safe: additive; absent/duplicate rows do not crash worldserver.
--
-- Each InstanceMapScript is ALSO registered by mapId in code (InstanceMapScript(name, <map>));
-- these instance_template rows make the maps instanceable and record the script names.
-- parent = 0 (no shared-progress parent).
--

-- The Voidspire -- Map 2912 (raid, 6 encounters), script 'instance_the_voidspire'.
DELETE FROM `instance_template` WHERE `map` = 2912;
INSERT INTO `instance_template` (`map`, `parent`, `script`) VALUES
(2912, 0, 'instance_the_voidspire');

-- March on Quel'Danas -- Map 2913 (raid, 2 encounters), script 'instance_march_on_queldanas'.
DELETE FROM `instance_template` WHERE `map` = 2913;
INSERT INTO `instance_template` (`map`, `parent`, `script`) VALUES
(2913, 0, 'instance_march_on_queldanas');

-- The Dreamrift -- Map 2939 (raid, 1 encounter), script 'instance_the_dreamrift'.
DELETE FROM `instance_template` WHERE `map` = 2939;
INSERT INTO `instance_template` (`map`, `parent`, `script`) VALUES
(2939, 0, 'instance_the_dreamrift');

-- Sporefall -- Map 1592 (raid, 1 encounter), script 'instance_sporefall'.
-- NOTE: Map 1592's Directory is "DevMapE" -- Blizzard's DEV-SHELL map @68887. World data may not
-- load; the instance_template row + code binding are shipped regardless so the encounter journal is
-- wired the moment a real (non-DevMapE) map becomes available. flex-Mythic Difficulty 233 is built
-- separately on feature/raid-season-s1.
DELETE FROM `instance_template` WHERE `map` = 1592;
INSERT INTO `instance_template` (`map`, `parent`, `script`) VALUES
(1592, 0, 'instance_sporefall');

-- ---------------------------------------------------------------------------------------------------
-- DEFERRED / LISTED-ONLY (NOT written this pass -- all CAPTURE-BLOCKED):
--
--   * Encounter tracking needs NO world-DB table at this baseline: it is DB2-driven via
--     LoadDungeonEncounterData(sDungeonEncounterStore) using the CONFIRMED DungeonEncounter.db2 ids
--     (DB2 @68887):
--       The Voidspire        : 3176 Imperator Averzian, 3177 Vorasius, 3178 Vaelgor & Ezzorak,
--                              3179 Fallen-King Salhadaar, 3180 Lightblinded Vanguard,
--                              3181 Crown of the Cosmos
--       March on Quel'Danas  : 3182 Belo'ren Child of Al'ar, 3183 Midnight Falls (final)
--       The Dreamrift        : 3306 Chimaerus the Undreamt God
--       Sporefall            : 3159 Rotmire
--     Nothing to ship here.
--
--   * creature_template rows for every boss -- CAPTURE-BLOCKED (creature entries are world-DB, NOT in
--     client DB2 @68887). Once captured, add rows with ScriptName bound to the (already-registered)
--     BossAI stubs. Example shape:
--       INSERT INTO creature_template (entry, ... , ScriptName) VALUES (<capture>, ..., 'boss_rotmire');
--     ScriptNames per boss:
--       'boss_imperator_averzian','boss_vorasius','boss_vaelgor_ezzorak','boss_fallen_king_salhadaar',
--       'boss_lightblinded_vanguard','boss_crown_of_the_cosmos','boss_beloren_child_of_alar',
--       'boss_midnight_falls','boss_chimaerus','boss_rotmire'
--
--   * creature spawns (bosses + trash) on maps 2912/2913/2939/1592 -- CAPTURE-BLOCKED (entries + coords).
--   * gameobject spawns (doors / gates) + door/boundary data -- CAPTURE-BLOCKED.
--   * Raid-wide gates (attunement quests, lockouts beyond difficulty rows) -- CAPTURE-BLOCKED.
-- ---------------------------------------------------------------------------------------------------
