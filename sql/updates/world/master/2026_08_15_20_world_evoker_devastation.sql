-- Evoker Devastation implement (EVR-37 slice #2 / EVR-41): 13 dummy talent nodes.
-- Build evidence: 12.0.7.67808 EvryDb2Export SpellEffect-pack-13 + follow-ons / Spell-pack-13.
-- Hosts extended (not rewritten): Fire Breath, Pyre, Disintegrate, Eternity Surge, Azure Strike,
-- Living Flame, Deep Breath, Dragonrage. Volatility bounces cast IsTriggered() Pyre (FtF-safe).
-- Power Swell 376850 / Imminent Destruction 411055 / Azure Sweep 1265871 OVERRIDE_ACTIONBAR are core.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_catalyze_fire_breath',
  'spell_evo_pyre_damage',
  'spell_evo_eternity_surge_damage',
  'spell_evo_shattering_star',
  'spell_evo_disintegrate_devastation',
  'spell_evo_disintegrate_titanic_wrath',
  'spell_evo_imminent_destruction_breath',
  'spell_evo_living_flame_devastation',
  'spell_evo_deep_breath_damage_giantkiller',
  'spell_evo_azure_sweep',
  'spell_evo_dragonrage_animosity'
);

DELETE FROM `spell_script_names` WHERE `spell_id` IN (382411) AND `ScriptName` = 'spell_evo_eternity_surge';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(357209, 'spell_evo_catalyze_fire_breath'),
(357212, 'spell_evo_pyre_damage'),
(359077, 'spell_evo_eternity_surge_damage'),
(1265804, 'spell_evo_shattering_star'),
(356995, 'spell_evo_disintegrate_devastation'),
(356995, 'spell_evo_disintegrate_titanic_wrath'),
(357210, 'spell_evo_imminent_destruction_breath'),
(433874, 'spell_evo_imminent_destruction_breath'),
(361500, 'spell_evo_living_flame_devastation'),
(353759, 'spell_evo_deep_breath_damage_giantkiller'),
(1265872, 'spell_evo_azure_sweep'),
(375087, 'spell_evo_dragonrage_animosity'),
(382411, 'spell_evo_eternity_surge');
