-- Putrefy (1247378): DUMMY that summons Lesser Ghoul (275430 / creature 237409) to cast Putrefy
-- strike (1277016) then explode (390220). Forbidden Knowledge (1256565) is handled in the same
-- script when present (grants Lesser Ghoul charges + Forbidden Sacrifice).
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dk_putrefy';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1247378, 'spell_dk_putrefy'); -- Putrefy
