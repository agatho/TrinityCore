-- ============================================================================
-- Arathi Catch-Up / RPE -- "My Beautiful Pumpkins" (90885) carrying peon  (2026-08-22)
-- ============================================================================
-- WIRE-CONFIRMED mechanic (Horde capture 69404, verified against the raw .pkt after the tester
-- correctly challenged an earlier "no evidence" claim):
--   * The player SPELLCLICKS each Prized Pumpkin (244956) -- CMSG_SPELLCLICK 0x3E002A x4, matching the
--     4-pumpkin objective 461736.
--   * On click the pumpkin casts spell 1236771 (creature_template_spell 244956->1236771) -- its own
--     launch / fly-to-peon visual.
--   * A Hammerfall Peon (249249) follows the player (SMSG_ON_MONSTER_MOVE splines) and casts spell
--     382691 (creature_template_spell 249249->382691) -- the carry visual -- ending the quest visibly
--     carrying 4 pumpkins.
-- Farmer Bruvk (244729) starts the quest. The C++ (zone_arathi_highlands_rpe.cpp) summons a PERSONAL
-- peon on accept that follows the player, accrues a carry stack per click, and self-despawns after
-- turn-in. This SQL wires the spellclick + script names. Supersedes the old gossip-hello credit.

-- (1) Prized Pumpkin 244956: switch from gossip-interact to SPELLCLICK, hand control to the C++ AI.
--     npcflag 0x1000000 = UNIT_NPC_FLAG_SPELLCLICK. Clear the SmartAI so the C++ ScriptName drives it.
UPDATE `creature_template` SET `npcflag`=0x1000000, `AIName`='', `ScriptName`='npc_arathi_rpe_prized_pumpkin' WHERE `entry`=244956;

-- Remove the old SmartAI gossip-hello kill-credit script (the C++ OnSpellClick now grants the credit).
DELETE FROM `smart_scripts` WHERE `entryorguid`=244956 AND `source_type`=0;

-- (2) Spellclick registration. Exactly ONE row -> Creature::InitializeInteractSpellId sets the pumpkin's
--     InteractSpellId so the client offers the click and sends CMSG_SPELLCLICK. castFlags=0 -> the
--     pumpkin casts the spell on itself (its launch visual), matching the wire (244956 casts 1236771).
--     1236771 is a real loaded spell (creature_template_spell 244956->1236771 loaded without error), so
--     the HandleSpellClick AssertSpellInfo is safe. The C++ OnSpellClick does the peon carry + credit.
DELETE FROM `npc_spellclick_spells` WHERE `npc_entry`=244956;
INSERT INTO `npc_spellclick_spells` (`npc_entry`,`spell_id`,`cast_flags`,`user_type`) VALUES
 (244956, 1236771, 0, 0);

-- (3) Script names for the peon (the summoned escort; entry shared with 6 ambient farm peons -- the AI
--     guards on IsSummon() so the ambient ones are inert) and Farmer Bruvk (summons the peon on accept).
UPDATE `creature_template` SET `ScriptName`='npc_arathi_rpe_pumpkin_peon' WHERE `entry`=249249;
UPDATE `creature_template` SET `ScriptName`='npc_arathi_rpe_farmer_bruvk' WHERE `entry`=244729;

-- (4) Faithful capture import: bind the peon's carry spell in creature_template_spell (the capture has
--     249249->382691; only 244956->1236771 was previously imported). Not strictly required -- the C++
--     casts 382691 by id, guarded -- but keeps the template consistent with the capture.
DELETE FROM `creature_template_spell` WHERE `CreatureID`=249249 AND `Spell`=382691;
INSERT INTO `creature_template_spell` (`CreatureID`,`Spell`,`VerifiedBuild`,`Index`) VALUES
 (249249, 382691, 69404, 0);
