--
-- World-quest rotation drift, quest 49091.
--    The 68974 capture 2 (dump_12.0.7.68974_2026-08-08_02-54-06, TESTER_SNIFF2_LINDORMI_MINE) shows
--    quest 49091 paired with activation VariableID 14062 (Value 1, Duration 21600) where the seeded
--    rotation (2026_08_08_00_world.sql) carried 14245 - an activation worldstate CAN change between
--    rotation instances of the same quest. Adopt the newer observation.
UPDATE `world_quest_template` SET `VariableID` = 14062 WHERE `QuestID` = 49091;

-- VerifiedBuild restamp to the dev client build 68887 for the world-DB rows we seeded with
-- 68275 (Lindormi / M+ keystone content; see 2026_08_07_63_world.sql, 2026_08_07_66_world.sql,
-- 2026_08_08_01_world.sql). The mythic_plus_* / item_conversion* seed restamps live in the
-- hotfixes update 2026_08_08_00_hotfixes.sql (hotfixes DB). delve_template is deliberately
-- NOT touched: the table has no VerifiedBuild column.
-- (The quest-49091 world-quest VariableID drift from this update lives on feature/world-quests
-- under this same filename.)
UPDATE `creature_template` SET `VerifiedBuild` = 68887 WHERE `entry` = 259053;
UPDATE `creature` SET `VerifiedBuild` = 68887 WHERE `guid` IN (9000200, 9000201);
UPDATE `gameobject` SET `VerifiedBuild` = 68887 WHERE `guid` = 9000300;
