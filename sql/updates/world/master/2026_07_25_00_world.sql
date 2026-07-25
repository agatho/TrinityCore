--
DELETE FROM `spell_script_names` WHERE `spell_id`=374763 AND `ScriptName`='spell_dragonriding_lift_off';
DELETE FROM `spell_script_names` WHERE `spell_id`=436854 AND `ScriptName`='spell_dragonriding_switch_flight_style';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(374763,'spell_dragonriding_lift_off'),
(436854,'spell_dragonriding_switch_flight_style');
