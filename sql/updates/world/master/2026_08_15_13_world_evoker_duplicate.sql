-- Evoker Augmentation Apex Duplicate: Future Self AI + summon aura script
-- Breath of Eons (403631) is already bound to spell_evo_breath_of_eons in
-- 2026_08_15_11_world_evoker_ebon_might.sql.

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_evo_duplicate';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1259171, 'spell_evo_duplicate');

UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_pet_evo_future_self' WHERE `entry` = 253466;
