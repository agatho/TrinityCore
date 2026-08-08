--
-- World-quest rotation drift, quest 49091.
--    The 68974 capture 2 (dump_12.0.7.68974_2026-08-08_02-54-06, TESTER_SNIFF2_LINDORMI_MINE) shows
--    quest 49091 paired with activation VariableID 14062 (Value 1, Duration 21600) where the seeded
--    rotation (2026_08_08_00_world.sql) carried 14245 - an activation worldstate CAN change between
--    rotation instances of the same quest. Adopt the newer observation.
UPDATE `world_quest_template` SET `VariableID` = 14062 WHERE `QuestID` = 49091;
