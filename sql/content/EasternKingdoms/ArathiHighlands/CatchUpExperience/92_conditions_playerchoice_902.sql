-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up / RPE :: PlayerChoice 902 level-bracket gating (Phase K #3)
-- ============================================================================
-- Closes the "TODO Phase K: level-bracket gating" left in 91_playerchoice_902.sql. Retail shows
-- only the age-appropriate destination(s) in the finale "Where Do You Want To Go?" picker: our
-- own Horde screenshot at ~level 20 offered ONLY Dragonflight, confirming the low end; the 70/80
-- boundaries are the datamined ranges (Warcraft Wiki), encoded exactly here.
--
--   ResponseId 9021  Dragonflight          level 10-69
--   ResponseId 9022  The War Within Recap  level 70-80
--   ResponseId 9023  The War Within        level >= 80 (max level)
--
-- Mechanism (verified against this fork's core, src/server/game/Conditions/ConditionMgr.h):
--   SourceType 36 = CONDITION_SOURCE_TYPE_PLAYER_CHOICE_RESPONSE (line 192; NOT 26=PHASE).
--     Lookup key for type 36 is {SourceGroup = ChoiceId, SourceEntry = ResponseId, SourceId = 0}
--     (ConditionMgr.cpp) -> SourceGroup=902, SourceEntry=9021/9022/9023, SourceId=0.
--   CondType 27 = CONDITION_LEVEL (line 89; NOT 9=QUESTTAKEN). Eval: CompareValues(op=Value2,
--     unitLevel, Value1) -> Value1 = level threshold, Value2 = comparison op.
--   ComparisonType (common/Utilities/Util.h): EQ=0, HIGH(>)=1, LOW(<)=2, HIGH_EQ(>=)=3, LOW_EQ(<=)=4.
--   A closed range = two CONDITION_LEVEL rows on the same response in the SAME ElseGroup (rows in
--   one ElseGroup are AND-ed): one >=min (op 3) and one <=max (op 4).
-- Column set is this fork's 16-column `conditions` layout (incl. ConditionStringValue1), matching
--   22_conditions_phasing.sql.
-- CANDIDATE ONLY -- idempotent (DELETE-scoped by SourceType+SourceGroup, then INSERT ... ODKU).
-- Apply AFTER 91_playerchoice_902.sql (the responses must exist first).
-- ============================================================================
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=36 AND `SourceGroup`=902;

INSERT INTO `conditions`
 (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,
  `ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,
  `ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
VALUES
 -- 9021 Dragonflight: level 10..69
 (36, 902, 9021, 0, 0, 27, 0, 10, 3, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9021 Dragonflight: level >= 10'),
 (36, 902, 9021, 0, 0, 27, 0, 69, 4, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9021 Dragonflight: level <= 69'),
 -- 9022 The War Within Recap: level 70..80
 (36, 902, 9022, 0, 0, 27, 0, 70, 3, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9022 TWW Recap: level >= 70'),
 (36, 902, 9022, 0, 0, 27, 0, 80, 4, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9022 TWW Recap: level <= 80'),
 -- 9023 The War Within: level >= 80 (max level)
 (36, 902, 9023, 0, 0, 27, 0, 80, 3, 0, '', 0, 0, 0, '', 'PlayerChoice 902 resp 9023 The War Within: level >= 80')
ON DUPLICATE KEY UPDATE `Comment`=VALUES(`Comment`);
