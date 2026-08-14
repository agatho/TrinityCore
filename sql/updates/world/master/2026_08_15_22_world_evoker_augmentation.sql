-- Evoker Augmentation talent dummies (EVR-37 slice #4 / EVR-43)
-- Flow 2R impossible-date; dual-check free NN=40 on master + evry (tip through _39).

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_augmentation_essence_burst_living_flame',
  'spell_evo_augmentation_essence_burst_azure_strike',
  'spell_evo_eruption_augmentation',
  'spell_evo_upheaval_augmentation',
  'spell_evo_breath_augmentation',
  'spell_evo_deep_breath_damage_augmentation',
  'spell_evo_prescience_anachronism',
  'spell_evo_fire_breath_inferno',
  'spell_evo_infernos_blessing',
  'spell_evo_infernos_blessing_damage',
  'spell_evo_living_flame_pupil',
  'spell_evo_azure_strike_echoing',
  'spell_evo_blistering_scales_augmentation',
  'spell_evo_aspects_favor_scales',
  'spell_evo_aspects_favor_hover',
  'spell_evo_bestow_weyrnstone',
  'spell_evo_emerald_blossom_dream_of_spring'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(361469, 'spell_evo_augmentation_essence_burst_living_flame'),
(362969, 'spell_evo_augmentation_essence_burst_azure_strike'),
(395160, 'spell_evo_eruption_augmentation'),
(396288, 'spell_evo_upheaval_augmentation'),
(403631, 'spell_evo_breath_augmentation'),
(357210, 'spell_evo_breath_augmentation'),
(433874, 'spell_evo_breath_augmentation'),
(353759, 'spell_evo_deep_breath_damage_augmentation'),
(409311, 'spell_evo_prescience_anachronism'),
(357208, 'spell_evo_fire_breath_inferno'),
(382266, 'spell_evo_fire_breath_inferno'),
(410263, 'spell_evo_infernos_blessing'),
(410265, 'spell_evo_infernos_blessing_damage'),
(361500, 'spell_evo_living_flame_pupil'),
(362969, 'spell_evo_azure_strike_echoing'),
(360827, 'spell_evo_blistering_scales_augmentation'),
(363916, 'spell_evo_aspects_favor_scales'),
(358267, 'spell_evo_aspects_favor_hover'),
(408233, 'spell_evo_bestow_weyrnstone'),
(355913, 'spell_evo_emerald_blossom_dream_of_spring');
