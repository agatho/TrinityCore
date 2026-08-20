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
-- 15901 is the DEFAULT phase on zone entry (no condition needed -- it is what a player
-- sees before any of 15902-15905's gate is met). 15902-15905 each need exactly the
-- quest-state gate the brief specifies; verified against
-- src/server/game/Conditions/ConditionMgr.h in the catchup-experience worktree:
--   CONDITION_SOURCE_TYPE_PHASE          = 26  (SourceGroup=PhaseId, SourceEntry=AreaId;
--                                                SourceEntry=0 -> "every area mapped to
--                                                this PhaseId in phase_area", see
--                                                ConditionMgr::addToPhases)
--   CONDITION_QUESTREWARDED (value 1)    =  8  (ConditionValue1=quest_id; true once the
--                                                quest has been turned in/rewarded)
--   CONDITION_QUESTTAKEN    (value 1)    =  9  (ConditionValue1=quest_id; true while the
--                                                quest is active/in the quest log)
-- ============================================================================

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
-- END -- 4 condition rows (15902/15903/15904/15905). PhaseId 15901 is the implicit
-- default arrival phase and needs no gate. All 5 PhaseIds are covered between this
-- file (the gates) and 21_phase_area.sql (the area membership).
-- ============================================================================
