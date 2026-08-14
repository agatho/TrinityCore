-- Rising Fury (1271687): Midnight's Devastation Evoker Apex Talent.
-- Dragonrage (375087) starts and ends the window, Rising Fury (1271783) stacks itself every 6s
-- while it is up, and rank 3 hands the accumulated stacks to Risen Fury (1271799) on the way out.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_evo_risen_fury', 'spell_evo_rising_fury', 'spell_evo_rising_fury_aura');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1271799, 'spell_evo_risen_fury'),      -- Risen Fury (post-Dragonrage window + Essence Burst)
(375087, 'spell_evo_rising_fury'),      -- Dragonrage (grants/converts Rising Fury)
(1271783, 'spell_evo_rising_fury_aura'); -- Rising Fury (stacking haste/damage buff)
