-- Festering Strike (85948) grants 2-3 charges of Lesser Ghoul (1254252) since 12.0.1; each Scourge
-- Strike (55090) or Vampiric Strike (433895) spends one to summon Lesser Ghoul (275430).
-- 1254252 marks both strikes with SPELL_AURA_SET_ACTION_BUTTON_SPELL_COUNT (EffectTriggerSpell
-- 55090 / 433895), which is only the button count badge - consumption is server-side.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dk_summon_lesser_ghoul';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(55090, 'spell_dk_summon_lesser_ghoul'),  -- Scourge Strike
(433895, 'spell_dk_summon_lesser_ghoul'); -- Vampiric Strike
