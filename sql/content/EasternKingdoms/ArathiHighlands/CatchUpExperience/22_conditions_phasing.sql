-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase D phasing conditions (FIX ROUND 2)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED -- was wrongly 2796; client uiMapID 2451 is display-only).
-- Depends on: 20_creature_spawns.sql (uses these REAL PhaseIds), 21_phase_area.sql
--   (AreaId->PhaseId map these conditions attach to via ConditionMgr::addToPhases --
--   SourceEntry=0 applies a condition to every AreaId already mapped to that PhaseId in
--   `phase_area`, so one row per PhaseId is enough), phase_shift.sql (the wire-derived
--   phase->quest graph, VerifiedBuild=69382).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is the full
--   SourceTypeOrReferenceId/SourceGroup/SourceEntry/SourceId/ElseGroup/
--   ConditionTypeOrReference/ConditionTarget/ConditionValue1/2/3/ConditionStringValue1
--   tuple, per `conditions`.CREATE TABLE in sql/base/dev/world_database.sql).
-- ============================================================================
--
-- **HORDE CROSS-VALIDATION CAVEAT (2026-08-20):** a full Horde capture (build 69404) was
-- run through the --phaseshift decoder. Its personal phase ids ({1,13,65,1951}) are almost
-- ENTIRELY DISJOINT from these Alliance ids ({1959,1961,1610,28,37,...}); even the coincidental
-- overlaps (1965, 4) carry DIFFERENT quest windows. => the numeric personal-phase ids are
-- SESSION/INSTANCE-ALLOCATED, NOT portable content constants. ONLY PhaseId 3 (slot-25 completion,
-- whole-span) cross-validates as canonical on BOTH factions. The real spine is the QUEST-STATE
-- CONDITIONS in 22_conditions_phasing.sql + server-assigned personal phases (instance/C++ per
-- PhasingHandler) -- these numeric PhaseIds are ILLUSTRATIVE of the phase-graph SHAPE only. Do NOT
-- port them to a Horde build or treat them as stable; Phase K: derive real ids per-realm at deploy.
-- ---- THE REAL PhaseId MAP (Fix Round 2 -- replaces the FABRICATED 15901-15905 block) ----
-- See 20_creature_spawns.sql / 21_phase_area.sql banners for the full graph. This file only
-- gates the 7 PhaseIds actually attached to a creature spawn (1961, 37, 1959, 4, 1610, 28,
-- 3). PhaseIds 8 and 1965 are documented in 21_phase_area.sql for completeness but are NOT
-- gated here -- they carry no creature spawn in this feature, so there is nothing for a
-- condition to turn on or off; a future task that spawns something against them should add
-- their conditions at that time (they'd mirror 4's and 1959's rows below respectively,
-- since the graph shows near-identical quest windows).
--
-- Enum values re-verified against I:/TrinityCore/mythic-plus/TrinityCore/src/server/game/
-- Conditions/ConditionMgr.h for this fix (unchanged from Fix Round 1):
--   CONDITION_SOURCE_TYPE_PHASE          = 26  (SourceGroup=PhaseId, SourceEntry=AreaId;
--                                                SourceEntry=0 -> "every area mapped to
--                                                this PhaseId in phase_area", see
--                                                ConditionMgr::addToPhases)
--   CONDITION_QUESTREWARDED (value 1)    =  8  (ConditionValue1=quest_id; true once the
--                                                quest has been turned in/rewarded)
--   CONDITION_QUESTTAKEN    (value 1)    =  9  (ConditionValue1=quest_id; true while the
--                                                quest is active/in the quest log)
--
-- ---- HONEST BOUNDARY on the quest-state gate chosen per PhaseId ----
-- phase_shift.sql's graph gives each PhaseId a SET of quests it was observed active during,
-- not a single canonical "on" trigger and "off" trigger -- that mapping to
-- QUESTTAKEN/QUESTREWARDED conditions is INFERRED here, same honesty caveat as
-- 20_creature_spawns.sql's per-spawn PhaseId assignment:
--   PhaseId 1961/37 (quests=[90883]) -> gated on QUESTTAKEN(90883). No wire evidence exists
--     for what precedes 90883 (the graph has no data for quest 90882, which the brief's
--     Requirement 2 cites as also relevant to this cluster) -- TODO Phase K: confirm whether
--     the arrival cluster should ALSO be visible while only 90882 is active/not yet 90883.
--   PhaseId 1959 (quests=[90885,86,87,88,93,95,96], the whole farm-through-climax span) ->
--     gated on QUESTREWARDED(90883) alone (the "entry" event into that whole span) rather
--     than reproducing the full 7-quest OR-chain -- matches Fix Round 1's positive-gate
--     design intent for this cluster, now on a real PhaseId.
--   PhaseId 4 (quests=[90885,86,87], farm-only) -> gated on QUESTREWARDED(90883) AND NOT
--     QUESTREWARDED(90888) -- the farm-trash window opens once 90883 is turned in and closes
--     once the siege quest 90888 is turned in (i.e. progression has moved past the farm).
--   PhaseId 1610/28 (quests=[90893,95], siege-only) -> gated on QUESTREWARDED(90888) AND NOT
--     QUESTREWARDED(90896) -- the siege window opens once 90888 is turned in and closes once
--     the climax quest 90896 is turned in.
--   PhaseId 3 (quests=[90883,85,86,87,88,93,95,96], the widest phase on the wire, also
--     flagged "completion phase" at slot 25) -> this file gates it on QUESTTAKEN(90896)
--     ONLY, matching how 20_creature_spawns.sql actually uses it (the narrow climax/Ro'grok
--     cluster), NOT the full multi-quest window the wire shows for this id in other
--     contexts. -- TODO Phase K: if a future task spawns something else against PhaseId 3
--     for an earlier quest step, this gate needs to become an OR across the full quest set.
--
-- ---- Fix Round 1's "peace-phase exclusivity" negated-condition system is DROPPED ----
-- The old (WRONG) file added a negated CONDITION_QUESTREWARDED(90896) leg to phases
-- 15901/15902/15903 so they would not linger alongside its invented terminal "peace" phase
-- 15905 once the whole quest chain was rewarded. That system doesn't carry forward as-is:
-- (a) PhaseId 3 (used for the climax cluster) is gated on QUESTTAKEN(90896), which already
--     self-closes once 90896 is REWARDED (a rewarded quest is no longer "taken") -- no
--     negation needed, same self-closing behavior the old file relied on for its 15904.
-- (b) PhaseId 1610/28 (siege) are explicitly gated NOT QUESTREWARDED(90896) above, so they
--     already stop once the climax quest is turned in -- this IS the old exclusivity idea,
--     just expressed as the phase's own positive+negative gate pair instead of a separate
--     bolt-on row.
-- (c) There is no real "peace" PhaseId on the wire and no in-scope creature spawn that would
--     use one (see 21_phase_area.sql's banner) -- so there is nothing left for 1961/37/1959/
--     4 to be made exclusive AGAINST. 1961/37 (arrival) and 1959/4 (farm) are left with only
--     their own positive gates; once the player has moved on to siege/climax, Hammerfall's
--     and the farm's spawns remain phased-in (same behavior a real personal-phasing "leave
--     the old phase behind as you progress" design would need a NOT-rewarded-quest leg for
--     too, but there is no wire evidence to say which quest that should be keyed to for
--     1961/37/1959/4 specifically) -- TODO Phase K: revisit if a re-capture shows these
--     phases actually drop out of the player's phase set once the player has progressed.
-- ============================================================================

-- PhaseId 1961 (Hammerfall/town base) requires quest 90883 TAKEN (active window per the
-- wire graph). INFERRED gate -- see banner.
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1961, 0, 0, 0, 9, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1961 (Hammerfall/town base) requires quest 90883 taken (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 37 (Hammerfall gnoll filler, per-quest) requires quest 90883 TAKEN -- same window
-- as 1961 above (both real PhaseIds correlate to quest 90883 in the graph).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 37, 0, 0, 0, 9, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 37 (Hammerfall gnoll filler) requires quest 90883 taken (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 1959 (Go'shek farm base/leads/prop) requires quest 90883 REWARDED -- the "entry
-- event" into the wire graph's whole 90885-90896 span for this id.
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1959, 0, 0, 0, 8, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1959 (Go''shek farm base) requires quest 90883 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- PhaseId 4 (Go'shek farm trash + Runk, per-quest) requires quest 90883 REWARDED AND quest
-- 90888 NOT YET rewarded (farm-only window per the wire graph: 90885/86/87).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 4, 0, 0, 0, 8, 0, 90883, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 4 (Go''shek farm trash+Runk) requires quest 90883 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 4, 0, 0, 0, 8, 0, 90888, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 4 (Go''shek farm trash+Runk) requires quest 90888 NOT rewarded (Fix Round 2, closes the farm window once siege begins)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 1610 (Stromgarde Keep base/leads/town) requires quest 90888 REWARDED AND quest
-- 90896 NOT YET rewarded (siege-only window per the wire graph: 90893/95).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1610, 0, 0, 0, 8, 0, 90888, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1610 (Stromgarde Keep base) requires quest 90888 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 1610, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 1610 (Stromgarde Keep base) requires quest 90896 NOT rewarded (Fix Round 2, closes the siege window once climax is turned in)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 28 (Stromgarde siege trash + catapult, per-quest) -- same gate as 1610 above (both
-- real PhaseIds correlate to the identical quest window 90893/95 in the graph).
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 28, 0, 0, 0, 8, 0, 90888, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 28 (Stromgarde siege trash+catapult) requires quest 90888 rewarded (Fix Round 2, wire-graph quest window)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 28, 0, 0, 0, 8, 0, 90896, 0, 0, '', 1, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 28 (Stromgarde siege trash+catapult) requires quest 90896 NOT rewarded (Fix Round 2, closes the siege window once climax is turned in)')
ON DUPLICATE KEY UPDATE `NegativeCondition`=VALUES(`NegativeCondition`), `Comment`=VALUES(`Comment`);

-- PhaseId 3 (climax/Ro'grok cluster, as actually used in 20_creature_spawns.sql) requires
-- quest 90896 TAKEN. NOTE: the wire graph's own window for id 3 is far broader
-- (90883-90896) -- this gate reflects only THIS file's narrow climax use, see banner.
INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 (26, 3, 0, 0, 0, 9, 0, 90896, 0, 0, '', 0, 0, 0, '',
  'Catch-Up Experience -- REAL PhaseId 3 (climax/Ro''grok cluster use) requires quest 90896 taken (Fix Round 2 -- narrower than this id''s full wire-graph window, see banner)')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);

-- ============================================================================
-- END -- 9 condition rows across 7 REAL PhaseIds (1961 x1, 37 x1, 1959 x1, 4 x2, 1610 x2,
-- 28 x2, 3 x1). PhaseIds 8 and 1965 (documented in 21_phase_area.sql for completeness) are
-- intentionally NOT gated here -- see this file's banner. The fabricated 15901-15905
-- exclusivity system from Fix Round 1 is fully retired (see banner for how its intent is
-- now covered, and what is honestly left un-covered).
-- ============================================================================
