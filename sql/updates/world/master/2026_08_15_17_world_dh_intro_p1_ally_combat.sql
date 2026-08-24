-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- DH intro P1 — ambient Demon Hunter staged-fight AI (post-38766 idle fix)
-- Evidence (sniff-backed only):
--   Retail sniff: Demon Hunter 94705/96930 FactionTemplate 2843 + Target=Legion (93716 / 102724 / 93115).
--   Local TC sniff temp/tc-dh-intro-playtest/tc-64864-ally-idle_parsed.txt: 96930 FactionTemplate 2843, Target=0
--     (idle next to Legion until player pulls) — faction already correct; missing combat AI.
-- Out of scope this file: Ashtongue/Coilskar/Shivarra CREATE FactionTemplate 35 (retail+local); Target often 0
--   — do not invent faction 2804 without fight evidence.

-- Ambient Demon Hunters (faction 2843 already): fighting script (replaces empty AI / thin 10y SmartAI)
UPDATE `creature_template` SET `AIName`='', `ScriptName`='npc_illidari_fighting_invasion_begins' WHERE `entry` IN (94704, 94705, 96930, 96931);

-- Drop obsolete OOC SmartAI attack scripts superseded by ScriptName above
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid` IN (94704, 94705);
