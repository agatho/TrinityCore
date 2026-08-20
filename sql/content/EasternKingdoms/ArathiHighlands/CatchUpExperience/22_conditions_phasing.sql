-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D phasing conditions
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2796  (client uiMapID 2451 is display-only, not used here)
-- Depends on: 20_creature_spawns.sql (owns the canonical PhaseId map, reproduced below),
--   21_phase_area.sql (AreaId->PhaseId map these conditions attach to via
--   ConditionMgr::addToPhases -- SourceEntry=0 applies a condition to every AreaId
--   already mapped to that PhaseId in `phase_area`, so one row per PhaseId is enough).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is the full
--   SourceTypeOrReferenceId/SourceGroup/SourceEntry/SourceId/ElseGroup/
--   ConditionTypeOrReference/ConditionTarget/ConditionValue1/2/3/ConditionStringValue1
--   tuple, per `conditions`.CREATE TABLE in sql/base/dev/world_database.sql).
-- ============================================================================
--
-- ---- THE CANONICAL PhaseId MAP (owned by 20_creature_spawns.sql; base 15900) ----
--   15901 Phase 1 arrival/Hammerfall warzone   15902 Phase 2 Go'shek farm
--   15903 Phase 3 Stromgarde siege             15904 Phase 4 siege climax
--   15905 Phase 5 peace (TERMINAL)
-- 15901 is the DEFAULT phase on zone entry, gated only by the exclusion below (it is
-- what a player sees before any of 15902-15905's positive gate is met, per the brief's
-- own PhaseId 15901 note: "Gates on quest 90882 active/not-90896-complete"). 15902-15905
-- each need the quest-state gate the brief specifies; verified against
-- src/server/game/Conditions/ConditionMgr.h in the catchup-experience worktree:
--   CONDITION_SOURCE_TYPE_PHASE          = 26  (SourceGroup=PhaseId, SourceEntry=AreaId;
--                                                SourceEntry=0 -> "every area mapped to
--                                                this PhaseId in phase_area", see
--                                                ConditionMgr::addToPhases)
--   CONDITION_QUESTREWARDED (value 1)    =  8  (ConditionValue1=quest_id; true once the
--                                                quest has been turned in/rewarded)
--   CONDITION_QUESTTAKEN    (value 1)    =  9  (ConditionValue1=quest_id; true while the
--                                                quest is active/in the quest log)
--
-- ---- FIX ROUND 1 correctness fix -- terminal peace phase exclusivity ----
-- 21_phase_area.sql maps the shared AreaIds (7658 Hammerfall, 7680 farm, 7667
-- Stromgarde) to BOTH an active warzone phase (15901/15902/15903) AND the peace phase
-- 15905. CONDITION_QUESTREWARDED is a one-way latch (stays true forever once earned),
-- so 90883-rewarded and 90888-rewarded never become false again -- a player who has
-- finished the whole chain (90896 rewarded) would satisfy the 15902/15903 positive
-- gates AND the 15905 gate simultaneously at the same AreaId, leaving warzone trash
-- (gnolls/farm/siege mobs) visible alongside the peace wildlife. Multiple `conditions`
-- rows sharing the same (SourceGroup, ElseGroup) are ANDed (ConditionMgr groups by
-- ElseGroup; ElseGroup=0 here for every row => AND), so adding a second, NEGATED
-- CONDITION_QUESTREWARDED(90896) row to 15901/15902/15903 makes each of them apply
-- ONLY while 90896 is NOT yet rewarded, alongside their existing positive gate. This
-- makes them mutually exclusive with 15905 at every shared AreaId. 15904 (90896 taken)
-- and 15905 (90896 rewarded) already exclude each other correctly (a quest cannot be
-- both "taken, not yet rewarded" and "rewarded" at once) -- left unchanged.
--
-- Truth table (per shared AreaId 7658/7680/7667; T=true/F=false; "--" = row not
-- evaluated / condition set not fully met -> phase inactive):
--   90883 rwd | 90888 rwd | 90896 taken | 90896 rwd || 15901 | 15902 | 15903 | 15904 | 15905
--   ----------|-----------|-------------|-----------||-------|-------|-------|-------|-------
--       F     |     F     |      F      |     F     ||   X   |       |       |       |
--       T     |     F     |      F      |     F     ||       |   X   |       |       |
--       T     |     T     |      F      |     F     ||       |       |   X   |       |
--       T     |     T     |      T      |     F     ||       |       |   X   |   X   |
--       T     |     T     |      F      |     T     ||       |       |       |       |   X
--   (90896 rewarded implies 90896 was taken first, then turned in -- CONDITION_
--   QUESTTAKEN(9) is false again once rewarded, so 15904's gate closes on its own;
--   no negation needed there.) Only 15905 is ever active once 90896 is rewarded --
--   the warzone phases 15901/15902/15903 are all forced off by their new negated leg.
-- ============================================================================

-- PhaseId 15901 (arrival/Hammerfall warzone) -- FIX ROUND 1: exclusivity leg only.
-- 15901 is otherwise the default phase (no positive gate); this negated row is what
-- turns it off once 90896 is rewarded, so it does not linger alongside 15905 at the
-- shared Hammerfall AreaId (7658). Matches the brief's own PhaseId 15901 definition:
-- "Gates on quest 90882 active/not-90896-complete."
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15901, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15901 (arrival) requires quest 90896 NOT rewarded (peace-phase exclusivity, Fix Round 1)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 15902 (Go'shek farm) requires quest 90883 REWARDED, per brief Requirement 4
-- and Plan 1.6 ("on 90883 complete -> Go'shek farm spawns...").
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15902, 0, 0, 0, 8, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15902 (Go''shek farm) requires quest 90883 rewarded')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 15902 -- FIX ROUND 1: exclusivity leg. ANDed with the row above (same
-- SourceGroup/ElseGroup=0) so 15902 requires 90883 rewarded AND 90896 NOT rewarded --
-- otherwise it would stay active forever after 90883 fires, overlapping 15905 at the
-- shared Go'shek-farm AreaId (7680).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15902, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15902 (Go''shek farm) requires quest 90896 NOT rewarded (peace-phase exclusivity, Fix Round 1)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 15903 (Stromgarde siege) requires quest 90888 REWARDED, per brief Requirement 4
-- and Plan 1.6 ("on 90888 -> Stromgarde battlefield spawns...").
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15903, 0, 0, 0, 8, 0, 90888, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15903 (Stromgarde siege) requires quest 90888 rewarded')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 15903 -- FIX ROUND 1: exclusivity leg. ANDed with the row above so 15903
-- requires 90888 rewarded AND 90896 NOT rewarded -- otherwise it would stay active
-- forever after 90888 fires, overlapping 15905 at the shared Stromgarde-Keep AreaId
-- (7667). (15904's own gate, 90896 TAKEN, already self-excludes from 15905 once 90896
-- is rewarded, since a rewarded quest is no longer "taken" -- no negation needed there.)
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15903, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15903 (Stromgarde siege) requires quest 90896 NOT rewarded (peace-phase exclusivity, Fix Round 1)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 15904 (siege climax) requires quest 90896 TAKEN, per brief Requirement 4
-- and Plan 1.6 ("90895/90893 progress -> Ogre Basher waves; Ro'grok active (90896)").
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15904, 0, 0, 0, 9, 0, 90896, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15904 (siege climax) requires quest 90896 taken')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 15905 (peace, TERMINAL) requires quest 90896 REWARDED, per brief Requirement 4
-- and Plan 1.6 ("on 90896 complete -> ... the warzone-to-calm transformation").
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 15905, 0, 0, 0, 8, 0, 90896, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- PhaseId 15905 (peace, terminal) requires quest 90896 rewarded')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- ============================================================================
-- END -- 7 condition rows (15901 x1, 15902 x2, 15903 x2, 15904 x1, 15905 x1; Fix Round 1
-- added the 3 negated 90896-rewarded exclusivity rows on 15901/15902/15903). All 5
-- PhaseIds are covered between this file (the gates) and 21_phase_area.sql (the area
-- membership); 15901/15902/15903 are now mutually exclusive with the terminal peace
-- phase 15905 at every shared AreaId (see truth table above).
-- ============================================================================
