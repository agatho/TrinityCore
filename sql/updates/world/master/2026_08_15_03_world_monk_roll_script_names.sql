-- Monk Roll script bindings (ported from evry/master-track/fel-rush 280a5d99bb).
-- Idempotent re-assert; spell_monk_roll / spell_monk_roll_aura already exist in this tree.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_monk_roll', 'spell_monk_roll_aura');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(109132, 'spell_monk_roll'),
(107427, 'spell_monk_roll_aura'),
(109131, 'spell_monk_roll_aura');
