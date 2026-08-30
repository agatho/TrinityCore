-- Evoker Scalecommander Bombardments: mark 434473 proc → damage 434481 (Mass apply sites reuse existing scripts).
-- Build evidence: 12.0.7.67808 temp/db2/12.0.7.67808/ (mark ProcChance 30, ProcTypeMask 664232; damage coeff 4.75 / 8yd).
-- No creature/AT SQL — air-support TempSummon unproven; combat path is scripted damage.

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_evo_bombardments_mark';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(434473, 'spell_evo_bombardments_mark');

-- Bombardments mark: damage taken on marked target (TAKE melee/ranged/spell/periodic).
-- ProcFlags 0x000A2228 = 664232 (matches SpellAuraOptions); Chance 30; Phase HIT.
DELETE FROM `spell_proc` WHERE `SpellId` = 434473;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(434473, 0x00, 0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0, 30, 0, 0);
