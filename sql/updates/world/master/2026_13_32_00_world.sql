-- Dragonriding / Skyriding spell scripts

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
    'spell_dragonriding_surge_forward',
    'spell_dragonriding_skyward_ascent',
    'spell_dragonriding_whirling_surge',
    'spell_dragonriding_launch_boost'
);

INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(372608,'spell_dragonriding_surge_forward'),
(372610,'spell_dragonriding_skyward_ascent'),
(361584,'spell_dragonriding_whirling_surge'),
(392752,'spell_dragonriding_launch_boost');
