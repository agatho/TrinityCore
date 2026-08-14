-- Demon Hunter Aldrachi Reaver invent implement (EVR-47 slice #4): 11 hero-tree seeds.
-- Build evidence: 12.0.7.67808 EvryDb2Export temp/db2/12.0.7.67808/SpellEffect-pack-16/18-spells.csv.
-- Core-only / deferred: Keen Edge E1–E4 (aura108/344/218); Thrill haste 442688 aura193;
-- Reaver's Mark debuff 442624 aura271/343; Warblade buff 442503 aura218.
-- Soft: Wounded Quarry shatter 10%; Bladecraft mark-cap via live CumulativeAura; set-bonus 1236360 ignored.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_dh_art_of_the_glaive',
  'spell_dh_art_of_the_glaive_stacks',
  'spell_dh_soul_fragment_consume_aldrachi',
  'spell_dh_reavers_glaive',
  'spell_dh_aldrachi_glaive_flurry',
  'spell_dh_aldrachi_rending_strike',
  'spell_dh_aldrachi_hunt_spite',
  'spell_dh_aldrachi_broken_spirit_chance',
  'spell_dh_wounded_quarry',
  'spell_dh_warblade_hunger_felblade'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Talent / state auras
(442290, 'spell_dh_art_of_the_glaive'),
(444661, 'spell_dh_art_of_the_glaive_stacks'),
(442806, 'spell_dh_wounded_quarry'),
-- Reaver's Glaive (Havoc + Vengeance override targets)
(442294, 'spell_dh_reavers_glaive'),
(1283344, 'spell_dh_reavers_glaive'),
-- Soul fragment consume hosts (Art / Warblade / Incorruptible)
(208014, 'spell_dh_soul_fragment_consume_aldrachi'),
(210047, 'spell_dh_soul_fragment_consume_aldrachi'),
(210050, 'spell_dh_soul_fragment_consume_aldrachi'),
(228542, 'spell_dh_soul_fragment_consume_aldrachi'),
(228540, 'spell_dh_soul_fragment_consume_aldrachi'),
(228556, 'spell_dh_soul_fragment_consume_aldrachi'),
-- Enhancement cleave hosts (Glaive Flurry)
(188499, 'spell_dh_aldrachi_glaive_flurry'),
(210152, 'spell_dh_aldrachi_glaive_flurry'),
(228477, 'spell_dh_aldrachi_glaive_flurry'),
-- Enhancement strike hosts (Rending Strike + Warblade hit)
(162794, 'spell_dh_aldrachi_rending_strike'),
(201427, 'spell_dh_aldrachi_rending_strike'),
(225919, 'spell_dh_aldrachi_rending_strike'),
(225921, 'spell_dh_aldrachi_rending_strike'),
(203782, 'spell_dh_aldrachi_rending_strike'),
-- Hunt / Sigil of Spite ready + Broken Spirit shards
(370965, 'spell_dh_aldrachi_hunt_spite'),
(390163, 'spell_dh_aldrachi_hunt_spite'),
-- Broken Spirit chance on SC / BD / CS
(228477, 'spell_dh_aldrachi_broken_spirit_chance'),
(188499, 'spell_dh_aldrachi_broken_spirit_chance'),
(210152, 'spell_dh_aldrachi_broken_spirit_chance'),
(162794, 'spell_dh_aldrachi_broken_spirit_chance'),
(201427, 'spell_dh_aldrachi_broken_spirit_chance'),
-- Warblade Felblade vacuum (Havoc)
(232893, 'spell_dh_warblade_hunger_felblade');
