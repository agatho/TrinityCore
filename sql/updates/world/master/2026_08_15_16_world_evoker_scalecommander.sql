-- Evoker Scalecommander hero tree: 11 DUMMY nodes + Diverted Power (sans Bombardments strafes).
-- Build evidence: 12.0.7.67808 temp/db2/12.0.7.67808/ (+ SimC sc_evoker follow-ons 438588/438653/441248).

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
  'spell_evo_eternity_surge',
  'spell_evo_mass_disintegrate_disintegrate',
  'spell_evo_onslaught',
  'spell_evo_unrelenting_siege',
  'spell_evo_unrelenting_siege_buff',
  'spell_evo_menacing_presence',
  'spell_evo_menacing_presence_dr',
  'spell_evo_menacing_presence_knock',
  'spell_evo_slipstream_breath',
  'spell_evo_maneuverability',
  'spell_evo_command_squadron_breath',
  'spell_evo_melt_armor_breath_damage',
  'spell_evo_wingleader',
  'spell_evo_extended_battle',
  'spell_evo_diverted_power'
);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(359073, 'spell_evo_eternity_surge'),
(356995, 'spell_evo_mass_disintegrate_disintegrate'),
(441245, 'spell_evo_onslaught'),
(441246, 'spell_evo_unrelenting_siege'),
(441248, 'spell_evo_unrelenting_siege_buff'),
(441181, 'spell_evo_menacing_presence'),
(441201, 'spell_evo_menacing_presence_dr'),
(368970, 'spell_evo_menacing_presence_knock'),
(357214, 'spell_evo_menacing_presence_knock'),
(357210, 'spell_evo_slipstream_breath'),
(403631, 'spell_evo_slipstream_breath'),
(433874, 'spell_evo_slipstream_breath'),
(442204, 'spell_evo_slipstream_breath'),
(433871, 'spell_evo_maneuverability'),
(357210, 'spell_evo_command_squadron_breath'),
(403631, 'spell_evo_command_squadron_breath'),
(433874, 'spell_evo_command_squadron_breath'),
(442204, 'spell_evo_command_squadron_breath'),
(353759, 'spell_evo_melt_armor_breath_damage'),
(409560, 'spell_evo_melt_armor_breath_damage'),
(441206, 'spell_evo_wingleader'),
(441212, 'spell_evo_extended_battle'),
(441219, 'spell_evo_diverted_power');

-- Menacing Presence: knock / harmful spell hits (script also binds Tail Swipe / Wing Buffet).
-- ProcFlags: DEAL_HARMFUL_ABILITY|DEAL_HARMFUL_SPELL; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 441181;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(441181, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00014000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0);

-- Menacing Presence DR: when the debuffed unit damages the auracaster.
-- ProcFlags: DEAL_HARMFUL_DAMAGE|DEAL_HARMFUL_SPELL|DEAL_HARMFUL_PERIODIC; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 441201;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(441201, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000A8000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0);

-- Extended Battle: Essence-cost hits while Bombardments mark is present (script filters).
-- ProcFlags: DEAL_HARMFUL_SPELL|DEAL_HARMFUL_PERIODIC|DEAL_HELPFUL_SPELL; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 441212;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(441212, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00254000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0);

-- Wingleader / Diverted Power: Bombardments damage 434481 (no strafe NPC in this slice).
-- ProcFlags: DEAL_HARMFUL_SPELL|DEAL_HARMFUL_PERIODIC; Phase: HIT.
DELETE FROM `spell_proc` WHERE `SpellId` IN (441206, 441219);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(441206, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00024000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0),
(441219, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00024000, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 100, 0, 0);
