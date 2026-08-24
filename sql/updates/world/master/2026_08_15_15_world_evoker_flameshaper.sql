-- Evoker Flameshaper hero tree: 10 needs-script nodes (Twin Flame first; Consume Flame last).
-- Build evidence: 12.0.7.67808 temp/db2/hero-flameshaper/

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_twin_flame',
  'spell_evo_titanic_precision',
  'spell_evo_hover_trailblazer',
  'spell_evo_trailblazer_flight',
  'spell_evo_shape_of_flame',
  'spell_evo_enkindle',
  'spell_evo_lifecinders',
  'spell_evo_draconic_instincts',
  'spell_evo_deep_exhalation_fire_breath',
  'spell_evo_consume_flame_disintegrate',
  'spell_evo_consume_flame_pyre',
  'spell_evo_consume_flame_verdant_embrace',
  'spell_evo_consume_flame_emerald_blossom'
);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1265979, 'spell_evo_twin_flame'),
(361469, 'spell_evo_titanic_precision'),
(362969, 'spell_evo_titanic_precision'),
(358267, 'spell_evo_hover_trailblazer'),
(357210, 'spell_evo_trailblazer_flight'),
(359816, 'spell_evo_trailblazer_flight'),
(368970, 'spell_evo_shape_of_flame'),
(357214, 'spell_evo_shape_of_flame'),
(444016, 'spell_evo_enkindle'),
(363916, 'spell_evo_lifecinders'),
(445958, 'spell_evo_draconic_instincts'),
(357209, 'spell_evo_deep_exhalation_fire_breath'),
(356995, 'spell_evo_consume_flame_disintegrate'),
(357212, 'spell_evo_consume_flame_pyre'),
(361195, 'spell_evo_consume_flame_verdant_embrace'),
(355916, 'spell_evo_consume_flame_emerald_blossom');

-- Twin Flame: fire when a cast consumed Essence Burst (script checks m_appliedMods).
-- ProcFlags: DEAL_HARMFUL_ABILITY|DEAL_HARMFUL_SPELL|DEAL_HELPFUL_ABILITY|DEAL_HELPFUL_SPELL; Phase: CAST.
DELETE FROM `spell_proc` WHERE `SpellId` = 1265979;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1265979, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00015400, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0, 100, 0, 0);

-- Enkindle: proc from Essence-cost damage/healing (script filters POWER_ESSENCE).
-- ProcFlags: DEAL_HARMFUL_SPELL|DEAL_HARMFUL_PERIODIC|DEAL_HELPFUL_SPELL|DEAL_HELPFUL_PERIODIC; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 444016;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(444016, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00254000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0);

-- Draconic Instincts: taken damage; script scales chance with hit size.
-- ProcFlags: TAKE_HARMFUL_DAMAGE|TAKE_HARMFUL_SPELL|TAKE_HARMFUL_PERIODIC; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 445958;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(445958, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00100000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0);
