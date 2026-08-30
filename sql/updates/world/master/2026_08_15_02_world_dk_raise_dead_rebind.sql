-- Raise Dead (46584) EffectIndex0 is SPELL_EFFECT_TRIGGER_SPELL -> 1242866 since 12.0.1, so the old
-- SPELL_EFFECT_DUMMY hook on 46584 could no longer match and the ghoul was never summoned. 1242866
-- carries the permanent (-1 duration) marker aura the summon belongs to; 52150 still summons
-- Risen Ghoul (26125). 46585 remains the core-handled temporary ghoul and needs no script.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dk_raise_dead';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1242866, 'spell_dk_raise_dead'); -- Raise Dead (triggered by 46584)
