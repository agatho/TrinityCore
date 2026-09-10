-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- SPELL_STASIS_3 (366636): quest popup with Kodethi as giver (core EffectQuestStart uses player GUID and client drops DisplayPopup).
DELETE FROM `spell_script_names` WHERE `spell_id` = 366636 AND `ScriptName` = 'spell_dracthyr_stasis_3';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(366636, 'spell_dracthyr_stasis_3');
