-- DEPENDS ON feature/midnight-class-abilities: the spell_dh_fel_rush* scripts these rows bind to
-- live in src/server/scripts/Spells/spell_dh.cpp, which this branch deliberately does not touch
-- (that file is owned by the class-abilities port of evry/master-track/class-abilities).
-- Until that lands, worldserver will log these ScriptNames as missing on startup - harmless, but
-- Fel Rush stays unscripted.
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_dh_fel_rush_damage';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(192611, 'spell_dh_fel_rush_damage');
