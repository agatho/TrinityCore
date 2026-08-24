-- ============================================================================
-- Arathi Catch-Up / RPE -- "Catapult Bombardment" (90895)  (2026-08-22)
-- ============================================================================
-- WIRE-CONFIRMED in BOTH captures (69382 Alliance / 69404 Horde) by a per-GUID census of the raw .pkt:
--   * Exactly FOUR distinct Worn Catapult (249269) GUIDs bombard the Stromgarde approach. The Horde
--     capture shows all four, each SPELLCLICKED exactly once (CMSG_SPELLCLICK 0x3E002A), and each one
--     STOPS CASTING immediately after its click (0-1 trailing in-flight cast) -> "destroyed".
--   * 467 catapult casts were recorded and 401 of them fall BEFORE the first click, so the bombardment
--     is AMBIENT siege fire on a loop, NOT a response to the click. Shot spells cycle ~2:2:1 across
--     1248641 / 1248649 / 1248657 (all three already bound in creature_template_spell = known-loaded).
--   * Every click is followed ~90-130 ticks later by the PLAYER casting 1248670 (the sabotage spell).
--   * No despawn event for 249269 in either capture -> the wreck STAYS on the field.
-- The C++ (npc_arathi_rpe_worn_catapult in zone_arathi_highlands_rpe.cpp) drives the ambient fire, the
-- per-player credit, and the silencing. This file supplies the data.

-- (1) DOUBLED OBJECTIVE -- same fabricated-duplicate bug as the farm quests (the 90NNN00 id pattern).
--     The real Blizzard objective is 461767 (StorageIndex 4, ObjectID 249269, Amount 4 "Catapults
--     destroyed"); 9089500 is a fabricated copy at StorageIndex 0 that shows the task twice.
DELETE FROM `quest_objectives` WHERE `ID`=9089500;

-- (2) UNDER-SPAWNED: the objective needs FOUR catapults and the per-GUID wire census proves four, but
--     only TWO are spawned. Add the missing two.
--     Position provenance: the union of both captures' objupdate decode yields THREE exact world
--     positions (with real Z). Two are already in the DB (guids 8000136/8000137); the third
--     (-1201.02,-1773.55,59.45) is Horde-only and is added here as 8002010.
--     The FOURTH catapult's position did not survive the objupdate decode in either capture (a decode
--     gap -- the GUID census proves the unit exists, but no create-block position was recovered), so
--     8002011 is INFERRED: placed within the captured catapult cluster on terrain height consistent
--     with its neighbours (~63, cf. 244685 at -1349.9,-1888.3 z=63.1). Move it if it sits badly.
--     NOTE: the addon channel's two extra "catapult" rows were REJECTED -- addon spawn coordinates are
--     the PLAYER's position when the unit was observed, not the unit's (one of them decodes to 1 yard
--     from Jaina 244658, the quest giver). They are map-2451 uiMap coords with x/y swapped and no Z.
DELETE FROM `creature` WHERE `guid` IN (8002010,8002011);
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`MovementType`,`VerifiedBuild`) VALUES
 (8002010, 249269, 2927, 28, -1201.0156, -1773.5504, 59.4456, 2.9809, 300, 0, 69404),   -- exact, Horde capture
 (8002011, 249269, 2927, 28, -1300.0000, -1868.0000, 63.0000, 2.8000, 300, 0, 69404);   -- INFERRED 4th (see note)

-- (3) SPELLCLICK. The catapult was npcflag=1 (gossip), which would open a gossip window instead of
--     taking the click. Set UNIT_NPC_FLAG_SPELLCLICK (0x1000000) and hand it to the C++ AI.
UPDATE `creature_template` SET `npcflag`=0x1000000, `AIName`='', `ScriptName`='npc_arathi_rpe_worn_catapult' WHERE `entry`=249269;
DELETE FROM `smart_scripts` WHERE `entryorguid`=249269 AND `source_type`=0;

-- (4) Spellclick registration. EXACTLY ONE row -> Creature::InitializeInteractSpellId sets the
--     InteractSpellId so the client offers the click and sends CMSG_SPELLCLICK.
--     SPELL CHOICE / SAFETY: the wire's sabotage spell is 1248670 (cast by the PLAYER), but 1248670 is
--     NOT resolvable server-side here -- it is absent from the ADB export AND from the hotfix spell
--     tables -- and Unit::HandleSpellClick runs AssertSpellInfo on the registered spell, which would
--     CRASH the worldserver on an unknown id. So the row uses 1248649, the catapult's dominant
--     bombardment shot, which IS proven loaded (its creature_template_spell row loads without a
--     DBErrors complaint). cast_flags=0 -> the catapult self-casts: it fires one last shot as it is
--     wrecked, which reads correctly in game. The quest CREDIT is granted explicitly by the C++
--     OnSpellClick (KilledMonsterCredit 249269), not by the spell, so nothing depends on 1248670.
DELETE FROM `npc_spellclick_spells` WHERE `npc_entry`=249269;
INSERT INTO `npc_spellclick_spells` (`npc_entry`,`spell_id`,`cast_flags`,`user_type`) VALUES
 (249269, 1248649, 0, 0);
