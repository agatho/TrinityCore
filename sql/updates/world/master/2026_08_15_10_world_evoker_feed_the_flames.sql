-- Feed the Flames (369846): Midnight Devastation talent that procs Firestorm (368847) from Pyre.
-- Counter aura 405874 and ready buff 411288 are driven by spell_evo_feed_the_flames_pyre on Pyre
-- impact (393568). Do not re-bind Snapfire (370818) — talent removed in Midnight.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_evo_feed_the_flames_pyre';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(393568, 'spell_evo_feed_the_flames_pyre'); -- Pyre impact (Feed the Flames counter + Firestorm)
