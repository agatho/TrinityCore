-- Apocalypse (275699): "dealing $s1 Shadow damage and summoning $s2 lesser Ghouls for $275430d".
-- EffectIndex1 is a DUMMY with EffectBasePointsF 2 and no implicit target, so the ghoul count is
-- server-side; the summon itself is Lesser Ghoul 275430 (creature 237409), same spell Festering
-- Strike charges spend.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dk_apocalypse';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(275699, 'spell_dk_apocalypse'); -- Apocalypse
