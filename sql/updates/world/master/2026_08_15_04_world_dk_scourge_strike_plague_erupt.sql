-- Scourge Strike (55090) EFFECT_1 is a DUMMY (EffectBasePointsF 35) for "causing your plagues on the
-- target to erupt, dealing their damage an additional time at $s2% effectiveness". Retail erupt
-- damage spells are Virulent Plague (Erupt) 1241167 and Dread Plague (Erupt) 1241171; neither has
-- its own damage coefficients, so the script supplies BASE_POINT0 from the matching DoT.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dk_scourge_strike_plague_erupt';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(55090, 'spell_dk_scourge_strike_plague_erupt'); -- Scourge Strike
