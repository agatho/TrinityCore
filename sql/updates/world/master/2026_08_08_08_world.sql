-- Chromie Time deselect path (audit R6): bind the script that handles gossip
-- option 51903 ("I'd like to return to the present timeline, Chromie.",
-- capture A rec 4675) to Chromie 167032. The option row itself ships with
-- 2026_08_08_07_world.sql.
UPDATE `creature_template` SET `ScriptName`='npc_chromie_timewalking' WHERE `entry`=167032;
