-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Final-review fix -- interact-credit SAI
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Plan reference: .superpowers/sdd/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN/final-fix-brief.md
--
-- *** WHY THIS SLICE EXISTS ***
-- Whole-branch final review found quests 90885 "My Beautiful Pumpkins" (recover 4 Prized
-- Pumpkins) and 90895 "Catapult Bombardment" (apply Jaina's Runes to 4 catapults) have NO
-- working completion path. 32_quest_objectives.sql objectives 9088500 (credits creature
-- 244956 Prized Pumpkin) and 9089500 (credits creature 249269 Worn Catapult) rely on
-- Player::KilledMonsterCredit, but 10_creature_template.sql ships 244956/249269 as
-- faction=35 (friendly, unattackable), npcflag=0, no spellclick, no SmartAI -- so nothing
-- a player can do against these entries ever fires a kill-credit. This slice makes them
-- interactable (gossip) and wires a SmartAI GOSSIP_HELLO -> CALL_KILLEDMONSTER(self) ->
-- CLOSE_GOSSIP -> FORCE_DESPAWN chain so interacting is what completes the objective.
--
-- *** BANNER: BLIZZLIKE-EQUIVALENT MECHANISM, NOT THE CONFIRMED RETAIL MECHANIC ***
-- This is a FUNCTIONAL interact-credit that makes 90885/90895 completable end-to-end.
-- Retail almost certainly uses a spellclick (pumpkin) / the "Jaina's Runes" quest-item-use
-- spell (catapult) instead of a gossip-triggered SAI chain -- the exact retail spell is a
-- capture GAP (npc_spellclick_spells was 0-rows-captured this session per the completeness
-- report) and is tracked as a Phase-K item. Do NOT treat this SAI as confirmed retail
-- behavior; it is the blizzlike-equivalent working mechanism chosen so the quest chain is
-- completable in the interim. Phase-K must replace this with the real spellclick/item-use
-- spell once captured, and should re-evaluate whether FORCE_DESPAWN-on-first-interact is
-- correct for a 4-of-4 multi-object objective (each of the 4 pumpkin/catapult WORLD SPAWNS
-- is expected to be a separate object instance credited once each via 20_creature_spawns.sql
-- -- this slice only touches creature_template + smart_scripts, not spawns).
--
-- SmartAI enum values verified against
-- I:/TrinityCore/mythic-plus/TrinityCore/src/server/game/AI/SmartScripts/SmartScriptMgr.h:
--   SMART_EVENT_GOSSIP_HELLO         = 64  (line 167; "noReportUse (for GOs)" param --
--                                            not applicable to creatures, authored as 0)
--   SMART_ACTION_CALL_KILLEDMONSTER  = 33  (line 491; param1 = CreatureId)
--   SMART_ACTION_CLOSE_GOSSIP        = 72  (line 530; no params)
--   SMART_ACTION_FORCE_DESPAWN       = 41  (line 499; param1 = timer)
--   SMART_TARGET_ACTION_INVOKER      = 7   (line 1318; "Unit who caused this Event to occur")
--   SMART_TARGET_SELF                = 1   (line 1312; self cast)
-- Cross-checked against SmartScript.cpp action handlers (same file tree):
--   SMART_ACTION_CALL_KILLEDMONSTER (~line 926): with a specific (non-SELF/NONE) target,
--     iterates `targets` and calls Player::KilledMonsterCredit(action_param1) on any Player
--     target -- so target_type=SMART_TARGET_ACTION_INVOKER (the player who opened gossip)
--     is correct and required for a single-player credit.
--   SMART_ACTION_CLOSE_GOSSIP (~line 1554): iterates `targets`, calls SendCloseGossip() on
--     any Player target -- also needs target_type=SMART_TARGET_ACTION_INVOKER (a gossip
--     "close" only means something for the player, not the creature).
--   SMART_ACTION_FORCE_DESPAWN (~line 1068): iterates `targets`, calls DespawnOrUnsummon()
--     on any Creature/GameObject target -- needs target_type=SMART_TARGET_SELF (the pumpkin/
--     catapult itself, not the player).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (UPDATE is a no-op if already set; smart_scripts DELETE-then-INSERT keyed on
-- entryorguid+source_type -> re-apply safe).
-- ============================================================================

-- ---- Prerequisite: creature_template.AIName='SmartAI' + npcflag=1 (gossip) so interacting
--      opens the gossip interaction that fires SMART_EVENT_GOSSIP_HELLO. faction is left
--      untouched (stays 35, friendly/non-hostile -- these must remain non-attackable
--      interactables, not turned into kill-mobs).
UPDATE `creature_template` SET `AIName` = 'SmartAI', `npcflag` = 1 WHERE `entry` IN (244956, 249269);

-- ---- smart_scripts : standard TC SmartAI idempotency pattern (DELETE then INSERT) ----
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (244956, 249269) AND `source_type` = 0;

INSERT INTO `smart_scripts`
  (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`)
VALUES
-- ---- 244956 Prized Pumpkin (quest 90885 obj 9088500) -- AIName/npcflag set above ----
  -- id0: GOSSIP_HELLO -> CALL_KILLEDMONSTER(self entry), targeting the invoking player
  (244956, 0, 0, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 33, 244956, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'FUNCTIONAL interact-credit (blizzlike-equivalent, NOT confirmed retail mechanism -- retail likely uses spellclick, Phase-K gap): Prized Pumpkin gossip-hello kill-credit for quest 90885 obj 9088500'),
  -- id1: GOSSIP_HELLO -> CLOSE_GOSSIP, targeting the invoking player
  (244956, 0, 1, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'FUNCTIONAL interact-credit: Prized Pumpkin close gossip window on the invoking player after credit'),
  -- id2: GOSSIP_HELLO -> FORCE_DESPAWN (timer=0, immediate), targeting self (consumes the object)
  (244956, 0, 2, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'FUNCTIONAL interact-credit: Prized Pumpkin force-despawn self after credit, consuming the recovered pumpkin'),

-- ---- 249269 Worn Catapult (quest 90895 obj 9089500) -- AIName/npcflag set above ----
  -- id0: GOSSIP_HELLO -> CALL_KILLEDMONSTER(self entry), targeting the invoking player
  (249269, 0, 0, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 33, 249269, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'FUNCTIONAL interact-credit (blizzlike-equivalent, NOT confirmed retail mechanism -- retail likely uses the "Jaina''s Runes" quest-item-use spell, Phase-K gap): Worn Catapult gossip-hello kill-credit for quest 90895 obj 9089500'),
  -- id1: GOSSIP_HELLO -> CLOSE_GOSSIP, targeting the invoking player
  (249269, 0, 1, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'FUNCTIONAL interact-credit: Worn Catapult close gossip window on the invoking player after credit'),
  -- id2: GOSSIP_HELLO -> FORCE_DESPAWN (timer=0, immediate), targeting self (consumes the object)
  (249269, 0, 2, 0, 64, 0, 100, 0, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'FUNCTIONAL interact-credit: Worn Catapult force-despawn self after credit, consuming the destroyed catapult')
;
