-- Assign gossip script to housing tutorial steward NPCs
-- Lyssabel Dawnpetal (233063) and Tocho (233708) grant quest kill credits
-- for "My First Home" (91863) when players interact with them.
-- Add GOSSIP flag (0x1) so the client shows gossip menu options.
UPDATE `creature_template` SET `ScriptName` = 'npc_housing_steward', `npcflag` = `npcflag` | 1 WHERE `entry` IN (233063, 233708);
