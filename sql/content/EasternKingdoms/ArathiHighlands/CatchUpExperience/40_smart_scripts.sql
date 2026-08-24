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
