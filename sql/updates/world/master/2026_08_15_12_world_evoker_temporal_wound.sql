-- Evoker Temporal Wound: Breath of Eons damage-copy accumulate/release

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_evo_temporal_wound';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(409560, 'spell_evo_temporal_wound');
