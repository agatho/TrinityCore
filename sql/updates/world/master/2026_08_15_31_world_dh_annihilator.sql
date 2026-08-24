-- Demon Hunter Annihilator invent implement (EVR-47 slice #7): 10 seeds.
-- Build evidence: 12.0.7.67808 EvryDb2Export temp/db2/12.0.7.67808/SpellEffect-pack-22/11.csv
-- + SpellAuraOptions-annihilator.csv (1256301 Cumul=3, 1256302 Cumul=3, 1256322 Dur=8s,
-- 1265768 Dur=5s, 1256308 Dark Matter ready).
-- Core-only (no binds): Harness the Cosmos 1279247 aura108; Path to Oblivion 1253399 /
-- Phase Shift 1256245 / Swift Erasure 1253668 aura219→label 5652 on Voidfall stacks.
-- Meteoric Rise E0 aura108 FD/VR +% core. Soft: Catastrophe DoT BP from meteor×%; Dark Matter
-- shower waves (AT 1256309 + initial 1264129/1264130); Doomsayer 1213636 window scale.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_dh_voidfall',
  'spell_dh_voidfall_generator',
  'spell_dh_voidfall_spender',
  'spell_dh_spirit_bomb_annihilator',
  'spell_dh_collapsing_star_annihilator',
  'spell_dh_annihilator_metamorphosis',
  'spell_dh_doomsayer_cast',
  'spell_dh_meteoric_rise_fel_devastation',
  'spell_dh_meteoric_rise_void_ray',
  'spell_dh_voidfall_meteor_damage',
  'spell_dh_otherworldly_focus_damage'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Keystone / stacks
(1253304, 'spell_dh_voidfall'),
(263642, 'spell_dh_voidfall_generator'),
(473662, 'spell_dh_voidfall_generator'),
(228477, 'spell_dh_voidfall_spender'),
(1226019, 'spell_dh_voidfall_spender'),
-- Spirit Bomb / Collapsing Star
(247454, 'spell_dh_spirit_bomb_annihilator'),
(1221150, 'spell_dh_collapsing_star_annihilator'),
(247455, 'spell_dh_otherworldly_focus_damage'),
(1221162, 'spell_dh_otherworldly_focus_damage'),
-- Meta / Doomsayer / Meteoric Rise
(187827, 'spell_dh_annihilator_metamorphosis'),
(1217607, 'spell_dh_annihilator_metamorphosis'),
(189110, 'spell_dh_doomsayer_cast'),
(473728, 'spell_dh_doomsayer_cast'),
(212084, 'spell_dh_meteoric_rise_fel_devastation'),
(473728, 'spell_dh_meteoric_rise_void_ray'),
-- Meteor damage (normal + World Killer enlarged)
(1256305, 'spell_dh_voidfall_meteor_damage'),
(1256306, 'spell_dh_voidfall_meteor_damage'),
(1256617, 'spell_dh_voidfall_meteor_damage'),
(1256619, 'spell_dh_voidfall_meteor_damage');
