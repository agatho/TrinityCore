-- Evoker Augmentation foundation: Ebon Might host + Sands of Time extension hosts

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
    'spell_evo_ebon_might',
    'spell_evo_eruption',
    'spell_evo_upheaval',
    'spell_evo_breath_of_eons'
);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(395152, 'spell_evo_ebon_might'),
(395160, 'spell_evo_eruption'),
(396286, 'spell_evo_upheaval'),
(408092, 'spell_evo_upheaval'),
(403631, 'spell_evo_breath_of_eons');
