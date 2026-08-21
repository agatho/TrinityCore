-- ============================================================================
-- ARATHI CATCH-UP / RPE CONSOLIDATION -- BEHAVIOR / NARRATIVE (issue 6)
-- ============================================================================
-- Branch: feature/arathi-rpe   Path: sql/updates/world/master/   Server mapID: 2927
-- Consolidated verbatim from the authoritative content slices (guid block 8000000);
-- runs after 2026_08_21_00 cleanup. Each source slice keeps its own banner + idempotency.
-- smart_scripts + interact-credit -> conversation -> broadcast/npc_text -> creature_text -> scene_template.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 40_smart_scripts.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase F SmartAI combat scripting
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve
--   capture): combatlog_smart_scripts.sql, smart_scripts_candidates.sql,
--   combatlog_creature_template_ainame.sql, combatlog_combatlog_beats.txt.
-- Plan reference: C:/dumps/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN.md sec 1.4 (combat
--   dossier) -- the evidence table reproduced in task-4-brief.md is the authoring
--   source of truth for WHICH triggers/spells get scripted.
--
-- *** REVIEW-ONLY CANDIDATES -- NOT VERIFIED RETAIL BEHAVIOR ***
-- These smart_scripts rows are derived from a SINGLE combat capture session
-- (one player, one pull per boss/trash mob). They are behavioral CANDIDATES for
-- human review, not confirmed blizzlike scripts. Specifically:
--  * Cadence/repeat-timers were NOT captured. HEALTH_PCT (event_type=2) triggers
--    are authored with event_flags=1 (SMART_EVENT_FLAG_NOT_REPEATABLE) and fire
--    ONCE when HP first enters [0,20]% -- this is a conservative default, NOT a
--    confirmed absence of a real repeat/enrage mechanic. A human must confirm
--    real retail cadence before this is treated as ship-ready.
--  * SMART_ACTION_CAST (action_type=11) spell NAMES resolved Phase K (2026-08-21) from
--    SpellName.db2 (build 68275 cache) and annotated inline per id: 305913=Shadow Bolt,
--    317547=Desecrate, 1270769=Whirlwind, 372369=Shoot, 448429=Fireball, 33239=Whirlwind,
--    399062=Boulder Throw. Names are for readability only; the cast ids/logic are unchanged.
--  * AGGRO barks for Runk (244675) and Ro'grok (244709) were NOT directly observed
--    in the combat log (the log only captured HEALTH_PCT/DEATH beats) -- they are
--    authored per task-4-brief.md Requirement 2 (boss aggro+death bark pattern)
--    and are lower-confidence than the HEALTH_PCT/DEATH rows below. The DEATH
--    bark for Ro'grok IS directly evidenced (plan sec 1.4: "event DEATH -> TALK
--    (bark)"). All TALK actions (SMART_ACTION_TALK=1) reference creature_text
--    groupid 0 (aggro) / 1 (death) -- see "-- depends: Task 5 creature_text" tags
--    below. Task 5 (not yet run as of this authoring) owns creature_text; do NOT
--    author creature_text rows in this file.
--  * Runk's (244675) HEALTH_PCT->cast row is INFERRED from the plan's evidence-
--    table pairing ("HEALTH_PCT/DEATH -> ACTION_CAST 305913") mirroring Ro'grok;
--    the raw combat log for entry 244675 only directly captured the DEATH-
--    triggered cast, not a separate HEALTH_PCT-triggered one. Flagged inline;
--    unconfirmed, human must verify before enabling.
--  * HORDE-XVAL ADD (2026-08-21): the Horde run directly observed Runk casting
--    317547 at HP<=30% + death, proving Runk's real repertoire is the SAME pair
--    as Ro'grok {305913, 317547} -- a Runk HEALTH_PCT<=20% -> CAST 317547 row
--    (id4, mirroring Ro'grok id1) is now authored below, cross-capture-confirmed.
--    Spell 317547 = Desecrate (resolved Phase K via SpellName.db2).
--  * EXCLUDED as capture artifacts (present in combatlog_smart_scripts.sql but
--    NOT authored here because they are not corroborated by the plan sec 1.4
--    evidence table, and are almost certainly "last spell observed before the
--    mob died" log noise rather than a genuine scripted on-death cast):
--      244670 DEATH->cast 372369 (evidence table lists 244670 as AGGRO-only)
--      244677 DEATH->cast 448429 (evidence table lists 244677 as AGGRO-only;
--        identical spell id to its own aggro cast -- classic "last cast before
--        death" artifact, not a distinct death mechanic)
--      244709 DEATH->cast 305913 (evidence table's death trigger for Ro'grok is
--        explicitly "-> TALK (bark)", not a cast; this artifact row duplicates
--        the HP<=20% cast id and is the last spell cast before dying)
--      244685 DEATH->cast 1270769 (evidence table lists 244685 as HEALTH_PCT-only) --
--        this DEATH-trigger exclusion still stands (last-cast-before-death noise), BUT
--        see HORDE-XVAL ADD below: 1270769 is now separately authored as a REAL
--        HEALTH_PCT-triggered ability for 244685 (id1), no longer treated as a pure
--        capture artifact -- cross-capture (Alliance HP-adjacent death observation +
--        Horde directly-observed HP<=30% mid-fight cast) confirms it belongs to
--        244685's real kit (also present in 10d_creature_template_spell.sql's advertised
--        spell list for 244685, index 12)
--    Also out of this task's roster entirely (not in the plan sec 1.4 evidence
--    table, so not authored): 244672, 244674, 244676, 244683, 257072.
--  * Entries 244671 (Gnoll Ripper) and 244669 (Scavenging Hyena) are intentionally
--    left with NO smart_scripts rows -- captured autoattack-only, no observed cast.
--  * Out-of-scope combat-bleed entries from other Chromie-Time zones (31228,
--    31233, 32722, 32724, 47649, 54983) are intentionally excluded.
--
-- SmartAI enum values verified against
-- I:/TrinityCore/mythic-plus/TrinityCore/src/server/game/AI/SmartScripts/SmartScriptMgr.h:
--   SMART_EVENT_HEALTH_PCT=2 (params: HPMin%, HPMax%, RepeatMin, RepeatMax)
--   SMART_EVENT_AGGRO=4 (no params)
--   SMART_EVENT_DEATH=6 (no params)
--   SMART_EVENT_FLAG_NOT_REPEATABLE=1
--   SMART_ACTION_TALK=1 (params: groupID, duration, useTalkTarget)
--   SMART_ACTION_CAST=11 (params: SpellId, CastFlags, TriggeredFlags)
--   SMART_TARGET_SELF=1, SMART_TARGET_VICTIM=2
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live
-- DB/realm. Idempotent (DELETE-then-INSERT keyed on entryorguid+source_type ->
-- re-apply safe).
-- ============================================================================

-- ---- Prerequisite: creature_template.AIName='SmartAI' for entries not covered
--      by Task 1 (10_creature_template.sql only set AIName on 244670/244675/
--      244682/244709). Without this, SmartScriptMgr silently ignores the
--      smart_scripts rows below for these three entries. Idempotent (re-running
--      SET AIName='SmartAI' is a no-op if already set).
-- NOTE: 244685 (Ogre Basher) and 244695 (Ettin Crusher) are the elite entries
--   called out in the task brief as missing AIName. 244677 (Kobold Firetender)
--   is ALSO missing AIName in 10_creature_template.sql (not one of the four the
--   brief said Task 1 covered) -- added here for the same reason: its aggro-cast
--   script below would otherwise never fire.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (244677, 244685, 244695);

-- ---- smart_scripts : standard TC SmartAI idempotency pattern (DELETE then INSERT) ----
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (244670, 244675, 244677, 244682, 244685, 244695, 244709) AND `source_type` = 0;

INSERT INTO `smart_scripts`
  (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`)
VALUES
-- ---- 244670 Gnoll Bowblaster (trash) -- AIName already set by Task 1 ----
  -- spell 372369 = Shoot (resolved Phase K via SpellName.db2)
  (244670, 0, 0, 0, 4, 0, 100, 0, 0, 0, 0, 0, 0, 11, 372369, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture): Gnoll Bowblaster aggro cast -- spell 372369 = Shoot (resolved Phase K via SpellName.db2)'),

-- ---- 244677 Kobold Firetender (trash) -- AIName set above in this slice ----
  -- spell 448429 = Fireball (resolved Phase K via SpellName.db2)
  (244677, 0, 0, 0, 4, 0, 100, 0, 0, 0, 0, 0, 0, 11, 448429, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture): Kobold Firetender aggro cast -- spell 448429 = Fireball (resolved Phase K via SpellName.db2); requires creature_template.AIName=SmartAI (see UPDATE above, not set by Task 1)'),

-- ---- 244682 Kobold Waxmancer (trash) -- AIName already set by Task 1 ----
  -- spell 448429 = Fireball (resolved Phase K via SpellName.db2)
  (244682, 0, 0, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 11, 448429, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture, weak signal n=1 per plan sec 1.4): Kobold Waxmancer death cast -- spell 448429 = Fireball (resolved Phase K via SpellName.db2)'),

-- ---- 244685 Ogre Basher (elite) -- AIName set above in this slice ----
  -- spell 33239 = Whirlwind (resolved Phase K via SpellName.db2)
  (244685, 0, 0, 0, 2, 0, 100, 1, 0, 20, 0, 0, 0, 11, 33239, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture, n=2): Ogre Basher HP<=20% cast, single-fire NOT_REPEATABLE, real cadence NOT captured -- spell 33239 = Whirlwind (resolved Phase K via SpellName.db2); requires creature_template.AIName=SmartAI (see UPDATE above, elite not set by Task 1)'),
  -- id1: HORDE-XVAL ADD (2026-08-21) -- HEALTH_PCT<=30% second cast, spell 1270769 = Whirlwind (resolved Phase K via SpellName.db2)
  (244685, 0, 1, 0, 2, 0, 100, 1, 0, 30, 0, 0, 0, 11, 1270769, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'HORDE-XVAL ADD, cross-capture-confirmed: Ogre Basher HP<=30% second cast -- Alliance capture observed 1270769 as a DEATH-adjacent last-observed-cast (treated as artifact, still excluded above), Horde run directly observed it fired mid-fight at HP<=30% (not death), confirming it is a REAL second ability distinct from the HP<=20% 33239 cast id0; single-fire NOT_REPEATABLE, real cadence NOT captured; spell 1270769 = Whirlwind (resolved Phase K via SpellName.db2); also present in 10d_creature_template_spell.sql advertised spell list for 244685 (index 12); requires creature_template.AIName=SmartAI (see UPDATE above)'),

-- ---- 244695 Ettin Crusher (elite) -- AIName set above in this slice ----
  -- spell 399062 = Boulder Throw (resolved Phase K via SpellName.db2)
  (244695, 0, 0, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 11, 399062, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture): Ettin Crusher death cast -- spell 399062 = Boulder Throw (resolved Phase K via SpellName.db2); requires creature_template.AIName=SmartAI (see UPDATE above, elite not set by Task 1)'),

-- ---- 244675 Runk (mini-boss) -- AIName already set by Task 1 ----
  -- id0: aggro bark -- depends: Task 5 creature_text (groupid 0=aggro)
  (244675, 0, 0, 0, 4, 0, 100, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY: Runk aggro bark -- depends: Task 5 creature_text groupid 0 (aggro). NOT directly captured in combat log (log only recorded HEALTH_PCT/DEATH beats); authored per task-4-brief.md Req.2 boss bark pattern -- confirm against Task 5 authored text before enabling'),
  -- id1: HEALTH_PCT<=20% cast -- INFERRED, see banner note. spell 305913 = Shadow Bolt (resolved Phase K via SpellName.db2)
  (244675, 0, 1, 0, 2, 0, 100, 1, 0, 20, 0, 0, 0, 11, 305913, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY, INFERRED (NOT directly captured for this entry): Runk HP<=20% cast mirrors Ro''grok pairing per plan sec 1.4 evidence table ("HEALTH_PCT/DEATH -> ACTION_CAST 305913"); raw combat log for 244675 only directly captured the DEATH-triggered cast below, not a separate HEALTH_PCT one -- unconfirmed, human must verify before enabling; single-fire NOT_REPEATABLE, cadence NOT captured; spell 305913 = Shadow Bolt (resolved Phase K via SpellName.db2)'),
  -- id2: DEATH cast, directly captured. spell 305913 = Shadow Bolt (resolved Phase K via SpellName.db2)
  (244675, 0, 2, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 11, 305913, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture, directly observed UNIT_DIED last-cast): Runk death cast -- spell 305913 = Shadow Bolt (resolved Phase K via SpellName.db2)'),
  -- id3: death bark -- depends: Task 5 creature_text (groupid 1=death)
  (244675, 0, 3, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY: Runk death bark -- depends: Task 5 creature_text groupid 1 (death). Directly evidenced by plan sec 1.4 ("HEALTH_PCT/DEATH -> ACTION_CAST 305913 + TALK")'),
  -- id4: HORDE-XVAL ADD (2026-08-21) -- HEALTH_PCT<=20% paired finisher cast, mirrors Ro'grok id1. spell 317547 = Desecrate (resolved Phase K via SpellName.db2)
  (244675, 0, 4, 0, 2, 0, 100, 1, 0, 20, 0, 0, 0, 11, 317547, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'HORDE-XVAL ADD, cross-capture-confirmed: Horde run directly observed Runk casting 317547 at HP<=30% + death, proving Runk''s real repertoire is the SAME {305913, 317547} finisher pair as Ro''grok 244709 (mirrors Ro''grok''s id1 row exactly: HP<=20% -> CAST 317547). Runk''s existing id1 (305913 HP<=20%, INFERRED) remains flagged separately -- this new row is the corroborated second half of the pair; single-fire NOT_REPEATABLE, real cadence NOT captured; spell 317547 = Desecrate (resolved Phase K via SpellName.db2); also present in 10d_creature_template_spell.sql advertised spell list for 244675 (index 16)'),

-- ---- 244709 Ro'grok (final boss) -- AIName already set by Task 1 ----
  -- id0: HEALTH_PCT<=20% primary cast, n=4. spell 305913 = Shadow Bolt (resolved Phase K via SpellName.db2)
  (244709, 0, 0, 0, 2, 0, 100, 1, 0, 20, 0, 0, 0, 11, 305913, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture, n=4 per plan sec 1.4): Ro''grok HP<=20% cast, single-fire NOT_REPEATABLE, real cadence NOT captured -- spell 305913 = Shadow Bolt (resolved Phase K via SpellName.db2)'),
  -- id1: HEALTH_PCT<=20% paired finisher, n=2. spell 317547 = Desecrate (resolved Phase K via SpellName.db2)
  (244709, 0, 1, 0, 2, 0, 100, 1, 0, 20, 0, 0, 0, 11, 317547, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (1 combat capture, n=2 per plan sec 1.4): Ro''grok paired finisher cast, HP<=20%, single-fire NOT_REPEATABLE, real cadence NOT captured -- spell 317547 = Desecrate (resolved Phase K via SpellName.db2)'),
  -- id2: aggro bark -- depends: Task 5 creature_text (groupid 0=aggro)
  (244709, 0, 2, 0, 4, 0, 100, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY: Ro''grok aggro bark -- depends: Task 5 creature_text groupid 0 (aggro). NOT directly captured in combat log; authored per task-4-brief.md Req.2 boss bark pattern -- confirm against Task 5 authored text before enabling'),
  -- id3: death bark, directly evidenced (plan sec 1.4 lists death trigger as TALK for Ro'grok, not a cast)
  (244709, 0, 3, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 'REVIEW-ONLY (directly evidenced, plan sec 1.4: "death -> TALK (bark)"): Ro''grok death bark -- depends: Task 5 creature_text groupid 1 (death). NOTE: raw combat log also recorded a UNIT_DIED last-observed-cast of spell 305913 for this entry, identical to its HP<=20% cast id0 -- treated as a logging artifact (last spell cast before dying, not a distinct scripted death-cast per the evidence table) and intentionally NOT authored as a second DEATH->CAST row; only the TALK bark is authored here')
;


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 41_interact_credit.sql  <<<<<<<<<<<<<<<<<<<<

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


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 50_conversation.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (conversations)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve capture)
-- Sources used: conversation_template.sql (7 rows), conversation_actors.sql (10 rows,
--   1 EXCLUDED, see below), conversation_line_template.sql (bundle body empty --
--   new=0 changed=0 same=11, i.e. no captured divergence from reference; rows below
--   authored explicitly anyway for documentation/completeness per task-5-brief Req.1),
--   conversation_groups.txt (conversationId=0x021826bb group, 16 raw lines, cross-
--   referenced against the DB2-backed NextConversationLineID chains reproduced in that
--   file), C:/dumps/tcharvest/out/db2_csv/ConversationLine.csv (per-line
--   BroadcastTextID/NextConversationLineID -- this CSV has NO ActorIdx column, so
--   per-line actor resolution below is TODO Phase K where more than one actor exists).
-- Plan reference: C:/dumps/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN.md sec 1.5.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================
--
-- SCOPE: only the 7 in-scope ConversationIds from plan sec 1.5 are authored below.
-- conversation_actors.sql's 10th row -- (17844, 64220, Idx 1, CreatureId 184650,
-- CreatureDisplayInfoId 107146) -- is Wrathion/Dragonflight bleed (Conversation 17844,
-- creature entry 184650, the Obsidian Warders intro at Stormwind) and is INTENTIONALLY
-- EXCLUDED, per task-5-brief explicit instruction. Conversation 17844 / creature entry
-- 184650 do NOT appear anywhere in this file.
--
-- The 7 conversations (ConvId -> FirstLineId -> bind-quest [inferred], plan sec 1.5):
--   29725 -> 82130 (bcast 290566 "Good to see you, $n. We must take back Hammerfall.") -> 90882 accept
--   29726 -> 82131 (bcast 290567 "The highlands have seen more than enough conflict...") -> 90882
--   29727 -> 82136 (player + actor 107910; BroadcastTextId=0 for this line -- no on-screen
--                   text captured, likely an emote/camera-only conversation beat) -> 90883
--   29728 -> 82137 (bcast 290571 "Raiding farms... gathering supplies for a larger strike.") -> 90886
--   29730 -> 82140 (bcast 290573 "The plans you recovered suggest that Stromgarde is their next target...") -> 90888
--   29735 -> 82148/82149 (bcast 290583/290584, two-line farewell exchange: "I will return to
--                   Hammerfall..."/"And I'll do the same for Stromgarde...") -> 90897
--   30602 -> 84222 (bcast 295548 "Not over yet... me can get out! Start new attack!" -- Ro'grok) -> 90896
--
-- REQUIREMENT 4 (GAP): conversation -> quest binding is a capture GAP -- 0
-- SMART_ACTION_CREATE_CONVERSATION triggers were found in the capture (see
-- quest_conversation_triggers.txt / quest_conversation_smart_scripts_candidates.sql --
-- both empty of confirmed hits for these 7 ConvIds). Do NOT author smart_scripts here;
-- Task 4 (40_smart_scripts.sql) owns all SmartAI for this content slice and did not
-- author conversation-trigger scripts either (out of its scope). As AUTHORING GUIDANCE
-- for a Phase-K follow-up, each conversation is intended to fire via
-- SMART_ACTION_CREATE_CONVERSATION (action_type=98) keyed off the bind-quest above:
--   29725/29726 -> quest 90882 accept (SMART_EVENT_ACCEPTED_QUEST or gossip-on-accept,
--                  on the Jaina/Thrall quest giver -- two sequential conv beats)
--   29727       -> quest 90883 objective/accept beat (actor 107910 interact)
--   29728       -> quest 90886 objective progress beat (farm-raid investigation)
--   29730       -> quest 90888 accept/objective beat (Stromgarde intel reveal)
--   29735       -> quest 90897 SMART_EVENT_QUEST_REWARDED (Jaina/Thrall farewell,
--                  matches the "Hammerfall garrison turns hostile" story beat flagged
--                  in 10_creature_template.sql for this same quest)
--   30602       -> quest 90896 SMART_EVENT_DEATH or HEALTH_PCT beat on Ro'grok (244709)
--                  -- "Not over yet... me can get out! Start new attack!" reads as a
--                  mid-fight escalation line, not a death line (compare creature_text
--                  244709 groupid 1 "Arathi... never... be ours..." which IS the death
--                  bark, see 51_creature_text.sql) -- likely a HEALTH_PCT-triggered
--                  conversation, not on-death; human must confirm against retail before
--                  wiring the SmartAI event.
-- None of the above SMART_ACTION_CREATE_CONVERSATION rows are authored in this file or
-- in 40_smart_scripts.sql -- this is guidance only, left for Phase K.
-- ============================================================================

-- ---- SECTION 1: conversation_template (7 rows) ----
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29725, 82130) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29726, 82131) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29727, 82136) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29728, 82137) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29730, 82140) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (29735, 82148) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);
INSERT INTO `conversation_template` (`Id`, `FirstLineId`) VALUES (30602, 84222) ON DUPLICATE KEY UPDATE `FirstLineId`=VALUES(`FirstLineId`);

-- ---- SECTION 2: conversation_actors (9 rows -- 10th bundle row is Wrathion/Dragonflight
--      bleed, ConversationId 17844, EXCLUDED per scope above). ActorId values (107908,
--      107909, 107910, 107913, 107917, 107937, 107938, 109705) are ConversationActor.db2
--      references, not creature_template entries -- CreatureId=0 on every row below means
--      the actor's model/name is resolved entirely from that DB2, so there is no FK
--      relationship to the Task 1 creature_template roster to verify here. ----
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29725, 107908, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29726, 107909, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
  -- 29727 Idx0 = the player themself (ActivePlayerObject=1), Idx1 = creature actor 107910
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29727, 0, 0, 0, 0, 0, 1) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29727, 107910, 1, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29728, 107913, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29730, 107917, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
  -- 29735 two-actor farewell exchange: Idx0 = actor 107938, Idx1 = actor 107937 (see
  -- conversation_line_template ActorIdx TODO below -- which actor speaks which line
  -- is unresolved from this capture)
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29735, 107938, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (29735, 107937, 1, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);
INSERT INTO `conversation_actors` (`ConversationId`, `ConversationActorId`, `Idx`, `CreatureId`, `CreatureDisplayInfoId`, `NoActorObject`, `ActivePlayerObject`) VALUES (30602, 109705, 0, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `ConversationActorId`=VALUES(`ConversationActorId`), `CreatureId`=VALUES(`CreatureId`), `CreatureDisplayInfoId`=VALUES(`CreatureDisplayInfoId`), `NoActorObject`=VALUES(`NoActorObject`), `ActivePlayerObject`=VALUES(`ActivePlayerObject`);

-- ---- SECTION 3: conversation_line_template (8 rows -- the 8 distinct ConversationLine
--      IDs reached by our 7 conversations' chains: 82130, 82131, 82136, 82137, 82140,
--      82148, 82149, 84222). Schema per ConversationDataStore.cpp::LoadConversationTemplates:
--      (Id, UiCameraID, ActorIdx, Flags, ChatType); Id must exist in ConversationLine.db2
--      (all 8 do -- confirmed via ConversationLine.csv). UiCameraID/Flags/ChatType are not
--      captured by this bundle -- authored as 0 (engine default), not fabricated non-zero
--      values. ActorIdx: ConversationLine.csv carries NO ActorIdx column, so it cannot be
--      resolved from this source for any line; single-actor conversations (29725, 29726,
--      29728, 29730, 30602 -- one ConversationActor at Idx 0) are safely authored as
--      ActorIdx=0 (the only actor). The two multi-actor conversations (29727: player+
--      actor 107910; 29735: actor 107938+actor 107937) get an explicit TODO Phase K tag
--      below since ActorIdx=0 may be wrong for lines actually spoken by the Idx-1 actor. ----
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82130, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82131, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
  -- TODO Phase K: actor Idx from ConversationLine.db2 -- conv 29727 has 2 actors (player Idx0, creature 107910 Idx1); BroadcastTextId=0 for this line (no on-screen text captured) so ActorIdx=0 default is low-risk, but unconfirmed
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82136, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82137, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82140, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
  -- TODO Phase K: actor Idx from ConversationLine.db2 -- conv 29735 has 2 actors (107938 Idx0, 107937 Idx1) exchanging farewell lines 82148/82149; ConversationLine.csv has no ActorIdx column to disambiguate which actor speaks which line, authored as ActorIdx=0 pending confirmation
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82148, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
  -- TODO Phase K: actor Idx from ConversationLine.db2 -- see 82148 note; this is the second (likely Idx1) speaker's line
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (82149, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);
INSERT INTO `conversation_line_template` (`Id`, `UiCameraID`, `ActorIdx`, `Flags`, `ChatType`) VALUES (84222, 0, 0, 0, 0) ON DUPLICATE KEY UPDATE `UiCameraID`=VALUES(`UiCameraID`), `ActorIdx`=VALUES(`ActorIdx`), `Flags`=VALUES(`Flags`), `ChatType`=VALUES(`ChatType`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 50b_broadcast_text.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (broadcast_text)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve
--   capture): conversation_groups.txt (conversationId=0x021826bb group text) cross-
--   referenced against broadcast_text.sql (bundle body empty -- new=0 changed=0 same=16,
--   i.e. all captured broadcastTextIds already resolve unchanged against the reference;
--   no diff to ship. This file re-authors the 7 in-scope rows explicitly and idempotently
--   for this content slice's self-containment, per task-5-brief Req.2).
--
-- *** TARGET DATABASE: HOTFIXES, NOT WORLD ***
-- broadcast_text in this core version is served from the HOTFIXES database
-- (I:/TrinityCore/mythic-plus/TrinityCore/sql/base/dev/hotfixes_database.sql:1372),
-- keyed PRIMARY KEY (`ID`,`VerifiedBuild`). This file is a hotfixes-targeted candidate
-- slice; it is kept in this feature directory alongside the world-DB slices for
-- discoverability, but MUST be applied to the hotfixes DB, not world. VerifiedBuild is
-- authored as 69382 (the capture build -- normalized, see note below; was previously
-- 68887, the project's target client build, per mythic-plus-project-layout memory note).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to
-- a live DB/realm. Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
--
-- BUILD NORMALIZE (final-review fix): VerifiedBuild below is authored as 69382 (the
-- capture build), not 68887 (project target client build). 62_npc_text.sql's own
-- broadcast_text rows (290606/290473) and the TCHarvest capture session are both 69382;
-- this file previously used 68887, which was inconsistent within the same feature's
-- broadcast_text rows. Normalized here for consistency -- see
-- .superpowers/sdd/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN/final-fix-brief.md Req.2.
--
-- SCOPE: only the 7 broadcastTextIds actually reached by the 7 in-scope conversations'
-- FirstLineId/chain (see 50_conversation.sql Section 1 comment table) are authored here.
-- The conversation_groups.txt 0x021826bb group captured 16 raw broadcastTextIds total,
-- but 9 of them are out of scope for this task and are NOT authored:
--   290606, 290569, 290576, 290473 -- belong to OTHER lines in the same client-side
--     conversation stream (82133/82141/unresolved) not part of our 7 ConvIds' chains.
--   225160, 232496, 226324, 215556, 227404 -- Wrathion/Dragonflight "Obsidian Warders"
--     intro bleed (Stormwind, Aspects' invitation) -- EXCLUDED, matches the
--     conversation_actors.sql Conversation 17844/creature 184650 exclusion in
--     50_conversation.sql; do not author.
-- Line 82136 (conversation 29727) has BroadcastTextId=0 in ConversationLine.csv --
-- no text was ever assigned to that line client-side, so there is no 8th broadcast_text
-- row to author for it (not a gap -- this is a captured, confirmed zero).
-- ============================================================================

INSERT INTO `broadcast_text` (`ID`, `VerifiedBuild`, `Text`, `Text1`, `LanguageID`, `ConditionID`, `EmotesID`, `Flags`, `ChatBubbleDurationMs`, `VoiceOverPriorityID`, `SoundKitID1`, `SoundKitID2`, `EmoteID1`, `EmoteID2`, `EmoteID3`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`)
VALUES
  -- conv 29725 / line 82130 -- bind-quest 90882 [inferred]
  (290566, 69382, 'Good to see you, $n. We must take back Hammerfall.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29726 / line 82131 -- bind-quest 90882 [inferred]
  (290567, 69382, 'The highlands have seen more than enough conflict. We have to end this before it can escalate.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29728 / line 82137 -- bind-quest 90886 [inferred]
  (290571, 69382, 'Raiding farms... the ogres and kobolds must be gathering supplies for a larger strike.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29730 / line 82140 -- bind-quest 90888 [inferred]
  (290573, 69382, 'The plans you recovered suggest that Stromgarde is their next target. We must make haste!', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29735 / line 82148 (first of farewell pair) -- bind-quest 90897 [inferred]
  (290583, 69382, 'I will return to Hammerfall to help with the repairs.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 29735 / line 82149 (second of farewell pair) -- bind-quest 90897 [inferred]
  (290584, 69382, 'And I''ll do the same for Stromgarde. We could all use a moment of respite.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- conv 30602 / line 84222 -- bind-quest 90896 [inferred] -- Ro'grok (244709) mid-fight escalation line, see 50_conversation.sql guidance note
  (295548, 69382, 'Not over yet... me can get out! Start new attack!', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Text1`=VALUES(`Text1`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 62_npc_text.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase H npc_text (gossip bodies)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/addon_npc_text.sql (8 raw
--   rows). Only the 2 rows belonging to NPCs THIS FEATURE OWNS (245026 Win'sa, 244714
--   Jaina) are authored here per task-6-brief Req.5; the other 6 (npc:3370 guild, npc:5188
--   Chromie-hub tabard vendor, npc:167032 Chromie, npc:189600/189603 dracthyr intro,
--   npc:241677 Sunwell) are OUT OF SCOPE and are not authored. Chromie (167032) in
--   particular is a map-85 hub NPC never owned by this feature -- see FIX ROUND 1 note
--   in 61_gossip.sql: her real gossip menu (25426) already ships elsewhere on this
--   branch, and this file must not touch her data (removed here, was previously
--   authored present-but-inert; see 61_gossip.sql's reference block for the captured
--   text, preserved there for provenance instead).
--
-- SCHEMA NOTE: `npc_text` has no direct text column -- gossip body text is indirected
-- through `BroadcastTextID0..7` (hotfixes-DB lookup). The bundle's addon_npc_text.sql
-- gives only raw plain-text strings keyed by a placeholder 'npc:<id>' id (not a literal,
-- insertable npc_text row). Real broadcastTextIds for both in-scope lines were cross-found
-- verbatim in conversation_groups.txt (same session capture, same exact text strings).
--
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
-- ============================================================================

-- ============================================================================
-- SECTION 1 -- broadcast_text
-- *** TARGET DATABASE: HOTFIXES, NOT WORLD ***
-- Same split as 50b_broadcast_text.sql (Phase G): broadcast_text in this core version is
-- served from the HOTFIXES database (sql/base/dev/hotfixes_database.sql), PK
-- (`ID`,`VerifiedBuild`). Kept in this world-DB-targeted directory for discoverability;
-- MUST be applied to the hotfixes DB, not world.
-- Provenance: conversation_groups.txt, conversationId=0x021826bb group (same capture
-- session as 50_conversation.sql / 50b_broadcast_text.sql) -- these 2 lines are in that
-- group but were NOT among the 7 in-scope ConversationLine chains 50b authored, so they
-- are authored here instead, for their actual use (npc_text gossip greetings).
-- ============================================================================
INSERT INTO `broadcast_text` (`ID`, `VerifiedBuild`, `Text`, `Text1`, `LanguageID`, `ConditionID`, `EmotesID`, `Flags`, `ChatBubbleDurationMs`, `VoiceOverPriorityID`, `SoundKitID1`, `SoundKitID2`, `EmoteID1`, `EmoteID2`, `EmoteID3`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`)
VALUES
  -- Win'sa (245026) gossip greeting -- conversation_groups.txt broadcastTextId=290606
  (290606, 69382, 'I got what ya need here.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  -- Lady Jaina Proudmoore (244714) gossip greeting -- conversation_groups.txt broadcastTextId=290473
  (290473, 69382, 'I know of a few places that could use your help.', '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Text1`=VALUES(`Text1`);

-- ============================================================================
-- SECTION 2 -- npc_text
-- ============================================================================
INSERT INTO `npc_text` (`ID`, `Probability0`, `BroadcastTextID0`, `VerifiedBuild`) VALUES
(39386, 1, 290606, 69382), -- Win'sa: "I got what ya need here."
(39348, 1, 290473, 69382)  -- Jaina: "I know of a few places that could use your help."
ON DUPLICATE KEY UPDATE `BroadcastTextID0`=VALUES(`BroadcastTextID0`), `VerifiedBuild`=VALUES(`VerifiedBuild`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 51_creature_text.sql  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (creature_text)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/creature_text.sql
--   (16 rows, new=16 changed=0 same=16 against the reference -- captured verbatim).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live
-- DB/realm. Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe).
--
-- ROSTER CHECK: all 10 distinct CreatureIDs below (244670, 244671, 244672, 244675,
-- 244655, 244658, 244677, 244682, 244709, 257072) are confirmed present in Task 1's
-- authored roster (10_creature_template.sql) -- grep-verified, no entries outside that
-- file's ~46-entry curated set.
--
-- EMOTE COLUMN: every row in the captured bundle has Emote=0 (no visible emote played
-- alongside the bark). Authored as captured -- 0 is a valid "no emote" value, so no
-- "-- Emote unverified (Phase K Emotes.db2)" tag is needed on any row below (that tag
-- would only apply to a captured non-zero Emote id, and none exist in this bundle).
--
-- *** TASK-4 CONTRACT: Runk (244675) / Ro'grok (244709) GroupID remap ***
-- 40_smart_scripts.sql's SMART_ACTION_TALK rows for Runk and Ro'grok reference
-- creature_text groupid 0 (SMART_EVENT_AGGRO) and groupid 1 (SMART_EVENT_DEATH),
-- target_type SMART_TARGET_SELF (see that file's "id0: aggro bark -- depends: Task 5
-- creature_text (groupid 0=aggro)" / "id3: death bark -- depends: Task 5 creature_text
-- (groupid 1=death)" comments for both bosses). The RAW capture, however, has BOTH of
-- each boss's lines under GroupID=0 (as sequential IDs 0 and 1 within that one group --
-- see the bundle SQL: (244675,0,0,...)/(244675,0,1,...) and (244709,0,0,...)/
-- (244709,0,1,...)). That raw grouping cannot satisfy Task 4's aggro/death split, so the
-- two rows for EACH of these two bosses are deliberately REMAPPED below:
--   GroupID 0 (aggro) <- the boastful "we will win" line (ID reset to 0 within the group)
--   GroupID 1 (death) <- the anguished/last-words line (ID reset to 0 within its group)
-- This split is not arbitrary: the line text itself disambiguates aggro vs. death ("We
-- take farm, Stromgarde, then ALL Arathi!" / "Me still win! Destroy ALL in Arathi!" are
-- defiant boasts fitting SMART_EVENT_AGGRO; "How... plan... fail?" / "Arathi... never...
-- be ours..." are last words fitting SMART_EVENT_DEATH), and for Ro'grok it is also
-- independently corroborated by the plan sec 1.4 combat-log evidence table, which
-- directly evidences Ro'grok's death trigger as "-> TALK (bark)" (reproduced in
-- 40_smart_scripts.sql's banner). No groupid collisions result: each boss now has
-- exactly one line in GroupID 0 and one line in GroupID 1.
-- All 12 remaining rows (244670, 244671, 244672, 244677 x3, 244655, 244658, 257072 x3,
-- 244682) have no Task 4 dependency and keep their captured GroupID/ID exactly as
-- harvested.
-- ============================================================================

INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244670, 0, 0, 'No more nasty Horde!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244671, 0, 0, 'Wanted... tasty... meats...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244672, 0, 0, 'Wanted... tasty... meats...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244677, 0, 0, 'We get LOTS of candles for this!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244677, 0, 1, 'Candle... burn... no more...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244677, 0, 2, 'Maybe... make.. bad... deal...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244655, 0, 0, 'The plans you recovered suggest that Stromgarde is their next target. We must make haste!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244658, 0, 0, 'Thrall and I will locate their leader. Meet up with us once you''ve disrupted their forces.', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 0, 'Burn... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 1, 'We take Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
  -- captured duplicate of GroupID0/ID0 text -- preserved as harvested, not deduplicated
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 2, 'Burn... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244682, 0, 0, 'We take ogre deal! You no stop!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- Runk 244675 -- REMAPPED to satisfy Task 4 groupid 0=aggro / 1=death contract (see banner) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244675, 0, 0, 'We take farm, Stromgarde, then ALL Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244675, 1, 0, 'How... plan... fail?', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- Ro'grok 244709 -- REMAPPED to satisfy Task 4 groupid 0=aggro / 1=death contract (see banner) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244709, 0, 0, 'Me still win! Destroy ALL in Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244709, 1, 0, 'Arathi... never... be ours...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ============================================================================
-- HORDE-XVAL ADD (2026-08-21): additional ambient bark pool lines
-- ============================================================================
-- The Horde run (C:/dumps/tcharvest/out/catchup_horde/zone_2927/creature_text.sql, 29
-- rows) captured a LARGER bark pool for several mobs already in the roster above, plus
-- 3 mobs (244676, 244683, 244685) that had zero creature_text rows in this file until
-- now. Every line below is copied VERBATIM (Text + Emote=0, all rows in the Horde bundle
-- have Emote=0) from that Horde capture; nothing invented. All Type=12 (say). New
-- GroupID/ID assigned per creature to avoid colliding with the existing rows above.
-- Ambient villain flavor -- flagged single-source-run (Horde only) same as the rest of
-- this file's confidence tier.
--
-- Two Horde-side duplicate rows were intentionally NOT copied (already represented by
-- an existing/added value in that creature's own pool, so re-adding the identical text
-- would add no new information):
--   244676 id3 'Food for siege! Not for you!' (dup of id1, same creature)
--   257072 id2 'We take Arathi!' (dup of this file's existing 257072 GroupID0/ID1)
-- ============================================================================

-- ---- 244670 Gnoll Bowblaster -- 2 new pool lines (Horde ids 0,1; existing GroupID0/ID0 kept) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244670, 0, 1, 'This place ours now!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244670, 0, 2, 'Wanted... tasty... meats...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 244671 Gnoll Ripper -- 2 new pool lines (Horde ids 0,1; existing GroupID0/ID0 kept) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244671, 0, 1, 'Down with Horde!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244671, 0, 2, 'Kill... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 244672 Gnoll Bruiser -- 2 new pool lines (Horde ids 0,1; existing GroupID0/ID0 kept) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244672, 0, 1, 'No more nasty Horde!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244672, 0, 2, 'Burn... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 257072 Gnoll Biter -- 3 new pool lines (Horde ids 0,1,3; existing GroupID0/ID0-2 kept; Horde id2 dup skipped, see banner) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 3, 'Wanted... tasty... meats...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 4, 'This place ours now!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (257072, 0, 5, 'Kill... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 244676 Kobold Pillager -- brand-new pool, 3 lines (Horde ids 0,1,2; id3 dup skipped, see banner) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244676, 0, 0, 'We use food! You no use!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244676, 0, 1, 'Food for siege! Not for you!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244676, 0, 2, 'We steal food! You no stop!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 244682 Kobold Waxmancer -- 4 new pool lines added (Horde ids 2,3,4,5; existing GroupID0/ID0 kept) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244682, 0, 1, 'Candle... burn... no more...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244682, 0, 2, 'Maybe... make.. bad... deal...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244682, 0, 3, 'No candle... for me...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244682, 0, 4, 'We get LOTS of candles for this!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 244683 Gnoll Prowler -- brand-new pool, 2 lines (Horde ids 0,1) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244683, 0, 0, 'We take Arathi!', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244683, 0, 1, 'Burn... all...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);

-- ---- 244685 Ogre Basher -- brand-new pool, 1 line (Horde id0) ----
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `BroadcastTextId`) VALUES (244685, 0, 0, 'Stromgarde... gotta... die...', 12, 0, 100.0, 0, 0, 0) ON DUPLICATE KEY UPDATE `Text`=VALUES(`Text`), `Type`=VALUES(`Type`), `Language`=VALUES(`Language`), `Probability`=VALUES(`Probability`), `Emote`=VALUES(`Emote`), `Duration`=VALUES(`Duration`), `BroadcastTextId`=VALUES(`BroadcastTextId`);


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: content 52_scene_template.sql (header-only gap doc, for provenance)  <<<<<<<<<<<<<<<<<<<<

-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase G narrative layer (scene_template)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)
-- Source bundle: C:/dumps/tcharvest/out/catchup_zone/zone_2796/ (TCHarvest self-serve
--   capture): scene_template.sql, scene_triggers_review.txt, sniff_scene_confidence.txt,
--   addon_cinematic_capture.txt, addon_movie_capture.txt, addon_subtitle_capture.txt.
--
-- *** HEADER-ONLY FILE -- NO SQL STATEMENTS BELOW. This is deliberate. ***
-- Per task-5-brief Requirement 5: 3 scenes fire during this content's playthrough, but
-- their SceneIds are a capture GAP (withheld/"same"), and the intro cinematic's
-- CinematicSequenceID is likewise unresolved. Do NOT invent SceneId, CinematicSequences,
-- or Movie IDs to fill this gap -- none are fabricated anywhere in this file. This file
-- exists solely to document the GAP and preserve the Phase-K resolution key (the intro
-- cinematic's 10-line subtitle fingerprint) so a future capture/DB2-lookup pass can
-- resolve the real IDs without re-deriving this evidence from scratch.
--
-- ---- GAP 1: 3 in-scope scenes, SceneIds withheld ----
-- sniff_scene_confidence.txt: scene_opcode=0x4500DF (matches=6/6), scenes_observed=3,
-- validated=3, divergent=0, new_candidates=0. "validated=3" means all 3 observed scenes
-- matched EXISTING scene_template rows already known to the reference/DB2 (i.e. these
-- are not custom net-new scenes needing a new SceneId allocated) -- but the confidence
-- artifact records only the match COUNT, not the numeric SceneId values themselves.
-- scene_template.sql (bundle) is correspondingly empty (new=0 changed=0 same=3): TCHarvest
-- found 3 matches against the reference and therefore emitted no candidate INSERT rows
-- (nothing new to add) -- so the actual SceneId integers are not present anywhere in this
-- bundle for us to read out and re-author here. AUTHORING GUIDANCE for Phase K: obtain
-- SceneId values via CASC-side DB2 SceneScript/SceneScriptPackage lookup keyed by
-- questId in {90882, 90883, 90886, 90888, 90897, 90896} (the bind-quests documented in
-- 50_conversation.sql), or via a fresh sniff capture on this build (68887) with the
-- SceneId field unmasked, before any scene_template row is authored for this content.
--
-- ---- GAP 2: intro cinematic, CinematicSequenceID unresolved ----
-- addon_cinematic_capture.txt: 1 row, cinematicID='?' (unresolved), nargs=2, map=2451
-- (client uiMapID for Arathi Highlands -- matches this zone), no last_npc/context_quests
-- captured. This is presumably the Catch-Up Experience's intro cutscene (the "Arathi
-- Highlands, once bitterly contested..." narration -- see fingerprint below), but its
-- numeric CinematicSequenceID was not captured. Per
-- addon_movie_capture.txt / addon_cinematic_capture.txt banners: "cinematic/movie
-- triggers have no world-DB table (WPP finding: MiscellaneousHandler's
-- TRIGGER_CINEMATIC/TRIGGER_MOVIE have no Storage.*/builder call)" -- i.e. even once the
-- ID is known, it is NOT a scene_template/world-DB row at all; it would be wired via a
-- SmartAI action (SMART_ACTION_PLAY_CINEMATIC) on a quest-accept/quest-complete trigger,
-- out of this file's scope entirely.
--
-- addon_movie_capture.txt's single captured row (movieID=470, map=85, last_npc=167032,
-- context_quests=51443/62568) is EXPLICITLY NOT part of this content's narrative: map=85
-- is the generic Eastern Kingdoms continent id (not our zone-specific map 2796/uiMap
-- 2451), and quest ids 51443/62568 are outside the Catch-Up Experience's 90882-90911
-- quest range. Excluded as unrelated capture bleed; not used to resolve GAP 2.
--
-- ---- Phase-K resolution key: intro cinematic 10-line subtitle fingerprint ----
-- addon_subtitle_capture.txt (10 rows, SHOW_SUBTITLE ordered transcript, tagged
-- "cinematic#1"). Reproduced here VERBATIM in seq order as the human/DB2-search anchor
-- for identifying the correct CinematicSequenceID (search CinematicSequences.db2 /
-- narration text for this exact 10-line monologue; no sender/speaker was captured,
-- consistent with narrator-only VO):
--   seq 1:  "The Arathi Highlands, once bitterly contested,"
--   seq 2:  "have seen a tenuous ceasefire between the Horde and the Alliance."
--   seq 3:  "But though the armistice brought a measure of peace,"
--   seq 4:  "a new threat has thrown the region into chaos."
--   seq 5:  "Stromgarde is under siege,"
--   seq 6:  "and Hammerfall has suffered heavy losses."
--   seq 7:  "You have answered the call for aid"
--   seq 8:  "and stand ready to join the fight."
--   seq 9:  "Band together with the Horde and the Alliance"
--   seq 10: "to secure the Arathi Highlands!"
--
-- No SceneId, CinematicSequences, or Movie ID is authored below. This file intentionally
-- contains zero INSERT/UPDATE statements.
-- ============================================================================


-- >>>>>>>>>>>>>>>>>>>>  SOURCE: PRESERVED 50-57 scene_template (file 56)  <<<<<<<<<<<<<<<<<<<<

-- ---------------------------------------------------------------------------
-- PRESERVED from 2026_08_15_56 (unique bit kept from Set B): the two pad-scene
-- scene_template rows. SPELL_AURA_PLAY_SCENE (430) carries the SceneID; without a
-- scene_template row the aura has nothing to play.
--   spell 1237116 -> SceneID 3692 (ambient pad)
--   spell 1248494 -> SceneID 3749 (Jaina stasis presentation)
-- UNVERIFIED: scene ids 3692/3749, Flags, ScriptPackageID (4617/4681), spells
--             1237116/1248494 (third-party 68453 capture, not re-verified here).
-- (Content 52_scene_template.sql is header-only -- it documented these SceneIds as a
--  capture GAP; Set B's file 56 actually resolved them, so Set B's rows are authoritative.)
-- ---------------------------------------------------------------------------
DELETE FROM `scene_template` WHERE `SceneId` IN (3692, 3749);
INSERT INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `Encrypted`, `ScriptName`) VALUES
(3692, 16, 4617, 0, ''), -- ambient pad; played by spell 1237116
(3749, 17, 4681, 0, ''); -- Jaina stasis presentation; played by spell 1248494
