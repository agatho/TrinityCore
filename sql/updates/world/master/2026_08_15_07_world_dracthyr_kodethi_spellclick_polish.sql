-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- Dracthyr Forbidden Reach: disable Disintegrate spellclick after first awaken (objective 421735 credited)
-- Complements 2026_08_15_05_world_dracthyr_kodethi_spellclick.sql (quest-in-log gate only)

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=18 AND `SourceGroup` IN (187223, 181494) AND `SourceEntry`=362355 AND `SourceId`=0 AND `ConditionTypeOrReference`=48;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(18, 187223, 362355, 0, 0, 48, 0, 421735, 0, 0, 0, 0, 0, '', 'Spellclick Kodethi: Disintegrate only while quest 64864 objective 421735 incomplete'),
(18, 181494, 362355, 0, 1, 48, 0, 421735, 0, 0, 0, 0, 0, '', 'Spellclick Dervishian: Disintegrate only while quest 64864 objective 421735 incomplete');
