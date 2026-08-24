--
-- Demon Hunter "Devourer" spec (ChrSpecialization 1480, Midnight 12.0.7) — NEEDS-SCRIPT bindings.
-- Attaches the WIP Devourer spell scripts (spell_dh_devourer.cpp) to their spells.
-- SHIPS ON THE BRANCH, LISTED ONLY — do NOT apply to the shared realm until the scripts are implemented.
-- All spell ids verified against DB2 build 12.0.7.68887.
--
DELETE FROM `spell_script_names` WHERE `ScriptName` IN
    ('spell_dh_void_metamorphosis','spell_dh_void_metamorphosis_drain','spell_dh_void_nova');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1217605, 'spell_dh_void_metamorphosis'),        -- Void Metamorphosis (activation, overrides 191427)
(1217607, 'spell_dh_void_metamorphosis_drain'),  -- Void Metamorphosis (transform buff, Fury drain sustain)
(1234195, 'spell_dh_void_nova');                 -- Void Nova (trait, tree 854)
