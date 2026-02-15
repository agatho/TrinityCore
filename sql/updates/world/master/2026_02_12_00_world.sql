-- Assign gossip script to housing tutorial steward NPCs
-- Lyssabel Dawnpetal (233063) and Tocho (233708) grant quest kill credits
-- for "My First Home" (91863) when players interact with them.
UPDATE `creature_template` SET `ScriptName` = 'npc_housing_steward' WHERE `entry` IN (233063, 233708);
