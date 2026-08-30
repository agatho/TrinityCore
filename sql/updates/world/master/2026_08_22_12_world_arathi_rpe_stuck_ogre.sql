-- ============================================================================
-- Arathi Catch-Up / RPE -- "Best Laid Plans of Kobolds" (90886) Stuck Ogre  (2026-08-22)
-- ============================================================================
-- BLOCKER: quest 90886 (given AND ended by Thrall 244656 at Go'shek Farm) has two objectives -- the
-- real Blizzard 461734 (7x item 243573, dropped by the farm raiders) and the real Blizzard 465804
-- (MONSTER credit on Stuck Ogre 253460, Amount 1). Objective 465804 is genuine content (its id is in
-- the Blizzard 46xxxx range, not the fabricated 90NNN00 range) -- but Stuck Ogre 253460 had ZERO
-- spawns on the map, so the objective could never be satisfied and 90886 was uncompletable.
--
-- POSITION IS INFERRED. A raw-wire GUID census proves the creature is real (3 distinct Creature-typed
-- GUIDs for 253460 in the Horde capture), but its create block never decodes a position in EITHER
-- capture, and it appears in no other channel -- so there is nothing to place it from. It is put at the
-- farm, on the terrain height its neighbours use (z=14), inside the 90886 quest window's phase 4
-- (gated "90883 rewarded AND NOT 90888 rewarded"). MOVE IT once someone sees where it actually belongs.
--
-- INTERACTION IS A BLIZZLIKE-EQUIVALENT, NOT CAPTURED. The creature is faction 35 (friendly), so it
-- cannot be killed for the MONSTER credit; retail must credit it some other way, and the capture does
-- not show which (no spellclick was recorded for 253460 -- the only two spellclicked entries in either
-- capture are the Prized Pumpkin and the Worn Catapult). Implemented as a gossip-interact credit, the
-- same functional pattern the Prized Pumpkin used before its real spellclick mechanic was recovered.
-- Pure data, so it works as soon as the realm restarts -- no core build needed.

DELETE FROM `creature` WHERE `guid`=8002020;
INSERT INTO `creature` (`guid`,`id`,`map`,`PhaseId`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`MovementType`,`VerifiedBuild`) VALUES
 (8002020, 253460, 2927, 4, -1490.0000, -2990.0000, 14.0000, 1.5000, 300, 0, 69404);

-- Make it interactable and hand it a gossip-hello credit script.
UPDATE `creature_template` SET `npcflag`=1, `AIName`='SmartAI' WHERE `entry`=253460;

DELETE FROM `smart_scripts` WHERE `entryorguid`=253460 AND `source_type`=0;
INSERT INTO `smart_scripts`
 (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,
  `event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,
  `action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,
  `target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
 (253460, 0, 0, 0, 64, 0, 100, 0, 0,0,0,0,0, 33, 253460, 0,0,0,0,0,0, 7, 0,0,0,0, 0,0,0,0, 'Stuck Ogre - gossip-hello kill credit for quest 90886 objective 465804 (functional equivalent, retail mechanism not captured)'),
 (253460, 0, 1, 0, 64, 0, 100, 0, 0,0,0,0,0, 72, 0, 0,0,0,0,0,0, 7, 0,0,0,0, 0,0,0,0, 'Stuck Ogre - close gossip after credit');
