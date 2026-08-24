--
-- Lindormi 197711: apply the parts of 2026_08_08_01_world.sql's UPDATE that this schema supports.
--
-- That file sets `gossip_menu_id` on `creature_template`, but this codebase's creature_template has NO
-- such column - the creature->menu link lives in its own table, `creature_template_gossip`. The whole
-- UPDATE therefore aborted with "Unknown column 'gossip_menu_id' in 'field list'", so Lindormi silently
-- kept ScriptName '' (npc_lindormi never bound) and npcflag 131.
--
-- The menu link itself needs no action: creature_template_gossip already carries
-- (CreatureID 197711, MenuID 29898), and gossip_menu 29898 exists.
--
-- npcflag 129 = GOSSIP (1) + VENDOR (128). The 131 currently in the DB additionally sets QUESTGIVER (2),
-- which Lindormi is not - she sells and talks, she gives no quests.
--
UPDATE `creature_template`
   SET `ScriptName` = 'npc_lindormi',
       `npcflag` = 129,
       `faction` = 35,
       `subname` = 'Mythic Keystones',
       `VerifiedBuild` = 68974
 WHERE `entry` = 197711;
