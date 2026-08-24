-- Demon Hunter Vengeance invent implement (EVR-47 slice #3): 16 talents + Apex Untethered Rage.
-- Build evidence: 12.0.7.67808 EvryDb2Export temp/db2/.../vengeance-slice3/.
-- Core-only / deferred: Ascending Flame 428603 E1/E2 aura108; Untethered Rage 1270448 aura107×2;
-- Ruinous Bulwark E0 aura108; Volatile Flameblood E3 aura108; Vengeful Beast E0 aura107;
-- Soulcrush E0/E2–E4 aura107. Soft: Untethered chance curve; Fallout 50% (no ProcChance/BP).

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_dh_spirit_bomb',
  'spell_dh_spirit_bomb_damage',
  'spell_dh_soul_cleave_vengeance',
  'spell_dh_soul_cleave_damage_vengeance',
  'spell_dh_fracture_vengeful_beast',
  'spell_dh_frailty',
  'spell_dh_sigil_of_flame_frailty',
  'spell_dh_ruinous_bulwark',
  'spell_dh_revel_in_pain',
  'spell_dh_charred_flesh',
  'spell_dh_volatile_flameblood',
  'spell_dh_felfire_fist_infernal_strike',
  'spell_dh_immolation_aura_initial_burst',
  'spell_dh_fallout'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(247454, 'spell_dh_spirit_bomb'),
(247455, 'spell_dh_spirit_bomb_damage'),
(228477, 'spell_dh_soul_cleave_vengeance'),
(228478, 'spell_dh_soul_cleave_damage_vengeance'),
(225919, 'spell_dh_fracture_vengeful_beast'),
(225921, 'spell_dh_fracture_vengeful_beast'),
(247456, 'spell_dh_frailty'),
(204598, 'spell_dh_sigil_of_flame_frailty'),
(212106, 'spell_dh_ruinous_bulwark'),
(343014, 'spell_dh_revel_in_pain'),
(336639, 'spell_dh_charred_flesh'),
(390808, 'spell_dh_volatile_flameblood'),
(189110, 'spell_dh_felfire_fist_infernal_strike'),
(258920, 'spell_dh_immolation_aura_initial_burst'),
(258921, 'spell_dh_fallout');
