-- Doomed Bidding (455386): on Sudden Doom (81340) spellmod consume, summon Lesser Ghoul (275430).
-- Forbidden Sacrifice (1256576): mastery amount comes from Forbidden Knowledge (1256565) EFFECT_0;
-- Putrefy applies both the mastery buff and Lesser Ghoul charges (script spell_dk_putrefy).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_dk_sudden_doom', 'spell_dk_forbidden_sacrifice');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(81340, 'spell_dk_sudden_doom'),           -- Sudden Doom (consume + Doomed Bidding)
(1256576, 'spell_dk_forbidden_sacrifice'); -- Forbidden Sacrifice (Mastery from Forbidden Knowledge)
