-- Evoker Chronowarden hero tree: 12 needs-script nodes (Chrono Flame keystone + dependents).
-- Build evidence: 12.0.7.67808 temp/db2/hero/chronowarden/

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_chrono_flame',
  'spell_evo_chrono_flames',
  'spell_evo_chronal_dynamo_living_flame',
  'spell_evo_tip_the_scales_temporal_burst',
  'spell_evo_temporal_burst',
  'spell_evo_reverberations_hot',
  'spell_evo_reverberations_dot',
  'spell_evo_hover_chronowarden',
  'spell_evo_temporality_dr',
  'spell_evo_time_convergence',
  'spell_evo_double_time',
  'spell_evo_prescience_double_time'
);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(431442, 'spell_evo_chrono_flame'),
(431443, 'spell_evo_chrono_flames'),
(361500, 'spell_evo_chronal_dynamo_living_flame'),
(361509, 'spell_evo_chronal_dynamo_living_flame'),
(370553, 'spell_evo_tip_the_scales_temporal_burst'),
(431698, 'spell_evo_temporal_burst'),
(409895, 'spell_evo_reverberations_hot'),
(431620, 'spell_evo_reverberations_dot'),
(358267, 'spell_evo_hover_chronowarden'),
(431872, 'spell_evo_temporality_dr'),
(431984, 'spell_evo_time_convergence'),
(431874, 'spell_evo_double_time'),
(410089, 'spell_evo_prescience_double_time');

-- Chrono Flames replaces Living Flame on the bar; keep Essence Burst chance on the override cast.
DELETE FROM `spell_script_names` WHERE `spell_id` = 431443 AND `ScriptName` = 'spell_evo_ruby_essence_burst';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(431443, 'spell_evo_ruby_essence_burst');

-- Double-time (Pres): proc on Fire Breath / Dream Breath periodic criticals.
-- ProcFlags: DEAL_HARMFUL_PERIODIC|DEAL_HELPFUL_PERIODIC; HitMask: CRITICAL; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 431874;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(431874, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00240000, 0x0, 0x0, 0x2, 0x2, 0x0, 0x0, 0, 100, 0, 0);
